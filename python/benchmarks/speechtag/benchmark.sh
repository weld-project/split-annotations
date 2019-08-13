#!/bin/bash

set -x

source ../benchmarks/bin/activate

tasks=( composer )
threads=( 1 2 4 8 16 )
runs=${1:-1}

for task in "${tasks[@]}"; do 
  rm -f $task.stdout $task.stderr
  git log | head -1 > $task.stderr
  git log | head -1 > $task.stdout
done

for i in {1..$runs}; do
  for task in "${tasks[@]}"; do 
    for nthreads in "${threads[@]}"; do 
      python speechtag_composer.py -n $nthreads >> $task.stdout 2>> $task.stderr
    done
  done
done

tasks=( spacy )
threads=( 1 )

for task in "${tasks[@]}"; do 
  rm -f $task.stdout $task.stderr
  git log | head -1 > $task.stderr
  git log | head -1 > $task.stdout
done

for i in {1..$runs}; do
  for task in "${tasks[@]}"; do 
    for nthreads in "${threads[@]}"; do 
      python speechtag.py >> $task.stdout 2>> $task.stderr
    done
  done
done
