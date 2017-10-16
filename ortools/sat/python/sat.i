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

// This .i file exposes the sat cp_model API.

%include "ortools/base/base.i"
//%include "net/proto/swig/protofunc.i"
%include "ortools/util/python/proto.i"

%{
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/swig_helper.h"
%}

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
%unignore operations_research::sat::SatHelper;
%unignore operations_research::sat::SatHelper::Solve;
%unignore operations_research::sat::SatHelper::SolveWithParameters;
%unignore operations_research::sat::SatHelper::SolveWithParametersAndSolutionObserver;

%include "ortools/sat/swig_helper.h"

%unignoreall
