#ifndef _MKL_EXTENSIONS_H_
#define _MKL_EXTENSIONS_H_

#ifdef __cplusplus
extern "C" {
#endif

void vdAddi(MKL_INT length, double *a, double b, double *result);
void vdSubi(MKL_INT length, double *a, double b, double *result);
void vdMuli(MKL_INT length, double *a, double b, double *result);
void vdDivi(MKL_INT length, double *a, double b, double *result);
void vdSubvi(MKL_INT length, double a, double *b, double *result);
void vdDivvi(MKL_INT length, double a, double *b, double *result);

#ifdef __cplusplus
}
#endif

#endif
