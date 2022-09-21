// Copyright 2010-2022 Google LLC
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

%include "std_string.i"

%include "ortools/util/python/proto.i"

%import "ortools/util/python/vector.i"

// We need to forward-declare the proto here, so that the PROTO_* macros
// involving them work correctly. The order matters very much: this declaration
// needs to be before the %{ #include ".../linear_solver.h" %}.
namespace operations_research {
class MPModelProto;
class MPModelRequest;
class MPSolutionResponse;
}  // namespace operations_research

%{
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/model_exporter.h"
#include "ortools/linear_solver/model_exporter_swig_helper.h"
#include "ortools/linear_solver/model_validator.h"
%}

%pythoncode %{
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
%}  // %pythoncode

%extend operations_research::MPVariable {
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

%extend operations_research::MPSolver {
  // Change the API of LoadModelFromProto() to simply return the error message:
  // it will always be empty iff the model was valid.
  std::string LoadModelFromProto(const operations_research::MPModelProto& input_model) {
    std::string error_message;
    $self->LoadModelFromProto(input_model, &error_message);
    return error_message;
  }

  // Ditto for LoadModelFromProtoWithUniqueNamesOrDie()
  std::string LoadModelFromProtoWithUniqueNamesOrDie(const operations_research::MPModelProto& input_model) {
    std::string error_message;
    $self->LoadModelFromProtoWithUniqueNamesOrDie(input_model, &error_message);
    return error_message;
  }

  // Change the API of LoadSolutionFromProto() to simply return a boolean.
  bool LoadSolutionFromProto(
      const operations_research::MPSolutionResponse& response,
      double tolerance = std::numeric_limits<double>::infinity()) {
    const absl::Status status =
        $self->LoadSolutionFromProto(response, tolerance);
    LOG_IF(ERROR, !status.ok()) << "LoadSolutionFromProto() failed: " << status;
    return status.ok();
  }

  std::string ExportModelAsLpFormat(bool obfuscated) {
    operations_research::MPModelExportOptions options;
    options.obfuscate = obfuscated;
    operations_research::MPModelProto model;
    $self->ExportModelToProto(&model);
    return ExportModelAsLpFormat(model, options).value_or("");
  }

  std::string ExportModelAsMpsFormat(bool fixed_format, bool obfuscated) {
    operations_research::MPModelExportOptions options;
    options.obfuscate = obfuscated;
    operations_research::MPModelProto model;
    $self->ExportModelToProto(&model);
    return ExportModelAsMpsFormat(model, options).value_or("");
  }

  /// Set a hint for solution.
  ///
  /// If a feasible or almost-feasible solution to the problem is already known,
  /// it may be helpful to pass it to the solver so that it can be used. A
  /// solver that supports this feature will try to use this information to
  /// create its initial feasible solution.
  ///
  /// Note that it may not always be faster to give a hint like this to the
  /// solver. There is also no guarantee that the solver will use this hint or
  /// try to return a solution "close" to this assignment in case of multiple
  /// optimal solutions.
  void SetHint(const std::vector<operations_research::MPVariable*>& variables,
               const std::vector<double>& values) {
    if (variables.size() != values.size()) {
      LOG(FATAL) << "Different number of variables and values when setting "
                 << "hint.";
    }
    std::vector<std::pair<const operations_research::MPVariable*, double> >
        hint(variables.size());
    for (int i = 0; i < variables.size(); ++i) {
      hint[i] = std::make_pair(variables[i], values[i]);
    }
    $self->SetHint(hint);
  }

  /// Sets the number of threads to be used by the solver.
  bool SetNumThreads(int num_theads) {
    return $self->SetNumThreads(num_theads).ok();
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

// Catch runtime exceptions in class methods
%exception operations_research::MPSolver {
    try {
      $action
    } catch ( std::runtime_error& e ) {
      SWIG_exception(SWIG_RuntimeError, e.what());
    }
  }


  static double Infinity() { return operations_research::MPSolver::infinity(); }
  void SetTimeLimit(int64_t x) { $self->set_time_limit(x); }
  int64_t WallTime() const { return $self->wall_time(); }
  int64_t Iterations() const { return $self->iterations(); }
}  // extend operations_research::MPSolver

%extend operations_research::MPVariable {
  double SolutionValue() const { return $self->solution_value(); }
  bool Integer() const { return $self->integer(); }
  double Lb() const { return $self->lb(); }
  double Ub() const { return $self->ub(); }
  void SetLb(double x) { $self->SetLB(x); }
  void SetUb(double x) { $self->SetUB(x); }
  double ReducedCost() const { return $self->reduced_cost(); }
}  // extend operations_research::MPVariable

%extend operations_research::MPConstraint {
  double Lb() const { return $self->lb(); }
  double Ub() const { return $self->ub(); }
  void SetLb(double x) { $self->SetLB(x); }
  void SetUb(double x) { $self->SetUB(x); }
  double DualValue() const { return $self->dual_value(); }
}  // extend operations_research::MPConstraint

%extend operations_research::MPObjective {
  double Offset() const { return $self->offset();}
}  // extend operations_research::MPObjective

PY_PROTO_TYPEMAP(ortools.linear_solver.linear_solver_pb2,
                 MPModelProto,
                 operations_research::MPModelProto);

PY_PROTO_TYPEMAP(ortools.linear_solver.linear_solver_pb2,
                 MPModelRequest,
                 operations_research::MPModelRequest);

PY_PROTO_TYPEMAP(ortools.linear_solver.linear_solver_pb2,
                 MPSolutionResponse,
                 operations_research::MPSolutionResponse);

// Actual conversions. This also includes the conversion to std::vector<Class>.
PY_CONVERT_HELPER_PTR(MPConstraint);
PY_CONVERT(MPConstraint);

PY_CONVERT_HELPER_PTR(MPVariable);
PY_CONVERT(MPVariable);

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
%unignore operations_research::MPSolver::CLP_LINEAR_PROGRAMMING;
%unignore operations_research::MPSolver::GLOP_LINEAR_PROGRAMMING;
%unignore operations_research::MPSolver::GLPK_LINEAR_PROGRAMMING;
%unignore operations_research::MPSolver::PDLP_LINEAR_PROGRAMMING;
%unignore operations_research::MPSolver::CBC_MIXED_INTEGER_PROGRAMMING;
%unignore operations_research::MPSolver::GLPK_MIXED_INTEGER_PROGRAMMING;
%unignore operations_research::MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING;
%unignore operations_research::MPSolver::BOP_INTEGER_PROGRAMMING;
%unignore operations_research::MPSolver::SAT_INTEGER_PROGRAMMING;
// These aren't unit tested, as they only run on machines with a Gurobi license.
%unignore operations_research::MPSolver::GUROBI_LINEAR_PROGRAMMING;
%unignore operations_research::MPSolver::GUROBI_MIXED_INTEGER_PROGRAMMING;
%unignore operations_research::MPSolver::CPLEX_LINEAR_PROGRAMMING;
%unignore operations_research::MPSolver::CPLEX_MIXED_INTEGER_PROGRAMMING;
%unignore operations_research::MPSolver::XPRESS_LINEAR_PROGRAMMING;
%unignore operations_research::MPSolver::XPRESS_MIXED_INTEGER_PROGRAMMING;


// Expose the MPSolver::ResultStatus enum.
%unignore operations_research::MPSolver::ResultStatus;
%unignore operations_research::MPSolver::OPTIMAL;
%unignore operations_research::MPSolver::FEASIBLE;  // No unit test
%unignore operations_research::MPSolver::INFEASIBLE;
%unignore operations_research::MPSolver::UNBOUNDED;  // No unit test
%unignore operations_research::MPSolver::ABNORMAL;
%unignore operations_research::MPSolver::MODEL_INVALID;  // No unit test
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
%newobject operations_research::MPSolver::CreateSolver;
%unignore operations_research::MPSolver::CreateSolver;
%unignore operations_research::MPSolver::ParseAndCheckSupportForProblemType;

%unignore operations_research::MPSolver::Solve;
%unignore operations_research::MPSolver::VerifySolution;
%unignore operations_research::MPSolver::infinity;
%unignore operations_research::MPSolver::set_time_limit;  // No unit test

// Proto-based API of the MPSolver. Use is encouraged.
%unignore operations_research::MPSolver::SolveWithProto;
%unignore operations_research::MPSolver::ExportModelToProto;
%unignore operations_research::MPSolver::FillSolutionResponseProto;
// LoadModelFromProto() is also visible: it's overridden by an %extend, above.
// LoadSolutionFromProto() is also visible: it's overridden by an %extend, above.

// Expose some of the more advanced MPSolver API.
%unignore operations_research::MPSolver::InterruptSolve;
%unignore operations_research::MPSolver::SupportsProblemType;  // No unit test
%unignore operations_research::MPSolver::wall_time;  // No unit test
%unignore operations_research::MPSolver::Clear;  // No unit test
%unignore operations_research::MPSolver::constraint;
%unignore operations_research::MPSolver::constraints;
%unignore operations_research::MPSolver::variable;
%unignore operations_research::MPSolver::variables;
%unignore operations_research::MPSolver::NumConstraints;
%unignore operations_research::MPSolver::NumVariables;
%unignore operations_research::MPSolver::EnableOutput;  // No unit test
%unignore operations_research::MPSolver::SuppressOutput;  // No unit test
%rename (LookupConstraint)
    operations_research::MPSolver::LookupConstraintOrNull;
%rename (LookupVariable) operations_research::MPSolver::LookupVariableOrNull;
%unignore operations_research::MPSolver::SetSolverSpecificParametersAsString;
%unignore operations_research::MPSolver::NextSolution;
// ExportModelAsLpFormat() is also visible: it's overridden by an %extend, above.
// ExportModelAsMpsFormat() is also visible: it's overridden by an %extend, above.

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

// MPVariable: writer API.
%unignore operations_research::MPVariable::SetLb;
%unignore operations_research::MPVariable::SetUb;
%unignore operations_research::MPVariable::SetBounds;
%unignore operations_research::MPVariable::SetInteger;
%unignore operations_research::MPVariable::SetBranchingPriority;

// MPVariable: reader API.
%unignore operations_research::MPVariable::solution_value;
%unignore operations_research::MPVariable::lb;
%unignore operations_research::MPVariable::ub;
%unignore operations_research::MPVariable::integer;  // No unit test
%unignore operations_research::MPVariable::name;  // No unit test
%unignore operations_research::MPVariable::index;  // No unit test
%unignore operations_research::MPVariable::basis_status;
%unignore operations_research::MPVariable::reduced_cost;  // For experts only.
%unignore operations_research::MPVariable::branching_priority;  // no unit test.

// MPConstraint: writer API.
%unignore operations_research::MPConstraint::SetCoefficient;
%unignore operations_research::MPConstraint::SetLb;
%unignore operations_research::MPConstraint::SetUb;
%unignore operations_research::MPConstraint::SetBounds;
%unignore operations_research::MPConstraint::set_is_lazy;
%unignore operations_research::MPConstraint::Clear;  // No unit test

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
%unignore operations_research::MPObjective::Offset;
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

// Expose the model exporters.
%rename (ModelExportOptions) operations_research::MPModelExportOptions;
%rename (ModelExportOptions) operations_research::MPModelExportOptions::MPModelExportOptions;
%rename (ExportModelAsLpFormat) operations_research::ExportModelAsLpFormatReturnString;
%rename (ExportModelAsMpsFormat) operations_research::ExportModelAsMpsFormatReturnString;

// Expose the model validator.
%rename (FindErrorInModelProto) operations_research::FindErrorInMPModelProto;

%include "ortools/linear_solver/linear_solver.h"
%include "ortools/linear_solver/model_exporter.h"
%include "ortools/linear_solver/model_exporter_swig_helper.h"

namespace operations_research {
  std::string FindErrorInMPModelProto(
      const operations_research::MPModelProto& input_model);
}  // namespace operations_research

%unignoreall

%pythoncode {
def setup_variable_operator(opname):
  setattr(Variable, opname,
          lambda self, *args: getattr(VariableExpr(self), opname)(*args))
for opname in LinearExpr.OVERRIDDEN_OPERATOR_METHODS:
  setup_variable_operator(opname)
}  // %pythoncode
