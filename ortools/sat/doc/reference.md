<h1 id="ortools.sat.python.cp_model">ortools.sat.python.cp_model</h1>

Propose a natural language on top of cp_model_pb2 python proto.

This file implements a easy-to-use API on top of the cp_model_pb2 protobuf
defined in ../ .

<h2 id="ortools.sat.python.cp_model.AssertIsInt64">AssertIsInt64</h2>

```python
AssertIsInt64(x)
```
Asserts that x is integer and x is in [min_int_64, max_int_64].
<h2 id="ortools.sat.python.cp_model.AssertIsInt32">AssertIsInt32</h2>

```python
AssertIsInt32(x)
```
Asserts that x is integer and x is in [min_int_32, max_int_32].
<h2 id="ortools.sat.python.cp_model.AssertIsBoolean">AssertIsBoolean</h2>

```python
AssertIsBoolean(x)
```
Asserts that x is 0 or 1.
<h2 id="ortools.sat.python.cp_model.CapInt64">CapInt64</h2>

```python
CapInt64(v)
```
Restrict v within [INT_MIN..INT_MAX] range.
<h2 id="ortools.sat.python.cp_model.CapSub">CapSub</h2>

```python
CapSub(x, y)
```
Saturated arithmetics. Returns x - y truncated to the int64 range.
<h2 id="ortools.sat.python.cp_model.DisplayBounds">DisplayBounds</h2>

```python
DisplayBounds(bounds)
```
Displays a flattened list of intervals.
<h2 id="ortools.sat.python.cp_model.ShortName">ShortName</h2>

```python
ShortName(model, i)
```
Returns a short name of an integer variable, or its negation.
<h2 id="ortools.sat.python.cp_model.IntegerExpression">IntegerExpression</h2>

```python
IntegerExpression(self)
```
Holds an integer expression.

An integer expressiom regroups linear expressions build from integer
constants and integer variables.

x + 2 * (y - z + 1) is such an integer expression, and can be written that way
directly in python, provided x, y, and z are integer variables.

Integer expressions are used in two places in the cp_model.
Associated with equality, inequality operators, the create bounded expressions
that can be added to the model as in:

    model.Add(x + 2 * y <= 5)
    model.Add(sum(array_of_vars) == 5)

IntegerExpressions can also be used to specify the objective of the model.

    model.Minimize(x + 2 * y + z)

<h3 id="ortools.sat.python.cp_model.IntegerExpression.GetVarValueMap">GetVarValueMap</h3>

```python
IntegerExpression.GetVarValueMap(self)
```
Scan the expression, and return a list of (var_coef_map, constant).
<h2 id="ortools.sat.python.cp_model.IntVar">IntVar</h2>

```python
IntVar(self, model, bounds, name)
```
Represents a IntegerExpression containing only a single variable.
<h2 id="ortools.sat.python.cp_model.BoundIntegerExpression">BoundIntegerExpression</h2>

```python
BoundIntegerExpression(self, expr, bounds)
```
Represents a constraint: IntegerExpression in domain.
<h2 id="ortools.sat.python.cp_model.Constraint">Constraint</h2>

```python
Constraint(self, constraints)
```
Base class for constraints.
<h3 id="ortools.sat.python.cp_model.Constraint.OnlyEnforceIf">OnlyEnforceIf</h3>

```python
Constraint.OnlyEnforceIf(self, boolvar)
```
Adds an enforcement literal to the constraint.

An enforcement literal (boolean variable or its negation) decides if the
constraint is active or not. It acts as an implication, thus literal is
true implies that the constraint must be enforce.
<h2 id="ortools.sat.python.cp_model.IntervalVar">IntervalVar</h2>

```python
IntervalVar(self, model, start_index, size_index, end_index, is_present_index, name)
```
Represents a Interval variable.
<h2 id="ortools.sat.python.cp_model.CpModel">CpModel</h2>

