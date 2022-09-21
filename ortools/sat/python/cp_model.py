# Copyright 2010-2022 Google LLC
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
"""Methods for building and solving CP-SAT models.

The following two sections describe the main
methods for building and solving CP-SAT models.

* [`CpModel`](#cp_model.CpModel): Methods for creating
models, including variables and constraints.
* [`CPSolver`](#cp_model.CpSolver): Methods for solving
a model and evaluating solutions.

The following methods implement callbacks that the
solver calls each time it finds a new solution.

* [`CpSolverSolutionCallback`](#cp_model.CpSolverSolutionCallback):
  A general method for implementing callbacks.
* [`ObjectiveSolutionPrinter`](#cp_model.ObjectiveSolutionPrinter):
  Print objective values and elapsed time for intermediate solutions.
* [`VarArraySolutionPrinter`](#cp_model.VarArraySolutionPrinter):
  Print intermediate solutions (variable values, time).
* [`VarArrayAndObjectiveSolutionPrinter`]
      (#cp_model.VarArrayAndObjectiveSolutionPrinter):
  Print both intermediate solutions and objective values.

Additional methods for solving CP-SAT models:

* [`Constraint`](#cp_model.Constraint): A few utility methods for modifying
  constraints created by `CpModel`.
* [`LinearExpr`](#lineacp_model.LinearExpr): Methods for creating constraints
  and the objective from large arrays of coefficients.

Other methods and functions listed are primarily used for developing OR-Tools,
rather than for solving specific optimization problems.
"""

import collections
import threading
import time
from typing import Optional
import warnings

from ortools.sat import cp_model_pb2
from ortools.sat import sat_parameters_pb2
from ortools.sat.python import cp_model_helper as cmh
from ortools.sat.python import swig_helper
from ortools.util.python import sorted_interval_list

Domain = sorted_interval_list.Domain

# The classes below allow linear expressions to be expressed naturally with the
# usual arithmetic operators + - * / and with constant numbers, which makes the
# python API very intuitive. See../ samples/*.py for examples.

INT_MIN = -9223372036854775808  # hardcoded to be platform independent.
INT_MAX = 9223372036854775807
INT32_MAX = 2147483647
INT32_MIN = -2147483648

# CpSolver status (exported to avoid importing cp_model_cp2).
UNKNOWN = cp_model_pb2.UNKNOWN
MODEL_INVALID = cp_model_pb2.MODEL_INVALID
FEASIBLE = cp_model_pb2.FEASIBLE
INFEASIBLE = cp_model_pb2.INFEASIBLE
OPTIMAL = cp_model_pb2.OPTIMAL

# Variable selection strategy
CHOOSE_FIRST = cp_model_pb2.DecisionStrategyProto.CHOOSE_FIRST
CHOOSE_LOWEST_MIN = cp_model_pb2.DecisionStrategyProto.CHOOSE_LOWEST_MIN
CHOOSE_HIGHEST_MAX = cp_model_pb2.DecisionStrategyProto.CHOOSE_HIGHEST_MAX
CHOOSE_MIN_DOMAIN_SIZE = (
    cp_model_pb2.DecisionStrategyProto.CHOOSE_MIN_DOMAIN_SIZE)
CHOOSE_MAX_DOMAIN_SIZE = (
    cp_model_pb2.DecisionStrategyProto.CHOOSE_MAX_DOMAIN_SIZE)

# Domain reduction strategy
SELECT_MIN_VALUE = cp_model_pb2.DecisionStrategyProto.SELECT_MIN_VALUE
SELECT_MAX_VALUE = cp_model_pb2.DecisionStrategyProto.SELECT_MAX_VALUE
SELECT_LOWER_HALF = cp_model_pb2.DecisionStrategyProto.SELECT_LOWER_HALF
SELECT_UPPER_HALF = cp_model_pb2.DecisionStrategyProto.SELECT_UPPER_HALF

# Search branching
AUTOMATIC_SEARCH = sat_parameters_pb2.SatParameters.AUTOMATIC_SEARCH
FIXED_SEARCH = sat_parameters_pb2.SatParameters.FIXED_SEARCH
PORTFOLIO_SEARCH = sat_parameters_pb2.SatParameters.PORTFOLIO_SEARCH
LP_SEARCH = sat_parameters_pb2.SatParameters.LP_SEARCH


def DisplayBounds(bounds):
    """Displays a flattened list of intervals."""
    out = ''
    for i in range(0, len(bounds), 2):
        if i != 0:
            out += ', '
        if bounds[i] == bounds[i + 1]:
            out += str(bounds[i])
        else:
            out += str(bounds[i]) + '..' + str(bounds[i + 1])
    return out


def ShortName(model, i):
    """Returns a short name of an integer variable, or its negation."""
    if i < 0:
        return 'Not(%s)' % ShortName(model, -i - 1)
    v = model.variables[i]
    if v.name:
        return v.name
    elif len(v.domain) == 2 and v.domain[0] == v.domain[1]:
        return str(v.domain[0])
    else:
        return '[%s]' % DisplayBounds(v.domain)


def ShortExprName(model, e):
    """Pretty-print LinearExpressionProto instances."""
    if not e.vars:
        return str(e.offset)
    if len(e.vars) == 1:
        var_name = ShortName(model, e.vars[0])
        coeff = e.coeffs[0]
        result = ''
        if coeff == 1:
            result = var_name
        elif coeff == -1:
            result = f'-{var_name}'
        elif coeff != 0:
            result = f'{coeff} * {var_name}'
        if e.offset > 0:
            result = f'{result} + {e.offset}'
        elif e.offset < 0:
            result = f'{result} - {-e.offset}'
        return result
    # TODO(user): Support more than affine expressions.
    return str(e)


