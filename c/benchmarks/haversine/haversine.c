/**
 * haversine.c
 *
 * Computes the Haversine distance workload.
 *
 */

#include <assert.h>

#include <stdlib.h>
#include <stdio.h>

#include <math.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>

// To set dependent parameters.
#include "mkl_vml_functions.h"
#include "mkl.h"

// Extensions that support immediates, etc.
#include <mkl_extensions.h>
#include <vec.h>
#include <generated/generated.h>
#include <composer.h>

#include <omp.h>

typedef enum {
  UNKNOWN = 0,
  MKL,
  MKL_COMPOSER,
  NOPIPE,
} exec_mode_t;

// Piece size for pipelined execution.
long piece_size = 4096;
// Number of threads.
long threads = 1;
// Dumps as CSV data if true.
int dump = 0;
// Size of the input.
size_t data_size = (1L << 26);
// Mode to use
exec_mode_t mode;

exec_mode_t get_mode(const char *s) {
  if (strcmp("mkl", s) == 0) {
    return MKL;
  } else if (strcmp("mklcomposer", s) == 0) {
    return MKL_COMPOSER;
  } else if (strcmp("nopipe", s) == 0) {
    return NOPIPE;
  } else {
    return UNKNOWN;
  }
}

vec_t run_mkl(double lat1, double lon1, vec_t vlat2, vec_t vlon2) {

  double MILES_CONST = 3959.0;

  vec_t va = new_vec(vlat2.length, 0);
  vec_t vdlat = new_vec(vlat2.length, 0);
  vec_t vdlon = new_vec(vlat2.length, 0);

  double *lat2 = vlat2.data;
  double *lon2 = vlon2.data;
  double *a = va.data;
  double *dlat = vdlat.data;
  double *dlon = vdlon.data;

  MKL_INT len = vlat2.length;

  vdSubi(len, lat2, lat1, dlat);
  vdSubi(len, lon2, lon1, dlon);

  // dlat = sin(dlat / 2.0) ** 2.0
  vdDivi(len, dlat, 2.0, dlat);
  vdSin(len, dlat, dlat);
  vdMul(len, dlat, dlat, dlat);

  // a = cos(lat1) * cos(len, lat2)
  double lat1_cos = cos(lat1);
  vdCos(len, lat2, a);
  vdMuli(len, a, lat1_cos, a);

  // a = a + sin(len, dlon / 2.0) ** 2.0
  vdDivi(len, dlon, 2.0, dlon);
  vdSin(len, dlon, dlon);
  vdMul(len, dlon, dlon, dlon);
  vdMul(len, a, dlon, a);
  vdAdd(len, dlat, a, a);

  double *c = a;
  vdSqrt(len, a, a);
  vdAsin(len, a, a);
  vdMuli(len, a, 2.0, c);

  double *mi = c;
  vdMuli(len, c, MILES_CONST, mi);
  return va;
}

vec_t run_mklcomposer_nopipe(double lat1, double lon1, vec_t vlat2, vec_t vlon2) {

  double MILES_CONST = 3959.0;

  vec_t va = new_vec(vlat2.length, 1);
  vec_t vdlat = new_vec(vlat2.length, 0);
  vec_t vdlon = new_vec(vlat2.length, 0);

  double *lat2 = vlat2.data;
  double *lon2 = vlon2.data;
  double *a = va.data;
  double *dlat = vdlat.data;
  double *dlon = vdlon.data;
  MKL_INT len = vlat2.length;

  //TEMPORARY(&dlat, sizeof(dlat));
  //TEMPORARY(&dlon, sizeof(dlon));

  c_vdSubi(len, lat2, lat1, dlat);
  composer_execute();
  c_vdSubi(len, lon2, lon1, dlon);
  composer_execute();

  // dlat = sin(dlat / 2.0) ** 2.0
  c_vdDivi(len, dlat, 2.0, dlat);
  composer_execute();
  c_vdSin(len, dlat, dlat);
  composer_execute();
  c_vdMul(len, dlat, dlat, dlat);
  composer_execute();

  // a = cos(lat1) * cos(len, lat2)
  double lat1_cos = cos(lat1);
  c_vdCos(len, lat2, a);
  composer_execute();
  c_vdMuli(len, a, lat1_cos, a);
  composer_execute();

  // a = a + sin(len, dlon / 2.0) ** 2.0
  c_vdDivi(len, dlon, 2.0, dlon);
  composer_execute();
  c_vdSin(len, dlon, dlon);
  composer_execute();
  c_vdMul(len, dlon, dlon, dlon);
  composer_execute();
  c_vdMul(len, a, dlon, a);
  composer_execute();
  c_vdAdd(len, dlat, a, a);
  composer_execute();

  double *c = a;
  c_vdSqrt(len, a, a);
  composer_execute();
  c_vdAsin(len, a, a);
  composer_execute();
  c_vdMuli(len, a, 2.0, c);
  composer_execute();

  double *mi = c;
  c_vdMuli(len, c, MILES_CONST, mi);
  composer_execute();

  return va;
}

