[home](README.md) | [boolean logic](boolean_logic.md) | [integer arithmetic](integer_arithmetic.md) | [channeling constraints](channeling.md) | [scheduling](scheduling.md) | [Using the CP-SAT solver](solver.md) | [Model manipulation](model.md) | [Troubleshooting](troubleshooting.md) | [Python API](https://or-tools.github.io/docs/pdoc/ortools/sat/python/cp_model.html)
----------------- | --------------------------------- | ------------------------------------------- | --------------------------------------- | --------------------------- | ------------------------------------ | ------------------------------ | ------------------------------------- | -----------------------------------------------------------------------------------
# Integer arithmetic recipes for the CP-SAT solver.

https://developers.google.com/optimization/

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
-   **Go**: `x := model.NewIntVar(0, 10).WithName("x")`

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
-   **Go**: `model.NewIntVarFromDomain(cpmodel.FromValues([]int64{1, 3, 4, 6})`

Variables can also be created using a list of intervals. Below, the variable
created is constrained to be 1, 2, 4, 5, or 6:

-   **C++**: `model.NewIntVar(Domain::FromIntervals({{1, 2}, {4, 6}});`
-   **Python**: `model.NewIntVarFromDomain(cp_model.Domain.FromIntervals([[1,
    2], [4, 6]]), 'x')`
-   **Java**: `model.newIntVarFromDomain(Domain.fromIntervals(new long[][] {{1,
    2}, {4, 6}}), "x");`
-   **C#**: `model.NewIntVarFromDomain(Domain.FromIntervals(new long[][] { new
    long[] {1, 2}, new long[] {4, 6} }), "x");`
-   **Go**: `model.NewIntVarFromDomain(cpmodel.FromIntervals(
    []cpmodel.ClosedInterval{{1, 2}, {4, 6}}))`

### Boolean variables

To create a Boolean variable, use the `NewBoolVar` method. Note that Boolean
variables are typed differently than integer variables, and that this type is
not uniform across languages.

-   **C++**: `BoolVar x = model.NewBoolVar().WithName("x");`
-   **Python**: `x = model.NewBoolVar('x')`
-   **Java**: `Literal x = model.newBoolVar("x");`
-   **C#**: `ILiteral x = model.NewBoolVar("x");`
-   **Go**: `x := model.NewBoolVar().WithName("x")`

### Other methods

To exclude a single value, use ranges combined with int64min and int64max
values, e.g., `[[int64min, -3], [-1, int64max]]`, or use the `Complement`
method.

To create a variable with a single value domain, use the `NewConstant()` API (or
`newConstant()` in Java).

## Linear constraints

### C++, Java, and Go linear constraints and linear expressions

**C++**, **Java**, and **Go** APIs do not use arithmetic operators (+, \*, -,
<=...). Linear constraints are created using a method of the model factory, such
as `cp_model.AddEquality(x, 3)` in C++, `cp_model.addGreaterOrEqual(x, 10)` in
Java, or `model.AddLessThan(x, cpmodel.NewConstant(5))` in Go.

Furthermore, helper methods can be used to create sums and scalar products like
`LinearExpr::Sum({x, y, z})` in C++, `LinearExpr.weightedSum(new IntVar[] {x, y,
z}, new long[] {1, 2, 3})` in Java, and `cpmodel.NewLinearExpr().AddSum(x, y,
z)` in Go.

### Python and C\# linear constraints and linear expressions

**Python** and **C\#** CP-SAT APIs support general linear arithmetic (+, \*, -,
==, >=, >, <, <=, !=). You need to use the Add method of the cp_model, as in
`cp_model.Add(x + y != 3)`.

### Generic linear constraint

in **all languages**, the cp_model factory offers a generic method to constrain
a linear expression to be in a domain. This is used in the step function
examples below.

### Limitations

-   Everything must be linear. Multiplying two variables is not supported with
    this API; instead, `model.AddMultiplicationEquality()` must be used.

-   In C++, there is a typing issue when using an array of Boolean variables in
    a sum or a scalar product. Use the `LinearExpr.BooleanSum()` method instead.

-   The Python construct `sum()` is supported, but `min()`, `max()` or any
    `numpy` constructs like `np.unique()` are not.

## Rabbits and Pheasants examples

Let's solve a simple children's puzzle: the Rabbits and Pheasants problem.

In a field of rabbits and pheasants, there are 20 heads and 56 legs. How many
rabbits and pheasants are there?

### Python code

```python
# Snippet from ortools/sat/samples/rabbits_and_pheasants_sat.py
"""Rabbits and Pheasants quizz."""

