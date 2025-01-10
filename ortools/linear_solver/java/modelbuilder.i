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

%include "stdint.i"

%include "ortools/base/base.i"
%include "enums.swg"
%include "ortools/util/java/vector.i"

%{
#include "ortools/linear_solver/wrappers/model_builder_helper.h"
%}

%module operations_research_modelbuilder

// This typemap is inspired by the constraints_solver java typemaps.
// The only difference is that the argument is not a basic type, and needs
// processing to be passed to the std::function.
//
// TODO(user): cleanup java/functions.i and move the code there.
%{
#include <memory> // std::make_shared<GlobalRefGuard>

/* Global JNI reference deleter. Instantiate it via std::make_shared<> */
class GlobalRefGuard {
  JavaVM *jvm_;
  jobject jref_;
  // non-copyable
  GlobalRefGuard(const GlobalRefGuard &) = delete;
  GlobalRefGuard &operator=(const GlobalRefGuard &) = delete;
 public:
  GlobalRefGuard(JavaVM *jvm, jobject jref): jvm_(jvm), jref_(jref) {}
  ~GlobalRefGuard() {
    JNIEnv *jenv = NULL;
    JavaVMAttachArgs args;
    args.version = JNI_VERSION_1_2;
    args.name = NULL;
    args.group = NULL;
    jvm_->AttachCurrentThread((void**)&jenv, &args);
    jenv->DeleteGlobalRef(jref_);
    jvm_->DetachCurrentThread();
  }
};
%}

%typemap(in) std::function<void(const std::string&)> %{
  // $input will be deleted once this function return.
  // So we create a JNI global reference to keep it alive.
  jobject $input_object = jenv->NewGlobalRef($input);
  // and we wrap it in a GlobalRefGuard object which will call the
  // JNI global reference deleter to avoid leak at destruction.
  JavaVM* jvm;
  jenv->GetJavaVM(&jvm);
  auto $input_guard = std::make_shared<GlobalRefGuard>(jvm, $input_object);

  jclass $input_object_class = jenv->GetObjectClass($input);
  if (nullptr == $input_object_class) return $null;
  jmethodID $input_method_id = jenv->GetMethodID(
    $input_object_class, "accept", "(Ljava/lang/Object;)V");
  assert($input_method_id != nullptr);

  // When the lambda will be destroyed, input_guard's destructor will be called.
  $1 = [jvm, $input_object, $input_method_id, $input_guard](
      const std::string& message) -> void {
        JNIEnv *jenv = NULL;
        JavaVMAttachArgs args;
        args.version = JNI_VERSION_1_2;
        args.name = NULL;
        args.group = NULL;
        jvm->AttachCurrentThread((void**)&jenv, &args);
        jenv->CallVoidMethod($input_object, $input_method_id, (jenv)->NewStringUTF(message.c_str()));
        jvm->DetachCurrentThread();
  };
%}
%typemap(jni) std::function<void(const std::string&)> "jobject" // Type used in the JNI C.
%typemap(jtype) std::function<void(const std::string&)> "java.util.function.Consumer<String>" // Type used in the JNI.java.
%typemap(jstype) std::function<void(const std::string&)> "java.util.function.Consumer<String>" // Type used in the Proxy class.
%typemap(javain) std::function<void(const std::string&)> "$javainput" // passing the Callback to JNI java class.

