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

// TODO(user): Refactor this file to adhere to the SWIG style guide.

%typemap(csimports) SWIGTYPE %{
using System;
using System.Runtime.InteropServices;
using System.Collections;
using System.Collections.Generic;
%}

%include "ortools/base/base.i"
%template(IntBoolPair) std::pair<int, bool>;
%include "enumsimple.swg"
%import "ortools/util/csharp/absl_string_view.i"
%import "ortools/util/csharp/vector.i"

%{
#include <algorithm>
%}

%template(IntVector) std::vector<int>;
%template(IntVectorVector) std::vector<std::vector<int> >;
VECTOR_AS_CSHARP_ARRAY(int, int, int, IntVector);
JAGGED_MATRIX_AS_CSHARP_ARRAY(int, int, int, IntVectorVector);

%template(Int64Vector) std::vector<int64_t>;
%template(Int64VectorVector) std::vector<std::vector<int64_t> >;
VECTOR_AS_CSHARP_ARRAY(int64_t, int64_t, long, Int64Vector);
JAGGED_MATRIX_AS_CSHARP_ARRAY(int64_t, int64_t, long, Int64VectorVector);

%import "ortools/constraint_solver/csharp/constraint_solver.i"
%import "ortools/util/csharp/sorted_interval_list.i"  // Domain

%include "ortools/routing/csharp/index_manager.i"

// We need to forward-declare the proto here, so that PROTO_INPUT involving it
// works correctly. The order matters very much: this declaration needs to be
// before the %{ #include ".../routing.h" %}.
namespace operations_research::routing {
// enum.proto
struct FirstSolutionStrategy;
struct LocalSearchMetaheuristic;
struct RoutingSearchStatus;
// ils.proto
class IteratedLocalSearchParameters;
// parameters.proto
class RoutingModelParameters;
class RoutingSearchParameters;
}  // namespace operations_research::routing

%module(directors="1") RoutingGlobals;

// Include the files we want to wrap a first time.
%{
#include "ortools/routing/enums.pb.h"
#include "ortools/routing/ils.pb.h"
#include "ortools/routing/types.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/routing.h"
#include "ortools/util/optional_boolean.pb.h"
%}

namespace operations_research::routing {

// GlobalVehicleBreaksConstraint
%unignore GlobalVehicleBreaksConstraint;
%typemap(csimports) GlobalVehicleBreaksConstraint %{
using Google.OrTools.ConstraintSolver;
%}

// PathsMetadata
%unignore PathsMetadata;

// Routing Dimension
%unignore Dimension;
%typemap(csimports) Dimension %{
using System;
using System.Collections.Generic;
using Google.OrTools.ConstraintSolver;
%}
%typemap(cscode) Dimension %{
  // Keep reference to delegate to avoid GC to collect them early.
  private List<IntIntToLong> limitCallbacks;
  private IntIntToLong StoreIntIntToLong(IntIntToLong limit) {
    if (limitCallbacks == null)
      limitCallbacks = new List<IntIntToLong>();
    limitCallbacks.Add(limit);
    return limit;
  }

  private List<LongLongToLong> groupDelayCallbacks;
  private LongLongToLong StoreLongLongToLong(LongLongToLong groupDelay) {
    if (groupDelayCallbacks == null)
      groupDelayCallbacks = new List<LongLongToLong>();
    groupDelayCallbacks.Add(groupDelay);
    return groupDelay;
  }
%}
%ignore Dimension::GetBreakDistanceDurationOfVehicle;

// Routing Model
%unignore Model;
%typemap(csimports) Model %{
using System;
using System.Collections.Generic;
using Google.OrTools.ConstraintSolver;
using Domain = Google.OrTools.Util.Domain;
%}
%typemap(cscode) Model %{
  // Keep reference to delegate to avoid GC to collect them early.
  private List<LongToLong> unaryTransitCallbacks;
  private LongToLong StoreLongToLong(LongToLong c) {
    if (unaryTransitCallbacks == null)
      unaryTransitCallbacks = new List<LongToLong>();
    unaryTransitCallbacks.Add(c);
    return c;
  }

  private List<LongLongToLong> transitCallbacks;
  private LongLongToLong StoreLongLongToLong(LongLongToLong c) {
    if (transitCallbacks == null)
      transitCallbacks = new List<LongLongToLong>();
    transitCallbacks.Add(c);
    return c;
  }

  private List<VoidToVoid> solutionCallbacks;
  private VoidToVoid StoreVoidToVoid(VoidToVoid c) {
    if (solutionCallbacks == null)
      solutionCallbacks = new List<VoidToVoid>();
    solutionCallbacks.Add(c);
    return c;
  }
%}
%rename("GetStatus") Model::status;
// Ignored:
%ignore Model::AddDimensionDependentDimensionWithVehicleCapacity;

%unignore Model::RegisterUnaryTransitVector;
%unignore Model::RegisterTransitMatrix;

%unignore Model::AddVectorDimension;
%unignore Model::AddMatrixDimension;

%ignore Model::AddSameVehicleRequiredTypeAlternatives;
%ignore Model::GetAllDimensionNames;
%ignore Model::GetAutomaticFirstSolutionStrategy;
%ignore Model::GetDeliveryIndexPairs;
%ignore Model::GetDimensions;
%ignore Model::GetDimensionsWithSoftAndSpanCosts;
%ignore Model::GetDimensionsWithSoftOrSpanCosts;
%ignore Model::GetGlobalDimensionCumulOptimizers;
%ignore Model::GetHardTypeIncompatibilitiesOfType;
%ignore Model::GetLocalDimensionCumulMPOptimizers;
%ignore Model::GetLocalDimensionCumulOptimizers;
%ignore Model::GetMutableGlobalCumulOptimizer;
%ignore Model::GetMutableLocalCumulOptimizer;
%ignore Model::GetMutableLocalCumulMPOptimizer;
%ignore Model::GetPerfectBinaryDisjunctions;
%ignore Model::GetPickupIndexPairs;
%ignore Model::HasTypeRegulations;
%ignore Model::MakeStateDependentTransit;
%ignore Model::PackCumulsOfOptimizerDimensionsFromAssignment;
%ignore Model::RegisterStateDependentTransitCallback;
%ignore Model::RemainingTime;
%ignore Model::StateDependentTransitCallback;
%ignore Model::SolveWithParameters(
    const RoutingSearchParameters& search_parameters,
    std::vector<const Assignment*>* solutions);
%ignore Model::SolveFromAssignmentWithParameters(
      const Assignment* assignment,
      const RoutingSearchParameters& search_parameters,
      std::vector<const Assignment*>* solutions);
%ignore Model::TransitCallback;
%ignore Model::UnaryTransitCallbackOrNull;

%ignore operations_research::routing::Model::AddResourceGroup;
%ignore operations_research::routing::Model::GetResourceGroups;

// ModelVisitor
%unignore ModelVisitor;
%typemap(csimports) ModelVisitor %{
using Google.OrTools.ConstraintSolver;
%}

// SimpleBoundCosts
%unignore BoundCost;
%unignore SimpleBoundCosts;
%rename("GetBoundCost") SimpleBoundCosts::bound_cost;
%rename("GetSize") SimpleBoundCosts::Size;

// TypeRegulationsConstraint
%unignore TypeRegulationsConstraint;
%typemap(csimports) TypeRegulationsConstraint %{
using Google.OrTools.ConstraintSolver;
%}

// TypeRegulationsChecker
%unignore TypeRegulationsChecker;
%ignore TypeRegulationsChecker::CheckVehicle;

}  // namespace operations_research::routing