from ortools.sat.python import cp_model


def rabbits_and_pheasants_sat():
  """Solves the rabbits + pheasants problem."""
  model = cp_model.CpModel()

  r = model.new_int_var(0, 100, 'r')
  p = model.new_int_var(0, 100, 'p')

  # 20 heads.
  model.add(r + p == 20)
  # 56 legs.
  model.add(4 * r + 2 * p == 56)

  # Solves and prints out the solution.
  solver = cp_model.CpSolver()
  status = solver.solve(model)

  if status == cp_model.OPTIMAL:
    print(f'{solver.value(r)} rabbits and {solver.value(p)} pheasants')


rabbits_and_pheasants_sat()
```

### C++ code

```cpp
// Snippet from ortools/sat/samples/rabbits_and_pheasants_sat.cc
#include <stdlib.h>

#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "absl/base/log_severity.h"
#include "absl/log/globals.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

void RabbitsAndPheasantsSat() {
  CpModelBuilder cp_model;

  const Domain all_animals(0, 20);
  const IntVar rabbits = cp_model.NewIntVar(all_animals).WithName("rabbits");
  const IntVar pheasants =
      cp_model.NewIntVar(all_animals).WithName("pheasants");

  cp_model.AddEquality(rabbits + pheasants, 20);
  cp_model.AddEquality(4 * rabbits + 2 * pheasants, 56);

  const CpSolverResponse response = Solve(cp_model.Build());

  if (response.status() == CpSolverStatus::OPTIMAL) {
    // Get the value of x in the solution.
    LOG(INFO) << SolutionIntegerValue(response, rabbits) << " rabbits, and "
              << SolutionIntegerValue(response, pheasants) << " pheasants";
  }
}

}  // namespace sat
}  // namespace operations_research

int main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  operations_research::sat::RabbitsAndPheasantsSat();
  return EXIT_SUCCESS;
}
```

### Java code

```java
// Snippet from ortools/sat/samples/RabbitsAndPheasantsSat.java
package com.google.ortools.sat.samples;

import com.google.ortools.Loader;
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
  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    // Creates the model.
    CpModel model = new CpModel();
    // Creates the variables.
    IntVar r = model.newIntVar(0, 100, "r");
    IntVar p = model.newIntVar(0, 100, "p");
    // 20 heads.
    model.addEquality(LinearExpr.newBuilder().add(r).add(p), 20);
    // 56 legs.
    model.addEquality(LinearExpr.newBuilder().addTerm(r, 4).addTerm(p, 2), 56);

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.solve(model);

    if (status == CpSolverStatus.OPTIMAL) {
      System.out.println(solver.value(r) + " rabbits, and " + solver.value(p) + " pheasants");
    }
  }
}
```

### C\# code

```csharp
// Snippet from ortools/sat/samples/RabbitsAndPheasantsSat.cs
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

        if (status == CpSolverStatus.Optimal)
        {
            Console.WriteLine(solver.Value(r) + " rabbits, and " + solver.Value(p) + " pheasants");
        }
    }
}

```

### Go code

```go
// Snippet from ortools/sat/samples/rabbits_and_pheasants_sat.go
// The rabbits_and_pheasants_sat command is an example of a simple sat program that
// solves the rabbits and pheasants problem.
package main

import (
	"fmt"

	log "github.com/golang/glog"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"

	cmpb "github.com/google/or-tools/ortools/sat/proto/cpmodel"
)

const numAnimals = 20

