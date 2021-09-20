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

// This .i file exposes the sorted_interval_list API.

%include "stdint.i"

%include "ortools/base/base.i"
%import "ortools/util/python/vector.i"

%{
#include "ortools/util/sorted_interval_list.h"
%}

%ignoreall

%unignore operations_research;

%unignore operations_research::Domain;
%unignore operations_research::Domain::Domain;

%unignore operations_research::Domain::AdditionWith;
%unignore operations_research::Domain::AllValues;
%unignore operations_research::Domain::Complement;
%unignore operations_research::Domain::Contains;
%unignore operations_research::Domain::FlattenedIntervals;
%unignore operations_research::Domain::FromFlatIntervals;
%rename (FromIntervals) operations_research::Domain::FromVectorIntervals;
%unignore operations_research::Domain::FromValues;
%unignore operations_research::Domain::IntersectionWith;
%unignore operations_research::Domain::IsEmpty;
%unignore operations_research::Domain::Max;
%unignore operations_research::Domain::Min;
%unignore operations_research::Domain::Negation;
%unignore operations_research::Domain::Size;
%rename (__str__) operations_research::Domain::ToString;
%unignore operations_research::Domain::UnionWith;

%include "ortools/util/sorted_interval_list.h"

%unignoreall
