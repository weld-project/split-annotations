
#include "generated/generated.h"
#include <stdio.h>

#define DEBUG 0

#define DBG(fmt, ...) \
        do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt "\n", __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)

struct RegularSplit {
  double *base;
  int size;
};

struct SizeSplit {
  size_t size;
};

void* RegularSplit_new(double **item_to_split, struct RegularSplit_init_args *a, int64_t *items) {
  struct RegularSplit *splitter = (struct RegularSplit *)malloc(sizeof(struct RegularSplit));
  splitter->base = *item_to_split;
  splitter->size = a->_0; 
  DBG("base=%p, size=%d\n", item_to_split, a->_0);

  *items = splitter->size;
  return (void *)splitter;
}

SplitterStatus RegularSplit_next(const void *s,
    int64_t start,
    int64_t end,
    double **out) {

  const struct RegularSplit *splitter = (const struct RegularSplit *)s;
  DBG("start=%lld, end=%lld, size=%d", start, end, splitter->size);
  if (splitter->size < start) {
    return SplitterFinished;
  } else {
    *out = splitter->base + start;
    return SplitterContinue;
  }
}

void* SizeSplit_new(MKL_INT *item_to_split, struct SizeSplit_init_args *_unused, int64_t *items) {
  struct SizeSplit *splitter = (struct SizeSplit *)malloc(sizeof(struct SizeSplit));
  splitter->size = *item_to_split;
  *items = splitter->size;
  return (void *)splitter;
}

SplitterStatus SizeSplit_next(const void *s,
    int64_t start,
    int64_t end,
    MKL_INT *out) {

  struct SizeSplit *splitter = (struct SizeSplit *)s;
  DBG("start=%lld, end=%lld, size=%zu", start, end, splitter->size);
  if (splitter->size < start) {
    return SplitterFinished;
  } else if (splitter->size < end) {
    *out = (splitter->size - start);
    return SplitterContinue;
  } else {
    *out = (end - start);
    return SplitterContinue;
  }
}
