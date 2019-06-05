| [home](README.md) | [boolean logic](boolean_logic.md) | [integer arithmetic](integer_arithmetic.md) | [channeling constraints](channeling.md) | [scheduling](scheduling.md) | [Using the CP-SAT solver](solver.md) | [Model manipulation](model.md) | [Reference manual](reference.md) |
| ----------------- | --------------------------------- | ------------------------------------------- | --------------------------------------- | --------------------------- | ------------------------------------ | ------------------------------ | -------------------------------- |


# Model manipulation



## Introduction

In all languages, the CpModel class is a thin wrapper around a
[cp_model.proto](../cp_model.proto).

Some functionalities can require going back to the cp_model protobuf. To write
code that manipulate this protobuf, one must understand how modeling objects
(variables, constraints) are mapped to onto the protobuf.

The fundamental idea is that the cp_model protobuf object contains a list of
variables and a list of a constraints. Constraints are build with variables and
reference them using their indices in the list of variables.

In all languages, the `IntVar` class has a method to query their index in the
model list of variables:

-   **C++**: `var.index()`
-   **Python**: `var.Index()`
-   **Java**: `var.getIndex()`
-   **C#**: `var.Index` or `var.GetIndex()`

The implementation of Boolean literals differs across languages.

-   **C++**: The `BoolVar` class is a separate class from the `IntVar` class. A
    `BoolVar` object can implicitely be casted into a `IntVar` object. A
    `BoolVar` object has two important methods: `index()` and `Not()`. `Not()`
    returns another `BoolVar` with a different index: `b.Not().index() =
    -b.index() - 1`.
-   **Python**: There is no `BoolVar` class. Boolean variables are defined as an
    `IntVar` with a Boolean domain (0 or 1). The `Not()` method returns a
    different class. Both the `IntVar` class and the negation class implements
    `Index()` and `Not()`.
-   **Java**: There is no `BoolVar` class. Boolean variables are defined as an
    `IntVar` with a Boolean domain (0 or 1). Boolean variables and their
    negation implement the `Literal` interface. This interface defines the
    `getIndex()` and the `not()` method.
-   **C#**: Boolean variables are defined as an `IntVar` with a Boolean domain
    (0 or 1). Boolean variables and their negation implement the `ILiteral`
    interface. This interface defines the `GetIndex()` and the `Not()` method.

## Solution hinting

The `CpModelProto` message has a `solution_hint` field. This field is a
`PartialVariableAssignment` message that contains two parallel vectors (variable
indices and hint values).

Adding solution hinting to a model implies filling these two fields.

Some remarks:

-   A solution hint is not a hard constraint. The solver will try to use it in
    search for a while, then will gradually revert to its default search.
-   An infeasible solution hint is not an issue. It can be helpful in some
    repair procedure where one want to look for a solution close to the previous
    violated state.
-   Solution hinting does not need to use all variables. Partial solution
    hinting is perfectly valid.

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
