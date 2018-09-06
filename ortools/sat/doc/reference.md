<h1 id="ortools.sat.python.cp_model">ortools.sat.python.cp_model</h1>

Propose a natural language on top of cp_model_pb2 python proto.

This file implements a easy-to-use API on top of the cp_model_pb2 protobuf
defined in ../ .

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
<h2 id="ortools.sat.python.cp_model.LinearExpression">LinearExpression</h2>

```python
LinearExpression(self, /, *args, **kwargs)
```
Holds an integer linear expression.

An linear expression is built from integer constants and variables.

x + 2 * (y - z + 1) is one such linear expression, and can be written that
way directly in Python, provided x, y, and z are integer variables.

Linear expressions are used in two places in the cp_model.
When used with equality and inequality operators, they create linear
inequalities that can be added to the model as in:

    model.Add(x + 2 * y <= 5)
    model.Add(sum(array_of_vars) == 5)

Linear expressions can also be used to specify the objective of the model.

    model.Minimize(x + 2 * y + z)

<h3 id="ortools.sat.python.cp_model.LinearExpression.GetVarValueMap">GetVarValueMap</h3>

```python
LinearExpression.GetVarValueMap(self)
```
Scan the expression, and return a list of (var_coef_map, constant).
<h2 id="ortools.sat.python.cp_model.IntVar">IntVar</h2>

```python
IntVar(self, model, bounds, name)
```
An integer variable.

An IntVar is an object that can take on any integer value within defined
ranges. Variables appears in constraint like:

    x + y >= 5
    AllDifferent([x, y, z])

Solving a model is equivalent to finding, for each variable, a single value
from the set of initial values (called the initial domain), such that the
model is feasible, or optimal if you provided an objective function.

<h3 id="ortools.sat.python.cp_model.IntVar.Not">Not</h3>

```python
IntVar.Not(self)
```
Returns the negation of a Boolean variable.

This method implements the logical negation of a Boolean variable.
It is only valid of the variable has a Boolean domain (0 or 1).

Note that this method is nilpotent: x.Not().Not() == x.

<h2 id="ortools.sat.python.cp_model.LinearInequality">LinearInequality</h2>

```python
LinearInequality(self, expr, bounds)
```
Represents a linear constraint: lb <= expression <= ub.

The only use of this class is to be added to the CpModel through
CpModel.Add(expression), as in:

    model.Add(x + 2 * y -1 >= z)

<h2 id="ortools.sat.python.cp_model.Constraint">Constraint</h2>

```python
Constraint(self, constraints)
```
Base class for constraints.

Constraints are built by the CpModel through the Add<XXX> methods.
Once created by the CpModel class, they are automatically added to the model.
The purpose of this class is to allow specification of enforcement literals
for this constraint.

    b = model.BoolVar('b')
    x = model.IntVar(0, 10, 'x')
    y = model.IntVar(0, 10, 'y')

    model.Add(x + 2 * y == 5).OnlyEnforceIf(b.Not())

<h3 id="ortools.sat.python.cp_model.Constraint.OnlyEnforceIf">OnlyEnforceIf</h3>

```python
Constraint.OnlyEnforceIf(self, boolvar)
```
Adds an enforcement literal to the constraint.

Args:
    boolvar: A boolean literal, that is a boolean variable or its negation.
      An enforcement literal (boolean variable or its negation) decides
      whether the constraint is active or not. It acts as an implication, so
      if the literal is true, that implies the constraint must be enforced.

<h2 id="ortools.sat.python.cp_model.IntervalVar">IntervalVar</h2>

```python
IntervalVar(self, model, start_index, size_index, end_index, is_present_index, name)
```
Represents a Interval variable.


An interval variable is both a constraint and a variable. It is defined by
three integer variables: start, size, and end.

It is a constraint because, internally, it enforces that start + size == end.

It is also a variable as it can appear in specific scheduling constraints:
NoOverlap, NoOverlap2D, Cumulative.

Optionally, an enforcement literal can be added to this
constraint. This enforcement literal is understood by the same constraints.
These constraints ignore interval variables with enforcement literals assigned
to false. Conversely, these constraints will also set these enforcement
literals to false if they cannot fit these intervals into the schedule.

<h2 id="ortools.sat.python.cp_model.CpModel">CpModel</h2>

```python
CpModel(self)
```
Wrapper class around the cp_model proto.

