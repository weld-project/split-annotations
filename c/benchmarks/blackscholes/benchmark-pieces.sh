#!/bin/bash

set -x

tasks=( mklcomposer )
pieces=( 256 512 1024 2048 4096 8192 16384 )

for task in "${tasks[@]}"; do
  rm -f $task.stdout $task.stderr
  git log | head -1 > $task.stderr
  git log | head -1 > $task.stdout
done

for i in {1..5}; do
  for task in "${tasks[@]}"; do
    for npieces in "${pieces[@]}"; do
      ./bench -m $task -s 30 -p $npieces -t 16 >> $task.stdout 2>> $task.stderr
    done
  done
done
