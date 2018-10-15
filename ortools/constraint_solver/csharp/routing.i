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

// TODO(user): Refactor this file to adhere to the SWIG style guide.
%include "ortools/constraint_solver/csharp/constraint_solver.i"

// We need to forward-declare the proto here, so that PROTO_INPUT involving it
// works correctly. The order matters very much: this declaration needs to be
// before the %{ #include ".../routing.h" %}.
namespace operations_research {
class RoutingModelParameters;
class RoutingSearchParameters;
}  // namespace operations_research

// Include the file we want to wrap a first time.
%{
#include "ortools/constraint_solver/routing_types.h"
#include "ortools/constraint_solver/routing_parameters.h"
#include "ortools/constraint_solver/routing.h"
%}

%module(directors="1") operations_research;
%feature("director") LongResultCallback1;
%feature("director") LongResultCallback2;
%feature("director") LongResultCallback3;

// Convert RoutingNodeIndex to (32-bit signed) integers.
%typemap(ctype) operations_research::RoutingNodeIndex "int"
%typemap(imtype) operations_research::RoutingNodeIndex "int"
%typemap(cstype) operations_research::RoutingNodeIndex "int"
%typemap(csin) operations_research::RoutingNodeIndex "$csinput"
%typemap(csout) operations_research::RoutingNodeIndex {
  return $imcall;
}
%typemap(in) operations_research::RoutingModel::NodeIndex {
  $1 = operations_research::RoutingModel::NodeIndex($input);
}
%typemap(out) operations_research::RoutingModel::NodeIndex {
  $result = $1.value();
}
%typemap(csvarin) operations_research::RoutingModel::NodeIndex
%{
        set { $imcall; }
%}
%typemap(csvarout, excode=SWIGEXCODE)  operations_research::RoutingModel::NodeIndex
%{
  get {
        return $imcall;
  }
%}

// This typemap maps C# TypeA[] arrays to C++ std::vector<TypeB>, where
// TypeA and TypeB are strictly equivalent, bit-wise (like an IntType
// and an int, for example). This is done via an intermediate mapping
// to pairs (int length, TypeA*).
//
// We need to redefine it ourselves it because std_vector.i will only
// allow these conversions when TypeA == TypeB.
%define CS_TYPEMAP_STDVECTOR(TYPE, CTYPE, CSHARPTYPE)
%typemap(ctype)    const std::vector<TYPE>&  %{ int length$argnum, CTYPE* %}
%typemap(imtype)   const std::vector<TYPE>&  %{ int length$argnum, CSHARPTYPE[] %}
%typemap(cstype)   const std::vector<TYPE>&  %{ CSHARPTYPE[] %}
%typemap(csin)     const std::vector<TYPE>&  "$csinput.Length, $csinput"
%typemap(freearg)  const std::vector<TYPE>&  { delete $1; }
%typemap(in)       const std::vector<TYPE>&  %{
  $1 = new std::vector<TYPE>;
  $1->reserve(length$argnum);
  for(int i = 0; i < length$argnum; ++i) {
    $1->emplace_back($input[i]);
  }
%}
%enddef // CS_TYPEMAP_STDVECTOR

CS_TYPEMAP_STDVECTOR(operations_research::RoutingModel::NodeIndex, int, int);

// Ditto, for bi-dimensional arrays: map C# TypeA[][] to C++
// std::vector<std::vector<TypeB>>.
%define CS_TYPEMAP_STDVECTOR_IN1(TYPE, CTYPE, CSHARPTYPE)
%typemap(ctype)  const std::vector<std::vector<TYPE> >&  %{
  int len$argnum_1, int len$argnum_2[], CTYPE*
%}
%typemap(imtype) const std::vector<std::vector<TYPE> >&  %{
  int len$argnum_1, int[] len$argnum_2, CSHARPTYPE[]
%}
%typemap(cstype) const std::vector<std::vector<TYPE> >&  %{ CSHARPTYPE[][] %}
%typemap(csin)   const std::vector<std::vector<TYPE> >&  "$csinput.GetLength(0), NestedArrayHelper.GetArraySecondSize($csinput), NestedArrayHelper.GetFlatArray($csinput)"
%typemap(in)     const std::vector<std::vector<TYPE> >&  (std::vector<std::vector<TYPE> > result) %{

  result.clear();
  result.resize(len$argnum_1);

  TYPE* inner_array = reinterpret_cast<TYPE*>($input);

  int actualIndex = 0;
  for (int index1 = 0; index1 < len$argnum_1; ++index1) {
    result[index1].reserve(len$argnum_2[index1]);
    for (int index2 = 0; index2 < len$argnum_2[index1]; ++index2) {
      const TYPE value = inner_array[actualIndex];
      result[index1].emplace_back(value);
      actualIndex++;
    }
  }

  $1 = &result;
%}
%enddef // CS_TYPEMAP_STDVECTOR_IN1

CS_TYPEMAP_STDVECTOR_IN1(operations_research::RoutingModel::NodeIndex, int, int);

