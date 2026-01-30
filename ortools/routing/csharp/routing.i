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
//%unignore FindErrorsInRoutingSearchParameters;  // vector<string> not wrapped
}  // namespace operations_research::routing

%include "ortools/routing/parameters.h"
%unignoreall

// Routing
// Wrap routing.h according to the SWIG style guide.
%ignoreall
%unignore operations_research::routing;
namespace operations_research::routing {

// GlobalVehicleBreaksConstraint
%unignore GlobalVehicleBreaksConstraint;
%typemap(csimports) GlobalVehicleBreaksConstraint %{
using Google.OrTools.ConstraintSolver;
%}

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
%unignore Dimension::Dimension;
%unignore Dimension::~Dimension;
%unignore Dimension::CumulVar;
%unignore Dimension::GetQuadraticCostSoftSpanUpperBoundForVehicle;
%unignore Dimension::GetSoftSpanUpperBoundForVehicle;
%unignore Dimension::HasQuadraticCostSoftSpanUpperBounds;
%unignore Dimension::HasSoftSpanUpperBounds;
%unignore Dimension::SetBreakIntervalsOfVehicle;
%unignore Dimension::SetCumulVarSoftUpperBound;
%unignore Dimension::SetGlobalSpanCostCoefficient;
%unignore Dimension::SetQuadraticCostSoftSpanUpperBoundForVehicle;
%unignore Dimension::SetSoftSpanUpperBoundForVehicle;
%unignore Dimension::SlackVar;
%unignore Dimension::TransitVar;

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
%unignore Model::Model;
%unignore Model::~Model;
%unignore Model::AddAtSolutionCallback;
%unignore Model::AddConstantDimensionWithSlack;
%unignore Model::AddDimension;
%unignore Model::AddDimensionWithVehicleCapacity;
%unignore Model::AddDisjunction;
%unignore Model::AddMatrixDimension;
%unignore Model::AddPickupAndDelivery;
%unignore Model::AddVariableMinimizedByFinalizer;
%unignore Model::AddVectorDimension;
%unignore Model::CostVar;
%unignore Model::CumulVar;
%unignore Model::End;
%unignore Model::GetArcCostForVehicle;
%unignore Model::GetDimensionOrDie;
%unignore Model::GetMutableDimension;
%unignore Model::IsEnd;
%unignore Model::IsStart;
%unignore Model::IsVehicleUsed;
%unignore Model::NextVar;
%unignore Model::ReadAssignmentFromRoutes;
%unignore Model::RegisterTransitCallback;
%unignore Model::RegisterTransitMatrix;
%unignore Model::RegisterUnaryTransitCallback;
%unignore Model::RegisterUnaryTransitVector;
%unignore Model::SetArcCostEvaluatorOfAllVehicles;
%unignore Model::SetArcCostEvaluatorOfVehicle;
%unignore Model::SetPickupAndDeliveryPolicyOfAllVehicles;
%unignore Model::Size;
%unignore Model::SolveFromAssignmentWithParameters(const operations_research::Assignment*, const RoutingSearchParameters&);
%unignore Model::SolveWithParameters(const RoutingSearchParameters&);
%unignore Model::Start;
%unignore Model::VehicleVar;
%rename("GetStatus") Model::status;
// Model nested enum
%unignore Model::PenaltyCostBehavior;
%unignore Model::PENALIZE_ONCE;
%unignore Model::PENALIZE_PER_INACTIVE;
%unignore Model::PickupAndDeliveryPolicy;
%unignore Model::PICKUP_AND_DELIVERY_FIFO;
%unignore Model::PICKUP_AND_DELIVERY_LIFO;
%unignore Model::PICKUP_AND_DELIVERY_NO_ORDER;
%unignore Model::VisitTypePolicy;
%unignore Model::TYPE_ADDED_TO_VEHICLE;
%unignore Model::ADDED_TYPE_REMOVED_FROM_VEHICLE;
%unignore Model::TYPE_ON_VEHICLE_UP_TO_VISIT;
%unignore Model::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED;

// ModelVisitor
%unignore ModelVisitor;
%typemap(csimports) ModelVisitor %{
using Google.OrTools.ConstraintSolver;
%}

// PathsMetadata
%unignore PathsMetadata;

// BoundCosts
%unignore BoundCost;
%unignore BoundCost::BoundCost;
%unignore BoundCost::bound;
%unignore BoundCost::cost;

%unignore SimpleBoundCosts;
%unignore SimpleBoundCosts::SimpleBoundCosts;
%rename("GetBoundCost") SimpleBoundCosts::bound_cost;
%rename("GetSize") SimpleBoundCosts::Size;

// TypeRegulationsConstraint
%unignore TypeRegulationsConstraint;
%typemap(csimports) TypeRegulationsConstraint %{
using Google.OrTools.ConstraintSolver;
%}

// TypeRegulationsChecker
%unignore TypeRegulationsChecker;

}  // namespace operations_research::routing

%include "ortools/routing/routing.h"
%unignoreall
