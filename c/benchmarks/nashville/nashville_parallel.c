#ifdef __linux__
#define _GNU_SOURCE
#endif
#include <stdio.h>

#include <assert.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/time.h>

#include <MagickWand.h>

#include <omp.h>

#include "nashville.h"

typedef struct piece_ {
  MagickWand *piece;
  int index;
} piece_t;

int compare(const void *a, const void *b) {
  const piece_t *left = (const piece_t *)a;
  const piece_t *right = (const piece_t *)b;
  return left->index - right->index;
}

MagickWand *aggregate_seq(piece_t *pieces, int count) {
  MagickWand *results = NewMagickWand();
  MagickResetIterator(results);

  for (int i = 0; i < count; i++) {
    MagickSetLastIterator(results);
    MagickAddImage(results, pieces[i].piece); 
  }

  MagickResetIterator(results);
  MagickWand *final = MagickAppendImages(results, 1);
  DestroyMagickWand(results);

  return final;
}

MagickWand *aggregate_par(piece_t *pieces, int count, int threads) {

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
      MagickAddImage(result, pieces[j].piece); 
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

MagickWand *colortone_parallel(MagickWand *input_wand, const char *color, const char *compose_opt, int negate, int threads) {
  size_t width = MagickGetImageWidth(input_wand);
  size_t height = MagickGetImageHeight(input_wand);

  printf("Image is (%ld x %ld) pixels\n", width, height);

  // We want each chunk to be close to the L2 cache size.
  const int l2_cache_size_bytes = 262144 * 3;
  // Number of rows to process per batch.
  size_t region_height = l2_cache_size_bytes / width;
  if (region_height == 0) {
    region_height = 1;
  }
  region_height = 199;

  // TODO this might shave off a few things.
  int num_regions = height / region_height;
  printf("Regions: %d\n", num_regions);

  struct timeval start, end, diff;
  gettimeofday(&start, NULL);

  piece_t *pieces = malloc(num_regions * sizeof(piece_t));

  #pragma omp parallel for
  for (int i = 0; i < num_regions; i++) {
    /*
    printf("%d Looking at region (%ld -> %ld, %ld -> %ld)\n", i,
        0l, 0l + width,
        region_height * i, region_height * i + region_height);
    */
    MagickWand *wand = MagickGetImageRegion(input_wand, width,
        region_height, 0, region_height * i);

    MagickWand *colorized_wand = CloneMagickWand(wand);
    MagickWand *colorspace_wand = CloneMagickWand(wand);

    do_colortone(wand, color, compose_opt, negate, colorized_wand, colorspace_wand);
    do_colortone(wand, color, compose_opt, negate, colorized_wand, colorspace_wand);
    MagickModulateImage(wand, HUE, SATURATION, VALUE);
    MagickGammaImage(wand, GAMMA);

    colorized_wand = DestroyMagickWand(colorized_wand);
    colorspace_wand = DestroyMagickWand(colorspace_wand);

    pieces[i].index = i;
    pieces[i].piece = wand;
  }

  gettimeofday(&end, NULL);
  timersub(&end, &start, &diff);
  double runtime = (double)diff.tv_sec + ((double)diff.tv_usec / 1000000.0);
  printf("Processing runtime: %.3f seconds\n", runtime);
  fflush(stdout);

  gettimeofday(&start, NULL);

  // Sort pieces by their index.
  qsort(pieces, num_regions, sizeof(piece_t), compare);

  gettimeofday(&end, NULL);
  timersub(&end, &start, &diff);
  runtime = (double)diff.tv_sec + ((double)diff.tv_usec / 1000000.0);
  printf("Sort runtime: %.3f seconds\n", runtime);
  fflush(stdout);

  gettimeofday(&start, NULL);

  MagickWand *final;
  if (num_regions / threads > 16) {
    printf("parallel aggregation\n");
    final = aggregate_par(pieces, num_regions, threads); 
  } else {
    printf("sequential aggregation\n");
    final = aggregate_seq(pieces, num_regions); 
  }

  free(pieces);

  gettimeofday(&end, NULL);
  timersub(&end, &start, &diff);
  runtime = (double)diff.tv_sec + ((double)diff.tv_usec / 1000000.0);
  printf("Total aggregation runtime: %.3f seconds\n", runtime);
  fflush(stdout);

  return final;
}
