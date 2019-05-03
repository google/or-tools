# OR-Tools - Google Optimization Tools

[![Build
Status](https://travis-ci.org/google/or-tools.svg?branch=master)](https://travis-ci.org/google/or-tools)
[![Build status](https://ci.appveyor.com/api/projects/status/9hyykkcm8sh3ua6x?svg=true)](https://ci.appveyor.com/project/lperron/or-tools-98u1n)  
[![PyPI version](https://badge.fury.io/py/ortools.svg)](https://pypi.org/project/ortools/)
[![PyPI download](https://img.shields.io/pypi/dm/ortools.svg)](https://pypi.org/project/ortools/#files)
[![Binder](https://mybinder.org/badge.svg)](https://mybinder.org/v2/gh/google/or-tools/master)  
[![NuGet version](https://badge.fury.io/nu/Google.OrTools.svg)](https://www.nuget.org/packages/Google.OrTools)
[![NuGet download](https://img.shields.io/nuget/dt/Google.OrTools.svg)](https://www.nuget.org/packages/Google.OrTools)

Google's software suite for combinatorial optimization.

## Table of Contents

*   [About OR-Tools](#about-or-tools)
*   [Codemap](#codemap)
*   [Installation](#installation)
*   [Experimental Build with CMake](#experimental-build-with-cmake)
*   [Quick Start](#quick-start)
*   [Documentation](#documentation)
*   [Contributing](#contributing)
*   [License](#license)

## About OR-Tools

Google Optimization Tools (a.k.a., OR-Tools) is an open-source, fast and
portable software suite for solving combinatorial optimization problems.

The suite contains:
* A constraint programming solver;
* A linear programming solver;
* Wrappers around commercial and other open source solvers, including mixed
integer solvers;
* Bin packing and knapsack algorithms;
* Algorithms for the Traveling Salesman Problem and Vehicle Routing Problem;
* Graph algorithms (shortest paths, min cost flow, max flow, linear sum
assignment).

We wrote OR-Tools in C++, but also provide wrappers in Python, C# and
Java.

## Codemap

This software suite is composed of the following components:

* [Makefile](Makefile) Top-level for [GNU Make](https://www.gnu.org/software/make/manual/make.html) based build.
* [makefiles](makefiles) Subsidiary Make files.
* [CMakeLists.txt](CMakeLists.txt) Top-level for [CMake](https://cmake.org/cmake/help/latest/) based build.
* [cmake](cmake) Subsidiary CMake files.
* [bazel](bazel) Subsidiary Bazel files.
  * [BUILD](bazel/BUILD) Top-level for [Bazel](https://docs.bazel.build/versions/master/bazel-overview.html) based build.

* [ortools](ortools) Root directory for source code.
  * [base](ortools/base) Basic utilities.
  * [algorithms](ortools/algorithms) Basic algorithms.
    * [csharp](ortools/algorithms/csharp) .Net wrapper.
    * [java](ortools/algorithms/java) Java wrapper.
    * [python](ortools/algorithms/python) Python wrapper.
  * [graph](ortools/graph) Graph algorithms.
    * [csharp](ortools/graph/csharp) .Net wrapper.
    * [java](ortools/graph/java) Java wrapper.
    * [python](ortools/graph/python) Python wrapper.
  * [linear_solver](ortools/linear_solver) Linear solver wrapper.
    * [csharp](ortools/linear_solver/csharp) .Net wrapper.
    * [java](ortools/linear_solver/java) Java wrapper.
    * [python](ortools/linear_solver/python) Python wrapper.
  * [glop](ortools/glop) Google linear solver.
  * [lp_data](ortools/lp_data) Data structures for linear models.
  * [constraint_solver](ortools/constraint_solver) Constraint and Routing solver.
    * [csharp](ortools/constraint_solver/csharp) .Net wrapper.
    * [java](ortools/constraint_solver/java) Java wrapper.
    * [python](ortools/constraint_solver/python) Python wrapper.
  * [sat](ortools/sat) SAT solver.
    * [csharp](ortools/sat/csharp) .Net wrapper.
    * [java](ortools/sat/java) Java wrapper.
    * [python](ortools/sat/python) Python wrapper.
  * [bop](ortools/bop) Boolean solver based on SAT.
  * [util](ortools/util) Utilities needed by the constraint solver
  * [com/google/ortools](ortools/com/google/ortools) Java source files.

* [examples](examples) Root directory for all examples.
  * [contrib](examples/contrib) Examples from the community.
  * [cpp](examples/cpp) C++ examples.
  * [dotnet](examples/dotnet) .Net examples.
  * [java](examples/java) Java examples.
  * [python](examples/python) Python examples.
  * [notebook](examples/notebook) Jupyter/IPython notebooks.
  * [flatzinc](examples/flatzinc) FlatZinc examples.
  * [data](examples/data) Data files for examples.
  * [tests](examples/tests) Unit tests and bug reports.

* [tools](tools) Delivery Tools (e.g. Windows GNU binaries, scripts, dockers)

## Installation

This software suite has been tested under:
- Ubuntu 16.04 and up (64-bit);
- Apple macOS Mojave with Xcode 7.x (64-bit);
- Microsoft Windows with Visual Studio 2017 (64-bit).

OR-Tools currently builds with a Makefile, but also provides Bazel and CMake support.

For installation instructions (both source and binary), please visit
https://developers.google.com/optimization/introduction/installing.

## Experimental Build with CMake

We also provide experimental CMake support.<br>Please check the
[CMake build instructions](cmake/README.md).

## Quick Start

The best way to learn how to use OR-Tools is to follow the tutorials in our
developer guide:

https://developers.google.com/optimization/introduction/get_started

If you want to learn from code examples, take a look at the examples in the
[examples](examples) directory.

## Documentation

The complete documentation for OR-Tools is available at:
https://developers.google.com/optimization/

## Contributing

The [CONTRIBUTING.md](CONTRIBUTING.md) file contains instructions on how to
submit the Contributor License Agreement before sending any pull requests (PRs).
Of course, if you're new to the project, it's usually best to discuss any
proposals and reach consensus before sending your first PR.

## License

The OR-Tools software suite is licensed under the terms of the Apache License 2.0.
<br>See [LICENSE-2.0](LICENSE-2.0.txt) for more information.
