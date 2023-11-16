[home](README.md) | [boolean logic](boolean_logic.md) | [integer arithmetic](integer_arithmetic.md) | [channeling constraints](channeling.md) | [scheduling](scheduling.md) | [Using the CP-SAT solver](solver.md) | [Model manipulation](model.md) | [Troubleshooting](troubleshooting.md) | [Python API](https://google.github.io/or-tools/python/ortools/sat/python/cp_model.html)
----------------- | --------------------------------- | ------------------------------------------- | --------------------------------------- | --------------------------- | ------------------------------------ | ------------------------------ | ------------------------------------- | ---------------------------------------------------------------------------------------
# Channeling constraints

https://developers.google.com/optimization/

A *channeling constraint* links variables inside a model. They're used when you
want to express a complicated relationship between variables, such as "if this
variable satisfies a condition, force another variable to a particular value".

Channeling is usually implemented using *half-reified* linear constraints: one
constraint implies another (a &rarr; b), but not necessarily the other way
around (a &larr; b).

## If-Then-Else expressions

Let's say you want to implement the following: "If *x* is less than 5, set *y*
to 0. Otherwise, set *y* to 10-*x*". You can do this creating an intermediate
boolean variable *b* that is true if *x* is greater than or equal to 5, and
false otherwise:

*b* implies *y* == 10 - *x*

not(*b*) implies *y* == 0

These are implemented using the `OnlyEnforceIf` method as shown below.

### Python code

```python
#!/usr/bin/env python3
"""Link integer constraints together."""


from ortools.sat.python import cp_model


class VarArraySolutionPrinter(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions."""

    def __init__(self, variables: list[cp_model.IntVar]):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__variables = variables

    def on_solution_callback(self) -> None:
        for v in self.__variables:
            print(f"{v}={self.value(v)}", end=" ")
        print()


def ChannelingSampleSat():
    """Demonstrates how to link integer constraints together."""

    # Create the CP-SAT model.
    model = cp_model.CpModel()

    # Declare our two primary variables.
    x = model.new_int_var(0, 10, "x")
    y = model.new_int_var(0, 10, "y")

    # Declare our intermediate boolean variable.
    b = model.new_bool_var("b")

    # Implement b == (x >= 5).
    model.add(x >= 5).only_enforce_if(b)
    model.add(x < 5).only_enforce_if(b.negated())

    # Create our two half-reified constraints.
    # First, b implies (y == 10 - x).
    model.add(y == 10 - x).only_enforce_if(b)
    # Second, not(b) implies y == 0.
    model.add(y == 0).only_enforce_if(b.negated())

    # Search for x values in increasing order.
    model.add_decision_strategy([x], cp_model.CHOOSE_FIRST, cp_model.SELECT_MIN_VALUE)

    # Create a solver and solve with a fixed search.
    solver = cp_model.CpSolver()

    # Force the solver to follow the decision strategy exactly.
    solver.parameters.search_branching = cp_model.FIXED_SEARCH
    # Enumerate all solutions.
    solver.parameters.enumerate_all_solutions = True

    # Search and print out all solutions.
    solution_printer = VarArraySolutionPrinter([x, y, b])
    solver.solve(model, solution_printer)


ChannelingSampleSat()
```

### C++ code

```cpp
#include <stdlib.h>

#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

void ChannelingSampleSat() {
  // Create the CP-SAT model.
  CpModelBuilder cp_model;

  // Declare our two primary variables.
  const IntVar x = cp_model.NewIntVar({0, 10});
  const IntVar y = cp_model.NewIntVar({0, 10});

  // Declare our intermediate boolean variable.
  const BoolVar b = cp_model.NewBoolVar();

  // Implement b == (x >= 5).
  cp_model.AddGreaterOrEqual(x, 5).OnlyEnforceIf(b);
  cp_model.AddLessThan(x, 5).OnlyEnforceIf(Not(b));

  // Create our two half-reified constraints.
  // First, b implies (y == 10 - x).
  cp_model.AddEquality(x + y, 10).OnlyEnforceIf(b);
  // Second, not(b) implies y == 0.
  cp_model.AddEquality(y, 0).OnlyEnforceIf(Not(b));

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
    LOG(INFO) << "x=" << SolutionIntegerValue(r, x)
              << " y=" << SolutionIntegerValue(r, y)
              << " b=" << SolutionBooleanValue(r, b);
  }));
  SolveCpModel(cp_model.Build(), &model);
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
package com.google.ortools.sat.samples;

import com.google.ortools.Loader;
import com.google.ortools.sat.BoolVar;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverSolutionCallback;
import com.google.ortools.sat.DecisionStrategyProto;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.LinearExpr;
import com.google.ortools.sat.SatParameters;

/** Link integer constraints together. */
public class ChannelingSampleSat {
  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    // Create the CP-SAT model.
    CpModel model = new CpModel();

    // Declare our two primary variables.
    IntVar[] vars = new IntVar[] {model.newIntVar(0, 10, "x"), model.newIntVar(0, 10, "y")};

    // Declare our intermediate boolean variable.
    BoolVar b = model.newBoolVar("b");

    // Implement b == (x >= 5).
    model.addGreaterOrEqual(vars[0], 5).onlyEnforceIf(b);
    model.addLessOrEqual(vars[0], 4).onlyEnforceIf(b.not());

    // Create our two half-reified constraints.
    // First, b implies (y == 10 - x).
    model.addEquality(LinearExpr.sum(vars), 10).onlyEnforceIf(b);
    // Second, not(b) implies y == 0.
    model.addEquality(vars[1], 0).onlyEnforceIf(b.not());

    // Search for x values in increasing order.
    model.addDecisionStrategy(new IntVar[] {vars[0]},
        DecisionStrategyProto.VariableSelectionStrategy.CHOOSE_FIRST,
        DecisionStrategyProto.DomainReductionStrategy.SELECT_MIN_VALUE);

    // Create the solver.
    CpSolver solver = new CpSolver();

    // Force the solver to follow the decision strategy exactly.
    solver.getParameters().setSearchBranching(SatParameters.SearchBranching.FIXED_SEARCH);
    // Tell the solver to enumerate all solutions.
    solver.getParameters().setEnumerateAllSolutions(true);

    // Solve the problem with the printer callback.
    solver.solve(model, new CpSolverSolutionCallback() {
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
    }.init(new IntVar[] {vars[0], vars[1], b}));
  }
}
```

### C\# code

```cs
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

public class ChannelingSampleSat
{
    static void Main()
    {
        // Create the CP-SAT model.
        CpModel model = new CpModel();

        // Declare our two primary variables.
        IntVar x = model.NewIntVar(0, 10, "x");
        IntVar y = model.NewIntVar(0, 10, "y");

        // Declare our intermediate boolean variable.
        BoolVar b = model.NewBoolVar("b");

        // Implement b == (x >= 5).
        model.Add(x >= 5).OnlyEnforceIf(b);
        model.Add(x < 5).OnlyEnforceIf(b.Not());

        // Create our two half-reified constraints.
        // First, b implies (y == 10 - x).
        model.Add(y == 10 - x).OnlyEnforceIf(b);
        // Second, not(b) implies y == 0.
        model.Add(y == 0).OnlyEnforceIf(b.Not());

        // Search for x values in increasing order.
        model.AddDecisionStrategy(new IntVar[] { x }, DecisionStrategyProto.Types.VariableSelectionStrategy.ChooseFirst,
                                  DecisionStrategyProto.Types.DomainReductionStrategy.SelectMinValue);

        // Create the solver.
        CpSolver solver = new CpSolver();

        // Force solver to follow the decision strategy exactly.
        // Tell the solver to search for all solutions.
        solver.StringParameters = "search_branching:FIXED_SEARCH, enumerate_all_solutions:true";

        VarArraySolutionPrinter cb = new VarArraySolutionPrinter(new IntVar[] { x, y, b });
        solver.Solve(model, cb);
    }
}
```

This displays the following:

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


## A bin-packing problem

As another example of a channeling constraint, consider a bin packing problem in
which one part of the model computes the load of each bin, while another
maximizes the number of bins under a given threshold. To implement this, you can
*channel* the load of each bin into a set of boolean variables, each indicating
whether it's under the threshold.

To make this more concrete, let's say you have 10 bins of capacity 100, and
items to pack into the bins. You would like to maximize the number of bins that
can accept one emergency load of size 20.

To do this, you need to maximize the number of bins that have a load less
than 80.  In the code below, channeling is used to link the *load* and *slack*
variables together:

### Python code

```python
#!/usr/bin/env python3
"""Solves a binpacking problem using the CP-SAT solver."""


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
            x[(i, b)] = model.new_int_var(0, num_copies, f"x[{i},{b}]")

    # Load variables.
    load = [model.new_int_var(0, bin_capacity, f"load[{b}]") for b in all_bins]

    # Slack variables.
    slacks = [model.new_bool_var(f"slack[{b}]") for b in all_bins]

    # Links load and x.
    for b in all_bins:
        model.add(load[b] == sum(x[(i, b)] * items[i][0] for i in all_items))

    # Place all items.
    for i in all_items:
        model.add(sum(x[(i, b)] for b in all_bins) == items[i][1])

    # Links load and slack through an equivalence relation.
    safe_capacity = bin_capacity - slack_capacity
    for b in all_bins:
        # slack[b] => load[b] <= safe_capacity.
        model.add(load[b] <= safe_capacity).only_enforce_if(slacks[b])
        # not(slack[b]) => load[b] > safe_capacity.
        model.add(load[b] > safe_capacity).only_enforce_if(slacks[b].negated())

    # Maximize sum of slacks.
    model.maximize(sum(slacks))

    # Solves and prints out the solution.
    solver = cp_model.CpSolver()
    status = solver.solve(model)
    print(f"solve status: {solver.status_name(status)}")
    if status == cp_model.OPTIMAL:
        print(f"Optimal objective value: {solver.objective_value}")
    print("Statistics")
    print(f"  - conflicts : {solver.num_conflicts}")
    print(f"  - branches  : {solver.num_branches}")
    print(f"  - wall time : {solver.wall_time}s")


BinpackingProblemSat()
```

### C++ code

```cpp
#include <stdlib.h>

#include <vector>

#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"

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
      expr += x[i][b] * items[i][0];
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
  cp_model.Maximize(LinearExpr::Sum(slacks));

  // Solving part.
  const CpSolverResponse response = Solve(cp_model.Build());
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
package com.google.ortools.sat.samples;

import com.google.ortools.Loader;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.LinearExpr;
import com.google.ortools.sat.LinearExprBuilder;
import com.google.ortools.sat.Literal;

/** Solves a bin packing problem with the CP-SAT solver. */
public class BinPackingProblemSat {
  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
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
    Literal[] slacks = new Literal[numBins];
    for (int b = 0; b < numBins; ++b) {
      slacks[b] = model.newBoolVar("slack_" + b);
    }

    // Links load and x.
    for (int b = 0; b < numBins; ++b) {
      LinearExprBuilder expr = LinearExpr.newBuilder();
      for (int i = 0; i < numItems; ++i) {
        expr.addTerm(x[i][b], items[i][0]);
      }
      model.addEquality(expr, load[b]);
    }

    // Place all items.
    for (int i = 0; i < numItems; ++i) {
      LinearExprBuilder expr = LinearExpr.newBuilder();
      for (int b = 0; b < numBins; ++b) {
        expr.add(x[i][b]);
      }
      model.addEquality(expr, items[i][1]);
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
    model.maximize(LinearExpr.sum(slacks));

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
        int num_bins = 5;

        int[,] items = new int[,] { { 20, 6 }, { 15, 6 }, { 30, 4 }, { 45, 3 } };
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
        BoolVar[] slacks = new BoolVar[num_bins];
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
            model.Add(load[b] == LinearExpr.WeightedSum(tmp, sizes));
        }

        // Place all items.
        for (int i = 0; i < num_items; ++i)
        {
            IntVar[] tmp = new IntVar[num_bins];
            for (int b = 0; b < num_bins; ++b)
            {
                tmp[b] = x[i, b];
            }
            model.Add(LinearExpr.Sum(tmp) == items[i, 1]);
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
        model.Maximize(LinearExpr.Sum(slacks));

        // Solves and prints out the solution.
        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);
        Console.WriteLine(String.Format("Solve status: {0}", status));
        if (status == CpSolverStatus.Optimal)
        {
            Console.WriteLine(String.Format("Optimal objective value: {0}", solver.ObjectiveValue));
            for (int b = 0; b < num_bins; ++b)
            {
                Console.WriteLine(String.Format("load_{0} = {1}", b, solver.Value(load[b])));
                for (int i = 0; i < num_items; ++i)
                {
                    Console.WriteLine(string.Format("  item_{0}_{1} = {2}", i, b, solver.Value(x[i, b])));
                }
            }
        }
        Console.WriteLine("Statistics");
        Console.WriteLine(String.Format("  - conflicts : {0}", solver.NumConflicts()));
        Console.WriteLine(String.Format("  - branches  : {0}", solver.NumBranches()));
        Console.WriteLine(String.Format("  - wall time : {0} s", solver.WallTime()));
    }
}
```
