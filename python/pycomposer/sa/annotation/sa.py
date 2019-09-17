from .annotation import Annotation, mut
from .config import config
from .dag import LogicalPlan, evaluate_dag
from .split_types import *
from .vm.driver import DEFAULT_BATCH_SIZE

import functools 

import copy

# The task graph.
_DAG = LogicalPlan()

class sa(object):
    """ A split annotation.

    Split annotations are Python decorators over ordinary Python functions or
    methods. For each function, a split annotation uses *split types* to define
    how each argument in the function is split. By splitting function
    arguments, the underlying runtime can introduce parallelization and
    optimizations such as loop pipelining.

    """

    def __init__(self, types, kwtypes, return_type):
        """ Creates a split annotation (SA) for the provided function signature.

        The SA can either be used as a Python decorator or as a function that
        is called on another function. For the latter, users should
        ``deepcopy`` this class for each annotated function.

        Parameters
        ----------

        postypes : tuple of SplitType
            a tuple of split types for each positional argument. The number of
            elements in the tuple must match the number of positional arguments
            in the function.

        kwtypes : dict from str -> SplitType or None
            a dictionary of split types for each keyword argument. Providing
            split types for keyword arguments is optional. If a keyword
            argument does not have a split type, its split type will default to
            "broadcast."

        return_type : SplitType or None
            split type of the value returned by this function.

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

def evaluate(workers=config["workers"], batch_size=config["batch_size"], profile=False):
    evaluate_dag(_DAG, workers, batch_size, profile)