class LinearExpr(object):
    """Holds an integer linear expression.

  A linear expression is built from integer constants and variables.
  For example, `x + 2 * (y - z + 1)`.

  Linear expressions are used in CP-SAT models in constraints and in the
  objective:

  * You can define linear constraints as in:

  ```
  model.Add(x + 2 * y <= 5)
  model.Add(sum(array_of_vars) == 5)
  ```

  * In CP-SAT, the objective is a linear expression:

  ```
  model.Minimize(x + 2 * y + z)
  ```

  * For large arrays, using the LinearExpr class is faster that using the python
  `sum()` function. You can create constraints and the objective from lists of
  linear expressions or coefficients as follows:

  ```
  model.Minimize(cp_model.LinearExpr.Sum(expressions))
  model.Add(cp_model.LinearExpr.WeightedSum(expressions, coefficients) >= 0)
  ```
  """

    @classmethod
    def Sum(cls, expressions):
        """Creates the expression sum(expressions)."""
        if len(expressions) == 1:
            return expressions[0]
        return _SumArray(expressions)

    @classmethod
    def WeightedSum(cls, expressions, coefficients):
        """Creates the expression sum(expressions[i] * coefficients[i])."""
        if LinearExpr.IsEmptyOrAllNull(coefficients):
            return 0
        elif len(expressions) == 1:
            return expressions[0] * coefficients[0]
        else:
            return _WeightedSum(expressions, coefficients)

    @classmethod
    def Term(cls, expression, coefficient):
        """Creates `expression * coefficient`."""
        if cmh.is_zero(coefficient):
            return 0
        else:
            return expression * coefficient

    @classmethod
    def IsEmptyOrAllNull(cls, coefficients):
        for c in coefficients:
            if not cmh.is_zero(c):
                return False
        return True

    @classmethod
    def RebuildFromLinearExpressionProto(cls, model, proto):
        """Recreate a LinearExpr from a LinearExpressionProto."""
        offset = proto.offset
        num_elements = len(proto.vars)
        if num_elements == 0:
            return offset
        elif num_elements == 1:
            return IntVar(model, proto.vars[0], None) * proto.coeffs[0] + offset
        else:
            variables = []
            coeffs = []
            all_ones = True
            for index, coeff in zip(proto.vars(), proto.coeffs()):
                variables.append(IntVar(model, index, None))
                coeffs.append(coeff)
                if not cmh.is_one(coeff):
                    all_ones = False
            if all_ones:
                return _SumArray(variables, offset)
            else:
                return _WeightedSum(variables, coeffs, offset)

    def GetIntegerVarValueMap(self):
        """Scans the expression, and returns (var_coef_map, constant)."""
        coeffs = collections.defaultdict(int)
        constant = 0
        to_process = [(self, 1)]
        while to_process:  # Flatten to avoid recursion.
            expr, coeff = to_process.pop()
            if cmh.is_integral(expr):
                constant += coeff * int(expr)
            elif isinstance(expr, _ProductCst):
                to_process.append(
                    (expr.Expression(), coeff * expr.Coefficient()))
            elif isinstance(expr, _Sum):
                to_process.append((expr.Left(), coeff))
                to_process.append((expr.Right(), coeff))
            elif isinstance(expr, _SumArray):
                for e in expr.Expressions():
                    to_process.append((e, coeff))
                constant += expr.Constant() * coeff
            elif isinstance(expr, _WeightedSum):
                for e, c in zip(expr.Expressions(), expr.Coefficients()):
                    to_process.append((e, coeff * c))
                constant += expr.Constant() * coeff
            elif isinstance(expr, IntVar):
                coeffs[expr] += coeff
            elif isinstance(expr, _NotBooleanVariable):
                constant += coeff
                coeffs[expr.Not()] -= coeff
            else:
                raise TypeError('Unrecognized linear expression: ' + str(expr))

        return coeffs, constant

    def GetFloatVarValueMap(self):
        """Scans the expression. Returns (var_coef_map, constant, is_integer)."""
        coeffs = {}
        constant = 0
        to_process = [(self, 1)]
        while to_process:  # Flatten to avoid recursion.
            expr, coeff = to_process.pop()
            if cmh.is_integral(expr):  # Keep integrality.
                constant += coeff * int(expr)
            elif cmh.is_a_number(expr):
                constant += coeff * float(expr)
            elif isinstance(expr, _ProductCst):
                to_process.append(
                    (expr.Expression(), coeff * expr.Coefficient()))
            elif isinstance(expr, _Sum):
                to_process.append((expr.Left(), coeff))
                to_process.append((expr.Right(), coeff))
            elif isinstance(expr, _SumArray):
                for e in expr.Expressions():
                    to_process.append((e, coeff))
                constant += expr.Constant() * coeff
            elif isinstance(expr, _WeightedSum):
                for e, c in zip(expr.Expressions(), expr.Coefficients()):
                    to_process.append((e, coeff * c))
                constant += expr.Constant() * coeff
            elif isinstance(expr, IntVar):
                if expr in coeffs:
                    coeffs[expr] += coeff
                else:
                    coeffs[expr] = coeff
            elif isinstance(expr, _NotBooleanVariable):
                constant += coeff
                if expr.Not() in coeffs:
                    coeffs[expr.Not()] -= coeff
                else:
                    coeffs[expr.Not()] = -coeff
            else:
                raise TypeError('Unrecognized linear expression: ' + str(expr))
        is_integer = cmh.is_integral(constant)
        if is_integer:
            for coeff in coeffs.values():
                if not cmh.is_integral(coeff):
                    is_integer = False
                    break
        return coeffs, constant, is_integer

    def __hash__(self):
        return object.__hash__(self)

    def __abs__(self):
        raise NotImplementedError(
            'calling abs() on a linear expression is not supported, '
            'please use CpModel.AddAbsEquality')

    def __add__(self, arg):
        if cmh.is_zero(arg):
            return self
        return _Sum(self, arg)

    def __radd__(self, arg):
        if cmh.is_zero(arg):
            return self
        return _Sum(self, arg)

    def __sub__(self, arg):
        if cmh.is_zero(arg):
            return self
        return _Sum(self, -arg)

    def __rsub__(self, arg):
        return _Sum(-self, arg)

    def __mul__(self, arg):
        arg = cmh.assert_is_a_number(arg)
        if cmh.is_one(arg):
            return self
        elif cmh.is_zero(arg):
            return 0
        return _ProductCst(self, arg)

    def __rmul__(self, arg):
        arg = cmh.assert_is_a_number(arg)
        if cmh.is_one(arg):
            return self
        elif cmh.is_zero(arg):
            return 0
        return _ProductCst(self, arg)

    def __div__(self, _):
        raise NotImplementedError(
            'calling / on a linear expression is not supported, '
            'please use CpModel.AddDivisionEquality')

    def __truediv__(self, _):
        raise NotImplementedError(
            'calling // on a linear expression is not supported, '
            'please use CpModel.AddDivisionEquality')

    def __mod__(self, _):
        raise NotImplementedError(
            'calling %% on a linear expression is not supported, '
            'please use CpModel.AddModuloEquality')

    def __pow__(self, _):
        raise NotImplementedError(
            'calling ** on a linear expression is not supported, '
            'please use CpModel.AddMultiplicationEquality')

    def __lshift__(self, _):
        raise NotImplementedError(
            'calling left shift on a linear expression is not supported')

    def __rshift__(self, _):
        raise NotImplementedError(
            'calling right shift on a linear expression is not supported')

    def __and__(self, _):
        raise NotImplementedError(
            'calling and on a linear expression is not supported, '
            'please use CpModel.AddBoolAnd')

    def __or__(self, _):
        raise NotImplementedError(
            'calling or on a linear expression is not supported, '
            'please use CpModel.AddBoolOr')

    def __xor__(self, _):
        raise NotImplementedError(
            'calling xor on a linear expression is not supported, '
            'please use CpModel.AddBoolXor')

    def __neg__(self):
        return _ProductCst(self, -1)

    def __bool__(self):
        raise NotImplementedError(
            'Evaluating a LinearExpr instance as a Boolean is not implemented.')

    def __eq__(self, arg):
        if arg is None:
            return False
        if cmh.is_integral(arg):
            arg = cmh.assert_is_int64(arg)
            return BoundedLinearExpression(self, [arg, arg])
        else:
            return BoundedLinearExpression(self - arg, [0, 0])

    def __ge__(self, arg):
        if cmh.is_integral(arg):
            arg = cmh.assert_is_int64(arg)
            return BoundedLinearExpression(self, [arg, INT_MAX])
        else:
            return BoundedLinearExpression(self - arg, [0, INT_MAX])

    def __le__(self, arg):
        if cmh.is_integral(arg):
            arg = cmh.assert_is_int64(arg)
            return BoundedLinearExpression(self, [INT_MIN, arg])
        else:
            return BoundedLinearExpression(self - arg, [INT_MIN, 0])

    def __lt__(self, arg):
        if cmh.is_integral(arg):
            arg = cmh.assert_is_int64(arg)
            if arg == INT_MIN:
                raise ArithmeticError('< INT_MIN is not supported')
            return BoundedLinearExpression(self, [INT_MIN, arg - 1])
        else:
            return BoundedLinearExpression(self - arg, [INT_MIN, -1])

    def __gt__(self, arg):
        if cmh.is_integral(arg):
            arg = cmh.assert_is_int64(arg)
            if arg == INT_MAX:
                raise ArithmeticError('> INT_MAX is not supported')
            return BoundedLinearExpression(self, [arg + 1, INT_MAX])
        else:
            return BoundedLinearExpression(self - arg, [1, INT_MAX])

    def __ne__(self, arg):
        if arg is None:
            return True
        if cmh.is_integral(arg):
            arg = cmh.assert_is_int64(arg)
            if arg == INT_MAX:
                return BoundedLinearExpression(self, [INT_MIN, INT_MAX - 1])
            elif arg == INT_MIN:
                return BoundedLinearExpression(self, [INT_MIN + 1, INT_MAX])
            else:
                return BoundedLinearExpression(
                    self, [INT_MIN, arg - 1, arg + 1, INT_MAX])
        else:
            return BoundedLinearExpression(self - arg,
                                           [INT_MIN, -1, 1, INT_MAX])


class _Sum(LinearExpr):
    """Represents the sum of two LinearExprs."""

    def __init__(self, left, right):
        for x in [left, right]:
            if not cmh.is_a_number(x) and not isinstance(x, LinearExpr):
                raise TypeError('Not an linear expression: ' + str(x))
        self.__left = left
        self.__right = right

    def Left(self):
        return self.__left

    def Right(self):
        return self.__right

    def __str__(self):
        return f'({self.__left} + {self.__right})'

    def __repr__(self):
        return f'Sum({repr(self.__left)}, {repr(self.__right)})'


class _ProductCst(LinearExpr):
    """Represents the product of a LinearExpr by a constant."""

    def __init__(self, expr, coeff):
        coeff = cmh.assert_is_a_number(coeff)
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

    def Coefficient(self):
        return self.__coef

    def Expression(self):
        return self.__expr


class _SumArray(LinearExpr):
    """Represents the sum of a list of LinearExpr and a constant."""

    def __init__(self, expressions, constant=0):
        self.__expressions = []
        self.__constant = constant
        for x in expressions:
            if cmh.is_a_number(x):
                if cmh.is_zero(x):
                    continue
                x = cmh.assert_is_a_number(x)
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

    def Expressions(self):
        return self.__expressions

    def Constant(self):
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
            c = cmh.assert_is_a_number(c)
            if cmh.is_zero(c):
                continue
            if cmh.is_a_number(e):
                e = cmh.assert_is_a_number(e)
                self.__constant += e * c
            elif isinstance(e, LinearExpr):
                self.__expressions.append(e)
                self.__coefficients.append(c)
            else:
                raise TypeError('Not an linear expression: ' + str(e))

    def __str__(self):
        output = None
        for expr, coeff in zip(self.__expressions, self.__coefficients):
            if not output and cmh.is_one(coeff):
                output = str(expr)
            elif not output and cmh.is_minus_one(coeff):
                output = '-' + str(expr)
            elif not output:
                output = '{} * {}'.format(coeff, str(expr))
            elif cmh.is_one(coeff):
                output += ' + {}'.format(str(expr))
            elif cmh.is_minus_one(coeff):
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

    def Expressions(self):
        return self.__expressions

    def Coefficients(self):
        return self.__coefficients

    def Constant(self):
        return self.__constant


class IntVar(LinearExpr):
    """An integer variable.

  An IntVar is an object that can take on any integer value within defined
  ranges. Variables appear in constraint like:

      x + y >= 5
      AllDifferent([x, y, z])

  Solving a model is equivalent to finding, for each variable, a single value
  from the set of initial values (called the initial domain), such that the
  model is feasible, or optimal if you provided an objective function.
  """

    def __init__(self, model, domain, name):
        """See CpModel.NewIntVar below."""
        self.__negation = None
        # Python do not support multiple __init__ methods.
        # This method is only called from the CpModel class.
        # We hack the parameter to support the two cases:
        # case 1:
        #     model is a CpModelProto, domain is a Domain, and name is a string.
        # case 2:
        #     model is a CpModelProto, domain is an index (int), and name is None.
        if cmh.is_integral(domain) and name is None:
            self.__index = int(domain)
            self.__var = model.variables[domain]
        else:
            self.__index = len(model.variables)
            self.__var = model.variables.add()
            self.__var.domain.extend(domain.FlattenedIntervals())
            self.__var.name = name

    def Index(self):
        """Returns the index of the variable in the model."""
        return self.__index

    def Proto(self):
        """Returns the variable protobuf."""
        return self.__var

    def IsEqualTo(self, other):
        """Returns true if self == other in the python sense."""
        if not isinstance(other, IntVar):
            return False
        return self.Index() == other.Index()

    def __str__(self):
        if not self.__var.name:
            if len(self.__var.domain
                  ) == 2 and self.__var.domain[0] == self.__var.domain[1]:
                # Special case for constants.
                return str(self.__var.domain[0])
            else:
                return 'unnamed_var_%i' % self.__index
        return self.__var.name

    def __repr__(self):
        return '%s(%s)' % (self.__var.name, DisplayBounds(self.__var.domain))

    def Name(self):
        return self.__var.name

    def Not(self):
        """Returns the negation of a Boolean variable.

    This method implements the logical negation of a Boolean variable.
    It is only valid if the variable has a Boolean domain (0 or 1).

    Note that this method is nilpotent: `x.Not().Not() == x`.
    """

        for bound in self.__var.domain:
            if bound < 0 or bound > 1:
                raise TypeError(
                    'Cannot call Not on a non boolean variable: %s' % self)
        if self.__negation is None:
            self.__negation = _NotBooleanVariable(self)
        return self.__negation


