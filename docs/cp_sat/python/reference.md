| [home](README.md) | [boolean logic](boolean_logic.md) | [integer arithmetic](integer_arithmetic.md) | [channeling constraints](channeling.md) | [scheduling](scheduling.md) | [Using the CP-SAT solver](solver.md) | [Model manipulation](model.md) | [Reference manual](reference.md) |
| ----------------- | --------------------------------- | ------------------------------------------- | --------------------------------------- | --------------------------- | ------------------------------------ | ------------------------------ | -------------------------------- |

# ortools.sat.python.cp_model
Methods for building and solving CP-SAT models.
## DisplayBounds
```python
DisplayBounds(bounds)
```
Displays a flattened list of intervals.
## ShortName
```python
ShortName(model, i)
```
Returns a short name of an integer variable, or its negation.
## LinearExpr
```python
LinearExpr(self, /, *args, **kwargs)
```
Holds an integer linear expression.

A linear expression is built from integer constants and variables.
For example, x + 2 * (y - z + 1).

Linear expressions are used in CP-SAT models in two ways:

* To define constraints. For example

  model.Add(x + 2 * y <= 5)
  model.Add(sum(array_of_vars) == 5)

* To define the objective function. For example

  model.Minimize(x + 2 * y + z)

For large arrays, you can create constraints and the objective
from lists of linear expressions or coefficients as follows:

    model.Minimize(cp_model.LinearExpr.Sum(expressions))
    model.Add(cp_model.LinearExpr.ScalProd(expressions, coefficients) >= 0)

### Sum
```python
LinearExpr.Sum(expressions)
```
Create the expression sum(expressions).
### ScalProd
```python
LinearExpr.ScalProd(expressions, coefficients)
```
Create the expression sum(expressions[i] * coefficients[i]).
### GetVarValueMap
```python
LinearExpr.GetVarValueMap(self)
```
Scan the expression, and return a list of (var_coef_map, constant).
## IntVar
```python
IntVar(self, model, domain, name)
```
An integer variable.

An IntVar is an object that can take on any integer value within defined
ranges. Variables appear in constraint like:

    x + y >= 5
    AllDifferent([x, y, z])

Solving a model is equivalent to finding, for each variable, a single value
from the set of initial values (called the initial domain), such that the
model is feasible, or optimal if you provided an objective function.

### Index
```python
IntVar.Index(self)
```
Returns the index of the variable in the model.
### Proto
```python
IntVar.Proto(self)
```
Returns the variable protobuf.
### Not
```python
IntVar.Not(self)
```
Returns the negation of a Boolean variable.

This method implements the logical negation of a Boolean variable.
It is only valid if the variable has a Boolean domain (0 or 1).

Note that this method is nilpotent: `x.Not().Not() == x`.

## BoundedLinearExpression
```python
BoundedLinearExpression(self, expr, bounds)
```
Represents a linear constraint: `lb <= linear expression <= ub`.

The only use of this class is to be added to the CpModel through
`CpModel.Add(expression)`, as in:

    model.Add(x + 2 * y -1 >= z)

## Constraint
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

### OnlyEnforceIf
```python
Constraint.OnlyEnforceIf(self, boolvar)
```
Adds an enforcement literal to the constraint.

**Args:**

- *boolvar:* A boolean literal or a list of boolean literals.

**Returns:**
    self.

This method adds one or more literals (that is, a boolean variable or its
negation) as enforcement literals. The conjunction of all these literals
determines whether the constraint is active or not. It acts as an
implication, so if the conjunction is true, it implies that the constraint
must be enforced. If it is false, then the constraint is ignored.

BoolOr, BoolAnd, and linear constraints all support enforcement literals.

### Index
```python
Constraint.Index(self)
```
Returns the index of the constraint in the model.
### Proto
```python
Constraint.Proto(self)
```
Returns the constraint protobuf.
## IntervalVar
```python
IntervalVar(self, model, start_index, size_index, end_index, is_present_index, name)
```
Represents an Interval variable.

An interval variable is both a constraint and a variable. It is defined by
three integer variables: start, size, and end.

It is a constraint because, internally, it enforces that start + size == end.

