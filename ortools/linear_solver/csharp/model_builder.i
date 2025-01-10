// Copyright 2010-2025 Google LLC
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

%include "ortools/base/base.i"
%include "enums.swg"
%import "ortools/util/csharp/vector.i"

%{
#include "ortools/linear_solver/wrappers/model_builder_helper.h"
%}

%template(IntVector) std::vector<int>;
VECTOR_AS_CSHARP_ARRAY(int, int, int, IntVector);

%template(DoubleVector) std::vector<double>;
VECTOR_AS_CSHARP_ARRAY(double, double, double, DoubleVector);

%module(directors="1") operations_research_model_builder

%extend operations_research::ModelBuilderHelper {
  std::string ExportToMpsString(bool obfuscate) {
    operations_research::MPModelExportOptions options;
    options.obfuscate = obfuscate;
    return $self->ExportToMpsString(options);
  }

  std::string ExportToLpString(bool obfuscate) {
    operations_research::MPModelExportOptions options;
    options.obfuscate = obfuscate;
    return $self->ExportToLpString(options);
  }

  bool WriteToMpsFile(const std::string& filename, bool obfuscate) {
    operations_research::MPModelExportOptions options;
    options.obfuscate = obfuscate;
    return $self->WriteToMpsFile(filename, options);
  }
}  // Extend operations_research::ModelBuilderHelper

%ignoreall

%unignore operations_research;

// Wrap the ModelBuilderHelper class.
%unignore operations_research::ModelBuilderHelper;
%unignore operations_research::ModelBuilderHelper::ModelBuilderHelper;
%unignore operations_research::ModelBuilderHelper::~ModelBuilderHelper;

// Var API.
%unignore operations_research::ModelBuilderHelper::AddVar;
%unignore operations_research::ModelBuilderHelper::VarIsIntegral;
%unignore operations_research::ModelBuilderHelper::VarLowerBound;
%unignore operations_research::ModelBuilderHelper::VarName;
%unignore operations_research::ModelBuilderHelper::VarObjectiveCoefficient;
%unignore operations_research::ModelBuilderHelper::VarUpperBound;
%unignore operations_research::ModelBuilderHelper::SetVarIntegrality;
%unignore operations_research::ModelBuilderHelper::SetVarLowerBound;
%unignore operations_research::ModelBuilderHelper::SetVarName;
%unignore operations_research::ModelBuilderHelper::SetVarObjectiveCoefficient;
%unignore operations_research::ModelBuilderHelper::SetVarUpperBound;

// Linear Constraint API.
%unignore operations_research::ModelBuilderHelper::AddConstraintTerm;
%unignore operations_research::ModelBuilderHelper::AddLinearConstraint;
%unignore operations_research::ModelBuilderHelper::ClearConstraintTerms;
%unignore operations_research::ModelBuilderHelper::ConstraintCoefficients;
%unignore operations_research::ModelBuilderHelper::ConstraintLowerBound;
%unignore operations_research::ModelBuilderHelper::ConstraintName;
%unignore operations_research::ModelBuilderHelper::ConstraintUpperBound;
%unignore operations_research::ModelBuilderHelper::ConstraintVarIndices;
%unignore operations_research::ModelBuilderHelper::SafeAddConstraintTerm;
%unignore operations_research::ModelBuilderHelper::SetConstraintCoefficient;
%unignore operations_research::ModelBuilderHelper::SetConstraintLowerBound;
%unignore operations_research::ModelBuilderHelper::SetConstraintName;
%unignore operations_research::ModelBuilderHelper::SetConstraintUpperBound;

// Enforced Linear Constraints API.
%unignore operations_research::ModelBuilderHelper::AddEnforcedConstraintTerm;
%unignore operations_research::ModelBuilderHelper::AddEnforcedLinearConstraint;
%unignore operations_research::ModelBuilderHelper::ClearEnforcedConstraintTerms;
%unignore operations_research::ModelBuilderHelper::EnforcedConstraintCoefficients;
%unignore operations_research::ModelBuilderHelper::EnforcedConstraintLowerBound;
%unignore operations_research::ModelBuilderHelper::EnforcedConstraintName;
%unignore operations_research::ModelBuilderHelper::EnforcedConstraintUpperBound;
%unignore operations_research::ModelBuilderHelper::EnforcedConstraintVarIndices;
%unignore operations_research::ModelBuilderHelper::EnforcedIndicatorValue;
%unignore operations_research::ModelBuilderHelper::EnforcedIndicatorVariableIndex;
%unignore operations_research::ModelBuilderHelper::IsEnforcedConstraint;
%unignore operations_research::ModelBuilderHelper::SafeAddEnforcedConstraintTerm;
%unignore operations_research::ModelBuilderHelper::SetEnforcedConstraintCoefficient;
%unignore operations_research::ModelBuilderHelper::SetEnforcedConstraintLowerBound;
%unignore operations_research::ModelBuilderHelper::SetEnforcedConstraintName;
%unignore operations_research::ModelBuilderHelper::SetEnforcedConstraintUpperBound;
%unignore operations_research::ModelBuilderHelper::SetEnforcedIndicatorValue;
%unignore operations_research::ModelBuilderHelper::SetEnforcedIndicatorVariableIndex;

