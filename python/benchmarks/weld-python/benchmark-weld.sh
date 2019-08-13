#!/bin/bash

set -x

size=14

tasks=( weld )
threads=( 1 2 4 8 16 32 )

for task in "${tasks[@]}"; do 
  rm -f $task.stdout $task.stderr
  git log | head -1 > $task.stderr
  git log | head -1 > $task.stdout
done

# Weld doesn't seem to free memory properly in this setup, so just run it ten times and add up...
for i in {1..5}; do
  for task in "${tasks[@]}"; do 
    for nthreads in "${threads[@]}"; do 
      python shallow_water_weld.py -s $size -i 1 -t $nthreads >> $task.stdout 2>> $task.stderr
    done
  done
done
