# Channeling constraints.



## Introduction

Channeling constraints are used to link different aspects of the same model.

For instance, with a bin packing problem, one part of the model computes the
load of each bin. Another part would like to maximize the number of bins under a
given threshold. To implement this, we will channel the load of each bin into a
set of Boolean variables that indicate if one bin load is under the threshold.

Channeling is usually implemented using half-reified linear constraints.

## A bin-packing problem

We have 10 bins of capacity 100, and items to pack into the bins. We would like
to maximize the number of bins that can accept one emergency load of size 20.

So, we need to maximize the number of bins that have a load less than 80.
Channeling is used to link the *load* and *slack* variables together in the
following code samples.

### Python code

```python
"""Solves a binpacking problem."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


def BinpackingProblem():
  """Solves a bin-packing problem."""
  # Data.
  bin_capacity = 100
  slack_capacity = 20
  num_bins = 5
  all_bins = range(num_bins)

  items = [(20, 6), (15, 6), (30, 4), (45, 3)]
  num_items = len(items)
  all_items = range(num_items)

  # Model.
  model = cp_model.CpModel()

  # Main variables.
  x = {}
  for i in all_items:
    num_copies = items[i][1]
    for b in all_bins:
      x[(i, b)] = model.NewIntVar(0, num_copies, 'x_%i_%i' % (i, b))

  # Load variables.
  load = [model.NewIntVar(0, bin_capacity, 'load_%i' % b) for b in all_bins]

  # Slack variables.
  slacks = [model.NewBoolVar('slack_%i' % b) for b in all_bins]

  # Links load and x.
  for b in all_bins:
    model.Add(load[b] == sum(x[(i, b)] * items[i][0] for i in all_items))

  # Place all items.
  for i in all_items:
    model.Add(sum(x[(i, b)] for b in all_bins) == items[i][1])

  # Links load and slack through an equivalence relation.
  safe_capacity = bin_capacity - slack_capacity
  for b in all_bins:
    # slack[b] => load[b] <= safe_capacity.
    model.Add(load[b] <= safe_capacity).OnlyEnforceIf(slacks[b])
    # not(slack[b]) => load[b] > safe_capacity.
    model.Add(load[b] > safe_capacity).OnlyEnforceIf(slacks[b].Not())

  # Maximize sum of slacks.
  model.Maximize(sum(slacks))

  # Solves and prints out the solution.
  solver = cp_model.CpSolver()
  status = solver.Solve(model)
  print('Solve status: %s' % solver.StatusName(status))
  if status == cp_model.OPTIMAL:
    print('Optimal objective value: %i' % solver.ObjectiveValue())
  print('Statistics')
  print('  - conflicts : %i' % solver.NumConflicts())
  print('  - branches  : %i' % solver.NumBranches())
  print('  - wall time : %f s' % solver.WallTime())


BinpackingProblem()
```

### C++ code

