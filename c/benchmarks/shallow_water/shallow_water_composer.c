#include <stdlib.h>
#include <stdio.h>

#include "mkl_vml_functions.h"
#include "mkl.h"

#include <mkl_extensions.h>
#include <vec.h>

#include "shallow_water.h"
#include "shallow_water_composer.h"

#include <generated/generated.h>

void c_spatialDerivative(
    // Inputs
    MKL_INT n, const double *restrict input, int axis, double grid_spacing,
    // Temporaries
    double *restrict tmp1, double *restrict tmp2,
    // Outputs
    double *output) {

  MKL_INT size = n * n;

  composer_execute();
  roll(n, input, axis, -1, tmp1);
  roll(n, input, axis,  1, tmp2);

  c_vdSub(size, tmp1, tmp2, output);
  c_vdDivi(size, output, grid_spacing * 2.0, output);
}

void c_d_dx(
    // Inputs
    MKL_INT n, const double *restrict input, double grid_spacing,
    // Temporaries
    double *restrict tmp1, double *restrict tmp2,
    // Output
    double *output) {
  c_spatialDerivative(n, input, 1, grid_spacing, tmp1, tmp2, output);
}

void c_d_dy(
    // Inputs
    MKL_INT n, const double *restrict input, double grid_spacing,
    // Temporaries
    double *restrict tmp1, double *restrict tmp2,
    // Output
    double *output) {
  c_spatialDerivative(n, input, 0, grid_spacing, tmp1, tmp2, output);
}

void c_d_dt(
    MKL_INT n, double *eta, double *u, double *v,
    double g, double b, double grid_spacing,
    // Outputs
    double *du_dt, double *dv_dt, double *deta_dt,
    // Temporaries
    double *tmp1, double *tmp2, double *tmp3, double *tmp4) {

  MKL_INT size = n * n;

  // STAGE 1:

  // du_dt = -g*d_dx(eta) - u*b
  c_d_dx(n, eta, grid_spacing, tmp2, tmp3, tmp1);
  c_vdMuli(size, tmp1, -g, tmp1);
  c_vdMuli(size, u, b, tmp2);
  c_vdSub(size, tmp1, tmp2, du_dt);

  // STAGE 2:
  // dv_dt = -g*d_dy(eta) - v*b
  c_d_dy(n, eta, grid_spacing, tmp2, tmp3, tmp1);
  c_vdMuli(size, tmp1, -g, tmp1);
  c_vdMuli(size, v, b, tmp2);
  c_vdSub(size, tmp1, tmp2, dv_dt);

  // STAGE 3 (not pipelined)

  // tmp1 = -d_dx(u * eta)
  c_vdMul(size, u, eta, tmp4);
  c_d_dx(n, tmp4, grid_spacing, tmp2, tmp3, tmp1);
  c_vdMuli(size, tmp1, -1, tmp1);

  // STAGE 4 (not pipelined)

  // deta_dt = d_dy(v * eta)
  c_vdMul(size, v, eta, tmp4);
  c_d_dy(n, tmp4, grid_spacing, tmp2, tmp3, deta_dt);

  // deta_dt = -d_dx(u*eta) - d_dy(v*eta)
  c_vdSub(size, tmp1, deta_dt, deta_dt);
}

void c_evolveEuler(
    // Inputs and Outputs
    MKL_INT n, double *eta, double *u, double *v,
    double g, double b, double dt, double grid_spacing,
    // Temporaries
    double *du_dt, double *dv_dt, double *deta_dt,
    double *tmp1, double *tmp2, double *tmp3, double *tmp4) {

  c_d_dt(
      n, eta, u, v,
      g, b, grid_spacing,
      du_dt, dv_dt, deta_dt,
      tmp1, tmp2, tmp3, tmp4);

  MKL_INT size = n * n;

  // eta = eta + deta_dt + dt
  c_vdMuli(size, deta_dt, dt, tmp1);
  c_vdAdd(size, eta, tmp1, eta);

  // u = u + du_dt * dt
  c_vdMuli(size, du_dt, dt, tmp1);
  c_vdAdd(size, u, tmp1, u);

  // v = v + dv_dt * dt
  c_vdMuli(size, dv_dt, dt, tmp1);
  c_vdAdd(size, v, tmp1, v);
}

void run_mkl_composer(
    int iterations,
    MKL_INT n,
    double *eta,  // Lazy
    double *u,    // Lazy
    double *v,    // Lazy
    double g,
    double b,
    double dt,
    double grid_spacing) {

  long size = n * n;

  // Generate outputs and temporaries.
  //
  // We mark these as lazy.
  vec_t du_dt = new_vec(size, 1);
  vec_t dv_dt = new_vec(size, 1);
  vec_t deta_dt = new_vec(size, 1);

  vec_t tmp1 = new_vec(size, 1);
  vec_t tmp2 = new_vec(size, 1);
  vec_t tmp3 = new_vec(size, 1);
  vec_t tmp4 = new_vec(size, 1);

  // TODO Benchmark with and without temporaries.
  //
  // Marking all of these as temporaries is not right because they are only
  // temporaries for the full program -- not temporaries in any particular
  // stage.
  /*
  composer_register_temporary(&du_dt.data, sizeof(double*));
  composer_register_temporary(&dv_dt.data, sizeof(double*));
  composer_register_temporary(&deta_dt.data, sizeof(double*));
  composer_register_temporary(&tmp1.data, sizeof(double*));
  composer_register_temporary(&tmp2.data, sizeof(double*));
  composer_register_temporary(&tmp3.data, sizeof(double*));
  */

  double time = 0;

  for (int i = 0; i < iterations; i++) {
    fprintf(stderr, "iteration %d\n", i);
    c_evolveEuler(n, eta, u, v, g, b, dt, grid_spacing,
        du_dt.data, dv_dt.data, deta_dt.data,
        tmp1.data, tmp2.data, tmp3.data, tmp4.data);
    time += dt;

    // Force execution at the end of an iteration.
    composer_execute();
  }
}