class _NotBooleanVariable(LinearExpr):
    """Negation of a boolean variable."""

    def __init__(self, boolvar):
        self.__boolvar = boolvar

    def Index(self):
        return -self.__boolvar.Index() - 1

    def Not(self):
        return self.__boolvar

    def __str__(self):
        return 'not(%s)' % str(self.__boolvar)

    def __bool__(self):
        raise NotImplementedError(
            'Evaluating a literal as a Boolean value is not implemented.')


class BoundedLinearExpression(object):
    """Represents a linear constraint: `lb <= linear expression <= ub`.

  The only use of this class is to be added to the CpModel through
  `CpModel.Add(expression)`, as in:

      model.Add(x + 2 * y -1 >= z)
  """

    def __init__(self, expr, bounds):
        self.__expr = expr
        self.__bounds = bounds

    def __str__(self):
        if len(self.__bounds) == 2:
            lb = self.__bounds[0]
            ub = self.__bounds[1]
            if lb > INT_MIN and ub < INT_MAX:
                if lb == ub:
                    return str(self.__expr) + ' == ' + str(lb)
                else:
                    return str(lb) + ' <= ' + str(
                        self.__expr) + ' <= ' + str(ub)
            elif lb > INT_MIN:
                return str(self.__expr) + ' >= ' + str(lb)
            elif ub < INT_MAX:
                return str(self.__expr) + ' <= ' + str(ub)
            else:
                return 'True (unbounded expr ' + str(self.__expr) + ')'
        elif (len(self.__bounds) == 4 and self.__bounds[0] == INT_MIN and
              self.__bounds[1] + 2 == self.__bounds[2] and
              self.__bounds[3] == INT_MAX):
            return str(self.__expr) + ' != ' + str(self.__bounds[1] + 1)
        else:
            return str(self.__expr) + ' in [' + DisplayBounds(
                self.__bounds) + ']'

    def Expression(self):
        return self.__expr

    def Bounds(self):
        return self.__bounds

    def __bool__(self):
        coeffs_map, constant = self.__expr.GetIntegerVarValueMap()
        all_coeffs = set(coeffs_map.values())
        same_var = set([0])
        eq_bounds = [0, 0]
        different_vars = set([-1, 1])
        ne_bounds = [INT_MIN, -1, 1, INT_MAX]
        if (len(coeffs_map) == 1 and all_coeffs == same_var and
                constant == 0 and
            (self.__bounds == eq_bounds or self.__bounds == ne_bounds)):
            return self.__bounds == eq_bounds
        if (len(coeffs_map) == 2 and all_coeffs == different_vars and
                constant == 0 and
            (self.__bounds == eq_bounds or self.__bounds == ne_bounds)):
            return self.__bounds == ne_bounds

        raise NotImplementedError(
            f'Evaluating a BoundedLinearExpression \'{self}\' as a Boolean value'
            + ' is not supported.')


class Constraint(object):
    """Base class for constraints.

  Constraints are built by the CpModel through the Add<XXX> methods.
  Once created by the CpModel class, they are automatically added to the model.
  The purpose of this class is to allow specification of enforcement literals
  for this constraint.

      b = model.NewBoolVar('b')
      x = model.NewIntVar(0, 10, 'x')
      y = model.NewIntVar(0, 10, 'y')

      model.Add(x + 2 * y == 5).OnlyEnforceIf(b.Not())
  """

    def __init__(self, constraints):
        self.__index = len(constraints)
        self.__constraint = constraints.add()

    def OnlyEnforceIf(self, *boolvar):
        """Adds an enforcement literal to the constraint.

    This method adds one or more literals (that is, a boolean variable or its
    negation) as enforcement literals. The conjunction of all these literals
    determines whether the constraint is active or not. It acts as an
    implication, so if the conjunction is true, it implies that the constraint
    must be enforced. If it is false, then the constraint is ignored.

    BoolOr, BoolAnd, and linear constraints all support enforcement literals.

    Args:
      *boolvar: One or more Boolean literals.

    Returns:
      self.
    """
        for lit in ExpandGeneratorOrTuple(boolvar):
            if (isinstance(lit, bool) and
                    bool(lit)) or (cmh.is_integral(lit) and int(lit) == 1):
                # Always true. Do nothing.
                pass
            else:
                self.__constraint.enforcement_literal.append(lit.Index())
        return self

    def WithName(self, name):
        """Sets the name of the constraint."""
        if name:
            self.__constraint.name = name
        else:
            self.__constraint.ClearField('name')
        return self

    def Name(self):
        """Returns the name of the constraint."""
        return self.__constraint.name

    def Index(self):
        """Returns the index of the constraint in the model."""
        return self.__index

    def Proto(self):
        """Returns the constraint protobuf."""
        return self.__constraint


class IntervalVar(object):
    """Represents an Interval variable.

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
  """

    def __init__(self, model, start, size, end, is_present_index, name):
        self.__model = model
        # As with the IntVar::__init__ method, we hack the __init__ method to
        # support two use cases:
        #   case 1: called when creating a new interval variable.
        #      {start|size|end} are linear expressions, is_present_index is either
        #      None or the index of a Boolean literal. name is a string
        #   case 2: called when querying an existing interval variable.
        #      start_index is an int, all parameters after are None.
        if (size is None and end is None and is_present_index is None and
                name is None):
            self.__index = start
            self.__ct = model.constraints[start]
        else:
            self.__index = len(model.constraints)
            self.__ct = self.__model.constraints.add()
            self.__ct.interval.start.CopyFrom(start)
            self.__ct.interval.size.CopyFrom(size)
            self.__ct.interval.end.CopyFrom(end)
            if is_present_index is not None:
                self.__ct.enforcement_literal.append(is_present_index)
            if name:
                self.__ct.name = name

    def Index(self):
        """Returns the index of the interval constraint in the model."""
        return self.__index

    def Proto(self):
        """Returns the interval protobuf."""
        return self.__ct.interval

    def __str__(self):
        return self.__ct.name

    def __repr__(self):
        interval = self.__ct.interval
        if self.__ct.enforcement_literal:
            return '%s(start = %s, size = %s, end = %s, is_present = %s)' % (
                self.__ct.name, ShortExprName(self.__model, interval.start),
                ShortExprName(self.__model, interval.size),
                ShortExprName(self.__model, interval.end),
                ShortName(self.__model, self.__ct.enforcement_literal[0]))
        else:
            return '%s(start = %s, size = %s, end = %s)' % (
                self.__ct.name, ShortExprName(self.__model, interval.start),
                ShortExprName(self.__model, interval.size),
                ShortExprName(self.__model, interval.end))

    def Name(self):
        return self.__ct.name

    def StartExpr(self):
        return LinearExpr.RebuildFromLinearExpressionProto(
            self.__model, self.__ct.interval.start)

    def SizeExpr(self):
        return LinearExpr.RebuildFromLinearExpressionProto(
            self.__model, self.__ct.interval.size)

    def EndExpr(self):
        return LinearExpr.RebuildFromLinearExpressionProto(
            self.__model, self.__ct.interval.end)


def ObjectIsATrueLiteral(literal):
    """Checks if literal is either True, or a Boolean literals fixed to True."""
    if isinstance(literal, IntVar):
        proto = literal.Proto()
        return (len(proto.domain) == 2 and proto.domain[0] == 1 and
                proto.domain[1] == 1)
    if isinstance(literal, _NotBooleanVariable):
        proto = literal.Not().Proto()
        return (len(proto.domain) == 2 and proto.domain[0] == 0 and
                proto.domain[1] == 0)
    if cmh.is_integral(literal):
        return int(literal) == 1
    return False


def ObjectIsAFalseLiteral(literal):
    """Checks if literal is either False, or a Boolean literals fixed to False."""
    if isinstance(literal, IntVar):
        proto = literal.Proto()
        return (len(proto.domain) == 2 and proto.domain[0] == 0 and
                proto.domain[1] == 0)
    if isinstance(literal, _NotBooleanVariable):
        proto = literal.Not().Proto()
        return (len(proto.domain) == 2 and proto.domain[0] == 1 and
                proto.domain[1] == 1)
    if cmh.is_integral(literal):
        return int(literal) == 0
    return False


