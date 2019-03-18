# Constraint Programming (CP) and Routing Solver

This directory contains a Constraint Programming (CP) solver and a Vehicle
Routing solver.

## CP solver

[Constraint Programming](http://en.wikipedia.org/wiki/Constraint_programming) is
a technology issued from IA and used in Operations Research.

To begin, skim

* [constraint_solver.h](../constraint_solver/constraint_solver.h):
Declaration of the core objects for the constraint solver.
* [constraint_solveri.h](../constraint_solver/constraint_solveri.h):
Collection of objects used to extend the Constraint Solver library.

### Parameters

* [solver_parameters.proto](../constraint_solver/solver_parameters.proto):
This file contains protocol buffers for all parameters of the CP solver.
* [search_limit.proto](../constraint_solver/search_limit.proto):
Holds parameters to limit the search space within the CP solver, which is
important for performance.

### Solution

* [assignment.proto](../constraint_solver/assignment.proto):
Holds the solution of a CP problem.
* [demon_profiler.proto](../constraint_solver/demon_profiler.proto):
Holds the timeline and execution profile of constraints and demons (daemons).

## Routing solver

[Vehicle Routing](http://en.wikipedia.org/wiki/Vehicle_routing) is a useful
extension that is implemented on top of the CP solver library.

To begin, skim

* [routing.h](../constraint_solver/routing.h):
The vehicle routing library lets one model and solve generic vehicle routing
problems ranging from the Traveling Salesman Problem to more complex problems
such as the Capacitated Vehicle Routing Problem with Time Windows.
* [routing_flags.h](../constraint_solver/routing_flags.h)

### Parameters

* [routing_parameters.proto](../constraint_solver/routing_parameters.proto):
The Vehicle Routing solver parameters.
* [routing_enums.proto](../constraint_solver/routing_enums.proto):
Enums used to define routing parameters.

### Solution

* [assignment.proto](assignment.proto):
Holds the solution of a Routing problem.

## Recipes

You can find a set of code recipes in
[the documentation directory](doc/README.md).