```python
CpModel(self)
```
Wrapper class around the cp_model proto.
<h3 id="ortools.sat.python.cp_model.CpModel.NewIntVar">NewIntVar</h3>

```python
CpModel.NewIntVar(self, lb, ub, name)
```
Creates an integer variable with domain [lb, ub].
<h3 id="ortools.sat.python.cp_model.CpModel.NewEnumeratedIntVar">NewEnumeratedIntVar</h3>

```python
CpModel.NewEnumeratedIntVar(self, bounds, name)
```
Creates an integer variable with an enumerated domain.

Args:
    bounds: A flattened list of disjoint intervals.
    name: The name of the variable.

Returns:
    a variable whose domain is union[bounds[2*i]..bounds[2*i + 1]].

To create a variable with domain [1, 2, 3, 5, 7, 8], pass in the
array [1, 3, 5, 5, 7, 8].

<h3 id="ortools.sat.python.cp_model.CpModel.NewBoolVar">NewBoolVar</h3>

```python
CpModel.NewBoolVar(self, name)
```
Creates a 0-1 variable with the given name.
<h3 id="ortools.sat.python.cp_model.CpModel.AddLinearConstraint">AddLinearConstraint</h3>

```python
CpModel.AddLinearConstraint(self, terms, lb, ub)
```
Adds the constraints lb <= sum(terms) <= ub, where term = (var, coef).
<h3 id="ortools.sat.python.cp_model.CpModel.AddSumConstraint">AddSumConstraint</h3>

```python
CpModel.AddSumConstraint(self, variables, lb, ub)
```
Adds the constraints lb <= sum(variables) <= ub.
<h3 id="ortools.sat.python.cp_model.CpModel.AddLinearConstraintWithBounds">AddLinearConstraintWithBounds</h3>

```python
CpModel.AddLinearConstraintWithBounds(self, terms, bounds)
```
Adds the constraints sum(terms) in bounds, where term = (var, coef).
<h3 id="ortools.sat.python.cp_model.CpModel.Add">Add</h3>

```python
CpModel.Add(self, ct)
```
Adds a BoundIntegerExpression to the model.
<h3 id="ortools.sat.python.cp_model.CpModel.AddAllDifferent">AddAllDifferent</h3>

```python
CpModel.AddAllDifferent(self, variables)
```
Adds AllDifferent(variables).

This constraint forces all variables to have different values.

Args:
  variables: a list of integer variables.

Returns:
  An instance of the Constraint class.

<h3 id="ortools.sat.python.cp_model.CpModel.AddElement">AddElement</h3>

```python
CpModel.AddElement(self, index, variables, target)
```
Adds the element constraint: variables[index] == target.
<h3 id="ortools.sat.python.cp_model.CpModel.AddCircuit">AddCircuit</h3>

```python
CpModel.AddCircuit(self, arcs)
```
Adds Circuit(arcs).

Adds a circuit constraint from a sparse list of arcs that encode the graph.

A circuit is a unique Hamiltonian path in a subgraph of the total
graph. In case a node 'i' is not in the path, then there must be a
loop arc 'i -> i' associated with a true literal. Otherwise
this constraint will fail.

Args:
  arcs: a list of arcs. An arc is a tuple
        (source_node, destination_node, literal).
        The arc is selected in the circuit if the literal is true.
        Both source_node and destination_node must be integer value between
        0 and the number of nodes - 1.

Returns:
  An instance of the Constraint class.

Raises:
  ValueError: If the list of arc is empty.

<h3 id="ortools.sat.python.cp_model.CpModel.AddAllowedAssignments">AddAllowedAssignments</h3>

```python
CpModel.AddAllowedAssignments(self, variables, tuples_list)
```
Adds AllowedAssignments(variables, tuples_list).

An AllowedAssignments constraint is a constraint on an array of variables
that forces, when all variables are fixed to a single value, that the
corresponding list of values is equal to one of the tuple of the
tuple_list.

Args:
  variables: A list of variables.

  tuples_list: A list of admissible tuples. Each tuple must have the same
               length as the variables, and the ith value of a tuple
               corresponds to the ith variable.