class CpModel(object):
    """Methods for building a CP model.

  Methods beginning with:

  * ```New``` create integer, boolean, or interval variables.
  * ```Add``` create new constraints and add them to the model.
  """

    def __init__(self):
        self.__model = cp_model_pb2.CpModelProto()
        self.__constant_map = {}

    # Naming.
    def Name(self):
        """Returns the name of the model."""
        return self.__model.name

    def SetName(self, name):
        """Sets the name of the model."""
        self.__model.name = name

    # Integer variable.

    def NewIntVar(self, lb, ub, name):
        """Create an integer variable with domain [lb, ub].

    The CP-SAT solver is limited to integer variables. If you have fractional
    values, scale them up so that they become integers; if you have strings,
    encode them as integers.

    Args:
      lb: Lower bound for the variable.
      ub: Upper bound for the variable.
      name: The name of the variable.

    Returns:
      a variable whose domain is [lb, ub].
    """

        return IntVar(self.__model, Domain(lb, ub), name)

    def NewIntVarFromDomain(self, domain, name):
        """Create an integer variable from a domain.

    A domain is a set of integers specified by a collection of intervals.
    For example, `model.NewIntVarFromDomain(cp_model.
         Domain.FromIntervals([[1, 2], [4, 6]]), 'x')`

    Args:
      domain: An instance of the Domain class.
      name: The name of the variable.

    Returns:
        a variable whose domain is the given domain.
    """
        return IntVar(self.__model, domain, name)

    def NewBoolVar(self, name):
        """Creates a 0-1 variable with the given name."""
        return IntVar(self.__model, Domain(0, 1), name)

    def NewConstant(self, value):
        """Declares a constant integer."""
        return IntVar(self.__model, self.GetOrMakeIndexFromConstant(value),
                      None)

    # Linear constraints.

    def AddLinearConstraint(self, linear_expr, lb, ub):
        """Adds the constraint: `lb <= linear_expr <= ub`."""
        return self.AddLinearExpressionInDomain(linear_expr, Domain(lb, ub))

    def AddLinearExpressionInDomain(self, linear_expr, domain):
        """Adds the constraint: `linear_expr` in `domain`."""
        if isinstance(linear_expr, LinearExpr):
            ct = Constraint(self.__model.constraints)
            model_ct = self.__model.constraints[ct.Index()]
            coeffs_map, constant = linear_expr.GetIntegerVarValueMap()
            for t in coeffs_map.items():
                if not isinstance(t[0], IntVar):
                    raise TypeError('Wrong argument' + str(t))
                c = cmh.assert_is_int64(t[1])
                model_ct.linear.vars.append(t[0].Index())
                model_ct.linear.coeffs.append(c)
            model_ct.linear.domain.extend([
                cmh.capped_subtraction(x, constant)
                for x in domain.FlattenedIntervals()
            ])
            return ct
        elif cmh.is_integral(linear_expr):
            if not domain.Contains(int(linear_expr)):
                return self.AddBoolOr([])  # Evaluate to false.
            # Nothing to do otherwise.
        else:
            raise TypeError(
                'Not supported: CpModel.AddLinearExpressionInDomain(' +
                str(linear_expr) + ' ' + str(domain) + ')')

    def Add(self, ct):
        """Adds a `BoundedLinearExpression` to the model.

    Args:
      ct: A [`BoundedLinearExpression`](#boundedlinearexpression).

    Returns:
      An instance of the `Constraint` class.
    """
        if isinstance(ct, BoundedLinearExpression):
            return self.AddLinearExpressionInDomain(
                ct.Expression(), Domain.FromFlatIntervals(ct.Bounds()))
        elif ct and isinstance(ct, bool):
            return self.AddBoolOr([True])
        elif not ct and isinstance(ct, bool):
            return self.AddBoolOr([])  # Evaluate to false.
        else:
            raise TypeError('Not supported: CpModel.Add(' + str(ct) + ')')

    # General Integer Constraints.

    def AddAllDifferent(self, *expressions):
        """Adds AllDifferent(expressions).

    This constraint forces all expressions to have different values.

    Args:
      *expressions: simple expressions of the form a * var + constant.

    Returns:
      An instance of the `Constraint` class.
    """
        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        expanded = ExpandGeneratorOrTuple(expressions)
        model_ct.all_diff.exprs.extend(
            [self.ParseLinearExpression(x) for x in expanded])
        return ct

    def AddElement(self, index, variables, target):
        """Adds the element constraint: `variables[index] == target`.

    Args:
      index: The index of the variable that's being constrained.
      variables: A list of variables.
      target: The value that the variable must be equal to.

    Returns:
      An instance of the `Constraint` class.
    """

        if not variables:
            raise ValueError('AddElement expects a non-empty variables array')

        if cmh.is_integral(index):
            return self.Add(list(variables)[int(index)] == target)

        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        model_ct.element.index = self.GetOrMakeIndex(index)
        model_ct.element.vars.extend(
            [self.GetOrMakeIndex(x) for x in variables])
        model_ct.element.target = self.GetOrMakeIndex(target)
        return ct

    def AddCircuit(self, arcs):
        """Adds Circuit(arcs).

    Adds a circuit constraint from a sparse list of arcs that encode the graph.

    A circuit is a unique Hamiltonian path in a subgraph of the total
    graph. In case a node 'i' is not in the path, then there must be a
    loop arc 'i -> i' associated with a true literal. Otherwise
    this constraint will fail.

    Args:
      arcs: a list of arcs. An arc is a tuple (source_node, destination_node,
        literal). The arc is selected in the circuit if the literal is true.
        Both source_node and destination_node must be integers between 0 and the
        number of nodes - 1.

    Returns:
      An instance of the `Constraint` class.

    Raises:
      ValueError: If the list of arcs is empty.
    """
        if not arcs:
            raise ValueError('AddCircuit expects a non-empty array of arcs')
        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        for arc in arcs:
            tail = cmh.assert_is_int32(arc[0])
            head = cmh.assert_is_int32(arc[1])
            lit = self.GetOrMakeBooleanIndex(arc[2])
            model_ct.circuit.tails.append(tail)
            model_ct.circuit.heads.append(head)
            model_ct.circuit.literals.append(lit)
        return ct

    def AddMultipleCircuit(self, arcs):
        """Adds a multiple circuit constraint, aka the "VRP" constraint.

    The direct graph where arc #i (from tails[i] to head[i]) is present iff
    literals[i] is true must satisfy this set of properties:
    - #incoming arcs == 1 except for node 0.
    - #outgoing arcs == 1 except for node 0.
    - for node zero, #incoming arcs == #outgoing arcs.
    - There are no duplicate arcs.
    - Self-arcs are allowed except for node 0.
    - There is no cycle in this graph, except through node 0.

    Args:
      arcs: a list of arcs. An arc is a tuple (source_node, destination_node,
        literal). The arc is selected in the circuit if the literal is true.
        Both source_node and destination_node must be integers between 0 and the
        number of nodes - 1.

    Returns:
      An instance of the `Constraint` class.

    Raises:
      ValueError: If the list of arcs is empty.
    """
        if not arcs:
            raise ValueError(
                'AddMultipleCircuit expects a non-empty array of arcs')
        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        for arc in arcs:
            tail = cmh.assert_is_int32(arc[0])
            head = cmh.assert_is_int32(arc[1])
            lit = self.GetOrMakeBooleanIndex(arc[2])
            model_ct.routes.tails.append(tail)
            model_ct.routes.heads.append(head)
            model_ct.routes.literals.append(lit)
        return ct

    def AddAllowedAssignments(self, variables, tuples_list):
        """Adds AllowedAssignments(variables, tuples_list).

    An AllowedAssignments constraint is a constraint on an array of variables,
    which requires that when all variables are assigned values, the resulting
    array equals one of the  tuples in `tuple_list`.

    Args:
      variables: A list of variables.
      tuples_list: A list of admissible tuples. Each tuple must have the same
        length as the variables, and the ith value of a tuple corresponds to the
        ith variable.

    Returns:
      An instance of the `Constraint` class.

    Raises:
      TypeError: If a tuple does not have the same size as the list of
          variables.
      ValueError: If the array of variables is empty.
    """

        if not variables:
            raise ValueError(
                'AddAllowedAssignments expects a non-empty variables '
                'array')

        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        model_ct.table.vars.extend([self.GetOrMakeIndex(x) for x in variables])
        arity = len(variables)
        for t in tuples_list:
            if len(t) != arity:
                raise TypeError('Tuple ' + str(t) + ' has the wrong arity')
            ar = []
            for v in t:
                ar.append(cmh.assert_is_int64(v))
            model_ct.table.values.extend(ar)
        return ct

    def AddForbiddenAssignments(self, variables, tuples_list):
        """Adds AddForbiddenAssignments(variables, [tuples_list]).

    A ForbiddenAssignments constraint is a constraint on an array of variables
    where the list of impossible combinations is provided in the tuples list.

    Args:
      variables: A list of variables.
      tuples_list: A list of forbidden tuples. Each tuple must have the same
        length as the variables, and the *i*th value of a tuple corresponds to
        the *i*th variable.

    Returns:
      An instance of the `Constraint` class.

    Raises:
      TypeError: If a tuple does not have the same size as the list of
                 variables.
      ValueError: If the array of variables is empty.
    """

        if not variables:
            raise ValueError(
                'AddForbiddenAssignments expects a non-empty variables '
                'array')

        index = len(self.__model.constraints)
        ct = self.AddAllowedAssignments(variables, tuples_list)
        self.__model.constraints[index].table.negated = True
        return ct

    def AddAutomaton(self, transition_variables, starting_state, final_states,
                     transition_triples):
        """Adds an automaton constraint.

    An automaton constraint takes a list of variables (of size *n*), an initial
    state, a set of final states, and a set of transitions. A transition is a
    triplet (*tail*, *transition*, *head*), where *tail* and *head* are states,
    and *transition* is the label of an arc from *head* to *tail*,
    corresponding to the value of one variable in the list of variables.

    This automaton will be unrolled into a flow with *n* + 1 phases. Each phase
    contains the possible states of the automaton. The first state contains the
    initial state. The last phase contains the final states.

    Between two consecutive phases *i* and *i* + 1, the automaton creates a set
    of arcs. For each transition (*tail*, *transition*, *head*), it will add
    an arc from the state *tail* of phase *i* and the state *head* of phase
    *i* + 1. This arc is labeled by the value *transition* of the variables
    `variables[i]`. That is, this arc can only be selected if `variables[i]`
    is assigned the value *transition*.

    A feasible solution of this constraint is an assignment of variables such
    that, starting from the initial state in phase 0, there is a path labeled by
    the values of the variables that ends in one of the final states in the
    final phase.

    Args:
      transition_variables: A non-empty list of variables whose values
        correspond to the labels of the arcs traversed by the automaton.
      starting_state: The initial state of the automaton.
      final_states: A non-empty list of admissible final states.
      transition_triples: A list of transitions for the automaton, in the
        following format (current_state, variable_value, next_state).

    Returns:
      An instance of the `Constraint` class.

    Raises:
      ValueError: if `transition_variables`, `final_states`, or
        `transition_triples` are empty.
    """

        if not transition_variables:
            raise ValueError(
                'AddAutomaton expects a non-empty transition_variables '
                'array')
        if not final_states:
            raise ValueError('AddAutomaton expects some final states')

        if not transition_triples:
            raise ValueError('AddAutomaton expects some transition triples')

        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        model_ct.automaton.vars.extend(
            [self.GetOrMakeIndex(x) for x in transition_variables])
        starting_state = cmh.assert_is_int64(starting_state)
        model_ct.automaton.starting_state = starting_state
        for v in final_states:
            v = cmh.assert_is_int64(v)
            model_ct.automaton.final_states.append(v)
        for t in transition_triples:
            if len(t) != 3:
                raise TypeError('Tuple ' + str(t) +
                                ' has the wrong arity (!= 3)')
            tail = cmh.assert_is_int64(t[0])
            label = cmh.assert_is_int64(t[1])
            head = cmh.assert_is_int64(t[2])
            model_ct.automaton.transition_tail.append(tail)
            model_ct.automaton.transition_label.append(label)
            model_ct.automaton.transition_head.append(head)
        return ct

    def AddInverse(self, variables, inverse_variables):
        """Adds Inverse(variables, inverse_variables).

    An inverse constraint enforces that if `variables[i]` is assigned a value
    `j`, then `inverse_variables[j]` is assigned a value `i`. And vice versa.

    Args:
      variables: An array of integer variables.
      inverse_variables: An array of integer variables.

    Returns:
      An instance of the `Constraint` class.

    Raises:
      TypeError: if variables and inverse_variables have different lengths, or
          if they are empty.
    """

        if not variables or not inverse_variables:
            raise TypeError(
                'The Inverse constraint does not accept empty arrays')
        if len(variables) != len(inverse_variables):
            raise TypeError(
                'In the inverse constraint, the two array variables and'
                ' inverse_variables must have the same length.')
        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        model_ct.inverse.f_direct.extend(
            [self.GetOrMakeIndex(x) for x in variables])
        model_ct.inverse.f_inverse.extend(
            [self.GetOrMakeIndex(x) for x in inverse_variables])
        return ct

    def AddReservoirConstraint(self, times, level_changes, min_level,
                               max_level):
        """Adds Reservoir(times, level_changes, min_level, max_level).

    Maintains a reservoir level within bounds. The water level starts at 0, and
    at any time, it must be between min_level and max_level.

    If the affine expression `times[i]` is assigned a value t, then the current
    level changes by `level_changes[i]`, which is constant, at time t.

     Note that min level must be <= 0, and the max level must be >= 0. Please
     use fixed level_changes to simulate initial state.

     Therefore, at any time:
         sum(level_changes[i] if times[i] <= t) in [min_level, max_level]

    Args:
      times: A list of affine expressions which specify the time of the filling
        or emptying the reservoir.
      level_changes: A list of integer values that specifies the amount of the
        emptying or filling.
      min_level: At any time, the level of the reservoir must be greater or
        equal than the min level.
      max_level: At any time, the level of the reservoir must be less or equal
        than the max level.

    Returns:
      An instance of the `Constraint` class.

    Raises:
      ValueError: if max_level < min_level.

      ValueError: if max_level < 0.

      ValueError: if min_level > 0
    """

        if max_level < min_level:
            return ValueError(
                'Reservoir constraint must have a max_level >= min_level')

        if max_level < 0:
            return ValueError('Reservoir constraint must have a max_level >= 0')

        if min_level > 0:
            return ValueError('Reservoir constraint must have a min_level <= 0')

        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        model_ct.reservoir.time_exprs.extend(
            [self.ParseLinearExpression(x) for x in times])
        model_ct.reservoir.level_changes.extend(
            [self.ParseLinearExpression(x) for x in level_changes])
        model_ct.reservoir.min_level = min_level
        model_ct.reservoir.max_level = max_level
        return ct

    def AddReservoirConstraintWithActive(self, times, level_changes, actives,
                                         min_level, max_level):
        """Adds Reservoir(times, level_changes, actives, min_level, max_level).

    Maintains a reservoir level within bounds. The water level starts at 0, and
    at any time, it must be between min_level and max_level.

    If the variable `times[i]` is assigned a value t, and `actives[i]` is
    `True`, then the current level changes by `level_changes[i]`, which is
    constant,
    at time t.

     Note that min level must be <= 0, and the max level must be >= 0. Please
     use fixed level_changes to simulate initial state.

     Therefore, at any time:
         sum(level_changes[i] * actives[i] if times[i] <= t) in [min_level,
         max_level]


    The array of boolean variables 'actives', if defined, indicates which
    actions are actually performed.

    Args:
      times: A list of affine expressions which specify the time of the filling
        or emptying the reservoir.
      level_changes: A list of integer values that specifies the amount of the
        emptying or filling.
      actives: a list of boolean variables. They indicates if the
        emptying/refilling events actually take place.
      min_level: At any time, the level of the reservoir must be greater or
        equal than the min level.
      max_level: At any time, the level of the reservoir must be less or equal
        than the max level.

    Returns:
      An instance of the `Constraint` class.

    Raises:
      ValueError: if max_level < min_level.

      ValueError: if max_level < 0.

      ValueError: if min_level > 0
    """

        if max_level < min_level:
            return ValueError(
                'Reservoir constraint must have a max_level >= min_level')

        if max_level < 0:
            return ValueError('Reservoir constraint must have a max_level >= 0')

        if min_level > 0:
            return ValueError('Reservoir constraint must have a min_level <= 0')

        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        model_ct.reservoir.time_exprs.extend(
            [self.ParseLinearExpression(x) for x in times])
        model_ct.reservoir.level_changes.extend(
            [self.ParseLinearExpression(x) for x in level_changes])
        model_ct.reservoir.active_literals.extend(
            [self.GetOrMakeIndex(x) for x in actives])
        model_ct.reservoir.min_level = min_level
        model_ct.reservoir.max_level = max_level
        return ct

    def AddMapDomain(self, var, bool_var_array, offset=0):
        """Adds `var == i + offset <=> bool_var_array[i] == true for all i`."""

        for i, bool_var in enumerate(bool_var_array):
            b_index = bool_var.Index()
            var_index = var.Index()
            model_ct = self.__model.constraints.add()
            model_ct.linear.vars.append(var_index)
            model_ct.linear.coeffs.append(1)
            model_ct.linear.domain.extend([offset + i, offset + i])
            model_ct.enforcement_literal.append(b_index)

            model_ct = self.__model.constraints.add()
            model_ct.linear.vars.append(var_index)
            model_ct.linear.coeffs.append(1)
            model_ct.enforcement_literal.append(-b_index - 1)
            if offset + i - 1 >= INT_MIN:
                model_ct.linear.domain.extend([INT_MIN, offset + i - 1])
            if offset + i + 1 <= INT_MAX:
                model_ct.linear.domain.extend([offset + i + 1, INT_MAX])

    def AddImplication(self, a, b):
        """Adds `a => b` (`a` implies `b`)."""
        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        model_ct.bool_or.literals.append(self.GetOrMakeBooleanIndex(b))
        model_ct.enforcement_literal.append(self.GetOrMakeBooleanIndex(a))
        return ct

    def AddBoolOr(self, *literals):
        """Adds `Or(literals) == true`: Sum(literals) >= 1."""
        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        model_ct.bool_or.literals.extend([
            self.GetOrMakeBooleanIndex(x)
            for x in ExpandGeneratorOrTuple(literals)
        ])
        return ct

    def AddAtLeastOne(self, *literals):
        """Same as `AddBoolOr`: `Sum(literals) >= 1`."""
        return self.AddBoolOr(*literals)

    def AddAtMostOne(self, *literals):
        """Adds `AtMostOne(literals)`: `Sum(literals) <= 1`."""
        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        model_ct.at_most_one.literals.extend([
            self.GetOrMakeBooleanIndex(x)
            for x in ExpandGeneratorOrTuple(literals)
        ])
        return ct

    def AddExactlyOne(self, *literals):
        """Adds `ExactlyOne(literals)`: `Sum(literals) == 1`."""
        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        model_ct.exactly_one.literals.extend([
            self.GetOrMakeBooleanIndex(x)
            for x in ExpandGeneratorOrTuple(literals)
        ])
        return ct

    def AddBoolAnd(self, *literals):
        """Adds `And(literals) == true`."""
        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        model_ct.bool_and.literals.extend([
            self.GetOrMakeBooleanIndex(x)
            for x in ExpandGeneratorOrTuple(literals)
        ])
        return ct

    def AddBoolXOr(self, *literals):
        """Adds `XOr(literals) == true`.

    In contrast to AddBoolOr and AddBoolAnd, it does not support
        .OnlyEnforceIf().

    Args:
      *literals: the list of literals in the constraint.

    Returns:
      An `Constraint` object.
    """
        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        model_ct.bool_xor.literals.extend([
            self.GetOrMakeBooleanIndex(x)
            for x in ExpandGeneratorOrTuple(literals)
        ])
        return ct

    def AddMinEquality(self, target, exprs):
        """Adds `target == Min(exprs)`."""
        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        model_ct.lin_max.exprs.extend(
            [self.ParseLinearExpression(x, True) for x in exprs])
        model_ct.lin_max.target.CopyFrom(
            self.ParseLinearExpression(target, True))
        return ct

    def AddMaxEquality(self, target, exprs):
        """Adds `target == Max(exprs)`."""
        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        model_ct.lin_max.exprs.extend(
            [self.ParseLinearExpression(x) for x in exprs])
        model_ct.lin_max.target.CopyFrom(self.ParseLinearExpression(target))
        return ct

    def AddDivisionEquality(self, target, num, denom):
        """Adds `target == num // denom` (integer division rounded towards 0)."""
        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        model_ct.int_div.exprs.append(self.ParseLinearExpression(num))
        model_ct.int_div.exprs.append(self.ParseLinearExpression(denom))
        model_ct.int_div.target.CopyFrom(self.ParseLinearExpression(target))
        return ct

    def AddAbsEquality(self, target, expr):
        """Adds `target == Abs(var)`."""
        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        model_ct.lin_max.exprs.append(self.ParseLinearExpression(expr))
        model_ct.lin_max.exprs.append(self.ParseLinearExpression(expr, True))
        model_ct.lin_max.target.CopyFrom(self.ParseLinearExpression(target))
        return ct

    def AddModuloEquality(self, target, var, mod):
        """Adds `target = var % mod`."""
        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        model_ct.int_mod.exprs.append(self.ParseLinearExpression(var))
        model_ct.int_mod.exprs.append(self.ParseLinearExpression(mod))
        model_ct.int_mod.target.CopyFrom(self.ParseLinearExpression(target))
        return ct

    def AddMultiplicationEquality(self, target, *expressions):
        """Adds `target == expressions[0] * .. * expressions[n]`."""
        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        model_ct.int_prod.exprs.extend([
            self.ParseLinearExpression(expr)
            for expr in ExpandGeneratorOrTuple(expressions)
        ])
        model_ct.int_prod.target.CopyFrom(self.ParseLinearExpression(target))
        return ct

    # Scheduling support

    def NewIntervalVar(self, start, size, end, name):
        """Creates an interval variable from start, size, and end.

    An interval variable is a constraint, that is itself used in other
    constraints like NoOverlap.

    Internally, it ensures that `start + size == end`.

    Args:
      start: The start of the interval. It can be an affine or constant
        expression.
      size: The size of the interval. It can be an affine or constant
        expression.
      end: The end of the interval. It can be an affine or constant expression.
      name: The name of the interval variable.

    Returns:
      An `IntervalVar` object.
    """

        self.Add(start + size == end)

        start_expr = self.ParseLinearExpression(start)
        size_expr = self.ParseLinearExpression(size)
        end_expr = self.ParseLinearExpression(end)
        if len(start_expr.vars) > 1:
            raise TypeError(
                'cp_model.NewIntervalVar: start must be affine or constant.')
        if len(size_expr.vars) > 1:
            raise TypeError(
                'cp_model.NewIntervalVar: size must be affine or constant.')
        if len(end_expr.vars) > 1:
            raise TypeError(
                'cp_model.NewIntervalVar: end must be affine or constant.')
        return IntervalVar(self.__model, start_expr, size_expr, end_expr, None,
                           name)

    def NewFixedSizeIntervalVar(self, start, size, name):
        """Creates an interval variable from start, and a fixed size.

    An interval variable is a constraint, that is itself used in other
    constraints like NoOverlap.

    Args:
      start: The start of the interval. It can be an affine or constant
        expression.
      size: The size of the interval. It must be an integer value.
      name: The name of the interval variable.

    Returns:
      An `IntervalVar` object.
    """
        size = cmh.assert_is_int64(size)
        start_expr = self.ParseLinearExpression(start)
        size_expr = self.ParseLinearExpression(size)
        end_expr = self.ParseLinearExpression(start + size)
        if len(start_expr.vars) > 1:
            raise TypeError(
                'cp_model.NewIntervalVar: start must be affine or constant.')
        return IntervalVar(self.__model, start_expr, size_expr, end_expr, None,
                           name)

    def NewOptionalIntervalVar(self, start, size, end, is_present, name):
        """Creates an optional interval var from start, size, end, and is_present.

    An optional interval variable is a constraint, that is itself used in other
    constraints like NoOverlap. This constraint is protected by an is_present
    literal that indicates if it is active or not.

    Internally, it ensures that `is_present` implies `start + size == end`.

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
      An `IntervalVar` object.
    """

        # Add the linear constraint.
        self.Add(start + size == end).OnlyEnforceIf(is_present)

        # Creates the IntervalConstraintProto object.
        is_present_index = self.GetOrMakeBooleanIndex(is_present)
        start_expr = self.ParseLinearExpression(start)
        size_expr = self.ParseLinearExpression(size)
        end_expr = self.ParseLinearExpression(end)
        if len(start_expr.vars) > 1:
            raise TypeError(
                'cp_model.NewIntervalVar: start must be affine or constant.')
        if len(size_expr.vars) > 1:
            raise TypeError(
                'cp_model.NewIntervalVar: size must be affine or constant.')
        if len(end_expr.vars) > 1:
            raise TypeError(
                'cp_model.NewIntervalVar: end must be affine or constant.')
        return IntervalVar(self.__model, start_expr, size_expr, end_expr,
                           is_present_index, name)

    def NewOptionalFixedSizeIntervalVar(self, start, size, is_present, name):
        """Creates an interval variable from start, and a fixed size.

    An interval variable is a constraint, that is itself used in other
    constraints like NoOverlap.

    Args:
      start: The start of the interval. It can be an affine or constant
        expression.
      size: The size of the interval. It must be an integer value.
      is_present: A literal that indicates if the interval is active or not. A
        inactive interval is simply ignored by all constraints.
      name: The name of the interval variable.

    Returns:
      An `IntervalVar` object.
    """
        size = cmh.assert_is_int64(size)
        start_expr = self.ParseLinearExpression(start)
        size_expr = self.ParseLinearExpression(size)
        end_expr = self.ParseLinearExpression(start + size)
        if len(start_expr.vars) > 1:
            raise TypeError(
                'cp_model.NewIntervalVar: start must be affine or constant.')
        is_present_index = self.GetOrMakeBooleanIndex(is_present)
        return IntervalVar(self.__model, start_expr, size_expr, end_expr,
                           is_present_index, name)

    def AddNoOverlap(self, interval_vars):
        """Adds NoOverlap(interval_vars).

    A NoOverlap constraint ensures that all present intervals do not overlap
    in time.

    Args:
      interval_vars: The list of interval variables to constrain.

    Returns:
      An instance of the `Constraint` class.
    """
        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        model_ct.no_overlap.intervals.extend(
            [self.GetIntervalIndex(x) for x in interval_vars])
        return ct

    def AddNoOverlap2D(self, x_intervals, y_intervals):
        """Adds NoOverlap2D(x_intervals, y_intervals).

    A NoOverlap2D constraint ensures that all present rectangles do not overlap
    on a plane. Each rectangle is aligned with the X and Y axis, and is defined
    by two intervals which represent its projection onto the X and Y axis.

    Furthermore, one box is optional if at least one of the x or y interval is
    optional.

    Args:
      x_intervals: The X coordinates of the rectangles.
      y_intervals: The Y coordinates of the rectangles.

    Returns:
      An instance of the `Constraint` class.
    """
        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        model_ct.no_overlap_2d.x_intervals.extend(
            [self.GetIntervalIndex(x) for x in x_intervals])
        model_ct.no_overlap_2d.y_intervals.extend(
            [self.GetIntervalIndex(x) for x in y_intervals])
        return ct

    def AddCumulative(self, intervals, demands, capacity):
        """Adds Cumulative(intervals, demands, capacity).

    This constraint enforces that:

        for all t:
          sum(demands[i]
            if (start(intervals[i]) <= t < end(intervals[i])) and
            (intervals[i] is present)) <= capacity

    Args:
      intervals: The list of intervals.
      demands: The list of demands for each interval. Each demand must be >= 0.
        Each demand can be an integer value, or an integer variable.
      capacity: The maximum capacity of the cumulative constraint. It must be a
        positive integer value or variable.

    Returns:
      An instance of the `Constraint` class.
    """
        ct = Constraint(self.__model.constraints)
        model_ct = self.__model.constraints[ct.Index()]
        model_ct.cumulative.intervals.extend(
            [self.GetIntervalIndex(x) for x in intervals])
        for d in demands:
            model_ct.cumulative.demands.append(self.ParseLinearExpression(d))
        model_ct.cumulative.capacity.CopyFrom(
            self.ParseLinearExpression(capacity))
        return ct

    # Support for deep copy.
    def CopyFrom(self, other_model):
        """Reset the model, and creates a new one from a CpModelProto instance."""
        self.__model.CopyFrom(other_model.Proto())

        # Rebuild constant map.
        self.__constant_map.clear()
        for i, var in enumerate(self.__model.variables):
            if len(var.domain) == 2 and var.domain[0] == var.domain[1]:
                self.__constant_map[var.domain[0]] = i

    def GetBoolVarFromProtoIndex(self, index):
        """Returns an already created Boolean variable from its index."""
        if index < 0 or index >= len(self.__model.variables):
            raise ValueError(
                f'GetBoolVarFromProtoIndex: out of bound index {index}')
        var = self.__model.variables[index]
        if len(var.domain) != 2 or var.domain[0] < 0 or var.domain[1] > 1:
            raise ValueError(
                f'GetBoolVarFromProtoIndex: index {index} does not reference' +
                ' a Boolean variable')

        return IntVar(self.__model, index, None)

    def GetIntVarFromProtoIndex(self, index):
        """Returns an already created integer variable from its index."""
        if index < 0 or index >= len(self.__model.variables):
            raise ValueError(
                f'GetIntVarFromProtoIndex: out of bound index {index}')
        return IntVar(self.__model, index, None)

    def GetIntervalVarFromProtoIndex(self, index):
        """Returns an already created interval variable from its index."""
        if index < 0 or index >= len(self.__model.constraints):
            raise ValueError(
                f'GetIntervalVarFromProtoIndex: out of bound index {index}')
        ct = self.__model.constraints[index]
        if not ct.HasField('interval'):
            raise ValueError(
                f'GetIntervalVarFromProtoIndex: index {index} does not reference an'
                + ' interval variable')

        return IntervalVar(self.__model, index, None, None, None, None)

    # Helpers.

    def __str__(self):
        return str(self.__model)

    def Proto(self):
        """Returns the underlying CpModelProto."""
        return self.__model

    def Negated(self, index):
        return -index - 1

    def GetOrMakeIndex(self, arg):
        """Returns the index of a variable, its negation, or a number."""
        if isinstance(arg, IntVar):
            return arg.Index()
        elif (isinstance(arg, _ProductCst) and
              isinstance(arg.Expression(), IntVar) and arg.Coefficient() == -1):
            return -arg.Expression().Index() - 1
        elif cmh.is_integral(arg):
            arg = cmh.assert_is_int64(arg)
            return self.GetOrMakeIndexFromConstant(arg)
        else:
            raise TypeError('NotSupported: model.GetOrMakeIndex(' + str(arg) +
                            ')')

    def GetOrMakeBooleanIndex(self, arg):
        """Returns an index from a boolean expression."""
        if isinstance(arg, IntVar):
            self.AssertIsBooleanVariable(arg)
            return arg.Index()
        elif isinstance(arg, _NotBooleanVariable):
            self.AssertIsBooleanVariable(arg.Not())
            return arg.Index()
        elif cmh.is_integral(arg):
            cmh.assert_is_boolean(arg)
            return self.GetOrMakeIndexFromConstant(int(arg))
        else:
            raise TypeError('NotSupported: model.GetOrMakeBooleanIndex(' +
                            str(arg) + ')')

    def GetIntervalIndex(self, arg):
        if not isinstance(arg, IntervalVar):
            raise TypeError('NotSupported: model.GetIntervalIndex(%s)' % arg)
        return arg.Index()

    def GetOrMakeIndexFromConstant(self, value):
        if value in self.__constant_map:
            return self.__constant_map[value]
        index = len(self.__model.variables)
        var = self.__model.variables.add()
        var.domain.extend([value, value])
        self.__constant_map[value] = index
        return index

    def VarIndexToVarProto(self, var_index):
        if var_index >= 0:
            return self.__model.variables[var_index]
        else:
            return self.__model.variables[-var_index - 1]

    def ParseLinearExpression(self, linear_expr, negate=False):
        """Returns a LinearExpressionProto built from a LinearExpr instance."""
        result = cp_model_pb2.LinearExpressionProto()
        mult = -1 if negate else 1
        if cmh.is_integral(linear_expr):
            result.offset = int(linear_expr) * mult
            return result

        if isinstance(linear_expr, IntVar):
            result.vars.append(self.GetOrMakeIndex(linear_expr))
            result.coeffs.append(mult)
            return result

        coeffs_map, constant = linear_expr.GetIntegerVarValueMap()
        result.offset = constant * mult
        for t in coeffs_map.items():
            if not isinstance(t[0], IntVar):
                raise TypeError('Wrong argument' + str(t))
            c = cmh.assert_is_int64(t[1])
            result.vars.append(t[0].Index())
            result.coeffs.append(c * mult)
        return result

    def _SetObjective(self, obj, minimize):
        """Sets the objective of the model."""
        self.ClearObjective()
        if isinstance(obj, IntVar):
            self.__model.objective.coeffs.append(1)
            self.__model.objective.offset = 0
            if minimize:
                self.__model.objective.vars.append(obj.Index())
                self.__model.objective.scaling_factor = 1
            else:
                self.__model.objective.vars.append(self.Negated(obj.Index()))
                self.__model.objective.scaling_factor = -1
        elif isinstance(obj, LinearExpr):
            coeffs_map, constant, is_integer = obj.GetFloatVarValueMap()
            if is_integer:
                if minimize:
                    self.__model.objective.scaling_factor = 1
                    self.__model.objective.offset = constant
                else:
                    self.__model.objective.scaling_factor = -1
                    self.__model.objective.offset = -constant
                for v, c, in coeffs_map.items():
                    self.__model.objective.coeffs.append(c)
                    if minimize:
                        self.__model.objective.vars.append(v.Index())
                    else:
                        self.__model.objective.vars.append(
                            self.Negated(v.Index()))
            else:
                self.__model.floating_point_objective.maximize = not minimize
                self.__model.floating_point_objective.offset = constant
                for v, c, in coeffs_map.items():
                    self.__model.floating_point_objective.coeffs.append(c)
                    self.__model.floating_point_objective.vars.append(v.Index())
        elif cmh.is_integral(obj):
            self.__model.objective.offset = int(obj)
            self.__model.objective.scaling_factor = 1
        else:
            raise TypeError('TypeError: ' + str(obj) +
                            ' is not a valid objective')

    def Minimize(self, obj):
        """Sets the objective of the model to minimize(obj)."""
        self._SetObjective(obj, minimize=True)

    def Maximize(self, obj):
        """Sets the objective of the model to maximize(obj)."""
        self._SetObjective(obj, minimize=False)

    def HasObjective(self):
        return (self.__model.HasField('objective') or
                self.__model.HasField('floating_point_objective'))

    def ClearObjective(self):
        self.__model.ClearField('objective')
        self.__model.ClearField('floating_point_objective')

    def AddDecisionStrategy(self, variables, var_strategy, domain_strategy):
        """Adds a search strategy to the model.

    Args:
      variables: a list of variables this strategy will assign.
      var_strategy: heuristic to choose the next variable to assign.
      domain_strategy: heuristic to reduce the domain of the selected variable.
        Currently, this is advanced code: the union of all strategies added to
        the model must be complete, i.e. instantiates all variables. Otherwise,
        Solve() will fail.
    """

        strategy = self.__model.search_strategy.add()
        for v in variables:
            strategy.variables.append(v.Index())
        strategy.variable_selection_strategy = var_strategy
        strategy.domain_reduction_strategy = domain_strategy

    def ModelStats(self):
        """Returns a string containing some model statistics."""
        return swig_helper.CpSatHelper.SerializedModelStats(
            self.__model.SerializeToString())

    def Validate(self):
        """Returns a string indicating that the model is invalid."""
        return swig_helper.CpSatHelper.SerializedValidateModel(
            self.__model.SerializeToString())

    def ExportToFile(self, file):
        """Write the model as a protocol buffer to 'file'.

    Args:
      file: file to write the model to. If the filename ends with 'txt', the
        model will be written as a text file, otherwise, the binary format will
        be used.

    Returns:
      True if the model was correctly written.
    """
        return swig_helper.CpSatHelper.SerializedWriteModelToFile(
            self.__model.SerializeToString(), file)

    def AssertIsBooleanVariable(self, x):
        if isinstance(x, IntVar):
            var = self.__model.variables[x.Index()]
            if len(var.domain) != 2 or var.domain[0] < 0 or var.domain[1] > 1:
                raise TypeError('TypeError: ' + str(x) +
                                ' is not a boolean variable')
        elif not isinstance(x, _NotBooleanVariable):
            raise TypeError('TypeError: ' + str(x) +
                            ' is not a boolean variable')

    def AddHint(self, var, value):
        """Adds 'var == value' as a hint to the solver."""
        self.__model.solution_hint.vars.append(self.GetOrMakeIndex(var))
        self.__model.solution_hint.values.append(value)

    def ClearHints(self):
        """Remove any solution hint from the model."""
        self.__model.ClearField('solution_hint')

    def AddAssumption(self, lit):
        """Add the literal 'lit' to the model as assumptions."""
        self.__model.assumptions.append(self.GetOrMakeBooleanIndex(lit))

    def AddAssumptions(self, literals):
        """Add the literals to the model as assumptions."""
        for lit in literals:
            self.AddAssumption(lit)

    def ClearAssumptions(self):
        """Remove all assumptions from the model."""
        self.__model.ClearField('assumptions')


