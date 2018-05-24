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

We give two code snippets the demonstrate how to build these objects in python
and C++.

### Python code

```python
from ortools.sat.python import cp_model

model = cp_model.CpModel()
start_var = model.NewIntVar(0, horizon, 'start')
duration = 10 # Python cp/sat code accept integer variables or constants.
end_var = model.NewIntVar(0, horizon, 'end')
interval_var = model.NewIntervalVar(start_var, duration, end_var, 'interval')
```

### C++ code

```cpp
#include "ortools/sat/cp_model.pb.h"

namespace operations_research {
namespace sat {

CpModelProto cp_model;

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
  interval_proto->set_start(start);
  interval_proto->set_size(duration);
  interval_proto->set_end(end);
  return index;
};

const int start_var = new_variable(0, horizon);
const int duration_var = new_constant(10);
const int end_var = new_variable(0, horizon);
const int interval_var = new_interval(start_var, duration_var, end_var);

}  // namespace sat
}  // namespace operations_research
```
