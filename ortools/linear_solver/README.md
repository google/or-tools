
# Linear Programming (LP) and Integer Programming (IP) Solver

Linear optimization problems are common throughout Google, and the Operations
Research team has a few ways to help with them.

These models have the form:
$$\begin{array}{lll}
(P) & \max & cx\\
    & s.t. & L\leq Ax\leq U\\
    &      & l\leq x\leq u\\
    &      &x_i\in\mathbb{Z}\quad\forall i\in I
\end{array}$$

Where $$A\in\mathbb{R}^{m\times n}$$, $$l,u,c\in\mathbb{R}^n$$,
$$L,U\in\mathbb{R}^m$$, $$n,m\in\mathbb{N}^+$$ and $$I\subseteq\{1,\ldots,n\}$$.

This module provides:

*   The `MPModelRequest` Proto API (in `linear_solver.proto`) for modeling an
    optimization problem.
*   The function `SolveMPModel()` (in `solve_mp_model.h`) for solving an
    optimization problem with a solver (Glop, Bop, Sat, SCIP, Gurobi etc.)
*   `ModelBuilder` (in `model_builder.h`) and similar classes in Python and Java
    to help construct an `MPModelRequest` (e.g. to provide linear expressions).
*   `MPSolver` which is no longer in development. `MPSolver` is largely
    interoperable with the `MPModelRequest` API, although the features supported
    are not identical.

To begin, skim

*   [linear_solver.proto](../linear_solver/linear_solver.proto):
    Specifically, look at `MPModelProto`. This gives a succinct description of
    what problems can be solved.

*   [solve_mp_model.h](../linear_solver/solve_mp_model.h):
    This file contains the key functions to run various solvers.

## Available solvers

Each *\*_interface.cc*  file corresponds to one of the solver accessible
through the wrapper.

* Google's BOP (boolean) solver: [bop_interface.cc](../linear_solver/bop_interface.cc)

* Google's GLOP (lp) solver: [glop_interface.cc](../linear_solver/glop_interface.cc)

* Gurobi (MIP) solver: [gurobi_interface.cc](../linear_solver/gurobi_interface.cc)

* Google's PLDP solver: [pdlp_interface.cc](../linear_solver/pdlp_interface.cc)

* SCIP (MIP) solver: [scip_interface.cc](../linear_solver/scip_interface.cc)

* Google's CP-SAT solver: [sat_interface.cc](../linear_solver/sat_interface.cc)

* Coin-OR Cbc (MIP) solver: [cbc_interface.cc](../linear_solver/cbc_interface.cc)

* Coin-OR Clp (LP) solver: [clp_interface.cc](../linear_solver/clp_interface.cc)

* CPLEX (MIP) solver: [cplex_interface.cc](../linear_solver/cplex_interface.cc)

* GLPK (MIP) solver: [glpk_interface.cc](../linear_solver/glpk_interface.cc)

* Xpress (MIP) solver: [xpress_interface.cc](../linear_solver/xpress_interface.cc)

## Wrappers

* [python](python): the SWIG code that makes the wrapper available in Python,
  and its unit tests.

* [java](java): the SWIG code that makes the wrapper available in Java,
  and its unit tests.

* [csharp](csharp): the SWIG code that makes the wrapper available in C#,
  and its unit tests.

## Samples

You can find some canonical examples in [samples](../linear_solver/samples/)