This class provides two types of methods:
  - NewXXX to create integer, boolean, or interval variables.
  - AddXXX to create new constraints and add them to the model.

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
Adds a LinearInequality to the model.
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
  arcs: a list of arcs. An arc is a tuple (source_node, destination_node,
    literal). The arc is selected in the circuit if the literal is true.
    Both source_node and destination_node must be integer value between 0
    and the number of nodes - 1.

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
    length as the variables, and the ith value of a tuple corresponds to the
    ith variable.

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
    length as the variables, and the ith value of a tuple corresponds to the
    ith variable.

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
  size: The size of the interval. It can be an integer value, or an integer
    variable.
  end: The end of the interval. It can be an integer value, or an integer
    variable.
  name: The name of the interval variable.

Returns:
  An IntervalVar object.

<h3 id="ortools.sat.python.cp_model.CpModel.NewOptionalIntervalVar">NewOptionalIntervalVar</h3>

```python
CpModel.NewOptionalIntervalVar(self, start, size, end, is_present, name)
```
Creates an optional interval var from start, size, end and is_present.

An optional interval variable is a constraint, that is itself used in other
constraints like NoOverlap. This constraint is protected by an is_present
literal that indicates if it is active or not.

Internally, it ensures that is_present implies start + size == end.

Args:
  start: The start of the interval. It can be an integer value, or an
    integer variable.
  size: The size of the interval. It can be an integer value, or an integer
    variable.
  end: The end of the interval. It can be an integer value, or an integer
    variable.
  is_present: A literal that indicates if the interval is active or not. A
    inactive interval is simply ignored by all constraints.
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
    Currently, this is advanced code, the union of all strategies added to
    the model must be complete, i.e. instantiates all variables. Otherwise,
    Solve() will fail.

<h2 id="ortools.sat.python.cp_model.EvaluateLinearExpression">EvaluateLinearExpression</h2>

```python
EvaluateLinearExpression(expression, solution)
```
Evaluate an linear expression against a solution.
<h2 id="ortools.sat.python.cp_model.EvaluateBooleanExpression">EvaluateBooleanExpression</h2>

```python
EvaluateBooleanExpression(literal, solution)
```
Evaluate an boolean expression against a solution.
<h2 id="ortools.sat.python.cp_model.CpSolverSolutionCallback">CpSolverSolutionCallback</h2>

```python
CpSolverSolutionCallback(self)
```
Solution callback.

This class implements a callback that will be called at each new solution
found during search.

The method OnSolutionCallback() will be called by the solver, and must be
implemented. The current solution can be queried using the BooleanValue()
and Value() methods.

<h3 id="ortools.sat.python.cp_model.CpSolverSolutionCallback.BooleanValue">BooleanValue</h3>

```python
CpSolverSolutionCallback.BooleanValue(self, lit)
```
Returns the boolean value of a boolean literal.

Args:
    lit: A boolean variable or its negation.

Returns:
    The boolean value of the literal in the solution.

Raises:
    RuntimeError: if 'lit' is not a boolean variable or its negation.

<h3 id="ortools.sat.python.cp_model.CpSolverSolutionCallback.Value">Value</h3>

```python
CpSolverSolutionCallback.Value(self, expression)
```
Evaluates an linear expression in the current solution.

Args:
    expression: a linear expression of the model.

Returns:
    An integer value equal to the evaluation of the linear expression
    against the current solution.

Raises:
    RuntimeError: if 'expression' is not a LinearExpression.

<h2 id="ortools.sat.python.cp_model.CpSolver">CpSolver</h2>

```python
CpSolver(self)
```
Main solver class.

The purpose of this class is to search for a solution of a model given to the
Solve() method.

Once Solve() is called, this class allows inspecting the solution found
with the Value() and BooleanValue() methods, as well as general statistics
about the solve procedure.

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
Returns the value of an linear expression after solve.
<h3 id="ortools.sat.python.cp_model.CpSolver.BooleanValue">BooleanValue</h3>

```python
CpSolver.BooleanValue(self, literal)
```
Returns the boolean value of a literal after solve.
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
<h3 id="ortools.sat.python.cp_model.CpSolver.NumBooleans">NumBooleans</h3>

```python
CpSolver.NumBooleans(self)
```
Returns the number of boolean variables managed by the SAT solver.
<h3 id="ortools.sat.python.cp_model.CpSolver.NumConflicts">NumConflicts</h3>

```python
CpSolver.NumConflicts(self)
```
Returns the number of conflicts since the creation of the solver.
<h3 id="ortools.sat.python.cp_model.CpSolver.NumBranches">NumBranches</h3>

```python
CpSolver.NumBranches(self)
```
Returns the number of search branches explored by the solver.
<h3 id="ortools.sat.python.cp_model.CpSolver.WallTime">WallTime</h3>

```python
CpSolver.WallTime(self)
```
Return the wall time in seconds since the creation of the solver.
<h3 id="ortools.sat.python.cp_model.CpSolver.UserTime">UserTime</h3>

```python
CpSolver.UserTime(self)
```
Return the user time in seconds since the creation of the solver.
