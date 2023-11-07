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
