import pandas as pd
import sys
import time

import warnings
warnings.filterwarnings('ignore')

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
data = pd.merge(ratings, users)
end = time.time()
print("Merge 1:", end - start)
start = end
data = pd.merge(data, movies)
end = time.time()
print("Merge 2:", end - start)
start = end
print(len(data))
data = data[data['age'] > 45]
print(len(data))
end = time.time()
print("Filter:", end - start)
start = end

mean_ratings = data.pivot_table('rating', index='title', columns='gender',
                                aggfunc='mean')
end = time.time()
print("Pivot:", end - start)
start = end

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
