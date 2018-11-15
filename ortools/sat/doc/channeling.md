# Channeling constraints.



## Introduction

Channeling constraints are used to link different aspects of the same model.

For instance, with a bin packing problem, one part of the model computes the
load of each bin. Another part would like to maximize the number of bins under a
given threshold. To implement this, we will channel the load of each bin into a
set of Boolean variables that indicate if one bin load is under the threshold.

Channeling is usually implemented using half-reified linear constraints.

## Implementing an If-Then-Else expression

We want to implement `y == x < 5 ? 0 : 10 - x`. This will be done with an
intermediate boolean variable `b` and some half-reified constraints.

The following code produces the following output:

```
x=0 y=0 b=0
x=1 y=0 b=0
x=2 y=0 b=0
x=3 y=0 b=0
x=4 y=0 b=0
x=5 y=5 b=1
x=6 y=4 b=1
x=7 y=3 b=1
x=8 y=2 b=1
x=9 y=1 b=1
x=10 y=0 b=1
```

### Python code

```python
"""Link integer constraints together."""

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

  def OnSolutionCallback(self):
    self.__solution_count += 1
    for v in self.__variables:
      print('%s=%i' % (v, self.Value(v)), end=' ')
    print()

  def SolutionCount(self):
    return self.__solution_count


def ChannelingSampleSat():
  """Demonstrates how to link integer constraints together."""

  # Model.
  model = cp_model.CpModel()

  # Variables.
  x = model.NewIntVar(0, 10, 'x')
  y = model.NewIntVar(0, 10, 'y')

  b = model.NewBoolVar('b')

  # Implement b == (x >= 5).
  model.Add(x >= 5).OnlyEnforceIf(b)
  model.Add(x < 5).OnlyEnforceIf(b.Not())

  # b implies (y == 10 - x).
  model.Add(y == 10 - x).OnlyEnforceIf(b)
  # not(b) implies y == 0.
  model.Add(y == 0).OnlyEnforceIf(b.Not())

  # Search for x values in increasing order.
  model.AddDecisionStrategy([x], cp_model.CHOOSE_FIRST,
                            cp_model.SELECT_MIN_VALUE)

  # Create a solver and solve with a fixed search.
  solver = cp_model.CpSolver()

  # Force solver to follow the decision strategy exactly.
  solver.parameters.search_branching = cp_model.FIXED_SEARCH

  # Searches and prints out all solutions.
  solution_printer = VarArraySolutionPrinter([x, y, b])
  solver.SearchForAllSolutions(model, solution_printer)


ChannelingSampleSat()
```

### C++ code

```cpp
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

void ChannelingSampleSat() {
  // Model.
  CpModelBuilder cp_model;

  // Main variables.
  const IntVar x = cp_model.NewIntVar({0, 10});
  const IntVar y = cp_model.NewIntVar({0, 10});
  const BoolVar b = cp_model.NewBoolVar();

  // b == (x >= 5).
  cp_model.AddGreaterOrEqual(x, 5).OnlyEnforceIf(b);
  cp_model.AddLessThan(x, 5).OnlyEnforceIf(Not(b));

  // b implies (y == 10 - x).
  cp_model.AddEquality(LinearExpr::Sum({x, y}), 10).OnlyEnforceIf(b);
  // not(b) implies y == 0.
  cp_model.AddEquality(y, 0).OnlyEnforceIf(Not(b));

  // Search for x values in increasing order.
  cp_model.AddDecisionStrategy({x}, DecisionStrategyProto::CHOOSE_FIRST,
                               DecisionStrategyProto::SELECT_MIN_VALUE);

  Model model;
  SatParameters parameters;
  parameters.set_search_branching(SatParameters::FIXED_SEARCH);
  parameters.set_enumerate_all_solutions(true);
  model.Add(NewSatParameters(parameters));
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    LOG(INFO) << "x=" << SolutionIntegerValue(r, x)
              << " y=" << SolutionIntegerValue(r, y)
              << " b=" << SolutionBooleanValue(r, b);
  }));
  SolveWithModel(cp_model, &model);
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::ChannelingSampleSat();

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

/** Link integer constraints together. */
public class ChannelingSampleSat {

  static { System.loadLibrary("jniortools"); }

  public static void main(String[] args) throws Exception {
    // Model.
    CpModel model = new CpModel();

    // Variables.
    IntVar x = model.newIntVar(0, 10, "x");
    IntVar y = model.newIntVar(0, 10, "y");

    IntVar b = model.newBoolVar("b");

    // Implements b == (x >= 5).
    model.addGreaterOrEqual(x, 5).onlyEnforceIf(b);
    model.addLessOrEqual(x, 4).onlyEnforceIf(b.not());

    // b implies (y == 10 - x).
    model.addLinearSumEqual(new IntVar[] {x, y}, 10).onlyEnforceIf(b);
    // not(b) implies y == 0.
    model.addEquality(y, 0).onlyEnforceIf(b.not());

    // Searches for x values in increasing order.
    model.addDecisionStrategy(
        new IntVar[] {x},
        DecisionStrategyProto.VariableSelectionStrategy.CHOOSE_FIRST,
        DecisionStrategyProto.DomainReductionStrategy.SELECT_MIN_VALUE);

    // Creates the solver.
    CpSolver solver = new CpSolver();

    // Forces the solver to follow the decision strategy exactly.
    solver.getParameters().setSearchBranching(SatParameters.SearchBranching.FIXED_SEARCH);

    // Solves the problem with the printer callback.
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
        }.init(new IntVar[] {x, y, b}));
  }
}
```

