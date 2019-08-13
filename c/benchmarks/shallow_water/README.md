
## Shallow Water Benchmark

This benchmark is based on the reference solution provided [here](https://github.com/mrocklin/ShallowWater/blob/master/shallowwater_simple.py). The actual workload simulates the flow of a disturbed fluid based on the equations described [here](http://en.wikipedia.org/wiki/Shallow_water_equations).

Since thie benchmark is more complex than Haversine and Black Scholes, it is divided into several files:

* `shallow_water.c` is the driver and contains `main()` and utilities for creating inputs, etc.
* `shallow_water_mkl.c` implements the workload using MKL.
* `shallow_water_composer.c` implements the workload using Composer. This basically just adds the `c_` prefix to all the MKL functions that are supported (note that if we were using C++, we could've just copied and pasted the MKL file and used namespaces).
