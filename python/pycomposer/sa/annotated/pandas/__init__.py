
# Fall back to NumPy if we don't support something.
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
