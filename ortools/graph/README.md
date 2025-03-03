# Graph and Network Flows

This directory contains data structures and algorithms for graph and network
flow problems.

It contains in particular:

*   well-tuned algorithms (for example, shortest paths and
    [Hamiltonian paths](https://en.wikipedia.org/wiki/Hamiltonian_path)).
*   hard-to-find algorithms (Hamiltonian paths, push-relabel flow algorithms).
*   other, more common algorithms, that are useful to use with graphs from
    `util/graph`.

Generic algorithms for shortest paths:

*   [`bounded_dijkstra.h`][bounded_dijkstra_h]: entry point for shortest paths.
    This is the preferred implementation for most needs.

*   [`bidirectional_dijkstra.h`][bidirectional_dijkstra_h]: for large graphs,
    this implementation might be faster than `bounded_dijkstra.h`.

*   [`shortest_paths.h`][shortest_paths_h]: shortest paths that are computed in
    parallel (only if there are several sources). Includes
    [Dijkstra](https://en.wikipedia.org/wiki/Dijkstra%27s_algorithm) and
    [Bellman-Ford](https://en.wikipedia.org/wiki/Bellman%E2%80%93Ford_algorithm)
    algorithms. *Although its name is very generic, only use this implementation
    if parallelism makes sense.*

Specific algorithms for paths:

*   [`hamiltonian_path.h`][hamiltonian_path_h]: entry point for computing
    minimum [Hamiltonian paths](https://en.wikipedia.org/wiki/Hamiltonian_path)
    and cycles on directed graphs with costs on arcs, using a
    dynamic-programming algorithm.

*   [`eulerian_path.h`][eulerian_path_h]: entry point for computing minimum
    [Eulerian paths](https://en.wikipedia.org/wiki/Eulerian_path) and cycles on
    undirected graphs.

Graph decompositions:

* [`connected_components.h`][connected_components_h]: entry point for computing
  connected components in an undirected graph. (It does not need an explicit
  graph class.)

* [`strongly_connected_components.h`][strongly_connected_components_h]: entry
  point for computing the strongly connected components of a directed graph.

*   [`cliques.h`][cliques_h]: entry point for computing maximum cliques and
    clique covers in a directed graph, based on the Bron-Kerbosch algorithm.(It
    does not need an explicit graph class.)

Flow algorithms:

*   [`linear_assignment.h`][linear_assignment_h]: entry point for solving linear
    sum assignment problems (classical assignment problems where the total cost
    is the sum of the costs of each arc used) on directed graphs with arc costs,
    based on the Goldberg-Kennedy push-relabel algorithm.

*   [`max_flow.h`][max_flow_h]: entry point for computing maximum flows on
    directed graphs with arc capacities, based on the Goldberg-Tarjan
    push-relabel algorithm.

*   [`min_cost_flow.h`][min_cost_flow_h]: entry point for computing minimum-cost
    flows on directed graphs with arc capacities, arc costs, and
    supplies/demands at nodes, based on the Goldberg-Tarjan push-relabel
    algorithm.

## Wrappers

*   [`python`](python): the SWIG code that makes the wrapper available in Python
    and its unit tests.

*   [`java`](java): the SWIG code that makes the wrapper available in Java and
    its unit tests.

*   [`csharp`](csharp): the SWIG code that makes the wrapper available in C# and
    its unit tests.

## Samples

You can find some canonical examples in [`samples`][samples].

<!-- Links used throughout the document. -->

[graph_h]: ../graph/graph.h
[bounded_dijkstra_h]: ../graph/bounded_dijkstra.h
[bidirectional_dijkstra_h]: ../graph/bidirectional_dijkstra.h
[shortest_paths_h]: ../graph/shortest_paths.h
[dag_shortest_path_h]: ../graph/dag_shortest_path.h
[dag_constrained_shortest_path_h]: ../graph/dag_constrained_shortest_path.h
[hamiltonian_path_h]: ../graph/hamiltonian_path.h
[eulerian_path_h]: ../graph/eulerian_path.h
[connected_components_h]: ../graph/connected_components.h
[strongly_connected_components_h]: ../graph/strongly_connected_components.h
[cliques_h]: ../graph/cliques.h
[linear_assignment_h]: ../graph/linear_assignment.h
[max_flow_h]: ../graph/max_flow.h
[min_cost_flow_h]: ../graph/min_cost_flow.h
[samples]: ../graph/samples/
[graph]: ../graph/
