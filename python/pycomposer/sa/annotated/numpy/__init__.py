
# Fall back to NumPy if we don't support something.
from numpy import *

from .annotated import *

# Provide an explicit evaluate function.
from sa.annotation import evaluate
