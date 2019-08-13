#!/usr/bin/python

# The usual preamble
import numpy as np
import time
import argparse

import sys

sys.path.append("../../lib")
sys.path.append("../../pycomposer/")

import composer_pandas as pd

def gen_data(size):
    values = ["1234567" for  _ in range(size)]
    return pd.Series(data=values)

def datacleaning_pandas(requests):
    requests = requests.str.slice(0, 5)
    zero_zips = requests == "00000"
    requests = requests.mask(zero_zips, np.nan)
    requests = requests.unique()
    return requests

def datacleaning_composer(requests, threads):
    # Fix requests with extra digits
    requests = pd.series_str_slice(requests, 0, 5)
    requests.dontsend = True

    # Fix requests with 00000 zipcodes
    zero_zips = pd.equal(requests, "00000")
    zero_zips.dontsend = True
    requests = pd.mask(requests, zero_zips, np.nan)
    requests.dontsend = True
    requests = pd.unique(requests)
    pd.evaluate(workers=threads)
    requests = requests.value
    return requests

def run():
    parser = argparse.ArgumentParser(
        description="Data Cleaning"
    )
    parser.add_argument('-s', "--size", type=int, default=26, help="Size of each array")
    parser.add_argument('-p', "--piece_size", type=int, default=16384*2, help="Size of each piece.")
    parser.add_argument('-t', "--threads", type=int, default=1, help="Number of threads.")
    parser.add_argument('-v', "--verbosity", type=str, default="none", help="Log level (debug|info|warning|error|critical|none)")
    parser.add_argument('-m', "--mode", type=str, required=True, help="Mode (composer|naive)")
    args = parser.parse_args()

    size = (1 << args.size)
    piece_size = args.piece_size
    threads = args.threads
    loglevel = args.verbosity
    mode = args.mode.strip().lower()

    assert mode == "composer" or mode == "naive"
    assert threads >= 1

    print("Size:", size)
    print("Piece Size:", piece_size)
    print("Threads:", threads)
    print("Log Level", loglevel)
    print("Mode:", mode)

    sys.stdout.write("Generating data...")
    sys.stdout.flush()
    inputs = gen_data(size)
    print("done.")

    start = time.time()
    if mode == "composer":
        result = datacleaning_composer(inputs, threads)
    elif mode == "naive":
        result = datacleaning_pandas(inputs)
    end = time.time()
    print(end - start, "seconds")
    print(result)

if __name__ == "__main__":
    run()