func rabbitsAndPheasants() error {
	model := cpmodel.NewCpModelBuilder()

	allAnimals := cpmodel.NewDomain(0, numAnimals)
	rabbits := model.NewIntVarFromDomain(allAnimals).WithName("rabbits")
	pheasants := model.NewIntVarFromDomain(allAnimals).WithName("pheasants")

	model.AddEquality(cpmodel.NewLinearExpr().AddSum(rabbits, pheasants), cpmodel.NewConstant(numAnimals))
	model.AddEquality(cpmodel.NewLinearExpr().AddTerm(rabbits, 4).AddTerm(pheasants, 2), cpmodel.NewConstant(56))

	m, err := model.Model()
	if err != nil {
		return fmt.Errorf("failed to instantiate the CP model: %w", err)
	}
	response, err := cpmodel.SolveCpModel(m)
	if err != nil {
		return fmt.Errorf("failed to solve the model: %w", err)
	}

	switch response.GetStatus() {
	case cmpb.CpSolverStatus_OPTIMAL, cmpb.CpSolverStatus_FEASIBLE:
		fmt.Printf("There are %d rabbits and %d pheasants.\n",
			cpmodel.SolutionIntegerValue(response, rabbits),
			cpmodel.SolutionIntegerValue(response, pheasants))
	default:
		fmt.Println("No solution found.")
	}

	return nil
}