def ExpandGeneratorOrTuple(args):
    if hasattr(args, '__len__'):  # Tuple
        if len(args) != 1:
            return args
        if cmh.is_a_number(args[0]) or isinstance(args[0], LinearExpr):
            return args
    # Generator
    return args[0]


def EvaluateLinearExpr(expression, solution):
    """Evaluate a linear expression against a solution."""
    if cmh.is_integral(expression):
        return int(expression)
    if not isinstance(expression, LinearExpr):
        raise TypeError('Cannot interpret %s as a linear expression.' %
                        expression)

    value = 0
    to_process = [(expression, 1)]
    while to_process:
        expr, coeff = to_process.pop()
        if cmh.is_integral(expr):
            value += int(expr) * coeff
        elif isinstance(expr, _ProductCst):
            to_process.append((expr.Expression(), coeff * expr.Coefficient()))
        elif isinstance(expr, _Sum):
            to_process.append((expr.Left(), coeff))
            to_process.append((expr.Right(), coeff))
        elif isinstance(expr, _SumArray):
            for e in expr.Expressions():
                to_process.append((e, coeff))
            value += expr.Constant() * coeff
        elif isinstance(expr, _WeightedSum):
            for e, c in zip(expr.Expressions(), expr.Coefficients()):
                to_process.append((e, coeff * c))
            value += expr.Constant() * coeff
        elif isinstance(expr, IntVar):
            value += coeff * solution.solution[expr.Index()]
        elif isinstance(expr, _NotBooleanVariable):
            value += coeff * (1 - solution.solution[expr.Not().Index()])
        else:
            raise TypeError(f'Cannot interpret {expr} as a linear expression.')

    return value


