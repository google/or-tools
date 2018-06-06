# Channeling constraints.



## Introduction

Channeling constraints are used to link to aspects of a model.

For instance, with a bin packing problem, one part of the model computes the
load of each bin. Another part would like to maximize the number of bins under a
given threshold. To implement this, we will channel the load of each bin into a
set of Boolean variables that indicate if one bin load is under the threshold.

Channeling is usually implemented using half-reified linear constraints.

## A bin-packing problem

We have 10 bins of capacity 100, and items to pack them. We would like to
maximize the number of bins that can accept emergency loads of size 20.

So, we need to count the number of bins that have a load less than 80.

### Python code

```python
from ortools.sat.python import cp_model


def BinpackingProblem():
  """Solves a bin-packing problem."""
  # Data.
  bin_capacity = 100
  slack_capacity = 20
  num_bins = 10
  all_bins = range(num_bins)

  items = [(20, 12), (15, 12), (30, 8), (45, 5)]
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
  loads = [model.NewIntVar(0, bin_capacity, 'load_%i' % b) for b in all_bins]

  # Slack variables.
  slacks = [model.NewBoolVar('slack_%i' % b) for b in all_bins]

  # Links load and x.
  for b in all_bins:
    model.Add(loads[b] == sum(x[(i, b)] * items[i][0] for i in all_items))

  # Place all items.
  for i in all_items:
    model.Add(sum(x[(i, b)] for b in all_bins) == items[i][1])

  # Links load and slack.
  safe_capacity = bin_capacity - slack_capacity
  for b in all_bins:
    # slack[b] => loads[b] <= safe_capacity
    model.Add(loads[b] <= safe_capacity).OnlyEnforceIf(slacks[b])
    # not(slack[b]) => loads[b] > safe_capacity
    model.Add(loads[b] > safe_capacity).OnlyEnforceIf(slacks[b].Not())

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
    IntVar[] loads = new IntVar[num_bins];
    for (int b = 0; b < num_bins; ++b)
    {
      loads[b] = model.NewIntVar(0, bin_capacity, String.Format("load_{0}", b));
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
      model.Add(loads[b] == tmp.ScalProd(sizes));
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
      //  slack[b] => loads[b] <= safe_capacity.
      model.Add(loads[b] <= safe_capacity).OnlyEnforceIf(slacks[b]);
      // not(slack[b]) => loads[b] > safe_capacity.
      model.Add(loads[b] > safe_capacity).OnlyEnforceIf(slacks[b].Not());
    }

    // Maximize sum of slacks.
    model.Maximize(slacks.Sum());

    Console.WriteLine(model.Model);

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
                                        b, solver.Value(loads[b])));
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
    RabbitsAndPheasants();
  }
}
```