// Objective API.
%unignore operations_research::ModelBuilderHelper::ClearObjective;
%rename (Maximize) operations_research::ModelBuilderHelper::maximize;
%unignore operations_research::ModelBuilderHelper::SetMaximize;
%unignore operations_research::ModelBuilderHelper::ObjectiveOffset;
%unignore operations_research::ModelBuilderHelper::SetObjectiveOffset;

// Hints
%unignore operations_research::ModelBuilderHelper::ClearHints;
%unignore operations_research::ModelBuilderHelper::AddHint;

// Model API.
%rename (VariablesCount) operations_research::ModelBuilderHelper::num_variables;
%rename (ConstraintsCount) operations_research::ModelBuilderHelper::num_constraints;
%rename (Name) operations_research::ModelBuilderHelper::name;
%unignore operations_research::ModelBuilderHelper::SetName;
%unignore operations_research::ModelBuilderHelper::ReadModelFromProtoFile;
%unignore operations_research::ModelBuilderHelper::WriteModelToProtoFile;
%unignore operations_research::ModelBuilderHelper::ImportFromMpsString;
%unignore operations_research::ModelBuilderHelper::ImportFromMpsFile;
%unignore operations_research::ModelBuilderHelper::ImportFromLpString;
%unignore operations_research::ModelBuilderHelper::ImportFromLpFile;
%unignore operations_research::ModelBuilderHelper::WriteToMpsFile(std::string, bool);
%unignore operations_research::ModelBuilderHelper::ExportToMpsString(bool);
%unignore operations_research::ModelBuilderHelper::ExportToLpString(bool);
%unignore operations_research::ModelBuilderHelper::OverwriteModel;

// Callbacks support.
%feature("director") operations_research::MbLogCallback;
%unignore operations_research::MbLogCallback;
%unignore operations_research::MbLogCallback::~MbLogCallback;
%unignore operations_research::MbLogCallback::NewMessage;

// Solver API.
%unignore operations_research::ModelSolverHelper;
%unignore operations_research::ModelSolverHelper::ModelSolverHelper(const std::string&);
%unignore operations_research::ModelSolverHelper::SolverIsSupported;
%unignore operations_research::ModelSolverHelper::Solve;
%unignore operations_research::ModelSolverHelper::InterruptSolve;
%rename (HasResponse) operations_research::ModelSolverHelper::has_response;
%rename (HasSolution) operations_research::ModelSolverHelper::has_solution;
%rename (Status) operations_research::ModelSolverHelper::status;
%rename (ObjectiveValue) operations_research::ModelSolverHelper::objective_value;
%rename (BestObjectiveBound) operations_research::ModelSolverHelper::best_objective_bound;
%rename (VariableValue) operations_research::ModelSolverHelper::variable_value;
%rename (ReducedCost) operations_research::ModelSolverHelper::reduced_cost;
%rename (DualValue) operations_research::ModelSolverHelper::dual_value;
%rename (Activity) operations_research::ModelSolverHelper::activity;
%rename (StatusString) operations_research::ModelSolverHelper::status_string;
%rename (WallTime) operations_research::ModelSolverHelper::wall_time;
%rename (UserTime) operations_research::ModelSolverHelper::user_time;
%unignore operations_research::ModelSolverHelper::EnableOutput;
%unignore operations_research::ModelSolverHelper::ClearLogCallback;
%unignore operations_research::ModelSolverHelper::SetLogCallbackFromDirectorClass;
%unignore operations_research::ModelSolverHelper::SetTimeLimitInSeconds;
%unignore operations_research::ModelSolverHelper::SetSolverSpecificParameters;

%unignore operations_research::SolveStatus;
%unignore operations_research::OPTIMAL;
%unignore operations_research::FEASIBLE;
%unignore operations_research::INFEASIBLE;
%unignore operations_research::UNBOUNDED;
%unignore operations_research::ABNORMAL;
%unignore operations_research::NOT_SOLVED;
%unignore operations_research::MODEL_IS_VALID;
%unignore operations_research::CANCELLED_BY_USER;
%unignore operations_research::UNKNOWN_STATUS;
%unignore operations_research::MODEL_INVALID;
%unignore operations_research::INVALID_SOLVER_PARAMETERS;
%unignore operations_research::SOLVER_TYPE_UNAVAILABLE;
%unignore operations_research::INCOMPATIBLE_OPTIONS;

// For enums
%include "ortools/linear_solver/wrappers/model_builder_helper.h"

%unignoreall