func main() {
	if err := rabbitsAndPheasants(); err != nil {
		log.Exitf("rabbitsAndPheasants returned with error: %v", err)
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

```
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
```

### Python code

```python
# Snippet from ortools/sat/samples/earliness_tardiness_cost_sample_sat.py
"""Encodes a convex piecewise linear function."""


from ortools.sat.python import cp_model


class VarArraySolutionPrinter(cp_model.CpSolverSolutionCallback):
  """Print intermediate solutions."""

  def __init__(self, variables: list[cp_model.IntVar]):
    cp_model.CpSolverSolutionCallback.__init__(self)
    self.__variables = variables

  def on_solution_callback(self) -> None:
    for v in self.__variables:
      print(f'{v}={self.value(v)}', end=' ')
    print()


def earliness_tardiness_cost_sample_sat():
  """Encode the piecewise linear expression."""

  earliness_date = 5  # ed.
  earliness_cost = 8
  lateness_date = 15  # ld.
  lateness_cost = 12

  # Model.
  model = cp_model.CpModel()

  # Declare our primary variable.
  x = model.new_int_var(0, 20, 'x')

  # Create the expression variable and implement the piecewise linear function.
  #
  #  \        /
  #   \______/
  #   ed    ld
  #
  large_constant = 1000
  expr = model.new_int_var(0, large_constant, 'expr')

  # First segment.
  s1 = model.new_int_var(-large_constant, large_constant, 's1')
  model.add(s1 == earliness_cost * (earliness_date - x))

  # Second segment.
  s2 = 0

  # Third segment.
  s3 = model.new_int_var(-large_constant, large_constant, 's3')
  model.add(s3 == lateness_cost * (x - lateness_date))

  # Link together expr and x through s1, s2, and s3.
  model.add_max_equality(expr, [s1, s2, s3])

  # Search for x values in increasing order.
  model.add_decision_strategy(
      [x], cp_model.CHOOSE_FIRST, cp_model.SELECT_MIN_VALUE
  )

  # Create a solver and solve with a fixed search.
  solver = cp_model.CpSolver()

  # Force the solver to follow the decision strategy exactly.
  solver.parameters.search_branching = cp_model.FIXED_SEARCH
  # Enumerate all solutions.
  solver.parameters.enumerate_all_solutions = True

  # Search and print out all solutions.
  solution_printer = VarArraySolutionPrinter([x, expr])
  solver.solve(model, solution_printer)


earliness_tardiness_cost_sample_sat()
```

### C++ code

```cpp
// Snippet from ortools/sat/samples/earliness_tardiness_cost_sample_sat.cc
#include <stdlib.h>

#include <cstdint>

#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "absl/base/log_severity.h"
#include "absl/log/globals.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

void EarlinessTardinessCostSampleSat() {
  const int64_t kEarlinessDate = 5;
  const int64_t kEarlinessCost = 8;
  const int64_t kLatenessDate = 15;
  const int64_t kLatenessCost = 12;

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
  const int64_t kLargeConstant = 1000;
  const IntVar expr = cp_model.NewIntVar({0, kLargeConstant});

  // Link together expr and x through the 3 segments.
  cp_model.AddMaxEquality(expr, {(kEarlinessDate - x) * kEarlinessCost, 0,
                                 (x - kLatenessDate) * kLatenessCost});

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

int main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  operations_research::sat::EarlinessTardinessCostSampleSat();
  return EXIT_SUCCESS;
}
```

### Java code

```java
// Snippet from ortools/sat/samples/EarlinessTardinessCostSampleSat.java
package com.google.ortools.sat.samples;

import com.google.ortools.Loader;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.DecisionStrategyProto;
import com.google.ortools.sat.SatParameters;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverSolutionCallback;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.LinearExpr;

/** Encode the piecewise linear expression. */
public class EarlinessTardinessCostSampleSat {
  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
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

    // Link together expr and the 3 segment.
    // First segment: y == earlinessCost * (earlinessDate - x).
    // Second segment: y = 0
    // Third segment: y == latenessCost * (x - latenessDate).
    model.addMaxEquality(
        expr,
        new LinearExpr[] {
          LinearExpr.newBuilder()
              .addTerm(x, -earlinessCost)
              .add(earlinessCost * earlinessDate)
              .build(),
          LinearExpr.constant(0),
          LinearExpr.newBuilder().addTerm(x, latenessCost).add(-latenessCost * latenessDate).build()
        });

    // Search for x values in increasing order.
    model.addDecisionStrategy(
        new IntVar[] {x},
        DecisionStrategyProto.VariableSelectionStrategy.CHOOSE_FIRST,
        DecisionStrategyProto.DomainReductionStrategy.SELECT_MIN_VALUE);

    // Create the solver.
    CpSolver solver = new CpSolver();

    // Force the solver to follow the decision strategy exactly.
    solver.getParameters().setSearchBranching(SatParameters.SearchBranching.FIXED_SEARCH);
    // Tell the solver to enumerate all solutions.
    solver.getParameters().setEnumerateAllSolutions(true);

    // Solve the problem with the printer callback.
    CpSolverStatus unusedStatus =
        solver.solve(
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

```csharp
// Snippet from ortools/sat/samples/EarlinessTardinessCostSampleSat.cs
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
                Console.Write(String.Format("{0}={1} ", v.ToString(), Value(v)));
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

        // Link together expr and x through s1, s2, and s3.
        model.AddMaxEquality(expr, new LinearExpr[] { earliness_cost * (earliness_date - x), model.NewConstant(0),
                                                      lateness_cost * (x - lateness_date) });

        // Search for x values in increasing order.
        model.AddDecisionStrategy(new IntVar[] { x }, DecisionStrategyProto.Types.VariableSelectionStrategy.ChooseFirst,
                                  DecisionStrategyProto.Types.DomainReductionStrategy.SelectMinValue);

        // Create the solver.
        CpSolver solver = new CpSolver();

        // Force solver to follow the decision strategy exactly.
        // Tell the solver to search for all solutions.
        solver.StringParameters = "search_branching:FIXED_SEARCH, enumerate_all_solutions:true";

        VarArraySolutionPrinter cb = new VarArraySolutionPrinter(new IntVar[] { x, expr });
        solver.Solve(model, cb);
    }
}
```

### Go code

```go
// Snippet from ortools/sat/samples/earliness_tardiness_cost_sample_sat.go
// The earliness_tardiness_cost_sample_sat command is an example of an implementation of a convex
// piecewise linear function.
package main

import (
	"fmt"

	log "github.com/golang/glog"
	"google.golang.org/protobuf/proto"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"

	cmpb "github.com/google/or-tools/ortools/sat/proto/cpmodel"
	sppb "github.com/google/or-tools/ortools/sat/proto/satparameters"
)

const (
	earlinessDate = 5
	earlinessCost = 8
	latenessDate  = 15
	latenessCost  = 12
	largeConstant = 1000
)

