# OR-Tools Makefile Build Instructions

## Introduction
<nav for="make"> |
<a href="#requirement">Requirement</a> |
<a href="#dependencies">Dependencies</a> |
<a href="docs/ci.md">CI</a> |
</nav>

**warning: Makefile build is deprecated, please use CMake or Bazel.**

OR-Tools comes with a GNU Make based build ([Makefile](../Makefile)) that can be
used on a wide range of platforms.

## Requirement
You'll need:

*   `GNU Make >= 4.3`.
*   `CMake >= 3.24`.
*   A C++20 compiler (GCC 10 or later, MSVC 2022 or later).

## Dependencies

OR-Tools depends on several mandatory libraries, either as infrastructure or as
optimization solvers. You can either compile all of these dependencies using the
target `third_party` or compile some of them on your own and give their
installation directories to the others using the Make variables below.

*   zlib (`UNIX_ZLIB_DIR` or `WINDOWS_ZLIB_DIR`),
*   Google Abseil-cpp (`UNIX_ABSL_DIR` or `WINDOWS_ABSL_DIR`),
*   Google Protobuf (`UNIX_PROTOBUF_DIR` or `WINDOWS_PROTOBUF_DIR`),
*   SCIP (`UNIX_SCIP_DIR` or `WINDOWS_SCIP_DIR`),
*   COIN-OR solvers:
    *   COIN-OR CoinUtils (`UNIX_COINUTILS_DIR` or `WINDOWS_COINUTILS_DIR`),
    *   COIN-OR Osi (`UNIX_OSI_DIR` or `WINDOWS_OSI_DIR`),
    *   COIN-OR Clp (`UNIX_CLP_DIR` or `WINDOWS_CLP_DIR`),
    *   COIN-OR Cgl (`UNIX_CGL_DIR` or `WINDOWS_CGL_DIR`),
    *   COIN-OR Cbc (`UNIX_CBC_DIR` or `WINDOWS_CBC_DIR`),

OR-Tools can also optionally (disabled by default) be compiled with support for
the following third-party solvers:

*   CPLEX (`UNIX_CPLEX_DIR` or `WINDOWS_CPLEX_DIR`),
*   GLPK (`UNIX_GLPK_DIR` or `WINDOWS_GLPK_DIR`),
*   GUROBI (`UNIX_GUROBI_DIR` or `WINDOWS_GUROBI_DIR`),
*   XPRESS (`UNIX_XPRESS_DIR` or `WINDOWS_XPRESS_DIR`)

**warning: Since these solvers are either proprietary (and require a specific
license) or available under the GPL, we can't test them on public CI and their
support may be broken.**
