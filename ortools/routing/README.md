# Routing

[Vehicle Routing](http://en.wikipedia.org/wiki/Vehicle_routing) is a useful
extension that is implemented on top of the CP solver library.

## Routing solver

To begin, skim:

*   [routing.h](../routing/routing.h): The
    vehicle routing library lets one model and solve generic vehicle routing
    problems ranging from the Traveling Salesman Problem to more complex
    problems such as the Capacitated Vehicle Routing Problem with Time Windows.

### Parameters

*   [parameters.proto](../routing/parameters.proto):
    The Vehicle Routing solver parameters.
*   [enums.proto](../routing/enums.proto):
    Enums used to define routing parameters.

### Solution

*   [assignment.proto](../constraint_solver/assignment.proto):
    Holds the solution of a Routing problem (as a special case of a CS problem).

## Parsers

Utilities for file formats are in the
[`parsers`](../routing/parsers) subfolder.
