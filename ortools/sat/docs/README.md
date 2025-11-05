[home](README.md) | [boolean logic](boolean_logic.md) | [integer arithmetic](integer_arithmetic.md) | [channeling constraints](channeling.md) | [scheduling](scheduling.md) | [Using the CP-SAT solver](solver.md) | [Model manipulation](model.md) | [Troubleshooting](troubleshooting.md) | [Python API](https://or-tools.github.io/docs/pdoc/ortools/sat/python/cp_model.html)
----------------- | --------------------------------- | ------------------------------------------- | --------------------------------------- | --------------------------- | ------------------------------------ | ------------------------------ | ------------------------------------- | -----------------------------------------------------------------------------------
# Using the CP-SAT solver

https://developers.google.com/optimization/cp/cp_solver

## Documentation structure

This document presents modeling recipes for the CP-SAT solver.

Code samples are given in C++, Python, Java, C#, and Go. Each language has
different requirements for the code samples.

## Searching for one (optimal) solution of a CP-SAT model

By default, searching for one solution will return the first solution found if
the model has no objective, or the optimal solution if the model has an
objective.

### Python code samples

The Python interface to the CP-SAT solver is implemented using two classes.

*   The **CpModel** class proposes modeling methods that creates variables, or
    add constraints. This class completely hides the underlying *CpModelProto*
    used to store the model.
*   The **CpSolver** class encapsulates the solve API and offers helpers to
    access the solution found by the solve.

```python
# Snippet from ortools/sat/samples/simple_sat_program.py
"""Simple solve."""
from ortools.sat.python import cp_model


def simple_sat_program():
  """Minimal CP-SAT example to showcase calling the solver."""
  # Creates the model.
  model = cp_model.CpModel()

  # Creates the variables.
  num_vals = 3
  x = model.new_int_var(0, num_vals - 1, 'x')
  y = model.new_int_var(0, num_vals - 1, 'y')
  z = model.new_int_var(0, num_vals - 1, 'z')

  # Creates the constraints.
  model.add(x != y)

  # Creates a solver and solves the model.
  solver = cp_model.CpSolver()
  status = solver.solve(model)

  if status == cp_model.OPTIMAL or status == cp_model.FEASIBLE:
    print(f'x = {solver.value(x)}')
    print(f'y = {solver.value(y)}')
    print(f'z = {solver.value(z)}')
  else:
    print('No solution found.')


simple_sat_program()
```

### C++ code samples

The interface to the C++ CP-SAT solver is implemented through the
**CpModelBuilder** class described in
*ortools/sat/cp_model.h*. This class is just a helper to
fill in the cp_model protobuf.

Calling Solve() method will return a CpSolverResponse protobuf that contains the
solve status, the values for each variable in the model if solve was successful,
and some metrics.

```cpp
// Snippet from ortools/sat/samples/simple_sat_program.cc
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

void SimpleSatProgram() {
  CpModelBuilder cp_model;

  const Domain domain(0, 2);
  const IntVar x = cp_model.NewIntVar(domain).WithName("x");
  const IntVar y = cp_model.NewIntVar(domain).WithName("y");
  const IntVar z = cp_model.NewIntVar(domain).WithName("z");

  cp_model.AddNotEqual(x, y);

  // Solving part.
  const CpSolverResponse response = Solve(cp_model.Build());

  if (response.status() == CpSolverStatus::OPTIMAL ||
      response.status() == CpSolverStatus::FEASIBLE) {
    // Get the value of x in the solution.
    LOG(INFO) << "x = " << SolutionIntegerValue(response, x);
    LOG(INFO) << "y = " << SolutionIntegerValue(response, y);
    LOG(INFO) << "z = " << SolutionIntegerValue(response, z);
  } else {
    LOG(INFO) << "No solution found.";
  }
}

}  // namespace sat
}  // namespace operations_research

int main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  operations_research::sat::SimpleSatProgram();
  return EXIT_SUCCESS;
}
```

### Java code samples

The Java code implements the same interface as the Python code, with a
**CpModel** and a **CpSolver** class.

```java
// Snippet from ortools/sat/samples/SimpleSatProgram.java
package com.google.ortools.sat.samples;
import com.google.ortools.Loader;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.IntVar;

/** Minimal CP-SAT example to showcase calling the solver. */
public final class SimpleSatProgram {
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

    // Create a solver and solve the model.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.solve(model);

    if (status == CpSolverStatus.OPTIMAL || status == CpSolverStatus.FEASIBLE) {
      System.out.println("x = " + solver.value(x));
      System.out.println("y = " + solver.value(y));
      System.out.println("z = " + solver.value(z));
    } else {
      System.out.println("No solution found.");
    }
  }

  private SimpleSatProgram() {}
}
```

### C\# code samples

The C\# code implements the same interface as the Python code, with a
**CpModel** and a **CpSolver** class.

```csharp
// Snippet from ortools/sat/samples/SimpleSatProgram.cs
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

        if (status == CpSolverStatus.Optimal || status == CpSolverStatus.Feasible)
        {
            Console.WriteLine("x = " + solver.Value(x));
            Console.WriteLine("y = " + solver.Value(y));
            Console.WriteLine("z = " + solver.Value(z));
        }
        else
        {
            Console.WriteLine("No solution found.");
        }
    }
}
```

### Go code samples

The interface to the CP-SAT solver is implemented through the **CpModelBuilder**
described in the package **cpmodel** in
*ortools/sat/go/cp_model.go*. This class is a helper to fill
in the cp_model protobuf.

Also within the **cpmodel** package is
*ortools/sat/go/cp_model.go* which provides functions to solve
the model along with helpers to access the solution found by the solver.

```go
// Snippet from ortools/sat/samples/simple_sat_program.go
// The simple_sat_program command is an example of a simple sat program.
package main

import (
	"fmt"

	log "github.com/golang/glog"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"

	cmpb "github.com/google/or-tools/ortools/sat/proto/cpmodel"
)

func simpleSatProgram() error {
	model := cpmodel.NewCpModelBuilder()

	domain := cpmodel.NewDomain(0, 2)
	x := model.NewIntVarFromDomain(domain).WithName("x")
	y := model.NewIntVarFromDomain(domain).WithName("y")
	z := model.NewIntVarFromDomain(domain).WithName("z")

	model.AddNotEqual(x, y)

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
		fmt.Printf("x = %d\n", cpmodel.SolutionIntegerValue(response, x))
		fmt.Printf("y = %d\n", cpmodel.SolutionIntegerValue(response, y))
		fmt.Printf("z = %d\n", cpmodel.SolutionIntegerValue(response, z))
	default:
		fmt.Println("No solution found.")
	}

	return nil
}

func main() {
	if err := simpleSatProgram(); err != nil {
		log.Exitf("simpleSatProgram returned with error: %v", err)
	}
}

```
