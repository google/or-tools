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

// Go swigging of ../linear_solver.h.
//
// IMPORTANT: The wrapping is completed by ./linearsolver.go, which makes the
// functions more go-friendly. The .swig and the .go file are directly related,
// please read and update them together.
%include "std_pair.i"
%include "ortools/base/base.i"
%include "ortools/util/go/proto.i"

// Forward declare protos.
namespace operations_research {
class MPModelProto;
class MPSolutionResponse;
class MPSolverResponseStatus;
}  // namespace operations_research

%{
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/model_exporter.h"
#include "ortools/linear_solver/model_exporter_swig_helper.h"
%}

PROTO_INPUT(operations_research::MPModelProto,
            MPModelProto);
PROTO_OUTPUT(operations_research::MPModelProto,
             MPModelProto);
PROTO_INPUT(operations_research::MPSolutionResponse,
            MPSolutionResponse);
PROTO_OUTPUT(operations_research::MPSolutionResponse,
             MPSolutionResponse);
PROTO_ENUM(operations_research::MPSolverResponseStatus,
           MPSolverResponseStatus);

// For ComputeConstraintActivities.
VECTOR_TYPEMAP(double, float64, float64, DoubleVector)

// For SetHint().
%template(VarValPair) std::pair<const operations_research::MPVariable*, double>;
%template(VarValPairVector) std::vector<std::pair<const operations_research::MPVariable*, double> >;

%ignoreall

%unignore operations_research;
%unignore operations_research::MPSolver;

%unignore operations_research::MPSolver::OptimizationProblemType;
%unignore operations_research::MPSolver::ResultStatus;
%unignore operations_research::MPSolver::SupportsProblemType;
%unignore operations_research::MPSolver::ProblemType;

%unignore operations_research::MPSolver::MPSolver;
%unignore operations_research::MPSolver::~MPSolver;

%apply string* OUTPUT {string *error_message};  // For LoadModelFromProto
%unignore operations_research::MPSolver::LoadModelFromProto;
%unignore operations_research::MPSolver::LoadModelFromProtoWithUniqueNamesOrDie;
%unignore operations_research::MPSolver::ExportModelToProto;

%unignore operations_research::MPModelExportOptions;
%rename (LogInvalidNames) operations_research::MPModelExportOptions::log_invalid_names;
%rename (MaxLineLength) operations_research::MPModelExportOptions::max_line_length;
%rename (ShowUnusedVariables) operations_research::MPModelExportOptions::show_unused_variables;
%unignore operations_research::MPModelExportOptions::obfuscate;
%unignore operations_research::MPModelExportOptions::MPModelExportOptions;
%unignore operations_research::ExportModelAsLpFormatReturnString;
%unignore operations_research::ExportModelAsMpsFormatReturnString;

%unignore operations_research::MPSolver::Solve;

%unignore operations_research::MPSolver::FillSolutionResponseProto;

%unignore operations_research::MPSolver::ComputeConstraintActivities;

// *****************************************************************************
// MPSolverParameters API. For expert users only.
// *****************************************************************************
%unignore operations_research::MPSolverParameters;
%unignore operations_research::MPSolverParameters::MPSolverParameters;

%unignore operations_research::MPSolverParameters::DoubleParam;
%unignore operations_research::MPSolverParameters::RELATIVE_MIP_GAP;
%unignore operations_research::MPSolverParameters::PRIMAL_TOLERANCE;
%unignore operations_research::MPSolverParameters::DUAL_TOLERANCE;

%unignore operations_research::MPSolverParameters::IntegerParam;
%unignore operations_research::MPSolverParameters::PRESOLVE;
%unignore operations_research::MPSolverParameters::LP_ALGORITHM;
%unignore operations_research::MPSolverParameters::INCREMENTALITY;
%unignore operations_research::MPSolverParameters::SCALING;

%unignore operations_research::MPSolverParameters::PresolveValues;
%unignore operations_research::MPSolverParameters::PRESOLVE_OFF;
%unignore operations_research::MPSolverParameters::PRESOLVE_ON;

%unignore operations_research::MPSolverParameters::LpAlgorithmValues;
%unignore operations_research::MPSolverParameters::DUAL;
%unignore operations_research::MPSolverParameters::PRIMAL;
%unignore operations_research::MPSolverParameters::BARRIER;

%unignore operations_research::MPSolverParameters::IncrementalityValues;
%unignore operations_research::MPSolverParameters::INCREMENTALITY_OFF;
%unignore operations_research::MPSolverParameters::INCREMENTALITY_ON;

%unignore operations_research::MPSolverParameters::ScalingValues;
%unignore operations_research::MPSolverParameters::SCALING_OFF;
%unignore operations_research::MPSolverParameters::SCALING_ON;

%unignore operations_research::MPSolverParameters::GetDoubleParam;
%unignore operations_research::MPSolverParameters::SetDoubleParam;
%unignore operations_research::MPSolverParameters::ResetDoubleParam;
%unignore operations_research::MPSolverParameters::GetIntegerParam;
%unignore operations_research::MPSolverParameters::SetIntegerParam;
%unignore operations_research::MPSolverParameters::ResetIntegerParam;

// *****************************************************************************
// Directly callable methods on the LinearSolver object in Go.
//
// Note that if a method name starts with a small letter, it will be renamed to
// start with a capital letter. For example, solver.Wall_time().
// *****************************************************************************
%unignore operations_research::MPSolver::Name;
%unignore operations_research::MPSolver::SolverVersion;

