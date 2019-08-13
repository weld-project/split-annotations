#include <stdlib.h>
#include <stdio.h>

#include "mkl_vml_functions.h"
#include "mkl.h"

#include <mkl_extensions.h>
#include <vec.h>

#include "shallow_water.h"

void spatialDerivative(
    // Inputs
    MKL_INT n, const double *restrict input, int axis, double grid_spacing,
    // Temporaries
    double *restrict tmp1, double *restrict tmp2,
    // Outputs
    double *output) {

  MKL_INT size = n * n;

  roll(n, input, axis, -1, tmp1);
  roll(n, input, axis,  1, tmp2);

  vdSub(size, tmp1, tmp2, output);
  vdDivi(size, output, grid_spacing * 2.0, output);
}

void d_dx(
    // Inputs
    MKL_INT n, const double *restrict input, double grid_spacing,
    // Temporaries
    double *restrict tmp1, double *restrict tmp2,
    // Output
    double *output) {
  spatialDerivative(n, input, 1, grid_spacing, tmp1, tmp2, output);
}

void d_dy(
    // Inputs
    MKL_INT n, const double *restrict input, double grid_spacing,
    // Temporaries
    double *restrict tmp1, double *restrict tmp2,
    // Output
    double *output) {
  spatialDerivative(n, input, 0, grid_spacing, tmp1, tmp2, output);
}

void d_dt(
    MKL_INT n, double *eta, double *u, double *v,
    double g, double b, double grid_spacing,
    // Outputs
    double *du_dt, double *dv_dt, double *deta_dt,
    // Temporaries
    double *tmp1, double *tmp2, double *tmp3, double *tmp4) {

  MKL_INT size = n * n;

  // STAGE 1:

  // du_dt = -g*d_dx(eta) - u*b
  d_dx(n, eta, grid_spacing, tmp2, tmp3, tmp1);
  vdMuli(size, tmp1, -g, tmp1);
  vdMuli(size, u, b, tmp2);
  vdSub(size, tmp1, tmp2, du_dt);

  // STAGE 2:
  // dv_dt = -g*d_dy(eta) - v*b
  d_dy(n, eta, grid_spacing, tmp2, tmp3, tmp1);
  vdMuli(size, tmp1, -g, tmp1);
  vdMuli(size, v, b, tmp2);
  vdSub(size, tmp1, tmp2, dv_dt);

  // STAGE 3 (not pipelined)

  // tmp1 = -d_dx(u * eta)
  vdMul(size, u, eta, tmp4);
  d_dx(n, tmp4, grid_spacing, tmp2, tmp3, tmp1);
  vdMuli(size, tmp1, -1, tmp1);

  // deta_dt = d_dy(v * eta)
  vdMul(size, v, eta, tmp4);
  d_dy(n, tmp4, grid_spacing, tmp2, tmp3, deta_dt);

  // deta_dt = -d_dx(u*eta) - d_dy(v*eta)
  vdSub(size, tmp1, deta_dt, deta_dt);
}

void evolveEuler(
    // Inputs and Outputs
    MKL_INT n, double *eta, double *u, double *v,
    double g, double b, double dt, double grid_spacing,
    // Temporaries
    double *du_dt, double *dv_dt, double *deta_dt,
    double *tmp1, double *tmp2, double *tmp3, double *tmp4) {

  d_dt(
      n, eta, u, v,
      g, b, grid_spacing,
      du_dt, dv_dt, deta_dt,
      tmp1, tmp2, tmp3, tmp4);

  MKL_INT size = n * n;

  // eta = eta + deta_dt + dt
  vdMuli(size, deta_dt, dt, tmp1);
  vdAdd(size, eta, tmp1, eta);

  // u = u + du_dt * dt
  vdMuli(size, du_dt, dt, tmp1);
  vdAdd(size, u, tmp1, u);

  // v = v + dv_dt * dt
  vdMuli(size, dv_dt, dt, tmp1);
  vdAdd(size, v, tmp1, v);
}

void run_mkl(
    int iterations,
    MKL_INT n,
    double *eta,
    double *u,
    double *v,
    double g,
    double b,
    double dt,
    double grid_spacing) {

  long size = n * n;

  // Generate outputs and temporaries.
  vec_t du_dt = new_vec(size, 0);
  vec_t dv_dt = new_vec(size, 0);
  vec_t deta_dt = new_vec(size, 0);

  vec_t tmp1 = new_vec(size, 0);
  vec_t tmp2 = new_vec(size, 0);
  vec_t tmp3 = new_vec(size, 0);
  vec_t tmp4 = new_vec(size, 0);

  double time = 0;

  for (int i = 0; i < iterations; i++) {
    fprintf(stderr, "iteration %d\n", i);
    evolveEuler(n, eta, u, v, g, b, dt, grid_spacing,
        du_dt.data, dv_dt.data, deta_dt.data,
        tmp1.data, tmp2.data, tmp3.data, tmp4.data);
    time += dt;
  }
}
