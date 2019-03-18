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

typedef int64_t int64;
typedef uint64_t uint64;

%{
#include "ortools/linear_solver/linear_solver.h"
%}

%typemap(javaimports) SWIGTYPE %{
import java.lang.reflect.*;
%}


%extend operations_research::MPSolver {
  std::string exportModelAsLpFormat(bool obfuscated) {
    std::string output;
    if (!$self->ExportModelAsLpFormat(obfuscated, &output)) return "";
    return output;
  }

  std::string exportModelAsMpsFormat(bool fixed_format, bool obfuscated) {
    std::string output;
    if (!$self->ExportModelAsMpsFormat(fixed_format, obfuscated, &output)) {
      return "";
    }
    return output;
  }

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
}

// Add java code on MPSolver.
%typemap(javacode) operations_research::MPSolver %{
  public MPVariable[] makeVarArray(int count, double lb, double ub, boolean integer) {
    MPVariable[] array = new MPVariable[count];
    for (int i = 0; i < count; ++i) {
      array[i] = makeVar(lb, ub, integer, "");
    }
    return array;
  }

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
%}

%ignoreall

%unignore operations_research;

// List of the classes exposed.
%unignore operations_research::MPSolver;
%unignore operations_research::MPSolver::MPSolver;
%unignore operations_research::MPSolver::~MPSolver;
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


// Expose some of the more advanced MPSolver API.
%rename (supportsProblemType) operations_research::MPSolver::SupportsProblemType;  // no test
%rename (setSolverSpecificParametersAsString)
    operations_research::MPSolver::SetSolverSpecificParametersAsString;  // no test
%rename (interruptSolve) operations_research::MPSolver::InterruptSolve;  // no test
%rename (wallTime) operations_research::MPSolver::wall_time;
%rename (clear) operations_research::MPSolver::Clear;  // no test
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

%include "ortools/linear_solver/linear_solver.h"

%unignoreall