%unignore operations_research::MPSolver::NumVariables;
%unignore operations_research::MPSolver::NumConstraints;
%unignore operations_research::MPSolver::Clear;
%unignore operations_research::MPSolver::Reset;

%unignore operations_research::MPSolver::VerifySolution;
%unignore operations_research::MPSolver::LoadSolutionFromProto;

%unignore operations_research::MPSolver::SetSolverSpecificParametersAsString;  // not tested
%unignore operations_research::MPSolver::GetSolverSpecificParametersAsString;  // not tested

%unignore operations_research::MPSolver::EnableOutput;
%unignore operations_research::MPSolver::SuppressOutput;
%unignore operations_research::MPSolver::OutputIsEnabled;

%unignore operations_research::MPSolver::wall_time;
%unignore operations_research::MPSolver::set_time_limit;
%unignore operations_research::MPSolver::time_limit;
%unignore operations_research::MPSolver::time_limit_in_secs;  // not tested

%unignore operations_research::MPSolver::iterations;          // not tested
%unignore operations_research::MPSolver::nodes;               // not tested

%unignore operations_research::MPSolver::ComputeExactConditionNumber;  // not tested
%unignore operations_research::MPSolver::SetHint;

// *****************************************************************************
// Model building APIs.
//
// Objective, Variable, and constraint.
// *****************************************************************************
%unignore operations_research::MPObjective;
%unignore operations_research::MPVariable;
%unignore operations_research::MPConstraint;

%unignore operations_research::MPSolver::MutableObjective;
%unignore operations_research::MPSolver::MakeVar;
// We intentionally don't expose MakeRowConstraint(LinearExpr), because this
// "natural language" API is specific to C++: other languages may add their own
// syntactic sugar on top of MPSolver instead of this.
%unignore operations_research::MPSolver::MakeRowConstraint(double, double);
%unignore operations_research::MPSolver::MakeRowConstraint();
%unignore operations_research::MPSolver::MakeRowConstraint(double, double, const string&);
%unignore operations_research::MPSolver::MakeRowConstraint(const string&);
%unignore operations_research::MPSolver::LookupVariableOrNull;
%unignore operations_research::MPSolver::LookupConstraintOrNull;

%unignore operations_research::MPSolver::BasisStatus;
%unignore operations_research::MPSolver::FREE;
%unignore operations_research::MPSolver::AT_LOWER_BOUND;
%unignore operations_research::MPSolver::AT_UPPER_BOUND;
%unignore operations_research::MPSolver::FIXED_VALUE;
%unignore operations_research::MPSolver::BASIC;

%unignore operations_research::MPObjective::SetCoefficient;
%unignore operations_research::MPObjective::GetCoefficient;

// MPObjective: writer API. Directly exposed.
%unignore operations_research::MPObjective::SetMinimization;
%unignore operations_research::MPObjective::SetMaximization;
%unignore operations_research::MPObjective::SetOptimizationDirection;
%unignore operations_research::MPObjective::Clear;
%unignore operations_research::MPObjective::SetOffset;

// MPObjective: reader API. Directly exposed.
%unignore operations_research::MPObjective::Value;
%unignore operations_research::MPObjective::minimization;
%unignore operations_research::MPObjective::maximization;
%unignore operations_research::MPObjective::offset;
%unignore operations_research::MPObjective::BestBound;  // No test.

// MPVariable: writer API. Directly exposed.
%unignore operations_research::MPVariable::SetInteger;
%unignore operations_research::MPVariable::SetLB;
%unignore operations_research::MPVariable::SetUB;
%unignore operations_research::MPVariable::SetBounds;

// MPVariable: reader API. Directly exposed.
%unignore operations_research::MPVariable::solution_value;
%rename (LB) operations_research::MPVariable::lb;
%rename (UB) operations_research::MPVariable::ub;
%unignore operations_research::MPVariable::name;
%unignore operations_research::MPVariable::integer;
%unignore operations_research::MPVariable::index;
%unignore operations_research::MPVariable::basis_status;
%unignore operations_research::MPVariable::reduced_cost;  // No test.
%unignore operations_research::MPVariable::unrounded_solution_value;  // No test.

%unignore operations_research::MPConstraint::SetCoefficient;
%unignore operations_research::MPConstraint::GetCoefficient;

// MPConstraint: writer API. Directly exposed.
%unignore operations_research::MPConstraint::SetLB;
%unignore operations_research::MPConstraint::SetUB;
%unignore operations_research::MPConstraint::SetBounds;
%unignore operations_research::MPConstraint::set_is_lazy;

// MPConstraint: reader API. Directly exposed.
%rename (LB) operations_research::MPConstraint::lb;
%rename (UB) operations_research::MPConstraint::ub;
%unignore operations_research::MPConstraint::name;
%unignore operations_research::MPConstraint::basis_status;
%unignore operations_research::MPConstraint::dual_value;
%unignore operations_research::MPConstraint::is_lazy;
%unignore operations_research::MPConstraint::index;

%include "ortools/util/linear_solver/linear_solver.h"
%include "ortools/util/linear_solver/model_exporter.h"
%include "ortools/util/linear_solver/model_exporter_swig_helper.h"

%unignoreall
