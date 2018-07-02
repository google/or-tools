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
-   To exclude a single value, use int64min and int64max values as in 
    [int64min, 4, 6, int64max].

## Linear constraints

In **C++**, the model supports linear constraints as in:

    sum (a_i * x_i) in domain

Where domain uses the same encoding as integer variables.

From this, the usual modeling tricks can express general arithmetic constraints:

    x > y

can be rewritten as

    1 * x + (-1) * y in [1, int64max]

**Python** and **C\#** CP-SAT APIs support general linear arithmetic (+, *, -,
==, >=, >, <, <=, !=).

## Rabbits and Pheasants examples

Let's solve a simple children's puzzle: the Rabbits and Pheasants problem.

WIn a field of rabbits and pheasants, there are 20 heads and 56 legs. How many
rabbits and pheasants are there?

### Python code

```python
from ortools.sat.python import cp_model

def RabbitsAndPheasants():
  """Solves the rabbits + pheasants problem."""
  model = cp_model.CpModel()

  r = model.NewIntVar(0, 100, 'r')
  p = model.NewIntVar(0, 100, 'p')

  # 20 heads.
  model.Add(r + p == 20)
  # 56 legs.
  model.Add(4 * r + 2 * p == 56)

  # Solves and print out the solutions.
  # Creates a solver and solves the model.
  solver = cp_model.CpSolver()
  status = solver.Solve(model)

  if status == cp_model.FEASIBLE:
    print('%i rabbits and %i pheasants' % (solver.Value(r), solver.Value(p)))
```

### C++ code

```cpp
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

void RabbitsAndPheasants() {
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

  auto add_linear_constraint = [&cp_model](const std::vector<int>& vars,
                                           const std::vector<int64>& coeffs,
                                           int64 lb, int64 ub) {
    LinearConstraintProto* const lin =
        cp_model.add_constraints()->mutable_linear();
    for (const int v : vars) {
      lin->add_vars(v);
    }
    for (const int64 c : coeffs) {
      lin->add_coeffs(c);
    }
    lin->add_domain(lb);
    lin->add_domain(ub);
  };

  // Creates variables.
  const int r = new_variable(0, 100);
  const int p = new_variable(0, 100);

  // 20 heads.
  add_linear_constraint({r, p}, {1, 1}, 20, 20);
  // 56 legs.
  add_linear_constraint({r, p}, {4, 2}, 56, 56);

  // Solving part.
  Model model;
  LOG(INFO) << CpModelStats(cp_model);
  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  LOG(INFO) << CpSolverResponseStats(response);

  if (response.status() == CpSolverStatus::FEASIBLE) {
    // Get the value of x in the solution.
    LOG(INFO) << response.solution(r) << " rabbits, and "
              << response.solution(p) << " pheasants";
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
  static void RabbitsAndPheasants()
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

  static void Main()
  {
    RabbitsAndPheasants();
  }
}
```
