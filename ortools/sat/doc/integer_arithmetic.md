| [home](README.md) | [boolean logic](boolean_logic.md) | [integer arithmetic](integer_arithmetic.md) | [channeling constraints](channeling.md) | [scheduling](scheduling.md) | [Using the CP-SAT solver](solver.md) | [Model manipulation](model.md) | [Reference manual](reference.md) |
| ----------------- | --------------------------------- | ------------------------------------------- | --------------------------------------- | --------------------------- | ------------------------------------ | ------------------------------ | -------------------------------- |

# Integer arithmetic recipes for the CP-SAT solver.


<!--ts-->
   * [Integer arithmetic recipes for the CP-SAT solver.](#integer-arithmetic-recipes-for-the-cp-sat-solver)
      * [Introduction](#introduction)
      * [Integer variables](#integer-variables)
         * [Interval domain](#interval-domain)
         * [Non-contiguous domain](#non-contiguous-domain)
         * [Boolean variables](#boolean-variables)
         * [Other methods](#other-methods)
      * [Linear constraints](#linear-constraints)
         * [C   and Java linear constraints and linear expressions](#c-and-java-linear-constraints-and-linear-expressions)
         * [Python and C# linear constraints and linear expressions](#python-and-c-linear-constraints-and-linear-expressions)
         * [Generic linear constraint](#generic-linear-constraint)
         * [Limitations](#limitations)
      * [Rabbits and Pheasants examples](#rabbits-and-pheasants-examples)
         * [Python code](#python-code)
         * [C   code](#c-code)
         * [Java code](#java-code)
         * [C# code](#c-code-1)
      * [Earliness-Tardiness cost function.](#earliness-tardiness-cost-function)
         * [Python code](#python-code-1)
         * [C   code](#c-code-2)
         * [Java code](#java-code-1)
         * [C# code](#c-code-3)
      * [Step function.](#step-function)
         * [Python code](#python-code-2)
         * [C   code](#c-code-4)
         * [Java code](#java-code-2)
         * [C# code](#c-code-5)

<!-- Added by: lperron, at: Fri Jun  7 09:58:32 CEST 2019 -->

<!--te-->


## Introduction

The CP-SAT solver can express integer variables and constraints.

## Integer variables

Integer variables can take on 64-bit signed integer values. When creating them,
a domain must be provided.

### Interval domain

To create a single contiguous integer domain, just call the `NewIntVar` method
with the lower and upper bounds. For instance, to create a variable that can
take on any value between 0 and 10, inclusive:

-   **C++**: `IntVar x = model.NewIntVar({0, 10}).WithName("x");`
-   **Python**: `x = model.NewIntVar(0, 10, 'x')`
-   **Java**: `IntVar x = model.newIntVar(0, 10, "x");`
-   **C#**: `IntVar x = model.NewIntVar(0, 10, "x");`

### Non-contiguous domain

An instance of the Domain class is needed to create variables with 
non-contiguous domains. Here, the variable can be any of 1, 3, 4, or 6:

-   **C++**: `model.NewIntVar(Domain::FromValues({1, 3, 4, 6});`
-   **Python**: `model.NewIntVarFromDomain(cp_model.Domain.FromValues([1, 3, 4,
    6]), 'x')`
-   **Java**: `model.newIntVarFromDomain(Domain.fromValues(new long[] {1, 3, 4,
    6}), "x");`
-   **C#**: `model.NewIntVarFromDomain(Domain.FromValues(new long[] {1, 3, 4,
    6}), "x");`

Variables can also be created using a list of intervals. Below, the variable
created is constrained to be 1, 2, 4, 5, or 6:

-   **C++**: `model.NewIntVar(Domain::FromIntervals({{1, 2}, {4, 6}});`
-   **Python**: `model.NewIntVarFromDomain(cp_model.Domain.FromIntervals([[1,
    2], [4, 6]]), 'x')`
-   **Java**: `model.newIntVarFromDomain(Domain.fromIntervals(new long[][] {{1,
    2}, {4, 6}}), "x");`
-   **C#**: `model.NewIntVarFromDomain(Domain.FromIntervals(new long[][] { new
    long[] {1, 2}, new long[] {4, 6} }), "x");`

### Boolean variables

To create a Boolean variable, use the `NewBoolVar` method. Please note that
Boolean variables are typed differently than integer variables, and that this
type is not uniform across languages.

-   **C++**: `BoolVar x = model.NewBoolVar().WithName("x");`
-   **Python**: `x = model.NewBoolVar('x')`
-   **Java**: `Literal x = model.newBoolVar("x");`
-   **C#**: `ILiteral x = model.NewBoolVar("x");`

### Other methods

To exclude a single value, use ranges combined with int64min and int64max
values, e.g., `[[int64min, -3], [-1, int64max]]`, or use the `Complement` method.

To create a variable with a single value domain, use the `NewConstant()` API (or
`newConstant()` in Java).

## Linear constraints

### C++ and Java linear constraints and linear expressions

**C++** and **Java** APIs do not use arithmetic operators (+, \*, -, <=...).
Linear constraints are created using a method of the model factory, such as 
`cp_model.AddEquality(x, 3)` in C++, or `cp_model.addGreaterOrEqual(x, 10)` in
Java.

Furthermore, helper methods can be used to create sums and scalar products like
`LinearExpr::Sum({x, y, z})` in C++, and `LinearExpr.scalProd(new IntVar[] {x,
y, z}, new long[] {1, 2, 3})` in Java.

### Python and C\# linear constraints and linear expressions

**Python** and **C\#** CP-SAT APIs support general linear arithmetic (+, \*, -,
==, >=, >, <, <=, !=). You need to use the Add method of the cp_model, as in
`cp_model.Add(x + y != 3)`.

### Generic linear constraint

in **all languages**, the cp_model factory offers a generic method to constrain a
linear expression to be in a domain. This is used in the step function examples
below.

### Limitations

-   Everything must be linear. Multiplying two variables is not supported
    with this API; instead, `model.AddProductEquality()` must be used.

-   In C++, there is a typing issue when using an array of Boolean variables in
    a sum or a scalar product. Use the `LinearExpr.BooleanSum()` method instead.

-   The Python construct `sum()` is supported, but `min()`, `max()`
    or any `numpy` constructs like `np.unique()` are not.

## Rabbits and Pheasants examples

Let's solve a simple children's puzzle: the Rabbits and Pheasants problem.

In a field of rabbits and pheasants, there are 20 heads and 56 legs. How many
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

  const CpSolverResponse response = Solve(cp_model.Build());

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
import com.google.ortools.sat.LinearExpr;

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
    model.addEquality(LinearExpr.sum(new IntVar[] {r, p}), 20);
    // 56 legs.
    model.addEquality(LinearExpr.scalProd(new IntVar[] {r, p}, new long[] {4, 2}), 56);

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

## Earliness-Tardiness cost function.

Let's encode a useful convex piecewise linear function that often appears in
scheduling. You want to encourage a delivery to happen during a time window. If
you deliver early, you pay a linear penalty on waiting time. If you deliver
late, you pay a linear penalty on the delay.

Because the function is convex, you can define all affine functions, and take
the max of them to define the piecewise linear function.

The following samples output:

    x=0 expr=40
    x=1 expr=32
    x=2 expr=24
    x=3 expr=16
    x=4 expr=8
    x=5 expr=0
    x=6 expr=0
    x=7 expr=0
    x=8 expr=0
    x=9 expr=0
    x=10 expr=0
    x=11 expr=0
    x=12 expr=0
    x=13 expr=0
    x=14 expr=0
    x=15 expr=0
    x=16 expr=12
    x=17 expr=24
    x=18 expr=36
    x=19 expr=48
    x=20 expr=60

### Python code

```python
"""Encodes an convex piecewise linear function."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


class VarArraySolutionPrinter(cp_model.CpSolverSolutionCallback):
  """Print intermediate solutions."""

  def __init__(self, variables):
    cp_model.CpSolverSolutionCallback.__init__(self)
    self.__variables = variables
    self.__solution_count = 0

  def on_solution_callback(self):
    self.__solution_count += 1
    for v in self.__variables:
      print('%s=%i' % (v, self.Value(v)), end=' ')
    print()

  def solution_count(self):
    return self.__solution_count


def earliness_tardiness_cost_sample_sat():
  """Encode the piecewise linear expression."""

  earliness_date = 5  # ed.
  earliness_cost = 8
  lateness_date = 15  # ld.
  lateness_cost = 12

  # Model.
  model = cp_model.CpModel()

  # Declare our primary variable.
  x = model.NewIntVar(0, 20, 'x')

  # Create the expression variable and implement the piecewise linear function.
  #
  #  \        /
  #   \______/
  #   ed    ld
  #
  large_constant = 1000
  expr = model.NewIntVar(0, large_constant, 'expr')

  # First segment.
  s1 = model.NewIntVar(-large_constant, large_constant, 's1')
  model.Add(s1 == earliness_cost * (earliness_date - x))

  # Second segment.
  s2 = 0

  # Third segment.
  s3 = model.NewIntVar(-large_constant, large_constant, 's3')
  model.Add(s3 == lateness_cost * (x - lateness_date))

  # Link together expr and x through s1, s2, and s3.
  model.AddMaxEquality(expr, [s1, s2, s3])

  # Search for x values in increasing order.
  model.AddDecisionStrategy([x], cp_model.CHOOSE_FIRST,
                            cp_model.SELECT_MIN_VALUE)

  # Create a solver and solve with a fixed search.
  solver = cp_model.CpSolver()

  # Force the solver to follow the decision strategy exactly.
  solver.parameters.search_branching = cp_model.FIXED_SEARCH

  # Search and print out all solutions.
  solution_printer = VarArraySolutionPrinter([x, expr])
  solver.SearchForAllSolutions(model, solution_printer)


earliness_tardiness_cost_sample_sat()
```

### C++ code

```cpp
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

void EarlinessTardinessCostSampleSat() {
  const int64 kEarlinessDate = 5;
  const int64 kEarlinessCost = 8;
  const int64 kLatenessDate = 15;
  const int64 kLatenessCost = 12;

  // Create the CP-SAT model.
  CpModelBuilder cp_model;

  // Declare our primary variable.
  const IntVar x = cp_model.NewIntVar({0, 20});

  // Create the expression variable and implement the piecewise linear function.
  //
  //  \        /
  //   \______/
  //   ed    ld
  //
  const int64 kLargeConstant = 1000;
  const IntVar expr = cp_model.NewIntVar({0, kLargeConstant});

  // First segment.
  const IntVar s1 = cp_model.NewIntVar({-kLargeConstant, kLargeConstant});
  cp_model.AddEquality(s1, LinearExpr::ScalProd({x}, {-kEarlinessCost})
                               .AddConstant(kEarlinessCost * kEarlinessDate));

  // Second segment.
  const IntVar s2 = cp_model.NewConstant(0);

  // Third segment.
  const IntVar s3 = cp_model.NewIntVar({-kLargeConstant, kLargeConstant});
  cp_model.AddEquality(s3, LinearExpr::ScalProd({x}, {kLatenessCost})
                               .AddConstant(-kLatenessCost * kLatenessDate));

  // Link together expr and x through s1, s2, and s3.
  cp_model.AddMaxEquality(expr, {s1, s2, s3});

  // Search for x values in increasing order.
  cp_model.AddDecisionStrategy({x}, DecisionStrategyProto::CHOOSE_FIRST,
                               DecisionStrategyProto::SELECT_MIN_VALUE);

  // Create a solver and solve with a fixed search.
  Model model;
  SatParameters parameters;
  parameters.set_search_branching(SatParameters::FIXED_SEARCH);
  parameters.set_enumerate_all_solutions(true);
  model.Add(NewSatParameters(parameters));
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    LOG(INFO) << "x=" << SolutionIntegerValue(r, x) << " expr"
              << SolutionIntegerValue(r, expr);
  }));
  SolveCpModel(cp_model.Build(), &model);
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::EarlinessTardinessCostSampleSat();

  return EXIT_SUCCESS;
}
```

### Java code

```java
import com.google.ortools.sat.DecisionStrategyProto;
import com.google.ortools.sat.SatParameters;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverSolutionCallback;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.LinearExpr;

/** Encode the piecewise linear expression. */
public class EarlinessTardinessCostSampleSat {
  static { System.loadLibrary("jniortools"); }

  public static void main(String[] args) throws Exception {
    long earlinessDate = 5;
    long earlinessCost = 8;
    long latenessDate = 15;
    long latenessCost = 12;

    // Create the CP-SAT model.
    CpModel model = new CpModel();

    // Declare our primary variable.
    IntVar x = model.newIntVar(0, 20, "x");

    // Create the expression variable and implement the piecewise linear function.
    //
    //  \        /
    //   \______/
    //   ed    ld
    //
    long largeConstant = 1000;
    IntVar expr = model.newIntVar(0, largeConstant, "expr");

    // First segment: s1 == earlinessCost * (earlinessDate - x).
    IntVar s1 = model.newIntVar(-largeConstant, largeConstant, "s1");
    model.addEquality(
        LinearExpr.scalProd(new IntVar[] {s1, x}, new long[] {1, earlinessCost}),
        earlinessCost * earlinessDate);

    // Second segment.
    IntVar s2 = model.newConstant(0);

    // Third segment: s3 == latenessCost * (x - latenessDate).
    IntVar s3 = model.newIntVar(-largeConstant, largeConstant, "s3");
    model.addEquality(
        LinearExpr.scalProd(new IntVar[] {s3, x}, new long[] {1, -latenessCost}),
        -latenessCost * latenessDate);

    // Link together expr and x through s1, s2, and s3.
    model.addMaxEquality(expr, new IntVar[] {s1, s2, s3});

    // Search for x values in increasing order.
    model.addDecisionStrategy(
        new IntVar[] {x},
        DecisionStrategyProto.VariableSelectionStrategy.CHOOSE_FIRST,
        DecisionStrategyProto.DomainReductionStrategy.SELECT_MIN_VALUE);

    // Create the solver.
    CpSolver solver = new CpSolver();

    // Force the solver to follow the decision strategy exactly.
    solver.getParameters().setSearchBranching(SatParameters.SearchBranching.FIXED_SEARCH);

    // Solve the problem with the printer callback.
    solver.searchAllSolutions(
        model,
        new CpSolverSolutionCallback() {
          public CpSolverSolutionCallback init(IntVar[] variables) {
            variableArray = variables;
            return this;
          }

          @Override
          public void onSolutionCallback() {
            for (IntVar v : variableArray) {
              System.out.printf("%s=%d ", v.getName(), value(v));
            }
            System.out.println();
          }

          private IntVar[] variableArray;
        }.init(new IntVar[] {x, expr}));
  }
}
```

### C\# code

```cs
using System;
using Google.OrTools.Sat;
using Google.OrTools.Util;

public class VarArraySolutionPrinter : CpSolverSolutionCallback
{
  public VarArraySolutionPrinter(IntVar[] variables)
  {
    variables_ = variables;
  }

  public override void OnSolutionCallback()
  {
    {
      foreach (IntVar v in variables_)
      {
        Console.Write(String.Format("{0}={1} ", v.ShortString(), Value(v)));
      }
      Console.WriteLine();
    }
  }

  private IntVar[] variables_;
}

public class EarlinessTardinessCostSampleSat
{
  static void Main()
  {
    long earliness_date = 5;
    long earliness_cost = 8;
    long lateness_date = 15;
    long lateness_cost = 12;

    // Create the CP-SAT model.
    CpModel model = new CpModel();

    // Declare our primary variable.
    IntVar x = model.NewIntVar(0, 20, "x");

    // Create the expression variable and implement the piecewise linear
    // function.
    //
    //  \        /
    //   \______/
    //   ed    ld
    //
    long large_constant = 1000;
    IntVar expr = model.NewIntVar(0, large_constant, "expr");

    // First segment.
    IntVar s1 = model.NewIntVar(-large_constant, large_constant, "s1");
    model.Add(s1 == earliness_cost * (earliness_date - x));

    // Second segment.
    IntVar s2 = model.NewConstant(0);

    // Third segment.
    IntVar s3 = model.NewIntVar(-large_constant, large_constant, "s3");
    model.Add(s3 == lateness_cost * (x - lateness_date));

    // Link together expr and x through s1, s2, and s3.
    model.AddMaxEquality(expr, new IntVar[] {s1, s2, s3});

    // Search for x values in increasing order.
    model.AddDecisionStrategy(
        new IntVar[] {x},
        DecisionStrategyProto.Types.VariableSelectionStrategy.ChooseFirst,
        DecisionStrategyProto.Types.DomainReductionStrategy.SelectMinValue);

    // Create the solver.
    CpSolver solver = new CpSolver();

    // Force solver to follow the decision strategy exactly.
    solver.StringParameters = "search_branching:FIXED_SEARCH";

    VarArraySolutionPrinter cb =
        new VarArraySolutionPrinter(new IntVar[] {x, expr});
    solver.SearchAllSolutions(model, cb);
  }
}
```

## Step function.

Let's encode a step function. We will use one Boolean variable per step value,
and filter the admissible domain of the input variable with this variable.

The following samples output:

    x=0 expr=2
    x=1 expr=2
    x=3 expr=2
    x=4 expr=2
    x=5 expr=0
    x=6 expr=0
    x=7 expr=3
    x=8 expr=0
    x=9 expr=0
    x=10 expr=0
    x=11 expr=2
    x=12 expr=2
    x=13 expr=2
    x=14 expr=2
    x=15 expr=2
    x=16 expr=2
    x=17 expr=2
    x=18 expr=2
    x=19 expr=2
    x=20 expr=2

### Python code

```python
"""Implements a step function."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


class VarArraySolutionPrinter(cp_model.CpSolverSolutionCallback):
  """Print intermediate solutions."""

  def __init__(self, variables):
    cp_model.CpSolverSolutionCallback.__init__(self)
    self.__variables = variables
    self.__solution_count = 0

  def on_solution_callback(self):
    self.__solution_count += 1
    for v in self.__variables:
      print('%s=%i' % (v, self.Value(v)), end=' ')
    print()

  def solution_count(self):
    return self.__solution_count


def step_function_sample_sat():
  """Encode the step function."""

  # Model.
  model = cp_model.CpModel()

  # Declare our primary variable.
  x = model.NewIntVar(0, 20, 'x')

  # Create the expression variable and implement the step function
  # Note it is not defined for x == 2.
  #
  #        -               3
  # -- --      ---------   2
  #                        1
  #      -- ---            0
  # 0 ================ 20
  #
  expr = model.NewIntVar(0, 3, 'expr')

  # expr == 0 on [5, 6] U [8, 10]
  b0 = model.NewBoolVar('b0')
  model.AddLinearExpressionInDomain(
      x, cp_model.Domain.FromIntervals([(5, 6), (8, 10)])).OnlyEnforceIf(b0)
  model.Add(expr == 0).OnlyEnforceIf(b0)

  # expr == 2 on [0, 1] U [3, 4] U [11, 20]
  b2 = model.NewBoolVar('b2')
  model.AddLinearExpressionInDomain(
      x, cp_model.Domain.FromIntervals([(0, 1), (3, 4),
                                        (11, 20)])).OnlyEnforceIf(b2)
  model.Add(expr == 2).OnlyEnforceIf(b2)

  # expr == 3 when x == 7
  b3 = model.NewBoolVar('b3')
  model.Add(x == 7).OnlyEnforceIf(b3)
  model.Add(expr == 3).OnlyEnforceIf(b3)

  # At least one bi is true. (we could use a sum == 1).
  model.AddBoolOr([b0, b2, b3])

  # Search for x values in increasing order.
  model.AddDecisionStrategy([x], cp_model.CHOOSE_FIRST,
                            cp_model.SELECT_MIN_VALUE)

  # Create a solver and solve with a fixed search.
  solver = cp_model.CpSolver()

  # Force the solver to follow the decision strategy exactly.
  solver.parameters.search_branching = cp_model.FIXED_SEARCH

  # Search and print out all solutions.
  solution_printer = VarArraySolutionPrinter([x, expr])
  solver.SearchForAllSolutions(model, solution_printer)


step_function_sample_sat()
```

### C++ code

```cpp
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

void StepFunctionSampleSat() {
  // Create the CP-SAT model.
  CpModelBuilder cp_model;

  // Declare our primary variable.
  const IntVar x = cp_model.NewIntVar({0, 20});

  // Create the expression variable and implement the step function
  // Note it is not defined for var == 2.
  //
  //        -               3
  // -- --      ---------   2
  //                        1
  //      -- ---            0
  // 0 ================ 20
  //
  IntVar expr = cp_model.NewIntVar({0, 3});

  // expr == 0 on [5, 6] U [8, 10]
  BoolVar b0 = cp_model.NewBoolVar();
  cp_model.AddLinearConstraint(x, Domain::FromValues({5, 6, 8, 9, 10}))
      .OnlyEnforceIf(b0);
  cp_model.AddEquality(expr, 0).OnlyEnforceIf(b0);

  // expr == 2 on [0, 1] U [3, 4] U [11, 20]
  BoolVar b2 = cp_model.NewBoolVar();
  cp_model
      .AddLinearConstraint(x, Domain::FromIntervals({{0, 1}, {3, 4}, {11, 20}}))
      .OnlyEnforceIf(b2);
  cp_model.AddEquality(expr, 2).OnlyEnforceIf(b2);

  // expr == 3 when x = 7
  BoolVar b3 = cp_model.NewBoolVar();
  cp_model.AddEquality(x, 7).OnlyEnforceIf(b3);
  cp_model.AddEquality(expr, 3).OnlyEnforceIf(b3);

  // At least one bi is true. (we could use a sum == 1).
  cp_model.AddBoolOr({b0, b2, b3});

  // Search for x values in increasing order.
  cp_model.AddDecisionStrategy({x}, DecisionStrategyProto::CHOOSE_FIRST,
                               DecisionStrategyProto::SELECT_MIN_VALUE);

  // Create a solver and solve with a fixed search.
  Model model;
  SatParameters parameters;
  parameters.set_search_branching(SatParameters::FIXED_SEARCH);
  parameters.set_enumerate_all_solutions(true);
  model.Add(NewSatParameters(parameters));
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    LOG(INFO) << "x=" << SolutionIntegerValue(r, x) << " expr"
              << SolutionIntegerValue(r, expr);
  }));
  SolveCpModel(cp_model.Build(), &model);
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::StepFunctionSampleSat();

  return EXIT_SUCCESS;
}
```

### Java code

```java
import com.google.ortools.sat.DecisionStrategyProto;
import com.google.ortools.sat.SatParameters;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverSolutionCallback;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.Literal;
import com.google.ortools.util.Domain;

/** Link integer constraints together. */
public class StepFunctionSampleSat {

  static { System.loadLibrary("jniortools"); }

  public static void main(String[] args) throws Exception {
    // Create the CP-SAT model.
    CpModel model = new CpModel();

    // Declare our primary variable.
    IntVar x = model.newIntVar(0, 20, "x");

    // Create the expression variable and implement the step function
    // Note it is not defined for var == 2.
    //
    //        -               3
    // -- --      ---------   2
    //                        1
    //      -- ---            0
    // 0 ================ 20
    //
    IntVar expr = model.newIntVar(0, 3, "expr");

    // expr == 0 on [5, 6] U [8, 10]
    Literal b0 = model.newBoolVar("b0");
    model
        .addLinearExpressionInDomain(x, Domain.fromValues(new long[] {5, 6, 8, 9, 10}))
        .onlyEnforceIf(b0);
    model.addEquality(expr, 0).onlyEnforceIf(b0);

    // expr == 2 on [0, 1] U [3, 4] U [11, 20]
    Literal b2 = model.newBoolVar("b2");
    model
        .addLinearExpressionInDomain(
            x, Domain.fromIntervals(new long[][] {{0, 1}, {3, 4}, {11, 20}}))
        .onlyEnforceIf(b2);
    model.addEquality(expr, 2).onlyEnforceIf(b2);

    // expr == 3 when x = 7
    Literal b3 = model.newBoolVar("b3");
    model.addEquality(x, 7).onlyEnforceIf(b3);
    model.addEquality(expr, 3).onlyEnforceIf(b3);

    // At least one bi is true. (we could use a sum == 1).
    model.addBoolOr(new Literal[] {b0, b2, b3});

    // Search for x values in increasing order.
    model.addDecisionStrategy(
        new IntVar[] {x},
        DecisionStrategyProto.VariableSelectionStrategy.CHOOSE_FIRST,
        DecisionStrategyProto.DomainReductionStrategy.SELECT_MIN_VALUE);

    // Create the solver.
    CpSolver solver = new CpSolver();

    // Force the solver to follow the decision strategy exactly.
    solver.getParameters().setSearchBranching(SatParameters.SearchBranching.FIXED_SEARCH);

    // Solve the problem with the printer callback.
    solver.searchAllSolutions(
        model,
        new CpSolverSolutionCallback() {
          public CpSolverSolutionCallback init(IntVar[] variables) {
            variableArray = variables;
            return this;
          }

          @Override
          public void onSolutionCallback() {
            for (IntVar v : variableArray) {
              System.out.printf("%s=%d ", v.getName(), value(v));
            }
            System.out.println();
          }

          private IntVar[] variableArray;
        }.init(new IntVar[] {x, expr}));
  }
}
```

### C\# code

```cs
using System;
using Google.OrTools.Sat;
using Google.OrTools.Util;

public class VarArraySolutionPrinter : CpSolverSolutionCallback
{
  public VarArraySolutionPrinter(IntVar[] variables)
  {
    variables_ = variables;
  }

  public override void OnSolutionCallback()
  {
    {
      foreach (IntVar v in variables_)
      {
        Console.Write(String.Format("{0}={1} ", v.ShortString(), Value(v)));
      }
      Console.WriteLine();
    }
  }

  private IntVar[] variables_;
}

public class StepFunctionSampleSat
{
  static void Main()
  {
    // Create the CP-SAT model.
    CpModel model = new CpModel();

    // Declare our primary variable.
    IntVar x = model.NewIntVar(0, 20, "x");

    // Create the expression variable and implement the step function
    // Note it is not defined for var == 2.
    //
    //        -               3
    // -- --      ---------   2
    //                        1
    //      -- ---            0
    // 0 ================ 20
    //
    IntVar expr = model.NewIntVar(0, 3, "expr");

    // expr == 0 on [5, 6] U [8, 10]
    ILiteral b0 = model.NewBoolVar("b0");
    model.AddLinearExpressionInDomain(
        x,
        Domain.FromValues(new long[] { 5, 6, 8, 9, 10 })).OnlyEnforceIf(b0);
    model.Add(expr == 0).OnlyEnforceIf(b0);

    // expr == 2 on [0, 1] U [3, 4] U [11, 20]
    ILiteral b2 = model.NewBoolVar("b2");
    model.AddLinearExpressionInDomain(
        x,
        Domain.FromIntervals(
            new long[][] {new long[] {0, 1},
                          new long[] {3, 4},
                          new long[] {11, 20}})).OnlyEnforceIf(b2);
    model.Add(expr == 2).OnlyEnforceIf(b2);

    // expr == 3 when x == 7
    ILiteral b3 = model.NewBoolVar("b3");
    model.Add(x == 7).OnlyEnforceIf(b3);
    model.Add(expr == 3).OnlyEnforceIf(b3);

    // At least one bi is true. (we could use a sum == 1).
    model.AddBoolOr(new ILiteral[] { b0, b2, b3 });

    // Search for x values in increasing order.
    model.AddDecisionStrategy(
        new IntVar[] { x },
        DecisionStrategyProto.Types.VariableSelectionStrategy.ChooseFirst,
        DecisionStrategyProto.Types.DomainReductionStrategy.SelectMinValue);

    // Create the solver.
    CpSolver solver = new CpSolver();

    // Force solver to follow the decision strategy exactly.
    solver.StringParameters = "search_branching:FIXED_SEARCH";

    VarArraySolutionPrinter cb =
        new VarArraySolutionPrinter(new IntVar[] { x, expr });
    solver.SearchAllSolutions(model, cb);
  }
}
```
