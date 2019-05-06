// Copyright 2010-2018 Google LLC
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
%include "std_vector.i"

%include "ortools/base/base.i"
%include "ortools/util/csharp/proto.i"
%include "ortools/util/csharp/vector.i"

%{
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/swig_helper.h"
#include "ortools/util/sorted_interval_list.h"
%}

typedef int64_t int64;
typedef uint64_t uint64;

%module(directors="1") operations_research_sat

PROTO_INPUT(operations_research::sat::CpModelProto,
            Google.OrTools.Sat.CpModelProto,
            model_proto);

PROTO_INPUT(operations_research::sat::SatParameters,
            Google.OrTools.Sat.SatParameters,
            parameters);

PROTO_INPUT(operations_research::sat::CpSolverResponse,
            Google.OrTools.Sat.CpSolverResponse,
            response);

PROTO2_RETURN(operations_research::sat::CpSolverResponse,
              Google.OrTools.Sat.CpSolverResponse);

%template(SatInt64Vector) std::vector<int64>;
%template(SatInt64VectorVector) std::vector<std::vector<int64> >;
VECTOR_AS_CSHARP_ARRAY(int64, int64, long, SatInt64Vector);
JAGGED_MATRIX_AS_CSHARP_ARRAY(int64, int64, long, SatInt64VectorVector);

%ignoreall

// SatParameters are proto2, thus not compatible with C# Protobufs.
// We will use API with std::string parameters.

%unignore operations_research;
%unignore operations_research::sat;
%unignore operations_research::sat::SatHelper;
%unignore operations_research::sat::SatHelper::Solve;
%unignore operations_research::sat::SatHelper::SolveWithStringParameters;
%unignore operations_research::sat::SatHelper::SolveWithStringParametersAndSolutionCallback;
%unignore operations_research::sat::SatHelper::ModelStats;
%unignore operations_research::sat::SatHelper::SolverResponseStats;
%unignore operations_research::sat::SatHelper::ValidateModel;

%feature("director") operations_research::sat::SolutionCallback;
%unignore operations_research::sat::SolutionCallback;
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

%unignore operations_research::Domain;
%unignore operations_research::Domain::Domain;
%unignore operations_research::Domain::AllValues;
%unignore operations_research::Domain::Complement;
%unignore operations_research::Domain::Contains;
%unignore operations_research::Domain::FlattenedIntervals;
%unignore operations_research::Domain::FromFlatIntervals;
%rename (FromIntervals) operations_research::Domain::FromVectorIntervals;
%unignore operations_research::Domain::FromValues;
%unignore operations_research::Domain::IsEmpty;
%unignore operations_research::Domain::Max;
%unignore operations_research::Domain::Min;
%unignore operations_research::Domain::Negation;
%unignore operations_research::Domain::Size;

%include "ortools/sat/swig_helper.h"
%include "ortools/util/sorted_interval_list.h"

%unignoreall