```cpp
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

void BinpackingProblem() {
  // Data.
  const int kBinCapacity = 100;
  const int kSlackCapacity = 20;
  const int kNumBins = 5;

  const std::vector<std::vector<int>> items = {
      {20, 6}, {15, 6}, {30, 4}, {45, 3}};
  const int num_items = items.size();

  // Model.
  CpModelProto cp_model;

  // Helpers.
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

  auto add_reified_variable_bounds = [&cp_model](int var, int64 lb, int64 ub,
                                                 int lit) {
    ConstraintProto* const ct = cp_model.add_constraints();
    ct->add_enforcement_literal(lit);
    LinearConstraintProto* const lin = ct->mutable_linear();
    lin->add_vars(var);
    lin->add_coeffs(1);
    lin->add_domain(lb);
    lin->add_domain(ub);
  };

  auto maximize = [&cp_model](const std::vector<int>& vars) {
    CpObjectiveProto* const obj = cp_model.mutable_objective();
    for (const int v : vars) {
      obj->add_vars(v);
      obj->add_coeffs(-1);  // Maximize.
    }
    obj->set_scaling_factor(-1.0);  // Maximize.
  };

  // Main variables.
  std::vector<std::vector<int>> x(num_items);
  for (int i = 0; i < num_items; ++i) {
    const int num_copies = items[i][1];
    for (int b = 0; b < kNumBins; ++b) {
      x[i].push_back(new_variable(0, num_copies));
    }
  }

  // Load variables.
  std::vector<int> load(kNumBins);
  for (int b = 0; b < kNumBins; ++b) {
    load[b] = new_variable(0, kBinCapacity);
  }

  // Slack variables.
  std::vector<int> slack(kNumBins);
  for (int b = 0; b < kNumBins; ++b) {
    slack[b] = new_variable(0, 1);
  }

  // Links load and x.
  for (int b = 0; b < kNumBins; ++b) {
    std::vector<int> vars;
    std::vector<int64> coeffs;
    vars.push_back(load[b]);
    coeffs.push_back(-1);
    for (int i = 0; i < num_items; ++i) {
      vars.push_back(x[i][b]);
      coeffs.push_back(items[i][0]);
    }
    add_linear_constraint(vars, coeffs, 0, 0);
  }

  // Place all items.
  for (int i = 0; i < num_items; ++i) {
    std::vector<int> vars;
    std::vector<int64> coeffs;
    for (int b = 0; b < kNumBins; ++b) {
      vars.push_back(x[i][b]);
      coeffs.push_back(1);
    }
    add_linear_constraint(vars, coeffs, items[i][1], items[i][1]);
  }

  // Links load and slack through an equivalence relation.
  const int safe_capacity = kBinCapacity - kSlackCapacity;
  for (int b = 0; b < kNumBins; ++b) {
    // slack[b] => load[b] <= safe_capacity.
    add_reified_variable_bounds(load[b], kint64min, safe_capacity, slack[b]);
    // not(slack[b]) => load[b] > safe_capacity.
    add_reified_variable_bounds(load[b], safe_capacity + 1, kint64max,
                                NegatedRef(slack[b]));
  }

  // Maximize sum of slacks.
  maximize(slack);

  // Solving part.
  Model model;
  LOG(INFO) << CpModelStats(cp_model);
  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  LOG(INFO) << CpSolverResponseStats(response);
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::BinpackingProblem();

  return EXIT_SUCCESS;
}
```

### Java code

```java
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.IntVar;

public class BinpackingProblem {

  static { System.loadLibrary("jniortools"); }

  public static void main(String[] args) throws Exception {
    // Data.
    int binCapacity = 100;
    int slackCapacity = 20;
    int numBins = 5;

    int[][] items = new int[][] {{20, 6}, {15, 6}, {30, 4}, {45, 3}};
    int numItems = items.length;

    // Model.
    CpModel model = new CpModel();

    // Main variables.
    IntVar[][] x = new IntVar[numItems][numBins];
    for (int i = 0; i < numItems; ++i) {
      int numCopies = items[i][1];
      for (int b = 0; b < numBins; ++b) {
        x[i][b] = model.newIntVar(0, numCopies, "x_" + i + "_" + b);
      }
    }

    // Load variables.
    IntVar[] load = new IntVar[numBins];
    for (int b = 0; b < numBins; ++b) {
      load[b] = model.newIntVar(0, binCapacity, "load_" + b);
    }

    // Slack variables.
    IntVar[] slacks = new IntVar[numBins];
    for (int b = 0; b < numBins; ++b) {
      slacks[b] = model.newBoolVar("slack_" + b);
    }

    // Links load and x.
    int[] sizes = new int[numItems];
    for (int i = 0; i < numItems; ++i) {
      sizes[i] = items[i][0];
    }
    for (int b = 0; b < numBins; ++b) {
      IntVar[] vars = new IntVar[numItems];
      for (int i = 0; i < numItems; ++i) {
        vars[i] = x[i][b];
      }
      model.addScalProdEqual(vars, sizes, load[b]);
    }

    // Place all items.
    for (int i = 0; i < numItems; ++i) {
      IntVar[] vars = new IntVar[numBins];
      for (int b = 0; b < numBins; ++b) {
        vars[b] = x[i][b];
      }
      model.addLinearSumEqual(vars, items[i][1]);
    }

    // Links load and slack.
    int safeCapacity = binCapacity - slackCapacity;
    for (int b = 0; b < numBins; ++b) {
      //  slack[b] => load[b] <= safeCapacity.
      model.addLessOrEqual(load[b], safeCapacity).onlyEnforceIf(slacks[b]);
      // not(slack[b]) => load[b] > safeCapacity.
      model.addGreaterOrEqual(load[b], safeCapacity + 1).onlyEnforceIf(slacks[b].not());
    }

    // Maximize sum of slacks.
    model.maximizeSum(slacks);

    // Solves and prints out the solution.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.solve(model);
    System.out.println("Solve status: " + status);
    if (status == CpSolverStatus.OPTIMAL) {
      System.out.println(String.format("Optimal objective value: %f", solver.objectiveValue()));
      for (int b = 0; b < numBins; ++b) {
        System.out.println(String.format("load_%d = %d", b, solver.value(load[b])));
        for (int i = 0; i < numItems; ++i) {
          System.out.println(String.format("  item_%d_%d = %d", i, b, solver.value(x[i][b])));
        }
      }
    }
    System.out.println("Statistics");
    System.out.println("  - conflicts : " + solver.numConflicts());
    System.out.println("  - branches  : " + solver.numBranches());
    System.out.println("  - wall time : " + solver.wallTime() + " s");
  }
}
```

