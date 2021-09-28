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

%include "ortools/base/base.i"
%include "ortools/util/python/proto.i"

%{
#include "ortools/scheduling/rcpsp_parser.h"
#include "ortools/scheduling/rcpsp.pb.h"
%}

PY_PROTO_TYPEMAP(ortools.scheduling.rcpsp_pb2,
                 RcpspProblem,
                 ::operations_research::scheduling::rcpsp::RcpspProblem);

%ignoreall

%unignore operations_research;
%unignore operations_research::scheduling;
%unignore operations_research::scheduling::rcpsp;
%unignore operations_research::scheduling::rcpsp::RcpspParser;
%unignore operations_research::scheduling::rcpsp::RcpspParser::RcpspParser();
%rename (Problem) operations_research::scheduling::rcpsp::RcpspParser::problem;
%unignore operations_research::scheduling::rcpsp::RcpspParser::ParseFile(const std::string& file_name);

%include "ortools/scheduling/rcpsp_parser.h"

%unignoreall
