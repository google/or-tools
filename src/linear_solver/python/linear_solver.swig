// Copyright 2010-2013 Google
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

%include base/base.swig
%include util/python/data.swig

// Swig doesn't like module initializers.
#define DECLARE_MODULE_INITIALIZER(x);

// Include the file we want to wrap a first time.
%{
#include "linear_solver/linear_solver.h"
#include "linear_solver/linear_solver2.pb.h"
#include "linear_solver/linear_solver_ext.h"
%}

// Define the renaming of methods.
%ignore MakeBoolVarArray;
%ignore MakeIntVarArray;
%ignore MakeNumVarArray;
%ignore MakeVarArray;
%rename (Activity) activity;
%rename (BasisStatus) basis_status;
%rename (BestObjectiveBound) best_objective_bound;
%rename (BoolVar) MakeBoolVar;
%rename (Constraint) MPConstraint;
%rename (Constraint) MakeRowConstraint;
%rename (DualValue) dual_value;
%rename (Index) index;
%rename (Infinity) infinity;
%rename (IntVar) MakeIntVar;
%rename (Integer) integer;
%rename (Iterations) iterations;
%rename (Lb) lb;
%rename (LookupConstraint) LookupConstraintOrNull;
%rename (LookupVariable) LookupVariableOrNull;
%rename (Nodes) nodes;
%rename (NumVar) MakeNumVar;
%rename (Offset) offset;
%rename (ReducedCost) reduced_cost;
%rename (SetLb) SetLB;
%rename (SetTimeLimit) set_time_limit;
%rename (SetUb) SetUB;
%rename (SolutionValue) solution_value;
%rename (Solver) MPSolver;
%rename (TimeLimit) time_limit;
%rename (Ub) ub;
%rename (Variable) MPVariable;
%rename (WallTime) wall_time;

// Ignore non-mutable version of the Objective accessor, rename the mutable one.
%ignore Objective;
%rename (Objective) MutableObjective;
// Access to the underlying solver is available only in C++.
%ignore underlying_solver;
// The ExportModelAs*Format methods return the string directly in the python
// API, using an empty string to indicate failures (which can also indicate an
// empty model).
%ignore ExportModelAsLpFormat(bool, string*);
%ignore ExportModelAsMpsFormat(bool, bool, string*);

