# C++ examples
The following examples showcase how to use the different Operations Research
libraries.

## Examples list
- Constraint Solver examples:
  - cryptarithm.cc Demonstrates the use of basic modeling objects
    (integer variables, arithmetic constraints and expressions,
    simple search).
  - golomb.cc Demonstrates how to handle objective functions and collect
    solutions found during the search.
  - magic_square.cc Shows how to use the automatic search to solve your
    problem.
  - costas_array.cc Solves the problem of Costas Array (a constrained
    assignment problem used for radars) with two version. On version is
    a feasibility version with hard constraints, the other version is
    an optimization version with soft constraints and violation costs.
  - jobshop.cc Demonstrates scheduling of jobs on different machines.
  - jobshop_ls.cc Demonstrates scheduling of jobs on different machines with
    a search using Local Search and Large Neighorhood Search.
  - nqueens.cc Solves the n-queen problem. It also demonstrates how to break
    symmetries during search.
  - network_routing.cc Solves a multicommodity mono-routing
    problem with capacity constraints and a max usage cost structure.
  - sports_scheduling.cc Finds a soccer championship schedule. Its uses an
    original approach where all constraints attached to either one team,
    or one week are regrouped into one global 'AllowedAssignment' constraints.
  - dobble_ls.cc Shows how to write Local Search operators and Local Search
    filters in a context of an assignment/partitioning problem. It also
    shows how to write a simple constraint.

- Routing examples:
  - tsp.cc    Travelling Salesman Problem.
  - cvrptw.cc Capacitated Vehicle Routing Problem with Time Windows.
  - pdptw.cc  Pickup and Delivery Problem with Time Windows.

- Graph examples:
  - flow_api.cc Demonstrates how to use Min-Cost Flow and Max-Flow api.
  - linear_assignment_api.cc Demonstrates how to use the Linear Sum
    Assignment solver.
  - dimacs_assignment.cc Solves DIMACS challenge on assignment
    problems.

- Linear and integer programming examples:
  - linear_programming.cc Demonstrates how to use the linear solver
    wrapper API to solve Linear Programming problems.
  - integer_programming.cc Demonstrates how to use the linear solver
    wrapper API to solve Integer Programming problems.
  - linear_solver_protocol_buffers.cc Demonstrates how protocol
    buffers can be used as input and output to the linear solver wrapper.
  - strawberry_fields_with_column_generation.cc Complex example that
    demonstrates how to use dynamic column generation to solve a 2D
    covering problem.

- Utilities
  - model_util.cc A utility to manipulate model files (.cp) dumped by the
    solver.

## Execution
Running the examples will involve building them, then running them.<br>
You can run the following command from the **top** directory:
```shell
make build SOURCE=examples/cpp/<example>.cc
make run SOURCE=examples/cpp/<example>.cc
```
