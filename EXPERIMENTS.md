## SOSP 2019 Experiments

This document describes how to run the main experiments in the SOSP 2019 paper. The goal of this document is to satisfy the ACM "Artifact Functional" requirements.

Most of the setup required in this document in done already in the Amazon AWS AMI image described below. *In particular, the steps that are pre-installed on this AMI are marked as "**(preinstalled)**."*

## System Requirements

We have only tested the experiments on an m4.10xlarge Amazon EC2 instance. We
have made a public AMI that can be used to run everything, with all necessary
packages etc. pre-installed.

| Field  | Value |
| -------------  | ------------- |
| Cloud Provider | AWS |
| Region         | us-west-2  |
| AMI ID         | ami-0312f629d1551e6b2  |
| AMI Name       | split-annotations-public-sosp19 |
| Instance Type  | m4.10xlarge |

See [this link](https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/finding-an-ami.html) for how to find and launch a public AMI (this assumes you have a valid billable AWS account setup).

For anyone running outside of this environment, the assumed system requirements are:

* At least 150GB of RAM
* At least 200GB of disk space
* At least 16 cores that can compile an optimized version of Intel MKL
* Running Ubuntu 16.04 with a recent Linux kernel (we've only tested it on 4.4.0)

## C Experiments

1. **(preinstalled)** Follow the build instructions in the README in this directory. Add the following to your `rc` file:

```
export WELD_HOME=$HOME/weld/ # We will install Weld here for the Weld baselines
export SA_HOME=<path-to-this-repo> # this directory should live in $HOME.
export PATH=$SA_HOME/c/target/release:$PATH
export LD_LIBRARY_PATH=$SA_HOME/c/target/release:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$SA_HOME/c/lib/composer_mkl:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$SA_HOME/c/lib/ImageMagick:$LD_LIBRARY_PATH
```

And then:

```bash
# Or whatever your rc file is
source ~/.bashrc
```

2. **(preinstalled)** Install Intel MKL and ImageMagick, as described below. If these are preinstalled, skip to step 3 below.

---

  #### Installing MKL (skip to Step 3 if preinstalled)

  We tested our code with MKL 2018 (Update 2). To install, try the following:
  
  ```bash
  wget http://registrationcenter-download.intel.com/akdlm/irc_nas/tec/12725/l_mkl_2018.2.199.tgz
  tar zxvf l_mkl_2018.2.199.tgz
  cd l_mkl_2018.2.199
  ./install.sh
  ```
  
  and follow the on-screen instructions. If using EC2, we suggest using the second installation option, "Install using sudo privileges."
  
  If the `wget` doesn't work, visit [this link](https://software.seek.intel.com/performance-libraries) and follow the instructions below.

  1. Fill out the information requested in the form and click "Submit"
  2. In the dropdown menu stating "Please select a Product" choose "Intel Math Kernel Library for Linux"
  3. Under "Choose a Version" choose "Intel MKL 2018 (Update 2)"
  4. Right-click "Full Package" and copy the link. `wget` as above, and the continue below.

  Once MKL is set up, make sure that the `$MKLROOT` environment variable is set to the correct value. On our system, it is set to the following:

  ```bash
  /opt/intel/compilers_and_libraries_2018.2.199/linux/mkl
  ```
  
  We suggest adding it to your `rc` file:
  
  ```bash
  export MKLROOT=/opt/intel/compilers_and_libraries_2018.2.199/linux/mkl
  source /opt/intel/bin/compilervars.sh intel64
  ```
  
  And then `source ~/.bashrc` (or whatever your `rc` file is for your shell).

  #### Installing ImageMagick (skip to Step 3 if preinstalled)

  We use ImageMagick-7 in our benchmarks. To install:

  1. Make sure build tools are available and up to date, and install `libtiff5`:

  ```bash
  sudo apt-get update
  sudo apt-get install build-essential libtiff5-dev
  sudo ldconfig
  ```

  2. Install ImageMagick from source:

  ```bash
  cd $HOME
  wget https://www.imagemagick.org/download/ImageMagick.tar.gz
  tar xvzf ImageMagick.tar.gz
  # Your minor version may be different, but the major version should be 7
  cd ImageMagick-7.0.8-59
  ```

  3. Configure, build and install:

  ```bash
  ./configure --with-tiff=yes
  # Set to number of cores on your machine
  make -j 40
  sudo make install
  sudo ldconfig
  ```

  4. Make sure everything worked:

  ```bash
  magick -version | head -1
  ```
 
  You should see `ImageMagick 7.xxxx`.
  
---
    
3. Build the annotated libraries. Assuming `$SA_HOME` is the root directory:

  ```bash
  cd $SA_HOME/c/lib/composer_mkl
  make
  cd $SA_HOME/c/lib/ImageMagick
  make
  ```

4. Run the benchmarks using the provided script. We suggest doing this in a `tmux` session, since it will take some time to complete. This will also download all the data needed to run the benchmarks. Make sure you change to the correct directory first, because some things use relative directories:

  ```bash
  cd $SA_HOME/c/benchmarks/
  ./run-all.sh
  ```

  The results will be in the `$SA_HOME/c/benchmarks/results` directory.

## Python Experiments (Mozart and Numba, Native Library, and Bohrium baselines)

1. **(preinstalled)** Install the necessary packages:

  ```bash
  sudo apt-get install python2.7-dev python3.5-dev unzip virtualenv
  ```

2. To run the Python experiments, go to the benchmark directory and run the provided `run-all.sh` script. This will set up an environment and download the necessary data, and run everything:

  ```bash
  cd $SA_HOME/python/benchmarks
  ./run-all.sh
  ```
  
  The results will be in the `$SA_HOME/python/benchmarks/results` directory.

## Weld Baselines

Since Weld requires slightly different Python distribution requirements and other dependencies, we run them in a separate virtual environment. Make sure everything
is run from the appropriate directory (e.g., `$HOME` if `cd $HOME` is specified):

1. **(preinstalled)** Clone the Weld repo. Make sure you are the `v0.2.0` branch, which supports multi-threading.

  ```bash
  cd $HOME
  git clone -b v0.2.0 https://github.com/weld-project/weld.git
  ```

2. **(preinstalled)** Make sure LLVM is installed and that everything is configured properly. In particular, you should be able to run `llvm-config --version` and see `6.x.x`. If you don't have
LLVM, run the following, which downloads all the Weld requirements:

  ```bash
  wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
  sudo apt-add-repository "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-6.0 main"
  sudo apt-get update
  ```
  
  Then install:
  ```bash
  sudo apt-get install llvm-6.0-dev clang-6.0 zlib1g-dev
  ```
  
  And link:
  ```bash
  sudo ln -s /usr/bin/llvm-config-6.0 /usr/local/bin/llvm-config
  ```

3. **(preinstalled)** Build Weld. You should already have Rust installed for Mozart:

  ```bash
  cd $HOME/weld
  cargo build --release
  ```

4. **(preinstalled)** Clone the Weld experiments.

  ```bash
  cd $HOME
  git clone https://github.com/sppalkia/weld-experiments-mozart.git
  ```

5. Run the experiments. This will build the Weld versions of Pandas and NumPy, setup
a environment, install the requirements, and run each experiment. **NOTE:** these should be run after running the experiments in the main repository, because the `run-all.sh` script will generate data that this script accesses.

  ```bash
  cd weld-experiments-mozart
  # Run all the benchmarks
  ./run-all.sh
  ```

This should conclude the main results of the paper. Please email shoumik@cs.stanford.edu with any questions.