### C\# code

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
      foreach (IntVar v in variables_)
      {
        Console.Write(String.Format("{0}={1} ", v.ShortString(), Value(v)));
      }
      Console.WriteLine();
    }
  }

  private IntVar[] variables_;
}

public class ChannelingSampleSat
{
  static void Main()
  {
    // Model.
    CpModel model = new CpModel();

    // Variables.
    IntVar x = model.NewIntVar(0, 10, "x");
    IntVar y = model.NewIntVar(0, 10, "y");

    IntVar b = model.NewBoolVar("b");

    // Implement b == (x >= 5).
    model.Add(x >= 5).OnlyEnforceIf(b);
    model.Add(x < 5).OnlyEnforceIf(b.Not());

    // b implies (y == 10 - x).
    model.Add(y == 10 - x).OnlyEnforceIf(b);
    // not(b) implies y == 0.
    model.Add(y == 0).OnlyEnforceIf(b.Not());

    // Search for x values in increasing order.
    model.AddDecisionStrategy(
        new IntVar[] {x},
        DecisionStrategyProto.Types.VariableSelectionStrategy.ChooseFirst,
        DecisionStrategyProto.Types.DomainReductionStrategy.SelectMinValue);

    // Create a solver and solve with a fixed search.
    CpSolver solver = new CpSolver();

    // Force solver to follow the decision strategy exactly.
    solver.StringParameters = "search_branching:FIXED_SEARCH";

    VarArraySolutionPrinter cb =
        new VarArraySolutionPrinter(new IntVar[] {x, y, b});
    solver.SearchAllSolutions(model, cb);
  }
}
```

## A bin-packing problem

We have 10 bins of capacity 100, and items to pack into the bins. We would like
to maximize the number of bins that can accept one emergency load of size 20.

So, we need to maximize the number of bins that have a load less than 80.
Channeling is used to link the *load* and *slack* variables together in the
following code samples.

### Python code

```python
"""Solves a binpacking problem using the CP-SAT solver."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


def BinpackingProblemSat():
  """Solves a bin-packing problem using the CP-SAT solver."""
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


BinpackingProblemSat()
```

### C++ code

```cpp
#include "ortools/sat/cp_model.h"

namespace operations_research {
namespace sat {

void BinpackingProblemSat() {
  // Data.
  const int kBinCapacity = 100;
  const int kSlackCapacity = 20;
  const int kNumBins = 5;

  const std::vector<std::vector<int>> items = {
      {20, 6}, {15, 6}, {30, 4}, {45, 3}};
  const int num_items = items.size();

  // Model.
  CpModelBuilder cp_model;

  // Main variables.
  std::vector<std::vector<IntVar>> x(num_items);
  for (int i = 0; i < num_items; ++i) {
    const int num_copies = items[i][1];
    for (int b = 0; b < kNumBins; ++b) {
      x[i].push_back(cp_model.NewIntVar({0, num_copies}));
    }
  }

  // Load variables.
  std::vector<IntVar> load(kNumBins);
  for (int b = 0; b < kNumBins; ++b) {
    load[b] = cp_model.NewIntVar({0, kBinCapacity});
  }

  // Slack variables.
  std::vector<BoolVar> slacks(kNumBins);
  for (int b = 0; b < kNumBins; ++b) {
    slacks[b] = cp_model.NewBoolVar();
  }

  // Links load and x.
  for (int b = 0; b < kNumBins; ++b) {
    LinearExpr expr;
    for (int i = 0; i < num_items; ++i) {
      expr.AddTerm(x[i][b], items[i][0]);
    }
    cp_model.AddEquality(expr, load[b]);
  }

  // Place all items.
  for (int i = 0; i < num_items; ++i) {
    cp_model.AddEquality(LinearExpr::Sum(x[i]), items[i][1]);
  }

  // Links load and slack through an equivalence relation.
  const int safe_capacity = kBinCapacity - kSlackCapacity;
  for (int b = 0; b < kNumBins; ++b) {
    // slack[b] => load[b] <= safe_capacity.
    cp_model.AddLessOrEqual(load[b], safe_capacity).OnlyEnforceIf(slacks[b]);
    // not(slack[b]) => load[b] > safe_capacity.
    cp_model.AddGreaterThan(load[b], safe_capacity)
        .OnlyEnforceIf(Not(slacks[b]));
  }

  // Maximize sum of slacks.
  cp_model.Maximize(LinearExpr::BooleanSum(slacks));

  // Solving part.
  const CpSolverResponse response = Solve(cp_model);
  LOG(INFO) << CpSolverResponseStats(response);
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::BinpackingProblemSat();

  return EXIT_SUCCESS;
}
```

### Java code

```java
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.IntVar;

/** Solves a bin packing problem with the CP-SAT solver. */
public class BinPackingProblemSat {

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
      System.out.printf("Optimal objective value: %f%n", solver.objectiveValue());
      for (int b = 0; b < numBins; ++b) {
        System.out.printf("load_%d = %d%n", b, solver.value(load[b]));
        for (int i = 0; i < numItems; ++i) {
          System.out.printf("  item_%d_%d = %d%n", i, b, solver.value(x[i][b]));
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

public class BinPackingProblemSat
{
  static void Main()
  {
    // Data.
    int bin_capacity = 100;
    int slack_capacity = 20;
    int num_bins = 10;

    int[,] items = new int[,] { { 20, 12 }, { 15, 12 }, { 30, 8 }, { 45, 5 } };
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
    for (int i = 0; i < num_items; ++i)
    {
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
                                    solver.WallTime()));
  }
}
```
