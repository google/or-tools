// Copyright 2010-2021 Google LLC
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
// a java codelab yet, as of July 2014)
//
// The java API is pretty much identical to the C++ API, with methods
// systematically renamed to the Java-style "lowerCamelCase", and using
// the Java-style getProperty() instead of the C++ Property(), for getters.
//
// USAGE EXAMPLES (j/c/g and jt/c/g refer to java/com/google and javatests/...):
// - j/c/g/ortools/samples/LinearProgramming.java
// - j/c/g/ortools/samples/IntegerProgramming.java
// - jt/c/g/ortools/linearsolver/LinearSolverTest.java
//
// TODO(user): unit test all the APIs that are currently marked with 'no test'.

%include "enums.swg"  // For native Java enum support.
%include "stdint.i"

%include "ortools/base/base.i"

// We prefer our in-house vector wrapper to std_vector.i, because it
// converts to and from native java arrays.
%import "ortools/util/java/vector.i"

%include "ortools/util/java/proto.i"

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
%}

%typemap(javaimports) SWIGTYPE %{
import java.lang.reflect.*;
%}

// Conversion of array of MPVariable or MPConstraint from/to C++ vectors.
CONVERT_VECTOR_WITH_CAST(operations_research::MPVariable, MPVariable, REINTERPRET_CAST,
    com/google/ortools/linearsolver);
CONVERT_VECTOR_WITH_CAST(operations_research::MPConstraint, MPConstraint, REINTERPRET_CAST,
    com/google/ortools/linearsolver);

// Support the proto-based APIs.
PROTO_INPUT(
    operations_research::MPModelProto,
    com.google.ortools.linearsolver.MPModelProto,
    input_model);
PROTO2_RETURN(
    operations_research::MPModelProto,
    com.google.ortools.linearsolver.MPModelProto);
PROTO_INPUT(
    operations_research::MPModelRequest,
    com.google.ortools.linearsolver.MPModelRequest,
    model_request);
PROTO_INPUT(
    operations_research::MPSolutionResponse,
    com.google.ortools.linearsolver.MPSolutionResponse,
    response);
PROTO2_RETURN(
    operations_research::MPSolutionResponse,
    com.google.ortools.linearsolver.MPSolutionResponse);


