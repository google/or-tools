// Copyright 2010-2024 Google LLC
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

%extend operations_research::mb::ModelBuilderHelper {
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
}  // Extend operations_research::mb::ModelBuilderHelper

%ignoreall

%unignore operations_research;
%unignore operations_research::mb;

// Wrap the ModelBuilderHelper class.
%unignore operations_research::mb::ModelBuilderHelper;
%unignore operations_research::mb::ModelBuilderHelper::ModelBuilderHelper;
%unignore operations_research::mb::ModelBuilderHelper::~ModelBuilderHelper;

// Var API.
%unignore operations_research::mb::ModelBuilderHelper::AddVar;
%unignore operations_research::mb::ModelBuilderHelper::VarIsIntegral;
%unignore operations_research::mb::ModelBuilderHelper::VarLowerBound;
%unignore operations_research::mb::ModelBuilderHelper::VarName;
%unignore operations_research::mb::ModelBuilderHelper::VarObjectiveCoefficient;
%unignore operations_research::mb::ModelBuilderHelper::VarUpperBound;
%unignore operations_research::mb::ModelBuilderHelper::SetVarIntegrality;
%unignore operations_research::mb::ModelBuilderHelper::SetVarLowerBound;
%unignore operations_research::mb::ModelBuilderHelper::SetVarName;
%unignore operations_research::mb::ModelBuilderHelper::SetVarObjectiveCoefficient;
%unignore operations_research::mb::ModelBuilderHelper::SetVarUpperBound;

// Linear Constraint API.
%unignore operations_research::mb::ModelBuilderHelper::AddConstraintTerm;
%unignore operations_research::mb::ModelBuilderHelper::AddLinearConstraint;
%unignore operations_research::mb::ModelBuilderHelper::ClearConstraintTerms;
%unignore operations_research::mb::ModelBuilderHelper::ConstraintCoefficients;
%unignore operations_research::mb::ModelBuilderHelper::ConstraintLowerBound;
%unignore operations_research::mb::ModelBuilderHelper::ConstraintName;
%unignore operations_research::mb::ModelBuilderHelper::ConstraintUpperBound;
%unignore operations_research::mb::ModelBuilderHelper::ConstraintVarIndices;
%unignore operations_research::mb::ModelBuilderHelper::SafeAddConstraintTerm;
%unignore operations_research::mb::ModelBuilderHelper::SetConstraintCoefficient;
%unignore operations_research::mb::ModelBuilderHelper::SetConstraintLowerBound;
%unignore operations_research::mb::ModelBuilderHelper::SetConstraintName;
%unignore operations_research::mb::ModelBuilderHelper::SetConstraintUpperBound;

// Enforced Linear Constraints API.
%unignore operations_research::mb::ModelBuilderHelper::AddEnforcedConstraintTerm;
%unignore operations_research::mb::ModelBuilderHelper::AddEnforcedLinearConstraint;
%unignore operations_research::mb::ModelBuilderHelper::ClearEnforcedConstraintTerms;
%unignore operations_research::mb::ModelBuilderHelper::EnforcedConstraintCoefficients;
%unignore operations_research::mb::ModelBuilderHelper::EnforcedConstraintLowerBound;
%unignore operations_research::mb::ModelBuilderHelper::EnforcedConstraintName;
%unignore operations_research::mb::ModelBuilderHelper::EnforcedConstraintUpperBound;
%unignore operations_research::mb::ModelBuilderHelper::EnforcedConstraintVarIndices;
%unignore operations_research::mb::ModelBuilderHelper::EnforcedIndicatorValue;
%unignore operations_research::mb::ModelBuilderHelper::EnforcedIndicatorVariableIndex;
%unignore operations_research::mb::ModelBuilderHelper::IsEnforcedConstraint;
%unignore operations_research::mb::ModelBuilderHelper::SafeAddEnforcedConstraintTerm;
%unignore operations_research::mb::ModelBuilderHelper::SetEnforcedConstraintCoefficient;
%unignore operations_research::mb::ModelBuilderHelper::SetEnforcedConstraintLowerBound;
%unignore operations_research::mb::ModelBuilderHelper::SetEnforcedConstraintName;
%unignore operations_research::mb::ModelBuilderHelper::SetEnforcedConstraintUpperBound;
%unignore operations_research::mb::ModelBuilderHelper::SetEnforcedIndicatorValue;
%unignore operations_research::mb::ModelBuilderHelper::SetEnforcedIndicatorVariableIndex;

// Objective API.
%unignore operations_research::mb::ModelBuilderHelper::ClearObjective;
%rename (Maximize) operations_research::mb::ModelBuilderHelper::maximize;
%unignore operations_research::mb::ModelBuilderHelper::SetMaximize;
%unignore operations_research::mb::ModelBuilderHelper::ObjectiveOffset;
%unignore operations_research::mb::ModelBuilderHelper::SetObjectiveOffset;