Returns:
  An instance of the Constraint class.

Raises:
  TypeError: If a tuple does not have the same size as the list of
      variables.
  ValueError: If the array of variables is empty.

<h3 id="ortools.sat.python.cp_model.CpModel.AddForbiddenAssignments">AddForbiddenAssignments</h3>

```python
CpModel.AddForbiddenAssignments(self, variables, tuples_list)
```
Adds AddForbiddenAssignments(variables, [tuples_list]).

A ForbiddenAssignments constraint is a constraint on an array of variables
where the list of impossible combinations is provided in the tuples list.

Args:
  variables: A list of variables.

  tuples_list: A list of forbidden tuples. Each tuple must have the same
               length as the variables, and the ith value of a tuple
               corresponds to the ith variable.

Returns:
  An instance of the Constraint class.

Raises:
  TypeError: If a tuple does not have the same size as the list of
             variables.
  ValueError: If the array of variables is empty.

<h3 id="ortools.sat.python.cp_model.CpModel.AddAutomaton">AddAutomaton</h3>

```python
CpModel.AddAutomaton(self, transition_variables, starting_state, final_states, transition_triples)
```
Adds an automaton constraint.

An automaton constraint takes a list of variables (of size n), an initial
state, a set of final states, and a set of transitions. A transition is a
triplet ('tail', 'transition', 'head'), where 'tail' and 'head' are states,
and 'transition' is the label of an arc from 'head' to 'tail',
corresponding to the value of one variable in the list of variables.

This automata will be unrolled into a flow with n + 1 phases. Each phase
contains the possible states of the automaton. The first state contains the
initial state. The last phase contains the final states.

Between two consecutive phases i and i + 1, the automaton creates a set of
arcs. For each transition (tail, transition, head), it will add an arc from
the state 'tail' of phase i and the state 'head' of phase i + 1. This arc
labeled by the value 'transition' of the variables 'variables[i]'. That is,
this arc can only be selected if 'variables[i]' is assigned the value
'transition'.

A feasible solution of this constraint is an assignment of variables such
that, starting from the initial state in phase 0, there is a path labeled by
the values of the variables that ends in one of the final states in the
final phase.

Args:
  transition_variables: A non empty list of variables whose values
    correspond to the labels of the arcs traversed by the automata.
  starting_state: The initial state of the automata.
  final_states: A non empty list of admissible final states.
  transition_triples: A list of transition for the automata, in the
    following format (current_state, variable_value, next_state).


Returns:
  An instance of the Constraint class.

Raises:
  ValueError: if transition_variables, final_states, or transition_triples
  are empty.

<h3 id="ortools.sat.python.cp_model.CpModel.AddInverse">AddInverse</h3>

```python
CpModel.AddInverse(self, variables, inverse_variables)
```
Adds Inverse(variables, inverse_variables).

An inverse constraint enforces that if 'variables[i]' is assigned a value
'j', then inverse_variables[j] is assigned a value 'i'. And vice versa.

Args:
  variables: An array of integer variables.
  inverse_variables: An array of integer variables.

Returns:
  An instance of the Constraint class.

Raises:
  TypeError: if variables and inverse_variables have different length, or
      if they are empty.

<h3 id="ortools.sat.python.cp_model.CpModel.AddReservoirConstraint">AddReservoirConstraint</h3>

```python
CpModel.AddReservoirConstraint(self, times, demands, min_level, max_level)
```
Adds Reservoir(times, demands, min_level, max_level).

Maintains a reservoir level within bounds. The water level starts at 0, and
at any time >= 0, it must be between min_level and max_level. Furthermore,
this constraints expect all times variables to be >= 0.
If the variable times[i] is assigned a value t, then the current level
changes by demands[i] (which is constant) at the time t.

