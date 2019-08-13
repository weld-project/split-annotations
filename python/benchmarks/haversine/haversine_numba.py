
import argparse
import math
import sys
import time

import numpy as np
from numba import njit

def get_data(size):
    lats = np.ones(size, dtype="float64") * 0.0698132
    lons = np.ones(size, dtype="float64") * 0.0698132
    return lats, lons


@njit(parallel=True)
def haversine(lat2, lon2):
    lat1 = 0.70984286
    lon1 = 1.23892197
    miles_constant = 3959.0
    dlat = lat2 - lat1 
    dlon = lon2 - lon1 
    a = np.sin(dlat/2.0)**2 + np.cos(lat1) * np.cos(lat2) * np.sin(dlon/2.0)**2
    c = 2.0 * np.arcsin(np.sqrt(a)) 
    mi = miles_constant * c
    return mi

def run():
    parser = argparse.ArgumentParser(
        description="Haversine distance computation."
    )
    parser.add_argument('-s', "--size", type=int, default=26, help="Size of each array")
    parser.add_argument('-p', "--piece_size", type=int, default=16384, help="Size of each piece.")
    parser.add_argument('-t', "--threads", type=int, default=1, help="Number of threads.")
    parser.add_argument('-v', "--verbosity", type=str, default="none", help="Log level (debug|info|warning|error|critical|none)")
    args = parser.parse_args()

    size = (1 << args.size)
    piece_size = args.piece_size
    threads = args.threads
    loglevel = args.verbosity

    print("Size:", size)
    print("Piece Size:", piece_size)
    print("Threads:", threads)
    print("Log Level", loglevel)

    sys.stdout.write("Generating data...")
    sys.stdout.flush()
    lats, lons = get_data(size)
    print("done.")


    start = time.time()
    mi = haversine(lats, lons)
    end = time.time()
    print("Runtime:", end - start)
    print(mi)

if __name__ == "__main__":
    run()

