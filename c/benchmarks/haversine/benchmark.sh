#!/bin/bash

set -x

tasks=( mkl mklcomposer )
threads=( 1 2 4 8 16 )
runs=${1:-1}
size=30

for task in "${tasks[@]}"; do 
  rm -f $task.stdout $task.stderr
  git log | head -1 > $task.stderr
  git log | head -1 > $task.stdout
done

for i in {1..$runs}; do
  for nthreads in "${threads[@]}"; do 
   export OMP_NUM_THREADS=$nthreads
   ./bench -m mkl -s $size -t $nthreads >> mkl.stdout 2>> mkl.stderr
  done
done

# Set the number of threads in the environment variable to 1, to prevent
# the MKL functions from launching N threads per task.
export OMP_NUM_THREADS=1
for i in {1..$runs}; do
  for nthreads in "${threads[@]}"; do 
   ./bench -m mklcomposer -s $size -t $nthreads >> mklcomposer.stdout 2>> mklcomposer.stderr
  done
done
