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

%module Globals

%include "std_pair.i"
%template(IntBoolPair) std::pair<int, bool>;

%include "ortools/base/base.i"
%include "ortools/util/java/vector.swig"
%include "ortools/util/java/proto.i"
%import "ortools/constraint_solver/java/constraint_solver.swig"
%import "ortools/util/java/sorted_interval_list.swig" // Domain

%include "ortools/routing/java/index_manager.swig"

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
#include "ortools/util/optional_boolean.pb.h"
%}

// Add needed import to GlobalsJNI.java
%pragma(java) jniclassimports=%{
// Types from ConstraintSolver
import com.google.ortools.constraintsolver.Assignment;
import com.google.ortools.constraintsolver.IntVar;
import com.google.ortools.constraintsolver.IntervalVar;
import com.google.ortools.constraintsolver.LocalSearchFilter;
import com.google.ortools.constraintsolver.LocalSearchOperator;
import com.google.ortools.constraintsolver.SearchMonitor;

// Used to wrap Domain
import com.google.ortools.util.Domain;

// Used to wrap std::function<std::string()>
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/Supplier.html
import java.util.function.Supplier;

// Used to wrap std::function<bool()>
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/BooleanSupplier.html
import java.util.function.BooleanSupplier;

// Used to wrap std::function<int(int64_t)>
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/LongToIntFunction.html
import java.util.function.LongToIntFunction;

// Used to wrap std::function<int64_t(int64_t)>
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/LongUnaryOperator.html
import java.util.function.LongUnaryOperator;

// Used to wrap std::function<int64_t(int64_t, int64_t)>
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/LongBinaryOperator.html
import java.util.function.LongBinaryOperator;

// Used to wrap std::function<int64_t(int64_t, int64_t, int64_t)>
// note: Java does not provide TernaryOperator so we provide it
import com.google.ortools.constraintsolver.LongTernaryOperator;

// Used to wrap std::function<int64_t(int, int)>
// note: Java does not provide it, so we provide it.
import com.google.ortools.constraintsolver.IntIntToLongFunction;

// Used to wrap std::function<bool(int64_t)>
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/LongPredicate.html
import java.util.function.LongPredicate;

// Used to wrap std::function<bool(int64_t, int64_t, int64_t)>
// note: Java does not provide TernaryPredicate so we provide it
import com.google.ortools.constraintsolver.LongTernaryPredicate;

// Used to wrap std::function<void(Solver*)>
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/Consumer.html
import java.util.function.Consumer;

// Used to wrap std::function<void(int64_t)>
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/LongConsumer.html
import java.util.function.LongConsumer;

// Used to wrap std::function<void()>
// see https://docs.oracle.com/javase/8/docs/api/java/lang/Runnable.html
import java.lang.Runnable;
%}

// Protobuf support
PROTO_INPUT(operations_research::routing::RoutingModelParameters,
            com.google.ortools.routing.RoutingModelParameters,
            parameters)
PROTO_INPUT(operations_research::routing::RoutingSearchParameters,
            com.google.ortools.routing.RoutingSearchParameters,
            search_parameters)
PROTO2_RETURN(operations_research::routing::IteratedLocalSearchParameters,
              com.google.ortools.routing.IteratedLocalSearchParameters)
PROTO2_RETURN(operations_research::routing::RoutingModelParameters,
              com.google.ortools.routing.RoutingModelParameters)
PROTO2_RETURN(operations_research::routing::RoutingSearchParameters,
              com.google.ortools.routing.RoutingSearchParameters)
PROTO_ENUM_RETURN(operations_research::routing::FirstSolutionStrategy::Value,
                  com.google.ortools.routing.FirstSolutionStrategy.Value)
PROTO_ENUM_RETURN(operations_research::routing::RoutingSearchStatus::Value,
                  com.google.ortools.routing.RoutingSearchStatus.Value)

// Add needed import to Globals.java
%pragma(java) moduleimports=%{
// Types from ConstraintSolver
import com.google.ortools.constraintsolver.Assignment;
import com.google.ortools.constraintsolver.IntVar;
%}

