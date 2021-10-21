# Graph and Network Flows

This directory contains data structures and algorithms for graph and
network flow problems.

It contains in particular:
* a compact and efficient graph representation
  ([EbertGraph](https://dl.acm.org/doi/abs/10.1145/214762.214769)),
  which is used for most of the algorithms herein, unless specified otherwise.
* well-tuned algorithms (for example shortest paths and
  [Hamiltonian paths](https://en.wikipedia.org/wiki/Hamiltonian_path).)
* hard-to-find algorithms (Hamiltonian paths, push-relabel flow algorithms.)
* other, more common algorithm, that are useful to use with EbertGraph.

Graph representations:
* [ebert_graph.h](./ebert_graph.h): Entry point for a directed graph class.

* [digraph.h](./digraph.h): Entry point for a directed graph class.
  To be deprecated by `ebert_graph.h`.

Paths:
* [shortestpaths.h](./shortestpaths.h): Entry point for shortest path computations.
  Includes [Dijkstra](https://en.wikipedia.org/wiki/Dijkstra%27s_algorithm) and
  [Bellman-Ford](https://en.wikipedia.org/wiki/Bellman%E2%80%93Ford_algorithm) algorithms.

* [hamiltonian_path.h](./hamiltonian_path.h): Entry point for computing minimum
  [Hamiltonian paths](https://en.wikipedia.org/wiki/Hamiltonian_path) and
  cycles on directed graphs with costs on arcs, using a dynamic-programming
  algorithm (Does not need `ebert_graph.h` or `digraph.h`.)

Graph decompositions:
* [connected_components.h](./connected_components.h): Entry point for computing connected
  components in an undirected graph. (Does not need `ebert_graph.h` or `digraph.h`.)

* [strongly_connected_components.h](./strongly_connected_components.h): Entry point for 
  computing the strongly connected components of a directed graph, based on an algorithm of Tarjan.

* [cliques.h](./cliques.h): Entry point for computing maximum cliques and clique covers in a directed graph,
  based on the Bron-Kerbosch algorithm. (Does not need `ebert_graph.h` or `digraph.h`.)

Flow algorithms:
* [linear_assignment.h](./linear_assignment.h): Entry point for solving linear sum assignment problems
  (classical assignment problems where the total cost is the sum of the costs
  of each arc used) on directed graphs with arc costs, based on a push-relabel
  algorithm of Goldberg and Kennedy.

* [max_flow.h](./max_flow.h): Entry point for computing maximum flows on directed graphs with
  arc capacities, based on a push-relabel algorithm of Goldberg and Tarjan.

* [min_cost_flow.h](./min_cost_flow.h): Entry point for computing minimum-cost flows on directed
  graphs with arc capacities, arc costs, and supplies/demands at nodes, based on
  a push-relabel algorithm of Goldberg and Tarjan.

## Wrappers

* [python](python): the SWIG code that makes the wrapper available in Python,
  and its unit tests.

* [java](java): the SWIG code that makes the wrapper available in Java,
  and its unit tests.

* [csharp](csharp): the SWIG code that makes the wrapper available in C#,
  and its unit tests.

## Samples

You can find some canonical examples in [samples](./samples)
