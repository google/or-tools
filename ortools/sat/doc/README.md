| [home](README.md) | [boolean logic](boolean_logic.md) | [integer arithmetic](integer_arithmetic.md) | [channeling constraints](channeling.md) | [scheduling](scheduling.md) | [Using the CP-SAT solver](solver.md) | [Model manipulation](model.md) | [Reference manual](reference.md) |
| ----------------- | --------------------------------- | ------------------------------------------- | --------------------------------------- | --------------------------- | ------------------------------------ | ------------------------------ | -------------------------------- |

# Using the CP-SAT solver



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

## Table of contents


<!--ts-->
   * [Using the CP-SAT solver](README.md#using-the-cp-sat-solver)
      * [Documentation structure](README.md#documentation-structure)
      * [Searching for one (optimal) solution of a CP-SAT model](README.md#searching-for-one-optimal-solution-of-a-cp-sat-model)
         * [Python code samples](README.md#python-code-samples)
         * [C   code samples](README.md#c-code-samples)
         * [Java code samples](README.md#java-code-samples)
         * [C# code samples](README.md#c-code-samples-1)
      * [Table of contents](README.md#table-of-contents)

   * [Boolean logic recipes for the CP-SAT solver.](boolean_logic.md#boolean-logic-recipes-for-the-cp-sat-solver)
      * [Introduction](boolean_logic.md#introduction)
      * [Boolean variables and literals](boolean_logic.md#boolean-variables-and-literals)
         * [Python code](boolean_logic.md#python-code)
         * [C   code](boolean_logic.md#c-code)
         * [Java code](boolean_logic.md#java-code)
         * [C# code](boolean_logic.md#c-code-1)
      * [Boolean constraints](boolean_logic.md#boolean-constraints)
         * [Python code](boolean_logic.md#python-code-1)
         * [C   code](boolean_logic.md#c-code-2)
         * [Java code](boolean_logic.md#java-code-1)
         * [C# code](boolean_logic.md#c-code-3)
      * [Reified constraints](boolean_logic.md#reified-constraints)
         * [Python code](boolean_logic.md#python-code-2)
         * [C   code](boolean_logic.md#c-code-4)
         * [Java code](boolean_logic.md#java-code-2)
         * [C# code](boolean_logic.md#c-code-5)
      * [Product of two Boolean Variables](boolean_logic.md#product-of-two-boolean-variables)
         * [Python code](boolean_logic.md#python-code-3)

   * [Integer arithmetic recipes for the CP-SAT solver.](integer_arithmetic.md#integer-arithmetic-recipes-for-the-cp-sat-solver)
      * [Introduction](integer_arithmetic.md#introduction)
      * [Integer variables](integer_arithmetic.md#integer-variables)
         * [Interval domain](integer_arithmetic.md#interval-domain)
         * [Non-contiguous domain](integer_arithmetic.md#non-contiguous-domain)
         * [Boolean variables](integer_arithmetic.md#boolean-variables)
         * [Other methods](integer_arithmetic.md#other-methods)
      * [Linear constraints](integer_arithmetic.md#linear-constraints)
         * [C   and Java linear constraints and linear expressions](integer_arithmetic.md#c-and-java-linear-constraints-and-linear-expressions)
         * [Python and C# linear constraints and linear expressions](integer_arithmetic.md#python-and-c-linear-constraints-and-linear-expressions)
         * [Generic linear constraint](integer_arithmetic.md#generic-linear-constraint)
         * [Limitations](integer_arithmetic.md#limitations)
      * [Rabbits and Pheasants examples](integer_arithmetic.md#rabbits-and-pheasants-examples)
         * [Python code](integer_arithmetic.md#python-code)
         * [C   code](integer_arithmetic.md#c-code)
         * [Java code](integer_arithmetic.md#java-code)
         * [C# code](integer_arithmetic.md#c-code-1)
      * [Earliness-Tardiness cost function.](integer_arithmetic.md#earliness-tardiness-cost-function)
         * [Python code](integer_arithmetic.md#python-code-1)
         * [C   code](integer_arithmetic.md#c-code-2)
         * [Java code](integer_arithmetic.md#java-code-1)
         * [C# code](integer_arithmetic.md#c-code-3)
      * [Step function.](integer_arithmetic.md#step-function)
         * [Python code](integer_arithmetic.md#python-code-2)
         * [C   code](integer_arithmetic.md#c-code-4)
         * [Java code](integer_arithmetic.md#java-code-2)
         * [C# code](integer_arithmetic.md#c-code-5)

   * [Channeling constraints](channeling.md#channeling-constraints)
      * [If-Then-Else expressions](channeling.md#if-then-else-expressions)
         * [Python code](channeling.md#python-code)
         * [C   code](channeling.md#c-code)
         * [Java code](channeling.md#java-code)
         * [C# code](channeling.md#c-code-1)
      * [A bin-packing problem](channeling.md#a-bin-packing-problem)
         * [Python code](channeling.md#python-code-1)
         * [C   code](channeling.md#c-code-2)
         * [Java code](channeling.md#java-code-1)
         * [C# code](channeling.md#c-code-3)

   * [Scheduling recipes for the CP-SAT solver.](scheduling.md#scheduling-recipes-for-the-cp-sat-solver)
      * [Introduction](scheduling.md#introduction)
      * [Interval variables](scheduling.md#interval-variables)
         * [Python code](scheduling.md#python-code)
         * [C   code](scheduling.md#c-code)
         * [Java code](scheduling.md#java-code)
         * [C# code](scheduling.md#c-code-1)
      * [Optional intervals](scheduling.md#optional-intervals)
         * [Python code](scheduling.md#python-code-1)
         * [C   code](scheduling.md#c-code-2)
         * [Java code](scheduling.md#java-code-1)
         * [C# code](scheduling.md#c-code-3)
      * [NoOverlap constraint](scheduling.md#nooverlap-constraint)
         * [Python code](scheduling.md#python-code-2)
         * [C   code](scheduling.md#c-code-4)
         * [Java code](scheduling.md#java-code-2)
         * [C# code](scheduling.md#c-code-5)
      * [Cumulative constraint](scheduling.md#cumulative-constraint)
      * [Alternative resources for one interval](scheduling.md#alternative-resources-for-one-interval)
      * [Ranking tasks in a disjunctive resource](scheduling.md#ranking-tasks-in-a-disjunctive-resource)
         * [Python code](scheduling.md#python-code-3)
         * [C   code](scheduling.md#c-code-6)
         * [Java code](scheduling.md#java-code-3)
         * [C# code](scheduling.md#c-code-7)
      * [Intervals spanning over breaks in the calendar](scheduling.md#intervals-spanning-over-breaks-in-the-calendar)
         * [Python code](scheduling.md#python-code-4)
      * [Transitions in a disjunctive resource](scheduling.md#transitions-in-a-disjunctive-resource)
      * [Precedences between intervals](scheduling.md#precedences-between-intervals)
      * [Convex hull of a set of intervals](scheduling.md#convex-hull-of-a-set-of-intervals)
      * [Reservoir constraint](scheduling.md#reservoir-constraint)

   * [Solving a CP-SAT model](solver.md#solving-a-cp-sat-model)
      * [Changing the parameters of the solver](solver.md#changing-the-parameters-of-the-solver)
         * [Specifying the time limit in Python](solver.md#specifying-the-time-limit-in-python)
         * [Specifying the time limit in C  ](solver.md#specifying-the-time-limit-in-c)
         * [Specifying the time limit in Java](solver.md#specifying-the-time-limit-in-java)
         * [Specifying the time limit in C#.](solver.md#specifying-the-time-limit-in-c-1)
      * [Printing intermediate solutions](solver.md#printing-intermediate-solutions)
         * [Python code](solver.md#python-code)
         * [C   code](solver.md#c-code)
         * [Java code](solver.md#java-code)
         * [C# code](solver.md#c-code-1)
      * [Searching for all solutions in a satisfiability model](solver.md#searching-for-all-solutions-in-a-satisfiability-model)
         * [Python code](solver.md#python-code-1)
         * [C   code](solver.md#c-code-2)
         * [Java code](solver.md#java-code-1)
         * [C# code](solver.md#c-code-3)
      * [Stopping search early](solver.md#stopping-search-early)
         * [Python code](solver.md#python-code-2)
         * [C   code](solver.md#c-code-4)
         * [Java code](solver.md#java-code-2)
         * [C# code](solver.md#c-code-5)

   * [Model manipulation](model.md#model-manipulation)
      * [Introduction](model.md#introduction)
      * [Solution hinting](model.md#solution-hinting)
         * [Python code](model.md#python-code)
<!--te-->