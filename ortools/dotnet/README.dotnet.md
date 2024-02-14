# Google OR-Tools

This is the .NET wrapper of OR-Tools.

## About OR-Tools

Google Optimization Tools (a.k.a. OR-Tools) is open source, fast and portable
software suite for solving *combinatorial optimization problems*, which seeks
to find the best solution to a problem out of a very large set of possible
solutions. Here are some examples of problems that OR-Tools solves:

* Vehicle routing: Find optimal routes for vehicle fleets that pick up and
  deliver packages given constraints (e.g., "this truck can't hold more than
  20,000 pounds" or "all deliveries must be made within a two-hour window").
* Scheduling: Find the optimal schedule for a complex set of tasks, some of
  which need to be performed before others, on a fixed set of machines, or
  other resources.
* Bin packing: Pack as many objects of various sizes as possible into a fixed
  number of bins with maximum capacities.

In most cases, problems like these have a vast number of possible solutions—too
many for a computer to search them all. To overcome this, OR-Tools uses
state-of-the-art algorithms to narrow down the search set, in order to find an
optimal (or close to optimal) solution.

OR-Tools includes solvers for:

### Constraint Programming

A set of techniques for finding feasible solutions to a problem expressed as
constraints (e.g., a room can't be used for two events simultaneously, or the
distance to the crops must be less than the length of the hose, or no more than
five TV shows can be recorded at once).

### Linear and Mixed-Integer Programming

The Glop linear optimizer finds the optimal value of a linear objective
function, given a set of linear inequalities as constraints (e.g., assigning
people to jobs, or finding the best allocation of a set of resources while
minimizing cost). The mixed-integer programming software SCIP is also available.

### Vehicle Routing

A specialized library for identifying best vehicle routes given constraints.

### Graph Algorithms

Code for finding shortest paths in graphs, min-cost flows, max flows, and linear sum assignments.

## Roadmap

See the [open issues](https://github.com/google/or-tools/issues) for a list of
proposed features and known issues.

## Contributing

The [CONTRIBUTING.md](https://github.com/google/or-tools/blob/main/CONTRIBUTING.md)
file contains instructions on how to submit the Contributor License Agreement
before sending any pull requests (PRs).

Of course, if you're new to the project, it's usually best to discuss any
proposals and reach consensus before sending your first PR.

## License

The OR-Tools software suite is licensed under the terms of the Apache License 2.0.

