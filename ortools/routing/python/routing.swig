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
//swiglint: disable full-signature

%include "ortools/base/base.i"
%include "ortools/util/python/proto.i"

// PY_CONVERT_HELPER_* macros.
%include "ortools/constraint_solver/python/constraint_solver_helpers.swig"

%include "ortools/util/python/functions.swig"
%include "ortools/util/python/pair.swig"
%include "ortools/util/python/vector.swig"

// Ignore warnings for operators like: Unreferenced %newobject *::operator -=
#pragma SWIG nowarn=551

// While the module name will be overridden by the one specified on the cmd line,
// without this, derived classes (e.g. TypeRequirementChecker) will import base
// class from the module specified in the following %import.
%module pywraprouting
%import(module="ortools.constraint_solver.pywrapcp") "ortools/constraint_solver/python/constraint_solver.swig"
%include "ortools/routing/python/index_manager.swig"

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

// Include the files we want to wrap a first time.
%{
#include "ortools/routing/enums.pb.h"
#include "ortools/routing/ils.pb.h"
#include "ortools/routing/types.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/routing.h"
#include "ortools/constraint_solver/search.h"
#include "ortools/util/optional_boolean.pb.h"
%}


// ============= Type conversions ==============

// See ./constraint_solver_helpers.swig.
PY_CONVERT_HELPER_INTEXPR_AND_INTVAR();
PY_CONVERT_HELPER_PTR(IntervalVar);
PY_CONVERT_HELPER_PTR(LocalSearchFilter);
PY_CONVERT_HELPER_PTR(LocalSearchOperator);
PY_CONVERT_HELPER_PTR(SearchMonitor);

PY_PROTO_ENUM_TYPEMAP(
    operations_research::routing::FirstSolutionStrategy::Value,
    ortools.routing.enums_pb2.FirstSolutionStrategy.Value)
PY_PROTO_ENUM_TYPEMAP(
    operations_research::routing::RoutingSearchStatus::Value,
    ortools.routing.enums_pb2.RoutingSearchStatus.Value)

PY_PROTO_TYPEMAP(ortools.routing.parameters_pb2,
                 IteratedLocalSearchParameters,
                 operations_research::routing::IteratedLocalSearchParameters)
PY_PROTO_TYPEMAP(ortools.routing.parameters_pb2,
                 RoutingModelParameters,
                 operations_research::routing::RoutingModelParameters)
PY_PROTO_TYPEMAP(ortools.routing.parameters_pb2,
                 RoutingSearchParameters,
                 operations_research::routing::RoutingSearchParameters)

// Parameters
// Wrap parameters.h according to the SWIG style guide.
%ignoreall
%unignore operations_research::routing;
namespace operations_research::routing {
// parameters.h
// IMPORTANT: These functions are global, so in .Net, they are in the RoutingGlobals.cs
%unignore DefaultRoutingSearchParameters;
%unignore DefaultRoutingModelParameters;
%unignore DefaultSecondaryRoutingSearchParameters;
%unignore DefaultIteratedLocalSearchParameters;
%unignore FindErrorInRoutingSearchParameters;
%unignore FindErrorsInRoutingSearchParameters;
}  // namespace operations_research::routing

%include "ortools/routing/parameters.h"
%unignoreall

// Routing
// Wrap routing.h according to the SWIG style guide.
%ignoreall

