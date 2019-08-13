#!/bin/bash

set -x

source ../benchmarks/bin/activate

size=30

tasks=( composerbatch )
batchsizes=( 512 2048 4096 8192 16384 32768 8388608 16777216 33554432 )

for task in "${tasks[@]}"; do 
  rm -f $task.stdout $task.stderr
  git log | head -1 > $task.stderr
  git log | head -1 > $task.stdout
done

for i in {1..5}; do
  for batchsize in "${batchsizes[@]}"; do 
     taskset -a -c 0-9,20-29 python blackscholes.py -m composer -s $size -t 16 -p $batchsize >> $task.stdout 2>> $task.stderr
  done
done
