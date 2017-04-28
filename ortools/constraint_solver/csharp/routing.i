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
#include "ortools/constraint_solver/routing.h"
%}

%module(directors="1") operations_research;
%feature("director") swig_util::NodeEvaluator2;

// Convert RoutingModel::NodeIndex to (32-bit signed) integers.
%typemap(ctype) operations_research::RoutingModel::NodeIndex "int"
%typemap(imtype) operations_research::RoutingModel::NodeIndex "int"
%typemap(cstype) operations_research::RoutingModel::NodeIndex "int"
%typemap(csin) operations_research::RoutingModel::NodeIndex "$csinput"
%typemap(csout) operations_research::RoutingModel::NodeIndex {
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

// Create input mapping for NodeEvaluator2 callback wrapping.
// TODO(user): split out the callback code; it creates another file since
// it uses a different module.
%{
namespace swig_util {
class NodeEvaluator2 : private operations_research::RoutingModel::NodeEvaluator2 {
 public:
  NodeEvaluator2() : used_as_permanent_handler_(false) {}

  virtual ~NodeEvaluator2() {}

  virtual int64 Run(int i, int j) = 0;

  operations_research::RoutingModel::NodeEvaluator2* GetPermanentCallback() {
    CHECK(!used_as_permanent_handler_);
    used_as_permanent_handler_ = true;
    return this;
  }

 private:
  virtual bool IsRepeatable() const { return true; }

  virtual int64 Run(operations_research::RoutingModel::NodeIndex i,
                    operations_research::RoutingModel::NodeIndex j) {
    return Run(i.value(), j.value());
  }

  bool used_as_permanent_handler_;
};
}  // namespace swig_util
%}

// We prefer keeping this C# extension here (instead of moving it to a dedicated
// C# file, using partial classes) because it uses SWIG's $descriptor().
%typemap(cscode) swig_util::NodeEvaluator2 %{
  public $descriptor(operations_research::RoutingModel::NodeEvaluator2*) DisownAndGetPermanentCallback() {
    swigCMemOwn = false;
    return GetPermanentCallback();
  }
%}

namespace swig_util {
class NodeEvaluator2 : private ::operations_research::RoutingModel::NodeEvaluator2 {
 public:
  NodeEvaluator2();
  virtual int64 Run(int i, int j) = 0;
  ::operations_research::RoutingModel::NodeEvaluator2* GetPermanentCallback();
  virtual ~NodeEvaluator2();

 private:
  virtual bool IsRepeatable() const;
  virtual int64 Run(::operations_research::RoutingModel::NodeIndex i,
                    ::operations_research::RoutingModel::NodeIndex j);
  bool used_as_permanent_handler_;
};
}  // namespace swig_util


// Typemaps for NodeEvaluator2 callbacks in csharp.
%typemap(cstype) operations_research::RoutingModel::NodeEvaluator2* "NodeEvaluator2";
%typemap(csin) operations_research::RoutingModel::NodeEvaluator2* "$descriptor(ResultCallback2<int64, RoutingNodeIndex, RoutingNodeIndex>*).getCPtr($csinput.DisownAndGetPermanentCallback())";

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

%ignore operations_research::RoutingModel::WrapIndexEvaluator(
    Solver::IndexEvaluator2* evaluator);

%ignore operations_research::RoutingModel::RoutingModel(
    int nodes, int vehicles,
    const std::vector<std::pair<NodeIndex, NodeIndex> >& start_end);

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
%include "ortools/constraint_solver/routing.h"
