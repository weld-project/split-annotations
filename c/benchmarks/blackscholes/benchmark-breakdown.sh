#!/bin/bash

set -x

tasks=( mklcomposer )
threads=( 16 )

for task in "${tasks[@]}"; do 
  rm -f $task.stdout $task.stderr
  git log | head -1 > $task.stderr
  git log | head -1 > $task.stdout
done

# For composer...
export RUST_LOG=info
export OMP_NUM_THREADS=1

for i in {1..5}; do
  for task in "${tasks[@]}"; do 
    for nthreads in "${threads[@]}"; do 
     ./bench -m $task -s 30 -t $nthreads >> $task.stdout 2>> $task.stderr
    done
  done
done
