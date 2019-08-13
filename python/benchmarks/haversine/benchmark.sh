#!/bin/bash

set -x

source ../benchmarks/bin/activate

size=30
runs=${1:-1}
tasks=( numba naive composer )
threads=( 1 2 4 8 16 )

for task in "${tasks[@]}"; do 
  rm -f $task.stdout $task.stderr
  git log | head -1 > $task.stderr
  git log | head -1 > $task.stdout
done

for i in {1..$runs}; do
  for nthreads in "${threads[@]}"; do 
   NUMBA_NUM_THREADS=$nthreads python haversine_numba.py -s $size >> numba.stdout 2>> numba.stderr
  done
done

for i in {1..$runs}; do
  for nthreads in "${threads[@]}"; do 
   python haversine.py -m composer -s $size -t $nthreads >> composer.stdout 2>> composer.stderr
  done
done

for i in {1..$runs}; do
  for nthreads in "${threads[@]}"; do 
   python haversine.py -m naive -s $size >> naive.stdout 2>> naive.stderr
  done
done