vec_t run_mklcomposer(double lat1, double lon1, vec_t vlat2, vec_t vlon2) {

  double MILES_CONST = 3959.0;

  vec_t va = new_vec(vlat2.length, 1);
  vec_t vdlat = new_vec(vlat2.length, 0);
  vec_t vdlon = new_vec(vlat2.length, 0);

  double *lat2 = vlat2.data;
  double *lon2 = vlon2.data;
  double *a = va.data;
  double *dlat = vdlat.data;
  double *dlon = vdlon.data;
  MKL_INT len = vlat2.length;

  c_vdSubi(len, lat2, lat1, dlat);
  c_vdSubi(len, lon2, lon1, dlon);

  // dlat = sin(dlat / 2.0) ** 2.0
  c_vdDivi(len, dlat, 2.0, dlat);
  c_vdSin(len, dlat, dlat);
  c_vdMul(len, dlat, dlat, dlat);

  // a = cos(lat1) * cos(len, lat2)
  double lat1_cos = cos(lat1);
  c_vdCos(len, lat2, a);
  c_vdMuli(len, a, lat1_cos, a);

  // a = a + sin(len, dlon / 2.0) ** 2.0
  c_vdDivi(len, dlon, 2.0, dlon);
  c_vdSin(len, dlon, dlon);
  c_vdMul(len, dlon, dlon, dlon);
  c_vdMul(len, a, dlon, a);
  c_vdAdd(len, dlat, a, a);

  double *c = a;
  c_vdSqrt(len, a, a);
  c_vdAsin(len, a, a);
  c_vdMuli(len, a, 2.0, c);

  double *mi = c;
  c_vdMuli(len, c, MILES_CONST, mi);

  return va;
}

int power_of_two(long x) {
  return x && !(x & (x - 1));
}

void print_usage(char **argv) {
  fprintf(stderr, "%s -m <mode> [-t <threads> -p <piece size> -s <data size> -h]\n", argv[0]);
  fprintf(stderr, "Available modes:\n");
  fprintf(stderr, "\tmkl\n"
                  "\tmklcomposer\n"
                  );
}

void parse_args(int argc, char **argv) {
  int opt;
  while ((opt = getopt(argc, argv, "m:t:p:s:dh")) != -1) {
    switch (opt) {
      case 'm':
        mode = get_mode(optarg);
        if (mode == UNKNOWN) {
          print_usage(argv);
          exit(EXIT_FAILURE);
        }
        break;
      case 'd':
        dump = 1;
        break;
      case 'p':
        piece_size = atol(optarg);
        break;
      case 't':
        threads = atol(optarg);
        if (!power_of_two(threads) || threads >= 64) {
          fprintf(stderr, "threads must be power-of-2 and <= 64\n");
          exit(EXIT_FAILURE);
        }
        break;
      case 's':
        data_size = atol(optarg);
        if (data_size > 30 || data_size <= 0) {
          fprintf(stderr, "data size must be 1 <= data_size <= 31\n");
          exit(EXIT_FAILURE);
        }
        data_size = (1L << data_size);
        break;
      case 'h':
      default:
        print_usage(argv);
        exit(EXIT_FAILURE);
    }
  }
}

int main(int argc, char **argv) {
  parse_args(argc, argv);

  if (mode == UNKNOWN) {
    print_usage(argv);
    exit(EXIT_FAILURE);
  }

  if (data_size < piece_size) {
    fprintf(stderr, "data_size must be a multiple of piece_size, and larger than piece_size.\n");
    exit(EXIT_FAILURE);
  }

  // Need to call this before any of the other library functions.
  if (mode == MKL_COMPOSER || mode == NOPIPE) {
    mkl_set_num_threads(1);  
    omp_set_num_threads(1);  
    composer_init(threads, piece_size);
  } else if (mode == MKL) {
    mkl_set_num_threads(threads);  
    omp_set_num_threads(threads);  
  } else {
    //omp_set_num_threads(threads);
  }

  printf("Data Size: %ld Piece Size: %ld Threads: %ld Mode: %d\n",
      data_size, piece_size, threads, mode);

  // Generate inputs.
  fprintf(stderr, "Initializing...");
  fflush(stdout);
  int lazy = (mode == MKL_COMPOSER || mode == NOPIPE);
  vec_t lat = vvals(data_size, 0.0698132, lazy);
  vec_t lon = vvals(data_size, 0.0698132, lazy);

  fprintf(stderr, "done.\n");
  fflush(stdout);

  fprintf(stderr, "Allocated Input Bytes: %ld\n", data_size * sizeof(double) * 2);

  fprintf(stderr, "--------------------\n");
    struct timeval start, end, diff;
    gettimeofday(&start, NULL);

    const double lat1 = 0.70984286;
    const double lon1 = -1.23892197;

    // Run function
    vec_t result;
    switch (mode) {
      case MKL:
        result = run_mkl(lat1, lon1, lat, lon);
        break;
      case MKL_COMPOSER:
        result = run_mklcomposer(lat1, lon1, lat, lon);
        break;
      case NOPIPE:
        result = run_mklcomposer_nopipe(lat1, lon1, lat, lon);
        break;
      case UNKNOWN:
      default:
        fprintf(stderr, "unsupported case");
        exit(EXIT_FAILURE);
    }

    // Required to force Composer's lazy evaluation for timing.
    double first = result.data[0];
    fprintf(stderr, "First value: %f\n", first);
    fflush(stderr);

    gettimeofday(&end, NULL);
    timersub(&end, &start, &diff);
    double runtime = (double)diff.tv_sec + ((double)diff.tv_usec / 1000000.0);
    for (int j = 0; j < 5; j++) {
      long index = rand() % data_size;
      fprintf(stderr, "(%f, %f) ", result.data[index], result.data[index]);
    }
    fprintf(stderr, "\n");
    printf("%f seconds\n", runtime);
    fflush(stderr);
    fflush(stdout);
}

