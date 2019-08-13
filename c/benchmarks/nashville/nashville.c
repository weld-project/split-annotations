// Andromeda: https://www.spacetelescope.org/images/heic1502a/
//
// https://www.spacetelescope.org/static/archives/images/publicationtiff40k/heic1502a.tif

#include <stdlib.h>

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
#include <composer.h>

#include "nashville.h"
#include "nashville_parallel.h"
#include "nashville_composer.h"

typedef enum {
  UNKNOWN = 0,
  NAIVE,
  COMPOSER,
  PARALLEL
} exec_mode_t;

// Number of threads.
long threads = 1;
// Mode to use
exec_mode_t mode;
// input filename.
char input_filename[2048];
// TODO This should be autotuned/based on the input size.
int pieces = 20;
// Should the result be written to a file?
int write_out = 0;

// Actual work for the colortone function.
void do_colortone(MagickWand *wand,
    const char *color,
    const char *compose_opt,
    int negate,
    MagickWand *colorized_wand,
    MagickWand *colorspace_wand) {

    // Colorize image. 
    PixelWand *colorize = NewPixelWand();
    PixelWand *alpha = NewPixelWand();
    PixelSetColor(colorize, color);
    PixelSetColor(alpha, "#fff");
    MagickColorizeImage(colorized_wand, colorize, alpha);

    // Convert to grayspace.
    MagickSetImageColorspace(colorspace_wand, GRAYColorspace);
    if (negate) {
      MagickNegateImage(colorspace_wand, 1);
    }

    MagickSetImageArtifact(wand, "compose:args", compose_opt);
    MagickCompositeImage(wand, colorspace_wand, BlendCompositeOp, 1, 0, 0);
    MagickCompositeImage(wand, colorized_wand, BlendCompositeOp, 1, 0, 0);

    // Cleanup.
    colorize = DestroyPixelWand(colorize);
    alpha = DestroyPixelWand(alpha);

}

MagickWand *colortone_simple(MagickWand *input_wand,
    const char *color,
    const char *compose_opt,
    int negate) {

  MagickWand *wand = CloneMagickWand(input_wand);
  MagickWand *colorized_wand = CloneMagickWand(wand);
  MagickWand *colorspace_wand = CloneMagickWand(wand);

  do_colortone(wand, color, compose_opt, negate, colorized_wand, colorspace_wand);
  do_colortone(wand, color, compose_opt, negate, colorized_wand, colorspace_wand);
  MagickModulateImage(wand, HUE, SATURATION, VALUE);
  MagickGammaImage(wand, GAMMA);

  colorized_wand = DestroyMagickWand(colorized_wand);
  colorspace_wand = DestroyMagickWand(colorspace_wand);

  return wand;
}

exec_mode_t get_mode(char *s) {
  if (strcmp("naive", s) == 0) {
    return NAIVE;
  } else if (strcmp("composer", s) == 0) {
    return COMPOSER;
  } else if (strcmp("parallel", s) == 0) {
    return PARALLEL;
  } else {
    return UNKNOWN;
  }
}

void print_usage(char **argv) {
  fprintf(stderr, "%s -i <filename> -m <mode> [-t <threads> -h]\n", argv[0]);
  fprintf(stderr, "Available modes:\n");
  fprintf(stderr, "\tnaive\n"
                  "\tcomposer\n"
                  );
}

int power_of_two(long x) {
  return x && !(x & (x - 1));
}

void parse_args(int argc, char **argv) {
  int opt;
  while ((opt = getopt(argc, argv, "i:m:p:t:h:o")) != -1) {
    switch (opt) {
      case 'i':
        sprintf(input_filename, "%s", optarg);
        break;
      case 'm':
        mode = get_mode(optarg);
        if (mode == UNKNOWN) {
          print_usage(argv);
          exit(EXIT_FAILURE);
        }
        break;
      case 't':
        threads = atol(optarg);
        break;
      case 'o':
        write_out = 1;
        break;
      case 'p':
        pieces = atol(optarg);
        if (pieces < 0) {
          fprintf(stderr, "pieces must be > 0\n");
          exit(EXIT_FAILURE);
        }
        break;
      case 'h':
      default:
        print_usage(argv);
        exit(EXIT_FAILURE);
    }
  }
}

int main(int argc,char **argv) {

  parse_args(argc, argv);
  if (mode == UNKNOWN || strlen(input_filename) == 0) {
    print_usage(argv);
    exit(EXIT_FAILURE);
  }

  struct stat s;
  if (stat(input_filename, &s) == -1) {
    perror("Input file error");
    exit(EXIT_FAILURE);
  }

  // Need to call this before any of the other library functions.
  if (mode == COMPOSER) {
    omp_set_num_threads(threads);
    composer_init(threads, pieces);
  } else {
    omp_set_num_threads(threads);
  }

  printf("Input file: %s (%ld bytes) Piece Size: %d Threads: %ld Mode: %d\n",
      input_filename, s.st_size, pieces, threads, mode);

  MagickWandGenesis();

  MagickWand *wand = NewMagickWand();

  printf("Reading image...");
  fflush(stdout);
  MagickReadImage(wand, input_filename);
  printf("done.\n");
  fflush(stdout);

  MagickWand *result;

  struct timeval start, end, diff;
  gettimeofday(&start, NULL);

  // Run function
  switch (mode) {
    case NAIVE:
      result = colortone_simple(wand, "#222b6d", "50,50", 1);
      break;
    case PARALLEL:
      result = colortone_parallel(wand, "#222b6d", "50,50", 1, threads);
      break;
    case COMPOSER:
      result = colortone_composer(wand, "#222b6d", "50,50", 1);
      break;
    case UNKNOWN:
    default:
      fprintf(stderr, "unsupported case");
      exit(EXIT_FAILURE);
  }
  gettimeofday(&end, NULL);

  timersub(&end, &start, &diff);
  double runtime = (double)diff.tv_sec + ((double)diff.tv_usec / 1000000.0);

  printf("%f seconds\n", runtime);
  fflush(stderr);

  if (write_out) {
    printf("Writing image...");
    fflush(stdout);
    char output[256];
    sprintf(output, "output-%d.jpg", mode);
    // MagickWriteImage(result, output);
    printf("done.\n");
    fflush(stdout);
  }

  wand = DestroyMagickWand(wand);
  result = DestroyMagickWand(result);
  MagickWandTerminus();
}