It is also a variable as it can appear in specific scheduling constraints:
NoOverlap, NoOverlap2D, Cumulative.

Optionally, an enforcement literal can be added to this constraint, in which
case these scheduling constraints will ignore interval variables with
enforcement literals assigned to false. Conversely, these constraints will
also set these enforcement literals to false if they cannot fit these
intervals into the schedule.

### Index
```python
IntervalVar.Index(self)
```
Returns the index of the interval constraint in the model.
### Proto
```python
IntervalVar.Proto(self)
```
Returns the interval protobuf.
## CpModel
```python
CpModel(self)
```
Methods for building a CP model.

Methods beginning with:

* ```New``` create integer, boolean, or interval variables.
* ```Add``` create new constraints and add them to the model.

### NewIntVar
```python
CpModel.NewIntVar(self, lb, ub, name)
```
Create an integer variable with domain [lb, ub].
### NewIntVarFromDomain
```python
CpModel.NewIntVarFromDomain(self, domain, name)
```
Create an integer variable from a domain.

**Args:**

- *domain:* A instance of the Domain class.
- *name:* The name of the variable.

**Returns:**
    a variable whose domain is the given domain.

### NewBoolVar
```python
CpModel.NewBoolVar(self, name)
```
Creates a 0-1 variable with the given name.
### NewConstant
```python
CpModel.NewConstant(self, value)
```
Declares a constant integer.
### AddLinearConstraint
```python
CpModel.AddLinearConstraint(self, linear_expr, lb, ub)
```
Adds the constraint: `lb <= linear_expr <= ub`.
### AddLinearExpressionInDomain
```python
CpModel.AddLinearExpressionInDomain(self, linear_expr, domain)
```
Adds the constraint: `linear_expr in domain`.
### Add
```python
CpModel.Add(self, ct)
```
Adds a BoundedLinearExpression to the model.
### AddAllDifferent
```python
CpModel.AddAllDifferent(self, variables)
```
Adds AllDifferent(variables).

This constraint forces all variables to have different values.

**Args:**

- *variables:* a list of integer variables.

**Returns:**
  An instance of the `Constraint` class.

### AddElement
```python
CpModel.AddElement(self, index, variables, target)
```
Adds the element constraint: `variables[index] == target`.
### AddCircuit
```python
CpModel.AddCircuit(self, arcs)
```
Adds Circuit(arcs).

Adds a circuit constraint from a sparse list of arcs that encode the graph.

A circuit is a unique Hamiltonian path in a subgraph of the total
graph. In case a node 'i' is not in the path, then there must be a
loop arc 'i -> i' associated with a true literal. Otherwise
this constraint will fail.

**Args:**

- *arcs:* a list of arcs. An arc is a tuple (source_node, destination_node,
    literal). The arc is selected in the circuit if the literal is true.
    Both source_node and destination_node must be integers between 0 and the
    number of nodes - 1.

**Returns:**
  An instance of the `Constraint` class.

**Raises:**

- *ValueError:* If the list of arcs is empty.

### AddAllowedAssignments
```python
CpModel.AddAllowedAssignments(self, variables, tuples_list)
```
Adds AllowedAssignments(variables, tuples_list).

An AllowedAssignments constraint is a constraint on an array of variables
that forces, when all variables are fixed to a single value, the
corresponding list of values to be equal to one of the tuples of
`tuple_list`.

**Args:**

- *variables:* A list of variables.
- *tuples_list:* A list of admissible tuples. Each tuple must have the same
    length as the variables, and the ith value of a tuple corresponds to the
    ith variable.

**Returns:**
  An instance of the `Constraint` class.

**Raises:**

- *TypeError:* If a tuple does not have the same size as the list of
      variables.
- *ValueError:* If the array of variables is empty.

### AddForbiddenAssignments
```python
CpModel.AddForbiddenAssignments(self, variables, tuples_list)
```
Adds AddForbiddenAssignments(variables, [tuples_list]).

A ForbiddenAssignments constraint is a constraint on an array of variables
where the list of impossible combinations is provided in the tuples list.

**Args:**

