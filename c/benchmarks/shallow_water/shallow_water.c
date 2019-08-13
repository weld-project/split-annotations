/**
 * shallow_water.c
 *
 */

#include <stdlib.h>
#include <stdio.h>

#include <assert.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>

#include <mkl.h>
#include <vec.h>
#include <composer.h>

#include <omp.h>

#include "shallow_water.h"
#include "shallow_water_mkl.h"
#include "shallow_water_composer.h"

typedef enum {
  UNKNOWN = 0,
  FUSED,
  MKL,
  MKL_COMPOSER,
} exec_mode_t;

// Piece size for pipelined execution.
long piece_size = 4096;
// Number of threads.
long threads = 1;
// Data size as a matrix dimension.
size_t data_size = 4096L;
// Number of iterations to run for.
int iterations = 1;
// Mode to use
exec_mode_t mode;

exec_mode_t get_mode(char *s) {
  if (strcmp("fused", s) == 0) {
    return FUSED;
  } else if (strcmp("mkl", s) == 0) {
    return MKL;
  } else if (strcmp("mklcomposer", s) == 0) {
    return MKL_COMPOSER;
  } else {
    return UNKNOWN;
  }
}

input_t inputs(long n, int lazy) {
  vec_t u = vvals(n*n, 0.0, lazy);
  vec_t v = vvals(n*n, 0.0, lazy);

  vec_t eta = vvals(n*n, 1.0, 0);
  // Initialize eta.
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      eta.data[n*i + j] = 0.1 * i;
    }
  }
  if (lazy) {
    composer_tolazy(eta.data);
  }

  input_t inp;
  inp.n = n;
  inp.u = u.data;
  inp.v = v.data;
  inp.eta = eta.data;
  inp.g = 1.0;
  inp.b = 0.0;

  inp.grid_spacing = 1.0 / n;
  inp.dt = inp.grid_spacing / 100.0;

  return inp;
}

inline int posmod(int i, int n) {
  return (i % n + n) % n;
}

void roll(
    // Inputs
    MKL_INT n, const double *restrict input, int axis, int amount,
    // Output
    double *restrict output) {

  if (amount == 0) {
    memcpy(output, input, sizeof(double) * n * n);
  } else if (axis == 1) {
    #pragma omp parallel for num_threads(8)
    for (int i = 0; i < n; i++) {
      const double *restrict input_row = input + i*n;
      double *restrict output_row = output + i*n;
      if (amount > 0) {
        memcpy(output_row + amount, input_row, sizeof(double) * (n - amount));
        memcpy(output_row, input_row + (n - amount), sizeof(double) * amount);
      } else {
        amount = abs(amount);
        memcpy(output_row, input_row + amount, sizeof(double) * (n - amount));  
        memcpy(output_row + (n - amount), input_row, sizeof(double) * amount);  
      }
    }
  } else if (axis == 0) {
    if (amount > 0) {
      memcpy(output + n * amount, input, sizeof(double) * n * (n - amount));
      memcpy(output, input + n * (n - amount), sizeof(double) * n * amount);
    } else {
      amount = abs(amount);
      memcpy(output, input + n * amount, sizeof(double) * n * (n - amount));
      memcpy(output + n * (n - amount), input, sizeof(double) * n * amount);
    }
  } else {
    fprintf(stderr, "invalid axis=%d in %s()", axis, __func__);
    exit(1);
  }
}

void print_matrix(int n, const double *v) {
  printf("-------------------\n");
  for (int i = 0; i < n; i++) {
    printf("[ ");
    for (int j = 0; j < n; j++) {
      printf("%.5f ", v[i*n + j]);
    }
    printf("]\n");
  }
}

int power_of_two(long x) {
  return x && !(x & (x - 1));
}

void print_usage(char **argv) {
  fprintf(stderr, "%s -m <mode> [-t <threads> -p <piece size> -s <matrix width/length> -h]\n", argv[0]);
  fprintf(stderr, "Available modes:\n");
  fprintf(stderr, "\tfused\n"
                  "\tmkl\n"
                  "\tmklcomposer\n"
                  );
}

void parse_args(int argc, char **argv) {
  int opt;
  while ((opt = getopt(argc, argv, "m:t:p:s:h:i:")) != -1) {
    switch (opt) {
      case 'm':
        mode = get_mode(optarg);
        if (mode == UNKNOWN) {
          print_usage(argv);
          exit(EXIT_FAILURE);
        }
        break;
      case 'p':
        piece_size = atol(optarg);
        break;
      case 't':
        threads = atol(optarg);
        if (!power_of_two(threads) || threads > 40) {
          fprintf(stderr, "threads must be power-of-2 and < 16\n");
          exit(EXIT_FAILURE);
        }
        break;
      case 'i':
        iterations = atol(optarg);
        break;
      case 's':
        data_size = atol(optarg);
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

  if (iterations <= 0) {
    fprintf(stderr, "iterations must be greater than 0.\n");
    exit(EXIT_FAILURE);
  }

  // Need to call this before any of the other library functions.
  if (mode == MKL_COMPOSER) {
    composer_init(threads, piece_size);
    omp_set_num_threads(threads);
    mkl_set_num_threads(1);
  } else if (mode == MKL) {
    omp_set_num_threads(threads);
    mkl_set_num_threads(threads);
  } else {
    fprintf(stderr, "Unknown mode\n");
    exit(1);
  }

  printf("Data Size: %ld Iterations: %d, Piece Size: %ld Threads: %ld Mode: %d\n",
      data_size, iterations, piece_size, threads, mode);

  // Generate inputs.
  fprintf(stderr, "Initializing...");
  fflush(stdout);
  int lazy = (mode == MKL_COMPOSER);

  // Create inputs.
  input_t inp = inputs(data_size, lazy);

  fprintf(stderr, "done.\n");
  fflush(stdout);

  fprintf(stderr, "Total working set bytes: %ld\n", data_size*data_size * sizeof(double) * 10);

  fprintf(stderr, "--------------------\n");
    struct timeval start, end, diff;
    gettimeofday(&start, NULL);

    // Run function
    switch (mode) {
      case FUSED:
        fprintf(stderr, "unimplemented\n");
        exit(1);
        break;
      case MKL:
        run_mkl(iterations, inp.n, inp.eta, inp.u, inp.v, inp.g, inp.b, inp.dt, inp.grid_spacing);
        break;
      case MKL_COMPOSER:
        run_mkl_composer(iterations, inp.n, inp.eta, inp.u, inp.v, inp.g, inp.b, inp.dt, inp.grid_spacing);
        break;
      case UNKNOWN:
      default:
        fprintf(stderr, "unsupported case");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "Evaluating lazy calls...\n");
    fflush(stderr);

    gettimeofday(&end, NULL);

    timersub(&end, &start, &diff);
    double runtime = (double)diff.tv_sec + ((double)diff.tv_usec / 1000000.0);

    // Print the results.
    //print_matrix(inp.n, inp.eta);
    printf("First number: %f\n", inp.eta[0]);

    fprintf(stderr, "\n");
    printf("%f seconds\n", runtime);
    fflush(stderr);
    fflush(stdout);
}