// Wrap parameters.h according to the SWIG style guide.
%ignoreall
%unignore operations_research::routing;
namespace operations_research::routing {
// parameters.h
// IMPORTANT(user): These functions are global, so in Java, they are in the
// Globals.java file. (import com.[...].routing.Globals).
%rename (defaultRoutingSearchParameters) DefaultRoutingSearchParameters;
%rename (defaultRoutingModelParameters) DefaultRoutingModelParameters;
%rename (defaultSecondaryRoutingSearchParameters) DefaultSecondaryRoutingSearchParameters;
%rename (defaultIteratedLocalSearchParameters) DefaultIteratedLocalSearchParameters;
%rename (findErrorInRoutingSearchParameters) FindErrorInRoutingSearchParameters;
%rename (findErrorsInRoutingSearchParameters) FindErrorsInRoutingSearchParameters;
%rename (makeSetValuesFromTargets) MakeSetValuesFromTargets;
}  // namespace operations_research::routing

%include "ortools/routing/parameters.h"
%unignoreall

// Routing
// Wrap routing.h according to the SWIG style guide.
%ignoreall
%unignore operations_research::routing;
namespace operations_research::routing {

// GlobalVehicleBreaksConstraint
%typemap(javaimports) GlobalVehicleBreaksConstraint %{
// Types from ConstraintSolver
import com.google.ortools.constraintsolver.Constraint;
%}

// routing::Model
%unignore Model;
%unignore Model::Model;
// Map transit callback to Java @FunctionalInterface types.
// This replaces the RoutingTransitCallback[1-2] in the Java proxy class
%typemap(javaimports) Model %{
// Types from ConstraintSolver
import com.google.ortools.constraintsolver.Assignment;
import com.google.ortools.constraintsolver.DecisionBuilder;
import com.google.ortools.constraintsolver.IntVar;
import com.google.ortools.constraintsolver.IntervalVar;
import com.google.ortools.constraintsolver.LocalSearchFilter;
import com.google.ortools.constraintsolver.LocalSearchOperator;
import com.google.ortools.constraintsolver.SearchMonitor;
import com.google.ortools.constraintsolver.Solver;
// Used to wrap Domain
import com.google.ortools.util.Domain;
// Used to wrap RoutingTransitCallback2
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/LongBinaryOperator.html
import java.util.function.LongBinaryOperator;
// Used to wrap RoutingTransitCallback1
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/LongUnaryOperator.html
import java.util.function.LongUnaryOperator;
%}

%ignore Model::AddDimensionDependentDimensionWithVehicleCapacity;
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
%ignore Model::GetSameVehicleRequiredTypeAlternativesOfType;
%ignore Model::HasTypeRegulations;
%ignore Model::MakeStateDependentTransit;
%ignore Model::PackCumulsOfOptimizerDimensionsFromAssignment;
%ignore Model::RegisterStateDependentTransitCallback;
%ignore Model::RemainingTime;
%ignore Model::SolveWithParameters(
    const RoutingSearchParameters& search_parameters,
    std::vector<const Assignment*>* solutions);
%ignore Model::SolveFromAssignmentWithParameters(
      const Assignment* assignment,
      const RoutingSearchParameters& search_parameters,
      std::vector<const Assignment*>* solutions);
%ignore Model::TransitCallback;
%ignore Model::StateDependentTransitCallback;
%ignore Model::UnaryTransitCallbackOrNull;
%rename (activeVar) Model::ActiveVar;
%rename (activeVehicleVar) Model::ActiveVehicleVar;
%rename (addAllActive) Model::AddAllActive;
%rename (addAtSolutionCallback) Model::AddAtSolutionCallback;

%rename (addConstantDimension) Model::AddConstantDimension;
%rename (addConstantDimensionWithSlack) Model::AddConstantDimensionWithSlack;
%rename (addVectorDimension) Model::AddVectorDimension;
%rename (addMatrixDimension) Model::AddMatrixDimension;
%rename (addDimension) Model::AddDimension;
%rename (addDimensionWithVehicleCapacity) Model::AddDimensionWithVehicleCapacity;
%rename (addDimensionWithVehicleTransitAndCapacity) Model::AddDimensionWithVehicleTransitAndCapacity;
%rename (addDimensionWithVehicleTransits) Model::AddDimensionWithVehicleTransits;

%rename (addDisjunction) Model::AddDisjunction;
%rename (addHardTypeIncompatibility) Model::AddHardTypeIncompatibility;
%rename (addIntervalToAssignment) Model::AddIntervalToAssignment;
%rename (addLocalSearchFilter) Model::AddLocalSearchFilter;
%rename (addLocalSearchOperator) Model::AddLocalSearchOperator;
%rename (addPickupAndDelivery) Model::AddPickupAndDelivery;
%rename (addPickupAndDeliverySets) Model::AddPickupAndDeliverySets;
%rename (addRequiredTypeAlternativesWhenAddingType) Model::AddRequiredTypeAlternativesWhenAddingType;
%rename (addRequiredTypeAlternativesWhenRemovingType) Model::AddRequiredTypeAlternativesWhenRemovingType;
%rename (addSearchMonitor) Model::AddSearchMonitor;
%rename (addSoftSameVehicleConstraint) Model::AddSoftSameVehicleConstraint;
%rename (addTemporalTypeIncompatibility) Model::AddTemporalTypeIncompatibility;
%rename (addToAssignment) Model::AddToAssignment;
%rename (addVariableMaximizedByFinalizer) Model::AddVariableMaximizedByFinalizer;
%rename (addVariableMinimizedByFinalizer) Model::AddVariableMinimizedByFinalizer;
%rename (addVariableTargetToFinalizer) Model::AddVariableTargetToFinalizer;
%rename (applyLocks) Model::ApplyLocks;
%rename (applyLocksToAllVehicles) Model::ApplyLocksToAllVehicles;
%rename (arcIsMoreConstrainedThanArc) Model::ArcIsMoreConstrainedThanArc;
%rename (assignmentToRoutes) Model::AssignmentToRoutes;
%rename (checkLimit) Model::CheckLimit;
%rename (closeModel) Model::CloseModel;
%rename (closeModelWithParameters) Model::CloseModelWithParameters;
%rename (compactAndCheckAssignment) Model::CompactAndCheckAssignment;
%rename (compactAssignment) Model::CompactAssignment;
%rename (computeLowerBound) Model::ComputeLowerBound;
%rename (costVar) Model::CostVar;
%rename (costsAreHomogeneousAcrossVehicles) Model::CostsAreHomogeneousAcrossVehicles;
%rename (debugOutputAssignment) Model::DebugOutputAssignment;
%rename (end) Model::End;
%rename (getAmortizedLinearCostFactorOfVehicles) Model::GetAmortizedLinearCostFactorOfVehicles;
%rename (getAmortizedQuadraticCostFactorOfVehicles) Model::GetAmortizedQuadraticCostFactorOfVehicles;
%rename (getArcCostForClass) Model::GetArcCostForClass;
%rename (getArcCostForFirstSolution) Model::GetArcCostForFirstSolution;
%rename (getArcCostForVehicle) Model::GetArcCostForVehicle;
%rename (getCostClassIndexOfVehicle) Model::GetCostClassIndexOfVehicle;
%rename (getCostClassesCount) Model::GetCostClassesCount;
%rename (getDepot) Model::GetDepot;
%rename (getDimensionOrDie) Model::GetDimensionOrDie;
%rename (getDisjunctionIndices) Model::GetDisjunctionIndices;
%rename (getDisjunctionMaxCardinality) Model::GetDisjunctionMaxCardinality;
%rename (getDisjunctionPenalty) Model::GetDisjunctionPenalty;
%rename (getFixedCostOfVehicle) Model::GetFixedCostOfVehicle;
%rename (getHomogeneousCost) Model::GetHomogeneousCost;
%rename (getMutableDimension) Model::GetMutableDimension;
%rename (getNonZeroCostClassesCount) Model::GetNonZeroCostClassesCount;
%rename (getNumOfSingletonNodes) Model::GetNumOfSingletonNodes;
%rename (getNumberOfDecisionsInFirstSolution) Model::GetNumberOfDecisionsInFirstSolution;
%rename (getNumberOfDisjunctions) Model::GetNumberOfDisjunctions;
%rename (getNumberOfRejectsInFirstSolution) Model::GetNumberOfRejectsInFirstSolution;
%rename (getNumberOfVisitTypes) Model::GetNumberOfVisitTypes;
%rename (getPickupAndDeliveryPolicyOfVehicle) Model::GetPickupAndDeliveryPolicyOfVehicle;
%rename (getPrimaryConstrainedDimension) Model::GetPrimaryConstrainedDimension;
%rename (getSameVehicleIndicesOfIndex) Model::GetSameVehicleIndicesOfIndex;
%rename (getTemporalTypeIncompatibilitiesOfType) Model::GetTemporalTypeIncompatibilitiesOfType;
%rename (getVehicleClassIndexOfVehicle) Model::GetVehicleClassIndexOfVehicle;
%rename (getVehicleClassesCount) Model::GetVehicleClassesCount;
%rename (getVisitType) Model::GetVisitType;
%rename (hasDimension) Model::HasDimension;
%rename (hasHardTypeIncompatibilities) Model::HasHardTypeIncompatibilities;
%rename (hasSameVehicleTypeRequirements) Model::HasSameVehicleTypeRequirements;
%rename (hasTemporalTypeIncompatibilities) Model::HasTemporalTypeIncompatibilities;
%rename (hasTemporalTypeRequirements) Model::HasTemporalTypeRequirements;
%rename (hasVehicleWithCostClassIndex) Model::HasVehicleWithCostClassIndex;
%rename (ignoreDisjunctionsAlreadyForcedToZero) Model::IgnoreDisjunctionsAlreadyForcedToZero;
%rename (isEnd) Model::IsEnd;
%rename (isMatchingModel) Model::IsMatchingModel;
%rename (isStart) Model::IsStart;
%rename (isVehicleAllowedForIndex) Model::IsVehicleAllowedForIndex;
%rename (isVehicleUsed) Model::IsVehicleUsed;
%rename (makeGuidedSlackFinalizer) Model::MakeGuidedSlackFinalizer;
%rename (makeSelfDependentDimensionFinalizer) Model::MakeSelfDependentDimensionFinalizer;
%rename (mutablePreAssignment) Model::MutablePreAssignment;
%rename (next) Model::Next;
%rename (nextVar) Model::NextVar;
%rename (nexts) Model::Nexts;
%rename (preAssignment) Model::PreAssignment;
%rename (readAssignment) Model::ReadAssignment;
%rename (readAssignmentFromRoutes) Model::ReadAssignmentFromRoutes;

%rename (registerUnaryTransitVector) Model::RegisterUnaryTransitVector;
%rename (registerUnaryTransitCallback) Model::RegisterUnaryTransitCallback;

%rename (registerTransitMatrix) Model::RegisterTransitMatrix;
%rename (registerTransitCallback) Model::RegisterTransitCallback;

%rename (restoreAssignment) Model::RestoreAssignment;
%rename (routesToAssignment) Model::RoutesToAssignment;
%rename (setAllowedVehiclesForIndex) Model::SetAllowedVehiclesForIndex;
%rename (setAmortizedCostFactorsOfAllVehicles) Model::SetAmortizedCostFactorsOfAllVehicles;
%rename (setAmortizedCostFactorsOfVehicle) Model::SetAmortizedCostFactorsOfVehicle;
%rename (setArcCostEvaluatorOfAllVehicles) Model::SetArcCostEvaluatorOfAllVehicles;
%rename (setArcCostEvaluatorOfVehicle) Model::SetArcCostEvaluatorOfVehicle;
%rename (setAssignmentFromOtherModelAssignment) Model::SetAssignmentFromOtherModelAssignment;
%rename (setFirstSolutionEvaluator) Model::SetFirstSolutionEvaluator;
%rename (setFixedCostOfAllVehicles) Model::SetFixedCostOfAllVehicles;
%rename (setFixedCostOfVehicle) Model::SetFixedCostOfVehicle;
%rename (setPickupAndDeliveryPolicyOfAllVehicles) Model::SetPickupAndDeliveryPolicyOfAllVehicles;
%rename (setPickupAndDeliveryPolicyOfVehicle) Model::SetPickupAndDeliveryPolicyOfVehicle;
%rename (setPrimaryConstrainedDimension) Model::SetPrimaryConstrainedDimension;
%rename (setVisitType) Model::SetVisitType;
%rename (size) Model::Size;
%rename (solve) Model::Solve;
%rename (solveFromAssignmentWithParameters) Model::SolveFromAssignmentWithParameters;
%rename (solveWithParameters) Model::SolveWithParameters;
%rename (start) Model::Start;
%rename (status) Model::status;
%rename (unperformedPenalty) Model::UnperformedPenalty;
%rename (unperformedPenaltyOrValue) Model::UnperformedPenaltyOrValue;
%rename (vehicleVar) Model::VehicleVar;
%rename (vehicleVars) Model::VehicleVars;
%rename (writeAssignment) Model::WriteAssignment;
%unignore Model::nodes;
%unignore Model::solver;
%unignore Model::vehicles;
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
%typemap(javaimports) ModelVisitor %{
// Types from ConstraintSolver
import com.google.ortools.constraintsolver.BaseObject;
%}

// RoutingDimension
%unignore Dimension;
// Map transit callback to Java @FunctionalInterface types.
// This replaces the RoutingTransitCallback[1-2] in the Java proxy class
%typemap(javaimports) Dimension %{
// Types from ConstraintSolver
import com.google.ortools.constraintsolver.IntVar;
import com.google.ortools.constraintsolver.IntervalVar;
import com.google.ortools.constraintsolver.LocalSearchOperator;
// Used to wrap std::function<int64_t(int, int)>
// note: Java does not provide it, so we provide it.
import com.google.ortools.constraintsolver.IntIntToLongFunction;
// Used to wrap std::function<int64_t(int64_t from_index, int64_t to_index)> group_delay
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/LongBinaryOperator.html
import java.util.function.LongBinaryOperator;
%}
%ignore Dimension::GetBreakDistanceDurationOfVehicle;
%rename (addNodePrecedence) Dimension::AddNodePrecedence;
%rename (cumulVar) Dimension::CumulVar;
%rename (fixedTransitVar) Dimension::FixedTransitVar;
%rename (getBreakIntervalsOfVehicle) Dimension::GetBreakIntervalsOfVehicle;
%rename (getCumulVarSoftLowerBound) Dimension::GetCumulVarSoftLowerBound;
%rename (getCumulVarSoftLowerBoundCoefficient) Dimension::GetCumulVarSoftLowerBoundCoefficient;
%rename (getCumulVarSoftUpperBound) Dimension::GetCumulVarSoftUpperBound;
%rename (getCumulVarSoftUpperBoundCoefficient) Dimension::GetCumulVarSoftUpperBoundCoefficient;
%rename (getGlobalSpanCostCoefficient) Dimension::global_span_cost_coefficient;
%rename (getLocalOptimizerOffsetForVehicle) Dimension::GetLocalOptimizerOffsetForVehicle;
%rename (setSoftSpanUpperBoundForVehicle) Dimension::SetSoftSpanUpperBoundForVehicle;
%rename (hasSoftSpanUpperBounds) Dimension::HasSoftSpanUpperBounds;
%rename (getSoftSpanUpperBoundForVehicle) Dimension::GetSoftSpanUpperBoundForVehicle;
%rename (setQuadraticCostSoftSpanUpperBoundForVehicle) Dimension::SetQuadraticCostSoftSpanUpperBoundForVehicle;
%rename (hasQuadraticCostSoftSpanUpperBounds) Dimension::HasQuadraticCostSoftSpanUpperBounds;
%rename (getQuadraticCostSoftSpanUpperBoundForVehicle) Dimension::GetQuadraticCostSoftSpanUpperBoundForVehicle;
%rename (getGroupDelay) Dimension::GetGroupDelay;
%rename (getNodeVisitTransitsOfVehicle) Dimension::GetNodeVisitTransitsOfVehicle;
%rename (getSpanCostCoefficientForVehicle) Dimension::GetSpanCostCoefficientForVehicle;
%rename (getSpanUpperBoundForVehicle) Dimension::GetSpanUpperBoundForVehicle;
%rename (getTransitValue) Dimension::GetTransitValue;
%rename (getTransitValueFromClass) Dimension::GetTransitValueFromClass;
%rename (hasCumulVarSoftLowerBound) Dimension::HasCumulVarSoftLowerBound;
%rename (hasCumulVarSoftUpperBound) Dimension::HasCumulVarSoftUpperBound;
%rename (hasPickupToDeliveryLimits) Dimension::HasPickupToDeliveryLimits;
%rename (setBreakDistanceDurationOfVehicle) Dimension::SetBreakDistanceDurationOfVehicle;
%rename (setBreakIntervalsOfVehicle) Dimension::SetBreakIntervalsOfVehicle;
%rename (setCumulVarSoftLowerBound) Dimension::SetCumulVarSoftLowerBound;
%rename (setCumulVarSoftUpperBound) Dimension::SetCumulVarSoftUpperBound;
%rename (setGlobalSpanCostCoefficient) Dimension::SetGlobalSpanCostCoefficient;
%rename (setPickupToDeliveryLimitFunctionForPair) Dimension::SetPickupToDeliveryLimitFunctionForPair;
%rename (setSpanCostCoefficientForAllVehicles) Dimension::SetSpanCostCoefficientForAllVehicles;
%rename (setSpanCostCoefficientForVehicle) Dimension::SetSpanCostCoefficientForVehicle;
%rename (setSpanUpperBoundForVehicle) Dimension::SetSpanUpperBoundForVehicle;
%rename (shortestTransitionSlack) Dimension::ShortestTransitionSlack;
%rename (slackVar) Dimension::SlackVar;
%rename (transitVar) Dimension::TransitVar;
%rename (vehicleHasBreakIntervals) Dimension::VehicleHasBreakIntervals;
%unignore Dimension::cumuls;
%unignore Dimension::slacks;
%unignore Dimension::transits;

// RoutingFilteredDecisionBuilder
%unignore RoutingFilteredDecisionBuilder;
%rename (getEndChainStart) RoutingFilteredDecisionBuilder::GetEndChainStart;
%rename (getStartChainEnd) RoutingFilteredDecisionBuilder::GetStartChainEnd;
%rename (initializeRoutes) RoutingFilteredDecisionBuilder::InitializeRoutes;
%rename (makeDisjunctionNodesUnperformed) RoutingFilteredDecisionBuilder::MakeDisjunctionNodesUnperformed;
%rename (makeUnassignedNodesUnperformed) RoutingFilteredDecisionBuilder::MakeUnassignedNodesUnperformed;

// TypeRegulationsChecker
%unignore TypeRegulationsChecker;
%ignore TypeRegulationsChecker::CheckVehicle;

%typemap(javaimports) TypeRegulationsConstraint %{
// Types from ConstraintSolver
import com.google.ortools.constraintsolver.Constraint;
%}

// SimpleBoundCosts
%unignore BoundCost;
%unignore BoundCost::BoundCost;
%unignore BoundCost::bound;
%unignore BoundCost::cost;

%unignore SimpleBoundCosts;
%rename (getBoundCost) SimpleBoundCosts::bound_cost;
%rename (getSize) SimpleBoundCosts::Size;

}  // namespace operations_research::routing

// Generic rename rules.
%rename (buildSolution) *::BuildSolution;

%include "ortools/routing/routing.h"
%unignoreall
