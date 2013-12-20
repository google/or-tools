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
%include util/data.swig
// Swig doesn't like module initializers.
#define DECLARE_MODULE_INITIALIZER(x);

// Include the file we want to wrap a first time.
%{
#include "linear_solver/linear_solver.h"
#include "linear_solver/linear_solver2.pb.h"
#include "linear_solver/linear_solver_ext.h"
%}

#ifdef SWIGPYTHON

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
%rename (ObjectiveValue) objective_value;
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

namespace operations_research {
%pythoncode {

import types;

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
      if isinstance(t, (int, long, float)):
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
      self.ClearObjective()
      self.SetMinimization()
      coeffs = {}
      offset = expr.Visit(coeffs)
      self.AddObjectiveOffset(offset)
      for v, c, in coeffs.iteritems():
        self.SetObjectiveCoefficient(v, float(c))

    def Maximize(self, expr):
      self.ClearObjective()
      self.SetMaximization()
      coeffs = {}
      offset = expr.Visit(coeffs)
      self.AddObjectiveOffset(offset)
      for v, c, in coeffs.iteritems():
        self.SetObjectiveCoefficient(v, float(c))
  }
}

} // namespace operations_research


#endif  // SWIGPYTHON

#ifdef SWIGJAVA

%module(directors="1") operations_research;

%typemap(javaimports) SWIGTYPE %{
import java.lang.reflect.*;
%}

