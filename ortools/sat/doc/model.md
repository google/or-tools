| [home](README.md) | [boolean logic](boolean_logic.md) | [integer arithmetic](integer_arithmetic.md) | [channeling constraints](channeling.md) | [scheduling](scheduling.md) | [Using the CP-SAT solver](solver.md) | [Model manipulation](model.md) | [Reference manual](reference.md) |
| ----------------- | --------------------------------- | ------------------------------------------- | --------------------------------------- | --------------------------- | ------------------------------------ | ------------------------------ | -------------------------------- |

# Model manipulation


<!--ts-->
   * [Model manipulation](#model-manipulation)
      * [Introduction](#introduction)
      * [Solution hinting](#solution-hinting)
         * [Python code](#python-code)

<!-- Added by: lperron, at: Fri Jun  7 09:59:01 CEST 2019 -->

<!--te-->


## Introduction

In all languages, the CpModel class is a thin wrapper around a
[cp_model.proto](../cp_model.proto).

Some functionalities require using the cp_model protobuf directly. To write code
that manipulates this protobuf, one must understand how modeling objects
(variables, constraints) are mapped onto the protobuf.

The fundamental idea is that the cp_model protobuf object contains a list of
variables and a list of constraints. Constraints are built with variables and
reference them using their indices in the list of variables.

In all languages, the `IntVar` class has a method to query their index in the
model list of variables:

-   **C++**: `var.index()`
-   **Python**: `var.Index()`
-   **Java**: `var.getIndex()`
-   **C#**: `var.Index` or `var.GetIndex()`

The implementation of Boolean literals differs across languages.

-   **C++**: The `BoolVar` class is a separate class from the `IntVar` class. A
    `BoolVar` object can implicitly be cast to an `IntVar` object. A `BoolVar`
    object has two important methods: `index()` and `Not()`. `Not()` returns
    another `BoolVar` with a different index: `b.Not().index() = -b.index() -
    1`.
-   **Python**: There is no `BoolVar` class. A Boolean variable is defined as an
    `IntVar` with a Boolean domain (0 or 1). The `Not()` method returns a
    different class. Both the `IntVar` class and the negation class implement
    `Index()` and `Not()`.
-   **Java**: There is no `BoolVar` class. A Boolean variable is defined as an
    `IntVar` with a Boolean domain (0 or 1). Boolean variables and their
    negation implement the `Literal` interface. This interface defines
    `getIndex()` and `not()` methods.
-   **C#**: Boolean variables are defined as an `IntVar` with a Boolean domain
    (0 or 1). Boolean variables and their negations implement the `ILiteral`
    interface. This interface defines `GetIndex()` and `Not()` methods.

## Solution hinting

A solution is a partial assignment of variables to values that the search will
try to stick to. It is meant to guide the search with external knowledge towards
good solutions.

The `CpModelProto` message has a `solution_hint` field. This field is a
`PartialVariableAssignment` message that contains two parallel vectors (variable
indices and hint values).

Adding solution hinting to a model implies filling these two fields.

Some remarks:

-   A solution hint is not a hard constraint. The solver will try to use it in
    search for a while, and then gradually revert to its default search
    technique.
-   It's OK to have an infeasible solution hint. It can still be helpful in some
    repair procedures, where one wants to identify a solution close to a
    previously violated state.
-   Solution hints don't need to use all variables: partial solution hints are
    fine.

### Python code

```python
"""Code sample that solves a model using solution hinting."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


def SolutionHintingSampleSat():
  """Showcases solution hinting."""
  # Creates the model.
  model = cp_model.CpModel()

  # Creates the variables.
  num_vals = 3
  x = model.NewIntVar(0, num_vals - 1, 'x')
  y = model.NewIntVar(0, num_vals - 1, 'y')
  z = model.NewIntVar(0, num_vals - 1, 'z')

  # Creates the constraints.
  model.Add(x != y)

  model.Maximize(x + 2 * y + 3 * z)

  # Solution hinting: x <- 1, y <- 2
  model.Proto().solution_hint.vars.append(x.Index())
  model.Proto().solution_hint.values.append(1)
  model.Proto().solution_hint.vars.append(y.Index())
  model.Proto().solution_hint.values.append(2)

  # Creates a solver and solves.
  solver = cp_model.CpSolver()
  solution_printer = cp_model.VarArrayAndObjectiveSolutionPrinter([x, y, z])
  status = solver.SolveWithSolutionCallback(model, solution_printer)

  print('Status = %s' % solver.StatusName(status))
  print('Number of solutions found: %i' % solution_printer.solution_count())


SolutionHintingSampleSat()
```