// Hints
%unignore operations_research::mb::ModelBuilderHelper::ClearHints;
%unignore operations_research::mb::ModelBuilderHelper::AddHint;

// Model API.
%rename (VariablesCount) operations_research::mb::ModelBuilderHelper::num_variables;
%rename (ConstraintsCount) operations_research::mb::ModelBuilderHelper::num_constraints;
%rename (Name) operations_research::mb::ModelBuilderHelper::name;
%unignore operations_research::mb::ModelBuilderHelper::SetName;
%unignore operations_research::mb::ModelBuilderHelper::ReadModelFromProtoFile;
%unignore operations_research::mb::ModelBuilderHelper::WriteModelToProtoFile;
%unignore operations_research::mb::ModelBuilderHelper::ImportFromMpsString;
%unignore operations_research::mb::ModelBuilderHelper::ImportFromMpsFile;
%unignore operations_research::mb::ModelBuilderHelper::ImportFromLpString;
%unignore operations_research::mb::ModelBuilderHelper::ImportFromLpFile;
%unignore operations_research::mb::ModelBuilderHelper::WriteToMpsFile(std::string, bool);
%unignore operations_research::mb::ModelBuilderHelper::ExportToMpsString(bool);
%unignore operations_research::mb::ModelBuilderHelper::ExportToLpString(bool);
%unignore operations_research::mb::ModelBuilderHelper::OverwriteModel;

// Callbacks support.
%feature("director") operations_research::mb::MbLogCallback;
%unignore operations_research::mb::MbLogCallback;
%unignore operations_research::mb::MbLogCallback::~MbLogCallback;
%unignore operations_research::mb::MbLogCallback::NewMessage;

// Solver API.
%unignore operations_research::mb::ModelSolverHelper;
%unignore operations_research::mb::ModelSolverHelper::ModelSolverHelper(const std::string&);
%unignore operations_research::mb::ModelSolverHelper::SolverIsSupported;
%unignore operations_research::mb::ModelSolverHelper::Solve;
%unignore operations_research::mb::ModelSolverHelper::InterruptSolve;
%rename (HasResponse) operations_research::mb::ModelSolverHelper::has_response;
%rename (HasSolution) operations_research::mb::ModelSolverHelper::has_solution;
%rename (Status) operations_research::mb::ModelSolverHelper::status;
%rename (ObjectiveValue) operations_research::mb::ModelSolverHelper::objective_value;
%rename (BestObjectiveBound) operations_research::mb::ModelSolverHelper::best_objective_bound;
%rename (VariableValue) operations_research::mb::ModelSolverHelper::variable_value;
%rename (ReducedCost) operations_research::mb::ModelSolverHelper::reduced_cost;
%rename (DualValue) operations_research::mb::ModelSolverHelper::dual_value;
%rename (Activity) operations_research::mb::ModelSolverHelper::activity;
%rename (StatusString) operations_research::mb::ModelSolverHelper::status_string;
%rename (WallTime) operations_research::mb::ModelSolverHelper::wall_time;
%rename (UserTime) operations_research::mb::ModelSolverHelper::user_time;
%unignore operations_research::mb::ModelSolverHelper::EnableOutput;
%unignore operations_research::mb::ModelSolverHelper::ClearLogCallback;
%unignore operations_research::mb::ModelSolverHelper::SetLogCallbackFromDirectorClass;
%unignore operations_research::mb::ModelSolverHelper::SetTimeLimitInSeconds;
%unignore operations_research::mb::ModelSolverHelper::SetSolverSpecificParameters;

%unignore operations_research::mb::SolveStatus;
%unignore operations_research::mb::OPTIMAL;
%unignore operations_research::mb::FEASIBLE;
%unignore operations_research::mb::INFEASIBLE;
%unignore operations_research::mb::UNBOUNDED;
%unignore operations_research::mb::ABNORMAL;
%unignore operations_research::mb::NOT_SOLVED;
%unignore operations_research::mb::MODEL_IS_VALID;
%unignore operations_research::mb::CANCELLED_BY_USER;
%unignore operations_research::mb::UNKNOWN_STATUS;
%unignore operations_research::mb::MODEL_INVALID;
%unignore operations_research::mb::INVALID_SOLVER_PARAMETERS;
%unignore operations_research::mb::SOLVER_TYPE_UNAVAILABLE;
%unignore operations_research::mb::INCOMPATIBLE_OPTIONS;

// For enums
%include "ortools/linear_solver/wrappers/model_builder_helper.h"

%unignoreall

