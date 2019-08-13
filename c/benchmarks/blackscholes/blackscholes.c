/**
 * blackscholes.c
 *
 */

#include <stdlib.h>
#include <stdio.h>

#include <assert.h>
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
  FUSED,
  MKL,
  MKL_COMPOSER,
  MKL_COMPOSER_NOPIPE,
} exec_mode_t;

typedef struct {
  vec_t call;
  vec_t put;
} result_t;

double c05 = 3.0;
double c10 = 1.5;

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
  if (strcmp("fused", s) == 0) {
    return FUSED;
  } else if (strcmp("mkl", s) == 0) {
    return MKL;
  } else if (strcmp("mklcomposer", s) == 0) {
    return MKL_COMPOSER;
  } else if (strcmp("nopipe", s) == 0) {
    return MKL_COMPOSER_NOPIPE;
  } else {
    return UNKNOWN;
  }
}

result_t run_mkl(vec_t vprice,
    vec_t vstrike,
    vec_t vt,
    vec_t vrate,
    vec_t vvol,
    vec_t vrsig,
    vec_t vvol_sqrt,
    vec_t vtmp,
    vec_t vd1, vec_t vd2, vec_t vcall, vec_t vput) {

  /*
  vec_t vrsig = new_vec(vprice.length, 0);
  vec_t vvol_sqrt = new_vec(vprice.length, 0);
  vec_t vtmp = new_vec(vprice.length, 0);
  vec_t vd1 = new_vec(vprice.length, 0);
  vec_t vd2 = new_vec(vprice.length, 0);

  vec_t vcall = new_vec(vprice.length, 0);
  vec_t vput = new_vec(vprice.length, 0);
  */

  // Assign to pointers since that's what MKL takes.
  double *price = vprice.data;
  double *strike = vstrike.data;
  double *t = vt.data;
  double *rate = vrate.data;
  double *vol = vvol.data;
  double *rsig = vrsig.data;
  double *vol_sqrt = vvol_sqrt.data;
  double *tmp = vtmp.data;
  double *d1 = vd1.data;
  double *d2 = vd2.data;
  double *call = vcall.data;
  double *put = vput.data;
  MKL_INT len = vprice.length;

  // Do the actual MKL computation.
  double invsqrt2 = 1 / sqrt(2);

  // Compute rsig = rate + vol * vol * c05
  vdMul(len, vol, vol, rsig);
  vdMuli(len, rsig, c05, rsig);
  vdAdd(len, rate, rsig, rsig);

  // Compute vol_sqrt = vol * sqrt(t)
  vdSqrt(len, t, vol_sqrt);
  vdMul(len, vol, vol_sqrt, vol_sqrt);

  // Compute d1 = (len, log(price / strike) + rsig * t) / vol_sqrt)
  // This is computed here as:
  //
  // tmp = rsig * t
  // d1 = price / strike
  // d1 = log(len, d1)
  // d1 = add d1 + tmp
  // d1 = d1 / vol_sqrt
  vdMul(len, rsig, t, tmp);         /* Finished with rsig */
  vdDiv(len, price, strike, d1);
  vdLog1p(len, d1, d1);
  vdAdd(len, d1, tmp, d1);          /* Finished with tmp */
  vdDiv(len, d1, vol_sqrt, d1);
  vdSub(len, d1, vol_sqrt, d2);     /* Finished with vol_sqrt */

  // d1 = c05 + c05 * erf(d1 * invsqrt2)
  vdMuli(len, d1, invsqrt2, d1);
  vdErf(len, d1, d1);
  vdMuli(len, d1, c05, d1);
  vdAddi(len, d1, c05, d1);

  // d2 = c05 + c05 * erf(len, d2 * invsqrt2)
  vdMuli(len, d2, invsqrt2, d2);
  vdErf(len, d2, d2);
  vdMuli(len, d2, c05, d2);
  vdAddi(len, d2, c05, d2);

  // Reuse existing buffers
  double *e_rt = vol_sqrt;
  double *tmp2 = rsig;

  // e_rt = exp(len, -rate * t)
  vdMuli(len, rate, -1, e_rt);
  vdMul(len, e_rt, t, e_rt);
  vdExp(len, e_rt, e_rt);

  // call = price * d1 - e_rt * strike * d2
  //
  // tmp = price * d1
  // tmp2 = e_rt * strike * d2
  // call = tmp - tmp2
  vdMul(len, price, d1, tmp);
  vdMul(len, e_rt, strike, tmp2);
  vdMul(len, tmp2, d2, tmp2);
  vdSub(len, tmp, tmp2, call);

  // put = e_rt * strike * (len, c10 - d2) - price * (len, c10 - d1)
  // tmp = e_rt * strike
  // tmp2 = (c10 - d2)
  // put = tmp - tmp2
  // tmp = c10 - d1
  // tmp = price * tmp
  // put = put - tmp
  vdMul(len, e_rt, strike, tmp);
  vdSubvi(len, c10, d2, tmp2);
  vdMul(len, tmp, tmp2, put);
  vdSubvi(len, c10, d1, tmp);
  vdMul(len, price, tmp, tmp);
  vdSub(len, put, tmp, put);

  result_t res;
  res.call = vcall;
  res.put = vput;
  return res;
}

