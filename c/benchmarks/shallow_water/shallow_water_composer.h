#ifndef _SHALLOW_WATER_COMPOSER_H_
#define  _SHALLOW_WATER_COMPOSER_H_

/** Run the shallow water simulation with MKL and Composer. */
void run_mkl_composer(
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