### C\# code

```cs
using System;
using Google.OrTools.Sat;

public class CodeSamplesSat
{
  static void BinpackingProblem()
  {
    // Data.
    int bin_capacity = 100;
    int slack_capacity = 20;
    int num_bins = 10;

    int[,] items = new int[,] { {20, 12}, {15, 12}, {30, 8}, {45, 5} };
    int num_items = items.GetLength(0);

    // Model.
    CpModel model = new CpModel();

    // Main variables.
    IntVar[,] x = new IntVar[num_items, num_bins];
    for (int i = 0; i < num_items; ++i)
    {
      int num_copies = items[i, 1];
      for (int b = 0; b < num_bins; ++b)
      {
        x[i, b] = model.NewIntVar(0, num_copies, String.Format("x_{0}_{1}", i, b));
      }
    }

    // Load variables.
    IntVar[] load = new IntVar[num_bins];
    for (int b = 0; b < num_bins; ++b)
    {
      load[b] = model.NewIntVar(0, bin_capacity, String.Format("load_{0}", b));
    }

    // Slack variables.
    IntVar[] slacks = new IntVar[num_bins];
    for (int b = 0; b < num_bins; ++b)
    {
      slacks[b] = model.NewBoolVar(String.Format("slack_{0}", b));
    }

    // Links load and x.
    int[] sizes = new int[num_items];
    for (int i = 0; i < num_items; ++i) {
      sizes[i] = items[i, 0];
    }
    for (int b = 0; b < num_bins; ++b)
    {
      IntVar[] tmp = new IntVar[num_items];
      for (int i = 0; i < num_items; ++i)
      {
        tmp[i] = x[i, b];
      }
      model.Add(load[b] == tmp.ScalProd(sizes));
    }

    // Place all items.
    for (int i = 0; i < num_items; ++i)
    {
      IntVar[] tmp = new IntVar[num_bins];
      for (int b = 0; b < num_bins; ++b)
      {
        tmp[b] = x[i, b];
      }
      model.Add(tmp.Sum() == items[i, 1]);
    }

    // Links load and slack.
    int safe_capacity = bin_capacity - slack_capacity;
    for (int b = 0; b < num_bins; ++b)
    {
      //  slack[b] => load[b] <= safe_capacity.
      model.Add(load[b] <= safe_capacity).OnlyEnforceIf(slacks[b]);
      // not(slack[b]) => load[b] > safe_capacity.
      model.Add(load[b] > safe_capacity).OnlyEnforceIf(slacks[b].Not());
    }

    // Maximize sum of slacks.
    model.Maximize(slacks.Sum());

    // Solves and prints out the solution.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.Solve(model);
    Console.WriteLine(String.Format("Solve status: {0}", status));
    if (status == CpSolverStatus.Optimal) {
      Console.WriteLine(String.Format("Optimal objective value: {0}",
                                      solver.ObjectiveValue));
      for (int b = 0; b < num_bins; ++b)
      {
        Console.WriteLine(String.Format("load_{0} = {1}",
                                        b, solver.Value(load[b])));
        for (int i = 0; i < num_items; ++i)
        {
          Console.WriteLine(string.Format("  item_{0}_{1} = {2}",
                                          i, b, solver.Value(x[i, b])));
        }
      }
    }
    Console.WriteLine("Statistics");
    Console.WriteLine(String.Format("  - conflicts : {0}",
                                    solver.NumConflicts()));
    Console.WriteLine(String.Format("  - branches  : {0}",
                                    solver.NumBranches()));
    Console.WriteLine(String.Format("  - wall time : {0} s",
  }

  static void Main()
  {
    BinpackingProblem();
  }
}
```
