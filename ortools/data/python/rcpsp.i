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

%include "ortools/base/base.i"
%include "ortools/util/python/proto.i"

%{
#include "ortools/data/rcpsp_parser.h"
#include "ortools/data/rcpsp.pb.h"
%}

PY_PROTO_TYPEMAP(ortools.data.rcpsp_pb2,
                 RcpspProblem,
                 ::operations_research::data::rcpsp::RcpspProblem);

%ignoreall

%unignore operations_research;
%unignore operations_research::data;
%unignore operations_research::data::rcpsp;
%unignore operations_research::data::rcpsp::RcpspParser;
%unignore operations_research::data::rcpsp::RcpspParser::RcpspParser;
%rename (Problem) operations_research::data::rcpsp::RcpspParser::problem;
%unignore operations_research::data::rcpsp::RcpspParser::ParseFile;

%include "ortools/data/rcpsp_parser.h"

%unignoreall
