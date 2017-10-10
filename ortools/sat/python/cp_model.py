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
"""Propose a natural linear API on top of cp_model_pb2 python proto.

This file implements a easy to use API on top of the cp_model_pb2 API
defined in ../ .
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import collections
import numbers
from six import iteritems

from ortools.sat import cp_model_pb2
from ortools.sat import pywrapsat

# The classes below allow linear expressions to be expressed naturally with the
# usual arithmetic operators +-*/ and with constant numbers, which makes the
# python API very intuitive. See cp_model_test.py for examples.

INT_MIN = -9223372036854775808  # hardcoded to be platform independent.
INT_MAX = 9223372036854775807


def AssertIsInt64(x):
  if not isinstance(x, numbers.Integral):
    raise TypeError('Not an integer: %s' % x)
  if x < INT_MIN or x > INT_MAX:
    raise OverflowError('Does not fit in an int64: %s' % x)


def AssertIsBoolean(x):
  if not isinstance(x, numbers.Integral) or x < 0 or x > 1:
    raise TypeError('Not an boolean: %s' % x)


def CapInt64(v):
  if v > INT_MAX:
    return INT_MAX
  if v < INT_MIN:
    return INT_MIN
  return v


def CapSub(x, y):
  """Saturated arithmetics."""
  if not isinstance(x, numbers.Integral):
    raise TypeError('Not integral: ' + str(x))
  if not isinstance(y, numbers.Integral):
    raise TypeError('Not integral: ' + str(y))
  AssertIsInt64(x)
  AssertIsInt64(y)
  if y == 0:
    return x
  if x == y:
    if x == INT_MAX or x == INT_MIN:
      raise OverflowError(
          'Integer NaN: substracting INT_MAX or INT_MIN to itself')
    return 0
  if x == INT_MAX or x == INT_MIN:
    return x
  if y == INT_MAX:
    return INT_MIN
  if y == INT_MIN:
    return INT_MAX
  return CapInt64(x - y)


def DisplayBounds(bounds):
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
  if i < 0:
    return 'Not(%s)' % ShortName(model, -i-1)
  v = model.variables[i]
  if v.name:
    return v.name
  elif len(v.domain) == 2 and v.domain[0] == v.domain[1]:
    return str(v.domain[0])
  else:
    return '[%s]' % DisplayBounds(v.domain)


class IntegerExpression(object):
  """Holds an integer expression."""

  def GetVarValueMap(self):
    """Scan the expression, and return a list of (var_coef_map, constant)."""
    coeffs = collections.defaultdict(int)
    constant = 0
    to_process = [(self, 1)]
    while to_process:  # Flatten to avoid recursion.
      expr, coef = to_process.pop()
      if isinstance(expr, ProductCst):
        to_process.append((expr.Expression(), coef * expr.Coefficient()))
      elif isinstance(expr, SumArray):
        for e in expr.Array():
          to_process.append((e, coef))
        constant += expr.Constant() * coef
      elif isinstance(expr, IntVar):
        coeffs[expr] += coef
      elif isinstance(expr, NotBooleanVariable):
        raise TypeError('Cannot interpret literals in a integer expression.')
      else:
        raise TypeError('Unrecognized integer expression: ' + str(expr))

    return coeffs, constant

  def __add__(self, expr):
    return SumArray([self, expr])

  def __radd__(self, arg):
    return SumArray([self, arg])

  def __sub__(self, expr):
    return SumArray([self, -expr])

  def __rsub__(self, arg):
    return SumArray([-self, arg])

  def __mul__(self, arg):
    if isinstance(arg, numbers.Integral):
      if arg == 1:
        return self
      AssertIsInt64(arg)
      return ProductCst(self, arg)
    elif isinstance(arg, IntegerExpression):
      return Product(self, arg)
    else:
      raise TypeError('Not an integer expression: ' + str(arg))

  def __rmul__(self, arg):
    AssertIsInt64(arg)
    if arg == 1:
      return self
    return ProductCst(self, arg)

  def __div__(self, _):
    raise NotImplementedError('IntegerExpression.__div__')

  def __truediv__(self, _):
    raise NotImplementedError('IntegerExpression.__truediv__')

  def __mod__(self, _):
    raise NotImplementedError('IntegerExpression.__mod__')

  def __neg__(self):
    return ProductCst(self, -1)

  def __eq__(self, arg):
    if isinstance(arg, numbers.Integral):
      AssertIsInt64(arg)
      return BoundIntegerExpression(self, [arg, arg])
    else:
      return BoundIntegerExpression(self - arg, [0, 0])

  def __ge__(self, arg):
    if isinstance(arg, numbers.Integral):
      AssertIsInt64(arg)
      return BoundIntegerExpression(self, [arg, INT_MAX])
    else:
      return BoundIntegerExpression(self - arg, [0, INT_MAX])

  def __le__(self, arg):
    if isinstance(arg, numbers.Integral):
      AssertIsInt64(arg)
      return BoundIntegerExpression(self, [INT_MIN, arg])
    else:
      return BoundIntegerExpression(self - arg, [INT_MIN, 0])

  def __lt__(self, arg):
    if isinstance(arg, numbers.Integral):
      AssertIsInt64(arg)
      if arg == INT_MIN:
        raise ArithmeticError('< INT_MIN is not supported')
      return BoundIntegerExpression(self, [INT_MIN, CapInt64(arg - 1)])
    else:
      return BoundIntegerExpression(self - arg, [INT_MIN, -1])

  def __gt__(self, arg):
    if isinstance(arg, numbers.Integral):
      AssertIsInt64(arg)
      if arg == INT_MAX:
        raise ArithmeticError('> INT_MAX is not supported')
      return BoundIntegerExpression(self, [CapInt64(arg + 1), INT_MAX])
    else:
      return BoundIntegerExpression(self - arg, [1, INT_MAX])

  def __ne__(self, arg):
    if isinstance(arg, numbers.Integral):
      AssertIsInt64(arg)
      if arg == INT_MAX:
        return BoundIntegerExpression(self, [INT_MIN, INT_MAX - 1])
      elif arg == INT_MIN:
        return BoundIntegerExpression(self, [INT_MIN + 1, INT_MAX])
      else:
        return BoundIntegerExpression(
            self, [INT_MIN,
                   CapInt64(arg - 1),
                   CapInt64(arg + 1), INT_MAX])
    else:
      return BoundIntegerExpression(self - arg, [INT_MIN, -1, 1, INT_MAX])


class ProductCst(IntegerExpression):
  """Represents the product of a IntegerExpression by a constant."""

  def __init__(self, expr, coef):
    AssertIsInt64(coef)
    if isinstance(expr, ProductCst):
      self.__expr = expr.Expression()
      self.__coef = expr.Coefficient() * coef
    else:
      self.__expr = expr
      self.__coef = coef

  def __str__(self):
    if self.__coef == -1:
      return '-' + str(self.__expr)
    else:
      return '(' + str(self.__coef) + ' * ' + str(self.__expr) + ')'

  def __repr__(self):
    return 'ProductCst(' + repr(self.__expr) + ', ' + repr(self.__coef) + ')'

  def Coefficient(self):
    return self.__coef

  def Expression(self):
    return self.__expr


class SumArray(IntegerExpression):
  """Represents the sum of a list of IntegerExpression and a constant."""

  def __init__(self, array):
    self.__array = []
    self.__constant = 0
    for x in array:
      if isinstance(x, numbers.Integral):
        AssertIsInt64(x)
        self.__constant += x
      elif isinstance(x, IntegerExpression):
        self.__array.append(x)
      else:
        raise TypeError('Not an integer expression: ' + str(x))

  def __str__(self):
    if self.__constant == 0:
      return '({})'.format(' + '.join(map(str, self.__array)))
    else:
      return '({} + {})'.format(' + '.join(map(str, self.__array)),
                                self.__constant)

  def __repr__(self):
    return 'SumArray({}, {})'.format(', '.join(map(repr, self.__array)),
                                     self.__constant)

  def Array(self):
    return self.__array

  def Constant(self):
    return self.__constant


class IntVar(IntegerExpression):
  """Represents a IntegerExpression containing only a single variable."""

  def __init__(self, model, lb, ub, name):
    self.__model = model
    self.__index = len(model.variables)
    self.__var = model.variables.add()
    self.__var.domain.extend([lb, ub])
    self.__var.name = name
    self.__negation = None

  def Index(self):
    return self.__index

  def __str__(self):
    return self.__var.name

  def __repr__(self):
    return '%s(%i..%i)' % (self.__var.name, self.__var.domain[0],
                           self.__var.domain[1])

  def Not(self):
    if not self.__negation:
      self.__negation = NotBooleanVariable(self)
    return self.__negation


class NotBooleanVariable(IntegerExpression):

  def __init__(self, boolvar):
    self.__boolvar = boolvar

  def Index(self):
    return -self.__boolvar.Index() - 1

  def Not(self):
    return self.__boolvar


class Product(IntegerExpression):
  """Represents the product of two IntegerExpressions."""

  def __init__(self, left, right):
    self.__left = left
    self.__right = right

  def __str__(self):
    return '(' + str(self.__left) + ' * ' + str(self.__right) + ')'

  def __repr__(self):
    return 'Product(' + repr(self.__left) + ', ' + repr(self.__right) + ')'

  def Left(self):
    return self.__left

  def Right(self):
    return self.__right


class BoundIntegerExpression(object):
  """Represents a constraint: IntegerExpression in domain."""

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
          return str(lb) + ' <= ' + str(self.__expr) + ' <= ' + str(ub)
      elif lb > INT_MIN:
        return str(self.__expr) + ' >= ' + str(lb)
      elif ub < INT_MAX:
        return str(self.__expr) + ' <= ' + str(ub)
      else:
        return 'True (unbounded expr ' + str(self.__expr) + ')'
    else:
      return str(self.__expr) + ' in [' + DisplayBounds(self.__bounds) + ']'

  def Expression(self):
    return self.__expr

  def Bounds(self):
    return self.__bounds


class Constraint(object):
  """Base class for constraints."""

  def __init__(self, constraints):
    self.__index = len(constraints)
    self.__constraint = constraints.add()

  def OnlyEnforceIf(self, boolvar):
    self.__constraint.enforcement_literal.append(boolvar.Index())

  def Index(self):
    return self.__index

  def ConstraintProto(self):
    return self.__constraint


class IntervalVar(object):
  """Represents a Interval variable."""

  def __init__(self, model, start_index, size_index, end_index,
               is_present_index, name):
    self.__model = model
    self.__index = len(model.constraints)
    self.__ct = self.__model.constraints.add()
    self.__ct.interval.start = start_index
    self.__ct.interval.size = size_index
    self.__ct.interval.end = end_index
    if is_present_index is not None:
      self.__ct.enforcement_literal.append(is_present_index)
    if name:
      self.__ct.name = name

  def Index(self):
    return self.__index

  def __str__(self):
    return self.__ct.name

  def __repr__(self):
    interval = self.__ct.interval
    if self.__ct.enforcement_literal:
      return '%s(start = %s, size = %s, end = %s, is_present = %s)' % (
          self.__ct.name, ShortName(self.__model, interval.start),
          ShortName(self.__model, interval.size),
          ShortName(self.__model, interval.end),
          ShortName(self.__model, self.__ct.enforcement_literal[0]))
    else:
      return '%s(start = %s, size = %s, end = %s)' % (
          self.__ct.name, ShortName(self.__model, interval.start),
          ShortName(self.__model, interval.size),
          ShortName(self.__model, interval.end))


class CpModel(object):
  """Wrapper class around the cp_model proto."""

  def __init__(self):
    self.__model = cp_model_pb2.CpModelProto()
    self.__constant_map = {}
    self.__optional_constant_map = {}

  # Integer variable.

  def NewIntVar(self, lb, ub, name):
    return IntVar(self.__model, lb, ub, name)

  def NewBoolVar(self, name):
    return IntVar(self.__model, 0, 1, name)

  # Integer constraints.

  def AddLinearConstraint(self, terms, lb, ub):
    """Adds the constraints lb <= sum(terms) <= ub, where term = (var, coef)."""
    ct = Constraint(self.__model.constraints)
    model_ct = self.__model.constraints[ct.Index()]
    for t in terms:
      if not isinstance(t[0], IntVar):
        raise TypeError('Wrong argument' + str(t))
      AssertIsInt64(t[1])
      model_ct.linear.vars.append(t[0].Index())
      model_ct.linear.coeffs.append(t[1])
    model_ct.linear.domain.extend([lb, ub])
    return ct

  def AddLinearConstraintWithBounds(self, terms, bounds):
    """Adds the constraints sum(terms) in bounds, where term = (var, coef)."""
    ct = Constraint(self.__model.constraints)
    model_ct = self.__model.constraints[ct.Index()]
    for t in terms:
      if not isinstance(t[0], IntVar):
        raise TypeError('Wrong argument' + str(t))
      AssertIsInt64(t[1])
      model_ct.linear.vars.append(t[0].Index())
      model_ct.linear.coeffs.append(t[1])
    model_ct.linear.domain.extend(bounds)
    return ct

  def Add(self, ct):
    """Adds a BoundIntegerExpression to the model."""
    if isinstance(ct, BoundIntegerExpression):
      coeffs_map, constant = ct.Expression().GetVarValueMap()
      bounds = [CapSub(x, constant) for x in ct.Bounds()]
      return self.AddLinearConstraintWithBounds(coeffs_map.iteritems(), bounds)
    else:
      raise TypeError('Not supported: CpModel.Add(' + str(ct) + ')')

  def AddAllDifferent(self, variables):
    """Adds AllDifferent(variables)."""
    ct = Constraint(self.__model.constraints)
    model_ct = self.__model.constraints[ct.Index()]
    model_ct.all_diff.vars.extend([self.GetOrMakeIndex(x) for x in variables])
    return ct

  def AddElement(self, index, variables, target):
    """Adds the element constraint: variables[index] == target."""

    if not variables:
      raise ValueError('AddElement expects a non empty variables array')

    ct = Constraint(self.__model.constraints)
    model_ct = self.__model.constraints[ct.Index()]
    model_ct.element.index = self.GetOrMakeIndex(index)
    model_ct.element.vars.extend([self.GetOrMakeIndex(x) for x in variables])
    model_ct.element.target = self.GetOrMakeIndex(target)
    return ct

  def AddCircuit(self, nexts):
    """Adds Circuit(nexts)."""
    if not nexts:
      raise ValueError('AddCircuit expects a non empty nexts array')
    ct = Constraint(self.__model.constraints)
    model_ct = self.__model.constraints[ct.Index()]
    model_ct.circuit.nexts.extend([self.GetOrMakeIndex(x) for x in nexts])
    return ct

  def AddAllowedAssignments(self, variables, tuples):
    """Adds AllowedAssignments(variables, [tuples])."""
    if not variables:
      raise ValueError('AddAllowedAssignments expects a non empty variables '
                       'array')

    ct = Constraint(self.__model.constraints)
    model_ct = self.__model.constraints[ct.Index()]
    model_ct.table.vars.extend([self.GetOrMakeIndex(x) for x in variables])
    arity = len(variables)
    for t in tuples:
      if len(t) != arity:
        raise TypeError('Tuple ' + str(t) + ' has the wrong arity')
      for v in t:
        AssertIsInt64(v)
        model_ct.table.values.append(v)

  def AddForbiddenAssignments(self, variables, tuples):
    """Adds AddForbiddenAssignments(variables, [tuples])."""

    if not variables:
      raise ValueError('AddForbiddenAssignments expects a non empty variables '
                       'array')

    index = len(self.__model.constraints)
    self.AddAllowedAssignments(variables, tuples)
    self.__model.constraints[index].table.negated = True

  def AddAutomata(self, transition_variables, starting_state, final_states,
                  transition_triples):
    """Adds an automata constraint.

    Args:
      transition_variables: A non empty list of variables whose values
        correspond to the labels of the arcs traversed by the automata.
      starting_state: The initial state of the automata.
      final_states: a non empty list of admissible final states.
      transition_triples: A list of transition for the automata, in the
        following format (current_state, variable_value, next_state).


    Raises:
      ValueError: if transition_variables, final_states, or transition_triples
      are empty.
    """

    if not transition_variables:
      raise ValueError('AddAutomata expects a non empty transition_variables '
                       'array')
    if not final_states:
      raise ValueError('AddAutomata expects some final states')

    if not transition_triples:
      raise ValueError('AddAutomata expects some transtion triples')

    ct = Constraint(self.__model.constraints)
    model_ct = self.__model.constraints[ct.Index()]
    model_ct.automata.vars.extend(
        [self.GetOrMakeIndex(x) for x in transition_variables])
    AssertIsInt64(starting_state)
    model_ct.automata.starting_state = starting_state
    for v in final_states:
      AssertIsInt64(v)
      model_ct.automata.final_states.append(v)
    for t in transition_triples:
      if len(t) != 3:
        raise TypeError('Tuple ' + str(t) + ' has the wrong arity (!= 3)')
      AssertIsInt64(t[0])
      AssertIsInt64(t[1])
      AssertIsInt64(t[2])
      model_ct.automata.transition_head.append(t[0])
      model_ct.automata.transition_label.append(t[0])
      model_ct.automata.transition_tail.append(t[0])

  def AddInverse(self, variables, inverse_variables):
    """Adds AddInverse(variables, inverse_variables)."""
    ct = Constraint(self.__model.constraints)
    model_ct = self.__model.constraints[ct.Index()]
    model_ct.inverse.f_direct.extend(
        [self.GetOrMakeIndex(x) for x in variables])
    model_ct.inverse.f_inverse.extend(
        [self.GetOrMakeIndex(x) for x in inverse_variables])
    return ct

  def AddBoolOr(self, literals):
    """Adds Or(literals) == true."""
    ct = Constraint(self.__model.constraints)
    model_ct = self.__model.constraints[ct.Index()]
    model_ct.bool_or.literals.extend(
        [self.GetOrMakeBooleanIndex(x) for x in literals])
    return ct

  def AddBoolAnd(self, literals):
    """Adds And(literals) == true."""
    ct = Constraint(self.__model.constraints)
    model_ct = self.__model.constraints[ct.Index()]
    model_ct.bool_and.literals.extend(
        [self.GetOrMakeBooleanIndex(x) for x in literals])
    return ct

  def AddBoolXOr(self, literals):
    """Adds XOr(literals) == true."""
    ct = Constraint(self.__model.constraints)
    model_ct = self.__model.constraints[ct.Index()]
    model_ct.bool_xor.literals.extend(
        [self.GetOrMakeBooleanIndex(x) for x in literals])
    return ct

  def AddMinEquality(self, target, variables):
    """Adds target == Min(variables)."""
    ct = Constraint(self.__model.constraints)
    model_ct = self.__model.constraints[ct.Index()]
    model_ct.int_min.vars.extend([self.GetOrMakeIndex(x) for x in variables])
    model_ct.int_min.target = target.Index()
    return ct

  def AddMaxEquality(self, target, args):
    """Adds target == Max(variables)."""
    ct = Constraint(self.__model.constraints)
    model_ct = self.__model.constraints[ct.Index()]
    model_ct.int_max.vars.extend([self.GetOrMakeIndex(x) for x in args])
    model_ct.int_max.target = target.Index()
    return ct

  def AddDivisionEquality(self, target, num, denom):
    """Creates target == num // denom."""
    ct = Constraint(self.__model.constraints)
    model_ct = self.__model.constraints[ct.Index()]
    model_ct.int_div.vars.extend(
        [self.GetOrMakeIndex(num),
         self.GetOrMakeIndex(denom)])
    model_ct.int_div.target = target.Index()
    return ct

  def AddModuloEquality(self, target, var, mod):
    """Creates target = var % mod."""
    ct = Constraint(self.__model.constraints)
    model_ct = self.__model.constraints[ct.Index()]
    model_ct.int_mod.vars.extend(
        [self.GetOrMakeIndex(var),
         self.GetOrMakeIndex(mod)])
    model_ct.int_mod.target = target.Index()
    return ct

  def AddProdEquality(self, target, args):
    """Creates target == PROD(args)."""
    ct = Constraint(self.__model.constraints)
    model_ct = self.__model.constraints[ct.Index()]
    model_ct.int_prod.vars.extend([self.GetOrMakeIndex(x) for x in args])
    model_ct.int_prod.target = target.Index()
    return ct

  # Scheduling support

  def NewIntervalVar(self, start, size, end, name):
    start_index = self.GetOrMakeIndex(start)
    size_index = self.GetOrMakeIndex(size)
    end_index = self.GetOrMakeIndex(end)
    return IntervalVar(self.__model, start_index, size_index, end_index, None,
                       name)

  def NewOptionalIntervalVar(self, start, size, end, is_present, name):
    is_present_index = self.GetOrMakeBooleanIndex(is_present)
    start_index = self.GetOrMakeOptionalIndex(start, is_present)
    size_index = self.GetOrMakeOptionalIndex(size, is_present)
    end_index = self.GetOrMakeOptionalIndex(end, is_present)
    return IntervalVar(self.__model, start_index, size_index, end_index,
                       is_present_index, name)

  def AddNoOverlap(self, interval_vars):
    """Adds NoOverlap(interval_vars)."""
    ct = Constraint(self.__model.constraints)
    model_ct = self.__model.constraints[ct.Index()]
    model_ct.no_overlap.intervals.extend(
        [self.GetIntervalIndex(x) for x in interval_vars])
    return ct

  def AddNoOverlap2D(self, x_transition_triples, y_transition_triples):
    """Adds NoOverlap2D(x_transition_triples, y_transition_triples)."""
    ct = Constraint(self.__model.constraints)
    model_ct = self.__model.constraints[ct.Index()]
    model_ct.no_overlap_2d.x_intervals.extend(
        [self.GetIntervalIndex(x) for x in x_transition_triples])
    model_ct.no_overlap_2d.y_intervals.extend(
        [self.GetIntervalIndex(x) for x in y_transition_triples])
    return ct

  def AddCumulative(self, transition_triples, demands, capacity):
    """Adds Cumulative(transition_triples, demands, capacity)."""
    ct = Constraint(self.__model.constraints)
    model_ct = self.__model.constraints[ct.Index()]
    model_ct.cumulative.intervals.extend(
        [self.GetIntervalIndex(x) for x in transition_triples])
    model_ct.cumulative.demands.extend(
        [self.GetOrMakeIndex(x) for x in demands])
    model_ct.cumulative.capacity = self.GetOrMakeIndex(capacity)
    return ct

  # Helpers.

  def __str__(self):
    return str(self.__model)

  def ModelProto(self):
    return self.__model

  def Negated(self, index):
    return -index - 1

  def GetOrMakeIndex(self, arg):
    if isinstance(arg, IntVar):
      return arg.Index()
    elif (isinstance(arg, ProductCst) and
          isinstance(arg.Expression(), IntVar) and arg.Coefficient() == -1):
      return -arg.Expression().Index() - 1
    elif isinstance(arg, numbers.Integral):
      AssertIsInt64(arg)
      return self.GetOrMakeIndexFromConstant(arg)
    else:
      raise TypeError('NotSupported: model.GetOrMakeIndex(' + str(arg) + ')')

  def GetOrMakeOptionalIndex(self, arg, is_present):
    if isinstance(arg, IntVar):
      return arg.Index()
    elif (isinstance(arg, ProductCst) and
          isinstance(arg.Expression(), IntVar) and arg.Coefficient() == -1):
      return -arg.Expression().Index() - 1
    elif isinstance(arg, numbers.Integral):
      AssertIsInt64(arg)
      return self.GetOrMakeOptionalIndexFromConstant(arg, is_present)
    else:
      raise TypeError('NotSupported: model.GetOrMakeOptionalIndex(' + str(arg) +
                      ')')

  def GetOrMakeBooleanIndex(self, arg):
    if isinstance(arg, IntVar):
      self._AssertIsBooleanVariable(arg)
      return arg.Index()
    elif isinstance(arg, NotBooleanVariable):
      self._AssertIsBooleanVariable(arg.Not())
      return arg.Index()
    elif isinstance(arg, numbers.Integral):
      AssertIsBoolean(arg)
      return self.GetOrMakeIndexFromConstant(arg)
    else:
      raise TypeError('NotSupported: model.GetOrMakeBooleanIndex(' + str(arg) +
                      ')')

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

  def GetOrMakeOptionalIndexFromConstant(self, value, is_present):
    self._AssertIsBooleanVariable(is_present)
    if (is_present, value) in self.__optional_constant_map:
      return self.__optional_constant_map[(is_present, value)]
    index = len(self.__model.variables)
    var = self.__model.variables.add()
    var.domain.extend([value, value])
    self.__optional_constant_map[(is_present, value)] = index
    return index

  def _SetObjective(self, obj, minimize):
    """Sets the objective of the model."""
    if isinstance(obj, IntVar):
      self.__model.ClearField('objective')
      self.__model.objective.coeffs.append(1)
      self.__model.objective.offset = 0
      if minimize:
        self.__model.objective.vars.append(obj.Index())
        self.__model.objective.scaling_factor = 1
      else:
        self.__model.objective.vars.append(self.Negated(obj.Index()))
        self.__model.objective.scaling_factor = -1
    elif isinstance(obj, IntegerExpression):
      coeffs_map, constant = obj.GetVarValueMap()
      self.__model.ClearField('objective')
      if minimize:
        self.__model.objective.scaling_factor = 1
        self.__model.objective.offset = constant
      else:
        self.__model.objective.scaling_factor = -1
        self.__model.objective.offset = -constant
      for v, c, in iteritems(coeffs_map):
        self.__model.objective.coeffs.append(c)
        if minimize:
          self.__model.objective.vars.append(v.Index())
        else:
          self.__model.objective.vars.append(self.Negated(v.Index()))
    else:
      raise TypeError('TypeError: ' + str(obj) + ' is not a valid objective')

  def Minimize(self, obj):
    """Sets the objective of the model to minimize(obj)."""
    self._SetObjective(obj, minimize=True)

  def Maximize(self, obj):
    """Sets the objective of the model to maximize(obj)."""
    self._SetObjective(obj, minimize=False)

  def _AssertIsBooleanVariable(self, x):
    if isinstance(x, IntVar):
      var = self.__model.variables[x.Index()]
      if len(var.domain) != 2 or var.domain[0] < 0 or var.domain[1] > 1:
        raise TypeError('TypeError: ' + str(x) + ' is not a boolean variable')
    elif not isinstance(x, NotBooleanVariable):
      raise TypeError('TypeError: ' + str(x) + ' is not a boolean variable')


class CpSolver(object):
  """Main solver class."""

  def __init__(self):
    self.__model = None
    self.__solution = None

  def Solve(self, model):
    self.__model = model
    self.__solution = pywrapsat.SatHelper.Solve(self.__model.ModelProto())
    return self.__solution.status

  def Value(self, expression):
    """Returns the value of an integer expression."""
    if not self.__solution:
      raise RuntimeError('Solve() has not be called.')
    value = 0
    to_process = [(expression, 1)]
    while to_process:
      expr, coef = to_process.pop()
      if isinstance(expr, ProductCst):
        to_process.append((expr.Expression(), coef * expr.Coefficient()))
      elif isinstance(expr, SumArray):
        for e in expr.Array():
          to_process.append((e, coef))
        value += expr.Constant() * coef
      elif isinstance(expr, IntVar):
        value += coef * self.__solution.solution[expr.Index()]
      elif isinstance(expr, NotBooleanVariable):
        raise TypeError('Cannot interpret literals in a integer expression.')
    return value

  def ObjectiveValue(self):
    return self.__solution.objective_value

  def StatusName(self, status):
    return cp_model_pb2.CpSolverStatus.Name(status)