// Create input mapping for LongResultCallbackN.
// Callback wrapping.
// TODO(user): split out the callback code; it creates another file since
// it uses a different module.
%{
class LongResultCallback1 {
 public:
  virtual int64 Run(int64) = 0;
  ResultCallback1<int64, int64>* GetPermanentCallback() {
    return NewPermanentCallback(this, &LongResultCallback1::Run);
  }
  virtual ~LongResultCallback1() {}
};
class LongResultCallback2 {
 public:
  virtual int64 Run(int64, int64) = 0;
  ResultCallback2<int64, int64, int64>* GetPermanentCallback() {
    return NewPermanentCallback(this, &LongResultCallback2::Run);
  }
  virtual ~LongResultCallback2() {}
};
class LongResultCallback3 {
 public:
  virtual int64 Run(int64, int64, int64) = 0;
  ResultCallback3<int64, int64, int64, int64>* GetPermanentCallback() {
    return NewPermanentCallback(this, &LongResultCallback3::Run);
  }
  virtual ~LongResultCallback3() {}
};

%}

class LongResultCallback1 {
 public:
  virtual int64 Run(int64) = 0;
  ResultCallback1<int64, int64>* GetPermanentCallback();
  virtual ~LongResultCallback1();
};
class LongResultCallback2 {
 public:
  virtual int64 Run(int64, int64) = 0;
  ResultCallback2<int64, int64, int64>* GetPermanentCallback();
  virtual ~LongResultCallback2();
};
class LongResultCallback3 {
 public:
  virtual int64 Run(int64, int64, int64) = 0;
  ResultCallback3<int64, int64, int64, int64>* GetPermanentCallback();
  virtual ~LongResultCallback3();
};

// Typemaps for callbacks in csharp.
%typemap(cstype) ResultCallback1<int64, int64>* "LongResultCallback1";
%typemap(csin) ResultCallback1<int64, int64>*
    "$descriptor(ResultCallback1<int64, int64>*)
     .getCPtr($csinput.GetPermanentCallback())";
%typemap(cstype) ResultCallback2<int64, int64, int64>* "LongResultCallback2";
%typemap(csin) ResultCallback2<int64, int64, int64>*
    "$descriptor(ResultCallback2<int64, int64, int64>*)
     .getCPtr($csinput.GetPermanentCallback())";
%typemap(cstype) ResultCallback3<int64, int64, int64, int64>*
    "LongResultCallback3";
%typemap(csin) ResultCallback3<int64, int64, int64, int64>*
    "$descriptor(ResultCallback3<int64, int64, int64, int64>*)
     .getCPtr($csinput.GetPermanentCallback())";

%rename (AddDimensionAux) operations_research::RoutingModel::AddDimension;
%rename (AddDimensionWithVehicleCapacityAux) operations_research::RoutingModel::AddDimensionWithVehicleCapacity;
%rename (SetArcCostEvaluatorOfAllVehiclesAux) operations_research::RoutingModel::SetArcCostEvaluatorOfAllVehicles;
%rename (SetArcCostEvaluatorOfVehicleAux) operations_research::RoutingModel::SetArcCostEvaluatorOfVehicle;
%rename (RoutingModelStatus) operations_research::RoutingModel::Status;

%ignore operations_research::RoutingModel::AddVectorDimension(
    const int64* values,
    int64 capacity,
    const std::string& name);

%ignore operations_research::RoutingModel::AddMatrixDimension(
    const int64* const* values,
    int64 capacity,
    const std::string& name);

%ignore operations_research::RoutingModel::RegisterStateDependentTransitCallback;
%ignore operations_research::RoutingModel::StateDependentTransitCallback;
%ignore operations_research::RoutingModel::MakeStateDependentTransit;
%ignore operations_research::RoutingModel::AddDimensionDependentDimensionWithVehicleCapacity;

%extend operations_research::RoutingModel {
  void AddVectorDimension(const std::vector<int64>& values,
                          int64 capacity,
                          bool fix_start_cumul_to_zero,
                          const std::string& name) {
    DCHECK_EQ(values.size(), self->nodes());
    self->AddVectorDimension(values.data(), capacity,
                             fix_start_cumul_to_zero, name);
  }
}

%ignore operations_research::RoutingModel::RoutingModel(
    const RoutingIndexManager&);
%ignore operations_research::RoutingModel::RoutingModel(
    const RoutingIndexManager&, const RoutingModelParameters&);

%rename("%(camelcase)s", %$isfunction) "";

// Protobuf support
PROTO_INPUT(operations_research::RoutingSearchParameters,
            Google.OrTools.ConstraintSolver.RoutingSearchParameters,
            search_parameters)
PROTO_INPUT(operations_research::RoutingModelParameters,
            Google.OrTools.ConstraintSolver.RoutingModelParameters,
            parameters)
PROTO2_RETURN(operations_research::RoutingSearchParameters,
              Google.OrTools.ConstraintSolver.RoutingSearchParameters)
PROTO2_RETURN(operations_research::RoutingModelParameters,
              Google.OrTools.ConstraintSolver.RoutingModelParameters)


%include "ortools/constraint_solver/routing_types.h"
%include "ortools/constraint_solver/routing_parameters.h"
%include "ortools/constraint_solver/routing.h"
