"""
The ``sa.annotated`` package describes annotated libraries that are included as a part of the
``sa.annotated`` package.

These libraries can be imported as a drop-in replacement for the libraries that
they annotate. Annotated functions will automatically use the SA runtime during
execution. Functions that are not supported are re-exported and can be used as
always.

As an example, to use the annotated version of NumPy, one can replace the following import statement::

  import numpy as np

with::

  import sa.annotated.numpy as np

"""