- *variables:* A list of variables.
- *tuples_list:* A list of forbidden tuples. Each tuple must have the same
    length as the variables, and the ith value of a tuple corresponds to the
    ith variable.

**Returns:**
  An instance of the `Constraint` class.

**Raises:**

- *TypeError:* If a tuple does not have the same size as the list of
             variables.
- *ValueError:* If the array of variables is empty.

### AddAutomaton
```python
CpModel.AddAutomaton(self, transition_variables, starting_state, final_states, transition_triples)
```
Adds an automaton constraint.

An automaton constraint takes a list of variables (of size n), an initial
state, a set of final states, and a set of transitions. A transition is a
triplet (*tail*, *transition*, *head*), where *tail* and *head* are states,
and *transition* is the label of an arc from *head* to *tail*,
corresponding to the value of one variable in the list of variables.

This automaton will be unrolled into a flow with n + 1 phases. Each phase
contains the possible states of the automaton. The first state contains the
initial state. The last phase contains the final states.

Between two consecutive phases i and i + 1, the automaton creates a set of
arcs. For each transition (*tail*, *transition*, *head*), it will add an arc
from the state *tail* of phase i and the state *head* of phase i + 1. This
arc is labeled by the value *transition* of the variables `variables[i]`.
That is, this arc can only be selected if `variables[i]` is assigned the
value *transition*.

A feasible solution of this constraint is an assignment of variables such
that, starting from the initial state in phase 0, there is a path labeled by
the values of the variables that ends in one of the final states in the
final phase.

**Args:**

- *transition_variables:* A non-empty list of variables whose values
    correspond to the labels of the arcs traversed by the automaton.
- *starting_state:* The initial state of the automaton.
- *final_states:* A non-empty list of admissible final states.
- *transition_triples:* A list of transitions for the automaton, in the
    following format (current_state, variable_value, next_state).

**Returns:**
  An instance of the `Constraint` class.

**Raises:**

- *ValueError:* if transition_variables, final_states, or transition_triples
  are empty.

### AddInverse
```python
CpModel.AddInverse(self, variables, inverse_variables)
```
Adds Inverse(variables, inverse_variables).

An inverse constraint enforces that if `variables[i]` is assigned a value
`j`, then `inverse_variables[j]` is assigned a value `i`. And vice versa.

**Args:**

- *variables:* An array of integer variables.
- *inverse_variables:* An array of integer variables.

**Returns:**
  An instance of the `Constraint` class.

**Raises:**

- *TypeError:* if variables and inverse_variables have different lengths, or
      if they are empty.

### AddReservoirConstraint
```python
CpModel.AddReservoirConstraint(self, times, demands, min_level, max_level)
```
Adds Reservoir(times, demands, min_level, max_level).

Maintains a reservoir level within bounds. The water level starts at 0, and
at any time >= 0, it must be between min_level and max_level. Furthermore,
this constraints expect all times variables to be >= 0.
If the variable `times[i]` is assigned a value t, then the current level
changes by `demands[i]`, which is constant, at time t.

Note that level min can be > 0, or level max can be < 0. It just forces
some demands to be executed at time 0 to make sure that we are within those
bounds with the executed demands. Therefore, at any time t >= 0:

    sum(demands[i] if times[i] <= t) in [min_level, max_level]

**Args:**

- *times:* A list of positive integer variables which specify the time of the
    filling or emptying the reservoir.
- *demands:* A list of integer values that specifies the amount of the
    emptying or feeling.
- *min_level:* At any time >= 0, the level of the reservoir must be greater of
    equal than the min level.
- *max_level:* At any time >= 0, the level of the reservoir must be less or
    equal than the max level.

**Returns:**
  An instance of the `Constraint` class.

**Raises:**

- *ValueError:* if max_level < min_level.

### AddReservoirConstraintWithActive
```python
CpModel.AddReservoirConstraintWithActive(self, times, demands, actives, min_level, max_level)
```
Adds Reservoir(times, demands, actives, min_level, max_level).

Maintain a reservoir level within bounds. The water level starts at 0, and
at
any time >= 0, it must be within min_level, and max_level. Furthermore, this
constraints expect all times variables to be >= 0.
If `actives[i]` is true, and if `times[i]` is assigned a value t, then the
level of the reservoir changes by `demands[i]`, which is constant, at
time t.