def EvaluateBooleanExpression(literal, solution):
    """Evaluate a boolean expression against a solution."""
    if cmh.is_integral(literal):
        return bool(literal)
    elif isinstance(literal, IntVar) or isinstance(literal,
                                                   _NotBooleanVariable):
        index = literal.Index()
        if index >= 0:
            return bool(solution.solution[index])
        else:
            return not solution.solution[-index - 1]
    else:
        raise TypeError(f'Cannot interpret {literal} as a boolean expression.')


class CpSolver(object):
    """Main solver class.

  The purpose of this class is to search for a solution to the model provided
  to the Solve() method.

  Once Solve() is called, this class allows inspecting the solution found
  with the Value() and BooleanValue() methods, as well as general statistics
  about the solve procedure.
  """

    def __init__(self):
        self.__solution: Optional[cp_model_pb2.CpSolverResponse] = None
        self.parameters = sat_parameters_pb2.SatParameters()
        self.log_callback = None
        self.__solve_wrapper: swig_helper.SolveWrapper = None
        self.__lock = threading.Lock()

    def Solve(self, model, solution_callback=None):
        """Solves a problem and passes each solution to the callback if not null."""
        with self.__lock:
            solve_wrapper = swig_helper.SolveWrapper()

        swig_helper.SolveWrapper.SetSerializedParameters(
            self.parameters.SerializeToString(), solve_wrapper)
        if solution_callback is not None:
            solve_wrapper.AddSolutionCallback(solution_callback)

        if self.log_callback is not None:
            solve_wrapper.AddLogCallback(self.log_callback)

        self.__solution = cp_model_pb2.CpSolverResponse.FromString(
            swig_helper.SolveWrapper.SerializedSolve(
                model.Proto().SerializeToString(), solve_wrapper))

        if solution_callback is not None:
            solve_wrapper.ClearSolutionCallback(solution_callback)

        with self.__lock:
            self.__solve_wrapper = None

        return self.__solution.status

    def SolveWithSolutionCallback(self, model, callback):
        """DEPRECATED Use Solve() with the callback argument."""
        warnings.warn(
            'SolveWithSolutionCallback is deprecated; use Solve() with' +
            'the callback argument.', DeprecationWarning)
        return self.Solve(model, callback)

    def SearchForAllSolutions(self, model, callback):
        """DEPRECATED Use Solve() with the right parameter.

    Search for all solutions of a satisfiability problem.

    This method searches for all feasible solutions of a given model.
    Then it feeds the solution to the callback.

    Note that the model cannot contain an objective.

    Args:
      model: The model to solve.
      callback: The callback that will be called at each solution.

    Returns:
      The status of the solve:

      * *FEASIBLE* if some solutions have been found
      * *INFEASIBLE* if the solver has proved there are no solution
      * *OPTIMAL* if all solutions have been found
    """
        warnings.warn(
            'SearchForAllSolutions is deprecated; use Solve() with' +
            'enumerate_all_solutions = True.', DeprecationWarning)
        if model.HasObjective():
            raise TypeError('Search for all solutions is only defined on '
                            'satisfiability problems')
        # Store old parameter.
        enumerate_all = self.parameters.enumerate_all_solutions
        self.parameters.enumerate_all_solutions = True

        self.Solve(model, callback)

        # Restore parameter.
        self.parameters.enumerate_all_solutions = enumerate_all
        return self.__solution.status

    def StopSearch(self):
        """Stops the current search asynchronously."""
        with self.__lock:
            if self.__solve_wrapper:
                self.__solve_wrapper.StopSearch()

    def Value(self, expression):
        """Returns the value of a linear expression after solve."""
        if not self.__solution:
            raise RuntimeError('Solve() has not be called.')
        return EvaluateLinearExpr(expression, self.__solution)

    def BooleanValue(self, literal):
        """Returns the boolean value of a literal after solve."""
        if not self.__solution:
            raise RuntimeError('Solve() has not be called.')
        return EvaluateBooleanExpression(literal, self.__solution)

    def ObjectiveValue(self):
        """Returns the value of the objective after solve."""
        return self.__solution.objective_value

    def BestObjectiveBound(self):
        """Returns the best lower (upper) bound found when min(max)imizing."""
        return self.__solution.best_objective_bound

    def StatusName(self, status=None):
        """Returns the name of the status returned by Solve()."""
        if status is None:
            status = self.__solution.status
        return cp_model_pb2.CpSolverStatus.Name(status)

    def NumBooleans(self):
        """Returns the number of boolean variables managed by the SAT solver."""
        return self.__solution.num_booleans

    def NumConflicts(self):
        """Returns the number of conflicts since the creation of the solver."""
        return self.__solution.num_conflicts

    def NumBranches(self):
        """Returns the number of search branches explored by the solver."""
        return self.__solution.num_branches

    def WallTime(self):
        """Returns the wall time in seconds since the creation of the solver."""
        return self.__solution.wall_time

    def UserTime(self):
        """Returns the user time in seconds since the creation of the solver."""
        return self.__solution.user_time

    def ResponseStats(self):
        """Returns some statistics on the solution found as a string."""
        return swig_helper.CpSatHelper.SerializedSolverResponseStats(
            self.__solution.SerializeToString())

    def ResponseProto(self):
        """Returns the response object."""
        return self.__solution

    def SufficientAssumptionsForInfeasibility(self):
        """Returns the indices of the infeasible assumptions."""
        return self.__solution.sufficient_assumptions_for_infeasibility

    def SolutionInfo(self):
        """Returns some information on the solve process.

    Returns some information on how the solution was found, or the reason
    why the model or the parameters are invalid.
    """
        return self.__solution.solution_info


