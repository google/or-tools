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
%include "ortools/util/java/vector.swig"

%{
#include "ortools/linear_solver/wrappers/model_builder_helper.h"
%}

%module operations_research_modelbuilder

// This typemap is inspired by the constraints_solver java typemaps.
// The only difference is that the argument is not a basic type, and needs
// processing to be passed to the std::function.
//
// TODO(user): cleanup java/functions.swig and move the code there.
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

%extend operations_research::mb::ModelBuilderHelper {
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
}  // Extend operations_research::mb::ModelBuilderHelper

%ignoreall

%unignore operations_research;
%unignore operations_research::mb;

// Wrap the ModelBuilderHelper class.
%unignore operations_research::mb::ModelBuilderHelper;
%unignore operations_research::mb::ModelBuilderHelper::ModelBuilderHelper;
%unignore operations_research::mb::ModelBuilderHelper::~ModelBuilderHelper;

// Var API.
%rename (addVar) operations_research::mb::ModelBuilderHelper::AddVar;
%rename (getVarIntegrality) operations_research::mb::ModelBuilderHelper::VarIsIntegral;
%rename (getVarLowerBound) operations_research::mb::ModelBuilderHelper::VarLowerBound;
%rename (getVarName) operations_research::mb::ModelBuilderHelper::VarName;
%rename (getVarObjectiveCoefficient) operations_research::mb::ModelBuilderHelper::VarObjectiveCoefficient;
%rename (getVarUpperBound) operations_research::mb::ModelBuilderHelper::VarUpperBound;
%rename (setVarIntegrality) operations_research::mb::ModelBuilderHelper::SetVarIntegrality;
%rename (setVarLowerBound) operations_research::mb::ModelBuilderHelper::SetVarLowerBound;
%rename (setVarName) operations_research::mb::ModelBuilderHelper::SetVarName;
%rename (setVarObjectiveCoefficient) operations_research::mb::ModelBuilderHelper::SetVarObjectiveCoefficient;
%rename (setVarUpperBound) operations_research::mb::ModelBuilderHelper::SetVarUpperBound;

// Linear Constraint API.
%rename (addConstraintTerm) operations_research::mb::ModelBuilderHelper::AddConstraintTerm;
%rename (addLinearConstraint) operations_research::mb::ModelBuilderHelper::AddLinearConstraint;
%rename (clearConstraintTerms) operations_research::mb::ModelBuilderHelper::ClearConstraintTerms;
%rename (getConstraintCoefficients) operations_research::mb::ModelBuilderHelper::ConstraintCoefficients;
%rename (getConstraintLowerBound) operations_research::mb::ModelBuilderHelper::ConstraintLowerBound;
%rename (getConstraintName) operations_research::mb::ModelBuilderHelper::ConstraintName;
%rename (getConstraintUpperBound) operations_research::mb::ModelBuilderHelper::ConstraintUpperBound;
%rename (getConstraintVarIndices) operations_research::mb::ModelBuilderHelper::ConstraintVarIndices;
%rename (safeAddConstraintTerm) operations_research::mb::ModelBuilderHelper::SafeAddConstraintTerm;
%rename (setConstraintCoefficient) operations_research::mb::ModelBuilderHelper::SetConstraintCoefficient;
%rename (setConstraintLowerBound) operations_research::mb::ModelBuilderHelper::SetConstraintLowerBound;
%rename (setConstraintName) operations_research::mb::ModelBuilderHelper::SetConstraintName;
%rename (setConstraintUpperBound) operations_research::mb::ModelBuilderHelper::SetConstraintUpperBound;

// Enforced Linear Constraint API.
%rename (addEnforcedConstraintTerm) operations_research::mb::ModelBuilderHelper::AddEnforcedConstraintTerm;
%rename (addEnforcedLinearConstraint) operations_research::mb::ModelBuilderHelper::AddEnforcedLinearConstraint;
%rename (clearEnforcedConstraintTerms) operations_research::mb::ModelBuilderHelper::ClearEnforcedConstraintTerms;
%rename (getEnforcedConstraintCoefficients) operations_research::mb::ModelBuilderHelper::EnforcedConstraintCoefficients;
%rename (getEnforcedConstraintLowerBound) operations_research::mb::ModelBuilderHelper::EnforcedConstraintLowerBound;
%rename (getEnforcedConstraintName) operations_research::mb::ModelBuilderHelper::EnforcedConstraintName;
%rename (getEnforcedConstraintUpperBound) operations_research::mb::ModelBuilderHelper::EnforcedConstraintUpperBound;
%rename (getEnforcedConstraintVarIndices) operations_research::mb::ModelBuilderHelper::EnforcedConstraintVarIndices;
%rename (getEnforcedIndicatorValue) operations_research::mb::ModelBuilderHelper::EnforcedIndicatorValue;
%rename (getEnforcedIndicatorVariableIndex) operations_research::mb::ModelBuilderHelper::EnforcedIndicatorVariableIndex;
%rename (isEnforcedConstraint) operations_research::mb::ModelBuilderHelper::IsEnforcedConstraint;
%rename (safeAddEnforcedConstraintTerm) operations_research::mb::ModelBuilderHelper::SafeAddEnforcedConstraintTerm;
%rename (setEnforcedConstraintCoefficient) operations_research::mb::ModelBuilderHelper::SetEnforcedConstraintCoefficient;
%rename (setEnforcedConstraintLowerBound) operations_research::mb::ModelBuilderHelper::SetEnforcedConstraintLowerBound;
%rename (setEnforcedConstraintName) operations_research::mb::ModelBuilderHelper::SetEnforcedConstraintName;
%rename (setEnforcedConstraintUpperBound) operations_research::mb::ModelBuilderHelper::SetEnforcedConstraintUpperBound;
%rename (setEnforcedIndicatorValue) operations_research::mb::ModelBuilderHelper::SetEnforcedIndicatorValue;
%rename (setEnforcedIndicatorVariableIndex) operations_research::mb::ModelBuilderHelper::SetEnforcedIndicatorVariableIndex;