Note that level_min can be > 0, or level_max can be < 0. It just forces
some demands to be executed at time 0 to make sure that we are within those
bounds with the executed demands. Therefore, at any time t >= 0:

    sum(demands[i] * actives[i] if times[i] <= t) in [min_level, max_level]

The array of boolean variables 'actives', if defined, indicates which
actions are actually performed.

**Args:**

- *times:* A list of positive integer variables which specify the time of the
    filling or emptying the reservoir.
- *demands:* A list of integer values that specifies the amount of the
    emptying or feeling.
- *actives:* a list of boolean variables. They indicates if the
    emptying/refilling events actually take place.
- *min_level:* At any time >= 0, the level of the reservoir must be greater of
    equal than the min level.
- *max_level:* At any time >= 0, the level of the reservoir must be less or
    equal than the max level.

**Returns:**
  An instance of the `Constraint` class.

**Raises:**

- *ValueError:* if max_level < min_level.

### AddMapDomain
```python
CpModel.AddMapDomain(self, var, bool_var_array, offset=0)
```
Adds `var == i + offset <=> bool_var_array[i] == true for all i`.
### AddImplication
```python
CpModel.AddImplication(self, a, b)
```
Adds `a => b`.
### AddBoolOr
```python
CpModel.AddBoolOr(self, literals)
```
Adds `Or(literals) == true`.
### AddBoolAnd
```python
CpModel.AddBoolAnd(self, literals)
```
Adds `And(literals) == true`.
### AddBoolXOr
```python
CpModel.AddBoolXOr(self, literals)
```
Adds `XOr(literals) == true`.
### AddMinEquality
```python
CpModel.AddMinEquality(self, target, variables)
```
Adds `target == Min(variables)`.
### AddMaxEquality
```python
CpModel.AddMaxEquality(self, target, variables)
```
Adds `target == Max(variables)`.
### AddDivisionEquality
```python
CpModel.AddDivisionEquality(self, target, num, denom)
```
Adds `target == num // denom` (integer division rounded towards 0).
### AddAbsEquality
```python
CpModel.AddAbsEquality(self, target, var)
```
Adds `target == Abs(var)`.
### AddModuloEquality
```python
CpModel.AddModuloEquality(self, target, var, mod)
```
Adds `target = var % mod`.
### AddMultiplicationEquality
```python
CpModel.AddMultiplicationEquality(self, target, variables)
```
Adds `target == variables[0] * .. * variables[n]`.
### AddProdEquality
```python
CpModel.AddProdEquality(self, target, variables)
```
Deprecated, use AddMultiplicationEquality.
### NewIntervalVar
```python
CpModel.NewIntervalVar(self, start, size, end, name)
```
Creates an interval variable from start, size, and end.

An interval variable is a constraint, that is itself used in other
constraints like NoOverlap.

Internally, it ensures that `start + size == end`.

**Args:**

- *start:* The start of the interval. It can be an integer value, or an
    integer variable.
- *size:* The size of the interval. It can be an integer value, or an integer
    variable.
- *end:* The end of the interval. It can be an integer value, or an integer
    variable.
- *name:* The name of the interval variable.

**Returns:**
  An `IntervalVar` object.

### NewOptionalIntervalVar
```python
CpModel.NewOptionalIntervalVar(self, start, size, end, is_present, name)
```
Creates an optional interval var from start, size, end, and is_present.

An optional interval variable is a constraint, that is itself used in other
constraints like NoOverlap. This constraint is protected by an is_present
literal that indicates if it is active or not.

Internally, it ensures that `is_present implies start + size == end`.

**Args:**

- *start:* The start of the interval. It can be an integer value, or an
    integer variable.
- *size:* The size of the interval. It can be an integer value, or an integer
    variable.
- *end:* The end of the interval. It can be an integer value, or an integer
    variable.
- *is_present:* A literal that indicates if the interval is active or not. A
    inactive interval is simply ignored by all constraints.
- *name:* The name of the interval variable.

**Returns:**
  An `IntervalVar` object.

