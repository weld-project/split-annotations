
code = "|a: vec[i64], b: vec[i64]| {a, result(for(zip(a, b), appender, |b, i, e| merge(b, e.$0 + e.$1)))}"

from compiled import *
from encoders import *

import numpy as np

myfunc = compile(code, (NumpyArrayEncoder(), NumpyArrayEncoder()), WeldVec(WeldLong()), NumpyArrayDecoder())

a = np.ones(5, dtype=np.int64)
b = np.ones(5, dtype=np.int64)

print(myfunc(a, b))
