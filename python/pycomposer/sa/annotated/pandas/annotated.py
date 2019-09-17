"""
Annotations for Pandas functions.

Note: For convinience, we just write a wrapper function that calls the Pandas function, and then
use those functions instead. We could equivalently just replace methods on the DataFrame class too and
split `self` instead of the DataFrame passed in here.
"""

import numpy as np
import pandas as pd
import time

from copy import deepcopy as dc
from sa.annotation import *
from sa.annotation.split_types import *

class UniqueSplit(SplitType):
    """ For the result of Unique """
    def combine(self, values):
        if len(values) > 0:
            return np.unique(np.concatenate(values))
        else:
            return np.array([])

    def split(self, values):
        raise ValueError

class DataFrameSplit(SplitType):
    def combine(self, values):
        do_combine = False
        for val in values:
            if val is not None:
                do_combine = True

        if do_combine and len(values) > 0:
            return pd.concat(values)

    def split(self, start, end, value):
        if not isinstance(value, pd.DataFrame) and not isinstance(value, pd.Series):
            # Assume this is a constant (str, int, etc.).
            return value
        return value[start:end]

    def elements(self, value):
        if not isinstance(value, pd.DataFrame) and not isinstance(value, pd.Series):
            return None
        return len(value)

class SumSplit(SplitType):
    def combine(self, values):
        return sum(values)

    def split(self, start, end, value):
        raise ValueError("can't split sum values")

class GroupBySplit(SplitType):
    def combine(self, values):
        return None

    def split(self, start, end, value):
        raise ValueError("can't split groupby values")

class SizeSplit(SplitType):
    def combine(self, values):
        return pd.concat(values)

    def split(self, start, end, value):
        raise ValueError("can't split size values")

def dfgroupby(df, keys):
    return df.groupby(keys)

def merge(left, right):
    return pd.merge(left, right)

def gbapply(grouped, func):
    return grouped.apply(func)

def gbsize(grouped):
    return grouped.size()

def filter(df, column, target):
    return df[df[column] > target]

@sa((DataFrameSplit(), DataFrameSplit()), {}, DataFrameSplit())
def divide(series, value):
    result = (series / value)
    return result

@sa((DataFrameSplit(), DataFrameSplit()), {}, DataFrameSplit())
def multiply(series, value):
    result = (series * value)
    return result

@sa((DataFrameSplit(), DataFrameSplit()), {}, DataFrameSplit())
def subtract(series, value):
    result = (series - value)
    return result

@sa((DataFrameSplit(), DataFrameSplit()), {}, DataFrameSplit())
def add(series, value):
    result = (series + value)
    return result

@sa((DataFrameSplit(), DataFrameSplit()), {}, DataFrameSplit())
def equal(series, value):
    result = (series == value)
    return result

@sa((DataFrameSplit(), DataFrameSplit()), {}, DataFrameSplit())
def greater_than(series, value):
    result = (series >= value)
    return result

@sa((DataFrameSplit(), DataFrameSplit()), {}, DataFrameSplit())
def less_than(series, value):
    result = (series < value)
    return result

@sa((DataFrameSplit(),), {}, SumSplit())
def pandasum(series):
    result = series.sum()
    return result

@sa((DataFrameSplit(),), {}, UniqueSplit())
def unique(series):
    result = series.unique()
    return result

@sa((DataFrameSplit(),), {}, DataFrameSplit())
def series_str(series):
    result = series.str
    return result

@sa((DataFrameSplit(), DataFrameSplit(), Broadcast()), {}, DataFrameSplit())
def mask(series, cond, val):
    result = series.mask(cond, val)
    return result

@sa((DataFrameSplit(), Broadcast(), Broadcast()), {}, DataFrameSplit())
def series_str_slice(series, start, end):
    result = series.str.slice(start, end)
    return result

@sa((DataFrameSplit(),), {}, DataFrameSplit())
def pandanot(series):
    return ~series

@sa((DataFrameSplit(), Broadcast()), {}, DataFrameSplit())
def series_str_contains(series, target):
    result = series.str.contains(target)
    return result

dfgroupby = sa((DataFrameSplit(), Broadcast()), {}, GroupBySplit())(dfgroupby)
merge = sa((DataFrameSplit(), Broadcast()), {}, DataFrameSplit())(merge)
filter = sa((DataFrameSplit(), Broadcast(), Broadcast()), {}, DataFrameSplit())(filter)

# Return split type should be ApplySplit(subclass of DataFrameSplit), and it
# should take the first argument as a parameter. The parameter is guaranteed to
# be a dag.Operation.  The combiner can then use the `by` arguments to groupby
# in the combiner again, and then apply again.
gbapply = sa((GroupBySplit(), Broadcast()), {}, DataFrameSplit())(gbapply)
gbsize = sa((GroupBySplit(), Broadcast()), {}, SizeSplit())(gbsize)
