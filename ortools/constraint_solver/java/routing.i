// Copyright 2010-2021 Google LLC
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

%include "std_pair.i"
%template(IntBoolPair) std::pair<int, bool>;

%include "ortools/constraint_solver/java/constraint_solver.i"
%include "ortools/constraint_solver/java/routing_types.i"
%include "ortools/constraint_solver/java/routing_index_manager.i"
%include "ortools/util/java/sorted_interval_list.i"

// We need to forward-declare the proto here, so that PROTO_INPUT involving it
// works correctly. The order matters very much: this declaration needs to be
// before the %{ #include ".../routing.h" %}.
namespace operations_research {
class RoutingModelParameters;
class RoutingSearchParameters;
}  // namespace operations_research

// Include the files we want to wrap a first time.
%{
#include "ortools/constraint_solver/routing_types.h"
#include "ortools/constraint_solver/routing_parameters.pb.h"
#include "ortools/constraint_solver/routing_parameters.h"
#include "ortools/constraint_solver/routing.h"
#include <memory>
%}

// RoutingModel methods.
DEFINE_INDEX_TYPE_TYPEDEF(
    operations_research::RoutingCostClassIndex,
    operations_research::RoutingModel::CostClassIndex);
DEFINE_INDEX_TYPE_TYPEDEF(
    operations_research::RoutingDimensionIndex,
    operations_research::RoutingModel::DimensionIndex);
DEFINE_INDEX_TYPE_TYPEDEF(
    operations_research::RoutingDisjunctionIndex,
    operations_research::RoutingModel::DisjunctionIndex);
DEFINE_INDEX_TYPE_TYPEDEF(
    operations_research::RoutingVehicleClassIndex,
    operations_research::RoutingModel::VehicleClassIndex);

namespace operations_research {

// RoutingModel
%unignore RoutingModel;
// Map transit callback to Java @FunctionalInterface types.
// This replaces the RoutingTransitCallback[1-2] in the Java proxy class
%typemap(javaimports) RoutingModel %{
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
%rename (closeVisitTypes) RoutingModel::CloseVisitTypes;
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
%rename (registerPositiveUnaryTransitCallback) RoutingModel::RegisterPositiveUnaryTransitCallback; // not tested

%rename (registerTransitMatrix) RoutingModel::RegisterTransitMatrix;
%rename (registerTransitCallback) RoutingModel::RegisterTransitCallback;
%rename (registerPositiveTransitCallback) RoutingModel::RegisterPositiveTransitCallback; // not tested

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
%rename (unperformedPenalty) RoutingModel::UnperformedPenalty;
%rename (unperformedPenaltyOrValue) RoutingModel::UnperformedPenaltyOrValue;
%rename (vehicleVar) RoutingModel::VehicleVar;
%rename (vehicleVars) RoutingModel::VehicleVars;
%rename (writeAssignment) RoutingModel::WriteAssignment;

// RoutingDimension
%unignore RoutingDimension;
// Map transit callback to Java @FunctionalInterface types.
// This replaces the RoutingTransitCallback[1-2] in the Java proxy class
%typemap(javaimports) RoutingDimension %{
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

}  // namespace operations_research

// Generic rename rules.
%rename (buildSolution) *::BuildSolution;

// Protobuf support
PROTO_INPUT(operations_research::RoutingSearchParameters,
            com.google.ortools.constraintsolver.RoutingSearchParameters,
            search_parameters)
PROTO_INPUT(operations_research::RoutingModelParameters,
            com.google.ortools.constraintsolver.RoutingModelParameters,
            parameters)
PROTO2_RETURN(operations_research::RoutingSearchParameters,
              com.google.ortools.constraintsolver.RoutingSearchParameters)
PROTO2_RETURN(operations_research::RoutingModelParameters,
              com.google.ortools.constraintsolver.RoutingModelParameters)

// Wrap routing_types.h, routing_parameters.h according to the SWIG styleguide.
%ignoreall
%unignore RoutingTransitCallback1;
%unignore RoutingTransitCallback2;
%unignore RoutingIndexPair;
%unignore RoutingIndexPairs;

namespace operations_research {
// IMPORTANT(viger): These functions from routing_parameters.h are global, so in
// java they are in the main.java (import com.[...].constraintsolver.main).
%rename (defaultRoutingSearchParameters) DefaultRoutingSearchParameters;
%rename (defaultRoutingModelParameters) DefaultRoutingModelParameters;
%rename (findErrorInRoutingSearchParameters) FindErrorInRoutingSearchParameters;
%rename (makeSetValuesFromTargets) MakeSetValuesFromTargets;
}  // namespace operations_research

%include "ortools/constraint_solver/routing_types.h"
%include "ortools/constraint_solver/routing_parameters.h"
%unignoreall

// TODO(user): Use ignoreall/unignoreall for this one. A lot of work.
//swiglint: disable include-h-allglobals
%include "ortools/constraint_solver/routing.h"