func earlinessTardinessCostSampleSat() error {
	// Create the CP-SAT model.
	model := cpmodel.NewCpModelBuilder()

	// Declare our primary variable.
	x := model.NewIntVar(0, 20)

	// Create the expression variable and implement the piecewise linear function.
	//
	//  \        /
	//   \______/
	//   ed    ld
	//
	expr := model.NewIntVar(0, largeConstant)

	// Link together expr and x through the 3 segments.
	firstSegment := cpmodel.NewConstant(earlinessDate*earlinessCost).AddTerm(x, -earlinessCost)
	secondSegment := cpmodel.NewConstant(0)
	thirdSegment := cpmodel.NewConstant(-latenessDate*latenessCost).AddTerm(x, latenessCost)
	model.AddMaxEquality(expr, firstSegment, secondSegment, thirdSegment)

	// Search for x values in increasing order.
	model.AddDecisionStrategy([]cpmodel.IntVar{x}, cmpb.DecisionStrategyProto_CHOOSE_FIRST, cmpb.DecisionStrategyProto_SELECT_MIN_VALUE)

	// Create a solver and solve with a fixed search.
	m, err := model.Model()
	if err != nil {
		return fmt.Errorf("failed to instantiate the CP model: %w", err)
	}
		params := &sppb.SatParameters{
		FillAdditionalSolutionsInResponse: proto.Bool(true),
		EnumerateAllSolutions:             proto.Bool(true),
		SolutionPoolSize:                  proto.Int32(21),
		SearchBranching:                   sppb.SatParameters_FIXED_SEARCH.Enum(),
	}
	response, err := cpmodel.SolveCpModelWithParameters(m, params)
	if err != nil {
		return fmt.Errorf("failed to solve the model: %w", err)
	}

	fmt.Printf("Status: %v\n", response.GetStatus())

	for _, additionalSolution := range response.GetAdditionalSolutions() {
		vs := additionalSolution.GetValues()
		fmt.Printf("x= %v expr= %v\n", vs[x.Index()], vs[expr.Index()])
	}

	return nil
}

func main() {
	if err := earlinessTardinessCostSampleSat(); err != nil {
		log.Exitf("earlinessTardinessCostSampleSat returned with error: %v", err)
	}
}

```

## Step function.

Let's encode a step function. We will use one Boolean variable per step value,
and filter the admissible domain of the input variable with this variable.

The following samples output:

```
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
```

### Python code

```python
# Snippet from ortools/sat/samples/step_function_sample_sat.py
"""Implements a step function."""

from ortools.sat.python import cp_model


class VarArraySolutionPrinter(cp_model.CpSolverSolutionCallback):
  """Print intermediate solutions."""

  def __init__(self, variables: list[cp_model.IntVar]):
    cp_model.CpSolverSolutionCallback.__init__(self)
    self.__variables = variables

  def on_solution_callback(self) -> None:
    for v in self.__variables:
      print(f'{v}={self.value(v)}', end=' ')
    print()


def step_function_sample_sat():
  """Encode the step function."""

  # Model.
  model = cp_model.CpModel()

  # Declare our primary variable.
  x = model.new_int_var(0, 20, 'x')

  # Create the expression variable and implement the step function
  # Note it is not defined for x == 2.
  #
  #        -               3
  # -- --      ---------   2
  #                        1
  #      -- ---            0
  # 0 ================ 20
  #
  expr = model.new_int_var(0, 3, 'expr')

  # expr == 0 on [5, 6] U [8, 10]
  b0 = model.new_bool_var('b0')
  model.add_linear_expression_in_domain(
      x, cp_model.Domain.from_intervals([(5, 6), (8, 10)])
  ).only_enforce_if(b0)
  model.add(expr == 0).only_enforce_if(b0)

  # expr == 2 on [0, 1] U [3, 4] U [11, 20]
  b2 = model.new_bool_var('b2')
  model.add_linear_expression_in_domain(
      x, cp_model.Domain.from_intervals([(0, 1), (3, 4), (11, 20)])
  ).only_enforce_if(b2)
  model.add(expr == 2).only_enforce_if(b2)

  # expr == 3 when x == 7
  b3 = model.new_bool_var('b3')
  model.add(x == 7).only_enforce_if(b3)
  model.add(expr == 3).only_enforce_if(b3)

  # At least one bi is true. (we could use an exactly one constraint).
  model.add_bool_or(b0, b2, b3)

  # Search for x values in increasing order.
  model.add_decision_strategy(
      [x], cp_model.CHOOSE_FIRST, cp_model.SELECT_MIN_VALUE
  )

  # Create a solver and solve with a fixed search.
  solver = cp_model.CpSolver()

  # Force the solver to follow the decision strategy exactly.
  solver.parameters.search_branching = cp_model.FIXED_SEARCH
  # Enumerate all solutions.
  solver.parameters.enumerate_all_solutions = True

  # Search and print out all solutions.
  solution_printer = VarArraySolutionPrinter([x, expr])
  solver.solve(model, solution_printer)


