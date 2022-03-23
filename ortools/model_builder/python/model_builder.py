# Copyright 2010-2021 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Methods for building and solving model_builder models.

The following two sections describe the main
methods for building and solving those models.

* [`ModelBuilder`](#model_builder.ModelBuilder): Methods for creating
models, including variables and constraints.
* [`ModelSolver`](#model_builder.ModelSolver): Methods for solving
a model and evaluating solutions.

Additional methods for solving ModelBuilder models:

* [`Constraint`](#model_builder.Constraint): A few utility methods for modifying
  constraints created by `ModelBuilder`.
* [`LinearExpr`](#model_builder.LinearExpr): Methods for creating constraints
  and the objective from large arrays of coefficients.

Other methods and functions listed are primarily used for developing OR-Tools,
rather than for solving specific optimization problems.
"""

import math

from ortools.linear_solver import linear_solver_pb2
from ortools.model_builder.python import model_builder_helper as mbh
from ortools.model_builder.python import pywrap_model_builder_helper as pwmb

# Forward solve statuses.
OPTIMAL = linear_solver_pb2.MPSOLVER_OPTIMAL
FEASIBLE = linear_solver_pb2.MPSOLVER_FEASIBLE
INFEASIBLE = linear_solver_pb2.MPSOLVER_INFEASIBLE
UNBOUNDED = linear_solver_pb2.MPSOLVER_UNBOUNDED
ABNORMAL = linear_solver_pb2.MPSOLVER_ABNORMAL
NOT_SOLVED = linear_solver_pb2.MPSOLVER_NOT_SOLVED
MODEL_IS_VALID = linear_solver_pb2.MPSOLVER_MODEL_IS_VALID
CANCELLED_BY_USER = linear_solver_pb2.MPSOLVER_CANCELLED_BY_USER
UNKNOWN_STATUS = linear_solver_pb2.MPSOLVER_UNKNOWN_STATUS
MODEL_INVALID = linear_solver_pb2.MPSOLVER_MODEL_INVALID


class LinearExpr(object):
    """Holds an linear expression.

  A linear expression is built from integer constants and variables.
  For example, `x + 2 * (y - z + 1)`.

  Linear expressions are used in ModelBuilder models in constraints and in the
  objective:

  * You can define linear constraints as in:

  ```
  model.Add(x + 2 * y <= 5)
  model.Add(sum(array_of_vars) == 5)
  ```

  * In ModelBuilder, the objective is a linear expression:

  ```
  model.Minimize(x + 2 * y + z)
  ```

  * For large arrays, using the LinearExpr class is faster that using the python
  `sum()` function. You can create constraints and the objective from lists of
  linear expressions or coefficients as follows:

  ```
  model.Minimize(model_builder.LinearExpr.Sum(expressions))
  model.Add(model_builder.LinearExpr.WeightedSum(expressions, coeffs) >= 0)
  ```
  """

    @classmethod
    def sum(cls, expressions):
        """Creates the expression sum(expressions)."""
        if len(expressions) == 1:
            return expressions[0]
        return _SumArray(expressions)

    @classmethod
    def weighted_sum(cls, expressions, coefficients):
        """Creates the expression sum(expressions[i] * coefficients[i])."""
        if LinearExpr.is_empty_or_null(coefficients):
            return 0
        elif len(expressions) == 1:
            return expressions[0] * coefficients[0]
        else:
            return _WeightedSum(expressions, coefficients)

    @classmethod
    def term(cls, expression, coefficient):
        """Creates `expression * coefficient`."""
        if mbh.is_zero(coefficient):
            return 0
        else:
            return expression * coefficient

    @classmethod
    def is_empty_or_null(cls, coefficients):
        for c in coefficients:
            if not mbh.is_zero(c):
                return False
        return True

    def get_var_value_map(self):
        """Scans the expression. Returns (var_coef_map, constant)."""
        coeffs = {}
        constant = 0.0
        to_process = [(self, 1.0)]
        while to_process:  # Flatten to avoid recursion.
            expr, coeff = to_process.pop()
            if mbh.is_a_number(expr):
                constant += coeff * mbh.assert_is_a_number(expr)
            elif isinstance(expr, _ProductCst):
                to_process.append(
                    (expr.expression(), coeff * expr.coefficient()))
            elif isinstance(expr, _Sum):
                to_process.append((expr.left(), coeff))
                to_process.append((expr.right(), coeff))
            elif isinstance(expr, _SumArray):
                for e in expr.expressions():
                    to_process.append((e, coeff))
                constant += expr.constant() * coeff
            elif isinstance(expr, _WeightedSum):
                for e, c in zip(expr.expressions(), expr.coefficients()):
                    to_process.append((e, coeff * c))
                constant += expr.constant() * coeff
            elif isinstance(expr, Variable):
                if expr in coeffs:
                    coeffs[expr] += coeff
                else:
                    coeffs[expr] = coeff
            else:
                raise TypeError('Unrecognized linear expression: ' + str(expr))
        return coeffs, constant

    def __hash__(self):
        return object.__hash__(self)

    def __abs__(self):
        raise NotImplementedError(
            'calling abs() on a linear expression is not supported')

    def __add__(self, arg):
        if mbh.is_zero(arg):
            return self
        return _Sum(self, arg)

    def __radd__(self, arg):
        if mbh.is_zero(arg):
            return self
        return _Sum(self, arg)

    def __sub__(self, arg):
        if mbh.is_zero(arg):
            return self
        return _Sum(self, -arg)

    def __rsub__(self, arg):
        return _Sum(-self, arg)

    def __mul__(self, arg):
        arg = mbh.assert_is_a_number(arg)
        if mbh.is_one(arg):
            return self
        elif mbh.is_zero(arg):
            return 0
        return _ProductCst(self, arg)

    def __rmul__(self, arg):
        arg = mbh.assert_is_a_number(arg)
        if mbh.is_one(arg):
            return self
        elif mbh.is_zero(arg):
            return 0
        return _ProductCst(self, arg)

    def __div__(self, arg):
        coeff = mbh.assert_is_a_number(arg)
        if coeff == 0.0:
            raise ValueError(
                'Cannot call the division operator with a zero divisor')
        return self.__mul__(1.0 / coeff)

    def __truediv__(self, _):
        raise NotImplementedError(
            'calling // on a linear expression is not supported')

    def __mod__(self, _):
        raise NotImplementedError(
            'calling %% on a linear expression is not supported')

    def __pow__(self, _):
        raise NotImplementedError(
            'calling ** on a linear expression is not supported')

    def __lshift__(self, _):
        raise NotImplementedError(
            'calling left shift on a linear expression is not supported')

    def __rshift__(self, _):
        raise NotImplementedError(
            'calling right shift on a linear expression is not supported')

    def __and__(self, _):
        raise NotImplementedError(
            'calling and on a linear expression is not supported')

    def __or__(self, _):
        raise NotImplementedError(
            'calling or on a linear expression is not supported')

    def __xor__(self, _):
        raise NotImplementedError(
            'calling xor on a linear expression is not supported')

    def __neg__(self):
        return _ProductCst(self, -1)

    def __bool__(self):
        raise NotImplementedError(
            'Evaluating a LinearExpr instance as a Boolean is not implemented.')

    def __eq__(self, arg):
        if arg is None:
            return False
        if mbh.is_a_number(arg):
            arg = mbh.assert_is_a_number(arg)
            return BoundedLinearExpression(self, arg, arg)
        else:
            return BoundedLinearExpression(self - arg, 0, 0)

    def __ge__(self, arg):
        if mbh.is_a_number(arg):
            arg = mbh.assert_is_a_number(arg)
            return BoundedLinearExpression(self, arg, math.inf)
        else:
            return BoundedLinearExpression(self - arg, 0, math.inf)

    def __le__(self, arg):
        if mbh.is_a_number(arg):
            arg = mbh.assert_is_a_number(arg)
            return BoundedLinearExpression(self, -math.inf, arg)
        else:
            return BoundedLinearExpression(self - arg, -math.inf, 0)

    def __ne__(self, arg):
        raise NotImplementedError(
            'calling != on a linear expression is not supported')

    def __lt__(self, arg):
        raise NotImplementedError(
            'calling < on a linear expression is not supported')

    def __gt__(self, arg):
        raise NotImplementedError(
            'calling > on a linear expression is not supported')


class _Sum(LinearExpr):
    """Represents the sum of two LinearExprs."""

    def __init__(self, left, right):
        for x in [left, right]:
            if not mbh.is_a_number(x) and not isinstance(x, LinearExpr):
                raise TypeError('Not an linear expression: ' + str(x))
        self.__left = left
        self.__right = right

    def left(self):
        return self.__left

    def right(self):
        return self.__right

    def __str__(self):
        return f'({self.__left} + {self.__right})'

    def __repr__(self):
        return f'Sum({repr(self.__left)}, {repr(self.__right)})'


class _ProductCst(LinearExpr):
    """Represents the product of a LinearExpr by a constant."""

    def __init__(self, expr, coeff):
        coeff = mbh.assert_is_a_number(coeff)
        if isinstance(expr, _ProductCst):
            self.__expr = expr.Expression()
            self.__coef = expr.Coefficient() * coeff
        else:
            self.__expr = expr
            self.__coef = coeff

    def __str__(self):
        if self.__coef == -1:
            return '-' + str(self.__expr)
        else:
            return '(' + str(self.__coef) + ' * ' + str(self.__expr) + ')'

    def __repr__(self):
        return 'ProductCst(' + repr(self.__expr) + ', ' + repr(
            self.__coef) + ')'

    def coefficient(self):
        return self.__coef

    def expression(self):
        return self.__expr


class _SumArray(LinearExpr):
    """Represents the sum of a list of LinearExpr and a constant."""

    def __init__(self, expressions, constant=0):
        self.__expressions = []
        self.__constant = constant
        for x in expressions:
            if mbh.is_a_number(x):
                if mbh.is_zero(x):
                    continue
                x = mbh.assert_is_a_number(x)
                self.__constant += x
            elif isinstance(x, LinearExpr):
                self.__expressions.append(x)
            else:
                raise TypeError('Not an linear expression: ' + str(x))

    def __str__(self):
        if self.__constant == 0:
            return '({})'.format(' + '.join(map(str, self.__expressions)))
        else:
            return '({} + {})'.format(' + '.join(map(str, self.__expressions)),
                                      self.__constant)

    def __repr__(self):
        return 'SumArray({}, {})'.format(
            ', '.join(map(repr, self.__expressions)), self.__constant)

    def expressions(self):
        return self.__expressions

    def constant(self):
        return self.__constant


class _WeightedSum(LinearExpr):
    """Represents sum(ai * xi) + b."""

    def __init__(self, expressions, coefficients, constant=0):
        self.__expressions = []
        self.__coefficients = []
        self.__constant = constant
        if len(expressions) != len(coefficients):
            raise TypeError(
                'In the LinearExpr.WeightedSum method, the expression array and the '
                ' coefficient array must have the same length.')
        for e, c in zip(expressions, coefficients):
            c = mbh.assert_is_a_number(c)
            if mbh.is_zero(c):
                continue
            if mbh.is_a_number(e):
                e = mbh.assert_is_a_number(e)
                self.__constant += e * c
            elif isinstance(e, LinearExpr):
                self.__expressions.append(e)
                self.__coefficients.append(c)
            else:
                raise TypeError('Not an linear expression: ' + str(e))

    def __str__(self):
        output = None
        for expr, coeff in zip(self.__expressions, self.__coefficients):
            if not output and mbh.is_one(coeff):
                output = str(expr)
            elif not output and mbh.is_minus_one(coeff):
                output = '-' + str(expr)
            elif not output:
                output = '{} * {}'.format(coeff, str(expr))
            elif mbh.is_one(coeff):
                output += ' + {}'.format(str(expr))
            elif mbh.is_minus_one(coeff):
                output += ' - {}'.format(str(expr))
            elif coeff > 1:
                output += ' + {} * {}'.format(coeff, str(expr))
            elif coeff < -1:
                output += ' - {} * {}'.format(-coeff, str(expr))
        if self.__constant > 0:
            output += ' + {}'.format(self.__constant)
        elif self.__constant < 0:
            output += ' - {}'.format(-self.__constant)
        if output is None:
            output = '0'
        return output

    def __repr__(self):
        return 'WeightedSum([{}], [{}], {})'.format(
            ', '.join(map(repr, self.__expressions)),
            ', '.join(map(repr, self.__coefficients)), self.__constant)

    def expressions(self):
        return self.__expressions

    def coefficients(self):
        return self.__coefficients

    def constant(self):
        return self.__constant


class Variable(LinearExpr):
    """An integer variable.

  An Variable is an object that can take on any integer value within defined
  ranges. Variables appear in constraint like:

      x + y >= 5

  Solving a model is equivalent to finding, for each variable, a single value
  from the set of initial values (called the initial domain), such that the
  model is feasible, or optimal if you provided an objective function.
  """

    def __init__(self, helper, lb, ub, is_integral, name):
        """See CpModel.NewVariable below."""
        self.__helper = helper
        # Python do not support multiple __init__ methods.
        # This method is only called from the ModelBuilder class.
        # We hack the parameter to support the two cases:
        # case 1:
        #     helper is a ModelBuilderHelper, lb is a double value, ub is a double
        #     value, is_integral is a Boolean value, and name is a string.
        # case 2:
        #     helper is a ModelBuilderHelper, lb is an index (int), ub is None,
        #     is_integral is None, and name is None.
        if mbh.is_integral(lb) and ub is None and is_integral is None:
            self.__index = int(lb)
            self.__helper = helper
        else:
            index = helper.AddVar()
            self.__index = index
            self.__helper = helper
            helper.SetVarLowerBound(index, lb)
            helper.SetVarUpperBound(index, ub)
            helper.SetVarIntegrality(index, is_integral)
            if name:
                helper.SetVarName(index, name)

    def index(self):
        """Returns the index of the variable in the helper."""
        return self.__index

    def helper(self):
        """Returns the underlying ModelBuilderHelper."""
        return self.__helper

    def is_equal_to(self, other):
        """Returns true if self == other in the python sense."""
        if not isinstance(other, Variable):
            return False
        return self.index() == other.index()

    def __str__(self):
        name = self.__helper.var_name(self.__index)
        if not name:
            if self.__helper.VarIsInteger(self.__index):
                return 'unnamed_int_var_%i' % self.__index
            else:
                return 'unnamed_num_var_%i' % self.__index
        return name

    def __repr__(self):
        index = self.__index
        name = self.__helper.VarName(index)
        lb = self.__helper.VarLowerBound(index)
        ub = self.__helper.VarUpperBound(index)
        is_integer = self.__helper.VarIsInteger(index)
        if name:
            if is_integer:
                return f'{name}(index={index}, lb={lb}, ub={ub}, integer)'
            else:
                return f'{name}(index={index}, lb={lb}, ub={ub})'
        else:
            if is_integer:
                return f'unnamed_var(index={index}, lb={lb}, ub={ub}, integer)'
            else:
                return f'unnamed_var(index={index}, lb={lb}, ub={ub})'

    def name(self):
        return self.__helper.var_name(self.__index)


class BoundedLinearExpression(object):
    """Represents a linear constraint: `lb <= linear expression <= ub`.

  The only use of this class is to be added to the ModelBuilder through
  `ModelBuilder.Add(bounded expression)`, as in:

      model.Add(x + 2 * y -1 >= z)
  """

    def __init__(self, expr, lb, ub):
        self.__expr = expr
        self.__lb = lb
        self.__ub = ub

    def __str__(self):
        if self.__lb > -math.inf and self.__ub < math.inf:
            if self.__lb == self.__ub:
                return str(self.__expr) + ' == ' + str(self.__lb)
            else:
                return str(self.__lb) + ' <= ' + str(
                    self.__expr) + ' <= ' + str(self.__ub)
        elif self.__lb > -math.inf:
            return str(self.__expr) + ' >= ' + str(self.__lb)
        elif self.__ub < math.inf:
            return str(self.__expr) + ' <= ' + str(self.__ub)
        else:
            return 'True (unbounded expr ' + str(self.__expr) + ')'

    def expression(self):
        return self.__expr

    def lower_bound(self):
        return self.__lb

    def upper_bound(self):
        return self.__ub

    def __bool__(self):
        coeffs_map, constant = self.__expr.get_var_value_map()
        all_coeffs = set(coeffs_map.values())
        same_var = set([0])
        different_vars = set([-1.0, 1.0])
        if (len(coeffs_map) == 1 and all_coeffs == same_var and
                constant == 0.0 and self.__lb == 0.0 and self.__ub == 0.0):
            return True
        if (len(coeffs_map) == 2 and all_coeffs == different_vars and
                constant == 0.0 and self.__lb == 0.0 and self.__ub == 0.0):
            variables = coeffs_map.keys()
            return variables[0] == variables[1]

        raise NotImplementedError(
            f'Evaluating a BoundedLinearExpression \'{self}\' as a Boolean value'
            + ' is not supported.')


class LinearConstraint(object):
    """Stores a linear equation.

  Example:

      x = model.NewVariable(0, 10, 'x')
      y = model.NewVariable(0, 10, 'y')

      model.Add(x + 2 * y == 5)
  """

    def __init__(self, helper):
        self.__index = helper.AddLinearConstraint()
        self.__helper = helper

    def index(self):
        """Returns the index of the constraint in the helper."""
        return self.__index

    def helper(self):
        """Returns the constraint protobuf."""
        return self.__helper


class ModelBuilder(object):
    """Methods for building a linear model.

  Methods beginning with:

  * ```New``` create integer, boolean, or interval variables.
  * ```Add``` create new constraints and add them to the model.
  """

    def __init__(self):
        self.__helper = pwmb.ModelBuilderHelper()
        self.__constant_map = {}

    # Integer variable.

    def new_var(self, lb, ub, is_integer, name):
        """Create an integer variable with domain [lb, ub].

    Args:
      lb: Lower bound for the variable.
      ub: Upper bound for the variable.
      is_integer: Indicates if the variable must take integral values.
      name: The name of the variable.

    Returns:
      a variable whose domain is [lb, ub].
    """

        return Variable(self.__helper, lb, ub, is_integer, name)

    def new_int_var(self, lb, ub, name):
        """Create an integer variable with domain [lb, ub].

    Args:
      lb: Lower bound for the variable.
      ub: Upper bound for the variable.
      name: The name of the variable.

    Returns:
      a variable whose domain is [lb, ub].
    """

        return self.new_var(lb, ub, True, name)

    def new_num_var(self, lb, ub, name):
        """Create an integer variable with domain [lb, ub].

    Args:
      lb: Lower bound for the variable.
      ub: Upper bound for the variable.
      name: The name of the variable.

    Returns:
      a variable whose domain is [lb, ub].
    """

        return self.new_var(lb, ub, False, name)

    def new_bool_var(self, name):
        """Creates a 0-1 variable with the given name."""
        return self.new_var(0, 1, True, name)

    def new_constant(self, value):
        """Declares a constant variable."""
        return self.new_var(value, value, False, '')

    def var_from_index(self, index):
        """Rebuilds a variable object from the model and its index."""
        return Variable(self.__helper, index, None, None, None)

    def num_variables(self):
        """Returns the number of variables in the model."""
        return self.__helper.num_variables()

    def var_lower_bound(self, var):
        """Returns the lower bound of the variable."""
        return self.__helper.var_lower_bound(var)

    def set_var_lower_bound(self, var, bound):
        """Sets the lower bound of the variable."""
        self.__helper.set_var_lower_bound(var, bound)

    def var_upper_bound(self, var):
        """Returns the upper bound of the variable."""
        return self.__helper.var_upper_bound(var)

    def set_var_upper_bound(self, var, bound):
        """Sets the upper bound of the variable."""
        self.__helper.set_var_upper_bound(var, bound)

    def var_is_integral(self, var):
        """Returns whether the variable is integral."""
        return self.__helper.var_is_integral(var.index())

    def set_var_integrality(self, var, is_integral):
        """Sets the integrality of the variable."""
        self.__helper.SetVarIntegrality(var.index(), is_integral)

    def var_name(self, var):
        """Returns the name of the variable."""
        return self.__helper.variable_name(var.index())

    def set_var_name(self, var, name):
        """Sets the name of the variable."""
        self.__helper.SetVarName(var.index(), name)

    # Linear constraints.

    def add_linear_constraint(self,
                              linear_expr,
                              lb=-math.inf,
                              ub=math.inf,
                              name=None):
        """Adds the constraint: `lb <= linear_expr <= ub` with the given name."""
        ct = LinearConstraint(self.__helper)
        index = ct.index()
        coeffs_map = {}
        constant = 0.0
        if isinstance(linear_expr, LinearExpr):
            coeffs_map, constant = linear_expr.get_var_value_map()
        elif mbh.is_a_number(linear_expr):
            constant = mbh.assert_is_a_number(linear_expr)
        else:
            raise TypeError(
                'Not supported: ModelBuilder.add_linear_constraint(' +
                f'{lb} <= {linear_expr} <= {ub})')

        for t in coeffs_map.items():
            if not isinstance(t[0], Variable):
                raise TypeError('Wrong argument' + str(t))
            c = mbh.assert_is_a_number(t[1])
            self.__helper.AddConstraintTerm(index, t[0].index(), c)
        self.__helper.SetConstraintLowerBound(index, lb - constant)
        self.__helper.SetConstraintUpperBound(index, ub - constant)
        if name:
            self.__helper.SetConstraintName(index, name)
        return ct

    def add(self, ct, name=None):
        """Adds a `BoundedLinearExpression` to the model.

    Args:
      ct: A [`BoundedLinearExpression`](#boundedlinearexpression).
      name: An optional name.

    Returns:
      An instance of the `Constraint` class.
    """
        if isinstance(ct, BoundedLinearExpression):
            return self.add_linear_constraint(ct.expression(), ct.lower_bound(),
                                              ct.upper_bound(), name)
        elif ct and isinstance(ct, bool):
            return self.add_linear_constraint(
                linear_expr=0.0)  # Evaluate to True.
        elif not ct and isinstance(ct, bool):
            return self.add_linear_constraint(1.0, 0.0,
                                              0.0)  # Evaluate to False.
        else:
            raise TypeError('Not supported: ModelBuilder.Add(' + str(ct) + ')')

    def num_constraints(self):
        return self.__helper.num_constraints()

    def constraint_lower_bound(self, ct):
        return self.__helper.constraint_lower_bound(ct.index())

    def set_constraint_lower_bound(self, ct, bound):
        self.__helper.SetConstraintLowerBound(ct.index(), bound)

    def constraint_upper_bound(self, ct):
        return self.__helper.constraint_upper_bound(ct.index())

    def set_constraint_upper_bound(self, ct, bound):
        self.__helper.SetConstraintUpperBound(ct.index(), bound)

    def constraint_name(self, ct):
        return self.__helper.constraint_name(ct.index())

    def set_constraint_name(self, ct, name):
        return self.__helper.SetConstraintName(ct.index(), name)

    def add_term_to_constraint(self, ct, var, coeff):
        self.__helper.AddConstraintTerm(ct.index(), var.index(), coeff)

    # Objective.

    def minimize(self, linear_expr):
        self.__optimize(linear_expr, False)

    def maximize(self, linear_expr):
        self.__optimize(linear_expr, True)

    def __optimize(self, linear_expr, maximize):
        """Defines the objective."""
        coeffs_map = {}
        # constant = 0.0
        if isinstance(linear_expr, LinearExpr):
            coeffs_map, constant = linear_expr.get_var_value_map()
        elif mbh.is_a_number(linear_expr):
            constant = mbh.assert_is_a_number(linear_expr)
        else:
            raise TypeError(
                f'Not supported: ModelBuilder.minimize/maximize({linear_expr})')

        for t in coeffs_map.items():
            if not isinstance(t[0], Variable):
                raise TypeError('Wrong argument' + str(t))
            c = mbh.assert_is_a_number(t[1])
            self.__helper.SetVarObjectiveCoefficient(t[0].index(), c)
        self.__helper.SetObjectiveOffset(constant)
        self.__helper.SetMaximize(maximize)

    def objective_offset(self):
        return self.__helper.objective_offset()

    def set_objective_offset(self, offset):
        self.__helper.SetObjectiveOffset(offset)

    def objective_coefficient(self, var):
        return self.__helper.var_objective_coefficient(var.index())

    def set_objective_coefficient(self, var, coeff):
        return self.__helper.SetVarObjectiveCoefficient(var.index(), coeff)

    # Solver backend and parameters.
    def set_solver(self, solver_name):
        return self.__helper.SetSolverName(solver_name)

    # Utilities
    def name(self):
        return self.__helper.name()

    def set_name(self, name):
        self.__helper.SetName(name)

    def helper(self):
        """Returns the model builder helper."""
        return self.__helper


class ModelSolver(object):
    """Main solver class.

  The purpose of this class is to search for a solution to the model provided
  to the Solve() method.

  Once Solve() is called, this class allows inspecting the solution found
  with the Value() method, as well as general statistics about the solve
  procedure.
  """

    def __init__(self):
        self.__solve_helper = pwmb.ModelSolverHelper()
        self.log_callback = None

    def solve(self, model):
        """Solves a problem and passes each solution to the callback if not null."""

        # swig_helper.SolveWrapper.SetSerializedParameters(
        #     self.parameters.SerializeToString(), solve_wrapper)
        if self.log_callback is not None:
            self.__solve_helper.SetLogCallback(self.log_callback)
        self.__solve_helper.Solve(model.helper())
        return self.__solve_helper.status()

    def stop_search(self):
        """Stops the current search asynchronously."""
        self.__solve_helper.InterruptSolve()

    def value(self, variable):
        """Returns the value of a linear expression after solve."""
        if not self.__solve_helper.has_response():
            raise RuntimeError('Solve() has not be called.')
        return self.__solve_helper.var_value(variable.index())

    def reduced_cost(self, variable):
        """Returns the reduced cost of a linear expression after solve."""
        if not self.__solve_helper.has_response():
            raise RuntimeError('Solve() has not be called.')
        return self.__solve_helper.reduced_cost(variable.index())

    def dual_value(self, ct):
        """Returns the dual value of a linear constraint after solve."""
        if not self.__solve_helper.has_response():
            raise RuntimeError('Solve() has not be called.')
        return self.__solve_helper.dual_value(ct.index())

    def objective_value(self):
        """Returns the value of the objective after solve."""
        if not self.__solve_helper.has_response():
            raise RuntimeError('Solve() has not be called.')
        return self.__solve_helper.objective_value()

    def best_objective_bound(self):
        """Returns the best lower (upper) bound found when min(max)imizing."""
        if not self.__solve_helper.has_response():
            raise RuntimeError('Solve() has not be called.')

        return self.__solve_helper.best_objective_bound()

    def status_name(self, status=None):
        """Returns the name of the status returned by Solve()."""
        if status is None:
            status = self.__solve_helper.status()
        return linear_solver_pb2.MPSolverResponseStatus.Name(status)

    def status_string(self):
        """Returns additional information of the last solve.

    It can describe why the model is invalid.
    """
        return self.__solve_helper.status_string()

    # def WallTime(self):
    #   """Returns the wall time in seconds since the creation of the solver."""
    #   return self.__solution.wall_time

    # def UserTime(self):
    #   """Returns the user time in seconds since the creation of the solver."""
    #   return self.__solution.user_time