%rename("%(camelcase)s", %$isfunction) "";

// Add needed import to RoutingGlobalsPINVOKE.cs
%pragma(csharp) imclassimports=%{
// Types from ConstraintSolver
using Google.OrTools.ConstraintSolver;
%}

// Protobuf support
PROTO_INPUT(operations_research::routing::RoutingModelParameters,
            Google.OrTools.Routing.RoutingModelParameters,
            parameters)
PROTO_INPUT(operations_research::routing::RoutingSearchParameters,
            Google.OrTools.Routing.RoutingSearchParameters,
            search_parameters)
PROTO2_RETURN(operations_research::routing::IteratedLocalSearchParameters,
              Google.OrTools.Routing.IteratedLocalSearchParameters)
PROTO2_RETURN(operations_research::routing::RoutingModelParameters,
              Google.OrTools.Routing.RoutingModelParameters)
PROTO2_RETURN(operations_research::routing::RoutingSearchParameters,
              Google.OrTools.Routing.RoutingSearchParameters)
PROTO_ENUM_RETURN(operations_research::routing::FirstSolutionStrategy::Value,
                  Google.OrTools.Routing.FirstSolutionStrategy.Types.Value)
PROTO_ENUM_RETURN(operations_research::routing::RoutingSearchStatus::Value,
                  Google.OrTools.Routing.RoutingSearchStatus.Types.Value)

// Add needed import to RoutingGlobals.cs
%pragma(csharp) moduleimports=%{
// Types from ConstraintSolver
using Google.OrTools.ConstraintSolver;
%}

// Wrap parameters.h according to the SWIG style guide.
%ignoreall
%unignore operations_research::routing;
namespace operations_research::routing {
// parameters.h
// IMPORTANT: These functions are global, so in .Net, they are in the
// RoutingGlobals.cs file.
%unignore DefaultRoutingSearchParameters;
%unignore DefaultRoutingModelParameters;
%unignore DefaultSecondaryRoutingSearchParameters;
%unignore DefaultIteratedLocalSearchParameters;
%unignore FindErrorInRoutingSearchParameters;
%unignore FindErrorsInRoutingSearchParameters;
}  // namespace operations_research::routing

%include "ortools/routing/parameters.h"
%unignoreall

// Wrap routing includes
// TODO(user): Replace with %ignoreall/%unignoreall
//swiglint: disable include-h-allglobals
%include "ortools/routing/routing.h"
