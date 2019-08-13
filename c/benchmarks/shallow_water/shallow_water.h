#ifndef  _SHALLOW_WATER_H_
#define  _SHALLOW_WATER_H_

#include <mkl.h>

// Inputs to the simulation.
typedef struct input {
  MKL_INT n;
  double *u;
  double *v;
  double *eta;
  double g;
  double b;
  double dt;
  double grid_spacing;
} input_t;

/** Initialize inputs.
 *
 * The inputs are initialized to be consistent with shallowwater_reference.py.
 *
 */
input_t inputs(long n, int lazy);

/** Prints an n * n matrix to stdout. */
void print_matrix(int n, const double *v);

/** Shifts the input vector along the given axis by amount.
 *
 * If axis is 0, rolls along columns. If axis is 1, rolls along rows.
 * The input should be an n * n matrix.
 */
void roll(
    // Inputs
    MKL_INT n, const double *restrict input, int axis, int amount,
    // Output
    double *restrict output);

#endif
