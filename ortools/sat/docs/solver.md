[home](README.md) | [boolean logic](boolean_logic.md) | [integer arithmetic](integer_arithmetic.md) | [channeling constraints](channeling.md) | [scheduling](scheduling.md) | [Using the CP-SAT solver](solver.md) | [Model manipulation](model.md) | [Troubleshooting](troubleshooting.md) | [Python API](https://google.github.io/or-tools/python/ortools/sat/python/cp_model.html)
----------------- | --------------------------------- | ------------------------------------------- | --------------------------------------- | --------------------------- | ------------------------------------ | ------------------------------ | ------------------------------------- | ---------------------------------------------------------------------------------------
# Solving a CP-SAT model

https://developers.google.com/optimization/

## Changing the parameters of the solver

The SatParameters protobuf encapsulates the set of parameters of a CP-SAT
solver. The most useful one is the time limit.

### Specifying the time limit in Python

```python
#!/usr/bin/env python3
"""Solves a problem with a time limit."""

from ortools.sat.python import cp_model


def SolveWithTimeLimitSampleSat():
    """Minimal CP-SAT example to showcase calling the solver."""
    # Creates the model.
    model = cp_model.CpModel()
    # Creates the variables.
    num_vals = 3
    x = model.new_int_var(0, num_vals - 1, "x")
    y = model.new_int_var(0, num_vals - 1, "y")
    z = model.new_int_var(0, num_vals - 1, "z")
    # Adds an all-different constraint.
    model.add(x != y)

    # Creates a solver and solves the model.
    solver = cp_model.CpSolver()

    # Sets a time limit of 10 seconds.
    solver.parameters.max_time_in_seconds = 10.0

    status = solver.solve(model)

    if status == cp_model.OPTIMAL:
        print(f"x = {solver.value(x)}")
        print(f"y = {solver.value(y)}")
        print(f"z = {solver.value(z)}")


SolveWithTimeLimitSampleSat()
```

### Specifying the time limit in C++

```cpp
#include <stdlib.h>

#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

void SolveWithTimeLimitSampleSat() {
  CpModelBuilder cp_model;

  const Domain domain(0, 2);
  const IntVar x = cp_model.NewIntVar(domain).WithName("x");
  const IntVar y = cp_model.NewIntVar(domain).WithName("y");
  const IntVar z = cp_model.NewIntVar(domain).WithName("z");

  cp_model.AddNotEqual(x, y);

  // Solving part.
  Model model;

  // Sets a time limit of 10 seconds.
  SatParameters parameters;
  parameters.set_max_time_in_seconds(10.0);
  model.Add(NewSatParameters(parameters));

  // Solve.
  const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);
  LOG(INFO) << CpSolverResponseStats(response);

  if (response.status() == CpSolverStatus::OPTIMAL) {
    LOG(INFO) << "  x = " << SolutionIntegerValue(response, x);
    LOG(INFO) << "  y = " << SolutionIntegerValue(response, y);
    LOG(INFO) << "  z = " << SolutionIntegerValue(response, z);
  }
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::SolveWithTimeLimitSampleSat();

  return EXIT_SUCCESS;
}
```

### Specifying the time limit in Java

```java
package com.google.ortools.sat.samples;

import com.google.ortools.Loader;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.IntVar;

/** Solves a problem with a time limit. */
public final class SolveWithTimeLimitSampleSat {
  public static void main(String[] args) {
    Loader.loadNativeLibraries();
    // Create the model.
    CpModel model = new CpModel();
    // Create the variables.
    int numVals = 3;

    IntVar x = model.newIntVar(0, numVals - 1, "x");
    IntVar y = model.newIntVar(0, numVals - 1, "y");
    IntVar z = model.newIntVar(0, numVals - 1, "z");
    // Create the constraint.
    model.addDifferent(x, y);

    // Create a solver and solve the model.
    CpSolver solver = new CpSolver();
    solver.getParameters().setMaxTimeInSeconds(10.0);
    CpSolverStatus status = solver.solve(model);

    if (status == CpSolverStatus.OPTIMAL) {
      System.out.println("x = " + solver.value(x));
      System.out.println("y = " + solver.value(y));
      System.out.println("z = " + solver.value(z));
    }
  }

  private SolveWithTimeLimitSampleSat() {}
}
```

### Specifying the time limit in C\#.

Parameters must be passed as string to the solver.

```cs
using System;
using Google.OrTools.Sat;

public class SolveWithTimeLimitSampleSat
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
        // Adds a different constraint.
        model.Add(x != y);

        // Creates a solver and solves the model.
        CpSolver solver = new CpSolver();

        // Adds a time limit. Parameters are stored as strings in the solver.
        solver.StringParameters = "max_time_in_seconds:10.0";

        CpSolverStatus status = solver.Solve(model);

        if (status == CpSolverStatus.Optimal)
        {
            Console.WriteLine("x = " + solver.Value(x));
            Console.WriteLine("y = " + solver.Value(y));
            Console.WriteLine("z = " + solver.Value(z));
        }
    }
}
```

## Printing intermediate solutions

In an optimization model, you can print intermediate solutions. For all
languages except Go, you need to register a callback on the solver that will be
called at each solution. For Go, callbacks are not implemented, but you can
still get the intermediate solutions in the response.

The exact implementation depends on the target language.

### Python code

```python
#!/usr/bin/env python3
"""Solves an optimization problem and displays all intermediate solutions."""

from ortools.sat.python import cp_model


# You need to subclass the cp_model.CpSolverSolutionCallback class.
class VarArrayAndObjectiveSolutionPrinter(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions."""

    def __init__(self, variables: list[cp_model.IntVar]):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__variables = variables
        self.__solution_count = 0

    def on_solution_callback(self) -> None:
        print(f"Solution {self.__solution_count}")
        print(f"  objective value = {self.objective_value}")
        for v in self.__variables:
            print(f"  {v}={self.value(v)}", end=" ")
        print()
        self.__solution_count += 1

    @property
    def solution_count(self) -> int:
        return self.__solution_count


def SolveAndPrintIntermediateSolutionsSampleSat():
    """Showcases printing intermediate solutions found during search."""
    # Creates the model.
    model = cp_model.CpModel()

    # Creates the variables.
    num_vals = 3
    x = model.new_int_var(0, num_vals - 1, "x")
    y = model.new_int_var(0, num_vals - 1, "y")
    z = model.new_int_var(0, num_vals - 1, "z")

    # Creates the constraints.
    model.add(x != y)

    model.maximize(x + 2 * y + 3 * z)

    # Creates a solver and solves.
    solver = cp_model.CpSolver()
    solution_printer = VarArrayAndObjectiveSolutionPrinter([x, y, z])
    status = solver.solve(model, solution_printer)

    print(f"Status = {solver.status_name(status)}")
    print(f"Number of solutions found: {solution_printer.solution_count}")


SolveAndPrintIntermediateSolutionsSampleSat()
```

### C++ code

```cpp
#include <stdlib.h>

#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/model.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

void SolveAndPrintIntermediateSolutionsSampleSat() {
  CpModelBuilder cp_model;

  const Domain domain(0, 2);
  const IntVar x = cp_model.NewIntVar(domain).WithName("x");
  const IntVar y = cp_model.NewIntVar(domain).WithName("y");
  const IntVar z = cp_model.NewIntVar(domain).WithName("z");

  cp_model.AddNotEqual(x, y);

  cp_model.Maximize(x + 2 * y + 3 * z);

  Model model;
  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    LOG(INFO) << "Solution " << num_solutions;
    LOG(INFO) << "  objective value = " << r.objective_value();
    LOG(INFO) << "  x = " << SolutionIntegerValue(r, x);
    LOG(INFO) << "  y = " << SolutionIntegerValue(r, y);
    LOG(INFO) << "  z = " << SolutionIntegerValue(r, z);
    num_solutions++;
  }));

  const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);

  LOG(INFO) << "Number of solutions found: " << num_solutions;
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::SolveAndPrintIntermediateSolutionsSampleSat();

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

/** Solves an optimization problem and displays all intermediate solutions. */
public class SolveAndPrintIntermediateSolutionsSampleSat {
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

    public int getSolutionCount() {
      return solutionCount;
    }

    private int solutionCount;
    private final IntVar[] variableArray;
  }

  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    // Create the model.
    CpModel model = new CpModel();

    // Create the variables.
    int numVals = 3;

    IntVar x = model.newIntVar(0, numVals - 1, "x");
    IntVar y = model.newIntVar(0, numVals - 1, "y");
    IntVar z = model.newIntVar(0, numVals - 1, "z");

    // Create the constraint.
    model.addDifferent(x, y);

    // Maximize a linear combination of variables.
    model.maximize(LinearExpr.weightedSum(new IntVar[] {x, y, z}, new long[] {1, 2, 3}));

    // Create a solver and solve the model.
    CpSolver solver = new CpSolver();
    VarArraySolutionPrinterWithObjective cb =
        new VarArraySolutionPrinterWithObjective(new IntVar[] {x, y, z});
    solver.solve(model, cb);

    System.out.println(cb.getSolutionCount() + " solutions found.");
  }
}
```

### C\# code

```cs
using System;
using Google.OrTools.Sat;

public class VarArraySolutionPrinterWithObjective : CpSolverSolutionCallback
{
    public VarArraySolutionPrinterWithObjective(IntVar[] variables)
    {
        variables_ = variables;
    }

    public override void OnSolutionCallback()
    {
        Console.WriteLine(String.Format("Solution #{0}: time = {1:F2} s", solution_count_, WallTime()));
        Console.WriteLine(String.Format("  objective value = {0}", ObjectiveValue()));
        foreach (IntVar v in variables_)
        {
            Console.WriteLine(String.Format("  {0} = {1}", v.ToString(), Value(v)));
        }
        solution_count_++;
    }

    public int SolutionCount()
    {
        return solution_count_;
    }

    private int solution_count_;
    private IntVar[] variables_;
}

public class SolveAndPrintIntermediateSolutionsSampleSat
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

        // Adds a different constraint.
        model.Add(x != y);

        // Maximizes a linear combination of variables.
        model.Maximize(x + 2 * y + 3 * z);

        // Creates a solver and solves the model.
        CpSolver solver = new CpSolver();
        VarArraySolutionPrinterWithObjective cb = new VarArraySolutionPrinterWithObjective(new IntVar[] { x, y, z });
        solver.Solve(model, cb);

        Console.WriteLine(String.Format("Number of solutions found: {0}", cb.SolutionCount()));
    }
}
```

## Searching for all solutions in a satisfiability model

In an non-optimization model, you can search for all solutions. For all
languages except Go, you need to register a callback on the solver that will be
called at each solution. For Go, callbacks are not implemented, but you can
still get the intermediate solutions in the response.

Please note that it does not work in parallel
(i. e. parameter `num_search_workers` > 1).

It also does not work if the model contains an objective.

The method will return the following:

  * *FEASIBLE* if some solutions have been found
  * *INFEASIBLE* if the solver has proved there are no solution
  * *OPTIMAL* if all solutions have been found

The exact implementation depends on the target language.

### Python code

To search for all solutions, use the Solve() method after setting the correct
parameter.

```python
#!/usr/bin/env python3
"""Code sample that solves a model and displays all solutions."""

from ortools.sat.python import cp_model


class VarArraySolutionPrinter(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions."""

    def __init__(self, variables: list[cp_model.IntVar]):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__variables = variables
        self.__solution_count = 0

    def on_solution_callback(self) -> None:
        self.__solution_count += 1
        for v in self.__variables:
            print(f"{v}={self.value(v)}", end=" ")
        print()

    @property
    def solution_count(self) -> int:
        return self.__solution_count


def SearchForAllSolutionsSampleSat():
    """Showcases calling the solver to search for all solutions."""
    # Creates the model.
    model = cp_model.CpModel()

    # Creates the variables.
    num_vals = 3
    x = model.new_int_var(0, num_vals - 1, "x")
    y = model.new_int_var(0, num_vals - 1, "y")
    z = model.new_int_var(0, num_vals - 1, "z")

    # Create the constraints.
    model.add(x != y)

    # Create a solver and solve.
    solver = cp_model.CpSolver()
    solution_printer = VarArraySolutionPrinter([x, y, z])
    # Enumerate all solutions.
    solver.parameters.enumerate_all_solutions = True
    # Solve.
    status = solver.solve(model, solution_printer)

    print(f"Status = {solver.status_name(status)}")
    print(f"Number of solutions found: {solution_printer.solution_count}")


SearchForAllSolutionsSampleSat()
```

### C++ code

To search for all solutions, a parameter of the SAT solver must be changed.

```cpp
#include <stdlib.h>

#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

void SearchAllSolutionsSampleSat() {
  CpModelBuilder cp_model;

  const Domain domain(0, 2);
  const IntVar x = cp_model.NewIntVar(domain).WithName("x");
  const IntVar y = cp_model.NewIntVar(domain).WithName("y");
  const IntVar z = cp_model.NewIntVar(domain).WithName("z");

  cp_model.AddNotEqual(x, y);

  Model model;

  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    LOG(INFO) << "Solution " << num_solutions;
    LOG(INFO) << "  x = " << SolutionIntegerValue(r, x);
    LOG(INFO) << "  y = " << SolutionIntegerValue(r, y);
    LOG(INFO) << "  z = " << SolutionIntegerValue(r, z);
    num_solutions++;
  }));

  // Tell the solver to enumerate all solutions.
  SatParameters parameters;
  parameters.set_enumerate_all_solutions(true);
  model.Add(NewSatParameters(parameters));
  const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);

  LOG(INFO) << "Number of solutions found: " << num_solutions;
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::SearchAllSolutionsSampleSat();

  return EXIT_SUCCESS;
}
```

### Java code

As in Python, CpSolver.solve() must be called after setting the correct
parameter.

```java
package com.google.ortools.sat.samples;

import com.google.ortools.Loader;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverSolutionCallback;
import com.google.ortools.sat.IntVar;

/** Code sample that solves a model and displays all solutions. */
public class SearchForAllSolutionsSampleSat {
  static class VarArraySolutionPrinter extends CpSolverSolutionCallback {
    public VarArraySolutionPrinter(IntVar[] variables) {
      variableArray = variables;
    }

    @Override
    public void onSolutionCallback() {
      System.out.printf("Solution #%d: time = %.02f s%n", solutionCount, wallTime());
      for (IntVar v : variableArray) {
        System.out.printf("  %s = %d%n", v.getName(), value(v));
      }
      solutionCount++;
    }

    public int getSolutionCount() {
      return solutionCount;
    }

    private int solutionCount;
    private final IntVar[] variableArray;
  }

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
    VarArraySolutionPrinter cb = new VarArraySolutionPrinter(new IntVar[] {x, y, z});
    // Tell the solver to enumerate all solutions.
    solver.getParameters().setEnumerateAllSolutions(true);
    // And solve.
    solver.solve(model, cb);

    System.out.println(cb.getSolutionCount() + " solutions found.");
  }
}
```

### C\# code

As in Python, CpSolver.Solve() must be called after setting the correct string
parameter.

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
                Console.WriteLine(String.Format("  {0} = {1}", v.ToString(), Value(v)));
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

public class SearchForAllSolutionsSampleSat
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

        // Adds a different constraint.
        model.Add(x != y);

        // Creates a solver and solves the model.
        CpSolver solver = new CpSolver();
        VarArraySolutionPrinter cb = new VarArraySolutionPrinter(new IntVar[] { x, y, z });
        // Search for all solutions.
        solver.StringParameters = "enumerate_all_solutions:true";
        // And solve.
        solver.Solve(model, cb);

        Console.WriteLine($"Number of solutions found: {cb.SolutionCount()}");
    }
}
```

## Stopping search early

Sometimes, you might decide that the current solution is good enough. In this
section, we will enumerate all possible combinations of three variables, and
stop after the fifth combination found.

### Python code

You can stop the search by calling StopSearch() inside of
CpSolverSolutionCallback.OnSolutionCallback().

```python
#!/usr/bin/env python3
"""Code sample that solves a model and displays a small number of solutions."""

