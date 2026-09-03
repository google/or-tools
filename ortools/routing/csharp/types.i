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

// Typemaps for Routing Index Types. This does not define any type wrappings,
// because these index types are are never exposed to the target language.
// Instead, indices are manipulated as native target language types (e.g. C#
// int).
// This file is to be %included when wrapped objects need to use these typemaps.

%include "ortools/base/base.i"
%import "ortools/util/csharp/vector.i"

%{
#include "ortools/routing/types.h"
%}

// This macro defines typemaps for IndexT, std::vector<IndexT> and
// std::vector<std::vector<IndexT>>.
%define DEFINE_INDEX_TYPE(IndexT)

// Convert IndexT to (32-bit signed) integers.
%typemap(cstype) IndexT "int"
%typemap(csin) IndexT "$csinput"
%typemap(imtype) IndexT "int"
%typemap(ctype) IndexT "int"
%typemap(in) IndexT {
  $1 = IndexT($input);
}
%typemap(out) IndexT {
  $result = $1.value();
}
%typemap(csout) IndexT {
  return $imcall;
}
%typemap(csvarin) IndexT
%{
   set { $imcall; }
%}
%typemap(csvarout, excode=SWIGEXCODE)  IndexT
%{
  get {
        return $imcall;
  }
%}

// Convert std::vector<IndexT> to/from int arrays.
VECTOR_AS_CSHARP_ARRAY(IndexT, int, int, IntVector);
// Convert std::vector<std::vector<IndexT>> to/from two-dimensional int arrays.
JAGGED_MATRIX_AS_CSHARP_ARRAY(IndexT, int, int, IntVectorVector);

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

DEFINE_INDEX_TYPE(operations_research::routing::NodeIndex);
DEFINE_INDEX_TYPE(operations_research::routing::CostClassIndex);
DEFINE_INDEX_TYPE(operations_research::routing::DimensionIndex);
DEFINE_INDEX_TYPE(operations_research::routing::DisjunctionIndex);
DEFINE_INDEX_TYPE(operations_research::routing::VehicleClassIndex);
DEFINE_INDEX_TYPE(operations_research::routing::ResourceClassIndex);

%ignoreall

%unignore operations_research::routing;
namespace operations_research::routing {
class NodeIndex;
class CostClassIndex;
class DimensionIndex;
class DisjunctionIndex;
class VehicleClassIndex;
class ResourceClassIndex;
}  // namespace operations_research::routing

%unignore operations_research::routing::PickupDeliveryPair;
%unignore operations_research::routing::TransitCallback1(int64_t);
%unignore operations_research::routing::TransitCallback2(int64_t, int64_t);

%include "ortools/routing/types.h"

%unignoreall
