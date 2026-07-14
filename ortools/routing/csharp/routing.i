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
%typemap(csdirectorin) const std::vector<int64_t>& "new Int64Vector($iminput, false).ToArray()"

%feature("director") operations_research::routing::RouteConstraintCallback;

%inline %{
namespace operations_research::routing {
/**
 * Result returned by RouteConstraintCallback::Evaluate().
 *
 * Set is_satisfied to false when the route violates the constraint.
 * In that case the solver should filter out the solution and cost is ignored.
 * Set is_satisfied to true and populate cost with the route penalty or cost
 * to apply.
 *
 * Example:
 * \code
 * var result = new RouteConstraintResult {
 *   is_satisfied = true,
 *   cost = 42,
 * };
 * \endcode
 */
struct RouteConstraintResult {
  bool is_satisfied = false;
  int64_t cost = 0;
};

/**
 * Implement this class in C# to inspect a route and return an optional cost.
 *
 * Override Evaluate(long[] route) and return a RouteConstraintResult.
 * Return is_satisfied = false when the route violates the constraint, in
 * which case the solver will filter out the solution. Return
 * is_satisfied = true and set cost to the penalty to apply.
 * The callback runs after the route is built, so it can use the full sequence
 * of visited nodes to compute a penalty.
 *
 * Example:
 * \code
 * private sealed class PenalizeLongRoutes : RouteConstraintCallback {
 *   public override RouteConstraintResult Evaluate(long[] route) {
 *     return new RouteConstraintResult {
 *       is_satisfied = route.Length > 10,
 *       cost = route.Length > 10 ? 100 : 0,
 *     };
 *   }
 * }
 *
 * model.AddRouteConstraint(new PenalizeLongRoutes());
 * \endcode
 */
class RouteConstraintCallback {
 public:
  RouteConstraintCallback() = default;
  virtual ~RouteConstraintCallback() = default;
  /**
   * Evaluates a complete route and returns an optional penalty.
   */
  virtual RouteConstraintResult Evaluate(
      const std::vector<int64_t>& route) const {
    return RouteConstraintResult();
  }
};
}  // namespace operations_research::routing
%}

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

  public IntVarVector Cumuls() {
    return cumuls();
  }

  public IntVarVector Slacks() {
    return slacks();
  }

  public IntVarVector Transits() {
    return transits();
  }
%}
%unignore Dimension::Dimension;
%unignore Dimension::~Dimension;
%unignore Dimension::CumulVar;
%unignore Dimension::FixedTransitVar;
%unignore Dimension::GetCumulVarMax;
%unignore Dimension::GetCumulVarMin;
%unignore Dimension::GetQuadraticCostSoftSpanUpperBoundForVehicle;
%unignore Dimension::GetSoftSpanUpperBoundForVehicle;
%unignore Dimension::GetSpanCostCoefficientForVehicle;
%unignore Dimension::GetTransitValue;
%unignore Dimension::GetTransitValueFromClass;
%unignore Dimension::HasPickupToDeliveryLimits;
%unignore Dimension::HasQuadraticCostSoftSpanUpperBounds;
%unignore Dimension::HasSoftSpanUpperBounds;
%unignore Dimension::InitializeBreaks;
%unignore Dimension::model;
%unignore Dimension::SetBreakIntervalsOfVehicle;
%unignore Dimension::SetBreakDistanceDurationOfVehicle;
%unignore Dimension::SetCumulVarSoftLowerBound;
%unignore Dimension::SetCumulVarRange;
%unignore Dimension::SetCumulVarSoftUpperBound;
%unignore Dimension::SetGlobalSpanCostCoefficient;
%unignore Dimension::SetPickupToDeliveryLimitFunctionForPair;
%unignore Dimension::SetQuadraticCostSoftSpanUpperBoundForVehicle;
%unignore Dimension::SetSlackCostCoefficientForVehicle;
%unignore Dimension::SetSlackCostCoefficientForAllVehicles;
%unignore Dimension::SetSpanCostCoefficientForVehicle;
%unignore Dimension::SetSpanCostCoefficientForAllVehicles;
%unignore Dimension::SetSpanUpperBoundForVehicle;
%unignore Dimension::SetSoftSpanUpperBoundForVehicle;
%csmethodmodifiers Dimension::cumuls "private";
%unignore Dimension::cumuls;
%csmethodmodifiers Dimension::slacks "private";
%unignore Dimension::slacks;
%unignore Dimension::SlackVar;
%csmethodmodifiers Dimension::transits "private";
%unignore Dimension::transits;
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

  private List<RouteConstraintCallback> routeConstraintCallbacks;
  private RouteConstraintCallback StoreRouteConstraintCallback(
      RouteConstraintCallback c) {
    if (routeConstraintCallbacks == null)
      routeConstraintCallbacks = new List<RouteConstraintCallback>();
    routeConstraintCallbacks.Add(c);
    return c;
  }

  private sealed class RouteConstraintOptionalCostCallback
      : RouteConstraintCallback {
    private readonly Func<long[], long?> routeEvaluator;

    public RouteConstraintOptionalCostCallback(
        Func<long[], long?> routeEvaluator) {
      this.routeEvaluator = routeEvaluator;
    }

    public override RouteConstraintResult Evaluate(long[] route) {
      long? cost = routeEvaluator(route);
      if (!cost.HasValue) {
        return new RouteConstraintResult {
            is_satisfied = false,
            cost = 0,
        };
      }
      return new RouteConstraintResult {
          is_satisfied = true,
          cost = cost.Value,
      };
    }
  }

  // Convenience overload: null means the route violates the constraint.
  public void AddRouteConstraint(
      Func<long[], long?> routeEvaluator,
      bool costs_are_homogeneous_across_vehicles = false) {
    AddRouteConstraint(
        new RouteConstraintOptionalCostCallback(routeEvaluator),
        costs_are_homogeneous_across_vehicles);
  }
