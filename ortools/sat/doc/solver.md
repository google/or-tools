# Solving a CP-SAT model



## Searching for one (optimal) solution

By default, searching for one solution will return the first solution found if
the model has no objective, or the optimal solution if the model has an
objective.

### Python solver code

The CpSolver class encapsulates searching for a solution of a model.

```python
"""Simple solve."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


def SimpleSolve():
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


SimpleSolve()
```

### C++ solver code

Calling SolveCpModel() will return a CpSolverResponse protobuf that contains the
solve status, the values for each variable in the model if solve was successful,
and some metrics.

```cpp
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

void SimpleSolve() {
  CpModelProto cp_model;

  // Trivial model with just one variable and no constraint.
  auto new_variable = [&cp_model](int64 lb, int64 ub) {
    CHECK_LE(lb, ub);
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(lb);
    var->add_domain(ub);
    return index;
  };

  const int x = new_variable(0, 3);

  // Solving part.
  Model model;
  LOG(INFO) << CpModelStats(cp_model);
  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  LOG(INFO) << CpSolverResponseStats(response);

  if (response.status() == CpSolverStatus::FEASIBLE) {
    // Get the value of x in the solution.
    const int64 value_x = response.solution(x);
    LOG(INFO) << "x = " << value_x;
  }
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::SimpleSolve();

  return EXIT_SUCCESS;
}
```

### Java code

As in python, the CpSolver class encapsulates searching for a solution of a
model.

```java
import com.google.ortools.sat.*;

public class SimpleSolve {

  static {
    System.loadLibrary("jniortools");
  }

  static void SimpleSolve() {
    // Creates the model.
    CpModel model = new CpModel();
    // Creates the variables.
    int num_vals = 3;

    IntVar x = model.newIntVar(0, num_vals - 1, "x");
    IntVar y = model.newIntVar(0, num_vals - 1, "y");
    IntVar z = model.newIntVar(0, num_vals - 1, "z");
    // Creates the constraints.
    model.addDifferent(x, y);

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.solve(model);

    if (status == CpSolverStatus.FEASIBLE)
    {
      System.out.println("x = " + solver.value(x));
      System.out.println("y = " + solver.value(y));
      System.out.println("z = " + solver.value(z));
    }

  }

  public static void main(String[] args) throws Exception {
    SimpleSolve();
  }
}
```

### C\# code

As in python, the CpSolver class encapsulates searching for a solution of a
model.

```cs
using System;
using Google.OrTools.Sat;

public class CodeSamplesSat
{
  static void SimpleSolve()
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

  static void Main()
  {
    SimpleSolve();
  }
}
```

## Changing the parameters of the solver

The SatParameters protobuf encapsulates the set of parameters of a CP-SAT
solver. The most useful one is the time limit.

### Specifying the time limit in python

```python
"""Solves a problem with a time limit."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


def MinimalCpSatWithTimeLimit():
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


MinimalCpSatWithTimeLimit()
```

### Specifying the time limit in C++

```cpp
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

void SolveWithTimeLimit() {
  CpModelProto cp_model;

  // Trivial model with just one variable and no constraint.
  auto new_variable = [&cp_model](int64 lb, int64 ub) {
    CHECK_LE(lb, ub);
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(lb);
    var->add_domain(ub);
    return index;
  };

  const int x = new_variable(0, 3);

  // Solving part.
  Model model;

  // Sets a time limit of 10 seconds.
  SatParameters parameters;
  parameters.set_max_time_in_seconds(10.0);
  model.Add(NewSatParameters(parameters));

  // Solve.
  LOG(INFO) << CpModelStats(cp_model);
  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  LOG(INFO) << CpSolverResponseStats(response);

  if (response.status() == CpSolverStatus::FEASIBLE) {
    // Get the value of x in the solution.
    const int64 value_x = response.solution(x);
    LOG(INFO) << "value_x = " << value_x;
  }
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::SolveWithTimeLimit();

  return EXIT_SUCCESS;
}
```

### Specifying the time limit in C\#.

Parameters must be passed as string to the solver.

```cs
using System;
using Google.OrTools.Sat;

public class CodeSamplesSat
{
  static void MinimalCpSatWithTimeLimit()
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
    solver.StringParameters = "max_time_in_seconds:10.0" ;

    CpSolverStatus status = solver.Solve(model);

    if (status == CpSolverStatus.Feasible)
    {
      Console.WriteLine("x = " + solver.Value(x));
      Console.WriteLine("y = " + solver.Value(y));
      Console.WriteLine("z = " + solver.Value(z));
    }
  }

  static void Main()
  {
    MinimalCpSatWithTimeLimit();
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
    self.__variables = variables
    self.__solution_count = 0

  def NewSolution(self):
    print('Solution %i' % self.__solution_count)
    print('  objective value = %i' % self.ObjectiveValue())
    for v in self.__variables:
      print('  %s = %i' % (v, self.Value(v)), end=' ')
    print()
    self.__solution_count += 1

  def SolutionCount(self):
    return self.__solution_count


def MinimalCpSatPrintIntermediateSolutions():
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
  status = solver.SolveWithSolutionObserver(model, solution_printer)

  print('Status = %s' % solver.StatusName(status))
  print('Number of solutions found: %i' % solution_printer.SolutionCount())


MinimalCpSatPrintIntermediateSolutions()
```

### C++ code

