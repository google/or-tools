# Constraint Programming (CP) and Routing Solver

This directory contains a Constraint Programming (CP) solver and a Vehicle
Routing solver.

## CP solver

[Constraint Programming](http://en.wikipedia.org/wiki/Constraint_programming) is
a technology issued from AI and used in operations research.

To begin, skim:

*   [constraint_solver.h](../constraint_solver/constraint_solver.h):
    Declaration of the core objects for the constraint solver.

### Parameters

* [solver_parameters.proto](../constraint_solver/solver_parameters.proto):
This file contains protocol buffers for all parameters of the CP solver.
* [search_limit.proto](../constraint_solver/search_limit.proto):
Holds parameters to limit the search space within the CP solver, which is
important for performance.

### Solution

*   [assignment.proto](../constraint_solver/assignment.proto):
    Holds the solution of a CP problem.
*   [demon_profiler.proto](../constraint_solver/demon_profiler.proto):
    Holds the timeline and execution profile of constraints and demons
    (daemons).

## Routing solver

[Vehicle Routing](http://en.wikipedia.org/wiki/Vehicle_routing) is a useful
extension that is implemented on top of the CP solver library. It is now
available as [a separate module](../routing/README.md).

## Recipes

You can find a set of code recipes in
[the documentation directory](docs/README.md).
