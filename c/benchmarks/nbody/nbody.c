
#include <stdlib.h>
#include <stdio.h>

#include <assert.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>

#include <composer.h>
#include <mkl.h>
#include <omp.h>
#include <vec.h>

#include "nbody.h"
#include "nbody_mkl.h"
#include "nbody_composer.h"

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
size_t data_size = 4096;
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

void print_matrix(int n, const double *v) {
  printf("-------------------\n");
  for (int i = 0; i < n; i++) {
    printf("[ ");
    for (int j = 0; j < n; j++) {
      printf("%.8e ", v[i*n + j]);
    }
    printf("]\n");
  }
}

void print_vector(int n, const double *v) {
  printf("[ ");
  for (int i = 0; i < n; i++) {
    printf("%.8e ", v[i]);
  }
  printf("]\n");
}

galaxy_t inputs(long n, int lazy) {
  vec_t m = vvals(n, 1.0, 0);
  vec_t x = vvals(n, 1.0, 0);
  vec_t y = vvals(n, 1.0, 0);
  vec_t z = vvals(n, 1.0, 0);

  double step = 1.0 / (double)n;
  for (int i = 0; i < n; i++) {
    m.data[i] = (((double)i * step) + 10.0) * m_sol / 10.0;
    x.data[i] = (((double)i * step) - 0.5) * r_ly / 100;
    y.data[i] = (((double)i * step) - 0.5) * r_ly / 100;
    z.data[i] = (((double)i * step) - 0.5) * r_ly / 100;
  }

  if (lazy) {
    composer_tolazy(m.data);
    composer_tolazy(x.data);
    composer_tolazy(y.data);
    composer_tolazy(z.data);
  }

  vec_t vx = vvals(n, 0.0, lazy);
  vec_t vy = vvals(n, 0.0, lazy);
  vec_t vz = vvals(n, 0.0, lazy);

  galaxy_t inp;
  inp.n = n;
  inp.m = m.data;
  inp.x = x.data;
  inp.y = y.data;
  inp.z = z.data;
  inp.vx = vx.data;
  inp.vy = vy.data;
  inp.vz = vz.data;

  return inp;
}

/** Performs a NumPy style x - x.T subtraction over a vector. */
void set_delta(MKL_INT n, const double *x, double *out) {
  #pragma omp parallel for
  for (int i = 0; i < n; i++) {
    double subtract = x[i];
    for (int j = 0; j < n; j++) {
      out[i*n+j] = x[j] - subtract;
    }
  }
}

/** Performs a NumPy style x * x.T multiplication over a vector. */
void set_pm(MKL_INT n, const double *x, double *out) {
  #pragma omp parallel for
  for (int i = 0; i < n; i++) {
    double mul = x[i];
    for (int j = 0; j < n; j++) {
      out[i*n+j] = x[j] * mul;
    }
  }
}

int power_of_two(long x) {
  return x && !(x & (x - 1));
}

void print_usage(char **argv) {
  fprintf(stderr, "%s -m <mode> [-t <threads> -p <piece size> -s <log2 elements> -h]\n", argv[0]);
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
    mkl_set_num_threads(threads);
    omp_set_num_threads(threads);
  } else {
  }

  printf("Data Size: %ld Iterations: %d, Piece Size: %ld Threads: %ld Mode: %d\n",
      data_size, iterations, piece_size, threads, mode);

  // Generate inputs.
  fprintf(stderr, "Initializing...");
  fflush(stdout);
  int lazy = (mode == MKL_COMPOSER);

  // Create inputs.
  galaxy_t inp = inputs(data_size, lazy);

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
        run_mkl(iterations, inp.n, inp.m, inp.x, inp.y, inp.z, inp.vx, inp.vy, inp.vz);
        break;
      case MKL_COMPOSER:
        run_mkl_composer(iterations, inp.n, inp.m, inp.x, inp.y, inp.z, inp.vx, inp.vy, inp.vz);
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

    // This should match the reference Python solution.
    double result = 0;
    for (int i = 0; i < inp.n; i++) {
      result += inp.x[i] + inp.y[i] + inp.z[i];
    }
    printf("Result: %.11e\n", result);

    fprintf(stderr, "\n");
    printf("%f seconds\n", runtime);
    fflush(stderr);
    fflush(stdout);
}

