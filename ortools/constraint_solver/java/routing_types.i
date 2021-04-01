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

// Typemaps for Routing Index Types. This does not define any type wrappings,
// because these index types are are never exposed to the target language.
// Instead, indices are manipulated as native target language types (e.g. Java
// int).
// This file is to be %included when wrapped objects need to use these typemaps.

%include "ortools/base/base.i"
%import "ortools/util/java/vector.i"

%{
#include "ortools/constraint_solver/routing_types.h"
%}

// This macro defines typemaps for IndexT, std::vector<IndexT> and
// std::vector<std::vector<IndexT>>.
%define DEFINE_INDEX_TYPE(IndexT)

// Convert IndexT to (32-bit signed) integers.
%typemap(jstype) IndexT "int"
%typemap(javain) IndexT "$javainput"
%typemap(jtype) IndexT "int"
%typemap(jni) IndexT "jint"
%typemap(in) IndexT {
  $1 = IndexT($input);
}
%typemap(out) IndexT {
  $result = (jint)$1.value();
}
%typemap(javaout) IndexT {
  return $jnicall;
}

// Convert std::vector<IndexT> to/from int arrays.
VECTOR_AS_JAVA_ARRAY(IndexT, int, Int);
MATRIX_AS_JAVA_ARRAY(IndexT, int, Int);

%enddef  // DEFINE_INDEX_TYPE

// This macro applies all typemaps for a given index type to a typedef.
// Normally we should not need that as SWIG is supposed to automatically apply
// all typemaps to typedef definitions (http://www.swig.org/Doc2.0/SWIGDocumentation.html#Typemaps_typedef_reductions),
// but this is not actually the case.
%define DEFINE_INDEX_TYPE_TYPEDEF(IndexT, NewIndexT)
%apply IndexT { NewIndexT };
%apply std::vector<IndexT> { std::vector<NewIndexT> };
%apply const std::vector<IndexT>& { std::vector<NewIndexT>& };
%apply const std::vector<std::vector<IndexT> >& { const std::vector<std::vector<NewIndexT> >& };
%enddef  // DEFINE_INDEX_TYPE_TYPEDEF

DEFINE_INDEX_TYPE(operations_research::RoutingNodeIndex);
DEFINE_INDEX_TYPE(operations_research::RoutingCostClassIndex);
DEFINE_INDEX_TYPE(operations_research::RoutingDimensionIndex);
DEFINE_INDEX_TYPE(operations_research::RoutingDisjunctionIndex);
DEFINE_INDEX_TYPE(operations_research::RoutingVehicleClassIndex);

