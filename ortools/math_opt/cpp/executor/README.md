# Solve Executor

The **Solve Executor** library is an extension of the MathOpt C++ API designed
to make different execution modes interoperable without imposing compile-time
dependencies on specific modes.

## Overview

The API mirrors the standard MathOpt in-process solving API. Instead of directly
calling `Solve(Model, SolverType, SolveArguments, SolverInitArguments)`, you:
1. Create a `SolveExecutor`.
2. Call `SolveExecutor::Solve(Model, SolverType, SolveArguments, ExecutorSolverInitArguments)`.

This virtual interface allows different implementations to execute solves
differently.

## Implementations

The open-source version includes:

*   `LocalSolveExecutor`: Solves the model in-process using the standard `Solve()` function.

## Incremental Solving

You can also create an `ExecutorIncrementalSolver` (similar to
`IncrementalSolver`) for cases where you need to solve a model incrementally.