namespace operations_research {
// Rename rules on MPVariable.
%rename (basisStatus) MPVariable::basis_status;
%rename (reducedCost) MPVariable::reduced_cost;
%rename (setBounds) MPVariable::SetBounds;
%rename (setInteger) MPVariable::SetInteger;
%rename (setLb) MPVariable::SetLB;
%rename (setUb) MPVariable::SetUB;
%rename (solutionValue) MPVariable::solution_value;

// Rename rules on MPConstraint.
%rename (basisStatus) MPConstraint::basis_status;
%rename (dualValue) MPConstraint::dual_value;
%rename (getCoefficient) MPConstraint::GetCoefficient;
%rename (setBounds) MPConstraint::SetBounds;
%rename (setCoefficient) MPConstraint::SetCoefficient;
%rename (setLb) MPConstraint::SetLB;
%rename (setUb) MPConstraint::SetUB;
%rename (setIsLazy) MPConstraint::set_is_lazy;
%rename (isLazy) MPConstraint::is_lazy;

// Rename rules on MPObjective.
%rename (addOffset) MPObjective::AddOffset;
%rename (bestBound) MPObjective::BestBound;
%rename (clear) MPObjective::Clear;
%rename (getCoefficient) MPObjective::GetCoefficient;
%rename (setCoefficient) MPObjective::SetCoefficient;
%rename (setMaximization) MPObjective::SetMaximization;
%rename (setMinimization) MPObjective::SetMinimization;
%rename (setOffset) MPObjective::SetOffset;
%rename (setOptimizationDirection) MPObjective::SetOptimizationDirection;
%rename (value) MPObjective::Value;

// Rename rules on MPSolverParameters.
%rename (getDoubleParam) MPSolverParameters::GetDoubleParam;
%rename (getIntegerParam) MPSolverParameters::GetIntegerParam;
%rename (reset) MPSolverParameters::Reset;
%rename (resetDoubleParam) MPSolverParameters::ResetDoubleParam;
%rename (resetIntegerParam) MPSolverParameters::ResetIntegerParam;
%rename (setDoubleParam) MPSolverParameters::SetDoubleParam;
%rename (setIntegerParam) MPSolverParameters::SetIntegerParam;

// Rename rules on MPSolver.
%rename (addObjectiveOffset) MPSolver::AddObjectiveOffset;
%rename (bestObjectiveBound) MPSolver::best_objective_bound;
%rename (checkAllNamesValidity) MPSolver::CheckAllNamesValidity;
%rename (checkNameValidity) MPSolver::CheckNameValidity;
%rename (clear) MPSolver::Clear;
%rename (clearObjective) MPSolver::ClearObjective;
%rename (computeExactConditionNumber) MPSolver::ComputeExactConditionNumber;
%rename (loadModel) MPSolver::LoadModel;
%rename (lookupVariableOrNull) MPSolver::LookupVariableOrNull;
%rename (lookupConstraintOrNull) MPSolver::LookupConstraintOrNull;
%rename (makeBoolVar) MPSolver::MakeBoolVar;
%rename (makeIntVar) MPSolver::MakeIntVar;
%rename (makeNumVar) MPSolver::MakeNumVar;
%rename (makeConstraint) MPSolver::MakeRowConstraint;
%rename (makeVar) MPSolver::MakeVar;
%rename (minimization) MPSolver::Minimization;
%rename (numConstraints) MPSolver::NumConstraints;
%rename (numVariables) MPSolver::NumVariables;
%rename (objective) MPSolver::MutableObjective;
%rename (objectiveValue) MPSolver::objective_value;
%rename (reset) MPSolver::Reset;
%rename (setMaximization) MPSolver::SetMaximization;
%rename (setMinimization) MPSolver::SetMinimization;
%rename (setObjectiveCoefficient) MPSolver::SetObjectiveCoefficient;
%rename (setObjectiveOffset) MPSolver::SetObjectiveOffset;
%rename (setOptimizationDirection) MPSolver::SetOptimizationDirection;
%rename (setTimeLimit) MPSolver::set_time_limit;
%rename (setWriteModelFilename) MPSolver::set_write_model_filename;
%rename (solve) MPSolver::Solve;
%rename (solverVersion) MPSolver::SolverVersion;
%rename (suppressOutput) MPSolver::SuppressOutput;
%rename (timeLimit) MPSolver::time_limit;
%rename (wallTime) MPSolver::wall_time;
%rename (writeModelFilename) MPSolver::write_model_filename;
// Ignore non-mutable version of the objective accessor.
%ignore MPSolver::Objective;
// Ignore Make*VarArray: see replacement java code below.
%ignore MPSolver::MakeVarArray;
%ignore MPSolver::MakeNumVarArray;
%ignore MPSolver::MakeIntVarArray;
%ignore MPSolver::MakeBoolVarArray;
// The following 6 methods that use protocol buffers as output
// arguments are replaced by methods that return a protocol buffer,
// see code below.
%ignore MPSolver::ExportModelToNewProto;
%ignore MPSolver::FillSolutionResponseProto;
%ignore MPSolver::SolveWithProto;
%ignore MPSolver::ExportModel;
%ignore MPSolver::FillSolutionResponse;
%ignore MPSolver::SolveWithProtocolBuffers;
// Access to the underlying solver is available only in C++.
%ignore MPSolver::underlying_solver;


// Add java code on MPSolver.
%typemap(javacode) MPSolver %{
  public MPVariable[] makeVarArray(int count,
                                   double lb,
                                   double ub,
                                   boolean integer) {
    MPVariable[] array = new MPVariable[count];
    for (int i = 0; i < count; ++i) {
      array[i] = makeVar(lb, ub, integer, "");
    }
    return array;
  }

  public MPVariable[] makeVarArray(int count,
                                   double lb,
                                   double ub,
                                   boolean integer,
                                   String var_name) {
    MPVariable[] array = new MPVariable[count];
    for (int i = 0; i < count; ++i) {
      array[i] = makeVar(lb, ub, integer, var_name + i);
    }
    return array;
  }

  public MPVariable[] makeNumVarArray(int count, double lb, double ub) {
    return makeVarArray(count, lb, ub, false);
  }

  public MPVariable[] makeNumVarArray(int count,
                                      double lb,
                                      double ub,
                                      String var_name) {
    return makeVarArray(count, lb, ub, false, var_name);
  }

  public MPVariable[] makeIntVarArray(int count, double lb, double ub) {
    return makeVarArray(count, lb, ub, true);
  }

  public MPVariable[] makeIntVarArray(int count,
                                      double lb,
                                      double ub,
                                      String var_name) {
    return makeVarArray(count, lb, ub, true, var_name);
  }

  public MPVariable[] makeBoolVarArray(int count) {
    return makeVarArray(count, 0.0, 1.0, true);
  }

  public MPVariable[] makeBoolVarArray(int count, String var_name) {
    return makeVarArray(count, 0.0, 1.0, true, var_name);
  }

  public static final int getSolverEnum(String solver_name)
      throws java.lang.ClassNotFoundException,
      java.lang.NoSuchFieldException,
      java.lang.IllegalAccessException {
Class c = Class.forName("com.google.ortools.linearsolver.MPSolver");
    Field field = c.getField(solver_name);
    return field.getInt(null);
  }
%}

}  // namespace operations_research

