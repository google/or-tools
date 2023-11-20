[home](README.md) | [boolean logic](boolean_logic.md) | [integer arithmetic](integer_arithmetic.md) | [channeling constraints](channeling.md) | [scheduling](scheduling.md) | [Using the CP-SAT solver](solver.md) | [Model manipulation](model.md) | [Troubleshooting](troubleshooting.md) | [Python API](https://google.github.io/or-tools/python/ortools/sat/python/cp_model.html)
----------------- | --------------------------------- | ------------------------------------------- | --------------------------------------- | --------------------------- | ------------------------------------ | ------------------------------ | ------------------------------------- | ---------------------------------------------------------------------------------------
# Scheduling recipes for the CP-SAT solver.

https://developers.google.com/optimization/

## Introduction

Scheduling in Operations Research involves problems of tasks, resources and
times. In general, scheduling problems have the following features: fixed or
variable durations, alternate ways of performing the same task, mutual
exclusivity between tasks, and temporal relations between tasks.

## Interval variables

Intervals are constraints containing three constant of affine expressions
(start, size, and end). Creating an interval constraint will enforce that
`start + size == end`.

The more general API uses three expressions to define the interval. If the size
is fixed, a simpler API uses the start expression and the fixed size.

Creating these intervals is illustrated in the following code snippets.

### Python code

```python
#!/usr/bin/env python3
"""Code sample to demonstrates how to build an interval."""


from ortools.sat.python import cp_model


def interval_sample_sat():
    """Showcases how to build interval variables."""
    model = cp_model.CpModel()
    horizon = 100

    # An interval can be created from three affine expressions.
    start_var = model.new_int_var(0, horizon, "start")
    duration = 10  # Python cp/sat code accept integer variables or constants.
    end_var = model.new_int_var(0, horizon, "end")
    interval_var = model.new_interval_var(start_var, duration, end_var + 2, "interval")

    print(f"interval = {repr(interval_var)}")

    # If the size is fixed, a simpler version uses the start expression and the
    # size.
    fixed_size_interval_var = model.new_fixed_size_interval_var(
        start_var, 10, "fixed_size_interval_var"
    )
    print(f"fixed_size_interval_var = {repr(fixed_size_interval_var)}")

    # A fixed interval can be created using the same API.
    fixed_interval = model.new_fixed_size_interval_var(5, 10, "fixed_interval")
    print(f"fixed_interval = {repr(fixed_interval)}")


interval_sample_sat()
```

### C++ code

```cpp
#include <stdlib.h>

#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"
#include "ortools/util/sorted_interval_list.h"

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
      cp_model.NewIntervalVar(x, y, z + 2).WithName("interval");
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
    IntervalVar intervalVar = model.newIntervalVar(
        startVar, LinearExpr.constant(10), LinearExpr.newBuilder().add(endVar).add(2), "interval");
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
        IntervalVar fixedSizeIntervalVar = model.NewFixedSizeIntervalVar(start_var, 10, "fixed_size_interval_var");
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
#!/usr/bin/env python3
"""Code sample to demonstrates how to build an optional interval."""

from ortools.sat.python import cp_model


def optional_interval_sample_sat():
    """Showcases how to build optional interval variables."""
    model = cp_model.CpModel()
    horizon = 100

    # An interval can be created from three affine expressions.
    start_var = model.new_int_var(0, horizon, "start")
    duration = 10  # Python cp/sat code accept integer variables or constants.
    end_var = model.new_int_var(0, horizon, "end")
    presence_var = model.new_bool_var("presence")
    interval_var = model.new_optional_interval_var(
        start_var, duration, end_var + 2, presence_var, "interval"
    )

    print(f"interval = {repr(interval_var)}")

    # If the size is fixed, a simpler version uses the start expression and the
    # size.
    fixed_size_interval_var = model.new_optional_fixed_size_interval_var(
        start_var, 10, presence_var, "fixed_size_interval_var"
    )
    print(f"fixed_size_interval_var = {repr(fixed_size_interval_var)}")

    # A fixed interval can be created using the same API.
    fixed_interval = model.new_optional_fixed_size_interval_var(
        5, 10, presence_var, "fixed_interval"
    )
    print(f"fixed_interval = {repr(fixed_interval)}")


optional_interval_sample_sat()
```

### C++ code

```cpp
#include <stdlib.h>

#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"
#include "ortools/util/sorted_interval_list.h"

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
      cp_model.NewOptionalIntervalVar(x, y, z + 2, presence_var)
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
    IntervalVar intervalVar = model.newOptionalIntervalVar(startVar, LinearExpr.constant(10),
        LinearExpr.newBuilder().add(endVar).add(2), presence, "interval");
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
        BoolVar presence_var = model.NewBoolVar("presence");
        IntervalVar interval = model.NewOptionalIntervalVar(start_var, 10, end_var + 2, presence_var, "interval");
        Console.WriteLine(interval);

        // If the size is fixed, a simpler version uses the start expression, the size and the
        // literal.
        IntervalVar fixedSizeIntervalVar =
            model.NewOptionalFixedSizeIntervalVar(start_var, 10, presence_var, "fixed_size_interval_var");
        Console.WriteLine(fixedSizeIntervalVar);

        // A fixed interval can be created using the same API.
        IntervalVar fixedInterval = model.NewOptionalFixedSizeIntervalVar(5, 10, presence_var, "fixed_interval");
        Console.WriteLine(fixedInterval);
    }
}
```

## NoOverlap constraint

A no_overlap constraint simply states that all intervals are disjoint. It is
built with a list of interval variables. Fixed intervals are useful for
excluding part of the timeline.

In the following examples. We want to schedule 3 tasks on 3 weeks excluding
weekends, making the final day as early as possible.

### Python code

```python
#!/usr/bin/env python3
"""Code sample to demonstrate how to build a NoOverlap constraint."""

from ortools.sat.python import cp_model


def no_overlap_sample_sat():
    """No overlap sample with fixed activities."""
    model = cp_model.CpModel()
    horizon = 21  # 3 weeks.

    # Task 0, duration 2.
    start_0 = model.new_int_var(0, horizon, "start_0")
    duration_0 = 2  # Python cp/sat code accepts integer variables or constants.
    end_0 = model.new_int_var(0, horizon, "end_0")
    task_0 = model.new_interval_var(start_0, duration_0, end_0, "task_0")
    # Task 1, duration 4.
    start_1 = model.new_int_var(0, horizon, "start_1")
    duration_1 = 4  # Python cp/sat code accepts integer variables or constants.
    end_1 = model.new_int_var(0, horizon, "end_1")
    task_1 = model.new_interval_var(start_1, duration_1, end_1, "task_1")

    # Task 2, duration 3.
    start_2 = model.new_int_var(0, horizon, "start_2")
    duration_2 = 3  # Python cp/sat code accepts integer variables or constants.
    end_2 = model.new_int_var(0, horizon, "end_2")
    task_2 = model.new_interval_var(start_2, duration_2, end_2, "task_2")

    # Weekends.
    weekend_0 = model.new_interval_var(5, 2, 7, "weekend_0")
    weekend_1 = model.new_interval_var(12, 2, 14, "weekend_1")
    weekend_2 = model.new_interval_var(19, 2, 21, "weekend_2")

    # No Overlap constraint.
    model.add_no_overlap([task_0, task_1, task_2, weekend_0, weekend_1, weekend_2])

    # Makespan objective.
    obj = model.new_int_var(0, horizon, "makespan")
    model.add_max_equality(obj, [end_0, end_1, end_2])
    model.minimize(obj)

    # Solve model.
    solver = cp_model.CpSolver()
    status = solver.solve(model)

    if status == cp_model.OPTIMAL:
        # Print out makespan and the start times for all tasks.
        print(f"Optimal Schedule Length: {solver.objective_value}")
        print(f"Task 0 starts at {solver.value(start_0)}")
        print(f"Task 1 starts at {solver.value(start_1)}")
        print(f"Task 2 starts at {solver.value(start_2)}")
    else:
        print(f"Solver exited with nonoptimal status: {status}")


no_overlap_sample_sat()
```

### C++ code

```cpp
#include <stdlib.h>

#include <cstdint>

#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/model.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

void NoOverlapSampleSat() {
  CpModelBuilder cp_model;
  const int64_t kHorizon = 21;  // 3 weeks.

  const Domain horizon(0, kHorizon);
  // Task 0, duration 2.
  const IntVar start_0 = cp_model.NewIntVar(horizon);
  const int64_t duration_0 = 2;
  const IntVar end_0 = cp_model.NewIntVar(horizon);
  const IntervalVar task_0 =
      cp_model.NewIntervalVar(start_0, duration_0, end_0);

  // Task 1, duration 4.
  const IntVar start_1 = cp_model.NewIntVar(horizon);
  const int64_t duration_1 = 4;
  const IntVar end_1 = cp_model.NewIntVar(horizon);
  const IntervalVar task_1 =
      cp_model.NewIntervalVar(start_1, duration_1, end_1);

  // Task 2, duration 3.
  const IntVar start_2 = cp_model.NewIntVar(horizon);
  const int64_t duration_2 = 3;
  const IntVar end_2 = cp_model.NewIntVar(horizon);
  const IntervalVar task_2 =
      cp_model.NewIntervalVar(start_2, duration_2, end_2);

  // Week ends.
  const IntervalVar weekend_0 = cp_model.NewIntervalVar(5, 2, 7);
  const IntervalVar weekend_1 = cp_model.NewIntervalVar(12, 2, 14);
  const IntervalVar weekend_2 = cp_model.NewIntervalVar(19, 2, 21);

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
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverStatus;
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
    IntervalVar task0 = model.newFixedSizeIntervalVar(start0, duration0, "task0");

    //  Task 1, duration 4.
    IntVar start1 = model.newIntVar(0, horizon, "start1");
    int duration1 = 4;
    IntervalVar task1 = model.newFixedSizeIntervalVar(start1, duration1, "task1");

    // Task 2, duration 3.
    IntVar start2 = model.newIntVar(0, horizon, "start2");
    int duration2 = 3;
    IntervalVar task2 = model.newFixedSizeIntervalVar(start2, duration2, "task2");

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
    model.addMaxEquality(obj,
        new LinearExpr[] {LinearExpr.newBuilder().add(start0).add(duration0).build(),
            LinearExpr.newBuilder().add(start1).add(duration1).build(),
            LinearExpr.newBuilder().add(start2).add(duration2).build()});
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

## Cumulative constraint with varying capacity profile.

A cumulative constraint takes a list of intervals, and a list of demands, and a
capacity. It enforces that at any time point, the sum of demands of tasks active
at that time point is less than a given capacity.

Modeling a varying profile can be done using fixed (interval, demand) to occupy
the capacity between the actual profile and it max capacity.

### Python code

```python
#!/usr/bin/env python3
"""Solves a simple scheduling problem with a variable work load."""

import io

import pandas as pd

from ortools.sat.python import cp_model


def create_data_model() -> tuple[pd.DataFrame, pd.DataFrame]:
    """Creates the two dataframes that describes the model."""

    capacity_str: str = """
  start_hour  capacity
     0            0
     2            0
     4            1
     6            3
     8            6
    10           12
    12            8
    14           12
    16           10
    18            4
    20            2
    22            0
  """

    tasks_str: str = """
  name  duration load  priority
   t1      60      3      2
   t2     180      2      1
   t3     240      5      3
   t4      90      4      2
   t5     120      3      1
   t6     300      3      3
   t7     120      1      2
   t8     100      5      2
   t9     110      2      1
   t10    300      5      3
   t11     90      4      2
   t12    120      3      1
   t13    250      3      3
   t14    120      1      2
   t15     40      5      3
   t16     70      4      2
   t17     90      8      1
   t18     40      3      3
   t19    120      5      2
   t20     60      3      2
   t21    180      2      1
   t22    240      5      3
   t23     90      4      2
   t24    120      3      1
   t25    300      3      3
   t26    120      1      2
   t27    100      5      2
   t28    110      2      1
   t29    300      5      3
   t30     90      4      2
  """

    capacity_df = pd.read_table(io.StringIO(capacity_str), sep=r"\s+")
    tasks_df = pd.read_table(io.StringIO(tasks_str), index_col=0, sep=r"\s+")
    return capacity_df, tasks_df


def main():
    """Create the model and solves it."""
    capacity_df, tasks_df = create_data_model()

    # Create the model.
    model = cp_model.CpModel()

    # Get the max capacity from the capacity dataframe.
    max_capacity = capacity_df.capacity.max()
    print(f"Max capacity = {max_capacity}")
    print(f"#tasks = {len(tasks_df)}")

    minutes_per_period: int = 120
    horizon: int = 24 * 60

    # Variables
    starts = model.new_int_var_series(
        name="starts", lower_bounds=0, upper_bounds=horizon, index=tasks_df.index
    )
    performed = model.new_bool_var_series(name="performed", index=tasks_df.index)

    intervals = model.new_optional_fixed_size_interval_var_series(
        name="intervals",
        index=tasks_df.index,
        starts=starts,
        sizes=tasks_df.duration,
        are_present=performed,
    )

    # Set up the profile. We use fixed (intervals, demands) to fill in the space
    # between the actual load profile and the max capacity.
    time_period_intervals = model.new_fixed_size_interval_var_series(
        name="time_period_intervals",
        index=capacity_df.index,
        starts=capacity_df.start_hour * minutes_per_period,
        sizes=minutes_per_period,
    )
    time_period_heights = max_capacity - capacity_df.capacity

    # Cumulative constraint.
    model.add_cumulative(
        intervals.to_list() + time_period_intervals.to_list(),
        tasks_df.load.to_list() + time_period_heights.to_list(),
        max_capacity,
    )

    # Objective: maximize the value of performed intervals.
    # 1 is the max priority.
    max_priority = max(tasks_df.priority)
    model.maximize(sum(performed * (max_priority + 1 - tasks_df.priority)))

    # Create the solver and solve the model.
    solver = cp_model.CpSolver()
    solver.parameters.log_search_progress = True
    solver.parameters.num_workers = 8
    solver.parameters.max_time_in_seconds = 30.0
    status = solver.solve(model)

    if status == cp_model.OPTIMAL or status == cp_model.FEASIBLE:
        start_values = solver.values(starts)
        performed_values = solver.boolean_values(performed)
        for task in tasks_df.index:
            if performed_values[task]:
                print(f"task {task} starts at {start_values[task]}")
            else:
                print(f"task {task} is not performed")
    elif status == cp_model.INFEASIBLE:
        print("No solution found")
    else:
        print("Something is wrong, check the status and the log of the solve")


if __name__ == "__main__":
    main()
```

## Alternative resources for one interval

## Ranking tasks in a disjunctive resource

To rank intervals in a no_overlap constraint, we will count the number of
performed intervals that precede each interval.

This is slightly complicated if some interval variables are optional. To
implement it, we will create a matrix of `precedences` boolean variables.
`precedences[i][j]` is set to true if and only if interval `i` is performed,
interval `j` is performed, and if the start of `i` is before the start of `j`.

Furthermore, `precedences[i][i]` is set to be equal to `presences[i]`. This way,
we can define the rank of an interval `i` as `sum over j(precedences[j][i]) -
1`. If the interval is not performed, the rank computed as -1, if the interval
is performed, its presence variable negates the -1, and the formula counts the
number of other intervals that precede it.

### Python code

```python
#!/usr/bin/env python3
"""Code sample to demonstrates how to rank intervals."""

from ortools.sat.python import cp_model


def rank_tasks(
    model: cp_model.CpModel,
    starts: list[cp_model.IntVar],
    presences: list[cp_model.IntVar],
    ranks: list[cp_model.IntVar],
):
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
                prec = model.new_bool_var(f"{i} before {j}")
                precedences[(i, j)] = prec
                model.add(starts[i] < starts[j]).only_enforce_if(prec)

    # Treats optional intervals.
    for i in range(num_tasks - 1):
        for j in range(i + 1, num_tasks):
            tmp_array = [precedences[(i, j)], precedences[(j, i)]]
            if not cp_model.object_is_a_true_literal(presences[i]):
                tmp_array.append(presences[i].negated())
                # Makes sure that if i is not performed, all precedences are false.
                model.add_implication(
                    presences[i].negated(), precedences[(i, j)].negated()
                )
                model.add_implication(
                    presences[i].negated(), precedences[(j, i)].negated()
                )
            if not cp_model.object_is_a_true_literal(presences[j]):
                tmp_array.append(presences[j].negated())
                # Makes sure that if j is not performed, all precedences are false.
                model.add_implication(
                    presences[j].negated(), precedences[(i, j)].negated()
                )
                model.add_implication(
                    presences[j].negated(), precedences[(j, i)].negated()
                )
            # The following bool_or will enforce that for any two intervals:
            #    i precedes j or j precedes i or at least one interval is not
            #        performed.
            model.add_bool_or(tmp_array)
            # Redundant constraint: it propagates early that at most one precedence
            # is true.
            model.add_implication(precedences[(i, j)], precedences[(j, i)].negated())
            model.add_implication(precedences[(j, i)], precedences[(i, j)].negated())

    # Links precedences and ranks.
    for i in all_tasks:
        model.add(ranks[i] == sum(precedences[(j, i)] for j in all_tasks) - 1)


def ranking_sample_sat():
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
        start = model.new_int_var(0, horizon, f"start[{t}]")
        duration = t + 1
        end = model.new_int_var(0, horizon, f"end[{t}]")
        if t < num_tasks // 2:
            interval = model.new_interval_var(start, duration, end, f"interval[{t}]")
            presence = True
        else:
            presence = model.new_bool_var(f"presence[{t}]")
            interval = model.new_optional_interval_var(
                start, duration, end, presence, f"o_interval[{t}]"
            )
        starts.append(start)
        ends.append(end)
        intervals.append(interval)
        presences.append(presence)

        # Ranks = -1 if and only if the tasks is not performed.
        ranks.append(model.new_int_var(-1, num_tasks - 1, f"rank[{t}]"))

    # Adds NoOverlap constraint.
    model.add_no_overlap(intervals)

    # Adds ranking constraint.
    rank_tasks(model, starts, presences, ranks)

    # Adds a constraint on ranks.
    model.add(ranks[0] < ranks[1])

    # Creates makespan variable.
    makespan = model.new_int_var(0, horizon, "makespan")
    for t in all_tasks:
        model.add(ends[t] <= makespan).only_enforce_if(presences[t])

    # Minimizes makespan - fixed gain per tasks performed.
    # As the fixed cost is less that the duration of the last interval,
    # the solver will not perform the last interval.
    model.minimize(2 * makespan - 7 * sum(presences[t] for t in all_tasks))

    # Solves the model model.
    solver = cp_model.CpSolver()
    status = solver.solve(model)

    if status == cp_model.OPTIMAL:
        # Prints out the makespan and the start times and ranks of all tasks.
        print(f"Optimal cost: {solver.objective_value}")
        print(f"Makespan: {solver.value(makespan)}")
        for t in all_tasks:
            if solver.value(presences[t]):
                print(
                    f"Task {t} starts at {solver.value(starts[t])} "
                    f"with rank {solver.value(ranks[t])}"
                )
            else:
                print(
                    f"Task {t} in not performed and ranked at {solver.value(ranks[t])}"
                )
    else:
        print(f"Solver exited with nonoptimal status: {status}")


ranking_sample_sat()
```

### C++ code

```cpp
#include <stdint.h>
#include <stdlib.h>

#include <vector>

#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

void RankingSampleSat() {
  CpModelBuilder cp_model;
  const int kHorizon = 100;
  const int kNumTasks = 4;

  auto add_task_ranking = [&cp_model](absl::Span<const IntVar> starts,
                                      absl::Span<const BoolVar> presences,
                                      absl::Span<const IntVar> ranks) {
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
        cp_model.AddImplication(Not(presences[j]), Not(precedences[j][i]));
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
        sum_of_predecessors += precedences[j][i];
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
    const int64_t duration = t + 1;
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
  LinearExpr objective = 2 * makespan;
  for (int t = 0; t < kNumTasks; ++t) {
    objective -= 7 * presences[t];
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
import com.google.ortools.sat.BoolVar;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.IntervalVar;
import com.google.ortools.sat.LinearExpr;
import com.google.ortools.sat.LinearExprBuilder;
import com.google.ortools.sat.Literal;
import java.util.ArrayList;
import java.util.List;

/** Code sample to demonstrates how to rank intervals. */
public class RankingSampleSat {
  /**
   * This code takes a list of interval variables in a noOverlap constraint, and a parallel list of
   * integer variables and enforces the following constraint
   *
   * <ul>
   *   <li>rank[i] == -1 iff interval[i] is not active.
   *   <li>rank[i] == number of active intervals that precede interval[i].
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
          BoolVar prec = model.newBoolVar(String.format("%d before %d", i, j));
          precedences[i][j] = prec;
          // Ensure that task i precedes task j if prec is true.
          model.addLessThan(starts[i], starts[j]).onlyEnforceIf(prec);
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
        model.addBoolOr(list);
        // For efficiency, we add a redundant constraint declaring that only one of i precedes j and
        // j precedes i are true. This will speed up the solve because the reason of this
        // propagation is shorter that using interval bounds is true.
        model.addImplication(precedences[i][j], precedences[j][i].not());
        model.addImplication(precedences[j][i], precedences[i][j].not());
      }
    }

    // Links precedences and ranks.
    for (int i = 0; i < numTasks; ++i) {
      // ranks == sum(precedences) - 1;
      LinearExprBuilder expr = LinearExpr.newBuilder();
      for (int j = 0; j < numTasks; ++j) {
        expr.add(precedences[j][i]);
      }
      expr.add(-1);
      model.addEquality(ranks[i], expr);
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

    Literal trueLiteral = model.trueLiteral();

    // Creates intervals, half of them are optional.
    for (int t = 0; t < numTasks; ++t) {
      starts[t] = model.newIntVar(0, horizon, "start_" + t);
      int duration = t + 1;
      ends[t] = model.newIntVar(0, horizon, "end_" + t);
      if (t < numTasks / 2) {
        intervals[t] = model.newIntervalVar(
            starts[t], LinearExpr.constant(duration), ends[t], "interval_" + t);
        presences[t] = trueLiteral;
      } else {
        presences[t] = model.newBoolVar("presence_" + t);
        intervals[t] = model.newOptionalIntervalVar(
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
    model.addLessThan(ranks[0], ranks[1]);

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
    LinearExprBuilder obj = LinearExpr.newBuilder();
    for (int t = 0; t < numTasks; ++t) {
      obj.addTerm(presences[t], -7);
    }
    obj.addTerm(makespan, 2);
    model.minimize(obj);

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.solve(model);

    if (status == CpSolverStatus.OPTIMAL) {
      System.out.println("Optimal cost: " + solver.objectiveValue());
      System.out.println("Makespan: " + solver.value(makespan));
      for (int t = 0; t < numTasks; ++t) {
        if (solver.booleanValue(presences[t])) {
          System.out.printf("Task %d starts at %d with rank %d%n", t, solver.value(starts[t]),
              solver.value(ranks[t]));
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
                    BoolVar prec = model.NewBoolVar(String.Format("{0} before {1}", i, j));
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
            List<IntVar> tasks = new List<IntVar>();
            for (int j = 0; j < num_tasks; ++j)
            {
                tasks.Add((IntVar)precedences[j, i]);
            }
            model.Add(ranks[i] == LinearExpr.Sum(tasks) - 1);
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

        ILiteral true_var = model.TrueLiteral();

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

## Ranking tasks in a disjunctive resource with a circuit constraint.

To rank intervals in a no_overlap constraint, we will use a circuit constraint
to perform the transitive reduction from precedences to successors.

This is slightly complicated if some interval variables are optional, and we
need to take into account the case where no task is performed.

### Python code

```python
#!/usr/bin/env python3
"""Code sample to demonstrates how to rank intervals using a circuit."""

from typing import List, Sequence


from ortools.sat.python import cp_model


def rank_tasks_with_circuit(
    model: cp_model.CpModel,
    starts: Sequence[cp_model.IntVar],
    durations: Sequence[int],
    presences: Sequence[cp_model.IntVar],
    ranks: Sequence[cp_model.IntVar],
):
    """This method uses a circuit constraint to rank tasks.

    This method assumes that all starts are disjoint, meaning that all tasks have
    a strictly positive duration, and they appear in the same NoOverlap
    constraint.

    To implement this ranking, we will create a dense graph with num_tasks + 1
    nodes.
    The extra node (with id 0) will be used to decide which task is first with
    its only outgoing arc, and whhich task is last with its only incoming arc.
    Each task i will be associated with id i + 1, and an arc between i + 1 and j +
    1 indicates that j is the immediate successor of i.

    The circuit constraint ensures there is at most 1 hamiltonian path of
    length > 1. If no such path exists, then no tasks are active.

    The multiple enforced linear constraints are meant to ensure the compatibility
    between the order of starts and the order of ranks,

    Args:
      model: The CpModel to add the constraints to.
      starts: The array of starts variables of all tasks.
      durations: the durations of all tasks.
      presences: The array of presence variables of all tasks.
      ranks: The array of rank variables of all tasks.
    """

    num_tasks = len(starts)
    all_tasks = range(num_tasks)

    arcs: List[cp_model.ArcT] = []
    for i in all_tasks:
        # if node i is first.
        start_lit = model.new_bool_var(f"start_{i}")
        arcs.append((0, i + 1, start_lit))
        model.add(ranks[i] == 0).only_enforce_if(start_lit)

        # As there are no other constraints on the problem, we can add this
        # redundant constraint.
        model.add(starts[i] == 0).only_enforce_if(start_lit)

        # if node i is last.
        end_lit = model.new_bool_var(f"end_{i}")
        arcs.append((i + 1, 0, end_lit))

        for j in all_tasks:
            if i == j:
                arcs.append((i + 1, i + 1, presences[i].negated()))
                model.add(ranks[i] == -1).only_enforce_if(presences[i].negated())
            else:
                literal = model.new_bool_var(f"arc_{i}_to_{j}")
                arcs.append((i + 1, j + 1, literal))
                model.add(ranks[j] == ranks[i] + 1).only_enforce_if(literal)

                # To perform the transitive reduction from precedences to successors,
                # we need to tie the starts of the tasks with 'literal'.
                # In a pure problem, the following inequality could be an equality.
                # It is not true in general.
                #
                # Note that we could use this literal to penalize the transition, add an
                # extra delay to the precedence.
                model.add(starts[j] >= starts[i] + durations[i]).only_enforce_if(
                    literal
                )

    # Manage the empty circuit
    empty = model.new_bool_var("empty")
    arcs.append((0, 0, empty))

    for i in all_tasks:
        model.add_implication(empty, presences[i].negated())

    # Add the circuit constraint.
    model.add_circuit(arcs)


def ranking_sample_sat():
    """Ranks tasks in a NoOverlap constraint."""

    model = cp_model.CpModel()
    horizon = 100
    num_tasks = 4
    all_tasks = range(num_tasks)

    starts = []
    durations = []
    intervals = []
    presences = []
    ranks = []

    # Creates intervals, half of them are optional.
    for t in all_tasks:
        start = model.new_int_var(0, horizon, f"start[{t}]")
        duration = t + 1
        presence = model.new_bool_var(f"presence[{t}]")
        interval = model.new_optional_fixed_size_interval_var(
            start, duration, presence, f"opt_interval[{t}]"
        )
        if t < num_tasks // 2:
            model.add(presence == 1)

        starts.append(start)
        durations.append(duration)
        intervals.append(interval)
        presences.append(presence)

        # Ranks = -1 if and only if the tasks is not performed.
        ranks.append(model.new_int_var(-1, num_tasks - 1, f"rank[{t}]"))

    # Adds NoOverlap constraint.
    model.add_no_overlap(intervals)

    # Adds ranking constraint.
    rank_tasks_with_circuit(model, starts, durations, presences, ranks)

    # Adds a constraint on ranks.
    model.add(ranks[0] < ranks[1])

    # Creates makespan variable.
    makespan = model.new_int_var(0, horizon, "makespan")
    for t in all_tasks:
        model.add(starts[t] + durations[t] <= makespan).only_enforce_if(presences[t])

    # Minimizes makespan - fixed gain per tasks performed.
    # As the fixed cost is less that the duration of the last interval,
    # the solver will not perform the last interval.
    model.minimize(2 * makespan - 7 * sum(presences[t] for t in all_tasks))

    # Solves the model model.
    solver = cp_model.CpSolver()
    status = solver.solve(model)

    if status == cp_model.OPTIMAL:
        # Prints out the makespan and the start times and ranks of all tasks.
        print(f"Optimal cost: {solver.objective_value}")
        print(f"Makespan: {solver.value(makespan)}")
        for t in all_tasks:
            if solver.value(presences[t]):
                print(
                    f"Task {t} starts at {solver.value(starts[t])} "
                    f"with rank {solver.value(ranks[t])}"
                )
            else:
                print(
                    f"Task {t} in not performed and ranked at {solver.value(ranks[t])}"
                )
    else:
        print(f"Solver exited with nonoptimal status: {status}")


ranking_sample_sat()
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
#!/usr/bin/env python3
"""Code sample to demonstrate how an interval can span across a break."""

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


def scheduling_with_calendar_sample_sat():
    """Interval spanning across a lunch break."""
    model = cp_model.CpModel()

    # The data is the following:
    #   Work starts at 8h, ends at 18h, with a lunch break between 13h and 14h.
    #   We need to schedule a task that needs 3 hours of processing time.
    #   Total duration can be 3 or 4 (if it spans the lunch break).
    #
    # Because the duration is at least 3 hours, work cannot start after 15h.
    # Because of the break, work cannot start at 13h.

    start = model.new_int_var_from_domain(
        cp_model.Domain.from_intervals([(8, 12), (14, 15)]), "start"
    )
    duration = model.new_int_var(3, 4, "duration")
    end = model.new_int_var(8, 18, "end")
    unused_interval = model.new_interval_var(start, duration, end, "interval")

    # We have 2 states (spanning across lunch or not)
    across = model.new_bool_var("across")
    non_spanning_hours = cp_model.Domain.from_values([8, 9, 10, 14, 15])
    model.add_linear_expression_in_domain(start, non_spanning_hours).only_enforce_if(
        across.negated()
    )
    model.add_linear_constraint(start, 11, 12).only_enforce_if(across)
    model.add(duration == 3).only_enforce_if(across.negated())
    model.add(duration == 4).only_enforce_if(across)

    # Search for x values in increasing order.
    model.add_decision_strategy(
        [start], cp_model.CHOOSE_FIRST, cp_model.SELECT_MIN_VALUE
    )

    # Create a solver and solve with a fixed search.
    solver = cp_model.CpSolver()

    # Force the solver to follow the decision strategy exactly.
    solver.parameters.search_branching = cp_model.FIXED_SEARCH
    # Enumerate all solutions.
    solver.parameters.enumerate_all_solutions = True

    # Search and print all solutions.
    solution_printer = VarArraySolutionPrinter([start, duration, across])
    solver.solve(model, solution_printer)


scheduling_with_calendar_sample_sat()
```

## Detecting if two intervals overlap.

We want a Boolean variable to be true if and only if two intervals overlap. To
enforce this, we will create 3 Boolean variables, link two of them to the
relative positions of the two intervals, and define the third one using the
other two Boolean variables.

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
#!/usr/bin/env python3
"""Code sample to demonstrates how to detect if two intervals overlap."""

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


def overlapping_interval_sample_sat():
    """Create the overlapping Boolean variables and enumerate all states."""
    model = cp_model.CpModel()

    horizon = 7

    # First interval.
    start_var_a = model.new_int_var(0, horizon, "start_a")
    duration_a = 3
    end_var_a = model.new_int_var(0, horizon, "end_a")
    unused_interval_var_a = model.new_interval_var(
        start_var_a, duration_a, end_var_a, "interval_a"
    )

    # Second interval.
    start_var_b = model.new_int_var(0, horizon, "start_b")
    duration_b = 2
    end_var_b = model.new_int_var(0, horizon, "end_b")
    unused_interval_var_b = model.new_interval_var(
        start_var_b, duration_b, end_var_b, "interval_b"
    )

    # a_after_b Boolean variable.
    a_after_b = model.new_bool_var("a_after_b")
    model.add(start_var_a >= end_var_b).only_enforce_if(a_after_b)
    model.add(start_var_a < end_var_b).only_enforce_if(a_after_b.negated())

    # b_after_a Boolean variable.
    b_after_a = model.new_bool_var("b_after_a")
    model.add(start_var_b >= end_var_a).only_enforce_if(b_after_a)
    model.add(start_var_b < end_var_a).only_enforce_if(b_after_a.negated())

    # Result Boolean variable.
    a_overlaps_b = model.new_bool_var("a_overlaps_b")

    # Option a: using only clauses
    model.add_bool_or(a_after_b, b_after_a, a_overlaps_b)
    model.add_implication(a_after_b, a_overlaps_b.negated())
    model.add_implication(b_after_a, a_overlaps_b.negated())

    # Option b: using an exactly one constraint.
    # model.add_exactly_one(a_after_b, b_after_a, a_overlaps_b)

    # Search for start values in increasing order for the two intervals.
    model.add_decision_strategy(
        [start_var_a, start_var_b],
        cp_model.CHOOSE_FIRST,
        cp_model.SELECT_MIN_VALUE,
    )

    # Create a solver and solve with a fixed search.
    solver = cp_model.CpSolver()

    # Force the solver to follow the decision strategy exactly.
    solver.parameters.search_branching = cp_model.FIXED_SEARCH
    # Enumerate all solutions.
    solver.parameters.enumerate_all_solutions = True

    # Search and print out all solutions.
    solution_printer = VarArraySolutionPrinter([start_var_a, start_var_b, a_overlaps_b])
    solver.solve(model, solution_printer)


overlapping_interval_sample_sat()
```

## Transitions in a disjunctive resource

## Precedences between intervals

## Convex hull of a set of intervals