class CpSolverSolutionCallback(swig_helper.SolutionCallback):
    """Solution callback.

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
  """

    def __init__(self):
        swig_helper.SolutionCallback.__init__(self)

    def OnSolutionCallback(self):
        """Proxy for the same method in snake case."""
        self.on_solution_callback()

    def BooleanValue(self, lit):
        """Returns the boolean value of a boolean literal.

    Args:
        lit: A boolean variable or its negation.

    Returns:
        The Boolean value of the literal in the solution.

    Raises:
        RuntimeError: if `lit` is not a boolean variable or its negation.
    """
        if not self.HasResponse():
            raise RuntimeError('Solve() has not be called.')
        if cmh.is_integral(lit):
            return bool(lit)
        elif isinstance(lit, IntVar) or isinstance(lit, _NotBooleanVariable):
            index = lit.Index()
            return self.SolutionBooleanValue(index)
        else:
            raise TypeError(f'Cannot interpret {lit} as a boolean expression.')

    def Value(self, expression):
        """Evaluates an linear expression in the current solution.

    Args:
        expression: a linear expression of the model.

    Returns:
        An integer value equal to the evaluation of the linear expression
        against the current solution.

    Raises:
        RuntimeError: if 'expression' is not a LinearExpr.
    """
        if not self.HasResponse():
            raise RuntimeError('Solve() has not be called.')

        value = 0
        to_process = [(expression, 1)]
        while to_process:
            expr, coeff = to_process.pop()
            if cmh.is_integral(expr):
                value += int(expr) * coeff
            elif isinstance(expr, _ProductCst):
                to_process.append(
                    (expr.Expression(), coeff * expr.Coefficient()))
            elif isinstance(expr, _Sum):
                to_process.append((expr.Left(), coeff))
                to_process.append((expr.Right(), coeff))
            elif isinstance(expr, _SumArray):
                for e in expr.Expressions():
                    to_process.append((e, coeff))
                    value += expr.Constant() * coeff
            elif isinstance(expr, _WeightedSum):
                for e, c in zip(expr.Expressions(), expr.Coefficients()):
                    to_process.append((e, coeff * c))
                value += expr.Constant() * coeff
            elif isinstance(expr, IntVar):
                value += coeff * self.SolutionIntegerValue(expr.Index())
            elif isinstance(expr, _NotBooleanVariable):
                value += coeff * (1 -
                                  self.SolutionIntegerValue(expr.Not().Index()))
            else:
                raise TypeError(
                    f'Cannot interpret {expression} as a linear expression.')

        return value

    def Response(self):
        """Returns the current solution response."""
        return cp_model_pb2.CpSolverResponse.FromString(
            swig_helper.SolutionCallback.SerializedResponse(self))


