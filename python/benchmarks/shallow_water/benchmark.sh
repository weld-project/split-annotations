#!/bin/bash

set -x

source ../benchmarks/bin/activate

size=14
runs=${1:-1}
iterations=10
tasks=( numba composer naive bohrium )
threads=( 1 2 4 8 16 )

for task in "${tasks[@]}"; do 
  rm -f $task.stdout $task.stderr
  git log | head -1 > $task.stderr
  git log | head -1 > $task.stdout
done

for i in {1..$runs}; do
  for nthreads in "${threads[@]}"; do 
    NUMBA_NUM_THREADS=$nthreads python shallow_water_numba.py -s $size -i $iterations >> numba.stdout 2>> numba.stderr
  done
done

for i in {1..$runs}; do
  for nthreads in "${threads[@]}"; do 
    OMP_NUM_THREADS=$nthreads python shallow_water.py -m bohrium -s $size -i $iterations -t $nthreads >> bohrium.stdout 2>> bohrium.stderr
  done
done

unset OMP_NUM_THREADS

for i in {1..$runs}; do
  for nthreads in "${threads[@]}"; do 
    python shallow_water.py -m composer -s $size -i $iterations -t $nthreads >> composer.stdout 2>> composer.stderr
  done
done

for i in {1..$runs}; do
  python shallow_water.py -m naive -s $size -i $iterations -t 1 >> naive.stdout 2>> naive.stderr
done
