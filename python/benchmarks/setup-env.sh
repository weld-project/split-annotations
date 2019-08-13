#!/bin/bash

virtualenv -p python3.5 benchmarks
source benchmarks/bin/activate

# Install everything
pip install -r requirements.txt