class ObjectiveSolutionPrinter(CpSolverSolutionCallback):
    """Display the objective value and time of intermediate solutions."""

    def __init__(self):
        CpSolverSolutionCallback.__init__(self)
        self.__solution_count = 0
        self.__start_time = time.time()

    def on_solution_callback(self):
        """Called on each new solution."""
        current_time = time.time()
        obj = self.ObjectiveValue()
        print('Solution %i, time = %0.2f s, objective = %i' %
              (self.__solution_count, current_time - self.__start_time, obj))
        self.__solution_count += 1

    def solution_count(self):
        """Returns the number of solutions found."""
        return self.__solution_count


class VarArrayAndObjectiveSolutionPrinter(CpSolverSolutionCallback):
    """Print intermediate solutions (objective, variable values, time)."""

    def __init__(self, variables):
        CpSolverSolutionCallback.__init__(self)
        self.__variables = variables
        self.__solution_count = 0
        self.__start_time = time.time()

    def on_solution_callback(self):
        """Called on each new solution."""
        current_time = time.time()
        obj = self.ObjectiveValue()
        print('Solution %i, time = %0.2f s, objective = %i' %
              (self.__solution_count, current_time - self.__start_time, obj))
        for v in self.__variables:
            print('  %s = %i' % (v, self.Value(v)), end=' ')
        print()
        self.__solution_count += 1

    def solution_count(self):
        """Returns the number of solutions found."""
        return self.__solution_count


class VarArraySolutionPrinter(CpSolverSolutionCallback):
    """Print intermediate solutions (variable values, time)."""

    def __init__(self, variables):
        CpSolverSolutionCallback.__init__(self)
        self.__variables = variables
        self.__solution_count = 0
        self.__start_time = time.time()

    def on_solution_callback(self):
        """Called on each new solution."""
        current_time = time.time()
        print('Solution %i, time = %0.2f s' %
              (self.__solution_count, current_time - self.__start_time))
        for v in self.__variables:
            print('  %s = %i' % (v, self.Value(v)), end=' ')
        print()
        self.__solution_count += 1

    def solution_count(self):
        """Returns the number of solutions found."""
        return self.__solution_count
