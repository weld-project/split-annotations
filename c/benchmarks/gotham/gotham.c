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

#include "gotham.h"
#include "gotham_composer.h"

typedef enum {
  UNKNOWN = 0,
  NAIVE,
  COMPOSER,
} exec_mode_t;

// Number of threads.
long threads = 1;
// Mode to use
exec_mode_t mode;
// input filename.
char input_filename[2048];
// TODO This should be autotuned/based on the input size.
int piece_size = 20;
// Should the result image be written to an output file?
//
// Used offline to check for correctness.
int write_out = 0;

MagickWand *do_gotham(MagickWand *input_wand) {
  // modulate 120,10,100
  MagickModulateImage(input_wand, HUE, SATURATION, VALUE);

  // colorize
  PixelWand *colorize = NewPixelWand();
  PixelWand *alpha = NewPixelWand();
	PixelSetColor(colorize,"#222b6d");
	PixelSetColor(alpha, "rgb(20%,20%,20%)");
	MagickColorizeImage(input_wand, colorize, alpha);
	MagickColorizeImage(input_wand, colorize, alpha);
	MagickColorizeImage(input_wand, colorize, alpha);
	MagickColorizeImage(input_wand, colorize, alpha);
	MagickColorizeImage(input_wand, colorize, alpha);

  // gamma 0.5
  MagickGammaImage(input_wand, GAMMA);

  // contrast
  MagickContrastImage(input_wand, 1);
  // contrast
  MagickContrastImage(input_wand, 1);

  DestroyPixelWand(alpha);
  DestroyPixelWand(colorize);
}

MagickWand *gotham_simple(MagickWand *input_wand) {
  do_gotham(input_wand);

  return input_wand;
}

exec_mode_t get_mode(char *s) {
  if (strcmp("naive", s) == 0) {
    return NAIVE;
  } else if (strcmp("composer", s) == 0) {
    return COMPOSER;
  } else {
    return UNKNOWN;
  }
}

void print_usage(char **argv) {
  fprintf(stderr, "%s -i <filename> -m <mode> [-t <threads> -h -o <enables writing out result>]\n", argv[0]);
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
  while ((opt = getopt(argc, argv, "i:m:t:h:o")) != -1) {
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
        if (!power_of_two(threads) || threads > 40) {
          fprintf(stderr, "threads must be power-of-2 and < 16\n");
          exit(EXIT_FAILURE);
        }
        break;
      case 'o':
        write_out = 1;
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
    composer_init(threads, piece_size);
    omp_set_num_threads(1);
  } else {
    omp_set_num_threads(threads);
  }

  printf("Input file: %s (%ld bytes) Piece Size: %d Threads: %ld Mode: %d\n",
      input_filename, s.st_size, piece_size, threads, mode);

  MagickWandGenesis();

  MagickWand *wand = NewMagickWand();

  printf("Reading image...");
  fflush(stdout);
  MagickReadImage(wand, input_filename);
  printf("done.\n");
  fflush(stdout);

  struct timeval start, end, diff;
  gettimeofday(&start, NULL);

  // Run function
  switch (mode) {
    case NAIVE:
      wand = gotham_simple(wand);
      break;
    case COMPOSER:
      wand = gotham_composer(wand);
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
    MagickWriteImage(wand, output);
    printf("done.\n");
    fflush(stdout);
  }

  wand = DestroyMagickWand(wand);
  MagickWandTerminus();
}