#endif  // SWIGJAVA

#if defined(SWIGCSHARP)

%module(directors="1") operations_research;

%typemap(csimports) SWIGTYPE %{
  using System;
  using System.Collections.Generic;
  using System.Runtime.InteropServices;
%}

namespace operations_research {
// Use system infinity.
%ignore MPSolver::infinity;
// Automatic renaming to camel case.
%rename("%(camelcase)s", %$isfunction) "";
// Rename MakeRowConstraint into MakeConstraint
%rename (MakeConstraint) MPSolver::MakeRowConstraint;
// Rename classes, remove MP prefix.
%rename (Solver) MPSolver;
%rename (Variable) MPVariable;
%rename (Constraint) MPConstraint;
%rename (Objective) MPObjective;
%rename (SolverParameters) MPSolverParameters;

// Ignore code on MPSolver, see replacement java code below.
%ignore MPSolver::MakeVarArray;
%ignore MPSolver::MakeNumVarArray;
%ignore MPSolver::MakeIntVarArray;
%ignore MPSolver::MakeBoolVarArray;
// The following 3 methods that use protocol buffers as output
// arguments are replaced by methods that return a protocol buffer,
// see code below.
%ignore MPSolver::ExportModel;
%ignore MPSolver::ExportModelToNewProto;
%ignore MPSolver::FillSolutionResponse;
%ignore MPSolver::SolveWithProtocolBuffers;

%typemap(cscode) MPVariable %{
  public static LinearExpr operator+(Variable a, double v)
  {
    return new VarWrapper(a) + v;
  }

  public static LinearExpr operator+(double v, Variable a)
  {
    return a + v;
  }

  public static LinearExpr operator+(Variable a, LinearExpr b)
  {
    return new VarWrapper(a) + b;
  }

  public static LinearExpr operator+(Variable a, Variable b)
  {
    return new VarWrapper(a) + new VarWrapper(b);
  }

  public static LinearExpr operator+(LinearExpr a, Variable b)
  {
    return a + new VarWrapper(b);
  }

  public static LinearExpr operator-(Variable a, double v)
  {
    return new VarWrapper(a) - v;
  }

  public static LinearExpr operator-(double v, Variable a)
  {
    return v - new VarWrapper(a);
  }

  public static LinearExpr operator-(Variable a, LinearExpr b)
  {
    return new VarWrapper(a) - b;
  }

  public static LinearExpr operator-(LinearExpr a, Variable b)
  {
    return a - new VarWrapper(b);
  }

  public static LinearExpr operator-(Variable a, Variable b)
  {
    return new VarWrapper(a) - new VarWrapper(b);
  }

  public static LinearExpr operator-(Variable a)
  {
    return - new VarWrapper(a);
  }

  public static LinearExpr operator*(Variable a, double v)
  {
    return new VarWrapper(a) * v;
  }

  public static LinearExpr operator/(Variable a, double v)
  {
    return new VarWrapper(a) / v;
  }

  public static LinearExpr operator*(double v, Variable a)
  {
    return v * new VarWrapper(a);
  }

  public static RangeConstraint operator==(Variable a, double v)
  {
    return new VarWrapper(a) == v;
  }

  public static RangeConstraint operator==(double v, Variable a)
  {
    return v == new VarWrapper(a);
  }

  public static RangeConstraint operator!=(Variable a, double v)
  {
    return new VarWrapper(a) != v;
  }

  public static RangeConstraint operator!=(double v, Variable a)
  {
    return new VarWrapper(a) != v;
  }

  public static Equality operator==(Variable a, LinearExpr b)
  {
    return new VarWrapper(a) == b;
  }

  public static Equality operator==(LinearExpr a, Variable b)
  {
    return a == new VarWrapper(b);
  }

  public static VarEquality operator==(Variable a, Variable b)
  {
    return new VarEquality(a, b, true);
  }

  public static Equality operator!=(Variable a, LinearExpr b)
  {
    return new VarWrapper(a) != b;
  }

  public static Equality operator!=(LinearExpr a, Variable b)
  {
    return a != new VarWrapper(b);
  }

  public static VarEquality operator!=(Variable a, Variable b)
  {
    return new VarEquality(a, b, false);
  }

  public static RangeConstraint operator<=(Variable a, double v)
  {
    return new VarWrapper(a) <= v;
  }

  public static RangeConstraint operator>=(Variable a, double v)
  {
    return new VarWrapper(a) >= v;
  }

  public static RangeConstraint operator<=(double v, Variable a)
  {
    return new VarWrapper(a) >= v;
  }

  public static RangeConstraint operator>=(double v, Variable a)
  {
    return new VarWrapper(a) <= v;
  }

  public static RangeConstraint operator<=(Variable a, LinearExpr b)
  {
    return new VarWrapper(a) <= b;
  }

  public static RangeConstraint operator>=(Variable a, LinearExpr b)
  {
    return new VarWrapper(a) >= b;
  }

  public static RangeConstraint operator<=(Variable a, Variable b)
  {
    return new VarWrapper(a) <= new VarWrapper(b);
  }

  public static RangeConstraint operator>=(Variable a, Variable b)
  {
    return new VarWrapper(a) >= new VarWrapper(b);
  }

  public static RangeConstraint operator<=(LinearExpr a, Variable b)
  {
    return a <= new VarWrapper(b);
  }

  public static RangeConstraint operator>=(LinearExpr a, Variable b)
  {
    return a >= new VarWrapper(b);
  }
%}

// Add csharp code on Solver.
%typemap(cscode) MPSolver %{
  public Variable[] MakeVarArray(int count,
                                 double lb,
                                 double ub,
                                 bool integer) {
    Variable[] array = new Variable[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeVar(lb, ub, integer, "");
    }
    return array;
  }

  public Variable[] MakeVarArray(int count,
                                 double lb,
                                 double ub,
                                 bool integer,
                                 string var_name) {
    Variable[] array = new Variable[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeVar(lb, ub, integer, var_name + i);
    }
    return array;
  }

  public Variable[,] MakeVarMatrix(int rows,
                                   int cols,
                                   double lb,
                                   double ub,
                                   bool integer) {
    Variable[,] matrix = new Variable[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        matrix[i,j] = MakeVar(lb, ub, integer, "");
      }
    }
    return matrix;
  }

