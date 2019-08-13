#!/bin/bash

# Runs all the C-based benchmarks. Specifically, this runs:
# - Black Scholes with MKL
# - Haversine with MKL
# - nBody with MKL
# - Shallow Water with MKL
# - Nashville with ImageMagick
# - Gotham with ImageMagick

set -x

# Get the data for Nashville and Gotham
./get-data.sh

rm -rf results/
mkdir results/

tasks=( blackscholes haversine nbody shallow_water gotham nashville )

# Write system information.
git log | head -1 > results/CONFIG.txt
uname -a >> results/CONFIG.txt
lsb_release -d >> results/CONFIG.txt

for task in "${tasks[@]}"; do 
  echo "Executing $task"  
  pushd $task
  make
  ./benchmark.sh
  popd
  mkdir results/$task
  mv $task/*.std* results/$task
done
