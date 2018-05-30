# Solving a CP-SAT model

https://developers.google.com/optimization/


## Searching for one (optimal) solution

By default, search for one solution will return the first solution found if the
model has no objective, or the optimal solution if the model has an objective.

### Python solver code

The CpSolver class encapsulates searching for a solution of a model.

```python
from ortools.sat.python import cp_model

model = cp_model.CpModel()

x = model.NewBoolVar('x')

# Create a solver and solve.
solver = cp_model.CpSolver()
status = solver.Solve(model)
if status == cp_model.OPTIMAL:
  print("Objective value: %i" % solver.ObjectiveValue())  # if defined.
  print('x= %i' %  solver.Value(x))
```

### C++ solver code

Calling SolveCpModel() will return a CpSolverResponse protobuf that contains the
solve status, the values for each variable in the model if solve was successful,
and some metrics.

```cpp
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
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

  if (response.status() == CpSolverStatus::MODEL_SAT) {
    // Get the value of x in the solution.
    const int64 value_x = response.solution(x);
    LOG(INFO) << "x = " << value_x;
  }
}

} // namespace sat
} // namespace operations_research
```

### C\# code

```cs
using System;
using Google.OrTools.Sat;

public class CodeSamplesSat
{
  static void MinimalCpSat()
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

    if (status == CpSolverStatus.ModelSat)
    {
      Console.WriteLine("x = " + solver.Value(x));
      Console.WriteLine("y = " + solver.Value(y));
      Console.WriteLine("z = " + solver.Value(z));
    }
  }

  static void Main()
  {
    MinimalCpSat();
  }
}
```

## Changing the parameters of the solver

The SatParameters protobuf encapsulate a set of parameters of a CP-SAT solver.
The most useful one is the time limit.

### Specifying the time limit in python

```python
from ortools.sat.python import cp_model

model = cp_model.CpModel()

x = model.NewBoolVar('x')

# Creates a solver and solve.
solver = cp_model.CpSolver()

# Sets a time limit of 10 seconds.
solver.parameters.max_time_in_seconds = 10.0

# Solves the problem and uses the solution found.
status = solver.Solve(model)
if status == cp_model.OPTIMAL:
  print("Objective value: %i" % solver.ObjectiveValue())  # if defined.
  print('x= %i' %  solver.Value(x))
```

### Specifying the time limit in C++

```cpp
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
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

  if (response.status() == CpSolverStatus::MODEL_SAT) {
    // Get the value of x in the solution.
    const int64 value_x = response.solution(x);
    LOG(INFO) << "x = " << value_x;
  }
}

} // namespace sat
} // namespace operations_research
```

### Specifying the time limit in C\#

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
    // Creates the constraints.
    model.Add(x != y);

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();

    // Adds a time limit. Parameters are stored as strings in the solver.
    solver.StringParameters = "max_time_in_seconds:10.0" ;

    CpSolverStatus status = solver.Solve(model);

    if (status == CpSolverStatus.ModelSat)
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

In an optimization model, you can print intermediate solution. You need to
register a callback on the solver that will be called at each solution.

The exact implementation depends on the target language.

### Python code

```python
from ortools.sat.python import cp_model


# You need to subclass the cp_model.CpSolverSolutionCallback class.
class VarArraySolutionPrinter(cp_model.CpSolverSolutionCallback):
  """Print intermediate solutions."""

  def __init__(self, variables):
    self.__variables = variables
    self.__solution_count = 0

  def NewSolution(self):
    print('Solution %i' % self.__solution_count)
    print('  objective value = %i' % self.ObjectiveValue())
    for v in self.__variables:
      print('  %s = %i' % (v, self.Value(v)), end = ' ')
    print()
    self.__solution_count += 1

  def SolutionCount(self):
    return self.__solution_count


def MinimalCpSatPrintIntermediateSolutions():
  # Creates the model.
  model = cp_model.CpModel()
  # Creates the variables.
  num_vals = 3
  x = model.NewIntVar(0, num_vals - 1, "x")
  y = model.NewIntVar(0, num_vals - 1, "y")
  z = model.NewIntVar(0, num_vals - 1, "z")
  # Create the constraints.
  model.Add(x != y)
  model.Maximize(x + 2 * y + 3 * z)

  # Create a solver and solve.
  solver = cp_model.CpSolver()
  solution_printer = VarArraySolutionPrinter([x, y, z])
  status = solver.SolveWithSolutionObserver(model, solution_printer)

  print('Number of solutions found: %i' % solution_printer.SolutionCount())
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
```

## Searching for all solutions in a satisfiability model

In an non-optimization model, you can search for all solutions. You need to
register a callback on the solver that will be called at each solution.

The exact implementation depends on the target language.

### Python code

```python
from ortools.sat.python import cp_model


# You need to subclass the cp_model.CpSolverSolutionCallback class.
class VarArraySolutionPrinter(cp_model.CpSolverSolutionCallback):
  """Print intermediate solutions."""

  def __init__(self, variables):
    self.__variables = variables
    self.__solution_count = 0

  def NewSolution(self):
    print('Solution %i' % self.__solution_count)
    for v in self.__variables:
      print('  %s = %i' % (v, self.Value(v)), end = ' ')
    print()
    self.__solution_count += 1

  def SolutionCount(self):
    return self.__solution_count


def MinimalSatSearchForAllSolutions():
  # Creates the model.
  model = cp_model.CpModel()
  # Creates the variables.
  num_vals = 3
  x = model.NewIntVar(0, num_vals - 1, "x")
  y = model.NewIntVar(0, num_vals - 1, "y")
  z = model.NewIntVar(0, num_vals - 1, "z")
  # Create the constraints.
  model.Add(x != y)

  # Create a solver and solve.
  solver = cp_model.CpSolver()
  solution_printer = VarArraySolutionPrinter([x, y, z])
  status = solver.SearchForAllSolutions(model, solution_printer)

  print('Number of solutions found: %i' % solution_printer.SolutionCount())
```

### C++ code

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
```
