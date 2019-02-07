// Copyright 2010-2018 Google LLC
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

// This .i file exposes the linear programming and integer programming
//
// The python API is enriched by custom code defined here, making it
// extremely intuitive, like:
//   solver = pywraplp.Solver(
//       'Example Solver', pywraplp.Solver.GLOP_LINEAR_PROGRAMMING)
//   x1 = solver.NumVar(0.0, 1.0, 'x1')
//   x2 = solver.NumVar(-3.0, 2.0, 'x2')
//   c1 = solver.Add(2 * x1 - 3.2 * x2 + 1 <= 2.5)
//   solver.Maximize(10 * x1 + 6 * x2)
//
// USAGE EXAMPLES:
// - examples/python/linear_programming.py
// - ./pywraplp_test.py
//
// TODO(user): test all the APIs that are currently marked as 'untested'.

%include "ortools/base/base.i"
%include "ortools/base/optimization_suites.h"
%include "std_vector.i"

// We need to forward-declare the proto here, so that the PROTO_* macros
// involving them work correctly. The order matters very much: this declaration
// needs to be before the %{ #include ".../linear_solver.h" %}.
namespace operations_research {
class MPModelProto;
class MPModelRequest;
class MPSolutionResponse;
}  // namespace operations_research

namespace std {
%template(Vector) vector < double >;
%template(Matrix)  vector < vector < double > >;
}

%{
#include "ortools/linear_solver/linear_solver.h"
%}

