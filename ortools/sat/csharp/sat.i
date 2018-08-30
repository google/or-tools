// Copyright 2010-2014 Google
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

// This .swig file exposes the sat cp_model API.

%typemap(csimports) SWIGTYPE %{
using System;
using System.Runtime.InteropServices;
using System.Collections;
%}

%include "ortools/base/base.i"
%include "ortools/util/csharp/proto.i"

%{
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/swig_helper.h"
%}

%module(directors="1") operations_research_sat

PROTO_INPUT(operations_research::sat::CpModelProto,
            Google.OrTools.Sat.CpModelProto,
            model_proto);

PROTO_INPUT(operations_research::sat::SatParameters,
            Google.OrTools.Sat.SatParameters,
            parameters);

PROTO2_RETURN(
    operations_research::sat::CpSolverResponse,
    Google.OrTools.Sat.CpSolverResponse);

%ignoreall

// SatParameters are proto2, thus not compatible with C# Protobufs.
// We will use API with string parameters.

%unignore operations_research;
%unignore operations_research::sat;
%unignore operations_research::sat::SatHelper;
%unignore operations_research::sat::SatHelper::Solve;
%unignore operations_research::sat::SatHelper::SolveWithStringParameters;
// We use the director version of the API.
%unignore operations_research::sat::SatHelper::SolveWithStringParametersAndSolutionCallback;

// --------- Include the swig helpers file to create the director classes ------
// We cannot use %ignoreall/%unignoreall as this is not compatible with nested
// swig files.

%feature("director") operations_research::sat::SolutionCallback;

%unignore operations_research::sat::SolutionCallback;
%unignore operations_research::sat::SolutionCallback::NumBinaryPropagations;
%unignore operations_research::sat::SolutionCallback::NumBooleans;
%unignore operations_research::sat::SolutionCallback::NumBranches;
%unignore operations_research::sat::SolutionCallback::NumConflicts;
%unignore operations_research::sat::SolutionCallback::NumIntegerPropagations;
%unignore operations_research::sat::SolutionCallback::ObjectiveValue;
%unignore operations_research::sat::SolutionCallback::OnSolutionCallback;
%unignore operations_research::sat::SolutionCallback::SolutionBooleanValue;
%unignore operations_research::sat::SolutionCallback::SolutionIntegerValue;
%unignore operations_research::sat::SolutionCallback::StopSearch;
%unignore operations_research::sat::SolutionCallback::UserTime;
%unignore operations_research::sat::SolutionCallback::WallTime;

%include "ortools/sat/swig_helper.h"

%unignoreall
