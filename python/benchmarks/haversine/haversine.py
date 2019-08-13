
import sys
sys.path.append("../../lib/")
sys.path.append("../../pycomposer/")

import argparse
import math
import time

def get_data(size, composer):
    if composer:
        import composer_numpy as np
    else:
        import numpy as np

    lats = np.ones(size, dtype="float64") * 0.0698132
    lons = np.ones(size, dtype="float64") * 0.0698132
    return lats, lons

def haversine(lat2, lon2, composer, threads):
    if composer:
        import composer_numpy as np
    else:
        import numpy as np

    lat1 = 0.70984286
    lon1 = 1.23892197
    MILES_CONST = 3959.0

    start = time.time()
    a = np.zeros(len(lat2), dtype="float64")
    dlat = np.zeros(len(lat2), dtype="float64")
    dlon = np.zeros(len(lat2), dtype="float64")
    end = time.time()
    print("Allocation time:", end-start)

    start = time.time()
    np.subtract(lat2, lat1, out=dlat)
    np.subtract(lon2, lon1, out=dlon)

    # dlat = sin(dlat / 2.0) ** 2.0
    np.divide(dlat, 2.0, out=dlat)
    np.sin(dlat, out=dlat)
    np.multiply(dlat, dlat, out=dlat)

    # a = cos(lat1) * cos(lat2)
    lat1_cos = math.cos(lat1)
    np.cos(lat2, out=a)
    np.multiply(a, lat1_cos, out=a)

    # a = a + sin(dlon / 2.0) ** 2.0
    np.divide(dlon, 2.0, out=dlon)
    np.sin(dlon, out=dlon)
    np.multiply(dlon, dlon, out=dlon)
    np.multiply(a, dlon, out=a)
    np.add(dlat, a, out=a)

    c = a
    np.sqrt(a, out=a)
    np.arcsin(a, out=a)
    np.multiply(a, 2.0, out=c)

    mi = c
    np.multiply(c, MILES_CONST, out=mi)

    if composer:
        np.evaluate(workers=threads)

    end = time.time()
    print("Runtime:", end-start)

    return mi

def run():
    parser = argparse.ArgumentParser(
        description="Haversine distance computation."
    )
    parser.add_argument('-s', "--size", type=int, default=26, help="Size of each array")
    parser.add_argument('-p', "--piece_size", type=int, default=16384, help="Size of each piece.")
    parser.add_argument('-t', "--threads", type=int, default=1, help="Number of threads.")
    parser.add_argument('-v', "--verbosity", type=str, default="none", help="Log level (debug|info|warning|error|critical|none)")
    parser.add_argument('-m', "--mode", type=str, required=True, help="Mode (composer|naive)")
    args = parser.parse_args()

    size = (1 << args.size)
    piece_size = args.piece_size
    threads = args.threads
    loglevel = args.verbosity
    mode = args.mode.strip().lower()

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
        raise ValueError("unknown mode", mode)

    sys.stdout.write("Generating data...")
    sys.stdout.flush()
    lats, lons = get_data(size, composer)
    print("done.")


    mi = haversine(lats, lons, composer, threads)
    print(mi)

if __name__ == "__main__":
    run()

