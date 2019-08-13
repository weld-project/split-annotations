
#include <composer.h>
#include <assert.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  double *data;
  size_t length;
} vec_t;

vec_t new_vec(size_t length, int lazy) {
  vec_t result;
  result.data = (double *)composer_malloc(sizeof(double) * length, lazy);
  result.length = length;
  return result;
}

vec_t new_vec_nolazy(size_t length) {
  vec_t result;
  result.data = (double *)malloc(sizeof(double) * length);
  assert(result.data);
  result.length = length;
  return result;
}

// Initialize a vector where the value is val.
vec_t vvals(size_t length, double val, int lazy) {
  vec_t result;
  size_t size = sizeof(double) * length;
  result.data = (double *)composer_malloc(size, 0);
  result.length = length;
  for (int i = 0; i < length; i++) {
    result.data[i] = val;
  }

  if (lazy) {
    composer_tolazy(result.data);
  }

  return result;
}

#ifdef __cplusplus
}
#endif
