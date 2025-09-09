# GLOP

GLOP is Google's Linear Optimization Package. It is a full-featured,
production-ready implementation of the revised simplex method designed for
performance and numerical stability in solving linear programming problems.

It's written in C++ but can also be used from Java, Python, or C# via our
[linear solver wrapper](../linear_solver).

It contains in particular:

* [`parameters.proto`][parameters_proto]: Contains the definitions for all the
  GLOP algorithm parameters and their default values.
* [`lp_solver.h`][lp_solver_h]: Contains the primary entry point class
  `LPSolver`.
* [`preprocessor.h`][preprocessor_h]: Contains the presolving code for a
  Linear Program.
* [`revised_simplex.h`][revised_simplex_h]: Contains the core implementation of
  the revised simplex algorithm as described by G.B. Dantzig.
* [`status.h`][status_h]: Return type for various solver functions.

<!-- Links used throughout the document. -->
[parameters_proto]: ../glop/parameters.proto
[benchmark_proto]: ../glop/benchmark.proto
[lp_solver_h]: ../glop/lp_solver.h
[preprocessor_h]: ../glop/preprocessor.h
[revised_simplex_h]: ../glop/revised_simplex.h
[status_h]: ../glop/status.h
