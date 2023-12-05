# Packing

This folder contains utilities and solvers for several packing problems:
knapsack and bin packing, including generalizations. The solvers take as input a
problem description in ProtoBuf.

## Knapsack

A typical knapsack problem considers a single container (the knapsack) and many
items. The container has a maximum capacity, items have both a weight and a
value. The goal is to find the subset of items that respects the capacity
constraint and has the maximum value. There can be only one container. More
generally, the knapsack problem models resource allocation.

More information on [Wikipedia](https://en.wikipedia.org/wiki/Knapsack_problem).

Multi-dimensional knapsack solver:

* [`algorithms/knapsack_solver.h`](../algorithms/knapsack_solver.h)

## Bin packing

In an instance of the bin-packing problem, the goal is to use the minimum amount
of bins to fit a series of items of varying size. Vector bin packing (VBP)
considers several dimensions for item size and bin capacity.

More information on
[Wikipedia](https://en.wikipedia.org/wiki/Bin_packing_problem).

Vector bin packing:

*   Problem description: [`vector_bin_packing.proto`](vector_bin_packing.proto)
*   Solver: [`arc_flow_solver.h`](arc_flow_solver.h)
*   VBP parser: [`vector_bin_packing_parser.h`](vector_bin_packing_parser.h)

2D bin packing:

*   Problem description:
    [`multiple_dimensions_bin_packing.proto`](multiple_dimensions_bin_packing.proto)
