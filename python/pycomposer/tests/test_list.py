
import itertools

from sa.annotation import evaluate, sa, mut
from sa.annotation.split_types import Broadcast, SplitType

import sa.annotation.config as sa_config

import numpy as np
import sharedmem

sa_config.config["workers"] = 2
sa_config.config["batch_size"] = 1

class ListSplit(SplitType):
    def combine(self, values):
        if values is not None:
            return list(itertools.chain.from_iterable(values))    
        
    def split(self, start, end, value):
        if start >= len(value):
            return STOP_ITERATION 
        if end > len(value):
            return value[start:len(value)]
        else:
            return value[start:end]

@sa((ListSplit(), ListSplit()), {}, ListSplit())
def add(a, b):
    """ add a and b elementswise """
    return [elem_a + elem_b for (elem_a, elem_b) in zip(a, b)]

@sa((ListSplit(), Broadcast()), {}, ListSplit())
def scale(a, const_b):
    """ scale a by a constant elementswise """
    return [elem_a * const_b for elem_a in a]

@sa((mut(ListSplit()), Broadcast()), {}, None)
def scale_inplace(a, const_b):
    """ scale a by a constant elementswise in place"""
    np.multiply(a, const_b, out=a)

class TestList:
    def test_add(self):
        a = [1] * 100
        b = [1] * 100

        c = add(a, b)
        c = add(a, c)
        c = add(a, c)
        c = add(a, c)

        assert c[0] == 5

    def test_scale(self):
        a = [1] * 100
        c = scale(a, 2)
        c = scale(c, 2)
        c = scale(c, 2)
        c = scale(c, 2)

        assert c[0] == 16

    def test_scale_inplace(self):
        # In place computation needs shared memory.
        a = sharedmem.empty(100)
        a[:] = np.ones(100)[:]

        scale_inplace(a, 2)
        scale_inplace(a, 2)
        scale_inplace(a, 2)
        scale_inplace(a, 2)

        # NOTE: can do this automatically?
        evaluate()

        assert a[0] == 16
