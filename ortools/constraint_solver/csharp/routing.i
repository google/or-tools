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

// TODO(user): Refactor this file to adhere to the SWIG style guide.
%include "ortools/constraint_solver/csharp/constraint_solver.i"
%include "ortools/constraint_solver/csharp/routing_types.i"
%include "ortools/constraint_solver/csharp/routing_index_manager.i"

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
#include "ortools/constraint_solver/routing_index_manager.h"
#include "ortools/constraint_solver/routing_parameters.h"
#include "ortools/constraint_solver/routing_parameters.pb.h"
#include "ortools/constraint_solver/routing_types.h"
%}

%module(directors="1") operations_research;

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

// RoutingModel
namespace operations_research {
// Define the delegate for Transit callback types.
// This replace the RoutingTransitCallback[1-2] in the C# proxy class
%typemap(csimports) RoutingModel %{
  using System.Collections.Generic; // List<>

  public delegate long UnaryTransitCallback(long fromIndex);
  public delegate long TransitCallback(long fromIndex, long toIndex);
%}

// Keep reference to delegate to avoid GC to collect them early
%typemap(cscode) RoutingModel %{
  // Store list of delegate to avoid the GC to reclaim them.
  private List<UnaryTransitCallback> unaryTransitCallbacks;
  private List<TransitCallback> transitCallbacks;
  private IndexEvaluator2 indexEvaluator2Callback;

  // Ensure that the GC does not collect any TransitCallback set from C#
  // as the underlying C++ class stores a shallow copy
  private UnaryTransitCallback StoreUnaryTransitCallback(UnaryTransitCallback c) {
    if (unaryTransitCallbacks == null) unaryTransitCallbacks = new List<UnaryTransitCallback>();
    unaryTransitCallbacks.Add(c);
    return c;
  }
  private TransitCallback StoreTransitCallback(TransitCallback c) {
    if (transitCallbacks == null) transitCallbacks = new List<TransitCallback>();
    transitCallbacks.Add(c);
    return c;
  }
  // only use in RoutingModel::SetFirstSolutionEvaluator()
  private IndexEvaluator2 StoreIndexEvaluator2(IndexEvaluator2 c) {
    indexEvaluator2Callback = c;
    return c;
  }
%}

// Types in Proxy class (foo.cs) e.g.:
// Foo::f(cstype $csinput, ...) {Foo_f_SWIG(csin, ...);}
%typemap(cstype, out="IntPtr") RoutingTransitCallback1 "UnaryTransitCallback"
%typemap(csin) RoutingTransitCallback1 "StoreUnaryTransitCallback($csinput)"
%typemap(cstype, out="IntPtr") RoutingTransitCallback2 "TransitCallback"
%typemap(csin) RoutingTransitCallback2 "StoreTransitCallback($csinput)"
// Type in the prototype of PINVOKE function.
%typemap(imtype, out="IntPtr") RoutingTransitCallback1 "UnaryTransitCallback"
%typemap(imtype, out="IntPtr") RoutingTransitCallback2 "TransitCallback"

// Type use in module_csharp_wrap.h function declaration.
// since SWIG generate code as: `ctype argX` we can't use a C function pointer type.
%typemap(ctype) RoutingTransitCallback1 "void*" // "int64 (*)(int64)"
%typemap(ctype) RoutingTransitCallback2 "void*" // "int64 (*)(int64, int64)"

// Convert in module_csharp_wrap.cc input argument (delegate marshaled in C function pointer) to original std::function<...>
%typemap(in) RoutingTransitCallback1  %{
  $1 = [$input](int64 fromIndex) -> int64 {
    return (*(int64 (*)(int64))$input)(fromIndex);
    };
%}
%typemap(in) RoutingTransitCallback2  %{
  $1 = [$input](int64 fromIndex, int64 toIndex) -> int64 {
    return (*(int64 (*)(int64, int64))$input)(fromIndex, toIndex);};
%}
}  // namespace operations_research

// Add PickupAndDeliveryPolicy enum value to RoutingModel (like RoutingModel::Status)
// For C++11 strongly typed enum SWIG support see https://github.com/swig/swig/issues/316
%extend operations_research::RoutingModel {
  static const operations_research::RoutingModel::PickupAndDeliveryPolicy ANY =
  operations_research::RoutingModel::PickupAndDeliveryPolicy::ANY;
  static const operations_research::RoutingModel::PickupAndDeliveryPolicy LIFO =
  operations_research::RoutingModel::PickupAndDeliveryPolicy::LIFO;
  static const operations_research::RoutingModel::PickupAndDeliveryPolicy FIFO =
  operations_research::RoutingModel::PickupAndDeliveryPolicy::FIFO;
}

%rename("GetStatus") operations_research::RoutingModel::status;
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


%include "ortools/constraint_solver/routing_parameters.h"
%include "ortools/constraint_solver/routing.h"