### AddNoOverlap
```python
CpModel.AddNoOverlap(self, interval_vars)
```
Adds NoOverlap(interval_vars).

A NoOverlap constraint ensures that all present intervals do not overlap
in time.

**Args:**

- *interval_vars:* The list of interval variables to constrain.

**Returns:**
  An instance of the `Constraint` class.

### AddNoOverlap2D
```python
CpModel.AddNoOverlap2D(self, x_intervals, y_intervals)
```
Adds NoOverlap2D(x_intervals, y_intervals).

A NoOverlap2D constraint ensures that all present rectangles do not overlap
on a plane. Each rectangle is aligned with the X and Y axis, and is defined
by two intervals which represent its projection onto the X and Y axis.

**Args:**

- *x_intervals:* The X coordinates of the rectangles.
- *y_intervals:* The Y coordinates of the rectangles.

**Returns:**
  An instance of the `Constraint` class.

### AddCumulative
```python
CpModel.AddCumulative(self, intervals, demands, capacity)
```
Adds Cumulative(intervals, demands, capacity).

This constraint enforces that:

    for all t:
      sum(demands[i]
        if (start(intervals[t]) <= t < end(intervals[t])) and
        (t is present)) <= capacity

**Args:**

- *intervals:* The list of intervals.
- *demands:* The list of demands for each interval. Each demand must be >= 0.
    Each demand can be an integer value, or an integer variable.
- *capacity:* The maximum capacity of the cumulative constraint. It must be a
    positive integer value or variable.

**Returns:**
  An instance of the `Constraint` class.

### Proto
```python
CpModel.Proto(self)
```
Returns the underling CpModelProto.
### GetOrMakeIndex
```python
CpModel.GetOrMakeIndex(self, arg)
```
Returns the index of a variables, its negation, or a number.
### GetOrMakeBooleanIndex
```python
CpModel.GetOrMakeBooleanIndex(self, arg)
```
Returns an index from a boolean expression.
### Minimize
```python
CpModel.Minimize(self, obj)
```
Sets the objective of the model to minimize(obj).
### Maximize
```python
CpModel.Maximize(self, obj)
```
Sets the objective of the model to maximize(obj).
### AddDecisionStrategy
```python
CpModel.AddDecisionStrategy(self, variables, var_strategy, domain_strategy)
```
Adds a search strategy to the model.

**Args:**

- *variables:* a list of variables this strategy will assign.
- *var_strategy:* heuristic to choose the next variable to assign.
- *domain_strategy:* heuristic to reduce the domain of the selected variable.
    Currently, this is advanced code: the union of all strategies added to
      the model must be complete, i.e. instantiates all variables.
      Otherwise, Solve() will fail.

### ModelStats
```python
CpModel.ModelStats(self)
```
Returns a string containing some model statistics.
### Validate
```python
CpModel.Validate(self)
```
Returns a string indicating that the model is invalid.
## EvaluateLinearExpr
```python
EvaluateLinearExpr(expression, solution)
```
Evaluate a linear expression against a solution.
## EvaluateBooleanExpression
```python
EvaluateBooleanExpression(literal, solution)
```
Evaluate a boolean expression against a solution.
## CpSolver
```python
CpSolver(self)
```
Main solver class.

The purpose of this class is to search for a solution to the model provided
to the Solve() method.

Once Solve() is called, this class allows inspecting the solution found
with the Value() and BooleanValue() methods, as well as general statistics
about the solve procedure.

### Solve
```python
CpSolver.Solve(self, model)
```
Solves the given model and returns the solve status.
### SolveWithSolutionCallback
```python
CpSolver.SolveWithSolutionCallback(self, model, callback)
```
Solves a problem and passes each solution found to the callback.
### SearchForAllSolutions
```python
CpSolver.SearchForAllSolutions(self, model, callback)
```
Search for all solutions of a satisfiability problem.

This method searches for all feasible solutions of a given model.
Then it feeds the solution to the callback.

Note that the model cannot contain an objective.

**Args:**

- *model:* The model to solve.
- *callback:* The callback that will be called at each solution.

**Returns:**
  The status of the solve:

  * *FEASIBLE* if some solutions have been found
  * *INFEASIBLE* if the solver has proved there are no solution
  * *OPTIMAL* if all solutions have been found