%}
%typemap(csin) operations_research::routing::RouteConstraintCallback* %{ RouteConstraintCallback.getCPtr(StoreRouteConstraintCallback($csinput)) %}

%feature("director") operations_research::routing::RouteConstraintCallback;
%unignore operations_research::routing::RouteConstraintResult;
%unignore operations_research::routing::RouteConstraintResult::RouteConstraintResult;
%unignore operations_research::routing::RouteConstraintResult::is_satisfied;
%unignore operations_research::routing::RouteConstraintResult::cost;
%unignore operations_research::routing::RouteConstraintCallback;
%unignore operations_research::routing::RouteConstraintCallback::RouteConstraintCallback;
%unignore operations_research::routing::RouteConstraintCallback::~RouteConstraintCallback;
%unignore operations_research::routing::RouteConstraintCallback::Evaluate;

%unignore Model::Model;
%unignore Model::~Model;
%unignore Model::AddAtSolutionCallback;
%unignore Model::AddEnterSearchCallback;
%unignore Model::AddSearchMonitor;
%unignore Model::AddConstantDimensionWithSlack;
%unignore Model::AddConstantDimension;
%unignore Model::AddDimension;
%unignore Model::AddDimensionWithCumulDependentVehicleTransitAndCapacity;
%unignore Model::AddDimensionWithVehicleTransits;
%unignore Model::AddDimensionWithVehicleCapacity;
%unignore Model::AddDimensionWithVehicleTransitAndCapacity;
%unignore Model::AddDisjunction;
%unignore Model::AddHardTypeIncompatibility;
%unignore Model::AddMatrixDimension;
%unignore Model::AddPickupAndDelivery;
%unignore Model::AddRequiredTypeAlternativesWhenAddingType;
%unignore Model::AddTemporalTypeIncompatibility;
%unignore Model::AddRouteConstraint;
%unignore Model::AddToAssignment;
%unignore Model::AddVariableMinimizedByFinalizer;
%unignore Model::AddVectorDimension;
%unignore Model::ActiveVar;
%unignore Model::ActiveVehicleVar;
%unignore Model::ApplyLocks;
%unignore Model::ApplyLocks(const std::vector<int64_t>&);
%unignore Model::ApplyLocksToAllVehicles;
%unignore Model::CancelSearch;
%unignore Model::CloseModelWithParameters;
%unignore Model::CompactAssignment;
%unignore Model::CostVar;
%unignore Model::CostsAreHomogeneousAcrossVehicles;
%unignore Model::CumulVar;
%unignore Model::CumulDependentTransitCallback;
%unignore Model::End;
%unignore Model::FastSolveFromAssignmentWithParameters;
%unignore Model::GetAllDimensionNames;
%unignore Model::GetAmortizedLinearCostFactorOfVehicles;
%unignore Model::GetAmortizedQuadraticCostFactorOfVehicles;
%unignore Model::GetArcCostForVehicle;
%unignore Model::GetAutomaticFirstSolutionStrategy;
%unignore Model::GetDimensionOrDie;
%unignore Model::GetDimensions;
%unignore Model::GetDisjunctionIndices;
%unignore Model::GetDisjunctionMaxCardinality;
%unignore Model::GetDisjunctionMinCardinality;
%unignore Model::GetDisjunctionSoftMaxCardinality;
%unignore Model::GetDisjunctionSoftMaxPenalty;
%unignore Model::GetDisjunctionSoftMaxPenaltyCostBehavior;
%unignore Model::GetDisjunctionSoftMinCardinality;
%unignore Model::GetDisjunctionSoftMinPenalty;
%unignore Model::GetDisjunctionSoftMinPenaltyCostBehavior;
%unignore Model::GetDisjunctionPenalty;
%unignore Model::GetFixedCostOfVehicle;
%unignore Model::GetNumberOfDecisionsInFirstSolution;
%unignore Model::GetNumberOfRejectsInFirstSolution;
%unignore Model::GetNumberOfDisjunctions;
%unignore Model::GetVehicleClassCount;
%unignore Model::GetVehicleClassesCount;
%unignore Model::GetVisitType;
%unignore Model::GetMutableDimension;
%unignore Model::IsDelivery;
%unignore Model::IsEnd;
%unignore Model::IsPickup;
%unignore Model::IsStart;
%unignore Model::Next;
%unignore Model::VehicleIndex;
%unignore Model::IsVehicleAllowedForIndex;
%unignore Model::IsVehicleUsedWhenEmpty;
%unignore Model::IsVehicleUsed;
%unignore Model::MakeDisjunction;
%unignore Model::SetDisjunctionHardMaximum;
%unignore Model::SetDisjunctionHardMinimum;
%unignore Model::SetDisjunctionSoftMaximum;
%unignore Model::SetDisjunctionSoftMinimum;
%unignore Model::NextVar;
%unignore Model::Nexts;
%unignore Model::ReadAssignment;
%unignore Model::ReadAssignmentFromRoutes;
%unignore Model::RegisterCumulDependentTransitCallback;
%unignore Model::RegisterTransitCallback;
%unignore Model::RegisterTransitMatrix;
%unignore Model::RegisterUnaryTransitCallback;
%unignore Model::RegisterUnaryTransitVector;
%unignore Model::ResourceVar;
%unignore Model::RestoreAssignment;
%unignore Model::SetAllowedVehiclesForIndex;
%unignore Model::SetAllowedVehiclesForIndex(const std::vector<int>&, int64_t);
%unignore Model::SetAmortizedCostFactorsOfAllVehicles;
%unignore Model::SetAmortizedCostFactorsOfVehicle;
%unignore Model::SetArcCostEvaluatorOfAllVehicles;
%unignore Model::SetArcCostEvaluatorOfVehicle;
%unignore Model::SetFixedCostOfAllVehicles;
%unignore Model::SetFixedCostOfVehicle;
%unignore Model::SetPathEnergyCostOfVehicle;
%unignore Model::SetPathEnergyCostsOfVehicle;
%unignore Model::SetPickupAndDeliveryPolicyOfAllVehicles;
%unignore Model::SetPrimaryConstrainedDimension;
%unignore Model::SetVehicleUsedWhenEmpty;
%unignore Model::SetVisitType;
%unignore Model::Size;
%unignore Model::Solve;
%unignore Model::SolveFromAssignmentWithParameters;
%unignore Model::SolveWithParameters;
%unignore Model::SolveFromAssignmentWithParameters(const operations_research::Assignment*, const RoutingSearchParameters&);
%unignore Model::SolveWithParameters(const RoutingSearchParameters&);
%unignore Model::Start;
%unignore Model::TransitCallback;
%unignore Model::UnaryTransitCallbackOrNull;
%unignore Model::UpdateTimeLimit;
%unignore Model::VehicleVar;
%unignore Model::VehicleRouteConsideredVar;
%unignore Model::WriteAssignment;
%unignore Model::nodes;
%unignore Model::solver;
%unignore Model::status;
%unignore Model::vehicles;
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

%extend operations_research::routing::Model {
  void ApplyLocks(const std::vector<int64_t>& locks) {
    $self->ApplyLocks(absl::Span<const int64_t>(locks));
  }

  void SetAllowedVehiclesForIndex(const std::vector<int>& vehicles,
                                  int64_t index) {
    $self->SetAllowedVehiclesForIndex(absl::Span<const int>(vehicles), index);
  }

  void AddRouteConstraint(
      operations_research::routing::RouteConstraintCallback* route_evaluator,
      bool costs_are_homogeneous_across_vehicles = false) {
    $self->AddRouteConstraint(
        [route_evaluator](const std::vector<int64_t>& route)
            -> std::optional<int64_t> {
          const operations_research::routing::RouteConstraintResult result =
              route_evaluator->Evaluate(route);
          if (!result.is_satisfied) return std::nullopt;
          return result.cost;
        },
        costs_are_homogeneous_across_vehicles);
  }
}

%unignoreall
