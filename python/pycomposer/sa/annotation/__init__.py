"""

The ``sa.annotation`` package defines the split annotations (SAs) users use to annotate library
functions. It contains the annotation itself, as well as an abstract
``SplitType`` class, which defines how function arguments are split.

Here is a basic example of how to write an SA for a simple function
that adds two lists element-wise::

    @sa((ListSplit(), ListSplit()), {}, ListSplit()))
    def add_vectors(x, y):
        ...

The SA takes three arguments: a tuple that assigns a split type for each
positional argument, a dictionary that optionally assigns a split type for each
keyword argument (this function has none), and a final split type for the
return type.

The split type ``ListSplit`` is a subclass of
``sa.annotation.split_types.SplitType``, which itself is an abstract class with
a ``split`` and ``combine`` methods. These methods define how to partition and
re-combine values before and after execution. As an example, the implementation of
``ListSplit`` may look as follows::

    class ListSplit(SplitType):
        def split(self, start, end, value):
            # NOTE: some additional bounds checking may be required.
            return value[start:end]

        def combine(self, values):
            import itertools
            # Combine list of lists into flat list
            return list(itertools.chain.from_iterable(values))

The split types and annotations together allows *chains* of annotated functions
to be pipelined and parallelized together.  The types ensure that
"incompatible" functions are never pipelined together, and also allow users to
define custom behavior for how the types in their library should be split.
"""

from .sa import sa, evaluate, mut
from .vm.driver import STOP_ITERATION
