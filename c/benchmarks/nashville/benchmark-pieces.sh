#!/bin/bash

set -x

tasks=( composer )
pieces=( 32 256 512 1024 2048 4096 8192 16384 )

for task in "${tasks[@]}"; do
  rm -f $task.stdout $task.stderr
  git log | head -1 > $task.stderr
  git log | head -1 > $task.stdout
done

for i in {1..5}; do
  for task in "${tasks[@]}"; do
    for npieces in "${pieces[@]}"; do
      RUST_LOG=info ./bench -m $task -i ~/heic1502a-40k.tif -p $npieces -t 8 >> $task.stdout 2>> $task.stderr
    done
  done
done
