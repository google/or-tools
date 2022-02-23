# Overview

You can find here, the documentation for the two following Or-Tools components.

* [CP Solver](CP.md)

  Constraint programming (CP), is the name given to identifying feasible
  solutions out of a very large set of candidates, where the problem can be
  modeled in terms of arbitrary constraints.

  **note:** We **strongly recommend** using the [CP-SAT solver](../../sat)
  rather than the original CP solver.

* [Routing Solver](ROUTING.md)

  A specialized library for identifying best vehicle routes given constraints.

  This library extensiond is implemented on top of the CP Solver library.
