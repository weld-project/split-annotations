
import sys

import argparse
import scipy.special as ss
import time

import numpy as np
from numba import njit, jit
from numba import vectorize, float64

def get_data(size):
    price = np.ones(size, dtype="float64") * 4.0
    strike = np.ones(size, dtype="float64") * 4.0
    t = np.ones(size, dtype="float64") * 4.0
    rate = np.ones(size, dtype="float64") * 4.0
    vol = np.ones(size, dtype="float64") * 4.0
    
    return price, strike, t, rate, vol

@njit(parallel=True)
def bs(price, strike, t, rate, vol):
    """
    This is the cookie-cutter implementation.
    """
    c05 = 3.0
    c10 = 1.5
    invsqrt2 = 1.0 / np.sqrt(2.0)

    c05 = np.float64(3.0)
    c10 = np.float64(1.5)

    rsig = rate + (vol**2) * c05
    vol_sqrt = vol * np.sqrt(t)

    d1 = (np.log(price / strike) + rsig * t) / vol_sqrt
    d2 = d1 - vol_sqrt

    d1 = c05 + c05 * np.exp(d1 * invsqrt2)
    d2 = c05 + c05 * np.exp(d2 * invsqrt2)

    e_rt = np.exp((-rate) * t)

    call = price * d1 - e_rt * strike * d2
    put = e_rt * strike * (c10 - d2) - price * (c10 - d1)
    return call, put


def run():
    parser = argparse.ArgumentParser(
        description="Chained Adds pipelining test on a single thread."
    )
    parser.add_argument('-s', "--size", type=int, default=27, help="Size of each array")
    parser.add_argument('-p', "--piece_size", type=int, default=16384, help="Size of each piece.")
    parser.add_argument('-t', "--threads", type=int, default=1, help="Number of threads.")
    parser.add_argument('-v', "--verbosity", type=str, default="none", help="Log level (debug|info|warning|error|critical|none)")
    args = parser.parse_args()

    size = (1 << args.size)
    piece_size = args.piece_size
    threads = args.threads
    loglevel = args.verbosity

    assert threads >= 1

    print("Size:", size)
    print("Piece Size:", piece_size)
    print("Threads:", threads)
    print("Log Level", loglevel)

    sys.stdout.write("Generating data...")
    sys.stdout.flush()
    a, b, c, d, e = get_data(size)
    print("done")


    start = time.time()
    call, put = bs(a, b, c, d, e)
    end = time.time()
    print(end-start)

if __name__ == "__main__":
    run()

