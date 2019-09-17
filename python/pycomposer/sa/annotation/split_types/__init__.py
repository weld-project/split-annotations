"""
This ``sa.annotation.split_types`` package defines the abstract ``SplitType``
class that users should subclass to define their own split types.

This module also contains the builtin ``Broadcast`` split type, which indicates
that a value should be broadcast (copied) rather than split. Broadcasted values
should be read-only within the function since the runtime may choose to share
them by reference.

The subpackage ``generics`` also contains names for *generic split types*,
which can be used when any split type is suitable as long as the split types
among the arguments and return types are consistent.

"""

from .split_types import *