Note that level min can be > 0, or level max can be < 0. It just forces
some demands to be executed at time 0 to make sure that we are within those
bounds with the executed demands. Therefore, at any time t >= 0:
    sum(demands[i] if times[i] <= t) in [min_level, max_level]

Args:
  times: A list of positive integer variables which specify the time of the
    filling or emptying the reservoir.
  demands: A list of integer values that specifies the amount of the
    emptying or feeling.
  min_level: At any time >= 0, the level of the reservoir must be greater of
    equal than the min level.
  max_level: At any time >= 0, the level of the reservoir must be less or
    equal than the max level.

Returns:
  An instance of the Constraint class.

Raises:
  ValueError: if max_level < min_level.

<h3 id="ortools.sat.python.cp_model.CpModel.AddReservoirConstraintWithActive">AddReservoirConstraintWithActive</h3>

```python
CpModel.AddReservoirConstraintWithActive(self, times, demands, actives, min_level, max_level)
```
Adds Reservoir(times, demands, actives, min_level, max_level).

Maintain a reservoir level within bounds. The water level starts at 0, and
at
any time >= 0, it must be within min_level, and max_level. Furthermore, this
constraints expect all times variables to be >= 0.
If the actives[i] is true, and if times[i] is assigned a value t, then the
level of the reservoir changes by demands[i] (which is constant) at time t.

Note that level_min can be > 0, or level_max can be < 0. It just forces
some demands to be executed at time 0 to make sure that we are within those
bounds with the executed demands. Therefore, at any time t >= 0:
    sum(demands[i] * actives[i] if times[i] <= t) in [min_level, max_level]
The array of boolean variables 'actives', if defined, indicates which
actions are actually performed.

Args:
  times: A list of positive integer variables which specify the time of the
    filling or emptying the reservoir.
  demands: A list of integer values that specifies the amount of the
    emptying or feeling.
  actives: a list of boolean variables. They indicates if the
    emptying/refilling events actually take place.
  min_level: At any time >= 0, the level of the reservoir must be greater of
    equal than the min level.
  max_level: At any time >= 0, the level of the reservoir must be less or
    equal than the max level.

Returns:
  An instance of the Constraint class.

Raises:
  ValueError: if max_level < min_level.

<h3 id="ortools.sat.python.cp_model.CpModel.AddMapDomain">AddMapDomain</h3>

```python
CpModel.AddMapDomain(self, var, bool_var_array, offset=0)
```
Adds var == i + offset <=> bool_var_array[i] == true for all i.
<h3 id="ortools.sat.python.cp_model.CpModel.AddImplication">AddImplication</h3>

```python
CpModel.AddImplication(self, a, b)
```
Adds a => b.
<h3 id="ortools.sat.python.cp_model.CpModel.AddBoolOr">AddBoolOr</h3>

```python
CpModel.AddBoolOr(self, literals)
```
Adds Or(literals) == true.
<h3 id="ortools.sat.python.cp_model.CpModel.AddBoolAnd">AddBoolAnd</h3>

```python
CpModel.AddBoolAnd(self, literals)
```
Adds And(literals) == true.
<h3 id="ortools.sat.python.cp_model.CpModel.AddBoolXOr">AddBoolXOr</h3>

```python
CpModel.AddBoolXOr(self, literals)
```
Adds XOr(literals) == true.
<h3 id="ortools.sat.python.cp_model.CpModel.AddMinEquality">AddMinEquality</h3>

```python
CpModel.AddMinEquality(self, target, variables)
```
Adds target == Min(variables).
<h3 id="ortools.sat.python.cp_model.CpModel.AddMaxEquality">AddMaxEquality</h3>

```python
CpModel.AddMaxEquality(self, target, args)
```
Adds target == Max(variables).
<h3 id="ortools.sat.python.cp_model.CpModel.AddDivisionEquality">AddDivisionEquality</h3>

```python
CpModel.AddDivisionEquality(self, target, num, denom)
```
Adds target == num // denom.
<h3 id="ortools.sat.python.cp_model.CpModel.AddModuloEquality">AddModuloEquality</h3>