### Value
```python
CpSolver.Value(self, expression)
```
Returns the value of a linear expression after solve.
### BooleanValue
```python
CpSolver.BooleanValue(self, literal)
```
Returns the boolean value of a literal after solve.
### ObjectiveValue
```python
CpSolver.ObjectiveValue(self)
```
Returns the value of the objective after solve.
### BestObjectiveBound
```python
CpSolver.BestObjectiveBound(self)
```
Returns the best lower (upper) bound found when min(max)imizing.
### StatusName
```python
CpSolver.StatusName(self, status)
```
Returns the name of the status returned by Solve().
### NumBooleans
```python
CpSolver.NumBooleans(self)
```
Returns the number of boolean variables managed by the SAT solver.
### NumConflicts
```python
CpSolver.NumConflicts(self)
```
Returns the number of conflicts since the creation of the solver.
### NumBranches
```python
CpSolver.NumBranches(self)
```
Returns the number of search branches explored by the solver.
### WallTime
```python
CpSolver.WallTime(self)
```
Returns the wall time in seconds since the creation of the solver.
### UserTime
```python
CpSolver.UserTime(self)
```
Returns the user time in seconds since the creation of the solver.
### ResponseStats
```python
CpSolver.ResponseStats(self)
```
Returns some statistics on the solution found as a string.
### ResponseProto
```python
CpSolver.ResponseProto(self)
```
Returns the response object.
## CpSolverSolutionCallback
```python
CpSolverSolutionCallback(self)
```
Solution callback.

This class implements a callback that will be called at each new solution
found during search.

The method OnSolutionCallback() will be called by the solver, and must be
implemented. The current solution can be queried using the BooleanValue()
and Value() methods.

It inherits the following methods from its base class:

* `ObjectiveValue(self)`
* `BestObjectiveBound(self)`
* `NumBooleans(self)`
* `NumConflicts(self)`
* `NumBranches(self)`
* `WallTime(self)`
* `UserTime(self)`

These methods returns the same information as their counterpart in the
`CpSolver` class.

### OnSolutionCallback
```python
CpSolverSolutionCallback.OnSolutionCallback(self)
```
Proxy for the same method in snake case.
### BooleanValue
```python
CpSolverSolutionCallback.BooleanValue(self, lit)
```
Returns the boolean value of a boolean literal.

**Args:**

- *lit:* A boolean variable or its negation.

**Returns:**
    The Boolean value of the literal in the solution.

**Raises:**

- *RuntimeError:* if `lit` is not a boolean variable or its negation.

### Value
```python
CpSolverSolutionCallback.Value(self, expression)
```
Evaluates an linear expression in the current solution.

**Args:**

- *expression:* a linear expression of the model.

**Returns:**
    An integer value equal to the evaluation of the linear expression
    against the current solution.

**Raises:**

- *RuntimeError:* if 'expression' is not a LinearExpr.

## ObjectiveSolutionPrinter
```python
ObjectiveSolutionPrinter(self)
```
Display the objective value and time of intermediate solutions.
### on_solution_callback
```python
ObjectiveSolutionPrinter.on_solution_callback(self)
```
Called on each new solution.
### solution_count
```python
ObjectiveSolutionPrinter.solution_count(self)
```
Returns the number of solutions found.
## VarArrayAndObjectiveSolutionPrinter
```python
VarArrayAndObjectiveSolutionPrinter(self, variables)
```
Print intermediate solutions (objective, variable values, time).
### on_solution_callback
```python
VarArrayAndObjectiveSolutionPrinter.on_solution_callback(self)
```
Called on each new solution.
### solution_count
```python
VarArrayAndObjectiveSolutionPrinter.solution_count(self)
```
Returns the number of solutions found.
## VarArraySolutionPrinter
```python
VarArraySolutionPrinter(self, variables)
```
Print intermediate solutions (variable values, time).
### on_solution_callback
```python
VarArraySolutionPrinter.on_solution_callback(self)
```
Called on each new solution.
### solution_count
```python
VarArraySolutionPrinter.solution_count(self)
```
Returns the number of solutions found.
