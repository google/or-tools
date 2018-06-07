# Scheduling recipes for the CP-SAT solver.



## Introduction

Scheduling in Operations Research deals with tasks that times. In general,
scheduling problems have the following features: fixed or variable durations,
alternate ways of performing the same task, mutual exclusivity between tasks,
and temporal relations between tasks.

## Interval variables

Intervals are constraints containing three integer variables (start, size, and
end). Creating an interval constraint will enforce that start + size == end.

To create an interval, we need to create three variables, for the start, the
size, and the end of the interval. Then we create an interval constraint using
these three variables.

We give two code snippets the demonstrate how to build these objects in python,
C++, and C\#.

### Python code

```python
from ortools.sat.python import cp_model

def IntervalSample():
  model = cp_model.CpModel()
  horizon = 100
  start_var = model.NewIntVar(0, horizon, 'start')
  duration = 10  # Python cp/sat code accept integer variables or constants.
  end_var = model.NewIntVar(0, horizon, 'end')
  interval_var = model.NewIntervalVar(start_var, duration, end_var, 'interval')
  print('start = %s, duration = %i, end = %s, interval = %s' %
        (start_var, duration, end_var, interval_var))
```

### C++ code

```cpp
#include "ortools/sat/cp_model.pb.h"

namespace operations_research {
namespace sat {

void IntervalSample() {
  CpModelProto cp_model;
  const int kHorizon = 100;

  auto new_variable = [&cp_model](int64 lb, int64 ub) {
    CHECK_LE(lb, ub);
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(lb);
    var->add_domain(ub);
    return index;
  };

  auto new_constant = [&cp_model, &new_variable](int64 v) {
    return new_variable(v, v);
  };

  auto new_interval = [&cp_model](int start, int duration, int end) {
    const int index = cp_model.constraints_size();
    IntervalConstraintProto* const interval =
        cp_model.add_constraints()->mutable_interval();
    interval->set_start(start);
    interval->set_size(duration);
    interval->set_end(end);
    return index;
  };

  const int start_var = new_variable(0, kHorizon);
  const int duration_var = new_constant(10);
  const int end_var = new_variable(0, kHorizon);
  const int interval_var = new_interval(start_var, duration_var, end_var);
  LOG(INFO) << "start_var = " << start_var
            << ", duration_var = " << duration_var << ", end_var = " << end_var
            << ", interval_var = " << interval_var;
}

}  // namespace sat
}  // namespace operations_research
```

### C\# code

```cs
using System;
using Google.OrTools.Sat;

public class CodeSamplesSat
{
  static void IntervalSample()
  {
    CpModel model = new CpModel();
    int horizon = 100;
    IntVar start_var = model.NewIntVar(0, horizon, "start");
    // C# code supports IntVar or integer constants in intervals.
    int duration = 10;
    IntVar end_var = model.NewIntVar(0, horizon, "end");
    IntervalVar interval =
        model.NewIntervalVar(start_var, duration, end_var, "interval");
  }

  static void Main()
  {
    IntervalSample();
  }
}
```

## Optional intervals

An interval can be marked as optional. The presence of this interval is
controlled by a literal. The **no overlap** and **cumulative** constraints
understand these presence literals, and correctly ignore inactive intervals.

### Python code

```python
from ortools.sat.python import cp_model

def OptionalIntervalSample():
  model = cp_model.CpModel()
  horizon = 100
  start_var = model.NewIntVar(0, horizon, 'start')
  duration = 10  # Python cp/sat code accept integer variables or constants.
  end_var = model.NewIntVar(0, horizon, 'end')
  presence = model.NewBoolVar('presence')
  interval_var = model.NewOptionalIntervalVar(start_var, duration, end_var,
                                              presence, 'interval')
  print('start = %s, duration = %i, end = %s, interval = %s' %
        (start_var, duration, end_var, interval_var))
```

### C++ code

```cpp
#include "ortools/sat/cp_model.pb.h"

namespace operations_research {
namespace sat {

void OptionalIntervalSample() {
  CpModelProto cp_model;
  const int kHorizon = 100;

  auto new_variable = [&cp_model](int64 lb, int64 ub) {
    CHECK_LE(lb, ub);
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(lb);
    var->add_domain(ub);
    return index;
  };

  auto new_constant = [&cp_model, &new_variable](int64 v) {
    return new_variable(v, v);
  };

  auto new_optional_interval = [&cp_model](int start, int duration, int end,
                                          int presence) {
    const int index = cp_model.constraints_size();
    ConstraintProto* const ct = cp_model.add_constraints();
    ct->add_enforcement_literal(presence);
    IntervalConstraintProto* const interval = ct->mutable_interval();
    interval->set_start(start);
    interval->set_size(duration);
    interval->set_end(end);
    return index;
  };

  const int start_var = new_variable(0, kHorizon);
  const int duration_var = new_constant(10);
  const int end_var = new_variable(0, kHorizon);
  const int presence_var = new_variable(0, 1);
  const int interval_var = new_optional_interval(start_var, duration_var,
                                                 end_var, presence_var);
  LOG(INFO) << "start_var = " << start_var
            << ", duration_var = " << duration_var
            << ", end_var = " << end_var
            << ", presence_var = " << presence_var
            << ", interval_var = " << interval_var;
}

}  // namespace sat
}  // namespace operations_research
```

### C\# code

```cs
using System;
using Google.OrTools.Sat;

public class CodeSamplesSat
{
  static void OptionalIntervalSample()
  {
    CpModel model = new CpModel();
    int horizon = 100;
    IntVar start_var = model.NewIntVar(0, horizon, "start");
    // C# code supports IntVar or integer constants in intervals.
    int duration = 10;
    IntVar end_var = model.NewIntVar(0, horizon, "end");
    IntVar presence_var = model.NewBoolVar("presence");
    IntervalVar interval = model.NewOptionalIntervalVar(
        start_var, duration, end_var, presence_var, "interval");
  }

  static void Main()
  {
    OptionalIntervalSample();
  }
}
```

## NoOverlap constraint

A no overlap constraints simply states that  all intervals are disjoint.

## Cumulative resource

A cumulative constraint takes a list of intervals, and a list of demands, and a
capacity. It enforces that at any time point, the sum of demands of tasks active
at that time point is less than a given capacity.

## Alternative resources for one interval

## Transitions in a disjunctive resource

## Precedences between intervals

## Convex hull of a set of intervals