```python
CpModel.AddModuloEquality(self, target, var, mod)
```
Adds target = var % mod.
<h3 id="ortools.sat.python.cp_model.CpModel.AddProdEquality">AddProdEquality</h3>

```python
CpModel.AddProdEquality(self, target, args)
```
Adds target == PROD(args).
<h3 id="ortools.sat.python.cp_model.CpModel.NewIntervalVar">NewIntervalVar</h3>

```python
CpModel.NewIntervalVar(self, start, size, end, name)
```
Creates an interval variable from start, size, and end.

An interval variable is a constraint, that is itself used in other
constraints like NoOverlap.

Internally, it ensures that start + size == end.

Args:
  start: The start of the interval. It can be an integer value, or an
         integer variable.
  size: The size of the interval. It can be an integer value, or an
        integer variable.
  end: The end of the interval. It can be an integer value, or an
       integer variable.
  name: The name of the interval variable.

Returns:
  An IntervalVar object.

<h3 id="ortools.sat.python.cp_model.CpModel.NewOptionalIntervalVar">NewOptionalIntervalVar</h3>

```python
CpModel.NewOptionalIntervalVar(self, start, size, end, is_present, name)
```
Creates an optional interval var from start, size, end. and is_present.

An optional interval variable is a constraint, that is itself used in other
constraints like NoOverlap. This constraint is protected by an is_present
literal that indicates if it is active or not.

Internally, it ensures that is_present implies start + size == end.

Args:
  start: The start of the interval. It can be an integer value, or an
         integer variable.
  size: The size of the interval. It can be an integer value, or an
        integer variable.
  end: The end of the interval. It can be an integer value, or an
       integer variable.
  is_present: A literal that indicates if the interval is active or not.
              A inactive interval is simply ignored by all constraints.
  name: The name of the interval variable.

Returns:
  An IntervalVar object.

<h3 id="ortools.sat.python.cp_model.CpModel.AddNoOverlap">AddNoOverlap</h3>

```python
CpModel.AddNoOverlap(self, interval_vars)
```
Adds NoOverlap(interval_vars).

A NoOverlap constraint ensures that all present intervals do not overlap
in time.

Args:
  interval_vars: The list of interval variables to constrain.

Returns:
  An instance of the Constraint class.

<h3 id="ortools.sat.python.cp_model.CpModel.AddNoOverlap2D">AddNoOverlap2D</h3>

```python
CpModel.AddNoOverlap2D(self, x_intervals, y_intervals)
```
Adds NoOverlap2D(x_intervals, y_intervals).

A NoOverlap2D constraint ensures that all present rectangles do not overlap
on a plan. Each rectangle is aligned with the X and Y axis, and is defined
by two intervals which represent its projection onto the X and Y axis.

Args:
  x_intervals: The X coordinates of the rectangles.
  y_intervals: The Y coordinates of the rectangles.

Returns:
  An instance of the Constraint class.

<h3 id="ortools.sat.python.cp_model.CpModel.AddCumulative">AddCumulative</h3>

```python
CpModel.AddCumulative(self, intervals, demands, capacity)
```
Adds Cumulative(intervals, demands, capacity).

This constraint enforces that:
  for all t:
    sum(demands[i]
        if (start(intervals[t]) <= t < end(intervals[t])) and
        (t is present)) <= capacity

Args:
  intervals: The list of intervals.
  demands: The list of demands for each interval. Each demand must be >= 0.
      Each demand can be an integer value, or an integer variable.
  capacity: The maximum capacity of the cumulative constraint. It must be a
            positive integer value or variable.

Returns:
  An instance of the Constraint class.

<h3 id="ortools.sat.python.cp_model.CpModel.GetOrMakeIndex">GetOrMakeIndex</h3>

```python
CpModel.GetOrMakeIndex(self, arg)
```
Returns the index of a variables, its negation, or a number.
<h3 id="ortools.sat.python.cp_model.CpModel.GetOrMakeBooleanIndex">GetOrMakeBooleanIndex</h3>