step_function_sample_sat()
```

### C++ code

```cpp
// Snippet from ortools/sat/samples/step_function_sample_sat.cc
#include <stdlib.h>

#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "absl/base/log_severity.h"
#include "absl/log/globals.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/sorted_interval_list.h"

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

  // At least one bi is true. (we could use an exactly one constraint).
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

int main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  operations_research::sat::StepFunctionSampleSat();
  return EXIT_SUCCESS;
}
```

### Java code

```java
// Snippet from ortools/sat/samples/StepFunctionSampleSat.java
package com.google.ortools.sat.samples;

import com.google.ortools.Loader;
import com.google.ortools.sat.CpSolverStatus;
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
  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
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
    // Tell the solver to enumerate all solutions.
    solver.getParameters().setEnumerateAllSolutions(true);

    // Solve the problem with the printer callback.
    CpSolverStatus unusedStatus =
        solver.solve(
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

```csharp
// Snippet from ortools/sat/samples/StepFunctionSampleSat.cs
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
                Console.Write(String.Format("{0}={1} ", v.ToString(), Value(v)));
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
        model.AddLinearExpressionInDomain(x, Domain.FromValues(new long[] { 5, 6, 8, 9, 10 })).OnlyEnforceIf(b0);
        model.Add(expr == 0).OnlyEnforceIf(b0);

        // expr == 2 on [0, 1] U [3, 4] U [11, 20]
        ILiteral b2 = model.NewBoolVar("b2");
        model
            .AddLinearExpressionInDomain(
                x,
                Domain.FromIntervals(new long[][] { new long[] { 0, 1 }, new long[] { 3, 4 }, new long[] { 11, 20 } }))
            .OnlyEnforceIf(b2);
        model.Add(expr == 2).OnlyEnforceIf(b2);

        // expr == 3 when x == 7
        ILiteral b3 = model.NewBoolVar("b3");
        model.Add(x == 7).OnlyEnforceIf(b3);
        model.Add(expr == 3).OnlyEnforceIf(b3);

        // At least one bi is true. (we could use a sum == 1).
        model.AddBoolOr(new ILiteral[] { b0, b2, b3 });

        // Search for x values in increasing order.
        model.AddDecisionStrategy(new IntVar[] { x }, DecisionStrategyProto.Types.VariableSelectionStrategy.ChooseFirst,
                                  DecisionStrategyProto.Types.DomainReductionStrategy.SelectMinValue);

        // Create the solver.
        CpSolver solver = new CpSolver();

        // Force solver to follow the decision strategy exactly.
        // Tells the solver to enumerate all solutions.
        solver.StringParameters = "search_branching:FIXED_SEARCH, enumerate_all_solutions:true";

        VarArraySolutionPrinter cb = new VarArraySolutionPrinter(new IntVar[] { x, expr });
        solver.Solve(model, cb);
    }
}
```

### Go code

```go
// Snippet from ortools/sat/samples/step_function_sample_sat.go
// The step_function_sample_sat command is an example of an implementation of a step function.
package main

import (
	"fmt"

	log "github.com/golang/glog"
	"google.golang.org/protobuf/proto"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"

	cmpb "github.com/google/or-tools/ortools/sat/proto/cpmodel"
	sppb "github.com/google/or-tools/ortools/sat/proto/satparameters"
)

func stepFunctionSampleSat() error {
	// Create the CP-SAT model.
	model := cpmodel.NewCpModelBuilder()

	// Declare our primary variable.
	x := model.NewIntVar(0, 20)

	// Create the expression variable and implement the step function
	// Note it is not defined for var == 2.
	//
	//        -               3
	// -- --      ---------   2
	//                        1
	//      -- ---            0
	//           1         2
	// 012345678901234567890
	//
	expr := model.NewIntVar(0, 3)

	// expr == 0 on [5, 6] U [8, 10]
	b0 := model.NewBoolVar()
	d0 := cpmodel.FromValues([]int64{5, 6, 8, 9, 10})
	model.AddLinearConstraintForDomain(x, d0).OnlyEnforceIf(b0)
	model.AddEquality(expr, cpmodel.NewConstant(0)).OnlyEnforceIf(b0)

	// expr == 2 on [0, 1] U [3, 4] U [11, 20]
	b2 := model.NewBoolVar()
	d2 := cpmodel.FromIntervals([]cpmodel.ClosedInterval{{Start: 0, End: 1}, {Start: 3, End: 4}, {Start: 11, End: 20}})
	model.AddLinearConstraintForDomain(x, d2).OnlyEnforceIf(b2)
	model.AddEquality(expr, cpmodel.NewConstant(2)).OnlyEnforceIf(b2)

	// expr = 3 when x = 7
	b3 := model.NewBoolVar()
	model.AddEquality(x, cpmodel.NewConstant(7)).OnlyEnforceIf(b3)
	model.AddEquality(expr, cpmodel.NewConstant(3)).OnlyEnforceIf(b3)

	// At least one Boolean variable is true.
	model.AddBoolOr(b0, b2, b3)

	// Search for x values in increasing order.
	model.AddDecisionStrategy([]cpmodel.IntVar{x}, cmpb.DecisionStrategyProto_CHOOSE_FIRST, cmpb.DecisionStrategyProto_SELECT_MIN_VALUE)

	// Create a solver and solve with fixed search.
	m, err := model.Model()
	if err != nil {
		return fmt.Errorf("failed to instantiate the CP model: %w", err)
	}
		params := &sppb.SatParameters{
		FillAdditionalSolutionsInResponse: proto.Bool(true),
		EnumerateAllSolutions:             proto.Bool(true),
		SolutionPoolSize:                  proto.Int32(21),
		SearchBranching:                   sppb.SatParameters_FIXED_SEARCH.Enum(),
	}
	response, err := cpmodel.SolveCpModelWithParameters(m, params)
	if err != nil {
		return fmt.Errorf("failed to solve the model: %w", err)
	}

	fmt.Printf("Status: %v\n", response.GetStatus())

	for _, additionalSolution := range response.GetAdditionalSolutions() {
		vs := additionalSolution.GetValues()
		fmt.Printf("x= %v expr= %v\n", vs[x.Index()], vs[expr.Index()])
	}

	return nil
}

func main() {
	if err := stepFunctionSampleSat(); err != nil {
		log.Exitf("stepFunctionSampleSat returned with error: %v", err)
	}
}

```

## Product of a Boolean variable and an integer variable

This sample implements an helper function that will take two variables (Boolean
and integer), and will return a new integer variable that is constrained to be
equal to the product of the two variables.

The following samples output:

```
x=1 b=0 p=0
x=2 b=0 p=0
x=3 b=0 p=0
x=5 b=0 p=0
x=6 b=0 p=0
x=7 b=0 p=0
x=9 b=0 p=0
x=10 b=0 p=0
x=1 b=1 p=1
x=2 b=1 p=2
x=3 b=1 p=3
x=5 b=1 p=5
x=6 b=1 p=6
x=7 b=1 p=7
x=9 b=1 p=9
x=10 b=1 p=10
```

### Python code

```python
# Snippet from ortools/sat/samples/bool_and_int_var_product_sample_sat.py
"""Code sample that encodes the product of a Boolean and an integer variable."""

from ortools.sat.python import cp_model


class VarArraySolutionPrinter(cp_model.CpSolverSolutionCallback):
  """Print intermediate solutions."""

  def __init__(self, variables: list[cp_model.IntVar]):
    cp_model.CpSolverSolutionCallback.__init__(self)
    self.__variables = variables

  def on_solution_callback(self) -> None:
    for v in self.__variables:
      print(f'{v}={self.value(v)}', end=' ')
    print()


def build_product_var(
    model: cp_model.CpModel, b: cp_model.IntVar, x: cp_model.IntVar, name: str
) -> cp_model.IntVar:
  """Builds the product of a Boolean variable and an integer variable."""
  p = model.new_int_var_from_domain(
      cp_model.Domain.from_flat_intervals(x.proto.domain).union_with(
          cp_model.Domain(0, 0)
      ),
      name,
  )
  model.add(p == x).only_enforce_if(b)
  model.add(p == 0).only_enforce_if(~b)
  return p


def bool_and_int_var_product_sample_sat():
  """Encoding of the product of two Boolean variables.

  p == x * y, which is the same as p <=> x and y
  """
  model = cp_model.CpModel()
  b = model.new_bool_var('b')
  x = model.new_int_var_from_domain(
      cp_model.Domain.from_values([1, 2, 3, 5, 6, 7, 9, 10]), 'x'
  )
  p = build_product_var(model, b, x, 'p')

  # Search for x and b values in increasing order.
  model.add_decision_strategy(
      [b, x], cp_model.CHOOSE_FIRST, cp_model.SELECT_MIN_VALUE
  )

  # Create a solver and solve.
  solver = cp_model.CpSolver()
  solution_printer = VarArraySolutionPrinter([x, b, p])
  solver.parameters.enumerate_all_solutions = True
  solver.parameters.search_branching = cp_model.FIXED_SEARCH
  solver.solve(model, solution_printer)


bool_and_int_var_product_sample_sat()
```

## Scanning the domain of variables.

In this example, we will implement the all_different_except_0 constraint. This
constraint is useful as it expresses that 2 active assignment should be
different, but we do not care when they are inactive (represented by being
assigned a zero value).

To implement this constraint, we will collect all values in the initial domain
of all variables and attach Boolean variables for each of them. This requires
reading back the values from the model.

### Python code

```python
# Snippet from ortools/sat/samples/all_different_except_zero_sample_sat.py
"""Implements AllDifferentExcept0 using atomic constraints."""

import collections

from ortools.sat.python import cp_model


def all_different_except_0():
  """Encode the AllDifferentExcept0 constraint."""

  # Model.
  model = cp_model.CpModel()

  # Declare our primary variable.
  x = [model.new_int_var(0, 10, f'x{i}') for i in range(5)]

  # Expand the AllDifferentExcept0 constraint.
  variables_per_value = collections.defaultdict(list)
  all_values = set()

  for var in x:
    all_encoding_literals = []
    # Domains of variables are represented by flat intervals.
    for i in range(0, len(var.proto.domain), 2):
      start = var.proto.domain[i]
      end = var.proto.domain[i + 1]
      for value in range(start, end + 1):  # Intervals are inclusive.
        # Create the literal attached to var == value.
        bool_var = model.new_bool_var(f'{var} == {value}')
        model.add(var == value).only_enforce_if(bool_var)

        # Collect all encoding literals for a given variable.
        all_encoding_literals.append(bool_var)

        # Collect all encoding literals for a given value.
        variables_per_value[value].append(bool_var)

        # Collect all different values.
        all_values.add(value)

    # One variable must have exactly one value.
    model.add_exactly_one(all_encoding_literals)

  # Add the all_different constraints.
  for value, literals in variables_per_value.items():
    if value == 0:
      continue
    model.add_at_most_one(literals)

  model.add(x[0] == 0)
  model.add(x[1] == 0)

  model.maximize(sum(x))

  # Create a solver and solve.
  solver = cp_model.CpSolver()
  status = solver.solve(model)

  # Checks and prints the output.
  if status == cp_model.OPTIMAL:
    print(f'Optimal solution: {solver.objective_value}, expected: 27.0')
  elif status == cp_model.FEASIBLE:
    print(f'Feasible solution: {solver.objective_value}, optimal 27.0')
  elif status == cp_model.INFEASIBLE:
    print('The model is infeasible')
  else:
    print('Something went wrong. Please check the status and the log')


all_different_except_0()
```
