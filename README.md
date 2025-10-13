# OR-Tools - Google Optimization Tools

[![PyPI version](https://img.shields.io/pypi/v/ortools.svg)](https://pypi.org/project/ortools/)
[![PyPI download](https://img.shields.io/pypi/dm/ortools.svg)](https://pypi.org/project/ortools/#files)
\
[![NuGet version](https://img.shields.io/nuget/v/Google.OrTools.svg)](https://www.nuget.org/packages/Google.OrTools)
[![NuGet download](https://img.shields.io/nuget/dt/Google.OrTools.svg)](https://www.nuget.org/packages/Google.OrTools)
\
[![Maven Central](https://img.shields.io/maven-central/v/com.google.ortools/ortools-java)](https://mvnrepository.com/artifact/com.google.ortools/ortools-java)
\
[![Discord](https://img.shields.io/discord/693088862481678374?color=7289DA&logo=discord&style=plastic)](https://discord.gg/ENkQrdf)

Google's software suite for combinatorial optimization.

## Table of Contents

*   [About OR-Tools](#about)
*   [Codemap](#codemap)
*   [Installation](#installation)
*   [Quick Start](#quick-start)
*   [Documentation](#documentation)
*   [Contributing](#contributing)
*   [License](#license)

<a name="about"></a>
## About OR-Tools

Google Optimization Tools (a.k.a., OR-Tools) is an open-source, fast and
portable software suite for solving combinatorial optimization problems.

The suite contains:

*   Two constraint programming solver (CP* and CP-SAT);
*   Two linear programming solvers (Glop and PDLP);
*   Wrappers around commercial and other open source solvers, including mixed
    integer solvers;
*   Bin packing and knapsack algorithms;
*   Algorithms for the Traveling Salesman Problem and Vehicle Routing Problem;
*   Graph algorithms (shortest paths, min cost flow, max flow, linear sum
    assignment).

We wrote OR-Tools in C++, but provide wrappers in Python, C# and Java.

## Codemap

This software suite is composed of the following components:

*   [Makefile](Makefile) Top-level for
    [GNU Make](https://www.gnu.org/software/make/manual/make.html) based build.
*   [makefiles](makefiles) Subsidiary Make files, CI and build system documentation.
*   [CMakeLists.txt](CMakeLists.txt) Top-level for
    [CMake](https://cmake.org/cmake/help/latest/) based build.
*   [cmake](cmake) Subsidiary CMake files, CI and build system documentation.
*   [WORKSPACE](WORKSPACE) Top-level for
    [Bazel](https://bazel.build/start/bazel-intro) based build.
*   [bazel](bazel) Subsidiary Bazel files, CI and build system documentation.
*   [ortools](ortools) Root directory for source code.
    *   [base](ortools/base) Basic utilities.
    *   [algorithms](ortools/algorithms) Basic algorithms.
        *   [samples](ortools/algorithms/samples) Carefully crafted samples.
    *   [graph](ortools/graph) Graph algorithms.
        *   [samples](ortools/graph/samples) Carefully crafted samples.
    *   [linear_solver](ortools/linear_solver) Linear solver wrapper.
        *   [samples](ortools/linear_solver/samples) Carefully crafted samples.
    *   [glop](ortools/glop) Simplex-based linear programming solver.
        *   [samples](ortools/glop/samples) Carefully crafted samples.
    *   [pdlp](ortools/pdlp) First-order linear programming solver.
        *   [samples](ortools/pdlp/samples) Carefully crafted samples.
    *   [lp_data](ortools/lp_data) Data structures for linear models.
    *   [constraint_solver](ortools/constraint_solver) Constraint and Routing
        solver.
        *   [docs](ortools/constraint_solver/docs) Documentation of the component.
        *   [samples](ortools/constraint_solver/samples) Carefully crafted samples.
    *   [sat](ortools/sat) SAT solver.
        *   [docs](ortools/sat/docs) Documentation of the component.
        *   [samples](ortools/sat/samples) Carefully crafted samples.
    *   [bop](ortools/bop) Boolean solver based on SAT.
    *   [util](ortools/util) Utilities needed by the constraint solver
*   [examples](examples) Root directory for all examples.
    *   [contrib](examples/contrib) Examples from the community.
    *   [cpp](examples/cpp) C++ examples.
    *   [dotnet](examples/dotnet) .Net examples.
    *   [java](examples/java) Java examples.
    *   [python](examples/python) Python examples.
    *   [notebook](examples/notebook) Jupyter/IPython notebooks.
    *   [flatzinc](examples/flatzinc) FlatZinc examples.
    *   [tests](examples/tests) Unit tests and bug reports.
*   [tools](tools) Delivery Tools (e.g. Windows GNU binaries, scripts, release dockers)

## Installation

This software suite has been tested under:

*   Ubuntu 18.04 LTS and up (64-bit);
*   Apple macOS Mojave with Xcode 9.x (64-bit);
*   Microsoft Windows with Visual Studio 2022 (64-bit).

OR-Tools currently builds with a Makefile, but also provides Bazel and CMake
support.

For installation instructions (both source and binary), please visit
https://developers.google.com/optimization/introduction/installing.

### Build from source using Make (legacy)

We provide a Make based build.<br>Please check the
[Make build instructions](makefiles/README.md).

### Build from source using CMake

We provide a CMake based build.<br>Please check the
[CMake build instructions](cmake/README.md).

### Build from source using Bazel

We provide a Bazel based build.<br>Please check the
[Bazel build instructions](bazel/README.md).

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
<br>See [LICENSE](LICENSE) for more information.
