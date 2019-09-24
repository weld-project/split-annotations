"""
Annotations for Pandas Series.
"""

import pandas as pd

from copy import deepcopy as dc
from sa.annotation import *
from sa.annotation.split_types import *

class SeriesSplit(SplitType):
    def combine(self, values):
        return pd.concat(values)

    def split(self, start, end, value):
        return value[start:end]

pd.Series.abs =  sa((SeriesSplit(),), {}, SeriesSplit())(pd.Series.abs)