%unignore operations_research::routing;
namespace operations_research::routing {

// routing::Model
%unignore Model;
%unignore Model::Model;
%unignore Model::~Model;
%unignore Model::AddAtSolutionCallback;
%unignore Model::AddConstantDimension;
%unignore Model::AddDimension;
%unignore Model::AddDimensionWithVehicleCapacity;
%unignore Model::AddDimensionWithVehicleTransits;
%unignore Model::AddDisjunction;
%unignore Model::AddMatrixDimension;
%unignore Model::AddPickupAndDelivery;
%unignore Model::AddVectorDimension;
%unignore Model::ApplyLocks(const std::vector<int64_t>&);
%extend Model {
  void ApplyLocks(
    const std::vector<int64_t>& locks) {
      $self->ApplyLocks(absl::Span<const int64_t>(locks));
  }
}
%unignore Model::CloseModelWithParameters;
%unignore Model::CostVar;
%unignore Model::End;
%unignore Model::GetAmortizedLinearCostFactorOfVehicles;
%unignore Model::GetAmortizedQuadraticCostFactorOfVehicles;
%unignore Model::GetAutomaticFirstSolutionStrategy;
%unignore Model::GetDimensionOrDie;
%unignore Model::GetFixedCostOfVehicle;
%unignore Model::GetNumberOfDecisionsInFirstSolution;
%unignore Model::GetNumberOfRejectsInFirstSolution;
%unignore Model::GetVehicleClassCount;
%unignore Model::GetVehicleClassesCount;
%unignore Model::GetVisitType;
%unignore Model::IsDelivery;
%unignore Model::IsEnd;
%unignore Model::IsPickup;
%unignore Model::IsVehicleAllowedForIndex;
%unignore Model::NextVar;
%unignore Model::ReadAssignmentFromRoutes;
%unignore Model::RegisterTransitCallback;
%unignore Model::RegisterTransitMatrix;
%unignore Model::RegisterUnaryTransitCallback;
%unignore Model::RegisterUnaryTransitVector;
%unignore Model::SetAllowedVehiclesForIndex(const std::vector<int>&, int64_t);
%extend Model {
  void SetAllowedVehiclesForIndex(
    const std::vector<int>& vehicles,
    int64_t index) {
      $self->SetAllowedVehiclesForIndex(absl::Span<const int>(vehicles), index);
  }
}
%unignore Model::SetAmortizedCostFactorsOfAllVehicles;
%unignore Model::SetAmortizedCostFactorsOfVehicle;
%unignore Model::SetArcCostEvaluatorOfAllVehicles;
%unignore Model::SetFixedCostOfAllVehicles;
%unignore Model::SetFixedCostOfVehicle;
%unignore Model::SetVisitType;
%unignore Model::Solve;
%unignore Model::SolveFromAssignmentWithParameters;
%unignore Model::SolveWithParameters;
%unignore Model::Start;
%unignore Model::VehicleVar;
%unignore Model::solver;
%unignore Model::status;
%unignore Model::vehicles;
// Used in samples/ but not in pywraprouting_test.py
%unignore Model::AddConstantDimensionWithSlack;
%unignore Model::AddToAssignment;
%unignore Model::AddVariableMinimizedByFinalizer;
%unignore Model::GetArcCostForVehicle;
%unignore Model::IsStart;
%unignore Model::IsVehicleUsed;
%unignore Model::SetPickupAndDeliveryPolicyOfAllVehicles;
%unignore Model::Size;
// untested but currently exported
// TODO(user) triage to ignore or add unit test
%unignore Model::ActiveVar;
%unignore Model::ActiveVehicleVar;
%unignore Model::AddDimensionWithCumulDependentVehicleTransitAndCapacity;
%unignore Model::AddDimensionWithVehicleTransitAndCapacity;
%unignore Model::AddEnterSearchCallback;
%unignore Model::AddHardTypeIncompatibility;
%unignore Model::AddRequiredTypeAlternativesWhenAddingType;
%unignore Model::AddSearchMonitor;
%unignore Model::AddTemporalTypeIncompatibility;
%unignore Model::ApplyLocksToAllVehicles;
%unignore Model::CancelSearch;
%unignore Model::CompactAssignment;
%unignore Model::CostsAreHomogeneousAcrossVehicles;
%unignore Model::CumulDependentTransitCallback;
%unignore Model::FastSolveFromAssignmentWithParameters;
%unignore Model::GetAllDimensionNames;
%unignore Model::GetDimensions;
%unignore Model::GetDisjunctionIndices;
%unignore Model::GetDisjunctionPenalty;
%unignore Model::ReadAssignment;
%unignore Model::RegisterCumulDependentTransitCallback;
%unignore Model::ResourceVar;
%unignore Model::RestoreAssignment;
%unignore Model::SetArcCostEvaluatorOfVehicle;
%unignore Model::SetPathEnergyCostOfVehicle;
%unignore Model::TransitCallback;
%unignore Model::UnaryTransitCallbackOrNull;
%unignore Model::UpdateTimeLimit;
%unignore Model::VehicleRouteConsideredVar;
%unignore Model::WriteAssignment;
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

// Dimension
%unignore Dimension;
%unignore Dimension::Dimension;
%unignore Dimension::CumulVar;
%unignore Dimension::GetQuadraticCostSoftSpanUpperBoundForVehicle;
%unignore Dimension::GetSoftSpanUpperBoundForVehicle;
%unignore Dimension::GetSpanCostCoefficientForVehicle;
%unignore Dimension::GetTransitValue;
%unignore Dimension::HasQuadraticCostSoftSpanUpperBounds;
%unignore Dimension::HasSoftSpanUpperBounds;
%unignore Dimension::SetBreakIntervalsOfVehicle;
%unignore Dimension::SetCumulVarSoftLowerBound;
%unignore Dimension::SetCumulVarSoftUpperBound;
%unignore Dimension::SetGlobalSpanCostCoefficient;
%unignore Dimension::SetQuadraticCostSoftSpanUpperBoundForVehicle;
%unignore Dimension::SetSlackCostCoefficientForVehicle;
%unignore Dimension::SetSoftSpanUpperBoundForVehicle;
%unignore Dimension::SetSpanCostCoefficientForVehicle;
%unignore Dimension::SlackVar;
%unignore Dimension::TransitVar;
// In Samples not in pywraprouting_test.py
// untested but currently exported
// TODO(user) triage to ignore or add unit test
%unignore Dimension::FixedTransitVar;
%unignore Dimension::GetCumulVarMax;
%unignore Dimension::GetCumulVarMin;
%unignore Dimension::GetTransitValueFromClass;
%unignore Dimension::SetCumulVarRange;
%unignore Dimension::SetSpanUpperBoundForVehicle;
%unignore Dimension::model;

// SimpleBoundCosts
%unignore BoundCost;
%unignore BoundCost::BoundCost;
%unignore BoundCost::bound;
%unignore BoundCost::cost;

%unignore SimpleBoundCosts;
%unignore SimpleBoundCosts::bound_cost;
%rename("size") SimpleBoundCosts::Size;

}  // namespace operations_research::routing

%include "ortools/routing/routing.h"
%unignoreall
