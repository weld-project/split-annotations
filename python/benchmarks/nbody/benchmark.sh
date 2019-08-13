#!/bin/bash

set -x

source ../benchmarks/bin/activate

runs=${1:-1}
size=15
iterations=5
tasks=( numba naive composer bohrium )
threads=( 1 2 4 8 16 )

for task in "${tasks[@]}"; do 
  rm -f $task.stdout $task.stderr
  git log | head -1 > $task.stderr
  git log | head -1 > $task.stdout
done

for i in {1..$runs}; do
  for nthreads in "${threads[@]}"; do 
    NUMBA_NUM_THREADS=$nthreads python nbody_numba.py -s $size -i $iterations >> numba.stdout 2>> numba.stderr
  done
done

for i in {1..$runs}; do
  for nthreads in "${threads[@]}"; do 
    python nbody.py -m composer -s $size -i $iterations -t $nthreads >> composer.stdout 2>> composer.stderr
  done
done

for i in {1..$runs}; do
  python nbody.py -m naive -s $size -i $iterations -t $nthreads >> naive.stdout 2>> naive.stderr
done

for i in {1..$runs}; do
  for nthreads in "${threads[@]}"; do 
    OMP_NUM_THREADS=$nthreads python nbody_boh.py -s $size -i $iterations -t $nthreads >> bohrium.stdout 2>> bohrium.stderr
  done
done
