# math_opt

The code in this directory provides a generic way of accessing mathematical
optimization solvers (sometimes called mathematical programming solvers), such
as GLOP, CP-SAT, SCIP and Gurobi. In particular, a single API is provided to
make these solvers largely interoperable.

New code should prefer MathOpt to `MPSolver`, as defined in
[linear_solver.h](../linear_solver/linear_solver.h)
when possible.

MathOpt has client libraries in C++, Python, and Java that most users should use
to build and solve their models. A proto API is also provided, but this is not
recommended for most users.

See
[parameters.proto](../math_opt/parameters.proto?q=SolverTypeProto)
for the list of supported solvers.
