// Copyright 2010-2014 Google
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

// This .swig file exposes the linear programming and integer programming
//
// The python API is enriched by custom code defined here, making it
// extremely intuitive, like:
//   solver = pywraplp.Solver(pywraplp.Solver.LINEAR_PROGRAMMING_GLOP)
//   x1 = solver.NumVar(0.0, 1.0, 'x1')
//   x2 = solver.NumVar(-3.0, 2.0, 'x2')
//   c1 = solver.Add(2 * x1 - 3.2 * x2 + 1 <= 2.5)
//   solver.Maximize(10 * x1 + 6 * x2)
//
// USAGE EXAMPLES:
// - python/linear_programming.py
// - ./pywraplp_test.py
//
// TODO(user): test all the APIs that are currently marked as 'untested'.

%include base/base.swig


%{
#include "linear_solver/linear_solver2.pb.h"
#include "linear_solver/linear_solver.h"
using std::string;
%}

namespace operations_research {

%pythoncode {
from ortools.linear_solver.linear_solver_natural_api import LinearExpr
from ortools.linear_solver.linear_solver_natural_api import ProductCst
from ortools.linear_solver.linear_solver_natural_api import Sum
from ortools.linear_solver.linear_solver_natural_api import SumArray
from ortools.linear_solver.linear_solver_natural_api import SumCst
from ortools.linear_solver.linear_solver_natural_api import LinearConstraint
}  // %pythoncode

%extend MPVariable {
  string __str__() {
    return self->name();
  }
  string __repr__() {
    return self->name();
  }

  // Note(user): the 74 lines below are a pure duplication of code from
  // ../linear_solver_natural_api.py. Unfortunately it wasn't easy to fix. In
  // particular, SWIG's %define won't work because they get prepended with a
  // C-style comment "/*...*/" upon expansion, which breaks the python code.
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
  }  // %pythoncode
}

# See ../python/linear_programming.py for example on how to use the nice
# extended python API provided below.
%extend MPSolver {
  string ExportModelAsLpFormat(bool obfuscated) {
    string output;
    if (!self->ExportModelAsLpFormat(obfuscated, &output)) return "";
    return output;
  }

  string ExportModelAsMpsFormat(bool fixed_format, bool obfuscated) {
    string output;
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
    objective = self.Objective()
    objective.Clear()
    objective.SetMinimization()
    coeffs = {}
    offset = expr.Visit(coeffs)
    objective.AddOffset(offset)
    for v, c, in coeffs.iteritems():
      objective.SetCoefficient(v, float(c))

  def Maximize(self, expr):
    objective = self.Objective()
    objective.Clear()
    objective.SetMaximization()
    coeffs = {}
    offset = expr.Visit(coeffs)
    objective.AddOffset(offset)
    for v, c, in coeffs.iteritems():
      objective.SetCoefficient(v, float(c))
  }  // %pythoncode
}

%extend MPSolver {
  %pythoncode {
  @staticmethod
  def Infinity():
    return MPSolver.infinity()

  def SetTimeLimit(self, x):
    self.set_time_limit(x)
  }  // %pythoncode
}

%extend MPVariable {
  %pythoncode {
  def SolutionValue(self):
    return self.solution_value()

  def Integer(self):
    return self.integer()

  def Lb(self):
    return self.lb()

  def Ub(self):
    return self.ub()
  }  // %pythoncode
}

%extend MPConstraint {
  %pythoncode {
  def Lb(self):
    return self.lb()

  def Ub(self):
    return self.ub()

  def SetLb(self, x):
    self.SetLB(x)

  def SetUb(self, x):
    self.SetUB(x)
  }  // %pythoncode
}

%extend MPConstraint {
  %pythoncode {
  def Offset(self):
    return self.offset()
  }  // %pythoncode
}
}  // namespace operations_research


%ignoreall

%unignore operations_research;

// Strip the "MP" prefix from the exposed classes.
%rename (Solver) operations_research::MPSolver;
%rename (Solver) operations_research::MPSolver::MPSolver;
%rename (Constraint) operations_research::MPConstraint;
%rename (Variable) operations_research::MPVariable;
%rename (Objective) operations_research::MPObjective;

// Expose the MPSolver::OptimizationProblemType enum.
%unignore operations_research::MPSolver::OptimizationProblemType;
%unignore operations_research::MPSolver::GLOP_LINEAR_PROGRAMMING;
%unignore operations_research::MPSolver::CLP_LINEAR_PROGRAMMING;
%unignore operations_research::MPSolver::GLPK_LINEAR_PROGRAMMING;
%unignore operations_research::MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING;
%unignore operations_research::MPSolver::CBC_MIXED_INTEGER_PROGRAMMING;
%unignore operations_research::MPSolver::GLPK_MIXED_INTEGER_PROGRAMMING;
// These aren't unit tested, as they only run on machines with a Gurobi license.
%unignore operations_research::MPSolver::GUROBI_LINEAR_PROGRAMMING;
%unignore operations_research::MPSolver::GUROBI_MIXED_INTEGER_PROGRAMMING;
%unignore operations_research::MPSolver::SULUM_LINEAR_PROGRAMMING;
%unignore operations_research::MPSolver::SULUM_MIXED_INTEGER_PROGRAMMING;


