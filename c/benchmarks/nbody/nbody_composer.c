
#include <stdlib.h>
#include <stdio.h>

#include <composer.h>
#include <mkl.h>
#include <mkl_extensions.h>
#include <string.h>
#include <vec.h>

#include "nbody.h"
#include "nbody_composer.h"

#include <generated/generated.h>

/** Computes Sum(G * pm / r ** 2 * (dx / r)).
 *
 * Diagonal elements are not counted in the sum.
 *
 */
void composer_compute_force(MKL_INT n,
    double *dx, double *pm, double *r,
    double *tmp1,
    double *output,
    int first) {

  MKL_INT size = n * n;

  if (!first) {
    composer_execute();
  }

  c_vdMuli(size, pm, G, tmp1);
  c_vdPowx(size, r, 2.0, output);
  c_vdDiv(size, tmp1, output, tmp1);
  c_vdDiv(size, dx, r, output);
  c_vdMul(size, tmp1, output, tmp1);

  memset(output, 0, sizeof(double) * n);

  #pragma omp parallel for
  for (MKL_INT i = 0; i < n; i++) {
    double sum = 0.0;
    for (MKL_INT j = 0; j < n; j++) {
      // Ignore diagonal elements.
      if (i != j) {
        sum += tmp1[i*n + j];
      }
    }
    output[i] += sum;
  }
}

void composer_move(MKL_INT n,
    double *m, double *x, double *y, double *z, double *vx, double *vy, double *vz,
    // Temporaries that have n * n space.
    double *dx, double *dy, double *dz, double *pm, double *r, double *tmp1, double *tmp2) {

  set_delta(n, x, dx);
  set_delta(n, y, dy);
  set_delta(n, z, dz);
  set_pm(n, m, pm);

  MKL_INT size = n * n;

  // r = sqrt(dx**2 + dy**2 + dz**2)
  c_vdPowx(size, dx, 2.0, tmp1);
  c_vdPowx(size, dy, 2.0, tmp2);
  c_vdAdd(size, tmp1, tmp2, tmp1);
  c_vdPowx(size, dz, 2.0, tmp2);
  c_vdAdd(size, tmp1, tmp2, tmp1);
  c_vdSqrt(size, tmp1, r);

  composer_compute_force(n, dx, pm, r, tmp1, tmp2, 1);
  c_vdDiv(n, tmp2, m, tmp1);
  c_vdMuli(n, tmp1, dt, tmp1);
  c_vdAdd(n, vx, tmp1, vx);

  c_vdMuli(n, vx, dt, tmp1);
  c_vdAdd(n, x, tmp1, x);

  composer_compute_force(n, dy, pm, r, tmp1, tmp2, 0);
  c_vdDiv(n, tmp2, m, tmp1);
  c_vdMuli(n, tmp1, dt, tmp1);
  c_vdAdd(n, vy, tmp1, vy);

  c_vdMuli(n, vy, dt, tmp1);
  c_vdAdd(n, y, tmp1, y);

  composer_compute_force(n, dz, pm, r, tmp1, tmp2, 0);
  c_vdDiv(n, tmp2, m, tmp1);
  c_vdMuli(n, tmp1, dt, tmp1);
  c_vdAdd(n, vz, tmp1, vz);

  c_vdMuli(n, vz, dt, tmp1);
  c_vdAdd(n, z, tmp1, z);
}

void run_mkl_composer(int iterations, MKL_INT n,
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
    composer_move(n, m, x, y, z, vx, vy, vz,
        dx.data, dy.data, dz.data, pm.data, r.data, tmp1.data, tmp2.data);
  }
}

