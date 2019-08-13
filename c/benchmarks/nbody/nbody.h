#ifndef _NBODY_H_
#define _NBODY_H_

// Constants.
#define G     ((double)(6.67384e-11))
#define dt    ((double)(60 * 60 * 24 * 365.25))
#define r_ly  ((double)(9.4607e15))
#define m_sol ((double)(1.9891e30))

typedef struct galaxy {
  MKL_INT n;
  double *m;
  double *x;
  double *y;
  double *z;
  double *vx;
  double *vy;
  double *vz;
} galaxy_t;

galaxy_t inputs(long n, int lazy); 

void set_delta(MKL_INT n, const double *x, double *out); 
void set_pm(MKL_INT n, const double *x, double *out);

void print_vector(int n, const double *v);
void print_matrix(int n, const double *v);

#endif