  public Variable[,] MakeVarMatrix(int rows,
                                   int cols,
                                   double lb,
                                   double ub,
                                   bool integer,
                                   string name) {
    Variable[,] matrix = new Variable[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        string var_name = name + "[" + i + ", " + j +"]";
        matrix[i,j] = MakeVar(lb, ub, integer, var_name);
      }
    }
    return matrix;
  }

  public Variable[] MakeNumVarArray(int count, double lb, double ub) {
    return MakeVarArray(count, lb, ub, false);
  }

  public Variable[] MakeNumVarArray(int count,
                                    double lb,
                                    double ub,
                                    string var_name) {
    return MakeVarArray(count, lb, ub, false, var_name);
  }

  public Variable[,] MakeNumVarMatrix(int rows,
                                      int cols,
                                      double lb,
                                      double ub) {
    Variable[,] matrix = new Variable[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        matrix[i,j] = MakeNumVar(lb, ub, "");
      }
    }
    return matrix;
  }

  public Variable[,] MakeNumVarMatrix(int rows,
                                      int cols,
                                      double lb,
                                      double ub,
                                      string name) {
    Variable[,] matrix = new Variable[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        string var_name = name + "[" + i + ", " + j +"]";
        matrix[i,j] = MakeNumVar(lb, ub, var_name);
      }
    }
    return matrix;
  }

  public Variable[] MakeIntVarArray(int count, double lb, double ub) {
    return MakeVarArray(count, lb, ub, true);
  }

  public Variable[] MakeIntVarArray(int count,
                                    double lb,
                                    double ub,
                                    string var_name) {
    return MakeVarArray(count, lb, ub, true, var_name);
  }

  public Variable[,] MakeIntVarMatrix(int rows,
                                      int cols,
                                      double lb,
                                      double ub) {
    Variable[,] matrix = new Variable[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        matrix[i,j] = MakeIntVar(lb, ub, "");
      }
    }
    return matrix;
  }

  public Variable[,] MakeIntVarMatrix(int rows,
                                      int cols,
                                      double lb,
                                      double ub,
                                      string name) {
    Variable[,] matrix = new Variable[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        string var_name = name + "[" + i + ", " + j +"]";
        matrix[i,j] = MakeIntVar(lb, ub, var_name);
      }
    }
    return matrix;
  }

  public Variable[] MakeBoolVarArray(int count) {
    return MakeVarArray(count, 0.0, 1.0, true);
  }

  public Variable[] MakeBoolVarArray(int count, string var_name) {
    return MakeVarArray(count, 0.0, 1.0, true, var_name);
  }

  public Variable[,] MakeBoolVarMatrix(int rows, int cols) {
    Variable[,] matrix = new Variable[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        matrix[i,j] = MakeBoolVar("");
      }
    }
    return matrix;
  }

  public Variable[,] MakeBoolVarMatrix(int rows, int cols, string name) {
    Variable[,] matrix = new Variable[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        string var_name = name + "[" + i + ", " + j +"]";
        matrix[i,j] = MakeBoolVar(var_name);
      }
    }
    return matrix;
  }

  public static int GetSolverEnum(String solverType) {
    System.Reflection.FieldInfo fieldInfo =
      typeof(Solver).GetField(solverType);
    if (fieldInfo != null) {
      return (int)fieldInfo.GetValue(null);
    } else {
      throw new System.ApplicationException("Solver not supported");
    }
  }

  public static Solver CreateSolver(String name, String type) {
    System.Reflection.FieldInfo fieldInfo =
        typeof(Solver).GetField(type);
    if (fieldInfo != null) {
      return new Solver(name, (int)fieldInfo.GetValue(null));
    } else {
      return null;
    }
  }

  public Constraint Add(LinearConstraint constraint) {
    return constraint.Extract(this);
  }

  public void Minimize(LinearExpr expr)
  {
    ClearObjective();
    SetMinimization();
    Dictionary<Variable, double> coefficients =
        new Dictionary<Variable, double>();
    double constant = expr.Visit(coefficients);
    foreach (KeyValuePair<Variable, double> pair in coefficients)
    {
      SetObjectiveCoefficient(pair.Key, pair.Value);
    }
    SetObjectiveOffset(constant);
  }

  public void Maximize(LinearExpr expr)
  {
    ClearObjective();
    SetMaximization();
    Dictionary<Variable, double> coefficients =
        new Dictionary<Variable, double>();
    double constant = expr.Visit(coefficients);
    foreach (KeyValuePair<Variable, double> pair in coefficients)
    {
      SetObjectiveCoefficient(pair.Key, pair.Value);
    }
    SetObjectiveOffset(constant);
  }

  public void Minimize(Variable var)
  {
    ClearObjective();
    SetMinimization();
    SetObjectiveCoefficient(var, 1.0);
  }

  public void Maximize(Variable var)
  {
    ClearObjective();
    SetMaximization();
    SetObjectiveCoefficient(var, 1.0);
  }
%}

}  // namespace operations_research

#endif  // SWIGCSHARP

// Wrap linear_solver includes
%include "linear_solver/linear_solver.h"
%include "linear_solver/linear_solver_ext.h"
