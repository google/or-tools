// Copyright 2010-2017 Google
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

// This .i file exposes the sat cp_model API.

%include "ortools/base/base.i"
%include "ortools/util/java/proto.i"

%{
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/swig_helper.h"
%}

%module(directors="1") operations_research_sat

PROTO_INPUT(operations_research::sat::CpModelProto,
            com.google.ortools.sat.CpModelProto,
            model_proto);

PROTO_INPUT(operations_research::sat::SatParameters,
            com.google.ortools.sat.SatParameters,
            parameters);

PROTO2_RETURN(
    operations_research::sat::CpSolverResponse,
    com.google.ortools.sat.CpSolverResponse);

%ignoreall

%unignore operations_research;
%unignore operations_research::sat;
%unignore operations_research::sat::SatHelper;
%unignore operations_research::sat::SatHelper::Solve;
%unignore operations_research::sat::SatHelper::SolveWithParameters;
// We use the director version of the API.
%unignore operations_research::sat::SatHelper::SolveWithParametersAndSolutionCallback;

// --------- Include the swig helpers file to create the director classes ------
// We cannot use %ignoreall/%unignoreall as this is not compatible with nested
// swig files.

%feature("director") operations_research::sat::SolutionCallback;

%unignore operations_research::sat::SolutionCallback;
%rename (numBinaryPropagations) operations_research::sat::SolutionCallback::NumBinaryPropagations;
%rename (numBooleans) operations_research::sat::SolutionCallback::NumBooleans;
%rename (numBranches) operations_research::sat::SolutionCallback::NumBranches;
%rename (numConflicts) operations_research::sat::SolutionCallback::NumConflicts;
%rename (numIntegerPropagations) operations_research::sat::SolutionCallback::NumIntegerPropagations;
%rename (objectiveValue) operations_research::sat::SolutionCallback::ObjectiveValue;
%rename (onSolutionCallback) operations_research::sat::SolutionCallback::OnSolutionCallback;
%rename (solutionBooleanValue) operations_research::sat::SolutionCallback::SolutionBooleanValue;
%rename (solutionIntegerValue) operations_research::sat::SolutionCallback::SolutionIntegerValue;
%rename (userTime) operations_research::sat::SolutionCallback::UserTime;
%rename (wallTime) operations_research::sat::SolutionCallback::WallTime;

%include "ortools/sat/swig_helper.h"

%unignoreall
