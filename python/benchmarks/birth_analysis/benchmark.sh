#!/bin/bash

set -x

source ../benchmarks/bin/activate

# File to use. babynames.txt is for testing. babynames-xlarge.txt is for benchmark.
filename="../datasets/birth_analysis/_data/babynames-xlarge.txt"
runs=${1:-1}

tasks=( composer naive )
threads=( 1 2 4 8 16 )

for task in "${tasks[@]}"; do 
  rm -f $task.stdout $task.stderr
  git log | head -1 > $task.stderr
  git log | head -1 > $task.stdout
done

for i in {1..$runs}; do
  python birth_analysis.py -f $filename >> naive.stdout 2>> naive.stderr
  for nthreads in "${threads[@]}"; do 
    python birth_analysis_composer.py -f $filename -t $nthreads >> composer.stdout 2>> composer.stderr
  done
done
