#!/bin/bash

set -x

tasks=( composer )
threads=( 16 )

for task in "${tasks[@]}"; do 
  rm -f $task.stderr $task.stdout
  git log | head -1 > $task.stderr
  git log | head -1 > $task.stdout
done

export RUST_LOG=info

for i in {1..5}; do
  for task in "${tasks[@]}"; do 
    for nthreads in "${threads[@]}"; do 
      taskset 0xffff ./bench -m $task -p 4096 -i ~/heic1502a-40k.tif -t $nthreads >> $task.stdout 2>> $task.stderr
    done
  done
done
