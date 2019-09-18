#!/usr/bin/env python

import setuptools

with open("README.md", "r") as fh:
    long_description = fh.read()

setuptools.setup(name='sa',
      version='0.0.2',
      description='Python split annotations package',
      long_description=long_description,
      long_description_content_type="text/markdown",
      author='Shoumik Palkar',
      packages=setuptools.find_packages(),
      author_email='shoumik@cs.stanford.edu',
      url='https://www.github.com/weld-project/split-annotations',
      classifiers=[
          "Programming Language :: Python :: 3",
          "License :: OSI Approved :: BSD License",
          "Operating System :: OS Independent",
          ],
      python_requires='>=3.6',
      )