```cpp
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

void MinimalSatPrintIntermediateSolutions() {
  CpModelProto cp_model;

  auto new_variable = [&cp_model](int64 lb, int64 ub) {
    CHECK_LE(lb, ub);
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(lb);
    var->add_domain(ub);
    return index;
  };

  auto add_different = [&cp_model](const int left_var, const int right_var) {
    LinearConstraintProto* const lin =
        cp_model.add_constraints()->mutable_linear();
    lin->add_vars(left_var);
    lin->add_coeffs(1);
    lin->add_vars(right_var);
    lin->add_coeffs(-1);
    lin->add_domain(kint64min);
    lin->add_domain(-1);
    lin->add_domain(1);
    lin->add_domain(kint64max);
  };

  auto maximize = [&cp_model](const std::vector<int>& vars,
                              const std::vector<int64>& coeffs) {
    CpObjectiveProto* const obj = cp_model.mutable_objective();
    for (const int v : vars) {
      obj->add_vars(v);
    }
    for (const int64 c : coeffs) {
      obj->add_coeffs(-c);  // Maximize.
    }
    obj->set_scaling_factor(-1.0);  // Maximize.
  };

  const int kNumVals = 3;
  const int x = new_variable(0, kNumVals - 1);
  const int y = new_variable(0, kNumVals - 1);
  const int z = new_variable(0, kNumVals - 1);

  add_different(x, y);

  maximize({x, y, z}, {1, 2, 3});

  Model model;
  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    LOG(INFO) << "Solution " << num_solutions;
    LOG(INFO) << "  objective value = " << r.objective_value();
    LOG(INFO) << "  x = " << r.solution(x);
    LOG(INFO) << "  y = " << r.solution(y);
    LOG(INFO) << "  z = " << r.solution(z);
    num_solutions++;
  }));
  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  LOG(INFO) << "Number of solutions found: " << num_solutions;
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::MinimalSatPrintIntermediateSolutions();

  return EXIT_SUCCESS;
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
  }

  public int SolutionCount()
  {
    return solution_count_;
  }

  private int solution_count_;
  private IntVar[] variables_;
}

public class CodeSamplesSat
{
  static void MinimalCpSatPrintIntermediateSolutions()
  {
    // Creates the model.
    CpModel model = new CpModel();
    // Creates the variables.
    int num_vals = 3;

    IntVar x = model.NewIntVar(0, num_vals - 1, 'x');
    IntVar y = model.NewIntVar(0, num_vals - 1, 'y');
    IntVar z = model.NewIntVar(0, num_vals - 1, 'z');

    // Adds a different constraint.
    model.Add(x != y);

    // Maximizes a linear combination of variables.
    model.Maximize(x + 2 * y + 3 * z);

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();
    VarArraySolutionPrinterWithObjective cb =
        new VarArraySolutionPrinterWithObjective(new IntVar[] {x, y, z});
    solver.SearchAllSolutions(model, cb);
    Console.WriteLine(String.Format('Number of solutions found: {0}',
                                    cb.SolutionCount()));
  }

  static void Main()
  {
    MinimalCpSatPrintIntermediateSolutions();
  }
}
```

## Searching for all solutions in a satisfiability model

In an non-optimization model, you can search for all solutions. You need to
register a callback on the solver that will be called at each solution.

The exact implementation depends on the target language.

### Python code

To search for all solutions, the SearchForAllSolutions method must be used.

```python
"""Code sample that solves a model and displays all solutions."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


class VarArraySolutionPrinter(cp_model.CpSolverSolutionCallback):
  """Print intermediate solutions."""

  def __init__(self, variables):
    self.__variables = variables
    self.__solution_count = 0

  def NewSolution(self):
    self.__solution_count += 1
    for v in self.__variables:
      print('%s=%i' % (v, self.Value(v)), end=' ')
    print()

  def SolutionCount(self):
    return self.__solution_count


def MinimalSatSearchForAllSolutions():
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
  print('Number of solutions found: %i' % solution_printer.SolutionCount())


MinimalSatSearchForAllSolutions()
```

### C++ code

To search for all solution, a parameter of the sat solver must be changed.

```cpp
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

void MinimalSatSearchForAllSolutions() {
  CpModelProto cp_model;

  auto new_variable = [&cp_model](int64 lb, int64 ub) {
    CHECK_LE(lb, ub);
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(lb);
    var->add_domain(ub);
    return index;
  };

  auto add_different = [&cp_model](const int left_var, const int right_var) {
    LinearConstraintProto* const lin =
        cp_model.add_constraints()->mutable_linear();
    lin->add_vars(left_var);
    lin->add_coeffs(1);
    lin->add_vars(right_var);
    lin->add_coeffs(-1);
    lin->add_domain(kint64min);
    lin->add_domain(-1);
    lin->add_domain(1);
    lin->add_domain(kint64max);
  };

  const int kNumVals = 3;
  const int x = new_variable(0, kNumVals - 1);
  const int y = new_variable(0, kNumVals - 1);
  const int z = new_variable(0, kNumVals - 1);

  add_different(x, y);

  Model model;

  // Tell the solver to enumerate all solutions.
  SatParameters parameters;
  parameters.set_enumerate_all_solutions(true);
  model.Add(NewSatParameters(parameters));

  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    LOG(INFO) << "Solution " << num_solutions;
    LOG(INFO) << "  x = " << r.solution(x);
    LOG(INFO) << "  y = " << r.solution(y);
    LOG(INFO) << "  z = " << r.solution(z);
    num_solutions++;
  }));
  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  LOG(INFO) << "Number of solutions found: " << num_solutions;
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::MinimalSatSearchForAllSolutions();

  return EXIT_SUCCESS;
}
```

### C\# code

As in python, CpSolver.SearchAllSolutions() must be called.

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


public class CodeSamplesSat
{
  static void MinimalCpSatAllSolutions()
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
        new VarArraySolutionPrinter(new IntVar[] {x, y, z});
    solver.SearchAllSolutions(model, cb);
    Console.WriteLine(String.Format("Number of solutions found: {0}",
                                    cb.SolutionCount()));
  }

  static void Main()
  {
    MinimalCpSatAllSolutions();
  }
}
```
