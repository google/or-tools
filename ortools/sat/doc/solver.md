| [home](README.md) | [boolean logic](boolean_logic.md) | [integer arithmetic](integer_arithmetic.md) | [channeling constraints](channeling.md) | [scheduling](scheduling.md) | [Using the CP-SAT solver](solver.md) | [Model manipulation](model.md) | [Reference manual](reference.md) |
| ----------------- | --------------------------------- | ------------------------------------------- | --------------------------------------- | --------------------------- | ------------------------------------ | ------------------------------ | -------------------------------- |

# Solving a CP-SAT model


<!--ts-->
   * [Solving a CP-SAT model](#solving-a-cp-sat-model)
      * [Changing the parameters of the solver](#changing-the-parameters-of-the-solver)
         * [Specifying the time limit in Python](#specifying-the-time-limit-in-python)
         * [Specifying the time limit in C  ](#specifying-the-time-limit-in-c)
         * [Specifying the time limit in Java](#specifying-the-time-limit-in-java)
         * [Specifying the time limit in C#.](#specifying-the-time-limit-in-c-1)
      * [Printing intermediate solutions](#printing-intermediate-solutions)
         * [Python code](#python-code)
         * [C   code](#c-code)
         * [Java code](#java-code)
         * [C# code](#c-code-1)
      * [Searching for all solutions in a satisfiability model](#searching-for-all-solutions-in-a-satisfiability-model)
         * [Python code](#python-code-1)
         * [C   code](#c-code-2)
         * [Java code](#java-code-1)
         * [C# code](#c-code-3)
      * [Stopping search early](#stopping-search-early)
         * [Python code](#python-code-2)
         * [C   code](#c-code-4)
         * [Java code](#java-code-2)
         * [C# code](#c-code-5)

<!-- Added by: lperron, at: Fri Jun  7 09:58:47 CEST 2019 -->

<!--te-->


## Changing the parameters of the solver

The SatParameters protobuf encapsulates the set of parameters of a CP-SAT
solver. The most useful one is the time limit.

### Specifying the time limit in Python

```python
"""Solves a problem with a time limit."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


def SolveWithTimeLimitSampleSat():
  """Minimal CP-SAT example to showcase calling the solver."""
  # Creates the model.
  model = cp_model.CpModel()
  # Creates the variables.
  num_vals = 3
  x = model.NewIntVar(0, num_vals - 1, 'x')
  y = model.NewIntVar(0, num_vals - 1, 'y')
  z = model.NewIntVar(0, num_vals - 1, 'z')
  # Adds an all-different constraint.
  model.Add(x != y)

  # Creates a solver and solves the model.
  solver = cp_model.CpSolver()

  # Sets a time limit of 10 seconds.
  solver.parameters.max_time_in_seconds = 10.0

  status = solver.Solve(model)

  if status == cp_model.FEASIBLE:
    print('x = %i' % solver.Value(x))
    print('y = %i' % solver.Value(y))
    print('z = %i' % solver.Value(z))


SolveWithTimeLimitSampleSat()
```

### Specifying the time limit in C++

```cpp
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"

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

  if (response.status() == CpSolverStatus::FEASIBLE) {
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
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.IntVar;

/** Solves a problem with a time limit. */
public class SolveWithTimeLimitSampleSat {

  static { System.loadLibrary("jniortools"); }

  public static void main(String[] args) throws Exception {
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

    if (status == CpSolverStatus.FEASIBLE) {
      System.out.println("x = " + solver.value(x));
      System.out.println("y = " + solver.value(y));
      System.out.println("z = " + solver.value(z));
    }
  }
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

    if (status == CpSolverStatus.Feasible)
    {
      Console.WriteLine("x = " + solver.Value(x));
      Console.WriteLine("y = " + solver.Value(y));
      Console.WriteLine("z = " + solver.Value(z));
    }
  }
}
```

## Printing intermediate solutions

In an optimization model, you can print intermediate solutions. You need to
register a callback on the solver that will be called at each solution.

The exact implementation depends on the target language.

### Python code

```python
"""Solves an optimization problem and displays all intermediate solutions."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


# You need to subclass the cp_model.CpSolverSolutionCallback class.
class VarArrayAndObjectiveSolutionPrinter(cp_model.CpSolverSolutionCallback):
  """Print intermediate solutions."""

  def __init__(self, variables):
    cp_model.CpSolverSolutionCallback.__init__(self)
    self.__variables = variables
    self.__solution_count = 0

  def on_solution_callback(self):
    print('Solution %i' % self.__solution_count)
    print('  objective value = %i' % self.ObjectiveValue())
    for v in self.__variables:
      print('  %s = %i' % (v, self.Value(v)), end=' ')
    print()
    self.__solution_count += 1

  def solution_count(self):
    return self.__solution_count


def SolveAndPrintIntermediateSolutionsSampleSat():
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
  solution_printer = VarArrayAndObjectiveSolutionPrinter([x, y, z])
  status = solver.SolveWithSolutionCallback(model, solution_printer)

  print('Status = %s' % solver.StatusName(status))
  print('Number of solutions found: %i' % solution_printer.solution_count())


SolveAndPrintIntermediateSolutionsSampleSat()
```

### C++ code

```cpp
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

void SolveAndPrintIntermediateSolutionsSampleSat() {
  CpModelBuilder cp_model;

  const Domain domain(0, 2);
  const IntVar x = cp_model.NewIntVar(domain).WithName("x");
  const IntVar y = cp_model.NewIntVar(domain).WithName("y");
  const IntVar z = cp_model.NewIntVar(domain).WithName("z");

  cp_model.AddNotEqual(x, y);

  cp_model.Maximize(LinearExpr::ScalProd({x, y, z}, {1, 2, 3}));

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
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverSolutionCallback;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.LinearExpr;

/** Solves an optimization problem and displays all intermediate solutions. */
public class SolveAndPrintIntermediateSolutionsSampleSat {

  static { System.loadLibrary("jniortools"); }

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
    model.maximize(LinearExpr.scalProd(new IntVar[] {x, y, z}, new int[] {1, 2, 3}));

    // Create a solver and solve the model.
    CpSolver solver = new CpSolver();
    VarArraySolutionPrinterWithObjective cb =
        new VarArraySolutionPrinterWithObjective(new IntVar[] {x, y, z});
    solver.solveWithSolutionCallback(model, cb);

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
    Console.WriteLine(String.Format("Solution #{0}: time = {1:F2} s",
          solution_count_, WallTime()));
    Console.WriteLine(
        String.Format("  objective value = {0}", ObjectiveValue()));
    foreach (IntVar v in variables_)
    {
      Console.WriteLine(
          String.Format("  {0} = {1}", v.ShortString(), Value(v)));
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
    VarArraySolutionPrinterWithObjective cb =
      new VarArraySolutionPrinterWithObjective(new IntVar[] { x, y, z });
    solver.SolveWithSolutionCallback(model, cb);

    Console.WriteLine(String.Format("Number of solutions found: {0}",
          cb.SolutionCount()));
  }
}
```

## Searching for all solutions in a satisfiability model

In an non-optimization model, you can search for all solutions. You need to
register a callback on the solver that will be called at each solution.

Please note that it does not work in parallel
(i. e. parameter `num_search_workers` > 1).

The exact implementation depends on the target language.

### Python code

To search for all solutions, use the SearchForAllSolutions method.

```python
"""Code sample that solves a model and displays all solutions."""

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


def SearchForAllSolutionsSampleSat():
  """Showcases calling the solver to search for all solutions."""
  # Creates the model.
  model = cp_model.CpModel()

  # Creates the variables.
  num_vals = 3
  x = model.NewIntVar(0, num_vals - 1, 'x')
  y = model.NewIntVar(0, num_vals - 1, 'y')
  z = model.NewIntVar(0, num_vals - 1, 'z')

  # Create the constraints.
  model.Add(x != y)

  # Create a solver and solve.
  solver = cp_model.CpSolver()
  solution_printer = VarArraySolutionPrinter([x, y, z])
  status = solver.SearchForAllSolutions(model, solution_printer)

  print('Status = %s' % solver.StatusName(status))
  print('Number of solutions found: %i' % solution_printer.solution_count())


SearchForAllSolutionsSampleSat()
```

### C++ code

To search for all solutions, a parameter of the SAT solver must be changed.

```cpp
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"

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

```java
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverSolutionCallback;
import com.google.ortools.sat.IntVar;

/** Code sample that solves a model and displays all solutions. */
public class SearchForAllSolutionsSampleSat {

  static { System.loadLibrary("jniortools"); }

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
    solver.searchAllSolutions(model, cb);

    System.out.println(cb.getSolutionCount() + " solutions found.");
  }
}
```

### C\# code

As in Python, CpSolver.SearchAllSolutions() must be called.

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
      Console.WriteLine(String.Format("Solution #{0}: time = {1:F2} s",
                                      solution_count_, WallTime()));
      foreach (IntVar v in variables_)
      {
        Console.WriteLine(
            String.Format("  {0} = {1}", v.ShortString(), Value(v)));
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
    VarArraySolutionPrinter cb =
        new VarArraySolutionPrinter(new IntVar[] { x, y, z });
    solver.SearchAllSolutions(model, cb);

    Console.WriteLine(String.Format("Number of solutions found: {0}",
                                    cb.SolutionCount()));
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
"""Code sample that solves a model and displays a small number of solutions."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


class VarArraySolutionPrinterWithLimit(cp_model.CpSolverSolutionCallback):
  """Print intermediate solutions."""

  def __init__(self, variables, limit):
    cp_model.CpSolverSolutionCallback.__init__(self)
    self.__variables = variables
    self.__solution_count = 0
    self.__solution_limit = limit

  def on_solution_callback(self):
    self.__solution_count += 1
    for v in self.__variables:
      print('%s=%i' % (v, self.Value(v)), end=' ')
    print()
    if self.__solution_count >= self.__solution_limit:
      print('Stop search after %i solutions' % self.__solution_limit)
      self.StopSearch()

  def solution_count(self):
    return self.__solution_count


def StopAfterNSolutionsSampleSat():
  """Showcases calling the solver to search for small number of solutions."""
  # Creates the model.
  model = cp_model.CpModel()
  # Creates the variables.
  num_vals = 3
  x = model.NewIntVar(0, num_vals - 1, 'x')
  y = model.NewIntVar(0, num_vals - 1, 'y')
  z = model.NewIntVar(0, num_vals - 1, 'z')

  # Create a solver and solve.
  solver = cp_model.CpSolver()
  solution_printer = VarArraySolutionPrinterWithLimit([x, y, z], 5)
  status = solver.SearchForAllSolutions(model, solution_printer)
  print('Status = %s' % solver.StatusName(status))
  print('Number of solutions found: %i' % solution_printer.solution_count())
  assert solution_printer.solution_count() == 5


StopAfterNSolutionsSampleSat()
```

### C++ code

Stopping search is done by registering an atomic bool on the model-owned time
limit, and setting that bool to true.

```cpp

#include <atomic>
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
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
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverSolutionCallback;
import com.google.ortools.sat.IntVar;

/** Code sample that solves a model and displays a small number of solutions. */
public class StopAfterNSolutionsSampleSat {

  static { System.loadLibrary("jniortools"); }

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

  public static void main(String[] args) throws Exception {
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
    solver.searchAllSolutions(model, cb);

    System.out.println(cb.getSolutionCount() + " solutions found.");
    if (cb.getSolutionCount() != 5) {
      throw new RuntimeException("Did not stop the search correctly.");
    }
  }
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
  public VarArraySolutionPrinterWithLimit(IntVar[] variables,
                                          int solution_limit)
  {
    variables_ = variables;
    solution_limit_ = solution_limit;
  }

  public override void OnSolutionCallback()
  {
    Console.WriteLine(String.Format("Solution #{0}: time = {1:F2} s",
          solution_count_, WallTime()));
    foreach (IntVar v in variables_)
    {
      Console.WriteLine(
          String.Format("  {0} = {1}", v.ShortString(), Value(v)));
    }
    solution_count_++;
    if (solution_count_ >= solution_limit_)
    {
      Console.WriteLine(
          String.Format("Stopping search after {0} solutions",
            solution_limit_));
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
    VarArraySolutionPrinterWithLimit cb =
      new VarArraySolutionPrinterWithLimit(new IntVar[] { x, y, z }, 5);
    solver.SearchAllSolutions(model, cb);
    Console.WriteLine(String.Format("Number of solutions found: {0}",
          cb.SolutionCount()));
  }
}
```