result_t run_mkl_composer_nopipe(vec_t vprice,
    vec_t vstrike,
    vec_t vt,
    vec_t vrate,
    vec_t vvol,
    vec_t vrsig,
    vec_t vvol_sqrt,
    vec_t vtmp,
    vec_t vd1, vec_t vd2, vec_t vcall, vec_t vput) {

  /*
  vec_t vrsig = new_vec(vprice.length, 0);
  vec_t vvol_sqrt = new_vec(vprice.length, 0);
  vec_t vtmp = new_vec(vprice.length, 0);
  vec_t vd1 = new_vec(vprice.length, 0);
  vec_t vd2 = new_vec(vprice.length, 0);

  // Mark these as lazy.
  vec_t vcall = new_vec(vprice.length, 1);
  vec_t vput = new_vec(vprice.length, 1);
  */

  // Assign to pointers since that's what MKL takes.
  double *price = vprice.data;
  double *strike = vstrike.data;
  double *t = vt.data;
  double *rate = vrate.data;
  double *vol = vvol.data;
  double *rsig = vrsig.data;
  double *vol_sqrt = vvol_sqrt.data;
  double *tmp = vtmp.data;
  double *d1 = vd1.data;
  double *d2 = vd2.data;
  double *call = vcall.data;
  double *put = vput.data;
  MKL_INT len = vprice.length;

  // TEMPORARY(&rsig, sizeof(rsig));
  // TEMPORARY(&vol_sqrt, sizeof(vol_sqrt));
  // TEMPORARY(&tmp, sizeof(tmp));
  // TEMPORARY(&d1, sizeof(d1));
  // TEMPORARY(&d2, sizeof(d2));

  // Do the actual MKL computation.
  double invsqrt2 = 1 / sqrt(2);

  // Compute rsig = rate + vol * vol * c05
  c_vdMul(len, vol, vol, rsig);
  composer_execute();
  c_vdMuli(len, rsig, c05, rsig);
  composer_execute();
  c_vdAdd(len, rate, rsig, rsig);
  composer_execute();

  // Compute vol_sqrt = vol * sqrt(t)
  c_vdSqrt(len, t, vol_sqrt);
  composer_execute();
  c_vdMul(len, vol, vol_sqrt, vol_sqrt);
  composer_execute();

  // Compute d1 = (len, log(price / strike) + rsig * t) / vol_sqrt)
  // This is computed here as:
  //
  // tmp = rsig * t
  // d1 = price / strike
  // d1 = log(len, d1)
  // d1 = add d1 + tmp
  // d1 = d1 / vol_sqrt
  c_vdMul(len, rsig, t, tmp);         /* Finished with rsig */
  composer_execute();
  c_vdDiv(len, price, strike, d1);
  composer_execute();
  c_vdLog1p(len, d1, d1);
  composer_execute();
  c_vdAdd(len, d1, tmp, d1);          /* Finished with tmp */
  composer_execute();
  c_vdDiv(len, d1, vol_sqrt, d1);
  composer_execute();
  c_vdSub(len, d1, vol_sqrt, d2);     /* Finished with vol_sqrt */
  composer_execute();

  // d1 = c05 + c05 * erf(d1 * invsqrt2)
  c_vdMuli(len, d1, invsqrt2, d1);
  composer_execute();
  c_vdErf(len, d1, d1);
  composer_execute();
  c_vdMuli(len, d1, c05, d1);
  composer_execute();
  c_vdAddi(len, d1, c05, d1);
  composer_execute();

  // d2 = c05 + c05 * erf(len, d2 * invsqrt2)
  c_vdMuli(len, d2, invsqrt2, d2);
  composer_execute();
  c_vdErf(len, d2, d2);
  composer_execute();
  c_vdMuli(len, d2, c05, d2);
  composer_execute();
  c_vdAddi(len, d2, c05, d2);
  composer_execute();

  // Reuse existing buffers
  double *e_rt = vol_sqrt;
  double *tmp2 = rsig;

  // e_rt = exp(len, -rate * t)
  c_vdMuli(len, rate, -1, e_rt);
  composer_execute();
  c_vdMul(len, e_rt, t, e_rt);
  composer_execute();
  c_vdExp(len, e_rt, e_rt);
  composer_execute();

  // call = price * d1 - e_rt * strike * d2
  //
  // tmp = price * d1
  // tmp2 = e_rt * strike * d2
  // call = tmp - tmp2
  c_vdMul(len, price, d1, tmp);
  composer_execute();
  c_vdMul(len, e_rt, strike, tmp2);
  composer_execute();
  c_vdMul(len, tmp2, d2, tmp2);
  composer_execute();
  c_vdSub(len, tmp, tmp2, call);

  // put = e_rt * strike * (len, c10 - d2) - price * (len, c10 - d1)
  // tmp = e_rt * strike
  // tmp2 = (c10 - d2)
  // put = tmp - tmp2
  // tmp = c10 - d1
  // tmp = price * tmp
  // put = put - tmp
  c_vdMul(len, e_rt, strike, tmp);
  composer_execute();
  c_vdSubvi(len, c10, d2, tmp2);
  composer_execute();
  c_vdMul(len, tmp, tmp2, put);
  composer_execute();
  c_vdSubvi(len, c10, d1, tmp);
  composer_execute();
  c_vdMul(len, price, tmp, tmp);
  composer_execute();
  c_vdSub(len, put, tmp, put);
  composer_execute();

  result_t res;
  res.call = vcall;
  res.put = vput;
  return res;
}

