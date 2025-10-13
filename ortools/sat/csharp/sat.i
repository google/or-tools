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

%typemap(csimports) SWIGTYPE %{
using System;
using System.Runtime.InteropServices;
using System.Collections;
%}

%include "stdint.i"

%include "ortools/base/base.i"
%include "ortools/util/csharp/proto.i"

%import "ortools/util/csharp/sorted_interval_list.i"

%{
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/swig_helper.h"
%}

%module(directors="1") operations_research_sat

%typemap(csimports) operations_research::sat::CpSatHelper %{
using Google.OrTools.Util;
%}

%typemap(csimports) operations_research::sat::SolveWrapper %{
// Used to wrap log callbacks (std::function<void(const std::string&>)
public delegate void StringToVoidDelegate(string message);
// Used to wrap best bound callbacks (std::function<void(double>)
public delegate void DoubleToVoidDelegate(double bound);
%}

PROTO_INPUT(operations_research::sat::CpModelProto,
            Google.OrTools.Sat.CpModelProto,
            model_proto);

PROTO_INPUT(operations_research::sat::SatParameters,
            Google.OrTools.Sat.SatParameters,
            parameters);

PROTO_INPUT(operations_research::sat::CpSolverResponse,
            Google.OrTools.Sat.CpSolverResponse,
            response);

PROTO_INPUT(operations_research::sat::IntegerVariableProto,
            Google.OrTools.Sat.IntegerVariableProto,
            variable_proto);

PROTO2_RETURN(operations_research::sat::CpSolverResponse,
              Google.OrTools.Sat.CpSolverResponse);

%template(IntVector) std::vector<int>;
VECTOR_AS_CSHARP_ARRAY(int, int, int, IntVector);

%template(Int64Vector) std::vector<int64_t>;
%template(Int64VectorVector) std::vector<std::vector<int64_t> >;
VECTOR_AS_CSHARP_ARRAY(int64_t, int64_t, long, Int64Vector);
JAGGED_MATRIX_AS_CSHARP_ARRAY(int64_t, int64_t, long, Int64VectorVector);

%ignoreall

// SatParameters are proto2, thus not compatible with C# Protobufs.
// We will use API with std::string parameters.

%unignore operations_research;
%unignore operations_research::sat;

// Temporary wrapper class for the StringToVoidDelegate.
%feature("director") operations_research::sat::LogCallback;
%unignore operations_research::sat::LogCallback;
%unignore operations_research::sat::LogCallback::~LogCallback;
%unignore operations_research::sat::LogCallback::NewMessage;

// Temporary wrapper class for the DoubleToVoidDelegate.
%feature("director") operations_research::sat::BestBoundCallback;
%unignore operations_research::sat::BestBoundCallback;
%unignore operations_research::sat::BestBoundCallback::~BestBoundCallback;
%unignore operations_research::sat::BestBoundCallback::NewBestBound;


// Wrap the SolveWrapper class.
%unignore operations_research::sat::SolveWrapper;
%unignore operations_research::sat::SolveWrapper::SolveWrapper;
%unignore operations_research::sat::SolveWrapper::AddBestBoundCallbackFromClass;
%unignore operations_research::sat::SolveWrapper::AddLogCallbackFromClass;
%unignore operations_research::sat::SolveWrapper::AddSolutionCallback;
%unignore operations_research::sat::SolveWrapper::ClearSolutionCallback;
%unignore operations_research::sat::SolveWrapper::SetStringParameters;
%unignore operations_research::sat::SolveWrapper::Solve;
%unignore operations_research::sat::SolveWrapper::StopSearch;

// Wrap the CpSatHelper class.
%unignore operations_research::sat::CpSatHelper;
%unignore operations_research::sat::CpSatHelper::ModelStats;
%unignore operations_research::sat::CpSatHelper::SolverResponseStats;
%unignore operations_research::sat::CpSatHelper::ValidateModel;
%unignore operations_research::sat::CpSatHelper::VariableDomain;
%unignore operations_research::sat::CpSatHelper::WriteModelToFile;

%feature("director") operations_research::sat::SolutionCallback;
%unignore operations_research::sat::SolutionCallback;
%unignore operations_research::sat::SolutionCallback::SolutionCallback;
%unignore operations_research::sat::SolutionCallback::~SolutionCallback;
%unignore operations_research::sat::SolutionCallback::BestObjectiveBound;
%feature("nodirector") operations_research::sat::SolutionCallback::BestObjectiveBound;
%unignore operations_research::sat::SolutionCallback::NumBinaryPropagations;
%feature("nodirector") operations_research::sat::SolutionCallback::NumBinaryPropagations;
%unignore operations_research::sat::SolutionCallback::NumBooleans;
%feature("nodirector") operations_research::sat::SolutionCallback::NumBooleans;
%unignore operations_research::sat::SolutionCallback::NumBranches;
%feature("nodirector") operations_research::sat::SolutionCallback::NumBooleans;
%unignore operations_research::sat::SolutionCallback::NumConflicts;
%feature("nodirector") operations_research::sat::SolutionCallback::NumConflicts;
%unignore operations_research::sat::SolutionCallback::NumIntegerPropagations;
%feature("nodirector") operations_research::sat::SolutionCallback::NumIntegerPropagations;
%unignore operations_research::sat::SolutionCallback::ObjectiveValue;
%feature("nodirector") operations_research::sat::SolutionCallback::ObjectiveValue;
%unignore operations_research::sat::SolutionCallback::OnSolutionCallback;
%unignore operations_research::sat::SolutionCallback::Response;
%feature("nodirector") operations_research::sat::SolutionCallback::Response;
%unignore operations_research::sat::SolutionCallback::SolutionBooleanValue;
%feature("nodirector") operations_research::sat::SolutionCallback::SolutionBooleanValue;
%unignore operations_research::sat::SolutionCallback::SolutionIntegerValue;
%feature("nodirector") operations_research::sat::SolutionCallback::SolutionIntegerValue;
%unignore operations_research::sat::SolutionCallback::StopSearch;
%feature("nodirector") operations_research::sat::SolutionCallback::StopSearch;
%unignore operations_research::sat::SolutionCallback::UserTime;
%feature("nodirector") operations_research::sat::SolutionCallback::UserTime;
%unignore operations_research::sat::SolutionCallback::WallTime;
%feature("nodirector") operations_research::sat::SolutionCallback::WallTime;

%include "ortools/sat/swig_helper.h"

%unignoreall