namespace operations_research {
%pythoncode {

import types
import sys

import ortools.linear_solver.linear_solver2_pb2

# The classes below allow linear expressions to be expressed naturally with the
# usual arithmetic operators +-*/ and with constant numbers, which makes the
# python API very intuitive. See RunLinearExampleNaturalLanguageAPI() in
# ../python/linear_programming.py for an example.
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

  def solution_value(self):
    """Value of this linear expr, using the solution_value of its vars."""
    coeffs = {}
    constant = self.Visit(coeffs)
    if sys.version_info[0] >= 3: # Python 3
      return constant + sum(
          [var.solution_value() * coeff for var, coeff in coeffs.items()])
    else:
      return constant + sum(
          [var.solution_value() * coeff for var, coeff in coeffs.iteritems()])


  def IsConstant(self, expr):
    if sys.version_info[0] >= 3: # Python 3
      return  isinstance(expr, (int, float))
    else:
      return  isinstance(expr, (int, long, float))

  def __add__(self, expr):
    if self.IsConstant(expr):
      return SumCst(self, expr)
    else:
      return Sum(self, expr)

  def __radd__(self, cst):
    if self.IsConstant(cst):
      return SumCst(self, cst)
    else:
      raise TypeError

  def __sub__(self, expr):
    if self.IsConstant(expr):
      return SumCst(self, -expr)
    else:
      return Sum(self, ProductCst(expr, -1))

  def __rsub__(self, cst):
    if self.IsConstant(cst):
      return SumCst(ProductCst(self, -1), cst)
    else:
      raise TypeError

  def __mul__(self, cst):
    if self.IsConstant(cst):
      return ProductCst(self, cst)
    else:
      raise TypeError

  def __rmul__(self, cst):
    if self.IsConstant(cst):
      return ProductCst(self, cst)
    else:
      raise TypeError

  def __div__(self, cst):
    if self.IsConstant(cst):
      if cst == 0.0:
        raise ZeroDivisionError
      else:
        return ProductCst(self, 1.0 / cst)
    else:
      raise TypeError

  def __truediv__(self, cst):
    if self.IsConstant(cst):
      if cst == 0.0:
        raise ZeroDivisionError
      else:
        return ProductCst(self, 1.0 / cst)
    else:
      raise TypeError

  def __neg__(self):
    return ProductCst(self, -1)

  def __eq__(self, arg):
    if self.IsConstant(arg):
      return LinearConstraint(self, arg, arg)
    else:
      return LinearConstraint(Sum(self, ProductCst(arg, -1)), 0.0, 0.0)

  def __ge__(self, arg):
    if self.IsConstant(arg):
      return LinearConstraint(self, arg, 1e308)
    else:
      return LinearConstraint(Sum(self, ProductCst(arg, -1)), 0.0, 1e308)

  def __le__(self, arg):
    if self.IsConstant(arg):
      return LinearConstraint(self, -1e308, arg)
    else:
      return LinearConstraint(Sum(self, ProductCst(arg, -1)), -1e308, 0.0)


class ProductCst(LinearExpr):
  def __init__(self, expr, coef):
    self.__expr = expr
    self.__coef = coef

  def __str__(self):
    if (self.__coef == -1):
      return '-' + str(self.__expr)
    else:
      return '(' + str(self.__coef) + ' * ' + str(self.__expr) + ')'

  def DoVisit(self, coeffs, multiplier):
    current_multiplier = multiplier * self.__coef
    if current_multiplier:
      return self.__expr.DoVisit(coeffs, current_multiplier)
    return 0.0


class Sum(LinearExpr):
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
      if self.IsConstant(t):
        constant += t * multiplier
      else:
        constant += t.DoVisit(coeffs, multiplier)
    return constant


class SumCst(LinearExpr):
  def __init__(self, expr, cst):
    self.__expr = expr
    self.__cst = cst

  def __str__(self):
    return '(' + str(self.__expr) + ' + ' + str(self.__cst) + ')'

  def DoVisit(self, coeffs, multiplier):
    constant = self.__expr.DoVisit(coeffs, multiplier)
    return constant + self.__cst * multiplier


class LinearConstraint(object):
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
                " <= " + str(self.__ub))
    elif self.__lb > -1e308:
      return str(self.__expr) + ' >= ' + str(self.__lb)
    elif self.__ub < 1e308:
      return str(self.__expr) + ' <= ' + str(self.__ub)
    else:
      return 'Trivial inequality (always true)'

  def Extract(self, solver, name=''):
    coeffs = {}
    constant = self.__expr.Visit(coeffs)
    lb = -solver.Infinity()
    ub = solver.Infinity()
    if self.__lb > -1e308:
      lb = self.__lb - constant
    if self.__ub < 1e308:
      ub = self.__ub - constant

    constraint = solver.RowConstraint(lb, ub, name)
    if sys.version_info[0] >= 3: # Python 3
      for v, c, in coeffs.items():
        constraint.SetCoefficient(v, float(c))
    else:
      for v, c, in coeffs.iteritems():
        constraint.SetCoefficient(v, float(c))
    return constraint
}

