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

import types

# The classes below allow linear expressions to be expressed naturally with the
# usual arithmetic operators +-*/ and with constant numbers, which makes the
# python API very intuitive. See the top-level comment for examples.


class LinearExpr(object):
  """Holds linear expressions.

  A linear expression is essentially an offset (floating-point value), and a
  dictionary mapping MPVariable objects to their coefficient (which is also a
  floating-point value).
  """

  def Visit(self, coeffs):
    """Fills the coefficient dictionary, and returns the offset."""
    return self.DoVisit(coeffs, 1.0)

  def DoVisit(self, coeffs, multiplier):
    """Like Visit, but do that with a global floating-point multiplier."""
    raise NotImplementedError

  def solution_value(self):  # pylint: disable=invalid-name
    """Value of this linear expr, using the solution_value of its vars."""
    coeffs = {}
    constant = self.Visit(coeffs)
    return constant + sum(
        [var.solution_value() * coeff for var, coeff in coeffs.iteritems()])

  def __add__(self, expr):
    if isinstance(expr, (int, long, float)):
      return SumCst(self, expr)
    else:
      return Sum(self, expr)

  def __radd__(self, cst):
    if isinstance(cst, (int, long, float)):
      return SumCst(self, cst)
    else:
      raise TypeError

  def __sub__(self, expr):
    if isinstance(expr, (int, long, float)):
      return SumCst(self, -expr)
    else:
      return Sum(self, ProductCst(expr, -1))

  def __rsub__(self, cst):
    if isinstance(cst, (int, long, float)):
      return SumCst(ProductCst(self, -1), cst)
    else:
      raise TypeError

  def __mul__(self, cst):
    if isinstance(cst, (int, long, float)):
      return ProductCst(self, cst)
    else:
      raise TypeError

  def __rmul__(self, cst):
    if isinstance(cst, (int, long, float)):
      return ProductCst(self, cst)
    else:
      raise TypeError

  def __div__(self, cst):
    if isinstance(cst, (int, long, float)):
      if cst == 0.0:
        raise ZeroDivisionError
      else:
        return ProductCst(self, 1.0 / cst)
    else:
      raise TypeError

  def __truediv__(self, cst):
    if isinstance(cst, (int, long, float)):
      if cst == 0.0:
        raise ZeroDivisionError
      else:
        return ProductCst(self, 1.0 / cst)
    else:
      raise TypeError

  def __neg__(self):
    return ProductCst(self, -1)

  def __eq__(self, arg):
    if isinstance(arg, (int, long, float)):
      return LinearConstraint(self, arg, arg)
    else:
      return LinearConstraint(Sum(self, ProductCst(arg, -1)), 0.0, 0.0)

  def __ge__(self, arg):
    if isinstance(arg, (int, long, float)):
      return LinearConstraint(self, arg, 1e308)
    else:
      return LinearConstraint(Sum(self, ProductCst(arg, -1)), 0.0, 1e308)

  def __le__(self, arg):
    if isinstance(arg, (int, long, float)):
      return LinearConstraint(self, -1e308, arg)
    else:
      return LinearConstraint(Sum(self, ProductCst(arg, -1)), -1e308, 0.0)


class ProductCst(LinearExpr):
  """Represents the product of a LinearExpr by a constant."""

  def __init__(self, expr, coef):
    self.__expr = expr
    self.__coef = coef

  def __str__(self):
    if self.__coef == -1:
      return '-' + str(self.__expr)
    else:
      return '(' + str(self.__coef) + ' * ' + str(self.__expr) + ')'

  def DoVisit(self, coeffs, multiplier):
    current_multiplier = multiplier * self.__coef
    if current_multiplier:
      return self.__expr.DoVisit(coeffs, current_multiplier)
    return 0.0


class Sum(LinearExpr):
  """Represents the sum of two LinearExpr."""

  def __init__(self, left, right):
    self.__left = left
    self.__right = right

  def __str__(self):
    return '(' + str(self.__left) + ' + ' + str(self.__right) + ')'

  def DoVisit(self, coeffs, multiplier):
    constant = self.__left.DoVisit(coeffs, multiplier)
    constant += self.__right.DoVisit(coeffs, multiplier)
    return constant


class SumArray(LinearExpr):
  """Represents the sum of an array of objects (constants or LinearExpr)."""

  def __init__(self, array):
    if type(array) is types.GeneratorType:
      self.__array = [x for x in array]
    else:
      self.__array = array

  def __str__(self):
    return 'Sum(' + str(self.__array) + ')'

  def DoVisit(self, coeffs, multiplier):
    constant = 0.0
    for t in self.__array:
      if isinstance(t, (int, long, float)):
        constant += t * multiplier
      else:
        constant += t.DoVisit(coeffs, multiplier)
    return constant


class SumCst(LinearExpr):
  """Represents the sum of a LinearExpr and a constant."""

  def __init__(self, expr, cst):
    self.__expr = expr
    self.__cst = cst

  def __str__(self):
    return '(' + str(self.__expr) + ' + ' + str(self.__cst) + ')'

  def DoVisit(self, coeffs, multiplier):
    constant = self.__expr.DoVisit(coeffs, multiplier)
    return constant + self.__cst * multiplier


class LinearConstraint(object):
  """Represents a linear constraint: LowerBound <= LinearExpr <= UpperBound."""

  def __init__(self, expr, lb, ub):
    self.__expr = expr
    self.__lb = lb
    self.__ub = ub

  def __str__(self):
    if self.__lb > -1e308 and self.__ub < 1e308:
      if self.__lb == self.__ub:
        return str(self.__expr) + ' == ' + str(self.__lb)
      else:
        return (str(self.__lb) + ' <= ' + str(self.__expr) +
                ' <= ' + str(self.__ub))
    elif self.__lb > -1e308:
      return str(self.__expr) + ' >= ' + str(self.__lb)
    elif self.__ub < 1e308:
      return str(self.__expr) + ' <= ' + str(self.__ub)
    else:
      return 'Trivial inequality (always true)'

  def Extract(self, solver, name=''):
    """Performs the actual creation of the constraint object."""
    coeffs = {}
    constant = self.__expr.Visit(coeffs)
    lb = -solver.infinity()
    ub = solver.infinity()
    if self.__lb > -1e308:
      lb = self.__lb - constant
    if self.__ub < 1e308:
      ub = self.__ub - constant

    constraint = solver.RowConstraint(lb, ub, name)
    for v, c, in coeffs.iteritems():
      constraint.SetCoefficient(v, float(c))
    return constraint
