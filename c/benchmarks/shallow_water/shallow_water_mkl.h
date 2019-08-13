#ifndef _SHALLOW_WATER_MKL_H_
#define _SHALLOW_WATER_MKL_H_

/** Run the shallow water simulation with MKL. */
void run_mkl(
    int iterations,
    MKL_INT n,
    double *eta,
    double *u,
    double *v,
    double g,
    double b,
    double dt,
    double grid_spacing);

#endif
