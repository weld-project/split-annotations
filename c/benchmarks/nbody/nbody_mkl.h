#ifndef _NBODY_MKL_H_
#define _NBODY_MKL_H_

#include <mkl.h>

void run_mkl(int iterations, MKL_INT n,
    double *m,
    double *x, double *y, double *z, double *vx, double *vy, double *vz);

#endif
