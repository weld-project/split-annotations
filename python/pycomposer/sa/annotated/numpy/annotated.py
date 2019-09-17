
import numpy as np
import scipy.special as ss
import sharedmem

from copy import deepcopy as dc
from sa.annotation import *
from sa.annotation.split_types import *

class NdArraySplit(SplitType):

    def __init__(self):
        self.slice_col = False
        self.merge = False

    def combine(self, values):
        if self.merge:
            return np.concatenate(values)

    def split(self, start, end, value):
        if isinstance(value, np.ndarray):
            shape = value.shape
            ndims = len(value.shape)
            if ndims == 1:
                if start >= shape[0]:
                    return STOP_ITERATION
                return value[start:min(end, shape[0])]
            elif ndims == 2:
                if shape[1] == 1:
                    return value
                if self.slice_col:
                    return value[:,start:end]
                else:
                    return value[start:end,:]
            else:
                return NotImplementedError("ndarray with dim > 2 not supported")
        else:
            # Scalar.
            return value

    def elements(self, value):
        if isinstance(value, np.ndarray):
            if len(value.shape) == 2 and value.shape[1] == 1:
                return value.shape[0]
            return value.shape[-1]

    def __str__(self):
        return "NdArraySplit"

_args = (NdArraySplit(), NdArraySplit())
_kwargs = { 'out' : mut(NdArraySplit()), 'axis': Broadcast() }
_ret = NdArraySplit()


# Binary ops.
add         = sa(dc(_args), dc(_kwargs), dc(_ret))(np.add)
subtract    = sa(dc(_args), dc(_kwargs), dc(_ret))(np.subtract)
multiply    = sa(dc(_args), dc(_kwargs), dc(_ret))(np.multiply)
divide      = sa(dc(_args), dc(_kwargs), dc(_ret))(np.divide)
power       = sa(dc(_args), dc(_kwargs), dc(_ret))(np.power)

_args = (NdArraySplit(),)

# Unary ops.
log         = sa(dc(_args), dc(_kwargs), dc(_ret))(np.log)
log2        = sa(dc(_args), dc(_kwargs), dc(_ret))(np.log2)
exp         = sa(dc(_args), dc(_kwargs), dc(_ret))(np.exp)
sin         = sa(dc(_args), dc(_kwargs), dc(_ret))(np.sin)
arcsin      = sa(dc(_args), dc(_kwargs), dc(_ret))(np.arcsin)
cos         = sa(dc(_args), dc(_kwargs), dc(_ret))(np.cos)
sqrt        = sa(dc(_args), dc(_kwargs), dc(_ret))(np.sqrt)
erf         = sa(dc(_args), dc(_kwargs), dc(_ret))(ss.erf)

# addreduce = np.add.reduce
addreduce = sa(dc(_args), dc(_kwargs), dc(_ret))(np.add.reduce)

def ones(shape, dtype=None, order='C'):
    result = sharedmem.empty(shape)
    result[:] = np.ones(shape, dtype, order)[:]
    return result

def zeros(shape, dtype=None, order='C'):
    result = sharedmem.empty(shape)
    result[:] = np.zeros(shape, dtype, order)[:]
    return result
