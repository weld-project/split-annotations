#ifndef _NBODY_COMPOSER_H_
#define _NBODY_COMPOSER_H_

#include <mkl.h>

void run_mkl_composer(int iterations, MKL_INT n,
    double *m,
    double *x, double *y, double *z, double *vx, double *vy, double *vz);

#endif
