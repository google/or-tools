# Constraint Programming (CP) and Routing Solver

This directory contains a Constraint Programming (CP) solver and a Vehicle Routing solver.

## Constraint Programming

[Constraint Programming](http://en.wikipedia.org/wiki/Constraint_programming) is a technology
issued from IA and used in Operations Research.

To begin, skim
* [constraint_solver.h](constraint_solver.h):
The point of entry for all constraint programming users.  
* [constraint_solveri.h](constraint_solveri.h):
An additional file that helps extending the constraint programming library.

### Parameters
* [solver_parameters.proto](solver_parameters.proto):
The CP solver parameters.
* [search_limit.proto](search_limit.proto):
Holds parameters to limit the search space within the CP solver, which is important for performance.

### Solution
* [assignment.proto](assignment.proto):
Holds the solution of a CP problem.
* [demon_profiler.proto](demon_profiler.proto):
Holds the timeline and execution profile of constraints and demons (daemons).

## Vehicle Routing

[Vehicle Routing](http://en.wikipedia.org/wiki/Vehicle_routing) is a useful
extension that is implemented on top of the CP library.

To begin, skim
* [routing.h](routing.h):
The point of entry for routing problems.

### Parameters
* [routing_parameters.proto](routing_parameters.proto):
The Vehicle Routing solver parameters.

### Recipes

You can find a set of code recipes in
[the documentation directory](doc/routing.md).
