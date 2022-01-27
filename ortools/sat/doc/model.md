| [home](README.md) | [boolean logic](boolean_logic.md) | [integer arithmetic](integer_arithmetic.md) | [channeling constraints](channeling.md) | [scheduling](scheduling.md) | [Using the CP-SAT solver](solver.md) | [Model manipulation](model.md) | [Python API](https://google.github.io/or-tools/python/ortools/sat/python/cp_model.html) |
| ----------------- | --------------------------------- | ------------------------------------------- | --------------------------------------- | --------------------------- | ------------------------------------ | ------------------------------ | -------------------------------- |

# Model manipulation

https://developers.google.com/optimization/

<!--ts-->
   * [Model manipulation](#model-manipulation)
      * [Introduction](#introduction)
      * [Solution hinting](#solution-hinting)
         * [Python code](#python-code)
         * [C   code](#c-code)
         * [Java code](#java-code)
         * [C# code](#c-code-1)
      * [Model copy](#model-copy)
         * [Python code](#python-code-1)
         * [C   code](#c-code-2)


<!--te-->


## Introduction

In all languages, the CpModel class is a thin wrapper around a
[protocol buffer](http://developers.google.com/protocol-buffers/) object
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

Adding solution hinting to a model implies filling these two fields. This is
done by the `addHint` or `AddHint` methods.

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
  model.AddHint(x, 1)
  model.AddHint(y, 2)

  # Creates a solver and solves.
  solver = cp_model.CpSolver()
  solution_printer = cp_model.VarArrayAndObjectiveSolutionPrinter([x, y, z])
  status = solver.Solve(model, solution_printer)

  print('Status = %s' % solver.StatusName(status))
  print('Number of solutions found: %i' % solution_printer.solution_count())


SolutionHintingSampleSat()
```

### C++ code

```cpp
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

void SolutionHintingSampleSat() {
  CpModelBuilder cp_model;

  const Domain domain(0, 2);
  const IntVar x = cp_model.NewIntVar(domain).WithName("x");
  const IntVar y = cp_model.NewIntVar(domain).WithName("y");
  const IntVar z = cp_model.NewIntVar(domain).WithName("z");

  cp_model.AddNotEqual(x, y);

  cp_model.Maximize(x + 2 * y + 3 * z);

  // Solution hinting: x <- 1, y <- 2
  cp_model.AddHint(x, 1);
  cp_model.AddHint(y, 2);

  Model model;

  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    LOG(INFO) << "Solution " << num_solutions;
    LOG(INFO) << "  x = " << SolutionIntegerValue(r, x);
    LOG(INFO) << "  y = " << SolutionIntegerValue(r, y);
    LOG(INFO) << "  z = " << SolutionIntegerValue(r, z);
    num_solutions++;
  }));

  // Solving part.
  const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);
  LOG(INFO) << CpSolverResponseStats(response);
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::SolutionHintingSampleSat();

  return EXIT_SUCCESS;
}
```

### Java code

```java
package com.google.ortools.sat.samples;

import com.google.ortools.Loader;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverSolutionCallback;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.LinearExpr;

/** Minimal CP-SAT example to showcase calling the solver. */
public class SolutionHintingSampleSat {
  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    // Create the model.
    CpModel model = new CpModel();

    // Create the variables.
    int numVals = 3;

    IntVar x = model.newIntVar(0, numVals - 1, "x");
    IntVar y = model.newIntVar(0, numVals - 1, "y");
    IntVar z = model.newIntVar(0, numVals - 1, "z");

    // Create the constraints.
    model.addDifferent(x, y);

    model.maximize(LinearExpr.scalProd(new IntVar[] {x, y, z}, new int[] {1, 2, 3}));

    // Solution hinting: x <- 1, y <- 2
    model.addHint(x, 1);
    model.addHint(y, 2);

    // Create a solver and solve the model.
    CpSolver solver = new CpSolver();
    VarArraySolutionPrinterWithObjective cb =
        new VarArraySolutionPrinterWithObjective(new IntVar[] {x, y, z});
    solver.solve(model, cb);
  }

  static class VarArraySolutionPrinterWithObjective extends CpSolverSolutionCallback {
    public VarArraySolutionPrinterWithObjective(IntVar[] variables) {
      variableArray = variables;
    }

    @Override
    public void onSolutionCallback() {
      System.out.printf("Solution #%d: time = %.02f s%n", solutionCount, wallTime());
      System.out.printf("  objective value = %f%n", objectiveValue());
      for (IntVar v : variableArray) {
        System.out.printf("  %s = %d%n", v.getName(), value(v));
      }
      solutionCount++;
    }

    private int solutionCount;
    private final IntVar[] variableArray;
  }
}
```

### C\# code

```cs
using System;
using Google.OrTools.Sat;

public class VarArraySolutionPrinter : CpSolverSolutionCallback
{
    public VarArraySolutionPrinter(IntVar[] variables)
    {
        variables_ = variables;
    }

    public override void OnSolutionCallback()
    {
        {
            Console.WriteLine(String.Format("Solution #{0}: time = {1:F2} s", solution_count_, WallTime()));
            foreach (IntVar v in variables_)
            {
                Console.WriteLine(String.Format("  {0} = {1}", v.ShortString(), Value(v)));
            }
            solution_count_++;
        }
    }

    public int SolutionCount()
    {
        return solution_count_;
    }

    private int solution_count_;
    private IntVar[] variables_;
}

public class SolutionHintingSampleSat
{
    static void Main()
    {
        // Creates the model.
        CpModel model = new CpModel();

        // Creates the variables.
        int num_vals = 3;

        IntVar x = model.NewIntVar(0, num_vals - 1, "x");
        IntVar y = model.NewIntVar(0, num_vals - 1, "y");
        IntVar z = model.NewIntVar(0, num_vals - 1, "z");

        // Creates the constraints.
        model.Add(x != y);

        // Solution hinting: x <- 1, y <- 2
        model.AddHint(x, 1);
        model.AddHint(y, 2);

        model.Maximize(LinearExpr.ScalProd(new IntVar[] { x, y, z }, new int[] { 1, 2, 3 }));

        // Creates a solver and solves the model.
        CpSolver solver = new CpSolver();
        VarArraySolutionPrinter cb = new VarArraySolutionPrinter(new IntVar[] { x, y, z });
        CpSolverStatus status = solver.Solve(model, cb);
    }
}
```

## Model copy

The CpModel classes supports deep copy from a previous model. This is useful to
solve variations of a base model. The trick is to recover the copies of the
variables in the original model to be able to manipulate the new model. This is
illustrated in the following examples.

### Python code

```python
"""Showcases deep copying of a model."""

from ortools.sat.python import cp_model


def CopyModelSat():
  """Showcases printing intermediate solutions found during search."""
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

  # Creates a solver and solves.
  solver = cp_model.CpSolver()
  status = solver.Solve(model)

  if status == cp_model.OPTIMAL:
    print('Optimal value of the original model: {}'.format(
        solver.ObjectiveValue()))

  # Copy the model.
  copy = cp_model.CpModel()
  copy.CopyFrom(model)

  copy_x = copy.GetIntVarFromProtoIndex(x.Index())
  copy_y = copy.GetIntVarFromProtoIndex(y.Index())

  copy.Add(copy_x + copy_y <= 1)
  status = solver.Solve(copy)

  if status == cp_model.OPTIMAL:
    print('Optimal value of the modified model: {}'.format(
        solver.ObjectiveValue()))


CopyModelSat()
```

### C++ code

```cpp
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

void CopyModelSat() {
  CpModelBuilder cp_model;

  const Domain domain(0, 2);
  const IntVar x = cp_model.NewIntVar(domain).WithName("x");
  const IntVar y = cp_model.NewIntVar(domain).WithName("y");
  const IntVar z = cp_model.NewIntVar(domain).WithName("z");

  cp_model.AddNotEqual(x, y);

  cp_model.Maximize(x + 2 * y + 3 * z);

  const CpSolverResponse initial_response = Solve(cp_model.Build());
  LOG(INFO) << "Optimal value of the original model: "
            << initial_response.objective_value();

  CpModelBuilder copy;
  copy.CopyFrom(cp_model.Proto());

  // Add new constraint: copy_of_x + copy_of_y == 1.
  IntVar copy_of_x = copy.GetIntVarFromProtoIndex(x.index());
  IntVar copy_of_y = copy.GetIntVarFromProtoIndex(y.index());

  copy.AddLessOrEqual(copy_of_x + copy_of_y, 1);

  const CpSolverResponse modified_response = Solve(copy.Build());
  LOG(INFO) << "Optimal value of the modified model: "
            << modified_response.objective_value();
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::CopyModelSat();

  return EXIT_SUCCESS;
}
```
