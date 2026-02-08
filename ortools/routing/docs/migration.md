# Migration from SWIG to pybind11 for Routing

This document describes how to migrate your Python code using the Operations
Research Routing library from the legacy SWIG-based bindings (`pywraprouting`)
to the new pybind11-based bindings (`routing`).

The new bindings offer better integration with Python, including:

 * Compliance with PEP8 naming conventions (snake_case for methods and
   functions).
 * Better type hinting support.
 * More pythonic APIs.

## Imports

### Old (SWIG)

```python
from google3.util.operations_research.routing import enums_pb2
from google3.util.operations_research.routing.python import pywraprouting
```

### New (pybind11)

```python
from google3.util.operations_research.constraint_solver.python import constraint_solver
from google3.util.operations_research.routing.python import routing
```

*Note: The `constraint_solver` import is often needed for types like
`Assignment` which are shared with the underlying CP solver.*

## Naming Conventions

The most significant change is the shift from CamelCase to snake_case for method
names and functions. Class names remain CamelCase.

Feature               | Legacy (SWIG)                                       | New (pybind11)
:-------------------- | :-------------------------------------------------- | :-------------
**Index Manager**     | `pywraprouting.IndexManager(...)`                   | `routing.IndexManager(...)`
**Routing Model**     | `pywraprouting.Model(manager)`                      | `routing.Model(manager)`
**Search Parameters** | `pywraprouting.DefaultRoutingSearchParameters()`    | `routing.default_routing_search_parameters()`
**Enums**             | `enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC` | `routing.FirstSolutionStrategy.PATH_CHEAPEST_ARC`

### Common Method Mappings

Legacy Method                           | New Method
:-------------------------------------- | :---------
`IndexToNode(index)`                    | `index_to_node(index)`
`NodeToIndex(node)`                     | `node_to_index(node)`
`RegisterTransitCallback(callback)`     | `register_transit_callback(callback)`
`SetArcCostEvaluatorOfAllVehicles(idx)` | `set_arc_cost_evaluator_of_all_vehicles(idx)`
`SolveWithParameters(params)`           | `solve_with_parameters(params)`
`Start(vehicle)`                        | `start(vehicle)`
`IsEnd(index)`                          | `is_end(index)`
`NextVar(index)`                        | `next_var(index)`
`GetDimensionOrDie(name)`               | `get_dimension_or_die(name)`
`AddDimension(...)`                     | `add_dimension(...)`
`AddVariableMinimizedByFinalizer(var)`  | `add_variable_minimized_by_finalizer(var)`
`GetNumberOfVehicles()`                 | `num_vehicles()`

### Assignment / Solution

Legacy Method               | New Method
:-------------------------- | :---------------------------
`solution.ObjectiveValue()` | `solution.objective_value()`
`solution.Value(var)`       | `solution.value(var)`
`solution.Min(var)`         | `solution.min(var)`
`solution.Max(var)`         | `solution.max(var)`

## Solver, Variables, and Constraints

The `constraint_solver` module (often imported as `from
google3.util.operations_research.constraint_solver.python import
constraint_solver`) also sees significant API changes, particularly in how
variables are created and constraints are added.

### Variable Creation

In the legacy API, variables were created using methods like `IntVar`,
`BoolVar`, etc. In the new API, these are prefixed with `new_` and snake_cased.

Legacy Method                          | New Method
:------------------------------------- | :---------
`solver.IntVar(...)`                   | `solver.new_int_var(...)`
`solver.BoolVar(...)`                  | `solver.new_bool_var(...)`
`solver.IntervalVar(...)`              | `solver.new_interval_var(...)`
`solver.FixedDurationIntervalVar(...)` | `solver.new_fixed_duration_interval_var(...)`

### Adding Constraints

The legacy API typically involved a two-step process: creating the constraint
object (often using methods named after the constraint, e.g.,
`solver.AllDifferent(...)`) and then adding it to the solver using
`solver.Add(...)`.

The new API provides dedicated `add_` methods that create and add the constraint
in a single step.

