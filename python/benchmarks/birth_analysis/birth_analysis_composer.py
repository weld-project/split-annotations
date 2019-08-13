
import sys

sys.path.append("../../lib/")
sys.path.append("../../pycomposer/")

import argparse
import composer_pandas as pd
import time

def analyze(top1000):
    start1 = time.time()
    all_names = pd.Series(top1000.name.unique())
    lesley_like = all_names[all_names.str.lower().str.contains('lesl')]
    filtered = top1000[top1000.name.isin(lesley_like)]
    table = filtered.pivot_table('births', index='year',
                                 columns='sex', aggfunc='sum')

    table = table.div(table.sum(1), axis=0)
    end1 = time.time()
    print("Analysis:", end1 - start1)
    return table

def get_top1000(group):
    return group.sort_values(by='births', ascending=False)[0:1000]

def run(filename, threads):
    years = range(1880, 2011)
    columns = ['year', 'sex', 'name', 'births']

    sys.stdout.write("Reading data...")
    sys.stdout.flush()
    names = pd.read_csv(filename, names=columns)
    print("done")

    print("Size of names:", len(names))

    e2e_start = time.time()

    start0 = time.time()
    grouped = pd.dfgroupby(names, ['year', 'sex'])
    top1000 = pd.gbapply(grouped, get_top1000)
    pd.evaluate(workers=threads)
    top1000 = top1000.value
    top1000.reset_index(inplace=True, drop=True)
    print(len(top1000))

    """
    grouped: Dag Operation
    GBApply Takes a DAG operation and stores it in its type. The operation must be a GroupBy
    GBApply has type ApplySplit. It's combiner:
        1. Combines the results of the dataFrame
        2. Resets index
        3. Gets the keys from the DAG operation
        4. Calls groupBy again
        5. Calls apply again.
    """

    localreduce_start = time.time()
    top1000 = top1000.groupby(['year', 'sex']).apply(get_top1000)
    localreduce_end = time.time()
    print("Local reduction:", localreduce_end - localreduce_start)
    top1000.reset_index(inplace=True, drop=True)
    end0 = time.time()

    print("Apply:", end0-start0)
    print("Elements in top1000:", len(top1000))

    result = analyze(top1000)

    e2e_end = time.time()
    print("Total time:", e2e_end - e2e_start)

    print(top1000['births'].sum())

def main():
    parser = argparse.ArgumentParser(
        description="Birth Analysis with Composer."
    )
    parser.add_argument('-f', "--filename", type=str, default="babynames.txt", help="Input file")
    parser.add_argument('-t', "--threads", type=int, default=1, help="Number of threads.")
    args = parser.parse_args()

    filename = args.filename
    threads = args.threads

    print("File:", filename)
    print("Threads:", threads)
    mi = run(filename, threads)


if __name__ == "__main__":
    main()
