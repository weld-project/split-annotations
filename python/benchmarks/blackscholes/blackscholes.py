
import sys
sys.path.append("../../lib/")
sys.path.append("../../pycomposer/")

import argparse
import math
import scipy.special as ss
import time

def get_data(size, composer):
    if composer:
        import composer_numpy as np
    else:
        import numpy as np

    price = np.ones(size, dtype="float64") * 4.0
    strike = np.ones(size, dtype="float64") * 4.0
    t = np.ones(size, dtype="float64") * 4.0
    rate = np.ones(size, dtype="float64") * 4.0
    vol = np.ones(size, dtype="float64") * 4.0
    
    return price, strike, t, rate, vol

def bs(price, strike, t, rate, vol, composer, threads, piece_size):

    if composer:
        import composer_numpy as np
    else:
        import numpy as np

    c05 = 3.0
    c10 = 1.5
    invsqrt2 = 1.0 / math.sqrt(2.0)

    start = time.time()

    tmp = np.ones(len(price), dtype="float64")
    vol_sqrt = np.ones(len(price), dtype="float64")
    rsig = np.ones(len(price), dtype="float64")
    d1 = np.ones(len(price), dtype="float64")
    d2 = np.ones(len(price), dtype="float64")

    # Outputs
    call = np.ones(len(price), dtype="float64")
    put = np.ones(len(price), dtype="float64")
    end = time.time()
    print("Allocation:", end - start)

    start = time.time()

    np.multiply(vol, vol, out=rsig)
    np.multiply(rsig, c05, out=rsig)
    np.add(rsig, rate, out=rsig)

    np.sqrt(t, out=vol_sqrt)
    np.multiply(vol_sqrt, vol, out=vol_sqrt)

    np.multiply(rsig, t, out=tmp)
    np.divide(price, strike, out=d1)
    np.log2(d1, out=d1)
    np.add(d1, tmp, out=d1)

    np.divide(d1, vol_sqrt, out=d1)
    np.subtract(d1, vol_sqrt, out=d2)

    # d1 = c05 + c05 * erf(d1 * invsqrt2)
    np.multiply(d1, invsqrt2, out=d1)

    if composer:
        np.erf(d1, out=d1)
    else:
        ss.erf(d1, out=d1)

    np.multiply(d1, c05, out=d1)
    np.add(d1, c05, out=d1)

    # d2 = c05 + c05 * erf(d2 * invsqrt2)
    np.multiply(d2, invsqrt2, out=d2)

    if composer:
        np.erf(d2, out=d2)
    else:
        ss.erf(d2, out=d2)

    np.multiply(d2, c05, out=d2)
    np.add(d2, c05, out=d2)

    # Reuse existing buffers
    e_rt = vol_sqrt
    tmp2 = rsig

    # e_rt = exp(-rate * t)
    np.multiply(rate, -1.0, out=e_rt)
    np.multiply(e_rt, t, out=e_rt)
    np.exp(e_rt, out=e_rt)

    # call = price * d1 - e_rt * strike * d2
    #
    # tmp = price * d1
    # tmp2 = e_rt * strike * d2
    # call = tmp - tmp2
    np.multiply(price, d1, out=tmp)
    np.multiply(e_rt, strike, out=tmp2)
    np.multiply(tmp2, d2, out=tmp2)
    np.subtract(tmp, tmp2, out=call)

    # put = e_rt * strike * (c10 - d2) - price * (c10 - d1)
    # tmp = e_rt * strike
    # tmp2 = (c10 - d2)
    # put = tmp - tmp2
    # tmp = c10 - d1
    # tmp = price * tmp
    # put = put - tmp
    np.multiply(e_rt, strike, out=tmp)
    np.subtract(c10, d2, out=tmp2)
    np.multiply(tmp, tmp2, out=put)
    np.subtract(c10, d1, out=tmp)
    np.multiply(price, tmp, out=tmp)
    np.subtract(put, tmp, out=put)

    end = time.time()
    print("Build time:", end - start)

    if composer:
        np.evaluate(workers=threads, batch_size=piece_size)

    end = time.time()
    print("Runtime:", end - start)

    return call, put

def run():
    parser = argparse.ArgumentParser(
        description="Chained Adds pipelining test on a single thread."
    )
    parser.add_argument('-s', "--size", type=int, default=27, help="Size of each array")
    parser.add_argument('-p', "--piece_size", type=int, default=16384, help="Size of each piece.")
    parser.add_argument('-t', "--threads", type=int, default=1, help="Number of threads.")
    parser.add_argument('-v', "--verbosity", type=str, default="none", help="Log level (debug|info|warning|error|critical|none)")
    parser.add_argument('-m', "--mode", type=str, required=False, help="Mode (composer|naive)")
    args = parser.parse_args()

    size = (1 << args.size)
    piece_size = args.piece_size
    threads = args.threads
    loglevel = args.verbosity
    mode = args.mode.strip().lower()

    assert threads >= 1

    print("Size:", size)
    print("Piece Size:", piece_size)
    print("Threads:", threads)
    print("Log Level", loglevel)
    print("Mode:", mode)

    if mode == "composer":
        composer = True
    elif mode == "naive":
        composer = False
    else:
        raise ValueError("invalid mode", mode)

    sys.stdout.write("Generating data...")
    sys.stdout.flush()
    a, b, c, d, e = get_data(size, composer)
    print("done.")

    call, put = bs(a, b, c, d, e, composer, threads, piece_size)
    print("Call:", call)
    print("Put:", put)

if __name__ == "__main__":
    run()