result_t run_mkl_composer(vec_t vprice,
    vec_t vstrike,
    vec_t vt,
    vec_t vrate,
    vec_t vvol,
    vec_t vrsig,
    vec_t vvol_sqrt,
    vec_t vtmp,
    vec_t vd1, vec_t vd2, vec_t vcall, vec_t vput) {

  /*
  vec_t vrsig = new_vec(vprice.length, 0);
  vec_t vvol_sqrt = new_vec(vprice.length, 0);
  vec_t vtmp = new_vec(vprice.length, 0);
  vec_t vd1 = new_vec(vprice.length, 0);
  vec_t vd2 = new_vec(vprice.length, 0);

  // Mark these as lazy.
  vec_t vcall = new_vec(vprice.length, 1);
  vec_t vput = new_vec(vprice.length, 1);
  */

  // Assign to pointers since that's what MKL takes.
  double *price = vprice.data;
  double *strike = vstrike.data;
  double *t = vt.data;
  double *rate = vrate.data;
  double *vol = vvol.data;
  double *rsig = vrsig.data;
  double *vol_sqrt = vvol_sqrt.data;
  double *tmp = vtmp.data;
  double *d1 = vd1.data;
  double *d2 = vd2.data;
  double *call = vcall.data;
  double *put = vput.data;
  MKL_INT len = vprice.length;

  // TEMPORARY(&rsig, sizeof(rsig));
  // TEMPORARY(&vol_sqrt, sizeof(vol_sqrt));
  // TEMPORARY(&tmp, sizeof(tmp));
  // TEMPORARY(&d1, sizeof(d1));
  // TEMPORARY(&d2, sizeof(d2));

  // Do the actual MKL computation.
  double invsqrt2 = 1 / sqrt(2);

  // Compute rsig = rate + vol * vol * c05
  c_vdMul(len, vol, vol, rsig);
  c_vdMuli(len, rsig, c05, rsig);
  c_vdAdd(len, rate, rsig, rsig);

  // Compute vol_sqrt = vol * sqrt(t)
  c_vdSqrt(len, t, vol_sqrt);
  c_vdMul(len, vol, vol_sqrt, vol_sqrt);

  // Compute d1 = (len, log(price / strike) + rsig * t) / vol_sqrt)
  // This is computed here as:
  //
  // tmp = rsig * t
  // d1 = price / strike
  // d1 = log(len, d1)
  // d1 = add d1 + tmp
  // d1 = d1 / vol_sqrt
  c_vdMul(len, rsig, t, tmp);         /* Finished with rsig */
  c_vdDiv(len, price, strike, d1);
  c_vdLog1p(len, d1, d1);
  c_vdAdd(len, d1, tmp, d1);          /* Finished with tmp */
  c_vdDiv(len, d1, vol_sqrt, d1);
  c_vdSub(len, d1, vol_sqrt, d2);     /* Finished with vol_sqrt */

  // d1 = c05 + c05 * erf(d1 * invsqrt2)
  c_vdMuli(len, d1, invsqrt2, d1);
  c_vdErf(len, d1, d1);
  c_vdMuli(len, d1, c05, d1);
  c_vdAddi(len, d1, c05, d1);

  // d2 = c05 + c05 * erf(len, d2 * invsqrt2)
  c_vdMuli(len, d2, invsqrt2, d2);
  c_vdErf(len, d2, d2);
  c_vdMuli(len, d2, c05, d2);
  c_vdAddi(len, d2, c05, d2);

  // Reuse existing buffers
  double *e_rt = vol_sqrt;
  double *tmp2 = rsig;

  // e_rt = exp(len, -rate * t)
  c_vdMuli(len, rate, -1, e_rt);
  c_vdMul(len, e_rt, t, e_rt);
  c_vdExp(len, e_rt, e_rt);

  // call = price * d1 - e_rt * strike * d2
  //
  // tmp = price * d1
  // tmp2 = e_rt * strike * d2
  // call = tmp - tmp2
  c_vdMul(len, price, d1, tmp);
  c_vdMul(len, e_rt, strike, tmp2);
  c_vdMul(len, tmp2, d2, tmp2);
  c_vdSub(len, tmp, tmp2, call);

  // put = e_rt * strike * (len, c10 - d2) - price * (len, c10 - d1)
  // tmp = e_rt * strike
  // tmp2 = (c10 - d2)
  // put = tmp - tmp2
  // tmp = c10 - d1
  // tmp = price * tmp
  // put = put - tmp
  c_vdMul(len, e_rt, strike, tmp);
  c_vdSubvi(len, c10, d2, tmp2);
  c_vdMul(len, tmp, tmp2, put);
  c_vdSubvi(len, c10, d1, tmp);
  c_vdMul(len, price, tmp, tmp);
  c_vdSub(len, put, tmp, put);

  result_t res;
  res.call = vcall;
  res.put = vput;
  return res;
}