namespace operations_research {

%pythoncode {
import numbers
from ortools.linear_solver.linear_solver_natural_api import OFFSET_KEY
from ortools.linear_solver.linear_solver_natural_api import inf
from ortools.linear_solver.linear_solver_natural_api import LinearExpr
from ortools.linear_solver.linear_solver_natural_api import ProductCst
from ortools.linear_solver.linear_solver_natural_api import Sum
from ortools.linear_solver.linear_solver_natural_api import SumArray
from ortools.linear_solver.linear_solver_natural_api import SumCst
from ortools.linear_solver.linear_solver_natural_api import LinearConstraint
from ortools.linear_solver.linear_solver_natural_api import VariableExpr
}  // %pythoncode

%extend MPVariable {
  std::string __str__() {
    return $self->name();
  }
  std::string __repr__() {
    return $self->name();
  }

  %pythoncode {
  def __getattr__(self, name):
    return getattr(VariableExpr(self), name)
  }  // %pythoncode
}

%extend MPSolver {
  // Change a (bool, std::string*) outputs to a python std::string (empty if bool=false).
  std::string ExportModelAsLpFormat(bool obfuscated) {
    std::string output;
    if (!$self->ExportModelAsLpFormat(obfuscated, &output)) return "";
    return output;
  }
  std::string ExportModelAsMpsFormat(bool fixed_format, bool obfuscated) {
    std::string output;
    if (!$self->ExportModelAsMpsFormat(fixed_format, obfuscated, &output)) {
      return "";
    }
    return output;
  }

  // Change the API of LoadModelFromProto() to simply return the error message:
  // it will always be empty iff the model was valid.
  std::string LoadModelFromProto(const MPModelProto& input_model) {
    std::string error_message;
    $self->LoadModelFromProto(input_model, &error_message);
    return error_message;
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

  def RowConstraint(self, *args):
    return self.Constraint(*args)

  def Minimize(self, expr):
    objective = self.Objective()
    objective.Clear()
    objective.SetMinimization()
    if isinstance(expr, numbers.Number):
        objective.SetOffset(expr)
    else:
        coeffs = expr.GetCoeffs()
        objective.SetOffset(coeffs.pop(OFFSET_KEY, 0.0))
        for v, c, in list(coeffs.items()):
          objective.SetCoefficient(v, float(c))

  def Maximize(self, expr):
    objective = self.Objective()
    objective.Clear()
    objective.SetMaximization()
    if isinstance(expr, numbers.Number):
        objective.SetOffset(expr)
    else:
        coeffs = expr.GetCoeffs()
        objective.SetOffset(coeffs.pop(OFFSET_KEY, 0.0))
        for v, c, in list(coeffs.items()):
          objective.SetCoefficient(v, float(c))
  }  // %pythoncode
}

%extend MPSolver {
// Catch runtime exceptions in class methods
%exception MPSolver {
    try {
      $action
    } catch ( std::runtime_error& e ) {
      SWIG_exception(SWIG_RuntimeError, e.what());
    }
  }

  static double Infinity() { return operations_research::MPSolver::infinity(); }
  void SetTimeLimit(int64 x) { $self->set_time_limit(x); }
  int64 WallTime() const { return $self->wall_time(); }
  int64 Iterations() const { return $self->iterations(); }
}  // extend MPSolver

%extend MPVariable {
  double SolutionValue() const { return $self->solution_value(); }
  bool Integer() const { return $self->integer(); }
  double Lb() const { return $self->lb(); }
  double Ub() const { return $self->ub(); }
  void SetLb(double x) { $self->SetLB(x); }
  void SetUb(double x) { $self->SetUB(x); }
  double ReducedCost() const { return $self->reduced_cost(); }
}  // extend MPVariable

%extend MPConstraint {
  double Lb() const { return $self->lb(); }
  double Ub() const { return $self->ub(); }
  void SetLb(double x) { $self->SetLB(x); }
  void SetUb(double x) { $self->SetUB(x); }
  double DualValue() const { return $self->dual_value(); }
}  // extend MPConstraint

%extend MPObjective {
  double Offset() const { return $self->offset();}
}  // extend MPObjective
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
%unignore operations_research::MPSolver::BOP_INTEGER_PROGRAMMING;
%unignore operations_research::MPSolver::SAT_INTEGER_PROGRAMMING;
// These aren't unit tested, as they only run on machines with a Gurobi license.
%unignore operations_research::MPSolver::GUROBI_LINEAR_PROGRAMMING;
%unignore operations_research::MPSolver::GUROBI_MIXED_INTEGER_PROGRAMMING;
%unignore operations_research::MPSolver::CPLEX_LINEAR_PROGRAMMING;
%unignore operations_research::MPSolver::CPLEX_MIXED_INTEGER_PROGRAMMING;

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
%rename (Var) operations_research::MPSolver::MakeVar;
// We intentionally don't expose MakeRowConstraint(LinearExpr), because this
// "natural language" API is specific to C++: other languages may add their own
// syntactic sugar on top of MPSolver instead of this.
%rename (Constraint) operations_research::MPSolver::MakeRowConstraint(double, double);
%rename (Constraint) operations_research::MPSolver::MakeRowConstraint();
%rename (Constraint) operations_research::MPSolver::MakeRowConstraint(double, double, const std::string&);
%rename (Constraint) operations_research::MPSolver::MakeRowConstraint(const std::string&);

%unignore operations_research::MPSolver::~MPSolver;
%unignore operations_research::MPSolver::Solve;
%unignore operations_research::MPSolver::VerifySolution;
%unignore operations_research::MPSolver::infinity;
%unignore operations_research::MPSolver::set_time_limit;  // No unit test


// Expose some of the more advanced MPSolver API.
%unignore operations_research::MPSolver::InterruptSolve;
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
%unignore operations_research::MPSolver::SetSolverSpecificParametersAsString;
%unignore operations_research::MPSolver::NextSolution;

// Expose very advanced parts of the MPSolver API. For expert users only.
%unignore operations_research::MPSolver::ComputeConstraintActivities;
%unignore operations_research::MPSolver::ComputeExactConditionNumber;
%unignore operations_research::MPSolver::nodes;
%unignore operations_research::MPSolver::iterations;  // No unit test
%unignore operations_research::MPSolver::BasisStatus;
%unignore operations_research::MPSolver::FREE;  // No unit test
%unignore operations_research::MPSolver::AT_LOWER_BOUND;
%unignore operations_research::MPSolver::AT_UPPER_BOUND;
%unignore operations_research::MPSolver::FIXED_VALUE;  // No unit test
%unignore operations_research::MPSolver::BASIC;

#ifdef MIP_SOLVER_WITH_SOS_CONSTRAINTS 
%define DOCSTRING
 " 
this class provides a solver optimizing an objective function containing 
a piecewise linear function as well as continuous variables.

Problem definition:
We have a piecewise linear (discrete) function given in tabular form with the relation

(1)     y_i = f(x_i), i = 1..k

Here x_i are m-dimensional real vectors i.e. x_i b.t. R(m), y_i are real scalar values i.e. y_i b.t. R

We would like to find the maximum of the following expression

(2)     f(x) + c^{T} * x + b^{T} * z

Here the subscript T denotes a matrix transpose operation. All quantities with small letters 
denote column vectors and all quantities with capital letters will denote matrices 
unless explicitly stated otherwise. The operator '*' denotes matrix multiplication.

The objective function (2) is subject to the following constraints:

(3)     A * x + B * z <= d
(4)     z >= 0

Here z b.t. R(l) is a l-dimensional vector of continuous variables while x_i is a vector of given values.
The number of individual constraints in (3) is p so the matrix A b.t. R(p,k), the matrix B b.t. R(p,l), 
and the vector d b.t. R(p).

After change of variables the problem given with (2)-(4) is transformed into

(5)    sum_{i=1}^{k} lambda_i * y_i + c^{T}*( sum_{i=1}^k lambda_i * x_i ) + b^{T} * z

subject to the system of constraints:

(6)    A * (sum_{i=1}^k lambda_i * x_i) + B * z <= d
(7)    sum_{i=1}^{k} lambda_i = 1
(8)    z >= 0

Here lambda_i, i = 1..k are the new set of binary variables where at most one of them can be 1. The goal is
to find such set of binary variables given with the vector lambda b.t. R(k) and continuous variables 
z b.t. R(l) such that the maximum of (5) is attained subject to constraints (6)-(8).

The dimensionality of the various parameters and variables in the PWL Solver is summarized below:

x b.t. R(m), y b.t. R, c b.t. R(l), z b.t. R(l)
A b.t. R(p,m), B b.t. R(p,l), d b.t. R(p)

It is useful to rewrite (5)-(8) in a more concise form where (5) becomes

(9)    maximize a * lambda + b * z

Here we have made the substiution 

(10)   a = y + c * x, a b.t. R

In (6) we make the substitution 

(11)   C = A * X, C b.t. R(p,k)

where X is a matrix with m rows and k columns composed of all x_i, i=1..k each represented as a column in X.

Then (6) becomes

(12)   C * lambda + B * z <= d

(13)   sum_{i=1}^{k} = 1

(14)   z >= 0
 "
%enddef

%feature("docstring", DOCSTRING) operations_research::PWLSolver;
%rename (PwlSolver) operations_research::PWLSolver;
%rename (PwlSolver) operations_research::PWLSolver::PWLSolver;
%rename (SosConstraint) operations_research::SOSConstraint;
%rename (NumSosConstraints) operations_research::MPSolver::NumSOSConstraints; 
%rename (SosConstraint) operations_research::MPSolver::MakeSOSConstraint(const SOSType);
%rename (SosConstraint) operations_research::MPSolver::MakeSOSConstraint(const std::string&,
                                                                         const SOSType);
%rename (NumOfXPoints) operations_research::PWLSolver::numb_of_x_points();
%rename (DimOfXPoint) operations_research::PWLSolver::dim_of_x_point();
%rename (NumConstraints) operations_research::PWLSolver::numb_of_constr();
%rename (NumRealVariables) operations_research::PWLSolver::numb_of_real_vars();
%rename (NumVariables) operations_research::PWLSolver::numb_of_vars();
%rename (Objective) operations_research::PWLSolver::objective;
%rename (GetVariable) operations_research::PWLSolver::GetVariableOrNull;
%rename (LookupSosConstraint)
         operations_research::MPSolver::LookupSOSConstraintOrNull;

%unignore operations_research::PWLSolver::OptimizationSuite;
%unignore operations_research::PWLSolver::SCIP;
%unignore operations_research::PWLSolver::CBC;
%unignore operations_research::PWLSolver::GLPK;
%unignore operations_research::PWLSolver::GUROBI;
%unignore operations_research::PWLSolver::CPLEX;

%unignore operations_research::PWLSolver::VectorParameterType;
%unignore operations_research::PWLSolver::bVector;
%unignore operations_research::PWLSolver::cVector;
%unignore operations_research::PWLSolver::dVector;

%unignore operations_research::PWLSolver::MatrixParameterType;
%unignore operations_research::PWLSolver::AMatrix;
%unignore operations_research::PWLSolver::BMatrix;

// Expose the PWLSolver's basic API.
%unignore operations_research::PWLSolver::~PWLSolver;
%unignore operations_research::PWLSolver::Solve;
%unignore operations_research::PWLSolver::VerifySolution;
%unignore operations_research::PWLSolver::infinity;
%unignore operations_research::PWLSolver::set_time_limit;  // No unit test
%unignore operations_research::PWLSolver::wall_time;
%unignore operations_research::PWLSolver::nodes;
%unignore operations_research::PWLSolver::SetXValues;
%unignore operations_research::PWLSolver::SetYValues;
%unignore operations_research::PWLSolver::SetParameter;
#ifndef NDEBUG
%unignore operations_research::PWLSolver::PrintXValues;
%unignore operations_research::PWLSolver::PrintYValues;
%unignore operations_research::PWLSolver::PrintAValues;
%unignore operations_research::PWLSolver::PrintBValues;
%unignore operations_research::PWLSolver::PrintbValues;
%unignore operations_research::PWLSolver::PrintcValues;
%unignore operations_research::PWLSolver::PrintdValues;
#endif

// Expose some of the more advanced PWLSolver API.
%unignore operations_research::PWLSolver::Clear;  // No unit test
%unignore operations_research::PWLSolver::NextSolution;

%unignore operations_research::MPSolver::SOSType;
%unignore operations_research::MPSolver::SOS1;
%unignore operations_research::MPSolver::SOS2;

// SOSConstraint: writer API.
%unignore operations_research::SOSConstraint::SetCoefficient;

// SOSConstraint: reader API.
%unignore operations_research::SOSConstraint::GetCoefficient;
%unignore operations_research::SOSConstraint::GetSOSType;
%unignore operations_research::SOSConstraint::name;
%unignore operations_research::SOSConstraint::index;
#endif

// MPVariable: reader API.
%unignore operations_research::MPVariable::solution_value;
%unignore operations_research::MPVariable::lb;  // No unit test
%unignore operations_research::MPVariable::ub;  // No unit test
%unignore operations_research::MPVariable::integer;  // No unit test
%unignore operations_research::MPVariable::name;  // No unit test
%unignore operations_research::MPVariable::index;  // No unit test
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
%unignore operations_research::MPConstraint::index;
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
%unignore operations_research::MPSolverParameters;
%unignore operations_research::MPSolverParameters::MPSolverParameters;

// Expose the MPSolverParameters::DoubleParam enum.
%unignore operations_research::MPSolverParameters::DoubleParam;
%unignore operations_research::MPSolverParameters::RELATIVE_MIP_GAP;
%unignore operations_research::MPSolverParameters::PRIMAL_TOLERANCE;
%unignore operations_research::MPSolverParameters::DUAL_TOLERANCE;
%unignore operations_research::MPSolverParameters::GetDoubleParam;
%unignore operations_research::MPSolverParameters::SetDoubleParam;
%unignore operations_research::MPSolverParameters::kDefaultRelativeMipGap;
%unignore operations_research::MPSolverParameters::kDefaultPrimalTolerance;
%unignore operations_research::MPSolverParameters::kDefaultDualTolerance;
// TODO(user): unit test kDefaultPrimalTolerance.

// Expose the MPSolverParameters::IntegerParam enum.
%unignore operations_research::MPSolverParameters::IntegerParam;
%unignore operations_research::MPSolverParameters::PRESOLVE;
%unignore operations_research::MPSolverParameters::LP_ALGORITHM;
%unignore operations_research::MPSolverParameters::INCREMENTALITY;
%unignore operations_research::MPSolverParameters::SCALING;
%unignore operations_research::MPSolverParameters::GetIntegerParam;
%unignore operations_research::MPSolverParameters::SetIntegerParam;
%unignore operations_research::MPSolverParameters::RELATIVE_MIP_GAP;
%unignore operations_research::MPSolverParameters::kDefaultPrimalTolerance;
// TODO(user): unit test kDefaultPrimalTolerance.

// Expose the MPSolverParameters::PresolveValues enum.
%unignore operations_research::MPSolverParameters::PresolveValues;
%unignore operations_research::MPSolverParameters::PRESOLVE_OFF;
%unignore operations_research::MPSolverParameters::PRESOLVE_ON;
%unignore operations_research::MPSolverParameters::kDefaultPresolve;

// Expose the MPSolverParameters::LpAlgorithmValues enum.
%unignore operations_research::MPSolverParameters::LpAlgorithmValues;
%unignore operations_research::MPSolverParameters::DUAL;
%unignore operations_research::MPSolverParameters::PRIMAL;
%unignore operations_research::MPSolverParameters::BARRIER;

// Expose the MPSolverParameters::IncrementalityValues enum.
%unignore operations_research::MPSolverParameters::IncrementalityValues;
%unignore operations_research::MPSolverParameters::INCREMENTALITY_OFF;
%unignore operations_research::MPSolverParameters::INCREMENTALITY_ON;
%unignore operations_research::MPSolverParameters::kDefaultIncrementality;

// Expose the MPSolverParameters::ScalingValues enum.
%unignore operations_research::MPSolverParameters::ScalingValues;
%unignore operations_research::MPSolverParameters::SCALING_OFF;
%unignore operations_research::MPSolverParameters::SCALING_ON;

%include "ortools/linear_solver/linear_solver.h"

%unignoreall

%pythoncode {
def setup_variable_operator(opname):
  setattr(Variable, opname,
          lambda self, *args: getattr(VariableExpr(self), opname)(*args))
for opname in LinearExpr.OVERRIDDEN_OPERATOR_METHODS:
  setup_variable_operator(opname)
}  // %pythoncode
