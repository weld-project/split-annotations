## Split Annotations

This is the main source code repository for Split Annotations. It contains the source code for the C implementation, the Python implementation, and the benchmarks from the SOSP 2019 paper.

Split annotations (SAs) are a system for enabling optimizations such as pipelining and parallelization underneath existing libraries. Other approaches for enabling these optimizations, such as intermediate representations, compilers, or DSLs, are heavyweight solutions that require re-architecting existing code. Unlike these approaches, SAs enable these optimizations _without requiring changes to existing library functions_.

You can get started with split annotations in Python with: `pip install sa`.

See [Installing from Source](#installing-from-source) for instructions on how to build the C version of split annotations.

## Installing from Source

1. Make sure you have the required dependencies:

  * Python 3.5
  * `virtualenv`
  * The latest version of [Rust](https://rustup.rs/). See the instructions in the link.
  * `git`
  * `pkgconfig`. You can download it as follows:
  
  ```bash
  sudo apt-get install pkg-config
  ```
  
  * The `build-essential` package on Linux distributions. You can download it as follows:
  
  ```bash
  sudo apt-get update
  sudo apt-get install build-essential
  ```
  
To build the C implementation:
  
2. Clone this repository and set the `$SA_HOME` environment variable (the latter is not necessary but simplifies the remaining steps):

  ```bash
  cd $HOME
  git clone https://github.com/weld-project/split-annotations.git
  cd split-annotations
  export SA_HOME=`pwd`
  ```
  
3. Build the C implementation:

  ```bash
  cd $SA_HOME/c
  cargo build --release
  ```
   
4. Optionally build the provided annotated C libraries (Intel MKL and ImageMagick). See `EXPERIMENTS.md` for directions on how to build MKL and ImageMagick, and then:

  ```bash
  cd $SA_HOME/c/lib/composer_mkl
  make
  cd $SA_HOME/c/lib/ImageMagick
  make
  ```
  
The Python implementation does not require any special installation, but running the benchmarks requires certain dependencies. See the instructions in `EXPERIMENTS.md`.

## Get Help

If you need help installing or using split annotations, or have general questions about the project, feel free to either create a GitHub issue or email shoumik @ stanford . edu (with the spaces removed).
