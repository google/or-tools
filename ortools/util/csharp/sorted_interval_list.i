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
%include "ortools/util/csharp/vector.i"

%{
#include "ortools/util/sorted_interval_list.h"
%}

typedef int64_t int64;
typedef uint64_t uint64;

%module(directors="1") operations_research_util

%template(UtilInt64Vector) std::vector<int64>;
%template(UtilInt64VectorVector) std::vector<std::vector<int64> >;
VECTOR_AS_CSHARP_ARRAY(int64, int64, long, UtilInt64Vector);
JAGGED_MATRIX_AS_CSHARP_ARRAY(int64, int64, long, UtilInt64VectorVector);

%ignoreall

// SatParameters are proto2, thus not compatible with C# Protobufs.
// We will use API with std::string parameters.

%unignore operations_research;

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

%include "ortools/util/sorted_interval_list.h"

%unignoreall
