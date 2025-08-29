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
class RoutingModelParameters;
class RoutingSearchParameters;

class RoutingSearchStatus;

}  // namespace operations_research::routing

// Include the files we want to wrap a first time.
%{
#include "ortools/routing/enums.pb.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/routing.h"
#include "ortools/routing/types.h"
%}

// RoutingModel methods.
DEFINE_INDEX_TYPE_TYPEDEF(
    operations_research::routing::RoutingCostClassIndex,
    operations_research::routing::RoutingModel::CostClassIndex);
DEFINE_INDEX_TYPE_TYPEDEF(
    operations_research::routing::RoutingDimensionIndex,
    operations_research::routing::RoutingModel::DimensionIndex);
DEFINE_INDEX_TYPE_TYPEDEF(
    operations_research::routing::RoutingDisjunctionIndex,
    operations_research::routing::RoutingModel::DisjunctionIndex);
DEFINE_INDEX_TYPE_TYPEDEF(
    operations_research::routing::RoutingVehicleClassIndex,
    operations_research::routing::RoutingModel::VehicleClassIndex);
DEFINE_INDEX_TYPE_TYPEDEF(
    operations_research::routing::RoutingResourceClassIndex,
    operations_research::routing::RoutingModel::ResourceClassIndex);

namespace operations_research::routing {

// GlobalVehicleBreaksConstraint
%typemap(javaimports) GlobalVehicleBreaksConstraint %{
// Types from ConstraintSolver
import com.google.ortools.constraintsolver.Constraint;
%}

// RoutingModel
%unignore RoutingModel;
// Map transit callback to Java @FunctionalInterface types.
// This replaces the RoutingTransitCallback[1-2] in the Java proxy class
%typemap(javaimports) RoutingModel %{
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

%ignore RoutingModel::AddDimensionDependentDimensionWithVehicleCapacity;
%ignore RoutingModel::AddSameVehicleRequiredTypeAlternatives;
%ignore RoutingModel::GetAllDimensionNames;
%ignore RoutingModel::GetAutomaticFirstSolutionStrategy;
%ignore RoutingModel::GetDeliveryIndexPairs;
%ignore RoutingModel::GetDimensions;
%ignore RoutingModel::GetDimensionsWithSoftAndSpanCosts;
%ignore RoutingModel::GetDimensionsWithSoftOrSpanCosts;
%ignore RoutingModel::GetGlobalDimensionCumulOptimizers;
%ignore RoutingModel::GetHardTypeIncompatibilitiesOfType;
%ignore RoutingModel::GetLocalDimensionCumulMPOptimizers;
%ignore RoutingModel::GetLocalDimensionCumulOptimizers;
%ignore RoutingModel::GetMutableGlobalCumulOptimizer;
%ignore RoutingModel::GetMutableLocalCumulOptimizer;
%ignore RoutingModel::GetMutableLocalCumulMPOptimizer;
%ignore RoutingModel::GetPerfectBinaryDisjunctions;
%ignore RoutingModel::GetPickupIndexPairs;
%ignore RoutingModel::GetSameVehicleRequiredTypeAlternativesOfType;
%ignore RoutingModel::HasTypeRegulations;
%ignore RoutingModel::MakeStateDependentTransit;
%ignore RoutingModel::PackCumulsOfOptimizerDimensionsFromAssignment;
%ignore RoutingModel::RegisterStateDependentTransitCallback;
%ignore RoutingModel::RemainingTime;
%ignore RoutingModel::SolveWithParameters(
    const RoutingSearchParameters& search_parameters,
    std::vector<const Assignment*>* solutions);
%ignore RoutingModel::SolveFromAssignmentWithParameters(
      const Assignment* assignment,
      const RoutingSearchParameters& search_parameters,
      std::vector<const Assignment*>* solutions);
%ignore RoutingModel::TransitCallback;
%ignore RoutingModel::StateDependentTransitCallback;
%ignore RoutingModel::UnaryTransitCallbackOrNull;
%rename (activeVar) RoutingModel::ActiveVar;
%rename (activeVehicleVar) RoutingModel::ActiveVehicleVar;
%rename (addAllActive) RoutingModel::AddAllActive;
%rename (addAtSolutionCallback) RoutingModel::AddAtSolutionCallback;

%rename (addConstantDimension) RoutingModel::AddConstantDimension;
%rename (addConstantDimensionWithSlack) RoutingModel::AddConstantDimensionWithSlack;
%rename (addVectorDimension) RoutingModel::AddVectorDimension;
%rename (addMatrixDimension) RoutingModel::AddMatrixDimension;
%rename (addDimension) RoutingModel::AddDimension;
%rename (addDimensionWithVehicleCapacity) RoutingModel::AddDimensionWithVehicleCapacity;
%rename (addDimensionWithVehicleTransitAndCapacity) RoutingModel::AddDimensionWithVehicleTransitAndCapacity;
%rename (addDimensionWithVehicleTransits) RoutingModel::AddDimensionWithVehicleTransits;

%rename (addDisjunction) RoutingModel::AddDisjunction;
%rename (addHardTypeIncompatibility) RoutingModel::AddHardTypeIncompatibility;
%rename (addIntervalToAssignment) RoutingModel::AddIntervalToAssignment;
%rename (addLocalSearchFilter) RoutingModel::AddLocalSearchFilter;
%rename (addLocalSearchOperator) RoutingModel::AddLocalSearchOperator;
%rename (addPickupAndDelivery) RoutingModel::AddPickupAndDelivery;
%rename (addPickupAndDeliverySets) RoutingModel::AddPickupAndDeliverySets;
%rename (addRequiredTypeAlternativesWhenAddingType) RoutingModel::AddRequiredTypeAlternativesWhenAddingType;
%rename (addRequiredTypeAlternativesWhenRemovingType) RoutingModel::AddRequiredTypeAlternativesWhenRemovingType;
%rename (addSearchMonitor) RoutingModel::AddSearchMonitor;
%rename (addSoftSameVehicleConstraint) RoutingModel::AddSoftSameVehicleConstraint;
%rename (addTemporalTypeIncompatibility) RoutingModel::AddTemporalTypeIncompatibility;
%rename (addToAssignment) RoutingModel::AddToAssignment;
%rename (addVariableMaximizedByFinalizer) RoutingModel::AddVariableMaximizedByFinalizer;
%rename (addVariableMinimizedByFinalizer) RoutingModel::AddVariableMinimizedByFinalizer;
%rename (addVariableTargetToFinalizer) RoutingModel::AddVariableTargetToFinalizer;
%rename (applyLocks) RoutingModel::ApplyLocks;
%rename (applyLocksToAllVehicles) RoutingModel::ApplyLocksToAllVehicles;
%rename (arcIsMoreConstrainedThanArc) RoutingModel::ArcIsMoreConstrainedThanArc;
%rename (assignmentToRoutes) RoutingModel::AssignmentToRoutes;
%rename (checkLimit) RoutingModel::CheckLimit;
%rename (closeModel) RoutingModel::CloseModel;
%rename (closeModelWithParameters) RoutingModel::CloseModelWithParameters;
%rename (compactAndCheckAssignment) RoutingModel::CompactAndCheckAssignment;
%rename (compactAssignment) RoutingModel::CompactAssignment;
%rename (computeLowerBound) RoutingModel::ComputeLowerBound;
%rename (costVar) RoutingModel::CostVar;
%rename (costsAreHomogeneousAcrossVehicles) RoutingModel::CostsAreHomogeneousAcrossVehicles;
%rename (debugOutputAssignment) RoutingModel::DebugOutputAssignment;
%rename (end) RoutingModel::End;
%rename (getAmortizedLinearCostFactorOfVehicles) RoutingModel::GetAmortizedLinearCostFactorOfVehicles;
%rename (getAmortizedQuadraticCostFactorOfVehicles) RoutingModel::GetAmortizedQuadraticCostFactorOfVehicles;
%rename (getArcCostForClass) RoutingModel::GetArcCostForClass;
%rename (getArcCostForFirstSolution) RoutingModel::GetArcCostForFirstSolution;
%rename (getArcCostForVehicle) RoutingModel::GetArcCostForVehicle;
%rename (getCostClassIndexOfVehicle) RoutingModel::GetCostClassIndexOfVehicle;
%rename (getCostClassesCount) RoutingModel::GetCostClassesCount;
%rename (getDepot) RoutingModel::GetDepot;
%rename (getDimensionOrDie) RoutingModel::GetDimensionOrDie;
%rename (getDisjunctionIndices) RoutingModel::GetDisjunctionIndices;
%rename (getDisjunctionMaxCardinality) RoutingModel::GetDisjunctionMaxCardinality;
%rename (getDisjunctionPenalty) RoutingModel::GetDisjunctionPenalty;
%rename (getFixedCostOfVehicle) RoutingModel::GetFixedCostOfVehicle;
%rename (getHomogeneousCost) RoutingModel::GetHomogeneousCost;
%rename (getMutableDimension) RoutingModel::GetMutableDimension;
%rename (getNonZeroCostClassesCount) RoutingModel::GetNonZeroCostClassesCount;
%rename (getNumOfSingletonNodes) RoutingModel::GetNumOfSingletonNodes;
%rename (getNumberOfDecisionsInFirstSolution) RoutingModel::GetNumberOfDecisionsInFirstSolution;
%rename (getNumberOfDisjunctions) RoutingModel::GetNumberOfDisjunctions;
%rename (getNumberOfRejectsInFirstSolution) RoutingModel::GetNumberOfRejectsInFirstSolution;
%rename (getNumberOfVisitTypes) RoutingModel::GetNumberOfVisitTypes;
%rename (getPickupAndDeliveryPolicyOfVehicle) RoutingModel::GetPickupAndDeliveryPolicyOfVehicle;
%rename (getPrimaryConstrainedDimension) RoutingModel::GetPrimaryConstrainedDimension;
%rename (getSameVehicleIndicesOfIndex) RoutingModel::GetSameVehicleIndicesOfIndex;
%rename (getTemporalTypeIncompatibilitiesOfType) RoutingModel::GetTemporalTypeIncompatibilitiesOfType;
%rename (getVehicleClassIndexOfVehicle) RoutingModel::GetVehicleClassIndexOfVehicle;
%rename (getVehicleClassesCount) RoutingModel::GetVehicleClassesCount;
%rename (getVisitType) RoutingModel::GetVisitType;
%rename (hasDimension) RoutingModel::HasDimension;
%rename (hasHardTypeIncompatibilities) RoutingModel::HasHardTypeIncompatibilities;
%rename (hasSameVehicleTypeRequirements) RoutingModel::HasSameVehicleTypeRequirements;
%rename (hasTemporalTypeIncompatibilities) RoutingModel::HasTemporalTypeIncompatibilities;
%rename (hasTemporalTypeRequirements) RoutingModel::HasTemporalTypeRequirements;
%rename (hasVehicleWithCostClassIndex) RoutingModel::HasVehicleWithCostClassIndex;
%rename (ignoreDisjunctionsAlreadyForcedToZero) RoutingModel::IgnoreDisjunctionsAlreadyForcedToZero;
%rename (isEnd) RoutingModel::IsEnd;
%rename (isMatchingModel) RoutingModel::IsMatchingModel;
%rename (isStart) RoutingModel::IsStart;
%rename (isVehicleAllowedForIndex) RoutingModel::IsVehicleAllowedForIndex;
%rename (isVehicleUsed) RoutingModel::IsVehicleUsed;
%rename (makeGuidedSlackFinalizer) RoutingModel::MakeGuidedSlackFinalizer;
%rename (makeSelfDependentDimensionFinalizer) RoutingModel::MakeSelfDependentDimensionFinalizer;
%rename (mutablePreAssignment) RoutingModel::MutablePreAssignment;
%rename (next) RoutingModel::Next;
%rename (nextVar) RoutingModel::NextVar;
%rename (nexts) RoutingModel::Nexts;
%rename (preAssignment) RoutingModel::PreAssignment;
%rename (readAssignment) RoutingModel::ReadAssignment;
%rename (readAssignmentFromRoutes) RoutingModel::ReadAssignmentFromRoutes;

%rename (registerUnaryTransitVector) RoutingModel::RegisterUnaryTransitVector;
%rename (registerUnaryTransitCallback) RoutingModel::RegisterUnaryTransitCallback;

%rename (registerTransitMatrix) RoutingModel::RegisterTransitMatrix;
%rename (registerTransitCallback) RoutingModel::RegisterTransitCallback;

%rename (restoreAssignment) RoutingModel::RestoreAssignment;
%rename (routesToAssignment) RoutingModel::RoutesToAssignment;
%rename (setAllowedVehiclesForIndex) RoutingModel::SetAllowedVehiclesForIndex;
%rename (setAmortizedCostFactorsOfAllVehicles) RoutingModel::SetAmortizedCostFactorsOfAllVehicles;
%rename (setAmortizedCostFactorsOfVehicle) RoutingModel::SetAmortizedCostFactorsOfVehicle;
%rename (setArcCostEvaluatorOfAllVehicles) RoutingModel::SetArcCostEvaluatorOfAllVehicles;
%rename (setArcCostEvaluatorOfVehicle) RoutingModel::SetArcCostEvaluatorOfVehicle;
%rename (setAssignmentFromOtherModelAssignment) RoutingModel::SetAssignmentFromOtherModelAssignment;
%rename (setFirstSolutionEvaluator) RoutingModel::SetFirstSolutionEvaluator;
%rename (setFixedCostOfAllVehicles) RoutingModel::SetFixedCostOfAllVehicles;
%rename (setFixedCostOfVehicle) RoutingModel::SetFixedCostOfVehicle;
%rename (setPickupAndDeliveryPolicyOfAllVehicles) RoutingModel::SetPickupAndDeliveryPolicyOfAllVehicles;
%rename (setPickupAndDeliveryPolicyOfVehicle) RoutingModel::SetPickupAndDeliveryPolicyOfVehicle;
%rename (setPrimaryConstrainedDimension) RoutingModel::SetPrimaryConstrainedDimension;
%rename (setVisitType) RoutingModel::SetVisitType;
%rename (size) RoutingModel::Size;
%rename (solve) RoutingModel::Solve;
%rename (solveFromAssignmentWithParameters) RoutingModel::SolveFromAssignmentWithParameters;
%rename (solveWithParameters) RoutingModel::SolveWithParameters;
%rename (start) RoutingModel::Start;
%rename (status) RoutingModel::status;
%rename (unperformedPenalty) RoutingModel::UnperformedPenalty;
%rename (unperformedPenaltyOrValue) RoutingModel::UnperformedPenaltyOrValue;
%rename (vehicleVar) RoutingModel::VehicleVar;
%rename (vehicleVars) RoutingModel::VehicleVars;
%rename (writeAssignment) RoutingModel::WriteAssignment;

// RoutingModelVisitor
%typemap(javaimports) RoutingModelVisitor %{
// Types from ConstraintSolver
import com.google.ortools.constraintsolver.BaseObject;
%}

// RoutingDimension
%unignore RoutingDimension;
// Map transit callback to Java @FunctionalInterface types.
// This replaces the RoutingTransitCallback[1-2] in the Java proxy class
%typemap(javaimports) RoutingDimension %{
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
%ignore RoutingDimension::GetBreakDistanceDurationOfVehicle;
%rename (addNodePrecedence) RoutingDimension::AddNodePrecedence;
%rename (cumulVar) RoutingDimension::CumulVar;
%rename (fixedTransitVar) RoutingDimension::FixedTransitVar;
%rename (getBreakIntervalsOfVehicle) RoutingDimension::GetBreakIntervalsOfVehicle;
%rename (getCumulVarSoftLowerBound) RoutingDimension::GetCumulVarSoftLowerBound;
%rename (getCumulVarSoftLowerBoundCoefficient) RoutingDimension::GetCumulVarSoftLowerBoundCoefficient;
%rename (getCumulVarSoftUpperBound) RoutingDimension::GetCumulVarSoftUpperBound;
%rename (getCumulVarSoftUpperBoundCoefficient) RoutingDimension::GetCumulVarSoftUpperBoundCoefficient;
%rename (getGlobalSpanCostCoefficient) RoutingDimension::global_span_cost_coefficient;
%rename (getLocalOptimizerOffsetForVehicle) RoutingDimension::GetLocalOptimizerOffsetForVehicle;
%rename (setSoftSpanUpperBoundForVehicle) RoutingDimension::SetSoftSpanUpperBoundForVehicle;
%rename (hasSoftSpanUpperBounds) RoutingDimension::HasSoftSpanUpperBounds;
%rename (getSoftSpanUpperBoundForVehicle) RoutingDimension::GetSoftSpanUpperBoundForVehicle;
%rename (setQuadraticCostSoftSpanUpperBoundForVehicle) RoutingDimension::SetQuadraticCostSoftSpanUpperBoundForVehicle;
%rename (hasQuadraticCostSoftSpanUpperBounds) RoutingDimension::HasQuadraticCostSoftSpanUpperBounds;
%rename (getQuadraticCostSoftSpanUpperBoundForVehicle) RoutingDimension::GetQuadraticCostSoftSpanUpperBoundForVehicle;
%rename (getGroupDelay) RoutingDimension::GetGroupDelay;
%rename (getNodeVisitTransitsOfVehicle) RoutingDimension::GetNodeVisitTransitsOfVehicle;
%rename (getSpanCostCoefficientForVehicle) RoutingDimension::GetSpanCostCoefficientForVehicle;
%rename (getSpanUpperBoundForVehicle) RoutingDimension::GetSpanUpperBoundForVehicle;
%rename (getTransitValue) RoutingDimension::GetTransitValue;
%rename (getTransitValueFromClass) RoutingDimension::GetTransitValueFromClass;
%rename (hasCumulVarSoftLowerBound) RoutingDimension::HasCumulVarSoftLowerBound;
%rename (hasCumulVarSoftUpperBound) RoutingDimension::HasCumulVarSoftUpperBound;
%rename (hasPickupToDeliveryLimits) RoutingDimension::HasPickupToDeliveryLimits;
%rename (setBreakDistanceDurationOfVehicle) RoutingDimension::SetBreakDistanceDurationOfVehicle;
%rename (setBreakIntervalsOfVehicle) RoutingDimension::SetBreakIntervalsOfVehicle;
%rename (setCumulVarSoftLowerBound) RoutingDimension::SetCumulVarSoftLowerBound;
%rename (setCumulVarSoftUpperBound) RoutingDimension::SetCumulVarSoftUpperBound;
%rename (setGlobalSpanCostCoefficient) RoutingDimension::SetGlobalSpanCostCoefficient;
%rename (setPickupToDeliveryLimitFunctionForPair) RoutingDimension::SetPickupToDeliveryLimitFunctionForPair;
%rename (setSpanCostCoefficientForAllVehicles) RoutingDimension::SetSpanCostCoefficientForAllVehicles;
%rename (setSpanCostCoefficientForVehicle) RoutingDimension::SetSpanCostCoefficientForVehicle;
%rename (setSpanUpperBoundForVehicle) RoutingDimension::SetSpanUpperBoundForVehicle;
%rename (shortestTransitionSlack) RoutingDimension::ShortestTransitionSlack;
%rename (slackVar) RoutingDimension::SlackVar;
%rename (transitVar) RoutingDimension::TransitVar;
%rename (vehicleHasBreakIntervals) RoutingDimension::VehicleHasBreakIntervals;

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
%unignore SimpleBoundCosts;
%rename (getBoundCost) SimpleBoundCosts::bound_cost;
%rename (getSize) SimpleBoundCosts::Size;

}  // namespace operations_research::routing

// Generic rename rules.
%rename (buildSolution) *::BuildSolution;

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
PROTO_INPUT(operations_research::routing::RoutingSearchParameters,
            com.google.ortools.routing.RoutingSearchParameters,
            search_parameters)
PROTO_INPUT(operations_research::routing::RoutingModelParameters,
            com.google.ortools.routing.RoutingModelParameters,
            parameters)
PROTO2_RETURN(operations_research::routing::RoutingSearchParameters,
              com.google.ortools.routing.RoutingSearchParameters)
PROTO2_RETURN(operations_research::routing::RoutingModelParameters,
              com.google.ortools.routing.RoutingModelParameters)
PROTO_ENUM_RETURN(operations_research::routing::RoutingSearchStatus::Value,
                  com.google.ortools.routing.RoutingSearchStatus.Value)

// Wrap types.h, parameters.h according to the SWIG style guide.
%ignoreall
%unignore RoutingTransitCallback1;
%unignore RoutingTransitCallback2;
%unignore RoutingIndexPair;
%unignore RoutingIndexPairs;

// Add needed import to Globals.java
%pragma(java) moduleimports=%{
// Types from ConstraintSolver
import com.google.ortools.constraintsolver.Assignment;
import com.google.ortools.constraintsolver.IntVar;
%}

namespace operations_research::routing {
// Globals
// IMPORTANT(user): These functions from parameters.h are global, so in Java
// they are in the Globals.java (import com.[...].routing.Globals).
%rename (defaultRoutingSearchParameters) DefaultRoutingSearchParameters;
%rename (defaultRoutingModelParameters) DefaultRoutingModelParameters;
%rename (findErrorInRoutingSearchParameters) FindErrorInRoutingSearchParameters;
%rename (makeSetValuesFromTargets) MakeSetValuesFromTargets;
}  // namespace operations_research::routing

%include "ortools/routing/types.h"
%include "ortools/routing/parameters.h"
%unignoreall

// TODO(user): Use ignoreall/unignoreall for this one. A lot of work.
//swiglint: disable include-h-allglobals
%include "ortools/routing/routing.h"
