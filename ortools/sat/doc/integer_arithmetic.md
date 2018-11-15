# Integer arithmetic recipes for the CP-SAT solver.



## Introduction

The CP-SAT solver can express integer variables and constraints.

## Integer variables

Integer variables are discrete variables ranging over 64 bit signed integer
values. When creating them, a domain must be given. The format of this domain is
a flattened list of disjoint intervals.

-   To represent a interval from 0 to 10, just pass a domain [0, 10].
-   To represent a single value (5), create a domain [5, 5].
-   From these, it is easy to represent an enumerated list of values [-5, -4,
    -3, 1, 3, 4, 5, 6] is encoded as [-5, -3, 1, 1, 3, 6].
-   To exclude a single value, use int64min and int64max values as in [int64min,
    4, 6, int64max].

## Linear constraints

In **C++** and **Java**, the model supports linear constraints as in:

    x <= y + 3 (also ==, !=, <, >=, >).

as well as domain constraints as in:

sum(ai * xi) in domain

Where domain uses the same encoding as integer variables.

**Python** and **C\#** CP-SAT APIs support general linear arithmetic (+, *, -,
==, >=, >, <, <=, !=).

## Rabbits and Pheasants examples

Let's solve a simple children's puzzle: the Rabbits and Pheasants problem.

WIn a field of rabbits and pheasants, there are 20 heads and 56 legs. How many
rabbits and pheasants are there?

### Python code

```python
"""Rabbits and Pheasants quizz."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


def RabbitsAndPheasantsSat():
  """Solves the rabbits + pheasants problem."""
  model = cp_model.CpModel()

  r = model.NewIntVar(0, 100, 'r')
  p = model.NewIntVar(0, 100, 'p')

  # 20 heads.
  model.Add(r + p == 20)
  # 56 legs.
  model.Add(4 * r + 2 * p == 56)

  # Solves and prints out the solution.
  solver = cp_model.CpSolver()
  status = solver.Solve(model)

  if status == cp_model.FEASIBLE:
    print('%i rabbits and %i pheasants' % (solver.Value(r), solver.Value(p)))


RabbitsAndPheasantsSat()
```

### C++ code

```cpp
#include "ortools/sat/cp_model.h"

namespace operations_research {
namespace sat {

void RabbitsAndPheasantsSat() {
  CpModelBuilder cp_model;

  const Domain all_animals(0, 20);
  const IntVar rabbits = cp_model.NewIntVar(all_animals).WithName("rabbits");
  const IntVar pheasants =
      cp_model.NewIntVar(all_animals).WithName("pheasants");

  cp_model.AddEquality(LinearExpr::Sum({rabbits, pheasants}), 20);
  cp_model.AddEquality(LinearExpr::ScalProd({rabbits, pheasants}, {4, 2}), 56);

  const CpSolverResponse response = Solve(cp_model);

  if (response.status() == CpSolverStatus::FEASIBLE) {
    // Get the value of x in the solution.
    LOG(INFO) << SolutionIntegerValue(response, rabbits) << " rabbits, and "
              << SolutionIntegerValue(response, pheasants) << " pheasants";
  }
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::RabbitsAndPheasantsSat();

  return EXIT_SUCCESS;
}
```

### Java code

```java
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.IntVar;

/**
 * In a field of rabbits and pheasants, there are 20 heads and 56 legs. How many rabbits and
 * pheasants are there?
 */
public class RabbitsAndPheasantsSat {

  static { System.loadLibrary("jniortools"); }

  public static void main(String[] args) throws Exception {
    // Creates the model.
    CpModel model = new CpModel();
    // Creates the variables.
    IntVar r = model.newIntVar(0, 100, "r");
    IntVar p = model.newIntVar(0, 100, "p");
    // 20 heads.
    model.addLinearSumEqual(new IntVar[] {r, p}, 20);
    // 56 legs.
    model.addScalProdEqual(new IntVar[] {r, p}, new long[] {4, 2}, 56);

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.solve(model);

    if (status == CpSolverStatus.FEASIBLE) {
      System.out.println(solver.value(r) + " rabbits, and " + solver.value(p) + " pheasants");
    }
  }
}
```

### C\# code

```cs
using System;
using Google.OrTools.Sat;

public class RabbitsAndPheasantsSat
{
  static void Main()
  {
    // Creates the model.
    CpModel model = new CpModel();
    // Creates the variables.
    IntVar r = model.NewIntVar(0, 100, "r");
    IntVar p = model.NewIntVar(0, 100, "p");
    // 20 heads.
    model.Add(r + p == 20);
    // 56 legs.
    model.Add(4 * r + 2 * p == 56);

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.Solve(model);

    if (status == CpSolverStatus.Feasible)
    {
      Console.WriteLine(solver.Value(r) + " rabbits, and " +
                        solver.Value(p) + " pheasants");
    }
  }
}
```
