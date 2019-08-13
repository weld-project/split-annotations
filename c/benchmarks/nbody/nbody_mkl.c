
#include <stdlib.h>
#include <stdio.h>

#include <mkl.h>
#include <mkl_extensions.h>
#include <string.h>
#include <vec.h>

#include "nbody.h"
#include "nbody_mkl.h"

/** Computes Sum(G * pm / r ** 2 * (dx / r)).
 *
 * Diagonal elements are not counted in the sum.
 *
 */
void compute_force(MKL_INT n,
    double *dx, double *pm, double *r,
    double *tmp1,
    double *output) {

  MKL_INT size = n * n;

  vdMuli(size, pm, G, tmp1);
  vdPowx(size, r, 2.0, output);
  vdDiv(size, tmp1, output, tmp1);
  vdDiv(size, dx, r, output);
  vdMul(size, tmp1, output, tmp1);

  memset(output, 0, sizeof(double) * n);

  #pragma omp parallel for
  for (MKL_INT i = 0; i < n; i++) {
    double sum = 0.0;
    for (MKL_INT j = 0; j < n; j++) {
      // Ignore diagonal elements.
      if (i != j) {
        // Causes some imprecision compared to reference?
        sum += tmp1[i*n + j];
      }
    }
    output[i] += sum;
  }
}

void move(MKL_INT n,
    double *m, double *x, double *y, double *z, double *vx, double *vy, double *vz,
    // Temporaries that have n * n space.
    double *dx, double *dy, double *dz, double *pm, double *r, double *tmp1, double *tmp2) {

  set_delta(n, x, dx);
  set_delta(n, y, dy);
  set_delta(n, z, dz);
  set_pm(n, m, pm);

  MKL_INT size = n * n;

  // r = sqrt(dx**2 + dy**2 + dz**2)
  vdPowx(size, dx, 2.0, tmp1);
  vdPowx(size, dy, 2.0, tmp2);
  vdAdd(size, tmp1, tmp2, tmp1);
  vdPowx(size, dz, 2.0, tmp2);
  vdAdd(size, tmp1, tmp2, tmp1);
  vdSqrt(size, tmp1, r);

  compute_force(n, dx, pm, r, tmp1, tmp2);
  vdDiv(n, tmp2, m, tmp1);
  vdMuli(n, tmp1, dt, tmp1);
  vdAdd(n, vx, tmp1, vx);

  vdMuli(n, vx, dt, tmp1);
  vdAdd(n, x, tmp1, x);

  compute_force(n, dy, pm, r, tmp1, tmp2);
  vdDiv(n, tmp2, m, tmp1);
  vdMuli(n, tmp1, dt, tmp1);
  vdAdd(n, vy, tmp1, vy);

  vdMuli(n, vy, dt, tmp1);
  vdAdd(n, y, tmp1, y);

  compute_force(n, dz, pm, r, tmp1, tmp2);
  vdDiv(n, tmp2, m, tmp1);
  vdMuli(n, tmp1, dt, tmp1);
  vdAdd(n, vz, tmp1, vz);

  vdMuli(n, vz, dt, tmp1);
  vdAdd(n, z, tmp1, z);
}

void run_mkl(int iterations, MKL_INT n,
    double *m,
    double *x, double *y, double *z, double *vx, double *vy, double *vz) {

  vec_t dx = new_vec(n * n, 0);
  vec_t dy = new_vec(n * n, 0);
  vec_t dz = new_vec(n * n, 0);
  vec_t pm = new_vec(n * n, 0);
  vec_t r = new_vec(n * n, 0);
  vec_t tmp1 = new_vec(n * n, 0);
  vec_t tmp2 = new_vec(n * n, 0);

  for (int i = 0; i < iterations; i++) {
    printf("iteration %d\n", i);
    move(n, m, x, y, z, vx, vy, vz,
        dx.data, dy.data, dz.data, pm.data, r.data, tmp1.data, tmp2.data);
  }
}

