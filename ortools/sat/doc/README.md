| [home](README.md) | [boolean logic](boolean_logic.md) | [integer arithmetic](integer_arithmetic.md) | [channeling constraints](channeling.md) | [scheduling](scheduling.md) | [Using the CP-SAT solver](solver.md) | [Model manipulation](model.md) | [Reference manual](reference.md) |
| ----------------- | --------------------------------- | ------------------------------------------- | --------------------------------------- | --------------------------- | ------------------------------------ | ------------------------------ | -------------------------------- |

# Using the CP-SAT solver



<!--ts-->
   * [Using the CP-SAT solver](#using-the-cp-sat-solver)
      * [Documentation structure](#documentation-structure)
      * [Searching for one (optimal) solution of a CP-SAT model](#searching-for-one-optimal-solution-of-a-cp-sat-model)
         * [Python code samples](#python-code-samples)
         * [C   code samples](#c-code-samples)
         * [Java code samples](#java-code-samples)
         * [C# code samples](#c-code-samples-1)

<!-- Added by: lperron, at: Fri Jun  7 09:58:09 CEST 2019 -->

<!--te-->


## Documentation structure

This document presents modeling recipes for the CP-SAT solver.

Code samples are given in C++, Python, Java and C#. Each language have different
requirements for the code samples.

## Searching for one (optimal) solution of a CP-SAT model

By default, searching for one solution will return the first solution found if
the model has no objective, or the optimal solution if the model has an
objective.

### Python code samples

The Python interface to the CP-SAT solver is implemented using two classes.

*   The **CpModel** class proposes modeling methods that creates variables, or
    add constraints. This class completely hides the underlying *CpModelProto*
    used to store the model.
*   The **CpSolver** class encapsulates the solve API. and offers helpers to
    access the solution found by the solve.

```python
"""Simple solve."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


def SimpleSatProgram():
  """Minimal CP-SAT example to showcase calling the solver."""
  # Creates the model.
  model = cp_model.CpModel()

  # Creates the variables.
  num_vals = 3
  x = model.NewIntVar(0, num_vals - 1, 'x')
  y = model.NewIntVar(0, num_vals - 1, 'y')
  z = model.NewIntVar(0, num_vals - 1, 'z')

  # Creates the constraints.
  model.Add(x != y)

  # Creates a solver and solves the model.
  solver = cp_model.CpSolver()
  status = solver.Solve(model)

  if status == cp_model.FEASIBLE:
    print('x = %i' % solver.Value(x))
    print('y = %i' % solver.Value(y))
    print('z = %i' % solver.Value(z))


SimpleSatProgram()
```

### C++ code samples

The interface to the C++ CP-SAT solver is implemented through the
**CpModelBuilder** class described in *ortools/sat/cp_model.h*.
This class is just a helper to fill in the cp_model protobuf.

Calling Solve() method will return a CpSolverResponse protobuf that contains the
solve status, the values for each variable in the model if solve was successful,
and some metrics.

```cpp
#include "ortools/sat/cp_model.h"

namespace operations_research {
namespace sat {

void SimpleSatProgram() {
  CpModelBuilder cp_model;

  const Domain domain(0, 2);
  const IntVar x = cp_model.NewIntVar(domain).WithName("x");
  const IntVar y = cp_model.NewIntVar(domain).WithName("y");
  const IntVar z = cp_model.NewIntVar(domain).WithName("z");

  cp_model.AddNotEqual(x, y);

  // Solving part.
  const CpSolverResponse response = Solve(cp_model.Build());
  LOG(INFO) << CpSolverResponseStats(response);

  if (response.status() == CpSolverStatus::FEASIBLE) {
    // Get the value of x in the solution.
    LOG(INFO) << "x = " << SolutionIntegerValue(response, x);
    LOG(INFO) << "y = " << SolutionIntegerValue(response, y);
    LOG(INFO) << "z = " << SolutionIntegerValue(response, z);
  }
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::SimpleSatProgram();

  return EXIT_SUCCESS;
}
```

### Java code samples

The Java code implements the same interface as the Python code, with a
**CpModel** and a **CpSolver** class.

```java
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.IntVar;

/** Minimal CP-SAT example to showcase calling the solver. */
public class SimpleSatProgram {

  static { System.loadLibrary("jniortools"); }

  public static void main(String[] args) throws Exception {
    // Create the model.
    CpModel model = new CpModel();

    // Create the variables.
    int numVals = 3;

    IntVar x = model.newIntVar(0, numVals - 1, "x");
    IntVar y = model.newIntVar(0, numVals - 1, "y");
    IntVar z = model.newIntVar(0, numVals - 1, "z");

    // Create the constraints.
    model.addDifferent(x, y);

    // Create a solver and solve the model.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.solve(model);

    if (status == CpSolverStatus.FEASIBLE) {
      System.out.println("x = " + solver.value(x));
      System.out.println("y = " + solver.value(y));
      System.out.println("z = " + solver.value(z));
    }
  }
}
```

### C\# code samples

The C\# code implements the same interface as the Python code, with a
**CpModel** and a **CpSolver** class.


```cs
using System;
using Google.OrTools.Sat;

public class SimpleSatProgram
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

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.Solve(model);

    if (status == CpSolverStatus.Feasible)
    {
      Console.WriteLine("x = " + solver.Value(x));
      Console.WriteLine("y = " + solver.Value(y));
      Console.WriteLine("z = " + solver.Value(z));
    }
  }
}
```