from ortools.sat.python import cp_model


class VarArraySolutionPrinterWithLimit(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions."""

    def __init__(self, variables: list[cp_model.IntVar], limit: int):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__variables = variables
        self.__solution_count = 0
        self.__solution_limit = limit

    def on_solution_callback(self) -> None:
        self.__solution_count += 1
        for v in self.__variables:
            print(f"{v}={self.value(v)}", end=" ")
        print()
        if self.__solution_count >= self.__solution_limit:
            print(f"Stop search after {self.__solution_limit} solutions")
            self.stop_search()

    @property
    def solution_count(self) -> int:
        return self.__solution_count


def StopAfterNSolutionsSampleSat():
    """Showcases calling the solver to search for small number of solutions."""
    # Creates the model.
    model = cp_model.CpModel()
    # Creates the variables.
    num_vals = 3
    x = model.new_int_var(0, num_vals - 1, "x")
    y = model.new_int_var(0, num_vals - 1, "y")
    z = model.new_int_var(0, num_vals - 1, "z")

    # Create a solver and solve.
    solver = cp_model.CpSolver()
    solution_printer = VarArraySolutionPrinterWithLimit([x, y, z], 5)
    # Enumerate all solutions.
    solver.parameters.enumerate_all_solutions = True
    # Solve.
    status = solver.solve(model, solution_printer)
    print(f"Status = {solver.status_name(status)}")
    print(f"Number of solutions found: {solution_printer.solution_count}")
    assert solution_printer.solution_count == 5


StopAfterNSolutionsSampleSat()
```

### C++ code

Stopping search is done by registering an atomic bool on the model-owned time
limit, and setting that bool to true.

```cpp
#include <stdlib.h>

#include <atomic>

#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

void StopAfterNSolutionsSampleSat() {
  CpModelBuilder cp_model;

  const Domain domain(0, 2);
  const IntVar x = cp_model.NewIntVar(domain).WithName("x");
  const IntVar y = cp_model.NewIntVar(domain).WithName("y");
  const IntVar z = cp_model.NewIntVar(domain).WithName("z");

  Model model;

  // Tell the solver to enumerate all solutions.
  SatParameters parameters;
  parameters.set_enumerate_all_solutions(true);
  model.Add(NewSatParameters(parameters));

  // Create an atomic Boolean that will be periodically checked by the limit.
  std::atomic<bool> stopped(false);
  model.GetOrCreate<TimeLimit>()->RegisterExternalBooleanAsLimit(&stopped);

  const int kSolutionLimit = 5;
  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    LOG(INFO) << "Solution " << num_solutions;
    LOG(INFO) << "  x = " << SolutionIntegerValue(r, x);
    LOG(INFO) << "  y = " << SolutionIntegerValue(r, y);
    LOG(INFO) << "  z = " << SolutionIntegerValue(r, z);
    num_solutions++;
    if (num_solutions >= kSolutionLimit) {
      stopped = true;
      LOG(INFO) << "Stop search after " << kSolutionLimit << " solutions.";
    }
  }));
  const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);
  LOG(INFO) << "Number of solutions found: " << num_solutions;
  CHECK_EQ(num_solutions, kSolutionLimit);
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::StopAfterNSolutionsSampleSat();

  return EXIT_SUCCESS;
}
```

### Java code

Stopping search is performed by calling stopSearch() inside of
CpSolverSolutionCallback.onSolutionCallback().

```java
package com.google.ortools.sat.samples;

import com.google.ortools.Loader;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverSolutionCallback;
import com.google.ortools.sat.IntVar;

/** Code sample that solves a model and displays a small number of solutions. */
public final class StopAfterNSolutionsSampleSat {
  static class VarArraySolutionPrinterWithLimit extends CpSolverSolutionCallback {
    public VarArraySolutionPrinterWithLimit(IntVar[] variables, int limit) {
      variableArray = variables;
      solutionLimit = limit;
    }

    @Override
    public void onSolutionCallback() {
      System.out.printf("Solution #%d: time = %.02f s%n", solutionCount, wallTime());
      for (IntVar v : variableArray) {
        System.out.printf("  %s = %d%n", v.getName(), value(v));
      }
      solutionCount++;
      if (solutionCount >= solutionLimit) {
        System.out.printf("Stop search after %d solutions%n", solutionLimit);
        stopSearch();
      }
    }

    public int getSolutionCount() {
      return solutionCount;
    }

    private int solutionCount;
    private final IntVar[] variableArray;
    private final int solutionLimit;
  }

  public static void main(String[] args) {
    Loader.loadNativeLibraries();
    // Create the model.
    CpModel model = new CpModel();
    // Create the variables.
    int numVals = 3;

    IntVar x = model.newIntVar(0, numVals - 1, "x");
    IntVar y = model.newIntVar(0, numVals - 1, "y");
    IntVar z = model.newIntVar(0, numVals - 1, "z");

    // Create a solver and solve the model.
    CpSolver solver = new CpSolver();
    VarArraySolutionPrinterWithLimit cb =
        new VarArraySolutionPrinterWithLimit(new IntVar[] {x, y, z}, 5);
    // Tell the solver to enumerate all solutions.
    solver.getParameters().setEnumerateAllSolutions(true);
    // And solve.
    solver.solve(model, cb);

    System.out.println(cb.getSolutionCount() + " solutions found.");
    if (cb.getSolutionCount() != 5) {
      throw new RuntimeException("Did not stop the search correctly.");
    }
  }

  private StopAfterNSolutionsSampleSat() {}
}
```

### C\# code

Stopping search is performed by calling StopSearch() inside of
CpSolverSolutionCallback.OnSolutionCallback().

```cs
using System;
using Google.OrTools.Sat;

public class VarArraySolutionPrinterWithLimit : CpSolverSolutionCallback
{
    public VarArraySolutionPrinterWithLimit(IntVar[] variables, int solution_limit)
    {
        variables_ = variables;
        solution_limit_ = solution_limit;
    }

    public override void OnSolutionCallback()
    {
        Console.WriteLine(String.Format("Solution #{0}: time = {1:F2} s", solution_count_, WallTime()));
        foreach (IntVar v in variables_)
        {
            Console.WriteLine(String.Format("  {0} = {1}", v.ToString(), Value(v)));
        }
        solution_count_++;
        if (solution_count_ >= solution_limit_)
        {
            Console.WriteLine(String.Format("Stopping search after {0} solutions", solution_limit_));
            StopSearch();
        }
    }

    public int SolutionCount()
    {
        return solution_count_;
    }

    private int solution_count_;
    private IntVar[] variables_;
    private int solution_limit_;
}

public class StopAfterNSolutionsSampleSat
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

        // Creates a solver and solves the model.
        CpSolver solver = new CpSolver();
        VarArraySolutionPrinterWithLimit cb = new VarArraySolutionPrinterWithLimit(new IntVar[] { x, y, z }, 5);
        solver.StringParameters = "enumerate_all_solutions:true";
        solver.Solve(model, cb);
        Console.WriteLine(String.Format("Number of solutions found: {0}", cb.SolutionCount()));
    }
}
```
