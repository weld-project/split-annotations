
#define INFINITE_PIECES (-1)

#include "mkl.h"

#include "mkl_extensions.h"
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

// Extensions to MKL, since it doesn't support immediates
// Unary Operators with Immediate values

void vdAddi(MKL_INT length, double *a, double b, double *result) {
  for (size_t i = 0; i < length; i++) {
    result[i] = a[i] + b;
  }
}

void vdSubi(MKL_INT length, double *a, double b, double *result) {
  for (size_t i = 0; i < length; i++) {
    result[i] = a[i] - b;
  }
}

void vdMuli(MKL_INT length, double *a, double b, double *result) {
  for (size_t i = 0; i < length; i++) {
    result[i] = a[i] * b;
  }
}

void vdDivi(MKL_INT length, double *a, double b, double *result) {
  for (size_t i = 0; i < length; i++) {
    result[i] = a[i] / b;
  }
}

void vdSubvi(MKL_INT length, double a, double *b, double *result) {
  for (size_t i = 0; i < length; i++) {
    result[i] = a - b[i];
  }
}

void vdDivvi(MKL_INT length, double a, double *b, double *result) {
  for (size_t i = 0; i < length; i++) {
    result[i] = a / b[i];
  }
}

#ifdef __cplusplus
}
#endif
