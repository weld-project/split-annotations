split annotations: optimizing apps that combine APIs
****************************************************

**Current version: 0.0.2**

Welcome to split annotations for Python!

Split annotations (SAs) are a system for enabling optimizations such as
pipelining and parallelization underneath existing libraries. Other approaches
for enabling these optimizations, such as intermediate representations,
compilers, or DSLs, are heavyweight solutions that require re-architecting
existing code. Unlike these approaches, SAs enable these optimizations without
requiring changes to existing library functions.

Split annotations are meant to be easy to integrate compared to compilers or
other runtimes.

You can install split annotations on most systems with ``pip install sa``. SAs
require Python 3.5 or above.

**Useful Links**:
`GitHub Repository <https://github.com/weld-project/split-annotations>`_ |
`SOSP Paper <https://shoumik.xyz/static/papers/mozart-sosp19final.pdf>`_ |
`PyPi <https://pypi.org/project/sa/>`_

Documentation
=============

.. toctree::
    :maxdepth: 1

    Home <index>
    getting_started
    api_reference
    annotated_libraries

Index
=============

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
