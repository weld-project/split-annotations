#ifndef _VEC_H_
#define _VEC_H_

/** A small convinience library for vectors used with composer. */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  double *data;
  size_t length;
} vec_t;


vec_t new_vec(size_t length, int lazy);
vec_t new_vec_nolazy(size_t length);
vec_t vvals(size_t length, double val, int lazy);

#ifdef __cplusplus
}
#endif

#endif