# We also apply the same idea to MPVariable.
%extend MPVariable {
  std::string __str__() {
    return self->name();
  }
  std::string __repr__() {
    return self->name();
  }
  %pythoncode {
  def IsConstant(self, expr):
    if sys.version_info[0] >= 3: # Python 3
      return  isinstance(expr, (int, float))
    else:
      return  isinstance(expr, (int, long, float))

  __hash__ = object.__hash__

  def __add__(self, expr):
    if self.IsConstant(expr):
      return SumCst(self, expr)
    else:
      return Sum(self, expr)

  def __radd__(self, cst):
    if self.IsConstant(cst):
      return SumCst(self, cst)
    else:
      raise TypeError

  def __sub__(self, expr):
    if self.IsConstant(expr):
      return SumCst(self, -expr)
    else:
      return Sum(self, ProductCst(expr, -1))

  def __rsub__(self, cst):
    if self.IsConstant(cst):
      return SumCst(ProductCst(self, -1), cst)
    else:
      raise TypeError

  def __mul__(self, cst):
    if self.IsConstant(cst):
      return ProductCst(self, cst)
    else:
      raise TypeError

  def __rmul__(self, cst):
    if self.IsConstant(cst):
      return ProductCst(self, cst)
    else:
      raise TypeError

  def __div__(self, cst):
    if self.IsConstant(cst):
      if cst == 0.0:
        raise ZeroDivisionError
      else:
        return ProductCst(self, 1.0 / cst)
    else:
      raise TypeError

  def __truediv__(self, cst):
    if self.IsConstant(cst):
      if cst == 0.0:
        raise ZeroDivisionError
      else:
        return ProductCst(self, 1.0 / cst)
    else:
      raise TypeError

  def __neg__(self):
    return ProductCst(self, -1)

  def __eq__(self, arg):
    if self.IsConstant(arg):
      return LinearConstraint(self, arg, arg)
    else:
      return LinearConstraint(Sum(self, ProductCst(arg, -1)), 0.0, 0.0)

  def __ge__(self, arg):
    if self.IsConstant(arg):
      return LinearConstraint(self, arg, 1e308)
    else:
      return LinearConstraint(Sum(self, ProductCst(arg, -1)), 0.0, 1e308)

  def __le__(self, arg):
    if self.IsConstant(arg):
      return LinearConstraint(self, -1e308, arg)
    else:
      return LinearConstraint(Sum(self, ProductCst(arg, -1)), -1e308, 0.0)

  def Visit(self, coeffs):
    return self.DoVisit(coeffs, 1.0)

  def DoVisit(self, coeffs, multiplier):
    if self in coeffs:
      coeffs[self] += multiplier
    else:
      coeffs[self] = multiplier
    return 0.0
  }
}

# See ../python/linear_programming.py for example on how to use the nice
# extended python API provided below.
%extend MPSolver {
  std::string ExportModelAsLpFormat(bool obfuscated) {
    std::string output;
    if (!self->ExportModelAsLpFormat(obfuscated, &output)) return "";
    return output;
  }

  std::string ExportModelAsMpsFormat(bool fixed_format, bool obfuscated) {
    std::string output;
    if (!self->ExportModelAsMpsFormat(fixed_format, obfuscated, &output)) {
      return "";
    }
    return output;
  }

  %pythoncode {
    def IsConstant(self, expr):
      if sys.version_info[0] >= 3: # Python 3
        return  isinstance(expr, (int, float))
      else:
        return  isinstance(expr, (int, long, float))

    def Add(self, constraint, name=''):
      if isinstance(constraint, bool):
        if constraint:
          return self.RowConstraint(0, 0, name)
        else:
          return self.RowConstraint(1, 1, name)
      else:
        return constraint.Extract(self, name)

    def Sum(self, expr_array):
      result = SumArray(expr_array)
      return result

    # Compatibility
    def RowConstraint(self, *args):
      return self.Constraint(*args)

    def Minimize(self, expr):
      objective = self.Objective()
      objective.Clear()
      objective.SetMinimization()
      coeffs = {}
      offset = expr.Visit(coeffs)
      objective.AddOffset(offset)
      if sys.version_info[0] >= 3: # Python 3
        for v, c, in coeffs.items():
          objective.SetCoefficient(v, float(c))
      else:
        for v, c, in coeffs.iteritems():
          objective.SetCoefficient(v, float(c))

    def Maximize(self, expr):
      objective = self.Objective()
      objective.Clear()
      objective.SetMaximization()
      coeffs = {}
      offset = expr.Visit(coeffs)
      objective.AddOffset(offset)
      if sys.version_info[0] >= 3: # Python 3
        for v, c, in coeffs.items():
          objective.SetCoefficient(v, float(c))
      else:
        for v, c, in coeffs.iteritems():
          objective.SetCoefficient(v, float(c))
  }
}

} // namespace operations_research

// Support protobuf.
PY_PROTO_TYPEMAP(ortools.linear_solver.linear_solver2_pb2,
                 operations_research::new_proto::MPModelProto,
                 MPModelProto);
PY_PROTO_TYPEMAP(ortools.linear_solver.linear_solver2_pb2,
                 operations_research::new_proto::MPSolutionResponse,
                 MPSolutionResponse);
PY_PROTO_TYPEMAP(ortools.linear_solver.linear_solver2_pb2,
                 operations_research::new_proto::MPModelRequest,
                 MPModelRequest);

// Wrap linear_solver includes
%include "linear_solver/linear_solver.h"
%include "linear_solver/linear_solver_ext.h"
