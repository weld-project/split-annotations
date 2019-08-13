
#include <composer.h>
#include <MagickWand.h>

#include <stdio.h>

#define DEBUG 0

#define DBG(fmt, ...) \
        do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt "\n", __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)


struct WandSplit {
  MagickWand *wand;
  size_t width;
  size_t height;
};

void* WandSplit_new(MagickWand **wand_to_split, struct WandSplit_init_args *_, int64_t *items) {
  struct WandSplit *splitter = (struct WandSplit *)malloc(sizeof(struct WandSplit));
  splitter->wand = *wand_to_split;

  // We'll split the image by row, since there are nice methods for reconstructing an image
  // in this way that are builtin.
  splitter->width = MagickGetImageWidth(splitter->wand);
  splitter->height = MagickGetImageHeight(splitter->wand);
  *items = splitter->height;
  DBG("items: %ld", splitter->height);
  return (void *)splitter;
}

SplitterStatus WandSplit_next(const void *s,
    int64_t start,
    int64_t end,
    MagickWand **out) {

  const struct WandSplit *splitter = (const struct WandSplit *)s;
  DBG("start: %ld end: %ld height: %ld", start, end, splitter->height);

  if (splitter->height <= start) {
    DBG("finished got range (%ld %ld)", start, end);
    return SplitterFinished;
  } else {
    size_t region_height = (end - start);
    if (splitter->height < end) {
      DBG("clipping region height by %ld", end - splitter->height);
      region_height = splitter->height - start;
    }
    DBG("range: %ld, %ld", start, start + region_height);

    MagickWand *wand = MagickGetImageRegion(splitter->wand, splitter->width, region_height, 0, start);
    *out = wand;
    return SplitterContinue;
  }
}

MagickWand *aggregate_seq(MagickWand **pieces, int64_t count) {
  MagickWand *results = NewMagickWand();
  MagickResetIterator(results);

  DBG("consturcted results image %p", results);

  for (int i = 0; i < count; i++) {
    DBG("adding image %d", i);
    fflush(stderr);
    MagickSetLastIterator(results);
    MagickAddImage(results, pieces[i]); 
  }

  MagickResetIterator(results);
  MagickWand *final = MagickAppendImages(results, 1);
  DestroyMagickWand(results);

  return final;
}

MagickWand *aggregate_par(MagickWand **pieces, int count, int threads) {
  // Holds aggregation state.
  MagickWand **results = (MagickWand **)malloc(sizeof(MagickWand *) * threads);
  for (int i = 0; i < threads; i++) {
    results[i] = NewMagickWand();
    MagickResetIterator(results[i]);
  }

  int values_per_thread = count / threads;
  printf("values per piece: %d\n", values_per_thread);

#pragma omp parallel for
  for (int i = 0; i < threads; i++) {
    int start = i * values_per_thread;
    int end = (i + 1) * values_per_thread;

    if (i == threads - 1) {
      end = count;
    }

    MagickWand *result = results[i];

    // printf("thread %d: %d->%d\n", omp_get_thread_num(), start, end);
    for (int j = start; j < end; j++) {
      MagickSetLastIterator(result);
      MagickAddImage(result, pieces[j]); 
    }

    MagickResetIterator(result);
    MagickWand *final = MagickAppendImages(result, 1);

    result = DestroyMagickWand(result);
    results[i] = final;
  }

  MagickWand *final_iterator = NewMagickWand();
  MagickResetIterator(final_iterator);
  for (int i = 0; i < threads; i++) {
    MagickSetLastIterator(final_iterator);
    MagickAddImage(final_iterator, results[i]);
  }
  MagickResetIterator(final_iterator);
  MagickWand *final = MagickAppendImages(final_iterator, 1);

  for (int i = 0; i < threads; i++) {
    DestroyMagickWand(results[i]);
  }
  free(results);

  return final;
}

void *WandSplit_merge(const void *s, int64_t length, int64_t threads) {
  MagickWand *final;
  MagickWand **pieces = (MagickWand **)s;

  if (length == 1) {
    DBG("only one item: returning it %d", 0);
    return ((MagickWand **)s)[0];
  }

  MagickWand *results = NewMagickWand();
  MagickResetIterator(results);
  for (int i = 0; i < length; i++) {
    MagickSetLastIterator(results);
    MagickAddImage(results, pieces[i]); 
  }
  MagickResetIterator(results);
  final = MagickAppendImages(results, 1);
  DestroyMagickWand(results);
  // DBG("aggregate_seq: %p", aggregate_seq);
  // final = aggregate_seq(pieces, length);

  return (void *)final;
}
