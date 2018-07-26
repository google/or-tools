# Scheduling recipes for the CP-SAT solver.



## Introduction

Scheduling in Operations Research involves problems of tasks, resources and
times. In general, scheduling problems have the following features: fixed or
variable durations, alternate ways of performing the same task, mutual
exclusivity between tasks, and temporal relations between tasks.

## Interval variables

Intervals are constraints containing three integer variables (start, size, and
end). Creating an interval constraint will enforce that start + size == end.

4:15PM, Jun 20 To define an interval, we need to create three variables: start,
size, and end.Then we create an interval constraint using these three variables.

We give two code snippets the demonstrate how to build these objects in Python,
C++, and C\#.

### Python code

```python
"""Code sample to demonstrates how to build an interval."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

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


IntervalSample()
```

### C++ code

```cpp
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"

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

  auto new_constant = [&new_variable](int64 v) { return new_variable(v, v); };

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

int main() {
  operations_research::sat::IntervalSample();

  return EXIT_SUCCESS;
}
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
"""Code sample to demonstrates how to build an optional interval."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


def OptionalIntervalSample():
  model = cp_model.CpModel()
  horizon = 100
  start_var = model.NewIntVar(0, horizon, 'start')
  duration = 10  # Python cp/sat code accept integer variables or constants.
  end_var = model.NewIntVar(0, horizon, 'end')
  presence_var = model.NewBoolVar('presence')
  interval_var = model.NewOptionalIntervalVar(start_var, duration, end_var,
                                              presence_var, 'interval')
  print('start = %s, duration = %i, end = %s, presence = %s, interval = %s' %
        (start_var, duration, end_var, presence_var, interval_var))


OptionalIntervalSample()
```

### C++ code

```cpp
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"

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

  auto new_constant = [&new_variable](int64 v) { return new_variable(v, v); };

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
  const int interval_var =
      new_optional_interval(start_var, duration_var, end_var, presence_var);
  LOG(INFO) << "start_var = " << start_var
            << ", duration_var = " << duration_var << ", end_var = " << end_var
            << ", presence_var = " << presence_var
            << ", interval_var = " << interval_var;
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::OptionalIntervalSample();

  return EXIT_SUCCESS;
}
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

A NoOverlap constraint simply states that all intervals are disjoint. It is
built with a list of interval variables. Fixed intervals are useful for
excluding part of the timeline.

In the following examples. We want to schedule 3 tasks on 3 weeks excluding
weekends, making the final day as early as possible.

### Python code

```python
"""Code sample to demonstrate how to build a NoOverlap constraint."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


def NoOverlapSample():
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


NoOverlapSample()
```

### C++ code

```cpp
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

void NoOverlapSample() {
  CpModelProto cp_model;
  const int kHorizon = 21;  // 3 weeks.

  auto new_variable = [&cp_model](int64 lb, int64 ub) {
    CHECK_LE(lb, ub);
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(lb);
    var->add_domain(ub);
    return index;
  };

  auto new_constant = [&new_variable](int64 v) { return new_variable(v, v); };

  auto new_interval = [&cp_model](int start, int duration, int end) {
    const int index = cp_model.constraints_size();
    IntervalConstraintProto* const interval =
        cp_model.add_constraints()->mutable_interval();
    interval->set_start(start);
    interval->set_size(duration);
    interval->set_end(end);
    return index;
  };

  auto new_fixed_interval = [&cp_model, &new_constant](int64 start,
                                                       int64 duration) {
    const int index = cp_model.constraints_size();
    IntervalConstraintProto* const interval =
        cp_model.add_constraints()->mutable_interval();
    interval->set_start(new_constant(start));
    interval->set_size(new_constant(duration));
    interval->set_end(new_constant(start + duration));
    return index;
  };

  auto add_no_overlap = [&cp_model](const std::vector<int>& intervals) {
    NoOverlapConstraintProto* const no_overlap =
        cp_model.add_constraints()->mutable_no_overlap();
    for (const int i : intervals) {
      no_overlap->add_intervals(i);
    }
  };

  auto add_precedence = [&cp_model](int before, int after) {
    LinearConstraintProto* const lin =
        cp_model.add_constraints()->mutable_linear();
    lin->add_vars(after);
    lin->add_coeffs(1L);
    lin->add_vars(before);
    lin->add_coeffs(-1L);
    lin->add_domain(0);
    lin->add_domain(kint64max);
  };

  // Task 0, duration 2.
  const int start_0 = new_variable(0, kHorizon);
  const int duration_0 = new_constant(2);
  const int end_0 = new_variable(0, kHorizon);
  const int task_0 = new_interval(start_0, duration_0, end_0);

  // Task 1, duration 4.
  const int start_1 = new_variable(0, kHorizon);
  const int duration_1 = new_constant(4);
  const int end_1 = new_variable(0, kHorizon);
  const int task_1 = new_interval(start_1, duration_1, end_1);

  // Task 2, duration 3.
  const int start_2 = new_variable(0, kHorizon);
  const int duration_2 = new_constant(3);
  const int end_2 = new_variable(0, kHorizon);
  const int task_2 = new_interval(start_2, duration_2, end_2);

  // Week ends.
  const int weekend_0 = new_fixed_interval(5, 2);
  const int weekend_1 = new_fixed_interval(12, 2);
  const int weekend_2 = new_fixed_interval(19, 2);

  // No Overlap constraint.
  add_no_overlap({task_0, task_1, task_2, weekend_0, weekend_1, weekend_2});

  // Makespan.
  const int makespan = new_variable(0, kHorizon);
  add_precedence(end_0, makespan);
  add_precedence(end_1, makespan);
  add_precedence(end_2, makespan);
  CpObjectiveProto* const obj = cp_model.mutable_objective();
  obj->add_vars(makespan);
  obj->add_coeffs(1);  // Minimization.

  // Solving part.
  Model model;
  LOG(INFO) << CpModelStats(cp_model);
  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  LOG(INFO) << CpSolverResponseStats(response);

  if (response.status() == CpSolverStatus::OPTIMAL) {
    LOG(INFO) << "Optimal Schedule Length: " << response.objective_value();
    LOG(INFO) << "Task 0 starts at " << response.solution(start_0);
    LOG(INFO) << "Task 1 starts at " << response.solution(start_1);
    LOG(INFO) << "Task 2 starts at " << response.solution(start_2);
  }
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::NoOverlapSample();

  return EXIT_SUCCESS;
}
```

### C\# code

```cs
using System;
using Google.OrTools.Sat;

public class CodeSamplesSat
{
  static void NoOverlapSample()
  {
    CpModel model = new CpModel();
    // Three weeks.
    int horizon = 21;

    // Task 0, duration 2.
    IntVar start_0 = model.NewIntVar(0, horizon, "start_0");
    int duration_0 = 2;
    IntVar end_0 = model.NewIntVar(0, horizon, "end_0");
    IntervalVar task_0 =
        model.NewIntervalVar(start_0, duration_0, end_0, "task_0");

    //  Task 1, duration 4.
    IntVar start_1 = model.NewIntVar(0, horizon, "start_1");
    int duration_1 = 4;
    IntVar end_1 = model.NewIntVar(0, horizon, "end_1");
    IntervalVar task_1 =
        model.NewIntervalVar(start_1, duration_1, end_1, "task_1");

    // Task 2, duration 3.
    IntVar start_2 = model.NewIntVar(0, horizon, "start_2");
    int duration_2 = 3;
    IntVar end_2 = model.NewIntVar(0, horizon, "end_2");
    IntervalVar task_2 =
        model.NewIntervalVar(start_2, duration_2, end_2, "task_2");

    // Weekends.
    IntervalVar weekend_0 = model.NewIntervalVar(5, 2, 7, "weekend_0");
    IntervalVar weekend_1 = model.NewIntervalVar(12, 2, 14, "weekend_1");
    IntervalVar weekend_2 = model.NewIntervalVar(19, 2, 21, "weekend_2");

    // No Overlap constraint.
    model.AddNoOverlap(new IntervalVar[] {task_0, task_1, task_2, weekend_0,
                                          weekend_1, weekend_2});

    // Makespan objective.
    IntVar obj = model.NewIntVar(0, horizon, "makespan");
    model.AddMaxEquality(obj, new IntVar[] {end_0, end_1, end_2});
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

  static void Main()
  {
    NoOverlapSample();
  }
}
```

## Cumulative constraint

A cumulative constraint takes a list of intervals, and a list of demands, and a
capacity. It enforces that at any time point, the sum of demands of tasks active
at that time point is less than a given capacity.

## Alternative resources for one interval

## Ranking tasks in a disjunctive resource

To rank intervals in a NoOverlap constraint, we will count the
number of performed intervals that precede each interval.

### Python code

```python
"""Code sample to demonstrates how to rank intervals."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


