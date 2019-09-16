""" A singleton representing an unevaluated computation. """

class _Unevaluated:
    """ An unevaluated value. """
    __slots__ = []

# UNEVALUATED singleton. 
UNEVALUATED = _Unevaluated()