Legacy Pattern                            | New Method
:---------------------------------------- | :---------------------------------
`solver.Add(solver.AllDifferent(...))`    | `solver.add_all_different(...)`
`solver.Add(solver.Cumulative(...))`      | `solver.add_cumulative(...)`
`solver.Add(solver.SumEquality(...))`     | `solver.add_sum_equality(...)`
`solver.Add(solver.MinEquality(...))`     | `solver.add_min_equality(...)`
`solver.Add(solver.MaxEquality(...))`     | `solver.add_max_equality(...)`
`solver.Add(solver.ElementEquality(...))` | `solver.add_element_equality(...)`
`solver.Add(solver.AbsEquality(...))`     | `solver.add_abs_equality(...)`
`solver.Add(solver.BetweenCt(...))`       | `solver.add_between_ct(...)`
`solver.Add(solver.MemberCt(...))`        | `solver.add_member_ct(...)`
`solver.Add(solver.NotMemberCt(...))`     | `solver.add_not_member_ct(...)`
`solver.Add(solver.Count(...))`           | `solver.add_count(...)`
`solver.Add(solver.Distribute(...))`      | `solver.add_distribute(...)`
`solver.Add(solver.Deviation(...))`       | `solver.add_deviation(...)`
`solver.Add(solver.Circuit(...))`         | `solver.add_circuit(...)`
`solver.Add(solver.SubCircuit(...))`      | `solver.add_sub_circuit(...)`
`solver.Add(solver.Pack(...))`            | `solver.add_pack(...)`

**Note:** For general constraints not covered by specific `add_` methods, you
can still use `solver.add(...)` (snake_case version of `solver.Add(...)`).

## Enums and Status

In the legacy API, enums were typically accessed via `enums_pb2` and treated as
integers or protobuf enum descriptors. In the new API, they are exposed as
nested classes within the `routing` module, behaving like standard Python Enums.

### Legacy

```python
from google3.util.operations_research.routing import enums_pb2

status = routing.Status()
print(f"Status: {enums_pb2.RoutingSearchStatus.Value.Name(status)}")
if status == enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL:
    pass
```

### New

```python
from google3.util.operations_research.routing.python import routing

status = routing_model.status()
# You can access the name directly on the enum member
print(f"Status: {status.name}")
if status == routing.RoutingSearchStatus.ROUTING_OPTIMAL:
    pass
```

## Example: Simple Routing Program

### Legacy Code

```python
from google3.util.operations_research.routing import enums_pb2
from google3.util.operations_research.routing.python import pywraprouting

def main():
  manager = pywraprouting.IndexManager(num_locations, num_vehicles, depot)
  routing = pywraprouting.Model(manager)

  def distance_callback(from_index, to_index):
    from_node = manager.IndexToNode(from_index)
    to_node = manager.IndexToNode(to_index)
    return abs(to_node - from_node)

  transit_callback_index = routing.RegisterTransitCallback(distance_callback)
  routing.SetArcCostEvaluatorOfAllVehicles(transit_callback_index)

  search_parameters = pywraprouting.DefaultRoutingSearchParameters()
  search_parameters.first_solution_strategy = (
      enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC
  )

  assignment = routing.SolveWithParameters(search_parameters)
  print(f"Objective: {assignment.ObjectiveValue()}")
```

### New Code

```python
from google3.util.operations_research.routing.python import routing

def main():
  manager = routing.IndexManager(num_locations, num_vehicles, depot)
  routing_model = routing.Model(manager)

  def distance_callback(from_index, to_index):
    from_node = manager.index_to_node(from_index)
    to_node = manager.index_to_node(to_index)
    return abs(to_node - from_node)

  transit_callback_index = routing_model.register_transit_callback(distance_callback)
  routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_callback_index)

  search_parameters = routing.default_routing_search_parameters()
  search_parameters.first_solution_strategy = (
      routing.FirstSolutionStrategy.PATH_CHEAPEST_ARC
  )

  assignment = routing_model.solve_with_parameters(search_parameters)
  print(f"Objective: {assignment.objective_value()}")
```

## Dimensions and Variables

When working with dimensions, the variable accessors also change to snake_case.

### Legacy

```python
time_dimension = routing.GetDimensionOrDie("Time")
index = routing.Start(0)
time_var = time_dimension.CumulVar(index)
time_var.SetRange(0, 10)
```

### New

```python
time_dimension = routing_model.get_dimension_or_die("Time")
index = routing_model.start(0)
time_var = time_dimension.cumul_var(index)
time_var.set_range(0, 10)
```
