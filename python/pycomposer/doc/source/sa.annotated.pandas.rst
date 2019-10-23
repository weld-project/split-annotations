``sa.annotated.pandas``
=======================

The pandas integration is a work in progress, and currently supports a small set of ``Series`` methods.
Evaluation must be done explicitly with a call to ``evaluate``::

  >>> import sa.annotated.pandas as pd
  >>> x = pd.Series([1,2,3])
  >>> y = pd.Series([1,2,3])
  >>> result = x.add(y)
  >>> result
  <sa.annotated.dag.Operation object at ...>
  >>> evaluate()
  >>> result.value
  0    2
  1    4
  2    6
  dtype: int64
