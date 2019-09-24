""" 
Test
"""

# Fall back to Pandas if we don't support something.
from pandas import *

from .annotated import *
from sa.annotation import evaluate

class SeriesSplit(SplitType):
    def combine(self, values):
        return concat(values)

    def split(self, start, end, value):
        return value[start:end]

Series.abs =  sa((SeriesSplit(),), {}, SeriesSplit())(Series.abs)

Series.add = sa((SeriesSplit(), SeriesSplit()), {}, SeriesSplit())(Series.add)
Series.all = sa((SeriesSplit(),), {}, AllSplit())(Series.all)
Series.any = sa((SeriesSplit(),), {}, AnySplit())(Series.any)
Series.apply = sa((SeriesSplit(), Broadcast()), {}, SeriesSplit())(Series.apply)

Series.between = sa((SeriesSplit(), Broadcast(), Broadcast()), {}, SeriesSplit())(Series.between)
Series.between_time = sa((SeriesSplit(), Broadcast(), Broadcast()), {}, SeriesSplit())(Series.between_time)

Series.bfill = sa((SeriesSplit(), ), {}, SeriesSplit())(Series.bfill)
Series.clip = sa((SeriesSplit(),), {}, SeriesSplit())(Series.clip)
Series.combine = sa((SeriesSplit(), SeriesSplit()), {}, SeriesSplit())(Series.combine)
Series.combine_first = sa((SeriesSplit(), SeriesSplit()), {}, SeriesSplit())(Series.combine_first)

Series.div = sa((SeriesSplit(), SeriesSplit()), {}, SeriesSplit())(Series.div)
Series.divide = sa((SeriesSplit(), SeriesSplit()), {}, SeriesSplit())(Series.divide)
Series.divmod = sa((SeriesSplit(), SeriesSplit()), {}, SeriesSplit())(Series.divmod)

Series.dot = sa((SeriesSplit(), SeriesSplit()), {}, DotSplit())(Series.dot)

Series.drop_duplicates = sa((SeriesSplit(),), {}, DotSplit())(Series.drop_duplicates)
Series.dropna = sa((SeriesSplit(),), {}, DotSplit())(Series.dropna)

Series.eq = sa((SeriesSplit(), SeriesSplit()), {}, EqSplit())(Series.eq)
Series.equals = sa((SeriesSplit(), SeriesSplit()), {}, EqualsSplit())(Series.equals)

Series.ffill = sa((SeriesSplit(), ), {}, SeriesSplit())(Series.ffill)
Series.floordiv = sa((SeriesSplit(), SeriesSplit()), {}, SeriesSplit())(Series.floordiv)

# Goal is to accelerate general Pandas workloads, but we don't want to rewrite Pandas from the ground up to do so (which was the approach
# that Baloo, Modin, Grizzly, Dask, etc. are all taking). Instead, we want to _incrementally_ shift existing libraries such as Pandas to
# use a runtime, and only in cases where the runtime will actually help improve performance or will exploit parallelism.

# 1. Annotate functions in Pandas, but and allow fallback to non-annoated Pandas functions
# 2. Support compilation for a subset of the Pandas functions.