%extend operations_research::ModelBuilderHelper {
  std::string exportToMpsString(bool obfuscate) {
    operations_research::MPModelExportOptions options;
    options.obfuscate = obfuscate;
    return $self->ExportToMpsString(options);
  }

  std::string exportToLpString(bool obfuscate) {
    operations_research::MPModelExportOptions options;
    options.obfuscate = obfuscate;
    return $self->ExportToLpString(options);
  }

  bool writeToMpsFile(const std::string& filename, bool obfuscate) {
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
%rename (addVar) operations_research::ModelBuilderHelper::AddVar;
%rename (getVarIntegrality) operations_research::ModelBuilderHelper::VarIsIntegral;
%rename (getVarLowerBound) operations_research::ModelBuilderHelper::VarLowerBound;
%rename (getVarName) operations_research::ModelBuilderHelper::VarName;
%rename (getVarObjectiveCoefficient) operations_research::ModelBuilderHelper::VarObjectiveCoefficient;
%rename (getVarUpperBound) operations_research::ModelBuilderHelper::VarUpperBound;
%rename (setVarIntegrality) operations_research::ModelBuilderHelper::SetVarIntegrality;
%rename (setVarLowerBound) operations_research::ModelBuilderHelper::SetVarLowerBound;
%rename (setVarName) operations_research::ModelBuilderHelper::SetVarName;
%rename (setVarObjectiveCoefficient) operations_research::ModelBuilderHelper::SetVarObjectiveCoefficient;
%rename (setVarUpperBound) operations_research::ModelBuilderHelper::SetVarUpperBound;

// Linear Constraint API.
%rename (addConstraintTerm) operations_research::ModelBuilderHelper::AddConstraintTerm;
%rename (addLinearConstraint) operations_research::ModelBuilderHelper::AddLinearConstraint;
%rename (clearConstraintTerms) operations_research::ModelBuilderHelper::ClearConstraintTerms;
%rename (getConstraintCoefficients) operations_research::ModelBuilderHelper::ConstraintCoefficients;
%rename (getConstraintLowerBound) operations_research::ModelBuilderHelper::ConstraintLowerBound;
%rename (getConstraintName) operations_research::ModelBuilderHelper::ConstraintName;
%rename (getConstraintUpperBound) operations_research::ModelBuilderHelper::ConstraintUpperBound;
%rename (getConstraintVarIndices) operations_research::ModelBuilderHelper::ConstraintVarIndices;
%rename (safeAddConstraintTerm) operations_research::ModelBuilderHelper::SafeAddConstraintTerm;
%rename (setConstraintCoefficient) operations_research::ModelBuilderHelper::SetConstraintCoefficient;
%rename (setConstraintLowerBound) operations_research::ModelBuilderHelper::SetConstraintLowerBound;
%rename (setConstraintName) operations_research::ModelBuilderHelper::SetConstraintName;
%rename (setConstraintUpperBound) operations_research::ModelBuilderHelper::SetConstraintUpperBound;

// Enforced Linear Constraint API.
%rename (addEnforcedConstraintTerm) operations_research::ModelBuilderHelper::AddEnforcedConstraintTerm;
%rename (addEnforcedLinearConstraint) operations_research::ModelBuilderHelper::AddEnforcedLinearConstraint;
%rename (clearEnforcedConstraintTerms) operations_research::ModelBuilderHelper::ClearEnforcedConstraintTerms;
%rename (getEnforcedConstraintCoefficients) operations_research::ModelBuilderHelper::EnforcedConstraintCoefficients;
%rename (getEnforcedConstraintLowerBound) operations_research::ModelBuilderHelper::EnforcedConstraintLowerBound;
%rename (getEnforcedConstraintName) operations_research::ModelBuilderHelper::EnforcedConstraintName;
%rename (getEnforcedConstraintUpperBound) operations_research::ModelBuilderHelper::EnforcedConstraintUpperBound;
%rename (getEnforcedConstraintVarIndices) operations_research::ModelBuilderHelper::EnforcedConstraintVarIndices;
%rename (getEnforcedIndicatorValue) operations_research::ModelBuilderHelper::EnforcedIndicatorValue;
%rename (getEnforcedIndicatorVariableIndex) operations_research::ModelBuilderHelper::EnforcedIndicatorVariableIndex;
%rename (isEnforcedConstraint) operations_research::ModelBuilderHelper::IsEnforcedConstraint;
%rename (safeAddEnforcedConstraintTerm) operations_research::ModelBuilderHelper::SafeAddEnforcedConstraintTerm;
%rename (setEnforcedConstraintCoefficient) operations_research::ModelBuilderHelper::SetEnforcedConstraintCoefficient;
%rename (setEnforcedConstraintLowerBound) operations_research::ModelBuilderHelper::SetEnforcedConstraintLowerBound;
%rename (setEnforcedConstraintName) operations_research::ModelBuilderHelper::SetEnforcedConstraintName;
%rename (setEnforcedConstraintUpperBound) operations_research::ModelBuilderHelper::SetEnforcedConstraintUpperBound;
%rename (setEnforcedIndicatorValue) operations_research::ModelBuilderHelper::SetEnforcedIndicatorValue;
%rename (setEnforcedIndicatorVariableIndex) operations_research::ModelBuilderHelper::SetEnforcedIndicatorVariableIndex;

// Objective API.
%rename (clearObjective) operations_research::ModelBuilderHelper::ClearObjective;
%rename (getMaximize) operations_research::ModelBuilderHelper::maximize;
%rename (setMaximize) operations_research::ModelBuilderHelper::SetMaximize;
%rename (getObjectiveOffset) operations_research::ModelBuilderHelper::ObjectiveOffset;
%rename (setObjectiveOffset) operations_research::ModelBuilderHelper::SetObjectiveOffset;

// Hints.
%rename (clearHints) operations_research::ModelBuilderHelper::ClearHints;
%rename (addHint) operations_research::ModelBuilderHelper::AddHint;

// Model API.
%rename (numVariables) operations_research::ModelBuilderHelper::num_variables;
%rename (numConstraints) operations_research::ModelBuilderHelper::num_constraints;
%rename (getName) operations_research::ModelBuilderHelper::name;
%rename (setName) operations_research::ModelBuilderHelper::SetName;
%rename (readModelFromProtoFile) operations_research::ModelBuilderHelper::ReadModelFromProtoFile;
%rename (writeModelToProtoFile) operations_research::ModelBuilderHelper::WriteModelToProtoFile;
%rename (importFromMpsString) operations_research::ModelBuilderHelper::ImportFromMpsString;
%rename (importFromMpsFile) operations_research::ModelBuilderHelper::ImportFromMpsFile;
%rename (importFromLpString) operations_research::ModelBuilderHelper::ImportFromLpString;
%rename (importFromLpFile) operations_research::ModelBuilderHelper::ImportFromLpFile;
%unignore operations_research::ModelBuilderHelper::exportToMpsString;
%unignore operations_research::ModelBuilderHelper::exportToLpString;
%unignore operations_research::ModelBuilderHelper::writeToMpsFile;
%rename (overwriteModel) operations_research::ModelBuilderHelper::OverwriteModel;

%unignore operations_research::ModelSolverHelper;
%unignore operations_research::ModelSolverHelper::ModelSolverHelper(const std::string&);
%rename (solverIsSupported) operations_research::ModelSolverHelper::SolverIsSupported;
%rename (solve) operations_research::ModelSolverHelper::Solve;
%rename (interruptSolve) operations_research::ModelSolverHelper::InterruptSolve;
%rename (hasResponse) operations_research::ModelSolverHelper::has_response;
%rename (hasSolution) operations_research::ModelSolverHelper::has_solution;
%rename (getStatus) operations_research::ModelSolverHelper::status;
%rename (getObjectiveValue) operations_research::ModelSolverHelper::objective_value;
%rename (getBestObjectiveBound) operations_research::ModelSolverHelper::best_objective_bound;
%rename (getVariableValue) operations_research::ModelSolverHelper::variable_value;
%rename (getReducedCost) operations_research::ModelSolverHelper::reduced_cost;
%rename (getDualValue) operations_research::ModelSolverHelper::dual_value;
%rename (getActivity) operations_research::ModelSolverHelper::activity;
%rename (getStatusString) operations_research::ModelSolverHelper::status_string;
%rename (getWallTime) operations_research::ModelSolverHelper::wall_time;
%rename (getUserTime) operations_research::ModelSolverHelper::user_time;
%rename (enableOutput) operations_research::ModelSolverHelper::EnableOutput;
%rename (clearLogCallback) operations_research::ModelSolverHelper::ClearLogCallback;
%rename (setLogCallback) operations_research::ModelSolverHelper::SetLogCallback;
%rename (setTimeLimitInSeconds) operations_research::ModelSolverHelper::SetTimeLimitInSeconds;
%rename (setSolverSpecificParameters) operations_research::ModelSolverHelper::SetSolverSpecificParameters;

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
%javaconst(1);

%include "ortools/linear_solver/wrappers/model_builder_helper.h"

%unignoreall

