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

%include "stdint.i"

%include "ortools/base/base.i"

%include "ortools/util/java/proto.i"

%import "ortools/util/java/sorted_interval_list.i"

%{
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/swig_helper.h"
#include "ortools/util/sorted_interval_list.h"

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

%module(directors="1") operations_research_sat

%typemap(javaimports) operations_research::sat::CpSatHelper %{
import com.google.ortools.util.Domain;
%}

PROTO_INPUT(operations_research::sat::CpModelProto,
            com.google.ortools.sat.CpModelProto,
            model_proto);

PROTO_INPUT(operations_research::sat::SatParameters,
            com.google.ortools.sat.SatParameters,
            parameters);

PROTO_INPUT(operations_research::sat::IntegerVariableProto,
            com.google.ortools.sat.IntegerVariableProto,
            variable_proto);

PROTO_INPUT(operations_research::sat::CpSolverResponse,
            com.google.ortools.sat.CpSolverResponse,
            response);

PROTO2_RETURN(operations_research::sat::CpSolverResponse,
              com.google.ortools.sat.CpSolverResponse);

// This typemap is inspired by the constraints_solver java typemaps.
// The only difference is that the argument is not a basic type, and needs
// processing to be passed to the std::function.
//
// TODO(user): cleanup java/functions.i and move the code there.
%{
#include <memory> // std::make_shared<GlobalRefGuard>
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

%ignoreall

%unignore operations_research;
%unignore operations_research::sat;

// Wrap the SolveWrapper class.
%unignore operations_research::sat::SolveWrapper;
%rename (addLogCallback) operations_research::sat::SolveWrapper::AddLogCallback;
%rename (addSolutionCallback) operations_research::sat::SolveWrapper::AddSolutionCallback;
%rename (clearSolutionCallback) operations_research::sat::SolveWrapper::ClearSolutionCallback;
%rename (setParameters) operations_research::sat::SolveWrapper::SetParameters;
%rename (solve) operations_research::sat::SolveWrapper::Solve;
%rename (stopSearch) operations_research::sat::SolveWrapper::StopSearch;

// Wrap the relevant part of the CpSatHelper.
%unignore operations_research::sat::CpSatHelper;
%rename (modelStats) operations_research::sat::CpSatHelper::ModelStats;
%rename (solverResponseStats) operations_research::sat::CpSatHelper::SolverResponseStats;
%rename (validateModel) operations_research::sat::CpSatHelper::ValidateModel;
%rename (variableDomain) operations_research::sat::CpSatHelper::VariableDomain;
%rename (writeModelToFile) operations_research::sat::CpSatHelper::WriteModelToFile;

// We use directors for the solution callback.
%feature("director") operations_research::sat::SolutionCallback;

%unignore operations_research::sat::SolutionCallback;
%unignore operations_research::sat::SolutionCallback::~SolutionCallback;
%rename (bestObjectiveBound) operations_research::sat::SolutionCallback::BestObjectiveBound;
%rename (numBinaryPropagations) operations_research::sat::SolutionCallback::NumBinaryPropagations;
%rename (numBooleans) operations_research::sat::SolutionCallback::NumBooleans;
%rename (numBranches) operations_research::sat::SolutionCallback::NumBranches;
%rename (numConflicts) operations_research::sat::SolutionCallback::NumConflicts;
%rename (numIntegerPropagations) operations_research::sat::SolutionCallback::NumIntegerPropagations;
%rename (objectiveValue) operations_research::sat::SolutionCallback::ObjectiveValue;
%rename (onSolutionCallback) operations_research::sat::SolutionCallback::OnSolutionCallback;
%rename (solutionBooleanValue) operations_research::sat::SolutionCallback::SolutionBooleanValue;
%rename (solutionIntegerValue) operations_research::sat::SolutionCallback::SolutionIntegerValue;
%rename (stopSearch) operations_research::sat::SolutionCallback::StopSearch;
%rename (userTime) operations_research::sat::SolutionCallback::UserTime;
%rename (wallTime) operations_research::sat::SolutionCallback::WallTime;

%include "ortools/sat/swig_helper.h"

%unignoreall
