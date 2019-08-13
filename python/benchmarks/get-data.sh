#!/bin/bash

set -x

cd datasets
TOP=`pwd`

# MovieLens dataset for the MovieLens workload.
cd movielens
wget http://files.grouplens.org/datasets/movielens/ml-1m.zip
unzip ml-1m.zip
rm -rf _data
mkdir -p _data
mv ml-1m _data
rm ml-1m.zip
./replicate-csv -i _data/ml-1m/movies.dat -o  _data/ml-1m/movies-large.dat -r 7
./replicate-csv -i _data/ml-1m/ratings.dat -o _data/ml-1m/ratings-large.dat -r 7
./replicate-csv -i _data/ml-1m/users.dat -o   _data/ml-1m/users-large.dat -r 7

# Birth Analysis
cd $TOP
cd birth_analysis
gunzip -k babynames.txt.gz
mkdir -p _data
mv babynames.txt _data
./replicate-csv -i _data/babynames.txt -o  _data/babynames-xlarge.txt -r 80
