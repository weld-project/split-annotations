#!/bin/bash

set -x

# Name of the environment
rm -rf benchmarks
./setup-env.sh

# Get the data
./get-data.sh

rm -rf results/
mkdir results/

tasks=( blackscholes birth_analysis crime_index data_cleaning haversine movielens nbody shallow_water speechtag )

# Write system information.
git log | head -1 > results/CONFIG.txt
uname -a >> results/CONFIG.txt
lsb_release -d >> results/CONFIG.txt

for task in "${tasks[@]}"; do 
  echo "Executing $task"  
  pushd $task
  ./benchmark.sh
  popd
  mkdir results/$task
  mv $task/*.std* results/$task
done
