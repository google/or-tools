# Copyright 2010-2014 Google
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
"""Patch to the python wrapper of ../linear_solver.h providing an algebraic API.

This is directly imported, and use exclusively in ./linear_solver.swig. See that
file.
For examples leveraging the code defined here, see ./pywraplp_test.py and
../../../python/linear_programming.py.
"""

import numbers
from six import iteritems

# The classes below allow linear expressions to be expressed naturally with the
# usual arithmetic operators +-*/ and with constant numbers, which makes the
# python API very intuitive. See the top-level comment for examples.


inf = float('inf')


class _FakeMPVariableRepresentingTheConstantOffset(object):
  """A dummy class for a singleton instance used to represent the constant.

  To represent linear expressions, we store a dictionary
  MPVariable->coefficient. To represent the constant offset of the expression,
  we use this class as a substitute: its coefficient will be the offset. To
  properly be evaluated, its solution_value() needs to be 1.
  """

  def solution_value(self):  # pylint: disable=invalid-name
    return 1

  def __repr__(self):
    return 'OFFSET_KEY'


OFFSET_KEY = _FakeMPVariableRepresentingTheConstantOffset()


def CastToLinExp(v):
  if isinstance(v, numbers.Number):
    return Constant(v)
  else:
    return v


class LinearExpr(object):
  """Holds linear expressions.

  A linear expression is essentially an offset (floating-point value), and a
  dictionary mapping MPVariable objects to their coefficient (which is also a
  floating-point value).
  """

  OVERRIDDEN_OPERATOR_METHODS = [
      '__%s__' % opname
      for opname in ['add', 'radd', 'sub', 'rsub', 'mul', 'rmul', 'div',
                     'truediv', 'neg', 'eq', 'ge', 'le', 'gt', 'lt', 'ne']
  ]

  def solution_value(self):  # pylint: disable=invalid-name
    """Value of this linear expr, using the solution_value of its vars."""
    coeffs = self.GetCoeffs()
    return sum(var.solution_value() * coeff for var, coeff in coeffs.items())

  def AddSelfToCoeffMap(self, coeffs, multiplier):
    raise NotImplementedError

  def GetCoeffs(self):
    coeffs = {}
    self.AddSelfToCoeffMap(coeffs, 1.0)
    return coeffs

  def __add__(self, expr):
    return Sum(self, expr)

  def __radd__(self, cst):
    return Sum(self, cst)

  def __sub__(self, expr):
    return Sum(self, -expr)

  def __rsub__(self, cst):
    return Sum(-self, cst)

  def __mul__(self, cst):
    return ProductCst(self, cst)

  def __rmul__(self, cst):
    return ProductCst(self, cst)

  def __div__(self, cst):
    return ProductCst(self, 1.0 / cst)

  def __truediv__(self, cst):
    return ProductCst(self, 1.0 / cst)

  def __neg__(self):
    return ProductCst(self, -1)

  def __eq__(self, arg):
    if isinstance(arg, numbers.Number):
      return LinearConstraint(self, arg, arg)
    else:
      return LinearConstraint(self - arg, 0.0, 0.0)

  def __ge__(self, arg):
    if isinstance(arg, numbers.Number):
      return LinearConstraint(self, arg, inf)
    else:
      return LinearConstraint(self - arg, 0.0, inf)

  def __le__(self, arg):
    if isinstance(arg, numbers.Number):
      return LinearConstraint(self, -inf, arg)
    else:
      return LinearConstraint(self - arg, -inf, 0.0)

  def __lt__(self, arg):
    raise ValueError(
        'Operators "<" and ">" not supported with the linear solver')

  def __gt__(self, arg):
    raise ValueError(
        'Operators "<" and ">" not supported with the linear solver')

  def __ne__(self, arg):
    raise ValueError('Operator "!=" not supported with the linear solver')


class VariableExpr(LinearExpr):
  """Represents a LinearExpr containing only a single variable."""

  def __init__(self, mpvar):
    self.__var = mpvar

  def AddSelfToCoeffMap(self, coeffs, multiplier):
    coeffs[self.__var] = coeffs.get(self.__var, 0.0) + multiplier


class ProductCst(LinearExpr):
  """Represents the product of a LinearExpr by a constant."""

  def __init__(self, expr, coef):
    self.__expr = CastToLinExp(expr)
    if isinstance(coef, numbers.Number):
      self.__coef = coef
    else:
      raise TypeError

  def __str__(self):
    if self.__coef == -1:
      return '-' + str(self.__expr)
    else:
      return '(' + str(self.__coef) + ' * ' + str(self.__expr) + ')'

  def AddSelfToCoeffMap(self, coeffs, multiplier):
    current_multiplier = multiplier * self.__coef
    if current_multiplier:
      self.__expr.AddSelfToCoeffMap(coeffs, current_multiplier)


class Constant(LinearExpr):

  def __init__(self, val):
    self.__val = val

  def __str__(self):
    return str(self.__val)

  def AddSelfToCoeffMap(self, coeffs, multiplier):
    coeffs[OFFSET_KEY] = coeffs.get(OFFSET_KEY, 0.0) + self.__val * multiplier


class SumArray(LinearExpr):
  """Represents the sum of a list of LinearExpr."""

  def __init__(self, array):
    self.__array = [CastToLinExp(elem) for elem in array]

  def __str__(self):
    return '({})'.format(' + '.join(map(str, self.__array)))

  def AddSelfToCoeffMap(self, coeffs, multiplier):
    for arg in self.__array:
      arg.AddSelfToCoeffMap(coeffs, multiplier)


def Sum(*args):
  return SumArray(args)

SumCst = Sum  # pylint: disable=invalid-name


class LinearConstraint(object):
  """Represents a linear constraint: LowerBound <= LinearExpr <= UpperBound."""

  def __init__(self, expr, lb, ub):
    self.__expr = expr
    self.__lb = lb
    self.__ub = ub

  def __str__(self):
    if self.__lb > -inf and self.__ub < inf:
      if self.__lb == self.__ub:
        return str(self.__expr) + ' == ' + str(self.__lb)
      else:
        return (str(self.__lb) + ' <= ' + str(self.__expr) +
                ' <= ' + str(self.__ub))
    elif self.__lb > -inf:
      return str(self.__expr) + ' >= ' + str(self.__lb)
    elif self.__ub < inf:
      return str(self.__expr) + ' <= ' + str(self.__ub)
    else:
      return 'Trivial inequality (always true)'

  def Extract(self, solver, name=''):
    """Performs the actual creation of the constraint object."""
    coeffs = self.__expr.GetCoeffs()
    constant = coeffs.pop(OFFSET_KEY, 0.0)
    lb = -solver.infinity()
    ub = solver.infinity()
    if self.__lb > -inf:
      lb = self.__lb - constant
    if self.__ub < inf:
      ub = self.__ub - constant

    constraint = solver.RowConstraint(lb, ub, name)
    for v, c, in iteritems(coeffs):
      constraint.SetCoefficient(v, float(c))
    return constraint