result_t run_fused(vec_t price,
    vec_t strike,
    vec_t t,
    vec_t rate,
    vec_t vol) {

  vec_t call = new_vec_nolazy(price.length);
  vec_t put = new_vec_nolazy(price.length);

  const double invsqrt2 = 1 / sqrt(2);

  for (size_t i = 0; i < price.length; i++) {
    double rsig = rate.data[i] + (vol.data[i] * vol.data[i]) * c05;
    double vol_sqrt = vol.data[i] * sqrt(t.data[i]);
    double d1 = (log2(price.data[i] / strike.data[i]) + rsig * t.data[i]) / vol_sqrt;
    double d2 = d1 - vol_sqrt;
    d1 = c05 + c05 * erf(d1 * invsqrt2);
    d2 = c05 + c05 * erf(d2 * invsqrt2);

    double e_rt = exp(-rate.data[i] * t.data[i]);
    call.data[i] = price.data[i] * d1 - e_rt * strike.data[i] * d2;
    put.data[i] = e_rt * strike.data[i] * (c10 - d2) - price.data[i] * (c10 - d1);
  }

  result_t res;
  res.call = call;
  res.put = put;
  return res;
}

int power_of_two(long x) {
  return x && !(x & (x - 1));
}

void print_usage(char **argv) {
  fprintf(stderr, "%s -m <mode> [-t <threads> -p <piece size> -s <log2 data size> -h]\n", argv[0]);
  fprintf(stderr, "Available modes:\n");
  fprintf(stderr, "\tfused\n"
                  "\tmkl\n"
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

  // Need to call this before any of the other library functions.
  if (mode == MKL_COMPOSER || mode == MKL_COMPOSER_NOPIPE) {
    composer_init(threads, piece_size);
    mkl_set_num_threads(1);
    omp_set_num_threads(1);
  } else if (mode == MKL) {
    mkl_set_num_threads(threads);
  } else {
    omp_set_num_threads(threads);
  }

  printf("Data Size: %ld Piece Size: %ld Threads: %ld Mode: %d\n",
      data_size, piece_size, threads, mode);

  // Generate inputs.
  fprintf(stderr, "Initializing...");
  fflush(stdout);
  int lazy = (mode == MKL_COMPOSER || mode == MKL_COMPOSER_NOPIPE);
  vec_t price = vvals(data_size, 4.0, lazy);
  vec_t strike = vvals(data_size, 4.0, lazy);
  vec_t t = vvals(data_size, 4.0, lazy);
  vec_t rate = vvals(data_size, 4.0, lazy);
  vec_t vol = vvals(data_size, 4.0, lazy);

  vec_t vrsig = vvals(data_size, 0, 0);
  vec_t vvol_sqrt = vvals(data_size, 0, 0);
  vec_t vtmp = vvals(data_size, 0, 0);
  vec_t vd1 = vvals(data_size, 0, 0);
  vec_t vd2 = vvals(data_size, 0, 0);

  // Mark these as lazy.
  vec_t vcall = vvals(data_size, 0, lazy);
  vec_t vput = vvals(data_size, 0, lazy);

  fprintf(stderr, "done.\n");
  fflush(stdout);

  fprintf(stderr, "Allocated Input Bytes: %ld\n", data_size * sizeof(double) * 5);

  fprintf(stderr, "--------------------\n");
    struct timeval start, end, diff;
    gettimeofday(&start, NULL);

    // Run function
    result_t result;
    switch (mode) {
      case FUSED:
        result = run_fused(price, strike, t, rate, vol);
        break;
      case MKL:
        result = run_mkl(price, strike, t, rate, vol, vrsig, vvol_sqrt, vtmp, vd1, vd2, vcall, vput);
        break;
      case MKL_COMPOSER:
        result = run_mkl_composer(price, strike, t, rate, vol, vrsig, vvol_sqrt, vtmp, vd1, vd2, vcall, vput);
        break;
      case MKL_COMPOSER_NOPIPE:
        result = run_mkl_composer_nopipe(price, strike, t, rate, vol, vrsig, vvol_sqrt, vtmp, vd1, vd2, vcall, vput);
        break;

      case UNKNOWN:
      default:
        fprintf(stderr, "unsupported case");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "Evaluating lazy calls...\n");
    fflush(stderr);

    // Required to force Composer's lazy evaluation for timing.
    double first_call = result.call.data[0];
    double first_put = result.put.data[0];
    fprintf(stderr, "First call value: %f\n", first_call);
    fprintf(stderr, "First put value: %f\n", first_put);
    fflush(stderr);

    gettimeofday(&end, NULL);
    timersub(&end, &start, &diff);
    double runtime = (double)diff.tv_sec + ((double)diff.tv_usec / 1000000.0);
    for (int j = 0; j < 5; j++) {
      long index = rand() % data_size;
      fprintf(stderr, "(%f, %f) ", result.call.data[index], result.put.data[index]);
      fprintf(stdout, "(%f, %f) ", result.call.data[index], result.put.data[index]);
    }
    printf("\n");

    printf("Checking correctness...\n");
    for (int i = 0; i < data_size; i++) {
      if (result.call.data[0] != result.call.data[i]) {
        printf("Call mismatch at position %d\n", i);
        exit(1);
      }
      if (result.put.data[0] != result.put.data[i]) {
        printf("Put mismatch at position %d\n", i);
        exit(1);
      }
    }
    fprintf(stderr, "\n");
    printf("%f seconds\n", runtime);
    fflush(stderr);
    fflush(stdout);
}