```python
CpModel.GetOrMakeBooleanIndex(self, arg)
```
Returns an index from a boolean expression.
<h3 id="ortools.sat.python.cp_model.CpModel.Minimize">Minimize</h3>

```python
CpModel.Minimize(self, obj)
```
Sets the objective of the model to minimize(obj).
<h3 id="ortools.sat.python.cp_model.CpModel.Maximize">Maximize</h3>

```python
CpModel.Maximize(self, obj)
```
Sets the objective of the model to maximize(obj).
<h3 id="ortools.sat.python.cp_model.CpModel.AddDecisionStrategy">AddDecisionStrategy</h3>

```python
CpModel.AddDecisionStrategy(self, variables, var_strategy, domain_strategy)
```
Adds a search strategy to the model.

Args:
  variables: a list of variables this strategy will assign.
  var_strategy: heuristic to choose the next variable to assign.
  domain_strategy: heuristic to reduce the domain of the selected variable.

Currently, this is advanced code, the union of all strategies added to the
model must be complete, i.e. instantiates all variables.
Otherwise, Solve() will fail.

<h2 id="ortools.sat.python.cp_model.EvaluateIntegerExpression">EvaluateIntegerExpression</h2>

```python
EvaluateIntegerExpression(expression, solution)
```
Evaluate an integer expression against a solution.
<h2 id="ortools.sat.python.cp_model.EvaluateBooleanExpression">EvaluateBooleanExpression</h2>

```python
EvaluateBooleanExpression(literal, solution)
```
Evaluate an boolean expression against a solution.
<h2 id="ortools.sat.python.cp_model.CpSolverSolutionCallback">CpSolverSolutionCallback</h2>

```python
CpSolverSolutionCallback(self)
```
Nicer solution callback that uses the CpSolver class.
<h3 id="ortools.sat.python.cp_model.CpSolverSolutionCallback.Value">Value</h3>

```python
CpSolverSolutionCallback.Value(self, expression)
```
Returns the value of an integer expression.
<h2 id="ortools.sat.python.cp_model.CpSolver">CpSolver</h2>

```python
CpSolver(self)
```
Main solver class.
<h3 id="ortools.sat.python.cp_model.CpSolver.Solve">Solve</h3>

```python
CpSolver.Solve(self, model)
```
Solves the given model and returns the solve status.
<h3 id="ortools.sat.python.cp_model.CpSolver.SolveWithSolutionCallback">SolveWithSolutionCallback</h3>

```python
CpSolver.SolveWithSolutionCallback(self, model, callback)
```
Solves a problem and pass each solution found to the callback.
<h3 id="ortools.sat.python.cp_model.CpSolver.SearchForAllSolutions">SearchForAllSolutions</h3>

```python
CpSolver.SearchForAllSolutions(self, model, callback)
```
Search for all solutions of a satisfiability problem.

This method searches for all feasible solution of a given model.
Then it feeds the solution to the callback.

Args:
  model: The model to solve.
  callback: The callback that will be called at each solution.

Returns:
  The status of the solve (FEASIBLE, INFEASIBLE...).

<h3 id="ortools.sat.python.cp_model.CpSolver.Value">Value</h3>

```python
CpSolver.Value(self, expression)
```
Returns the value of an integer expression after solve.
<h3 id="ortools.sat.python.cp_model.CpSolver.BooleanValue">BooleanValue</h3>

```python
CpSolver.BooleanValue(self, literal)
```
Returns the boolean value of an integer expression after solve.
<h3 id="ortools.sat.python.cp_model.CpSolver.ObjectiveValue">ObjectiveValue</h3>

```python
CpSolver.ObjectiveValue(self)
```
Returns the value of objective after solve.
<h3 id="ortools.sat.python.cp_model.CpSolver.StatusName">StatusName</h3>

```python
CpSolver.StatusName(self, status)
```
Returns the name of the status returned by Solve().
