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

This module provides an unified wrapper (MPSolver) around different linear and
integer solvers (Glop, Bop, Sat, SCIP, Gurobi etc.).


To begin, skim

*   [linear_solver.h](../linear_solver/linear_solver.h):
    the point of entry for the MPSolver wrapper that provides a simple and
    unified interface to several linear programming and mixed integer
    programming solvers.

*   [linear_solver.cc](../linear_solver/linear_solver.cc):
    the C++ code of the MPSolver wrapper that is common to all solvers
    accessible through the wrapper.

## Parameters and Solution

`MPSolver` uses a proto interface `MPModelProto`.

You can find the protocol buffer definition here:

*   [linear_solver.proto](../linear_solver/linear_solver.proto):
    MPSolver parameters, model and solution messages.

## Available solvers

Each *\*_interface.cc*  file corresponds to one of the solver accessible
through the wrapper.

* Google's BOP (boolean) solver: [bop_interface.cc]
(../linear_solver/bop_interface.cc)

* Google's GLOP (lp) solver: [glop_interface.cc]
(../linear_solver/glop_interface.cc)

* Gurobi (MIP) solver: [gurobi_interface.cc]
(../linear_solver/gurobi_interface.cc)

* SCIP (MIP) solver: [scip_interface.cc]
(../linear_solver/scip_interface.cc)

* Google's CP-SAT Solver: [sat_interface.cc]
(../linear_solver/sat_interface.cc)


* Coin-OR Cbc (MIP) solver: [cbc_interface.cc]
(../linear_solver/cbc_interface.cc)

* Coin-OR Clp (LP) solver: [clp_interface.cc]
(../linear_solver/clp_interface.cc)

## Wrappers

* [python](python): the SWIG code that makes the wrapper available in Python,
  and its unit tests.

* [java](java): the SWIG code that makes the wrapper available in Java,
  and its unit tests.

* [csharp](csharp): the SWIG code that makes the wrapper available in C#,
  and its unit tests.

## Samples

You can find some canonical examples in [samples](../linear_solver/samples/)

