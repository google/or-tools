// Copyright 2010-2011 Google
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

// Include the file we want to wrap a first time.
%{
#include "linear_solver/linear_solver.h"
#include "linear_solver/linear_solver.pb.h"
%}

#ifdef SWIGPYTHON

// Define the renaming of methods.
%rename (BoolVar) MakeBoolVar;
%rename (IntVar) MakeIntVar;
%rename (NumVar) MakeNumVar;
%rename (Constraint) MPConstraint;
%rename (Constraint) MakeRowConstraint;
%rename (Solver) MPSolver;
%rename (Variable) MPVariable;
%ignore MakeVarArray;
%ignore MakeNumVarArray;
%ignore MakeIntVarArray;
%ignore MakeBoolVarArray;
// Access to the underlying solver is available only in C++.
%ignore underlying_solver;

namespace operations_research {
%pythoncode {

# The classes below allow linear expressions to be expressed naturally with the
# usual arithmetic operators +-*/ and with constant numbers, which makes the
# python API very intuitive. See RunLinearExampleNaturalLanguageAPI() in
# ../python/linear_programming.py for an example.
class LinearExpr(object):
  def DoVisit(self, coeffs, multiplier):
    raise NotImplementedError

  def Visit(self, coeffs):
    return self.DoVisit(coeffs, 1.0)

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
      return 'true inequality'

  def Extract(self, solver):
    coeffs = {}
    constant = self.__expr.Visit(coeffs)
    lb = -solver.infinity()
    ub = solver.infinity()
    if self.__lb > -1e308:
      lb = self.__lb - constant
    if self.__ub < 1e308:
      ub = self.__ub - constant

    constraint = solver.RowConstraint(lb, ub)
    for v, c, in coeffs.iteritems():
      constraint.SetCoefficient(v, float(c))
    return constraint
}

# We also apply the same idea to MPVariable.
%extend MPVariable {
  string __str__() {
    return self->name();
  }
  string __repr__() {
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
  %pythoncode {
    def Add(self, constraint):
      return constraint.Extract(self)

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
%rename (setBounds) MPConstraint::SetBounds;
%rename (setCoefficient) MPConstraint::SetCoefficient;
%rename (setLb) MPConstraint::SetLB;
%rename (setUb) MPConstraint::SetUB;

// Rename rules on MPObjective.
%rename (addOffset) MPObjective::AddOffset;
%rename (clear) MPObjective::Clear;
%rename (setCoefficient) MPObjective::SetCoefficient;
%rename (setOffset) MPObjective::SetOffset;

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
%rename (makeBoolVar) MPSolver::MakeBoolVar;
%rename (makeIntVar) MPSolver::MakeIntVar;
%rename (makeNumVar) MPSolver::MakeNumVar;
%rename (makeConstraint) MPSolver::MakeRowConstraint;
%rename (makeVar) MPSolver::MakeVar;
%rename (minimization) MPSolver::Minimization;
%rename (numConstraints) MPSolver::NumConstraints;
%rename (numVariables) MPSolver::NumVariables;
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
// Ignore code on MPSolver, see replacement java code below.
%ignore MPSolver::MakeVarArray;
%ignore MPSolver::MakeNumVarArray;
%ignore MPSolver::MakeIntVarArray;
%ignore MPSolver::MakeBoolVarArray;
// The following 3 methods that use protocol buffers as output
// arguments are replaced by methods that return a protocol buffer,
// see code below.
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

// Wrap linear_solver includes
%include "linear_solver/linear_solver.h"
