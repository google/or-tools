# Primal-Dual Hybrid Gradient Solver (PDLP)

This directory contains PDLP, a library for solving linear programming (LP) and
quadratic programming (QP) problems using first-order methods.

The implementation is based on the Primal-Dual Hybrid Gradient (PDHG) algorithm,
which is preprocessed with scaling and optional presolving to improve
performance and numerical stability. See also
[Mathematical background for PDLP][background].

## Core C++ libraries:

* [`primal_dual_hybrid_gradient.h`][primal_dual_hybrid_gradient_h]: The main
  entry point `PrimalDualHybridGradient()` for the solver, which takes a
  `QuadraticProgram` and solver parameters.
* [`quadratic_program.h`][quadratic_program_h]: Defines the `QuadraticProgram`
  struct to represent the optimization problem, including objective vectors,
  constraint matrices, and bounds.
* [`quadratic_program_io.h`][quadratic_program_io_h]: Provides utilities to read
  quadratic programs from various file formats, including MPS and MPModelProto.
* [`sharded_quadratic_program.h`][sharded_quadratic_program_h] and
  [`sharder.h`][sharder_h]: These provide the infrastructure for sharding
  problem data and performing parallel computations.
* [`scheduler.h`][scheduler_h]: A thread scheduling interface that supports
  multiple backends (e.g. Eigen's thread pools).
* [`iteration_stats.h`][iteration_stats_h] and
  [`termination.h`][termination_h]: Contain logic for computing convergence and
  infeasibility statistics and checking termination criteria.

## Configuration and Logging

* [`solvers.proto`][solvers_proto]: Defines the `PrimalDualHybridGradientParams`
  message for configuring the solver, including termination criteria,
  algorithmic choices like restart strategies, and linesearch rules.
* [`solve_log.proto`][solve_log_proto]: Defines messages for logging the
  solver's progress and final result, such as `IterationStats` and `SolveLog`.

## Wrappers and Samples

* [`python/`](python): Contains the `pybind11` wrapper to expose the C++ library
  to Python, along with its build definitions and tests.
* [`samples/`](samples): This directory provides example usage of the library.

<!-- Links used throughout the document. -->
[background]: https://developers.google.com/optimization/lp/pdlp_math
[primal_dual_hybrid_gradient_h]: ../pdlp/primal_dual_hybrid_gradient.h
[quadratic_program_h]: ../pdlp/quadratic_program.h
[quadratic_program_io_h]: ../pdlp/quadratic_program_io.h
[sharded_quadratic_program_h]: ../pdlp/sharded_quadratic_program.h
[sharder_h]: ../pdlp/sharder.h
[scheduler_h]: ../pdlp/scheduler.h
[iteration_stats_h]: ../pdlp/iteration_stats.h
[termination_h]: ../pdlp/termination.h
[solvers_proto]: ../pdlp/solvers.proto
[solve_log_proto]: ../pdlp/solve_log.proto
