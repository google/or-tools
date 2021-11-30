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

// This .i file exposes the sat cp_model API.

%include "ortools/base/base.i"
%include "ortools/util/python/proto.i"

// std::function utilities.
%include "ortools/util/python/functions.i"

// import the domain class.
%import "ortools/util/python/sorted_interval_list.i"

%{
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/swig_helper.h"
%}

%module(directors="1", threads="1") operations_research_sat

%pythoncode {
import numbers
}

PY_PROTO_TYPEMAP(ortools.sat.cp_model_pb2,
                 CpModelProto,
                 operations_research::sat::CpModelProto);

PY_PROTO_TYPEMAP(ortools.sat.cp_model_pb2,
                 CpSolverResponse,
                 operations_research::sat::CpSolverResponse);

PY_PROTO_TYPEMAP(ortools.sat.sat_parameters_pb2,
                 SatParameters,
                 operations_research::sat::SatParameters);


%ignoreall

%unignore operations_research;
%unignore operations_research::sat;

// Wrap the SolveWrapper class.
%unignore operations_research::sat::SolveWrapper;
%unignore operations_research::sat::SolveWrapper::AddLogCallback;
%unignore operations_research::sat::SolveWrapper::AddSolutionCallback;
%unignore operations_research::sat::SolveWrapper::ClearSolutionCallback;
%unignore operations_research::sat::SolveWrapper::SetParameters;
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
%unignore operations_research::sat::SolutionCallback::~SolutionCallback;
%unignore operations_research::sat::SolutionCallback::BestObjectiveBound;
%feature("nodirector") operations_research::sat::SolutionCallback::BestObjectiveBound;
%unignore operations_research::sat::SolutionCallback::HasResponse;
%feature("nodirector") operations_research::sat::SolutionCallback::HasResponse;
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

