``sa.annotated.numpy``
======================

The NumPy integration is a work in progress, and currently requires some additional code beyond just an
import statement. In particular, users need to allocate memory using the ``sharedmem`` package and explicitly
call ``evaluate`` to execute annotated functions. Here is a minimal example::

  # In place computation needs shared memory.
  a = sharedmem.empty(100)
  a[:] = np.ones(100)[:]

  # This is an annotated function that updates a in-place
  scale_inplace(a, 2)
  scale_inplace(a, 2)
  scale_inplace(a, 2)
  scale_inplace(a, 2)

  np.evaluate()
