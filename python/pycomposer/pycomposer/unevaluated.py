""" A singleton representing an unevaluated computation. """

class _Unevaluated:
    """ An unevaluated value.

    Users should access the UNEVALUATED singleton instead of
    making instances of this directly.

    """
    __slots__ = []

UNEVALUATED = _Unevaluated()