// Objective API.
%rename (clearObjective) operations_research::mb::ModelBuilderHelper::ClearObjective;
%rename (getMaximize) operations_research::mb::ModelBuilderHelper::maximize;
%rename (setMaximize) operations_research::mb::ModelBuilderHelper::SetMaximize;
%rename (getObjectiveOffset) operations_research::mb::ModelBuilderHelper::ObjectiveOffset;
%rename (setObjectiveOffset) operations_research::mb::ModelBuilderHelper::SetObjectiveOffset;

// Hints.
%rename (clearHints) operations_research::mb::ModelBuilderHelper::ClearHints;
%rename (addHint) operations_research::mb::ModelBuilderHelper::AddHint;

// Model API.
%rename (numVariables) operations_research::mb::ModelBuilderHelper::num_variables;
%rename (numConstraints) operations_research::mb::ModelBuilderHelper::num_constraints;
%rename (getName) operations_research::mb::ModelBuilderHelper::name;
%rename (setName) operations_research::mb::ModelBuilderHelper::SetName;
%rename (readModelFromProtoFile) operations_research::mb::ModelBuilderHelper::ReadModelFromProtoFile;
%rename (writeModelToProtoFile) operations_research::mb::ModelBuilderHelper::WriteModelToProtoFile;
%rename (importFromMpsString) operations_research::mb::ModelBuilderHelper::ImportFromMpsString;
%rename (importFromMpsFile) operations_research::mb::ModelBuilderHelper::ImportFromMpsFile;
%rename (importFromLpString) operations_research::mb::ModelBuilderHelper::ImportFromLpString;
%rename (importFromLpFile) operations_research::mb::ModelBuilderHelper::ImportFromLpFile;
%unignore operations_research::mb::ModelBuilderHelper::exportToMpsString;
%unignore operations_research::mb::ModelBuilderHelper::exportToLpString;
%unignore operations_research::mb::ModelBuilderHelper::writeToMpsFile;
%rename (overwriteModel) operations_research::mb::ModelBuilderHelper::OverwriteModel;

%unignore operations_research::mb::ModelSolverHelper;
%unignore operations_research::mb::ModelSolverHelper::ModelSolverHelper(const std::string&);
%rename (solverIsSupported) operations_research::mb::ModelSolverHelper::SolverIsSupported;
%rename (solve) operations_research::mb::ModelSolverHelper::Solve;
%rename (interruptSolve) operations_research::mb::ModelSolverHelper::InterruptSolve;
%rename (hasResponse) operations_research::mb::ModelSolverHelper::has_response;
%rename (hasSolution) operations_research::mb::ModelSolverHelper::has_solution;
%rename (getStatus) operations_research::mb::ModelSolverHelper::status;
%rename (getObjectiveValue) operations_research::mb::ModelSolverHelper::objective_value;
%rename (getBestObjectiveBound) operations_research::mb::ModelSolverHelper::best_objective_bound;
%rename (getVariableValue) operations_research::mb::ModelSolverHelper::variable_value;
%rename (getReducedCost) operations_research::mb::ModelSolverHelper::reduced_cost;
%rename (getDualValue) operations_research::mb::ModelSolverHelper::dual_value;
%rename (getActivity) operations_research::mb::ModelSolverHelper::activity;
%rename (getStatusString) operations_research::mb::ModelSolverHelper::status_string;
%rename (getWallTime) operations_research::mb::ModelSolverHelper::wall_time;
%rename (getUserTime) operations_research::mb::ModelSolverHelper::user_time;
%rename (enableOutput) operations_research::mb::ModelSolverHelper::EnableOutput;
%rename (clearLogCallback) operations_research::mb::ModelSolverHelper::ClearLogCallback;
%rename (setLogCallback) operations_research::mb::ModelSolverHelper::SetLogCallback;
%rename (setTimeLimitInSeconds) operations_research::mb::ModelSolverHelper::SetTimeLimitInSeconds;
%rename (setSolverSpecificParameters) operations_research::mb::ModelSolverHelper::SetSolverSpecificParameters;

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
%javaconst(1);

%include "ortools/linear_solver/wrappers/model_builder_helper.h"

%unignoreall

