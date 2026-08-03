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

%module(directors="1") JniSolver
%include "ortools/base/base.i"
%include "ortools/util/java/proto.i"
%include "ortools/util/java/status.swig"
%import "ortools/util/java/java_swig_solve_interrupter.swig"
%{
#include "ortools/math_opt/core/java/solver.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
%}
%feature("director") operations_research::math_opt::CppMessageCallback;
%feature("director") operations_research::math_opt::CppSolveCallback;
PROTO_INPUT(operations_research::math_opt::ModelProto, com.google.ortools.mathopt.ModelProto, model);
PROTO2_ENUM_INPUT_COMMON(operations_research::math_opt::SolverTypeProto, com.google.ortools.mathopt.SolverTypeProto, solverType);
PROTO_INPUT(operations_research::math_opt::SolveParametersProto, com.google.ortools.mathopt.SolveParametersProto, solveParameters);
PROTO_INPUT(operations_research::math_opt::ModelSolveParametersProto, com.google.ortools.mathopt.ModelSolveParametersProto, modelSolveParameters);
PROTO_INPUT(operations_research::math_opt::CallbackRegistrationProto, com.google.ortools.mathopt.CallbackRegistrationProto, cbRegistration);
PROTO2_RETURN(operations_research::math_opt::SolveResultProto, com.google.ortools.mathopt.SolveResultProto);
STATUSOR_RETURN(double);
STATUSOR_RETURN(operations_research::math_opt::SolveResultProto);
PROTO2_CALLBACK_ARGUMENT(operations_research::math_opt::CallbackDataProto,
    com.google.ortools.mathopt.CallbackDataProto,
    "com/google/ortools/mathopt/CallbackDataProto",
    cbData);
PROTO2_CALLBACK_RETURN(operations_research::math_opt::CallbackResultProto,
    com.google.ortools.mathopt.CallbackResultProto,
    "com/google/ortools/mathopt/CallbackResultProto");

%pragma(java) moduleimports=%{
import com.google.ortools.util.JavaSwigSolveInterrupter;
%}
%pragma(java) jniclassimports=%{
import com.google.ortools.util.JavaSwigSolveInterrupter;
%}

%ignoreall

%unignore operations_research;
%unignore operations_research::math_opt;

%unignore operations_research::math_opt::CppMessageCallback;
%unignore operations_research::math_opt::CppMessageCallback::~CppMessageCallback;
%unignore operations_research::math_opt::CppMessageCallback::onMessage(const std::string&);

%unignore operations_research::math_opt::CppSolveCallback;
%unignore operations_research::math_opt::CppSolveCallback::~CppSolveCallback;
%unignore operations_research::math_opt::CppSolveCallback::onEvent(const operations_research::math_opt::CallbackDataProto&);

%unignore operations_research::math_opt::cppSolve(
    const operations_research::math_opt::ModelProto&,
    operations_research::math_opt::SolverTypeProto,
    const operations_research::math_opt::SolveParametersProto&,
    const operations_research::math_opt::ModelSolveParametersProto&,
    operations_research::math_opt::CppMessageCallback*,
    const operations_research::math_opt::CallbackRegistrationProto&,
    operations_research::math_opt::CppSolveCallback*,
    operations_research::JavaSwigSolveInterrupter*);
%include "ortools/math_opt/core/java/solver.h"

%unignoreall
