
import argparse
import sys

sys.path.append("../../lib/")
sys.path.append("../../pycomposer/")

import composer_pandas as pd
import time

import warnings
warnings.filterwarnings('ignore')

def run(threads):
    # Make display smaller
    pd.options.display.max_rows = 10

    sys.stdout.write("Reading data...")
    sys.stdout.flush()
    unames = ['user_id', 'gender', 'age', 'occupation', 'zip']
    users = pd.read_table('../datasets/movielens/_data/ml-1m/users-large.dat', sep='::', header=None,
                          names=unames)

    rnames = ['user_id', 'movie_id', 'rating', 'timestamp']
    ratings = pd.read_table('../datasets/movielens/_data/ml-1m/ratings-large.dat', sep='::', header=None,
                            names=rnames)

    mnames = ['movie_id', 'title', 'genres']
    movies = pd.read_table('../datasets/movielens/_data/ml-1m/movies-large.dat', sep='::', header=None,
                           names=mnames)
    print("Done")

    e2e_start = time.time()

    start = time.time()
    tmp = pd.merge(ratings, users)
    tmp.dontsend = True
    data = pd.merge(tmp, movies)
    data.dontsend = True
    data = pd.filter(data, 'age', 45)
    pd.evaluate(workers=threads)
    data = data.value
    end = time.time()
    print("Merge 2:", end - start)
    start = end

    mean_ratings = data.pivot_table('rating', index='title', columns='gender',
                                    aggfunc='mean')
    end = time.time()
    print("Pivot:", end - start)
    start = end

    """
    ratings_by_title = pd.dfgroupby(data, 'title')
    ratings_by_title = pd.gbsize(ratings_by_title)
    pd.evaluate(workers=threads)
    ratings_by_title = ratings_by_title.value
    """
    ratings_by_title = data.groupby('title').size()
    end = time.time()

    print("GroupBy size:", end - start)
    start = end

    active_titles = ratings_by_title.index[ratings_by_title >= 250]
    mean_ratings = mean_ratings.loc[active_titles]
    mean_ratings['diff'] = mean_ratings['M'] - mean_ratings['F']
    sorted_by_diff = mean_ratings.sort_values(by='diff')
    end = time.time()
    print("Diff:", end - start)
    start = end

    rating_std_by_title = data.groupby('title')['rating'].std()
    end = time.time()
    print("GroupBy std:", end - start)
    start = end

    rating_std_by_title = rating_std_by_title.loc[active_titles]
    rating_std_by_title = rating_std_by_title.sort_values(ascending=False)[:10]
    end = time.time()
    print("Sort:", end - start)
    start = end

    e2e_end = time.time()

    print(sorted_by_diff.head())
    print(rating_std_by_title.head())

    print("Total:", e2e_end - e2e_start)

def main():
    parser = argparse.ArgumentParser(
        description="MovieLens with Composer."
    )
    parser.add_argument('-t', "--threads", type=int, default=16, help="Number of threads.")
    args = parser.parse_args()

    threads = args.threads

    print("Threads:", threads)
    mi = run(threads)


if __name__ == "__main__":
    main()