def RankTasks(model, starts, presences, ranks):
  """This method adds constraints and variables to links tasks and ranks.

  This method assumes that all starts are disjoint, that is all tasks have a
  strictly positive duration, and they appear in the same NoOverlap
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
      if presences[i] != 1:
        tmp_array.append(presences[i].Not())
        # Makes sure that if i is not performed, all precedences are false.
        model.AddImplication(presences[i].Not(), precedences[(i, j)].Not())
        model.AddImplication(presences[i].Not(), precedences[(j, i)].Not())
      if presences[j] != 1:
        tmp_array.append(presences[j].Not())
        # Makes sure that if j is not performed, all precedences are false.
        model.AddImplication(presences[j].Not(), precedences[(i, j)].Not())
        model.AddImplication(presences[j].Not(), precedences[(j, i)].Not())
      # The following loops will enforce that for any two intervals:
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


def RankingSample():
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
    if t < num_tasks / 2:
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

  # AddsNoOverlap constraint.
  model.AddNoOverlap(intervals)

  # Add ranking constraint.
  RankTasks(model, starts, presences, ranks)

  # Adds a constraint on ranks.
  model.Add(ranks[0] < ranks[1])

  # Creates makespan variable
  makespan = model.NewIntVar(0, horizon, 'makespan')
  for t in all_tasks:
    if presences[t] == 1:
      model.Add(ends[t] <= makespan)
    else:
      model.Add(ends[t] <= makespan).OnlyEnforceIf(presences[t])

  # Minimizes makespan - fixed gain per tasks performed.
  # As the fixed cost is less that the duration of the last interval,
  # the solver will not perform the last interval.
  model.Minimize(makespan - 3 * sum(presences[t] for t in all_tasks))

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


RankingSample()
```

## Transitions in a disjunctive resource

## Precedences between intervals

## Convex hull of a set of intervals

## Reservoir constraint

## Advanced: Optional start and end variables in intervals