%extend operations_research::MPSolver {
  /**
   * Loads a model and returns the error message, which will be empty iff the
   * model is valid.
   */
  std::string loadModelFromProto(
      const operations_research::MPModelProto& input_model) {
    std::string error_message;
    $self->LoadModelFromProto(input_model, &error_message);
    return error_message;
  }

  // Replaces MPSolver::LoadModelFromProtoWithUniqueNamesOrDie
  std::string loadModelFromProtoWithUniqueNamesOrDie(
      const operations_research::MPModelProto& input_model) {
    std::unordered_set<std::string> names;
    for (const auto var : input_model.variable()) {
      if (!var.name().empty() && !names.insert(var.name()).second) {
        LOG(FATAL) << "found duplicated variable names " + var.name();
      }
    }
    std::string error_message;
    $self->LoadModelFromProtoWithUniqueNamesOrDie(input_model, &error_message);
    return error_message;
  }

  /**
   * Export the loaded model to proto and returns it.
   */
  operations_research::MPModelProto exportModelToProto() {
    operations_research::MPModelProto model;
    $self->ExportModelToProto(&model);
    return model;
  }

  /**
   * Fills the solution found to a response proto and returns it.
   */
  operations_research::MPSolutionResponse createSolutionResponseProto() {
    operations_research::MPSolutionResponse response;
    $self->FillSolutionResponseProto(&response);
    return response;
  }

  /**
   * Load a solution encoded in a protocol buffer onto this solver for easy
  access via the MPSolver interface.
   *
   * IMPORTANT: This may only be used in conjunction with ExportModel(),
  following this example:
   *
   \code
     MPSolver my_solver;
     ... add variables and constraints ...
     MPModelProto model_proto;
     my_solver.ExportModelToProto(&model_proto);
     MPSolutionResponse solver_response;
     MPSolver::SolveWithProto(model_proto, &solver_response);
     if (solver_response.result_status() == MPSolutionResponse::OPTIMAL) {
       CHECK_OK(my_solver.LoadSolutionFromProto(solver_response));
       ... inspect the solution using the usual API: solution_value(), etc...
     }
  \endcode
   *
   * The response must be in OPTIMAL or FEASIBLE status.
   *
   * Returns a false if a problem arised (typically, if it wasn't used
   *     like it should be):
   * - loading a solution whose variables don't correspond to the solver's
   *   current variables
   * - loading a solution with a status other than OPTIMAL / FEASIBLE.
   *
   * Note: the objective value isn't checked. You can use VerifySolution() for
   *       that.
   */
   bool loadSolutionFromProto(const MPSolutionResponse& response) {
     return $self->LoadSolutionFromProto(response).ok();
   }

  /**
   * Solves the given model proto and returns a response proto.
   */
  static operations_research::MPSolutionResponse solveWithProto(
      const operations_research::MPModelRequest& model_request) {
    operations_research::MPSolutionResponse response;
    operations_research::MPSolver::SolveWithProto(model_request, &response);
    return response;
  }

  /**
   * Export the loaded model in LP format.
   */
  std::string exportModelAsLpFormat(
      const operations_research::MPModelExportOptions& options =
          operations_research::MPModelExportOptions()) {
    operations_research::MPModelProto model;
    $self->ExportModelToProto(&model);
    return ExportModelAsLpFormat(model, options).value_or("");
  }

  /**
   * Export the loaded model in MPS format.
   */
  std::string exportModelAsMpsFormat(
      const operations_research::MPModelExportOptions& options =
          operations_research::MPModelExportOptions()) {
    operations_research::MPModelProto model;
    $self->ExportModelToProto(&model);
    return ExportModelAsMpsFormat(model, options).value_or("");
  }

  /**
   * Sets a hint for solution.
   *
   * If a feasible or almost-feasible solution to the problem is already known,
   * it may be helpful to pass it to the solver so that it can be used. A
   * solver that supports this feature will try to use this information to
   * create its initial feasible solution.
   *
   * Note that it may not always be faster to give a hint like this to the
   * solver. There is also no guarantee that the solver will use this hint or
   * try to return a solution "close" to this assignment in case of multiple
   * optimal solutions.
   */
  void setHint(const std::vector<operations_research::MPVariable*>& variables,
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

  /**
   * Sets the number of threads to be used by the solver.
   */
  bool setNumThreads(int num_theads) {
    return $self->SetNumThreads(num_theads).ok();
  }
}  // Extend operations_research::MPSolver

// Add java code on MPSolver.
%typemap(javacode) operations_research::MPSolver %{
  /**
   * Creates and returns an array of variables.
   */
  public MPVariable[] makeVarArray(int count, double lb, double ub, boolean integer) {
    MPVariable[] array = new MPVariable[count];
    for (int i = 0; i < count; ++i) {
      array[i] = makeVar(lb, ub, integer, "");
    }
    return array;
  }

  /**
   * Creates and returns an array of named variables.
   */
  public MPVariable[] makeVarArray(int count, double lb, double ub, boolean integer,
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

  public MPVariable[] makeNumVarArray(int count, double lb, double ub, String var_name) {
    return makeVarArray(count, lb, ub, false, var_name);
  }

  public MPVariable[] makeIntVarArray(int count, double lb, double ub) {
    return makeVarArray(count, lb, ub, true);
  }

  public MPVariable[] makeIntVarArray(int count, double lb, double ub, String var_name) {
    return makeVarArray(count, lb, ub, true, var_name);
  }

  public MPVariable[] makeBoolVarArray(int count) {
    return makeVarArray(count, 0.0, 1.0, true);
  }

  public MPVariable[] makeBoolVarArray(int count, String var_name) {
    return makeVarArray(count, 0.0, 1.0, true, var_name);
  }
%}  // %typemap(javacode) operations_research::MPSolver

%ignoreall

%unignore operations_research;

// List of the classes exposed.
%unignore operations_research::MPSolver;
%unignore operations_research::MPSolver::MPSolver;
%unignore operations_research::MPSolver::~MPSolver;
%newobject operations_research::MPSolver::CreateSolver;
%rename (createSolver) operations_research::MPSolver::CreateSolver;

%unignore operations_research::MPConstraint;
%unignore operations_research::MPVariable;
%unignore operations_research::MPObjective;
%unignore operations_research::MPSolverParameters;

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
%unignore operations_research::MPSolver::XPRESS_LINEAR_PROGRAMMING;
%unignore operations_research::MPSolver::XPRESS_MIXED_INTEGER_PROGRAMMING;


// Expose the MPSolver::ResultStatus enum.
%unignore operations_research::MPSolver::ResultStatus;
%unignore operations_research::MPSolver::OPTIMAL;
%unignore operations_research::MPSolver::FEASIBLE;  // no test
%unignore operations_research::MPSolver::INFEASIBLE;  // no test
%unignore operations_research::MPSolver::UNBOUNDED;  // no test
%unignore operations_research::MPSolver::ABNORMAL;  // no test
%unignore operations_research::MPSolver::NOT_SOLVED;  // no test

// Expose the MPSolver's basic API, with some non-trivial renames.
%rename (objective) operations_research::MPSolver::MutableObjective;
// We intentionally don't expose MakeRowConstraint(LinearExpr), because this
// "natural language" API is specific to C++: other languages may add their own
// syntactic sugar on top of MPSolver instead of this.
%rename (makeConstraint) operations_research::MPSolver::MakeRowConstraint(double, double);
%rename (makeConstraint) operations_research::MPSolver::MakeRowConstraint();
%rename (makeConstraint) operations_research::MPSolver::MakeRowConstraint(double, double, const std::string&);
%rename (makeConstraint) operations_research::MPSolver::MakeRowConstraint(const std::string&);

// Expose the MPSolver's basic API, with trivial renames.
%rename (makeBoolVar) operations_research::MPSolver::MakeBoolVar;  // no test
%rename (makeIntVar) operations_research::MPSolver::MakeIntVar;
%rename (makeNumVar) operations_research::MPSolver::MakeNumVar;
%rename (makeVar) operations_research::MPSolver::MakeVar;  // no test
%rename (solve) operations_research::MPSolver::Solve;
%rename (verifySolution) operations_research::MPSolver::VerifySolution;
%rename (reset) operations_research::MPSolver::Reset;  // no test
%rename (infinity) operations_research::MPSolver::infinity;
%rename (setTimeLimit) operations_research::MPSolver::set_time_limit;  // no test
%rename (isMip) operations_research::MPSolver::IsMIP;  // no test

// Proto-based API of the MPSolver. Use is encouraged.
// Note: the following proto-based methods aren't listed here, but are
// supported (that's because we re-implement them in java below):
// - loadModelFromProto
// - exportModelToProto
// - createSolutionResponseProto
// - solveWithProto
%unignore operations_research::MPSolver::LoadStatus;
%unignore operations_research::MPSolver::NO_ERROR;  // no test
%unignore operations_research::MPSolver::UNKNOWN_VARIABLE_ID;  // no test
// - loadSolutionFromProto;  // Use hand-written version.

// Expose some of the more advanced MPSolver API.
%rename (problemType) operations_research::MPSolver::ProblemType;  // no test
%rename (supportsProblemType) operations_research::MPSolver::SupportsProblemType;  // no test
%rename (setSolverSpecificParametersAsString)
    operations_research::MPSolver::SetSolverSpecificParametersAsString;  // no test
%rename (interruptSolve) operations_research::MPSolver::InterruptSolve;  // no test
%rename (wallTime) operations_research::MPSolver::wall_time;
%rename (clear) operations_research::MPSolver::Clear;  // no test
%unignore operations_research::MPSolver::constraint;
%unignore operations_research::MPSolver::constraints;
%unignore operations_research::MPSolver::variable;
%unignore operations_research::MPSolver::variables;
%rename (numVariables) operations_research::MPSolver::NumVariables;
%rename (numConstraints) operations_research::MPSolver::NumConstraints;
%rename (enableOutput) operations_research::MPSolver::EnableOutput;  // no test
%rename (suppressOutput) operations_research::MPSolver::SuppressOutput;  // no test
%rename (lookupConstraintOrNull) operations_research::MPSolver::LookupConstraintOrNull;  // no test
%rename (lookupVariableOrNull) operations_research::MPSolver::LookupVariableOrNull;  // no test

// Expose very advanced parts of the MPSolver API. For expert users only.
%rename (computeConstraintActivities) operations_research::MPSolver::ComputeConstraintActivities;
%rename (computeExactConditionNumber) operations_research::MPSolver::ComputeExactConditionNumber;
%rename (nodes) operations_research::MPSolver::nodes;
%rename (iterations) operations_research::MPSolver::iterations;
%unignore operations_research::MPSolver::BasisStatus;  // no test
%unignore operations_research::MPSolver::FREE;  // no test
%unignore operations_research::MPSolver::AT_LOWER_BOUND;
%unignore operations_research::MPSolver::AT_UPPER_BOUND;  // no test
%unignore operations_research::MPSolver::FIXED_VALUE;  // no test
%unignore operations_research::MPSolver::BASIC;
%ignore operations_research::MPSolver::SetStartingLpBasis; // no typemap for const vector<BasisStatus>&

// MPVariable: writer API.
%rename (setInteger) operations_research::MPVariable::SetInteger;
%rename (setLb) operations_research::MPVariable::SetLB;  // no test
%rename (setUb) operations_research::MPVariable::SetUB;  // no test
%rename (setBounds) operations_research::MPVariable::SetBounds;  // no test

// MPVariable: reader API.
%rename (solutionValue) operations_research::MPVariable::solution_value;
%rename (lb) operations_research::MPVariable::lb;  // no test
%rename (ub) operations_research::MPVariable::ub;  // no test
%rename (name) operations_research::MPVariable::name;  // no test
%rename (basisStatus) operations_research::MPVariable::basis_status;
%rename (reducedCost) operations_research::MPVariable::reduced_cost;  // For experts only.
%rename (index) operations_research::MPVariable::index;  // no test

// MPConstraint: writer API.
%rename (setCoefficient) operations_research::MPConstraint::SetCoefficient;
%rename (setLb) operations_research::MPConstraint::SetLB;  // no test
%rename (setUb) operations_research::MPConstraint::SetUB;  // no test
%rename (setBounds) operations_research::MPConstraint::SetBounds;  // no test
%rename (setIsLazy) operations_research::MPConstraint::set_is_lazy;

// MPConstraint: reader API.
%rename (getCoefficient) operations_research::MPConstraint::GetCoefficient;
%rename (lb) operations_research::MPConstraint::lb;  // no test
%rename (ub) operations_research::MPConstraint::ub;  // no test
%rename (name) operations_research::MPConstraint::name;
%rename (basisStatus) operations_research::MPConstraint::basis_status;
%rename (dualValue) operations_research::MPConstraint::dual_value;  // For experts only.
%rename (isLazy) operations_research::MPConstraint::is_lazy;  // For experts only.
%rename (index) operations_research::MPConstraint::index;

// MPObjective: writer API.
%rename (setCoefficient) operations_research::MPObjective::SetCoefficient;
%rename (setMinimization) operations_research::MPObjective::SetMinimization;  // no test
%rename (setMaximization) operations_research::MPObjective::SetMaximization;
%rename (setOptimizationDirection) operations_research::MPObjective::SetOptimizationDirection;
%rename (clear) operations_research::MPObjective::Clear;  // no test
%rename (setOffset) operations_research::MPObjective::SetOffset;

// MPObjective: reader API.
%rename (value) operations_research::MPObjective::Value;
%rename (getCoefficient) operations_research::MPObjective::GetCoefficient;
%rename (minimization) operations_research::MPObjective::minimization;
%rename (maximization) operations_research::MPObjective::maximization;
%rename (offset) operations_research::MPObjective::offset;
%rename (bestBound) operations_research::MPObjective::BestBound;

// MPSolverParameters API. For expert users only.
// TODO(user): unit test all of it.

%unignore operations_research::MPSolverParameters;  // no test
%unignore operations_research::MPSolverParameters::MPSolverParameters;  // no test

// Expose the MPSolverParameters::DoubleParam enum.
%unignore operations_research::MPSolverParameters::DoubleParam;  // no test
%unignore operations_research::MPSolverParameters::RELATIVE_MIP_GAP;  // no test
%unignore operations_research::MPSolverParameters::PRIMAL_TOLERANCE;  // no test
%unignore operations_research::MPSolverParameters::DUAL_TOLERANCE;  // no test
%rename (getDoubleParam) operations_research::MPSolverParameters::GetDoubleParam;  // no test
%rename (setDoubleParam) operations_research::MPSolverParameters::SetDoubleParam;  // no test
%unignore operations_research::MPSolverParameters::kDefaultRelativeMipGap;  // no test
%unignore operations_research::MPSolverParameters::kDefaultPrimalTolerance;  // no test
%unignore operations_research::MPSolverParameters::kDefaultDualTolerance;  // no test

// Expose the MPSolverParameters::IntegerParam enum.
%unignore operations_research::MPSolverParameters::IntegerParam;  // no test
%unignore operations_research::MPSolverParameters::PRESOLVE;  // no test
%unignore operations_research::MPSolverParameters::LP_ALGORITHM;  // no test
%unignore operations_research::MPSolverParameters::INCREMENTALITY;  // no test
%unignore operations_research::MPSolverParameters::SCALING;  // no test
%rename (getIntegerParam) operations_research::MPSolverParameters::GetIntegerParam;  // no test
%rename (setIntegerParam) operations_research::MPSolverParameters::SetIntegerParam;  // no test

// Expose the MPSolverParameters::PresolveValues enum.
%unignore operations_research::MPSolverParameters::PresolveValues;  // no test
%unignore operations_research::MPSolverParameters::PRESOLVE_OFF;  // no test
%unignore operations_research::MPSolverParameters::PRESOLVE_ON;  // no test
%unignore operations_research::MPSolverParameters::kDefaultPresolve;  // no test

// Expose the MPSolverParameters::LpAlgorithmValues enum.
%unignore operations_research::MPSolverParameters::LpAlgorithmValues;  // no test
%unignore operations_research::MPSolverParameters::DUAL;  // no test
%unignore operations_research::MPSolverParameters::PRIMAL;  // no test
%unignore operations_research::MPSolverParameters::BARRIER;  // no test

// Expose the MPSolverParameters::IncrementalityValues enum.
%unignore operations_research::MPSolverParameters::IncrementalityValues;  // no test
%unignore operations_research::MPSolverParameters::INCREMENTALITY_OFF;  // no test
%unignore operations_research::MPSolverParameters::INCREMENTALITY_ON;  // no test
%unignore operations_research::MPSolverParameters::kDefaultIncrementality;  // no test

// Expose the MPSolverParameters::ScalingValues enum.
%unignore operations_research::MPSolverParameters::ScalingValues;  // no test
%unignore operations_research::MPSolverParameters::SCALING_OFF;  // no test
%unignore operations_research::MPSolverParameters::SCALING_ON;  // no test

// Expose the model exporters.
%unignore operations_research::MPModelExportOptions;
%unignore operations_research::MPModelExportOptions::MPModelExportOptions;
%typemap(javaclassmodifiers) operations_research::MPModelExportOptions
    "public final class";
%rename (Obfuscate) operations_research::MPModelExportOptions::obfuscate;
%rename (LogInvalidNames) operations_research::MPModelExportOptions::log_invalid_names;
%rename (ShowUnusedVariables) operations_research::MPModelExportOptions::show_unused_variables;
%rename (MaxLineLength) operations_research::MPModelExportOptions::max_line_length;

%include "ortools/linear_solver/linear_solver.h"
%include "ortools/linear_solver/model_exporter.h"

%unignoreall