// Expose the MPSolver::ResultStatus enum.
%unignore operations_research::MPSolver::ResultStatus;
%unignore operations_research::MPSolver::OPTIMAL;
%unignore operations_research::MPSolver::FEASIBLE;  // No unit test
%unignore operations_research::MPSolver::INFEASIBLE;
%unignore operations_research::MPSolver::UNBOUNDED;  // No unit test
%unignore operations_research::MPSolver::ABNORMAL;
%unignore operations_research::MPSolver::NOT_SOLVED;  // No unit test

// Expose the MPSolver's basic API, with some renames.
%rename (Objective) operations_research::MPSolver::MutableObjective;
%rename (BoolVar) operations_research::MPSolver::MakeBoolVar;  // No unit test
%rename (IntVar) operations_research::MPSolver::MakeIntVar;
%rename (NumVar) operations_research::MPSolver::MakeNumVar;
%rename (Constraint) operations_research::MPSolver::MakeRowConstraint;
%unignore operations_research::MPSolver::~MPSolver;
%unignore operations_research::MPSolver::Solve;
%unignore operations_research::MPSolver::VerifySolution;
%unignore operations_research::MPSolver::infinity;
%unignore operations_research::MPSolver::set_time_limit;  // No unit test


// Expose some of the more advanced MPSolver API.
%unignore operations_research::MPSolver::SupportsProblemType;  // No unit test
%unignore operations_research::MPSolver::wall_time;  // No unit test
%unignore operations_research::MPSolver::Clear;  // No unit test
%unignore operations_research::MPSolver::NumVariables;
%unignore operations_research::MPSolver::NumConstraints;
%unignore operations_research::MPSolver::EnableOutput;  // No unit test
%unignore operations_research::MPSolver::SuppressOutput;  // No unit test
%rename (LookupConstraint)
    operations_research::MPSolver::LookupConstraintOrNull;
%rename (LookupVariable) operations_research::MPSolver::LookupVariableOrNull;

// Expose very advanced parts of the MPSolver API. For expert users only.
%unignore operations_research::MPSolver::ComputeExactConditionNumber;
%unignore operations_research::MPSolver::nodes;
%unignore operations_research::MPSolver::iterations;  // No unit test
%unignore operations_research::MPSolver::BasisStatus;
%unignore operations_research::MPSolver::FREE;  // No unit test
%unignore operations_research::MPSolver::AT_LOWER_BOUND;
%unignore operations_research::MPSolver::AT_UPPER_BOUND;
%unignore operations_research::MPSolver::FIXED_VALUE;  // No unit test
%unignore operations_research::MPSolver::BASIC;

// MPVariable: reader API.
%unignore operations_research::MPVariable::solution_value;
%unignore operations_research::MPVariable::lb;  // No unit test
%unignore operations_research::MPVariable::ub;  // No unit test
%unignore operations_research::MPVariable::name;  // No unit test
%unignore operations_research::MPVariable::basis_status;
%unignore operations_research::MPVariable::reduced_cost;  // For experts only.

// MPConstraint: writer API.
%unignore operations_research::MPConstraint::SetCoefficient;
%unignore operations_research::MPConstraint::SetLB;
%unignore operations_research::MPConstraint::SetUB;
%unignore operations_research::MPConstraint::SetBounds;
%unignore operations_research::MPConstraint::set_is_lazy;

// MPConstraint: reader API.
%unignore operations_research::MPConstraint::GetCoefficient;
%unignore operations_research::MPConstraint::lb;
%unignore operations_research::MPConstraint::ub;
%unignore operations_research::MPConstraint::name;
%unignore operations_research::MPConstraint::activity;
%unignore operations_research::MPConstraint::basis_status;
%unignore operations_research::MPConstraint::dual_value;  // For experts only.

// MPObjective: writer API.
%unignore operations_research::MPObjective::SetCoefficient;
%unignore operations_research::MPObjective::SetMinimization;
%unignore operations_research::MPObjective::SetMaximization;
%unignore operations_research::MPObjective::SetOptimizationDirection;
%unignore operations_research::MPObjective::Clear;  // No unit test
%unignore operations_research::MPObjective::SetOffset;
%unignore operations_research::MPObjective::AddOffset;  // No unit test

// MPObjective: reader API.
%unignore operations_research::MPObjective::Value;
%unignore operations_research::MPObjective::GetCoefficient;
%unignore operations_research::MPObjective::minimization;
%unignore operations_research::MPObjective::maximization;
%unignore operations_research::MPObjective::offset;
%unignore operations_research::MPObjective::BestBound;

// MPSolverParameters API. For expert users only.
// TODO(user): also strip "MP" from the class name.
// TODO(user): unit test all of it.
%unignore operations_research::MPSolverParameters;
%unignore operations_research::MPSolverParameters::MPSolverParameters;
%unignore operations_research::MPSolverParameters::DoubleParam;
%unignore operations_research::MPSolverParameters::RELATIVE_MIP_GAP;
%unignore operations_research::MPSolverParameters::SetDoubleParam;
%unignore operations_research::MPSolverParameters::kDefaultPrimalTolerance;

// We want to ignore the CoeffMap class; but since it inherits from some
// hash_map<>, swig complains about an undefined base class. Silence it.
%warnfilter(401) CoeffMap;

%include "linear_solver/linear_solver.h"

%unignoreall
