This file describes how to install the or-tools python binary archive.

OR-Tools is located at https://developers.google.com/optimization

These modules have been tested under:
  - Ubuntu 14.04 and 16.04 up (64-bit).
  - Mac OS X El Capitan with Xcode 7.x (64 bit).
  - Microsoft Windows with Visual Studio 2013 and 2015 (64-bit)

Upon decompressing the archive, you will get the following structure:

or-tools/
  LICENSE-2.0.txt      <- Apache License
  check_python_deps.py <- Python script to check the dependencies
  README               <- This file
  data/                <- Data for the examples.
  examples/            <- Python examples
  ortools/             <- ortools package
    algorithms/        <- Directory containing non-graph algorithms.
    constraint_solver/ <- The main directory for the constraint library.
    graph/             <- Graph algorithms.
    linear_solver/     <- Linear solver wrapper.


To install the package, please run:
  make install
or  
  python setup.py install --user 

This will check the ortools module dependecies. You still can do it by running: 
  make check
or
  python check_python_deps.py

Once this is done, you can run the samples in the examples/ directory.
As in python examples/hidato_puzzle.py
