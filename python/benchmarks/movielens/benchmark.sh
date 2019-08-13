#!/bin/bash

set -x

source ../benchmarks/bin/activate

runs=${1:-1}
tasks=( naive composer )
threads=( 1 2 4 8 16 )

for task in "${tasks[@]}"; do 
  rm -f $task.stdout $task.stderr
  git log | head -1 > $task.stderr
  git log | head -1 > $task.stdout
done

for i in {1..$runs}; do
  for nthreads in "${threads[@]}"; do 
    python movielens_composer.py -t $nthreads >> composer.stdout 2>> composer.stderr
  done
done

for i in {1..$runs}; do
  python movielens.py >> naive.stdout 2>> naive.stderr
done
