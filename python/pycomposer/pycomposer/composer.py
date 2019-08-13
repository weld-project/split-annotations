
from .annotation import Annotation, mut
from .dag import LogicalPlan, evaluate_dag
from .split_types import *
from .vm.driver import DEFAULT_BATCH_SIZE

import functools 

import copy

# The task graph.
_DAG = LogicalPlan()

class sa(object):
    """ A splitability annotation."""

    def __init__(self, types, kwtypes, return_type):
        """ A splitability annotation.

        Parameters
        ----------

        postypes : a tuple of split types for each positional argument. The number of elements in the tuple must match the number
        of positional arguments in the funciton.

        kwtypes : a dictionary of split types for each keyword argument. Providing
        split types for keyword arguments is optional. If a keyword argument does
        not have a split type, its split type will default to "broadcast."

        return_type : split type of the value returned by this function.

        """
        self.types = types
        self.kwtypes = kwtypes
        self.return_type = return_type

    def __call__(self, func):
        annotation = Annotation(func, self.types, self.kwtypes, self.return_type)

        @functools.wraps(func)
        def _decorated(*args, **kwargs):
            return _DAG.register(func, args, kwargs, annotation)

        return _decorated

def evaluate(workers=1, batch_size=DEFAULT_BATCH_SIZE, profile=False):
    evaluate_dag(_DAG, workers, batch_size, profile)
