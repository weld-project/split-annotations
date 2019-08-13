#!/bin/bash

set -x

tasks=( mkl mklcomposer )
threads=( 1 2 4 8 16 )
runs=${1:-1}

for task in "${tasks[@]}"; do 
  rm -f $task.stderr $task.stdout
  git log | head -1 > $task.stderr
  git log | head -1 > $task.stdout
done

for i in {1..$runs}; do
  for task in "${tasks[@]}"; do 
    for nthreads in "${threads[@]}"; do 
      ./bench -m $task -s 32768 -t $nthreads -i 3 -p 8192 >> $task.stdout 2>> $task.stderr
    done
  done
done
