Getting Started with Split Annotations
======================================

This package contains examples of how to use split annotations (SAs) for your own
programs.

Disclaimer: unlike many of the examples shown here, SAs will be most effective
for pipelining when using C-backed libraries such as NumPy. However, SAs can
still parallelize pure Python functions.

Using the included annotated libraries
======================================

The ``sa.annotated`` package contains two annotated modules for you to try
out of the box: NumPy and Pandas. You can include each of them as follows::

    import sa.annotated.numpy as numpy
    import sa.annotated.pandas as pandas

These modules annotate a small subset of the NumPy and Pandas functions
available. They also re-export the remaining NumPy and Pandas functions, so
functions that applications use that do not have annotations should work as
always.

Writing your own annotations 
============================

Say we have a simple library that performs element-wise operations on Python
lists. It has a few functions, as define below::

    def add(a, b):
        """ add a and b elementswise """
        return [elem_a + elem_b for (elem_a, elem_b) in zip(a, b)]

    def multiply(a, b):
        """ multiply a and b elementswise """
        return [elem_a * elem_b for (elem_a, elem_b) in zip(a, b)]

    def scale(a, const_b):
        """ scale a by a constant elementswise """
        return [elem_a * const_b_b for elem_a in a]

    def scale_in_place(a, const_b):
        """ scale a by a constant elementswise in-place"""
        for i in range(len(a)):
            a[i] = a[i] * const_b

We can annotate these using SAs to obtain a few benefits. First,
split annotations will allow pipelining chains of calls to `add` and
`multiply`. While this may not have a huge effect on Python lists (which use
lots of indirection when executing code like this, for contiguously allocated
data structures such as NumPy arrays, this can have a huge impact (up to 100x!)
on performance.

Second, annotating functions will allow the SA runtime to parallelize the
functions. The functions above are written in Python, so the runtime will
parallelize them using processes and communicate results back using RPC and
serialization.

Defining split types
--------------------

The first thing to do when writing SAs is to define *split types*, which are classes that define how to split data.
Split types will inheret from the ``sa.annotation.split_types.SplitType`` abstract class. Let's write a split type for
Python lists::

    from sa.annotation import STOP_ITERATION
    from sa.annotation.split_types import SplitType

    class ListSplit(SplitType):
        def combine(self, values):
            return list(itertools.chain.from_iterable(values))    
            
        def split(self, start, end, value):
            if start >= len(value):
                return STOP_ITERATION 
            if end > len(value):
                return value[start:len(value)]
            else:
                return value[start:end]

The ``combine`` function concatenates a list of the type being split--in this
case a Python list--into a single value. The ``split`` function splits the
value into an iteratable of values of the same type. Note that slicing a Python
list actually creates a copy of the list, while slicing more efficient types
only create *views* (references).

Writing annotations
-------------------

Having defined a split type, we can use it to annotate our functions. Let's add
a function decorator over the ``add`` function to define how to split it::

    from sa.annotation import sa

    @sa((ListSplit(), ListSplit()), {}, ListSplit())
    def add(a, b):
        """ add a and b elementswise """
        return [elem_a + elem_b for (elem_a, elem_b) in zip(a, b)]

The SA takes a tuple that assigns a split type for each positional argument (in
this case, ``a`` and ``b``), a dictionary that optionally assigns a split type
for each keyword argument, and a split type for the return argument. We need to
specify a split type for the returned value so the system knows which
``combine`` function to call when execution finishes.

As a second example, let's add an annotation on the ``scale`` function, which takes a scalar argument::

    from sa.annotation import sa
    from sa.annotation.split_types import Broadcast

    @sa((ListSplit(), Broadcast()), {}, ListSplit())
    def scale(a, const_b):
        """ scale a by a constant elementswise """
        return [elem_a * const_b_b for elem_a in a]

Notice how we used a special builtin split type ``Broadcast`` in this example.
This split type just tells the system to copy and broadcast the value to each
parallel worker. In this case, we want to broadcast the constant ``const_b``
instead of splitting it.

As a final example, let's look at a function that returns nothing and modifies
an argument in-place::

    from sa.annotation import sa, mut
    from sa.annotation.split_types import Broadcast

    @sa((mut(ListSplit()), Broadcast()), {}, None)
    def scale(a, const_b):
        """ scale a by a constant elementswise """
        return [elem_a * const_b_b for elem_a in a]

Notice how we wrapped the split type for the first argument (which is mutated
during execution of the function) with ``mut``. This signals to the system that
the argument will be mutated, and allows it to draw the correct dependencies
between this function and other functions that may also read and write the
argument.
