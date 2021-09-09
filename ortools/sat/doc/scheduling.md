| [home](README.md) | [boolean logic](boolean_logic.md) | [integer arithmetic](integer_arithmetic.md) | [channeling constraints](channeling.md) | [scheduling](scheduling.md) | [Using the CP-SAT solver](solver.md) | [Model manipulation](model.md) | [Python API](https://google.github.io/or-tools/python/ortools/sat/python/cp_model.html) |
| ----------------- | --------------------------------- | ------------------------------------------- | --------------------------------------- | --------------------------- | ------------------------------------ | ------------------------------ | -------------------------------- |

# Scheduling recipes for the CP-SAT solver.

https://developers.google.com/optimization/

<!--ts-->
   * [Scheduling recipes for the CP-SAT solver.](#scheduling-recipes-for-the-cp-sat-solver)
      * [Introduction](#introduction)
      * [Interval variables](#interval-variables)
         * [Python code](#python-code)
         * [C   code](#c-code)
         * [Java code](#java-code)
         * [C# code](#c-code-1)
      * [Optional intervals](#optional-intervals)
         * [Python code](#python-code-1)
         * [C   code](#c-code-2)
         * [Java code](#java-code-1)
         * [C# code](#c-code-3)
      * [NoOverlap constraint](#nooverlap-constraint)
         * [Python code](#python-code-2)
         * [C   code](#c-code-4)
         * [Java code](#java-code-2)
         * [C# code](#c-code-5)
      * [Cumulative constraint](#cumulative-constraint)
      * [Alternative resources for one interval](#alternative-resources-for-one-interval)
      * [Ranking tasks in a disjunctive resource](#ranking-tasks-in-a-disjunctive-resource)
         * [Python code](#python-code-3)
         * [C   code](#c-code-6)
         * [Java code](#java-code-3)
         * [C# code](#c-code-7)
      * [Intervals spanning over breaks in the calendar](#intervals-spanning-over-breaks-in-the-calendar)
         * [Python code](#python-code-4)
      * [Detecting if two intervals overlap.](#detecting-if-two-intervals-overlap)
         * [Python code](#python-code-5)
      * [Transitions in a disjunctive resource](#transitions-in-a-disjunctive-resource)
      * [Precedences between intervals](#precedences-between-intervals)
      * [Convex hull of a set of intervals](#convex-hull-of-a-set-of-intervals)
      * [Reservoir constraint](#reservoir-constraint)


<!--te-->


## Introduction

Scheduling in Operations Research involves problems of tasks, resources and
times. In general, scheduling problems have the following features: fixed or
variable durations, alternate ways of performing the same task, mutual
exclusivity between tasks, and temporal relations between tasks.

## Interval variables

Intervals are constraints containing three constant of affine expressions
(start, size, and end). Creating an interval constraint will enforce that start
+ size == end.

The more general API uses three expressions to define the interval. If the size
is fixed, a simpler API uses the start expression and the fixed size.

Creating these intervals is illustrated in the following code snippets.

### Python code

```python
"""Code sample to demonstrates how to build an interval."""


from ortools.sat.python import cp_model


def IntervalSampleSat():
  """Showcases how to build interval variables."""
  model = cp_model.CpModel()
  horizon = 100

  # An interval can be created from three affine expressions.
  start_var = model.NewIntVar(0, horizon, 'start')
  duration = 10  # Python cp/sat code accept integer variables or constants.
  end_var = model.NewIntVar(0, horizon, 'end')
  interval_var = model.NewIntervalVar(start_var, duration, end_var + 2,
                                      'interval')

  print(f'interval = {repr(interval_var)}')

  # If the size is fixed, a simpler version uses the start expression and the
  # size.
  fixed_size_interval_var = model.NewFixedSizeIntervalVar(
      start_var, 10, 'fixed_size_interval_var')
  print(f'fixed_size_interval_var = {repr(fixed_size_interval_var)}')

  # A fixed interval can be created using the same API.
  fixed_interval = model.NewFixedSizeIntervalVar(5, 10, 'fixed_interval')
  print(f'fixed_interval = {repr(fixed_interval)}')


IntervalSampleSat()
```

### C++ code

```cpp
#include "ortools/sat/cp_model.h"

namespace operations_research {
namespace sat {

void IntervalSampleSat() {
  CpModelBuilder cp_model;
  const int kHorizon = 100;
  const Domain horizon(0, kHorizon);

  // An interval can be created from three affine expressions.
  const IntVar x = cp_model.NewIntVar(horizon).WithName("x");
  const IntVar y = cp_model.NewIntVar({2, 4}).WithName("y");
  const IntVar z = cp_model.NewIntVar(horizon).WithName("z");

  const IntervalVar interval_var =
      cp_model.NewIntervalVar(x, y, LinearExpr(z).AddConstant(2))
          .WithName("interval");
  LOG(INFO) << "start = " << interval_var.StartExpr()
            << ", size = " << interval_var.SizeExpr()
            << ", end = " << interval_var.EndExpr()
            << ", interval_var = " << interval_var;

  // If the size is fixed, a simpler version uses the start expression and the
  // size.
  const IntervalVar fixed_size_interval_var =
      cp_model.NewFixedSizeIntervalVar(x, 10).WithName(
          "fixed_size_interval_var");
  LOG(INFO) << "start = " << fixed_size_interval_var.StartExpr()
            << ", size = " << fixed_size_interval_var.SizeExpr()
            << ", end = " << fixed_size_interval_var.EndExpr()
            << ", fixed_size_interval_var = " << fixed_size_interval_var;

  // A fixed interval can be created using the same API.
  const IntervalVar fixed_interval =
      cp_model.NewFixedSizeIntervalVar(5, 10).WithName("fixed_interval");
  LOG(INFO) << "start = " << fixed_interval.StartExpr()
            << ", size = " << fixed_interval.SizeExpr()
            << ", end = " << fixed_interval.EndExpr()
            << ", fixed_interval = " << fixed_interval;
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::IntervalSampleSat();

  return EXIT_SUCCESS;
}
```

### Java code

```java
package com.google.ortools.sat.samples;

import com.google.ortools.Loader;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.IntervalVar;
import com.google.ortools.sat.LinearExpr;

/** Code sample to demonstrates how to build an interval. */
public class IntervalSampleSat {
  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    CpModel model = new CpModel();
    int horizon = 100;

    // An interval can be created from three affine expressions.
    IntVar startVar = model.newIntVar(0, horizon, "start");
    IntVar endVar = model.newIntVar(0, horizon, "end");
    IntervalVar intervalVar =
        model.newIntervalVar(
            startVar, LinearExpr.constant(10), LinearExpr.affine(endVar, 1, 2), "interval");
    System.out.println(intervalVar);

    // If the size is fixed, a simpler version uses the start expression and the size.
    IntervalVar fixedSizeIntervalVar =
        model.newFixedSizeIntervalVar(startVar, 10, "fixed_size_interval_var");
    System.out.println(fixedSizeIntervalVar);

    // A fixed interval can be created using another method.
    IntervalVar fixedInterval = model.newFixedInterval(5, 10, "fixed_interval");
    System.out.println(fixedInterval);
  }
}
```

### C\# code

```cs
using System;
using Google.OrTools.Sat;

public class IntervalSampleSat
{
    static void Main()
    {
        CpModel model = new CpModel();
        int horizon = 100;

        // C# code supports constant of affine expressions.
        IntVar start_var = model.NewIntVar(0, horizon, "start");
        IntVar end_var = model.NewIntVar(0, horizon, "end");
        IntervalVar interval = model.NewIntervalVar(start_var, 10, end_var + 2, "interval");
        Console.WriteLine(interval);

        // If the size is fixed, a simpler version uses the start expression, the size and the
        // literal.
        IntervalVar fixedSizeIntervalVar =
            model.NewFixedSizeIntervalVar(start_var, 10, "fixed_size_interval_var");
        Console.WriteLine(fixedSizeIntervalVar);

        // A fixed interval can be created using the same API.
        IntervalVar fixedInterval = model.NewFixedSizeIntervalVar(5, 10, "fixed_interval");
        Console.WriteLine(fixedInterval);
    }
}
```

## Optional intervals

An interval can be marked as optional. The presence of this interval is
controlled by a literal. The **no overlap** and **cumulative** constraints
understand these presence literals, and correctly ignore inactive intervals.

### Python code

```python
"""Code sample to demonstrates how to build an optional interval."""

from ortools.sat.python import cp_model


def OptionalIntervalSampleSat():
  """Showcases how to build optional interval variables."""
  model = cp_model.CpModel()
  horizon = 100

  # An interval can be created from three affine expressions.
  start_var = model.NewIntVar(0, horizon, 'start')
  duration = 10  # Python cp/sat code accept integer variables or constants.
  end_var = model.NewIntVar(0, horizon, 'end')
  presence_var = model.NewBoolVar('presence')
  interval_var = model.NewOptionalIntervalVar(start_var, duration, end_var + 2,
                                              presence_var, 'interval')

  print(f'interval = {repr(interval_var)}')

  # If the size is fixed, a simpler version uses the start expression and the
  # size.
  fixed_size_interval_var = model.NewOptionalFixedSizeIntervalVar(
      start_var, 10, presence_var, 'fixed_size_interval_var')
  print(f'fixed_size_interval_var = {repr(fixed_size_interval_var)}')

  # A fixed interval can be created using the same API.
  fixed_interval = model.NewOptionalFixedSizeIntervalVar(
      5, 10, presence_var, 'fixed_interval')
  print(f'fixed_interval = {repr(fixed_interval)}')


OptionalIntervalSampleSat()
```

### C++ code

```cpp
#include "ortools/sat/cp_model.h"

namespace operations_research {
namespace sat {

void OptionalIntervalSampleSat() {
  CpModelBuilder cp_model;
  const int kHorizon = 100;
  const Domain horizon(0, kHorizon);

  // An optional interval can be created from three affine expressions and a
  // BoolVar.
  const IntVar x = cp_model.NewIntVar(horizon).WithName("x");
  const IntVar y = cp_model.NewIntVar({2, 4}).WithName("y");
  const IntVar z = cp_model.NewIntVar(horizon).WithName("z");
  const BoolVar presence_var = cp_model.NewBoolVar().WithName("presence");

  const IntervalVar interval_var =
      cp_model
          .NewOptionalIntervalVar(x, y, LinearExpr(z).AddConstant(2),
                                  presence_var)
          .WithName("interval");
  LOG(INFO) << "start = " << interval_var.StartExpr()
            << ", size = " << interval_var.SizeExpr()
            << ", end = " << interval_var.EndExpr()
            << ", presence = " << interval_var.PresenceBoolVar()
            << ", interval_var = " << interval_var;

  // If the size is fixed, a simpler version uses the start expression and the
  // size.
  const IntervalVar fixed_size_interval_var =
      cp_model.NewOptionalFixedSizeIntervalVar(x, 10, presence_var)
          .WithName("fixed_size_interval_var");
  LOG(INFO) << "start = " << fixed_size_interval_var.StartExpr()
            << ", size = " << fixed_size_interval_var.SizeExpr()
            << ", end = " << fixed_size_interval_var.EndExpr()
            << ", presence = " << fixed_size_interval_var.PresenceBoolVar()
            << ", interval_var = " << fixed_size_interval_var;
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::OptionalIntervalSampleSat();

  return EXIT_SUCCESS;
}
```

### Java code

```java
package com.google.ortools.sat.samples;

import com.google.ortools.Loader;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.IntervalVar;
import com.google.ortools.sat.LinearExpr;
import com.google.ortools.sat.Literal;

/** Code sample to demonstrates how to build an optional interval. */
public class OptionalIntervalSampleSat {
  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    CpModel model = new CpModel();
    int horizon = 100;

    // An interval can be created from three affine expressions, and a literal.
    IntVar startVar = model.newIntVar(0, horizon, "start");
    IntVar endVar = model.newIntVar(0, horizon, "end");
    Literal presence = model.newBoolVar("presence");
    IntervalVar intervalVar =
        model.newOptionalIntervalVar(
            startVar,
            LinearExpr.constant(10),
            LinearExpr.affine(endVar, 1, 2),
            presence,
            "interval");
    System.out.println(intervalVar);

    // If the size is fixed, a simpler version uses the start expression, the size and the literal.
    IntervalVar fixedSizeIntervalVar =
        model.newOptionalFixedSizeIntervalVar(startVar, 10, presence, "fixed_size_interval_var");
    System.out.println(fixedSizeIntervalVar);

    // A fixed interval can be created using another method.
    IntervalVar fixedInterval = model.newOptionalFixedInterval(5, 10, presence, "fixed_interval");
    System.out.println(fixedInterval);
  }
}
```

### C\# code

```cs
using System;
using Google.OrTools.Sat;

public class OptionalIntervalSampleSat
{
    static void Main()
    {
        CpModel model = new CpModel();
        int horizon = 100;

        // C# code supports constant of affine expressions.
        IntVar start_var = model.NewIntVar(0, horizon, "start");
        IntVar end_var = model.NewIntVar(0, horizon, "end");
        IntVar presence_var = model.NewBoolVar("presence");
        IntervalVar interval =
            model.NewOptionalIntervalVar(start_var, 10, end_var + 2, presence_var, "interval");
        Console.WriteLine(interval);

        // If the size is fixed, a simpler version uses the start expression, the size and the
        // literal.
        IntervalVar fixedSizeIntervalVar = model.NewOptionalFixedSizeIntervalVar(
            start_var, 10, presence_var, "fixed_size_interval_var");
        Console.WriteLine(fixedSizeIntervalVar);

        // A fixed interval can be created using the same API.
        IntervalVar fixedInterval =
            model.NewOptionalFixedSizeIntervalVar(5, 10, presence_var, "fixed_interval");
        Console.WriteLine(fixedInterval);
    }
}
```

## NoOverlap constraint

A NoOverlap constraint simply states that all intervals are disjoint. It is
built with a list of interval variables. Fixed intervals are useful for
excluding part of the timeline.

In the following examples. We want to schedule 3 tasks on 3 weeks excluding
weekends, making the final day as early as possible.

### Python code

```python
"""Code sample to demonstrate how to build a NoOverlap constraint."""

from ortools.sat.python import cp_model


def NoOverlapSampleSat():
  """No overlap sample with fixed activities."""
  model = cp_model.CpModel()
  horizon = 21  # 3 weeks.

  # Task 0, duration 2.
  start_0 = model.NewIntVar(0, horizon, 'start_0')
  duration_0 = 2  # Python cp/sat code accepts integer variables or constants.
  end_0 = model.NewIntVar(0, horizon, 'end_0')
  task_0 = model.NewIntervalVar(start_0, duration_0, end_0, 'task_0')
  # Task 1, duration 4.
  start_1 = model.NewIntVar(0, horizon, 'start_1')
  duration_1 = 4  # Python cp/sat code accepts integer variables or constants.
  end_1 = model.NewIntVar(0, horizon, 'end_1')
  task_1 = model.NewIntervalVar(start_1, duration_1, end_1, 'task_1')

  # Task 2, duration 3.
  start_2 = model.NewIntVar(0, horizon, 'start_2')
  duration_2 = 3  # Python cp/sat code accepts integer variables or constants.
  end_2 = model.NewIntVar(0, horizon, 'end_2')
  task_2 = model.NewIntervalVar(start_2, duration_2, end_2, 'task_2')

  # Weekends.
  weekend_0 = model.NewIntervalVar(5, 2, 7, 'weekend_0')
  weekend_1 = model.NewIntervalVar(12, 2, 14, 'weekend_1')
  weekend_2 = model.NewIntervalVar(19, 2, 21, 'weekend_2')

  # No Overlap constraint.
  model.AddNoOverlap([task_0, task_1, task_2, weekend_0, weekend_1, weekend_2])

  # Makespan objective.
  obj = model.NewIntVar(0, horizon, 'makespan')
  model.AddMaxEquality(obj, [end_0, end_1, end_2])
  model.Minimize(obj)

  # Solve model.
  solver = cp_model.CpSolver()
  status = solver.Solve(model)

  if status == cp_model.OPTIMAL:
    # Print out makespan and the start times for all tasks.
    print('Optimal Schedule Length: %i' % solver.ObjectiveValue())
    print('Task 0 starts at %i' % solver.Value(start_0))
    print('Task 1 starts at %i' % solver.Value(start_1))
    print('Task 2 starts at %i' % solver.Value(start_2))
  else:
    print('Solver exited with nonoptimal status: %i' % status)


NoOverlapSampleSat()
```

### C++ code

```cpp
#include <cstdint>

#include "ortools/sat/cp_model.h"

namespace operations_research {
namespace sat {

void NoOverlapSampleSat() {
  CpModelBuilder cp_model;
  const int64_t kHorizon = 21;  // 3 weeks.

  const Domain horizon(0, kHorizon);
  // Task 0, duration 2.
  const IntVar start_0 = cp_model.NewIntVar(horizon);
  const IntVar duration_0 = cp_model.NewConstant(2);
  const IntVar end_0 = cp_model.NewIntVar(horizon);
  const IntervalVar task_0 =
      cp_model.NewIntervalVar(start_0, duration_0, end_0);

  // Task 1, duration 4.
  const IntVar start_1 = cp_model.NewIntVar(horizon);
  const IntVar duration_1 = cp_model.NewConstant(4);
  const IntVar end_1 = cp_model.NewIntVar(horizon);
  const IntervalVar task_1 =
      cp_model.NewIntervalVar(start_1, duration_1, end_1);

  // Task 2, duration 3.
  const IntVar start_2 = cp_model.NewIntVar(horizon);
  const IntVar duration_2 = cp_model.NewConstant(3);
  const IntVar end_2 = cp_model.NewIntVar(horizon);
  const IntervalVar task_2 =
      cp_model.NewIntervalVar(start_2, duration_2, end_2);

  // Week ends.
  const IntervalVar weekend_0 =
      cp_model.NewIntervalVar(cp_model.NewConstant(5), cp_model.NewConstant(2),
                              cp_model.NewConstant(7));
  const IntervalVar weekend_1 =
      cp_model.NewIntervalVar(cp_model.NewConstant(12), cp_model.NewConstant(2),
                              cp_model.NewConstant(14));
  const IntervalVar weekend_2 =
      cp_model.NewIntervalVar(cp_model.NewConstant(19), cp_model.NewConstant(2),
                              cp_model.NewConstant(21));

  // No Overlap constraint.
  cp_model.AddNoOverlap(
      {task_0, task_1, task_2, weekend_0, weekend_1, weekend_2});

  // Makespan.
  IntVar makespan = cp_model.NewIntVar(horizon);
  cp_model.AddLessOrEqual(end_0, makespan);
  cp_model.AddLessOrEqual(end_1, makespan);
  cp_model.AddLessOrEqual(end_2, makespan);

  cp_model.Minimize(makespan);

  // Solving part.
  Model model;
  const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);
  LOG(INFO) << CpSolverResponseStats(response);

  if (response.status() == CpSolverStatus::OPTIMAL) {
    LOG(INFO) << "Optimal Schedule Length: " << response.objective_value();
    LOG(INFO) << "Task 0 starts at " << SolutionIntegerValue(response, start_0);
    LOG(INFO) << "Task 1 starts at " << SolutionIntegerValue(response, start_1);
    LOG(INFO) << "Task 2 starts at " << SolutionIntegerValue(response, start_2);
  }
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::NoOverlapSampleSat();

  return EXIT_SUCCESS;
}
```

### Java code

```java
package com.google.ortools.sat.samples;

import com.google.ortools.Loader;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.IntervalVar;
import com.google.ortools.sat.LinearExpr;

/**
 * We want to schedule 3 tasks on 3 weeks excluding weekends, making the final day as early as
 * possible.
 */
public class NoOverlapSampleSat {
  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    CpModel model = new CpModel();
    // Three weeks.
    int horizon = 21;

    // Task 0, duration 2.
    IntVar start0 = model.newIntVar(0, horizon, "start0");
    int duration0 = 2;
    IntVar end0 = model.newIntVar(0, horizon, "end0");
    IntervalVar task0 = model.newIntervalVar(start0, LinearExpr.constant(duration0), end0, "task0");

    //  Task 1, duration 4.
    IntVar start1 = model.newIntVar(0, horizon, "start1");
    int duration1 = 4;
    IntVar end1 = model.newIntVar(0, horizon, "end1");
    IntervalVar task1 = model.newIntervalVar(start1, LinearExpr.constant(duration1), end1, "task1");

    // Task 2, duration 3.
    IntVar start2 = model.newIntVar(0, horizon, "start2");
    int duration2 = 3;
    IntVar end2 = model.newIntVar(0, horizon, "end2");
    IntervalVar task2 = model.newIntervalVar(start2, LinearExpr.constant(duration2), end2, "task2");

    // Weekends.
    IntervalVar weekend0 = model.newFixedInterval(5, 2, "weekend0");
    IntervalVar weekend1 = model.newFixedInterval(12, 2, "weekend1");
    IntervalVar weekend2 = model.newFixedInterval(19, 2, "weekend2");

    // No Overlap constraint. This constraint enforces that no two intervals can overlap.
    // In this example, as we use 3 fixed intervals that span over weekends, this constraint makes
    // sure that all tasks are executed on weekdays.
    model.addNoOverlap(new IntervalVar[] {task0, task1, task2, weekend0, weekend1, weekend2});

    // Makespan objective.
    IntVar obj = model.newIntVar(0, horizon, "makespan");
    model.addMaxEquality(obj, new IntVar[] {end0, end1, end2});
    model.minimize(obj);

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.solve(model);

    if (status == CpSolverStatus.OPTIMAL) {
      System.out.println("Optimal Schedule Length: " + solver.objectiveValue());
      System.out.println("Task 0 starts at " + solver.value(start0));
      System.out.println("Task 1 starts at " + solver.value(start1));
      System.out.println("Task 2 starts at " + solver.value(start2));
    }
  }
}
```

### C\# code

```cs
using System;
using Google.OrTools.Sat;

public class NoOverlapSampleSat
{
    static void Main()
    {
        CpModel model = new CpModel();
        // Three weeks.
        int horizon = 21;

        // Task 0, duration 2.
        IntVar start_0 = model.NewIntVar(0, horizon, "start_0");
        int duration_0 = 2;
        IntVar end_0 = model.NewIntVar(0, horizon, "end_0");
        IntervalVar task_0 = model.NewIntervalVar(start_0, duration_0, end_0, "task_0");

        //  Task 1, duration 4.
        IntVar start_1 = model.NewIntVar(0, horizon, "start_1");
        int duration_1 = 4;
        IntVar end_1 = model.NewIntVar(0, horizon, "end_1");
        IntervalVar task_1 = model.NewIntervalVar(start_1, duration_1, end_1, "task_1");

        // Task 2, duration 3.
        IntVar start_2 = model.NewIntVar(0, horizon, "start_2");
        int duration_2 = 3;
        IntVar end_2 = model.NewIntVar(0, horizon, "end_2");
        IntervalVar task_2 = model.NewIntervalVar(start_2, duration_2, end_2, "task_2");

        // Weekends.
        IntervalVar weekend_0 = model.NewIntervalVar(5, 2, 7, "weekend_0");
        IntervalVar weekend_1 = model.NewIntervalVar(12, 2, 14, "weekend_1");
        IntervalVar weekend_2 = model.NewIntervalVar(19, 2, 21, "weekend_2");

        // No Overlap constraint.
        model.AddNoOverlap(new IntervalVar[] { task_0, task_1, task_2, weekend_0, weekend_1, weekend_2 });

        // Makespan objective.
        IntVar obj = model.NewIntVar(0, horizon, "makespan");
        model.AddMaxEquality(obj, new IntVar[] { end_0, end_1, end_2 });
        model.Minimize(obj);

        // Creates a solver and solves the model.
        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);

        if (status == CpSolverStatus.Optimal)
        {
            Console.WriteLine("Optimal Schedule Length: " + solver.ObjectiveValue);
            Console.WriteLine("Task 0 starts at " + solver.Value(start_0));
            Console.WriteLine("Task 1 starts at " + solver.Value(start_1));
            Console.WriteLine("Task 2 starts at " + solver.Value(start_2));
        }
    }
}
```

## Cumulative constraint

A cumulative constraint takes a list of intervals, and a list of demands, and a
capacity. It enforces that at any time point, the sum of demands of tasks active
at that time point is less than a given capacity.

## Alternative resources for one interval

## Ranking tasks in a disjunctive resource

To rank intervals in a NoOverlap constraint, we will count the number of
performed intervals that precede each interval.

This is slightly complicated if some interval variables are optional. To
implement it, we will create a matrix of `precedences` boolean variables.
`precedences[i]][j]` is set to true if and only if interval `i` is performed,
interval `j` is performed, and if the start of `i` is before the start of `j`.

Furthermore, `precedences[i][i]` is set to be equal to `presences[i]`. This way,
we can define the rank of an interval `i` as `sum over j(precedences[j][i]) -
1`. If the interval is not performed, the rank computed as -1, if the interval
is performed, its presence variable negates the -1, and the formula counts the
number of other intervals that precede it.

### Python code

```python
"""Code sample to demonstrates how to rank intervals."""

from ortools.sat.python import cp_model



def RankTasks(model, starts, presences, ranks):
  """This method adds constraints and variables to links tasks and ranks.

  This method assumes that all starts are disjoint, meaning that all tasks have
  a strictly positive duration, and they appear in the same NoOverlap
  constraint.

  Args:
    model: The CpModel to add the constraints to.
    starts: The array of starts variables of all tasks.
    presences: The array of presence variables of all tasks.
    ranks: The array of rank variables of all tasks.
  """

  num_tasks = len(starts)
  all_tasks = range(num_tasks)

  # Creates precedence variables between pairs of intervals.
  precedences = {}
  for i in all_tasks:
    for j in all_tasks:
      if i == j:
        precedences[(i, j)] = presences[i]
      else:
        prec = model.NewBoolVar('%i before %i' % (i, j))
        precedences[(i, j)] = prec
        model.Add(starts[i] < starts[j]).OnlyEnforceIf(prec)

  # Treats optional intervals.
  for i in range(num_tasks - 1):
    for j in range(i + 1, num_tasks):
      tmp_array = [precedences[(i, j)], precedences[(j, i)]]
      if not cp_model.ObjectIsATrueLiteral(presences[i]):
        tmp_array.append(presences[i].Not())
        # Makes sure that if i is not performed, all precedences are false.
        model.AddImplication(presences[i].Not(), precedences[(i, j)].Not())
        model.AddImplication(presences[i].Not(), precedences[(j, i)].Not())
      if not cp_model.ObjectIsATrueLiteral(presences[j]):
        tmp_array.append(presences[j].Not())
        # Makes sure that if j is not performed, all precedences are false.
        model.AddImplication(presences[j].Not(), precedences[(i, j)].Not())
        model.AddImplication(presences[j].Not(), precedences[(j, i)].Not())
      # The following bool_or will enforce that for any two intervals:
      #    i precedes j or j precedes i or at least one interval is not
      #        performed.
      model.AddBoolOr(tmp_array)
      # Redundant constraint: it propagates early that at most one precedence
      # is true.
      model.AddImplication(precedences[(i, j)], precedences[(j, i)].Not())
      model.AddImplication(precedences[(j, i)], precedences[(i, j)].Not())

  # Links precedences and ranks.
  for i in all_tasks:
    model.Add(ranks[i] == sum(precedences[(j, i)] for j in all_tasks) - 1)


def RankingSampleSat():
  """Ranks tasks in a NoOverlap constraint."""

  model = cp_model.CpModel()
  horizon = 100
  num_tasks = 4
  all_tasks = range(num_tasks)

  starts = []
  ends = []
  intervals = []
  presences = []
  ranks = []

  # Creates intervals, half of them are optional.
  for t in all_tasks:
    start = model.NewIntVar(0, horizon, 'start_%i' % t)
    duration = t + 1
    end = model.NewIntVar(0, horizon, 'end_%i' % t)
    if t < num_tasks // 2:
      interval = model.NewIntervalVar(start, duration, end, 'interval_%i' % t)
      presence = True
    else:
      presence = model.NewBoolVar('presence_%i' % t)
      interval = model.NewOptionalIntervalVar(start, duration, end, presence,
                                              'o_interval_%i' % t)
    starts.append(start)
    ends.append(end)
    intervals.append(interval)
    presences.append(presence)

    # Ranks = -1 if and only if the tasks is not performed.
    ranks.append(model.NewIntVar(-1, num_tasks - 1, 'rank_%i' % t))

  # Adds NoOverlap constraint.
  model.AddNoOverlap(intervals)

  # Adds ranking constraint.
  RankTasks(model, starts, presences, ranks)

  # Adds a constraint on ranks.
  model.Add(ranks[0] < ranks[1])

  # Creates makespan variable.
  makespan = model.NewIntVar(0, horizon, 'makespan')
  for t in all_tasks:
    model.Add(ends[t] <= makespan).OnlyEnforceIf(presences[t])

  # Minimizes makespan - fixed gain per tasks performed.
  # As the fixed cost is less that the duration of the last interval,
  # the solver will not perform the last interval.
  model.Minimize(2 * makespan - 7 * sum(presences[t] for t in all_tasks))

  # Solves the model model.
  solver = cp_model.CpSolver()
  status = solver.Solve(model)

  if status == cp_model.OPTIMAL:
    # Prints out the makespan and the start times and ranks of all tasks.
    print('Optimal cost: %i' % solver.ObjectiveValue())
    print('Makespan: %i' % solver.Value(makespan))
    for t in all_tasks:
      if solver.Value(presences[t]):
        print('Task %i starts at %i with rank %i' % (t, solver.Value(starts[t]),
                                                     solver.Value(ranks[t])))
      else:
        print('Task %i in not performed and ranked at %i' %
              (t, solver.Value(ranks[t])))
  else:
    print('Solver exited with nonoptimal status: %i' % status)


RankingSampleSat()
```

### C++ code

```cpp
#include "ortools/sat/cp_model.h"

namespace operations_research {
namespace sat {

void RankingSampleSat() {
  CpModelBuilder cp_model;
  const int kHorizon = 100;
  const int kNumTasks = 4;

  auto add_task_ranking = [&cp_model](const std::vector<IntVar>& starts,
                                      const std::vector<BoolVar>& presences,
                                      const std::vector<IntVar>& ranks) {
    const int num_tasks = starts.size();

    // Creates precedence variables between pairs of intervals.
    std::vector<std::vector<BoolVar>> precedences(num_tasks);
    for (int i = 0; i < num_tasks; ++i) {
      precedences[i].resize(num_tasks);
      for (int j = 0; j < num_tasks; ++j) {
        if (i == j) {
          precedences[i][i] = presences[i];
        } else {
          BoolVar prec = cp_model.NewBoolVar();
          precedences[i][j] = prec;
          cp_model.AddLessOrEqual(starts[i], starts[j]).OnlyEnforceIf(prec);
        }
      }
    }

    // Treats optional intervals.
    for (int i = 0; i < num_tasks - 1; ++i) {
      for (int j = i + 1; j < num_tasks; ++j) {
        // Makes sure that if i is not performed, all precedences are
        // false.
        cp_model.AddImplication(Not(presences[i]), Not(precedences[i][j]));
        cp_model.AddImplication(Not(presences[i]), Not(precedences[j][i]));
        // Makes sure that if j is not performed, all precedences are
        // false.
        cp_model.AddImplication(Not(presences[j]), Not(precedences[i][j]));
        cp_model.AddImplication(Not(presences[i]), Not(precedences[j][i]));
        //  The following bool_or will enforce that for any two intervals:
        //    i precedes j or j precedes i or at least one interval is not
        //        performed.
        cp_model.AddBoolOr({precedences[i][j], precedences[j][i],
                            Not(presences[i]), Not(presences[j])});
        // Redundant constraint: it propagates early that at most one
        // precedence is true.
        cp_model.AddImplication(precedences[i][j], Not(precedences[j][i]));
        cp_model.AddImplication(precedences[j][i], Not(precedences[i][j]));
      }
    }
    // Links precedences and ranks.
    for (int i = 0; i < num_tasks; ++i) {
      LinearExpr sum_of_predecessors(-1);
      for (int j = 0; j < num_tasks; ++j) {
        sum_of_predecessors.AddVar(precedences[j][i]);
      }
      cp_model.AddEquality(ranks[i], sum_of_predecessors);
    }
  };

  std::vector<IntVar> starts;
  std::vector<IntVar> ends;
  std::vector<IntervalVar> intervals;
  std::vector<BoolVar> presences;
  std::vector<IntVar> ranks;

  const Domain horizon(0, kHorizon);
  const Domain possible_ranks(-1, kNumTasks - 1);

  for (int t = 0; t < kNumTasks; ++t) {
    const IntVar start = cp_model.NewIntVar(horizon);
    const IntVar duration = cp_model.NewConstant(t + 1);
    const IntVar end = cp_model.NewIntVar(horizon);
    const BoolVar presence =
        t < kNumTasks / 2 ? cp_model.TrueVar() : cp_model.NewBoolVar();
    const IntervalVar interval =
        cp_model.NewOptionalIntervalVar(start, duration, end, presence);
    const IntVar rank = cp_model.NewIntVar(possible_ranks);

    starts.push_back(start);
    ends.push_back(end);
    intervals.push_back(interval);
    presences.push_back(presence);
    ranks.push_back(rank);
  }

  // Adds NoOverlap constraint.
  cp_model.AddNoOverlap(intervals);

  // Ranks tasks.
  add_task_ranking(starts, presences, ranks);

  // Adds a constraint on ranks.
  cp_model.AddLessThan(ranks[0], ranks[1]);

  // Creates makespan variables.
  const IntVar makespan = cp_model.NewIntVar(horizon);
  for (int t = 0; t < kNumTasks; ++t) {
    cp_model.AddLessOrEqual(ends[t], makespan).OnlyEnforceIf(presences[t]);
  }

  // Create objective: minimize 2 * makespan - 7 * sum of presences.
  // That is you gain 7 by interval performed, but you pay 2 by day of delays.
  LinearExpr objective;
  objective.AddTerm(makespan, 2);
  for (int t = 0; t < kNumTasks; ++t) {
    objective.AddTerm(presences[t], -7);
  }
  cp_model.Minimize(objective);

  // Solving part.
  const CpSolverResponse response = Solve(cp_model.Build());
  LOG(INFO) << CpSolverResponseStats(response);

  if (response.status() == CpSolverStatus::OPTIMAL) {
    LOG(INFO) << "Optimal cost: " << response.objective_value();
    LOG(INFO) << "Makespan: " << SolutionIntegerValue(response, makespan);
    for (int t = 0; t < kNumTasks; ++t) {
      if (SolutionBooleanValue(response, presences[t])) {
        LOG(INFO) << "task " << t << " starts at "
                  << SolutionIntegerValue(response, starts[t]) << " with rank "
                  << SolutionIntegerValue(response, ranks[t]);
      } else {
        LOG(INFO) << "task " << t << " is not performed and ranked at "
                  << SolutionIntegerValue(response, ranks[t]);
      }
    }
  }
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::RankingSampleSat();

  return EXIT_SUCCESS;
}
```

### Java code

```java
package com.google.ortools.sat.samples;

import com.google.ortools.Loader;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.IntervalVar;
import com.google.ortools.sat.LinearExpr;
import com.google.ortools.sat.Literal;
import java.util.ArrayList;
import java.util.List;

/** Code sample to demonstrates how to rank intervals. */
public class RankingSampleSat {
  /**
   * This code takes a list of interval variables in a noOverlap constraint, and a parallel list of
   * integer variables and enforces the following constraint
   * <ul>
   * <li>rank[i] == -1 iff interval[i] is not active.
   * <li>rank[i] == number of active intervals that precede interval[i].
   * </ul>
   */
  static void rankTasks(CpModel model, IntVar[] starts, Literal[] presences, IntVar[] ranks) {
    int numTasks = starts.length;

    // Creates precedence variables between pairs of intervals.
    Literal[][] precedences = new Literal[numTasks][numTasks];
    for (int i = 0; i < numTasks; ++i) {
      for (int j = 0; j < numTasks; ++j) {
        if (i == j) {
          precedences[i][i] = presences[i];
        } else {
          IntVar prec = model.newBoolVar(String.format("%d before %d", i, j));
          precedences[i][j] = prec;
          // Ensure that task i precedes task j if prec is true.
          model.addLessOrEqualWithOffset(starts[i], starts[j], 1).onlyEnforceIf(prec);
        }
      }
    }

    // Create optional intervals.
    for (int i = 0; i < numTasks - 1; ++i) {
      for (int j = i + 1; j < numTasks; ++j) {
        List<Literal> list = new ArrayList<>();
        list.add(precedences[i][j]);
        list.add(precedences[j][i]);
        list.add(presences[i].not());
        // Makes sure that if i is not performed, all precedences are false.
        model.addImplication(presences[i].not(), precedences[i][j].not());
        model.addImplication(presences[i].not(), precedences[j][i].not());
        list.add(presences[j].not());
        // Makes sure that if j is not performed, all precedences are false.
        model.addImplication(presences[j].not(), precedences[i][j].not());
        model.addImplication(presences[j].not(), precedences[j][i].not());
        // The following boolOr will enforce that for any two intervals:
        //    i precedes j or j precedes i or at least one interval is not
        //        performed.
        model.addBoolOr(list.toArray(new Literal[0]));
        // For efficiency, we add a redundant constraint declaring that only one of i precedes j and
        // j precedes i are true. This will speed up the solve because the reason of this
        // propagation is shorter that using interval bounds is true.
        model.addImplication(precedences[i][j], precedences[j][i].not());
        model.addImplication(precedences[j][i], precedences[i][j].not());
      }
    }

    // Links precedences and ranks.
    for (int i = 0; i < numTasks; ++i) {
      IntVar[] vars = new IntVar[numTasks + 1];
      int[] coefs = new int[numTasks + 1];
      for (int j = 0; j < numTasks; ++j) {
        vars[j] = (IntVar) precedences[j][i];
        coefs[j] = 1;
      }
      vars[numTasks] = ranks[i];
      coefs[numTasks] = -1;
      // ranks == sum(precedences) - 1;
      model.addEquality(LinearExpr.scalProd(vars, coefs), 1);
    }
  }

  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    CpModel model = new CpModel();
    int horizon = 100;
    int numTasks = 4;

    IntVar[] starts = new IntVar[numTasks];
    IntVar[] ends = new IntVar[numTasks];
    IntervalVar[] intervals = new IntervalVar[numTasks];
    Literal[] presences = new Literal[numTasks];
    IntVar[] ranks = new IntVar[numTasks];

    IntVar trueVar = model.newConstant(1);

    // Creates intervals, half of them are optional.
    for (int t = 0; t < numTasks; ++t) {
      starts[t] = model.newIntVar(0, horizon, "start_" + t);
      int duration = t + 1;
      ends[t] = model.newIntVar(0, horizon, "end_" + t);
      if (t < numTasks / 2) {
        intervals[t] =
            model.newIntervalVar(
                starts[t], LinearExpr.constant(duration), ends[t], "interval_" + t);
        presences[t] = trueVar;
      } else {
        presences[t] = model.newBoolVar("presence_" + t);
        intervals[t] =
            model.newOptionalIntervalVar(
                starts[t], LinearExpr.constant(duration), ends[t], presences[t], "o_interval_" + t);
      }

      // The rank will be -1 iff the task is not performed.
      ranks[t] = model.newIntVar(-1, numTasks - 1, "rank_" + t);
    }

    // Adds NoOverlap constraint.
    model.addNoOverlap(intervals);

    // Adds ranking constraint.
    rankTasks(model, starts, presences, ranks);

    // Adds a constraint on ranks (ranks[0] < ranks[1]).
    model.addLessOrEqualWithOffset(ranks[0], ranks[1], 1);

    // Creates makespan variable.
    IntVar makespan = model.newIntVar(0, horizon, "makespan");
    for (int t = 0; t < numTasks; ++t) {
      model.addLessOrEqual(ends[t], makespan).onlyEnforceIf(presences[t]);
    }
    // The objective function is a mix of a fixed gain per task performed, and a fixed cost for each
    // additional day of activity.
    // The solver will balance both cost and gain and minimize makespan * per-day-penalty - number
    // of tasks performed * per-task-gain.
    //
    // On this problem, as the fixed cost is less that the duration of the last interval, the solver
    // will not perform the last interval.
    IntVar[] objectiveVars = new IntVar[numTasks + 1];
    int[] objectiveCoefs = new int[numTasks + 1];
    for (int t = 0; t < numTasks; ++t) {
      objectiveVars[t] = (IntVar) presences[t];
      objectiveCoefs[t] = -7;
    }
    objectiveVars[numTasks] = makespan;
    objectiveCoefs[numTasks] = 2;
    model.minimize(LinearExpr.scalProd(objectiveVars, objectiveCoefs));

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.solve(model);

    if (status == CpSolverStatus.OPTIMAL) {
      System.out.println("Optimal cost: " + solver.objectiveValue());
      System.out.println("Makespan: " + solver.value(makespan));
      for (int t = 0; t < numTasks; ++t) {
        if (solver.booleanValue(presences[t])) {
          System.out.printf(
              "Task %d starts at %d with rank %d%n",
              t, solver.value(starts[t]), solver.value(ranks[t]));
        } else {
          System.out.printf(
              "Task %d in not performed and ranked at %d%n", t, solver.value(ranks[t]));
        }
      }
    } else {
      System.out.println("Solver exited with nonoptimal status: " + status);
    }
  }
}
```

### C\# code

```cs
using System;
using System.Collections.Generic;
using Google.OrTools.Sat;

public class RankingSampleSat
{
    static void RankTasks(CpModel model, IntVar[] starts, ILiteral[] presences, IntVar[] ranks)
    {
        int num_tasks = starts.Length;

        // Creates precedence variables between pairs of intervals.
        ILiteral[,] precedences = new ILiteral[num_tasks, num_tasks];
        for (int i = 0; i < num_tasks; ++i)
        {
            for (int j = 0; j < num_tasks; ++j)
            {
                if (i == j)
                {
                    precedences[i, i] = presences[i];
                }
                else
                {
                    IntVar prec = model.NewBoolVar(String.Format("{0} before {1}", i, j));
                    precedences[i, j] = prec;
                    model.Add(starts[i] < starts[j]).OnlyEnforceIf(prec);
                }
            }
        }

        // Treats optional intervals.
        for (int i = 0; i < num_tasks - 1; ++i)
        {
            for (int j = i + 1; j < num_tasks; ++j)
            {
                List<ILiteral> tmp_array = new List<ILiteral>();
                tmp_array.Add(precedences[i, j]);
                tmp_array.Add(precedences[j, i]);
                tmp_array.Add(presences[i].Not());
                // Makes sure that if i is not performed, all precedences are false.
                model.AddImplication(presences[i].Not(), precedences[i, j].Not());
                model.AddImplication(presences[i].Not(), precedences[j, i].Not());
                tmp_array.Add(presences[j].Not());
                // Makes sure that if j is not performed, all precedences are false.
                model.AddImplication(presences[j].Not(), precedences[i, j].Not());
                model.AddImplication(presences[j].Not(), precedences[j, i].Not());
                // The following bool_or will enforce that for any two intervals:
                //    i precedes j or j precedes i or at least one interval is not
                //        performed.
                model.AddBoolOr(tmp_array);
                // Redundant constraint: it propagates early that at most one precedence
                // is true.
                model.AddImplication(precedences[i, j], precedences[j, i].Not());
                model.AddImplication(precedences[j, i], precedences[i, j].Not());
            }
        }

        // Links precedences and ranks.
        for (int i = 0; i < num_tasks; ++i)
        {
            IntVar[] tmp_array = new IntVar[num_tasks];
            for (int j = 0; j < num_tasks; ++j)
            {
                tmp_array[j] = (IntVar)precedences[j, i];
            }
            model.Add(ranks[i] == LinearExpr.Sum(tmp_array) - 1);
        }
    }

    static void Main()
    {
        CpModel model = new CpModel();
        // Three weeks.
        int horizon = 100;
        int num_tasks = 4;

        IntVar[] starts = new IntVar[num_tasks];
        IntVar[] ends = new IntVar[num_tasks];
        IntervalVar[] intervals = new IntervalVar[num_tasks];
        ILiteral[] presences = new ILiteral[num_tasks];
        IntVar[] ranks = new IntVar[num_tasks];

        IntVar true_var = model.NewConstant(1);

        // Creates intervals, half of them are optional.
        for (int t = 0; t < num_tasks; ++t)
        {
            starts[t] = model.NewIntVar(0, horizon, String.Format("start_{0}", t));
            int duration = t + 1;
            ends[t] = model.NewIntVar(0, horizon, String.Format("end_{0}", t));
            if (t < num_tasks / 2)
            {
                intervals[t] = model.NewIntervalVar(starts[t], duration, ends[t], String.Format("interval_{0}", t));
                presences[t] = true_var;
            }
            else
            {
                presences[t] = model.NewBoolVar(String.Format("presence_{0}", t));
                intervals[t] = model.NewOptionalIntervalVar(starts[t], duration, ends[t], presences[t],
                                                            String.Format("o_interval_{0}", t));
            }

            // Ranks = -1 if and only if the tasks is not performed.
            ranks[t] = model.NewIntVar(-1, num_tasks - 1, String.Format("rank_{0}", t));
        }

        // Adds NoOverlap constraint.
        model.AddNoOverlap(intervals);

        // Adds ranking constraint.
        RankTasks(model, starts, presences, ranks);

        // Adds a constraint on ranks.
        model.Add(ranks[0] < ranks[1]);

        // Creates makespan variable.
        IntVar makespan = model.NewIntVar(0, horizon, "makespan");
        for (int t = 0; t < num_tasks; ++t)
        {
            model.Add(ends[t] <= makespan).OnlyEnforceIf(presences[t]);
        }
        // Minimizes makespan - fixed gain per tasks performed.
        // As the fixed cost is less that the duration of the last interval,
        // the solver will not perform the last interval.
        IntVar[] presences_as_int_vars = new IntVar[num_tasks];
        for (int t = 0; t < num_tasks; ++t)
        {
            presences_as_int_vars[t] = (IntVar)presences[t];
        }
        model.Minimize(2 * makespan - 7 * LinearExpr.Sum(presences_as_int_vars));

        // Creates a solver and solves the model.
        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);

        if (status == CpSolverStatus.Optimal)
        {
            Console.WriteLine(String.Format("Optimal cost: {0}", solver.ObjectiveValue));
            Console.WriteLine(String.Format("Makespan: {0}", solver.Value(makespan)));
            for (int t = 0; t < num_tasks; ++t)
            {
                if (solver.BooleanValue(presences[t]))
                {
                    Console.WriteLine(String.Format("Task {0} starts at {1} with rank {2}", t, solver.Value(starts[t]),
                                                    solver.Value(ranks[t])));
                }
                else
                {
                    Console.WriteLine(
                        String.Format("Task {0} in not performed and ranked at {1}", t, solver.Value(ranks[t])));
                }
            }
        }
        else
        {
            Console.WriteLine(String.Format("Solver exited with nonoptimal status: {0}", status));
        }
    }
}
```

## Intervals spanning over breaks in the calendar

Sometimes, a task can be interrupted by a break (overnight, lunch break). In
that context, although the processing time of the task is the same, the duration
can vary.

To implement this feature, we will have the duration of the task be a function
of the start of the task. This is implemented using channeling constraints.

The following code displays:

    start=8 duration=3 across=0
    start=9 duration=3 across=0
    start=10 duration=3 across=0
    start=11 duration=4 across=1
    start=12 duration=4 across=1
    start=14 duration=3 across=0
    start=15 duration=3 across=0

### Python code

```python
"""Code sample to demonstrate how an interval can span across a break."""

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


def SchedulingWithCalendarSampleSat():
  """Interval spanning across a lunch break."""
  model = cp_model.CpModel()

  # The data is the following:
  #   Work starts at 8h, ends at 18h, with a lunch break between 13h and 14h.
  #   We need to schedule a task that needs 3 hours of processing time.
  #   Total duration can be 3 or 4 (if it spans the lunch break).
  #
  # Because the duration is at least 3 hours, work cannot start after 15h.
  # Because of the break, work cannot start at 13h.

  start = model.NewIntVarFromDomain(
      cp_model.Domain.FromIntervals([(8, 12), (14, 15)]), 'start')
  duration = model.NewIntVar(3, 4, 'duration')
  end = model.NewIntVar(8, 18, 'end')
  unused_interval = model.NewIntervalVar(start, duration, end, 'interval')

  # We have 2 states (spanning across lunch or not)
  across = model.NewBoolVar('across')
  non_spanning_hours = cp_model.Domain.FromValues([8, 9, 10, 14, 15])
  model.AddLinearExpressionInDomain(start, non_spanning_hours).OnlyEnforceIf(
      across.Not())
  model.AddLinearConstraint(start, 11, 12).OnlyEnforceIf(across)
  model.Add(duration == 3).OnlyEnforceIf(across.Not())
  model.Add(duration == 4).OnlyEnforceIf(across)

  # Search for x values in increasing order.
  model.AddDecisionStrategy([start], cp_model.CHOOSE_FIRST,
                            cp_model.SELECT_MIN_VALUE)

  # Create a solver and solve with a fixed search.
  solver = cp_model.CpSolver()

  # Force the solver to follow the decision strategy exactly.
  solver.parameters.search_branching = cp_model.FIXED_SEARCH
  # Enumerate all solutions.
  solver.parameters.enumerate_all_solutions = True

  # Search and print all solutions.
  solution_printer = VarArraySolutionPrinter([start, duration, across])
  solver.Solve(model, solution_printer)


SchedulingWithCalendarSampleSat()
```

## Detecting if two intervals overlap.

We want a Boolean variable to be true iff two intervals overlap. To enforce
this, we will create 3 Boolean variables, link two of them to the relative
positions of the two intervals, and define the third one using the other two
Boolean variables.

There are two ways of linking the three Boolean variables. The first version
uses one clause and two implications. Propagation will be faster using this
version. The second version uses a `sum(..) == 1` equation. It is more compact,
but assumes the length of the two intervals is > 0.

Note that we need to create the intervals to enforce `start + size == end`, but
we do not actually use them in this code sample.

The following code displays

```
start_a=0 start_b=0 a_overlaps_b=1
start_a=0 start_b=1 a_overlaps_b=1
start_a=0 start_b=2 a_overlaps_b=1
start_a=0 start_b=3 a_overlaps_b=0
start_a=0 start_b=4 a_overlaps_b=0
start_a=0 start_b=5 a_overlaps_b=0
start_a=1 start_b=0 a_overlaps_b=1
start_a=1 start_b=1 a_overlaps_b=1
start_a=1 start_b=2 a_overlaps_b=1
start_a=1 start_b=3 a_overlaps_b=1
start_a=1 start_b=4 a_overlaps_b=0
start_a=1 start_b=5 a_overlaps_b=0
...
```

### Python code

```python
"""Code sample to demonstrates how to detect if two intervals overlap."""

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


def OverlappingIntervals():
  """Create the overlapping Boolean variables and enumerate all states."""
  model = cp_model.CpModel()

  horizon = 7

  # First interval.
  start_var_a = model.NewIntVar(0, horizon, 'start_a')
  duration_a = 3
  end_var_a = model.NewIntVar(0, horizon, 'end_a')
  unused_interval_var_a = model.NewIntervalVar(start_var_a, duration_a,
                                               end_var_a, 'interval_a')

  # Second interval.
  start_var_b = model.NewIntVar(0, horizon, 'start_b')
  duration_b = 2
  end_var_b = model.NewIntVar(0, horizon, 'end_b')
  unused_interval_var_b = model.NewIntervalVar(start_var_b, duration_b,
                                               end_var_b, 'interval_b')

  # a_after_b Boolean variable.
  a_after_b = model.NewBoolVar('a_after_b')
  model.Add(start_var_a >= end_var_b).OnlyEnforceIf(a_after_b)
  model.Add(start_var_a < end_var_b).OnlyEnforceIf(a_after_b.Not())

  # b_after_a Boolean variable.
  b_after_a = model.NewBoolVar('b_after_a')
  model.Add(start_var_b >= end_var_a).OnlyEnforceIf(b_after_a)
  model.Add(start_var_b < end_var_a).OnlyEnforceIf(b_after_a.Not())

  # Result Boolean variable.
  a_overlaps_b = model.NewBoolVar('a_overlaps_b')

  # Option a: using only clauses
  model.AddBoolOr([a_after_b, b_after_a, a_overlaps_b])
  model.AddImplication(a_after_b, a_overlaps_b.Not())
  model.AddImplication(b_after_a, a_overlaps_b.Not())

  # Option b: using a sum() == 1.
  # model.Add(a_after_b + b_after_a + a_overlaps_b == 1)

  # Search for start values in increasing order for the two intervals.
  model.AddDecisionStrategy([start_var_a, start_var_b], cp_model.CHOOSE_FIRST,
                            cp_model.SELECT_MIN_VALUE)

  # Create a solver and solve with a fixed search.
  solver = cp_model.CpSolver()

  # Force the solver to follow the decision strategy exactly.
  solver.parameters.search_branching = cp_model.FIXED_SEARCH
  # Enumerate all solutions.
  solver.parameters.enumerate_all_solutions = True

  # Search and print out all solutions.
  solution_printer = VarArraySolutionPrinter(
      [start_var_a, start_var_b, a_overlaps_b])
  solver.Solve(model, solution_printer)


OverlappingIntervals()
```

## Transitions in a disjunctive resource

## Precedences between intervals

## Convex hull of a set of intervals

## Reservoir constraint
