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

%include "ortools/constraint_solver/java/constraint_solver.i"
%include "ortools/constraint_solver/java/routing_types.i"
%include "ortools/constraint_solver/java/routing_index_manager.i"
%include "ortools/util/java/functions.i"

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
%}

%typemap(in) const std::vector<std::vector<operations_research::RoutingModel::NodeIndex> >&
(std::vector<std::vector<operations_research::RoutingModel::NodeIndex> > temp) {
  if ($input) {
    const int size = jenv->GetArrayLength($input);
    temp.clear();
    temp.resize(size);
    for (int i = 0; i < size; ++i) {
      jintArray values =
          (jintArray)jenv->GetObjectArrayElement((jobjectArray)$input, i);
      const int inner_size = jenv->GetArrayLength(values);
      jint* inner_values = jenv->GetIntArrayElements(values, nullptr);
      for (int j = 0; j < inner_size; ++j) {
        const int value = inner_values[j];
        temp[i].push_back(operations_research::RoutingModel::NodeIndex(value));
      }
      jenv->ReleaseIntArrayElements(values, inner_values, 0);
      jenv->DeleteLocalRef(values);
    }
    $1 = &temp;
  } else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null table");
    return $null;
  }
}

%ignore operations_research::RoutingModel::AddMatrixDimension(
    std::vector<std::vector<int64> > values,
    int64 capacity,
    bool fix_start_cumul_to_zero,
    const std::string& name);

%extend operations_research::RoutingModel {
  void addMatrixDimension(const std::vector<std::vector<int64> >& values,
                          int64 capacity, bool fix_start_cumul_to_zero,
                          const std::string& name) {
    $self->AddMatrixDimension(values, capacity, fix_start_cumul_to_zero, name);
  }
}

%ignore operations_research::RoutingModel::RegisterStateDependentTransitCallback;
%ignore operations_research::RoutingModel::StateDependentTransitCallback;
%ignore operations_research::RoutingModel::MakeStateDependentTransit;
%ignore operations_research::RoutingModel::AddDimensionDependentDimensionWithVehicleCapacity;

// RoutingModel methods.
%rename (activeVar) operations_research::RoutingModel::ActiveVar;
%rename (addAllActive) operations_research::RoutingModel::AddAllActive;
%rename (addAtSolutionCallback) operations_research::RoutingModel::AddAtSolutionCallback;
%rename (addConstantDimension) operations_research::RoutingModel::AddConstantDimension;
%rename (addConstantDimensionWithSlack) operations_research::RoutingModel::AddConstantDimensionWithSlack;
%rename (addDimension) operations_research::RoutingModel::AddDimension;
%rename (addDimensionWithVehicleCapacity) operations_research::RoutingModel::AddDimensionWithVehicleCapacity;
%rename (addDimensionWithVehicleTransitAndCapacity) operations_research::RoutingModel::AddDimensionWithVehicleTransitAndCapacity;
%rename (addDimensionWithVehicleTransits) operations_research::RoutingModel::AddDimensionWithVehicleTransits;
%rename (addDisjunction) operations_research::RoutingModel::AddDisjunction;
%rename (addIntervalToAssignment) operations_research::RoutingModel::AddIntervalToAssignment;
%rename (addLocalSearchFilter) operations_research::RoutingModel::AddLocalSearchFilter;
%rename (addLocalSearchOperator) operations_research::RoutingModel::AddLocalSearchOperator;
%rename (addPickupAndDelivery) operations_research::RoutingModel::AddPickupAndDelivery;
%rename (addPickupAndDeliverySets) operations_research::RoutingModel::AddPickupAndDeliverySets;
%rename (addSearchMonitor) operations_research::RoutingModel::AddSearchMonitor;
%rename (addSoftSameVehicleConstraint) operations_research::RoutingModel::AddSoftSameVehicleConstraint;
%rename (addToAssignment) operations_research::RoutingModel::AddToAssignment;
%rename (addTypeIncompatibility) operations_research::RoutingModel::AddTypeIncompatibility;
%rename (addVariableMaximizedByFinalizer) operations_research::RoutingModel::AddVariableMaximizedByFinalizer;
%rename (addVariableMinimizedByFinalizer) operations_research::RoutingModel::AddVariableMinimizedByFinalizer;
%rename (addVectorDimension) operations_research::RoutingModel::AddVectorDimension;
%rename (applyLocks) operations_research::RoutingModel::ApplyLocks;
%rename (applyLocksToAllVehicles) operations_research::RoutingModel::ApplyLocksToAllVehicles;
%rename (arcIsMoreConstrainedThanArc) operations_research::RoutingModel::ArcIsMoreConstrainedThanArc;
%rename (assignmentToRoutes) operations_research::RoutingModel::AssignmentToRoutes;
%rename (checkLimit) operations_research::RoutingModel::CheckLimit;
%rename (closeModel) operations_research::RoutingModel::CloseModel;
%rename (closeModelWithParameters) operations_research::RoutingModel::CloseModelWithParameters;
%rename (compactAndCheckAssignment) operations_research::RoutingModel::CompactAndCheckAssignment;
%rename (compactAssignment) operations_research::RoutingModel::CompactAssignment;
%rename (computeLowerBound) operations_research::RoutingModel::ComputeLowerBound;
%rename (costVar) operations_research::RoutingModel::CostVar;
%rename (costsAreHomogeneousAcrossVehicles) operations_research::RoutingModel::CostsAreHomogeneousAcrossVehicles;
%rename (debugOutputAssignment) operations_research::RoutingModel::DebugOutputAssignment;
%rename (end) operations_research::RoutingModel::End;
%rename (getAllDimensionNames) operations_research::RoutingModel::GetAllDimensionNames;
%rename (getAmortizedLinearCostFactorOfVehicles) operations_research::RoutingModel::GetAmortizedLinearCostFactorOfVehicles;
%rename (getAmortizedQuadraticCostFactorOfVehicles) operations_research::RoutingModel::GetAmortizedQuadraticCostFactorOfVehicles;
%rename (getArcCostForClass) operations_research::RoutingModel::GetArcCostForClass;
%rename (getArcCostForFirstSolution) operations_research::RoutingModel::GetArcCostForFirstSolution;
%rename (getArcCostForVehicle) operations_research::RoutingModel::GetArcCostForVehicle;
%rename (getCostClassIndexOfVehicle) operations_research::RoutingModel::GetCostClassIndexOfVehicle;
%rename (getCostClassesCount) operations_research::RoutingModel::GetCostClassesCount;
%rename (getDeliveryIndexPairs) operations_research::RoutingModel::GetDeliveryIndexPairs;
%rename (getDepot) operations_research::RoutingModel::GetDepot;
%rename (getDimensionOrDie) operations_research::RoutingModel::GetDimensionOrDie;
%rename (getDimensions) operations_research::RoutingModel::GetDimensions;
%rename (getDimensionsWithSoftAndSpanCosts) operations_research::RoutingModel::GetDimensionsWithSoftAndSpanCosts;
%rename (getDimensionsWithSoftOrSpanCosts) operations_research::RoutingModel::GetDimensionsWithSoftOrSpanCosts;
%rename (getDisjunctionIndices) operations_research::RoutingModel::GetDisjunctionIndices;
%rename (getDisjunctionMaxCardinality) operations_research::RoutingModel::GetDisjunctionMaxCardinality;
%rename (getDisjunctionPenalty) operations_research::RoutingModel::GetDisjunctionPenalty;
%rename (getFixedCostOfVehicle) operations_research::RoutingModel::GetFixedCostOfVehicle;
%rename (getHomogeneousCost) operations_research::RoutingModel::GetHomogeneousCost;
%rename (getMutableDimension) operations_research::RoutingModel::GetMutableDimension;
%rename (getNonZeroCostClassesCount) operations_research::RoutingModel::GetNonZeroCostClassesCount;
%rename (getNumberOfDecisionsInFirstSolution) operations_research::RoutingModel::GetNumberOfDecisionsInFirstSolution;
%rename (getNumberOfDisjunctions) operations_research::RoutingModel::GetNumberOfDisjunctions;
%rename (getNumberOfRejectsInFirstSolution) operations_research::RoutingModel::GetNumberOfRejectsInFirstSolution;
%rename (getNumberOfVisitTypes) operations_research::RoutingModel::GetNumberOfVisitTypes;
%rename (getPerfectBinaryDisjunctions) operations_research::RoutingModel::GetPerfectBinaryDisjunctions;
%rename (getPickupIndexPairs) operations_research::RoutingModel::GetPickupIndexPairs;
%rename (getPrimaryConstrainedDimension) operations_research::RoutingModel::GetPrimaryConstrainedDimension;
%rename (getSameVehicleIndicesOfIndex) operations_research::RoutingModel::GetSameVehicleIndicesOfIndex;
%rename (getTypeIncompatibilities) operations_research::RoutingModel::GetTypeIncompatibilities;
%rename (getVehicleClassIndexOfVehicle) operations_research::RoutingModel::GetVehicleClassIndexOfVehicle;
%rename (getVehicleClassesCount) operations_research::RoutingModel::GetVehicleClassesCount;
%rename (getVisitType) operations_research::RoutingModel::GetVisitType;
%rename (hasDimension) operations_research::RoutingModel::HasDimension;
%rename (hasVehicleWithCostClassIndex) operations_research::RoutingModel::HasVehicleWithCostClassIndex;
%rename (ignoreDisjunctionsAlreadyForcedToZero) operations_research::RoutingModel::IgnoreDisjunctionsAlreadyForcedToZero;
%rename (isEnd) operations_research::RoutingModel::IsEnd;
%rename (isMatchingModel) operations_research::RoutingModel::IsMatchingModel;
%rename (isStart) operations_research::RoutingModel::IsStart;
%rename (isVehicleAllowedForIndex) operations_research::RoutingModel::IsVehicleAllowedForIndex;
%rename (isVehicleUsed) operations_research::RoutingModel::IsVehicleUsed;
%rename (makeGuidedSlackFinalizer) operations_research::RoutingModel::MakeGuidedSlackFinalizer;
%rename (makeSelfDependentDimensionFinalizer) operations_research::RoutingModel::MakeSelfDependentDimensionFinalizer;
%rename (mutablePreAssignment) operations_research::RoutingModel::MutablePreAssignment;
%rename (next) operations_research::RoutingModel::Next;
%rename (nextVar) operations_research::RoutingModel::NextVar;
%rename (nexts) operations_research::RoutingModel::Nexts;
%rename (preAssignment) operations_research::RoutingModel::PreAssignment;
%rename (readAssignment) operations_research::RoutingModel::ReadAssignment;
%rename (readAssignmentFromRoutes) operations_research::RoutingModel::ReadAssignmentFromRoutes;
%rename (registerPositiveTransitCallback) operations_research::RoutingModel::RegisterPositiveTransitCallback;
%rename (registerTransitCallback) operations_research::RoutingModel::RegisterTransitCallback;
%rename (registerUnaryTransitCallback) operations_research::RoutingModel::RegisterUnaryTransitCallback;
%rename (restoreAssignment) operations_research::RoutingModel::RestoreAssignment;
%rename (routesToAssignment) operations_research::RoutingModel::RoutesToAssignment;
%rename (setAllowedVehiclesForIndex) operations_research::RoutingModel::SetAllowedVehiclesForIndex;
%rename (setAmortizedCostFactorsOfAllVehicles) operations_research::RoutingModel::SetAmortizedCostFactorsOfAllVehicles;
%rename (setAmortizedCostFactorsOfVehicle) operations_research::RoutingModel::SetAmortizedCostFactorsOfVehicle;
%rename (setArcCostEvaluatorOfAllVehicles) operations_research::RoutingModel::SetArcCostEvaluatorOfAllVehicles;
%rename (setArcCostEvaluatorOfVehicle) operations_research::RoutingModel::SetArcCostEvaluatorOfVehicle;
%rename (setAssignmentFromOtherModelAssignment) operations_research::RoutingModel::SetAssignmentFromOtherModelAssignment;
%rename (setFirstSolutionEvaluator) operations_research::RoutingModel::SetFirstSolutionEvaluator;
%rename (setFixedCostOfAllVehicles) operations_research::RoutingModel::SetFixedCostOfAllVehicles;
%rename (setFixedCostOfVehicle) operations_research::RoutingModel::SetFixedCostOfVehicle;
%rename (setPrimaryConstrainedDimension) operations_research::RoutingModel::SetPrimaryConstrainedDimension;
%rename (setVisitType) operations_research::RoutingModel::SetVisitType;
%rename (size) operations_research::RoutingModel::Size;
%rename (solve) operations_research::RoutingModel::Solve;
%rename (solveFromAssignmentWithParameters) operations_research::RoutingModel::SolveFromAssignmentWithParameters;
%rename (solveWithParameters) operations_research::RoutingModel::SolveWithParameters;
%rename (start) operations_research::RoutingModel::Start;
%rename (transitCallback) operations_research::RoutingModel::TransitCallback;
%rename (unaryTransitCallbackOrNull) operations_research::RoutingModel::UnaryTransitCallbackOrNull;
%rename (unperformedPenalty) operations_research::RoutingModel::UnperformedPenalty;
%rename (unperformedPenaltyOrValue) operations_research::RoutingModel::UnperformedPenaltyOrValue;
%rename (vehicleVar) operations_research::RoutingModel::VehicleVar;
%rename (vehicleVars) operations_research::RoutingModel::VehicleVars;
%rename (writeAssignment) operations_research::RoutingModel::WriteAssignment;

// RoutingDimension methods.
%rename (cumulVar) operations_research::RoutingDimension::CumulVar;
%rename (fixedTransitVar) operations_research::RoutingDimension::FixedTransitVar;
%rename (getBreakIntervalsOfVehicle) operations_research::RoutingDimension::GetBreakIntervalsOfVehicle;
%rename (getCumulVarSoftLowerBound) operations_research::RoutingDimension::GetCumulVarSoftLowerBound;
%rename (getCumulVarSoftLowerBoundCoefficient) operations_research::RoutingDimension::GetCumulVarSoftLowerBoundCoefficient;
%rename (getCumulVarSoftUpperBound) operations_research::RoutingDimension::GetCumulVarSoftUpperBound;
%rename (getCumulVarSoftUpperBoundCoefficient) operations_research::RoutingDimension::GetCumulVarSoftUpperBoundCoefficient;
%rename (getGlobalSpanCostCoefficient) operations_research::RoutingDimension::global_span_cost_coefficient;
%rename (getGroupDelay) operations_research::RoutingDimension::GetGroupDelay;
%rename (getNodeVisitTransitsOfVehicle) operations_research::RoutingDimension::GetNodeVisitTransitsOfVehicle;
%rename (getSpanCostCoefficientForVehicle) operations_research::RoutingDimension::GetSpanCostCoefficientForVehicle;
%rename (getSpanUpperBoundForVehicle) operations_research::RoutingDimension::GetSpanUpperBoundForVehicle;
%rename (getTransitValue) operations_research::RoutingDimension::GetTransitValue;
%rename (getTransitValueFromClass) operations_research::RoutingDimension::GetTransitValueFromClass;
%rename (hasCumulVarSoftLowerBound) operations_research::RoutingDimension::HasCumulVarSoftLowerBound;
%rename (hasCumulVarSoftUpperBound) operations_research::RoutingDimension::HasCumulVarSoftUpperBound;
%rename (hasPickupToDeliveryLimits) operations_research::RoutingDimension::HasPickupToDeliveryLimits;
%rename (setBreakIntervalsOfVehicle) operations_research::RoutingDimension::SetBreakIntervalsOfVehicle;
%rename (setCumulVarSoftLowerBound) operations_research::RoutingDimension::SetCumulVarSoftLowerBound;
%rename (setCumulVarSoftUpperBound) operations_research::RoutingDimension::SetCumulVarSoftUpperBound;
%rename (setGlobalSpanCostCoefficient) operations_research::RoutingDimension::SetGlobalSpanCostCoefficient;
%rename (setPickupToDeliveryLimitFunctionForPair) operations_research::RoutingDimension::SetPickupToDeliveryLimitFunctionForPair;
%rename (setSpanCostCoefficientForAllVehicles) operations_research::RoutingDimension::SetSpanCostCoefficientForAllVehicles;
%rename (setSpanCostCoefficientForVehicle) operations_research::RoutingDimension::SetSpanCostCoefficientForVehicle;
%rename (setSpanUpperBoundForVehicle) operations_research::RoutingDimension::SetSpanUpperBoundForVehicle;
%rename (shortestTransitionSlack) operations_research::RoutingDimension::ShortestTransitionSlack;
%rename (slackVar) operations_research::RoutingDimension::SlackVar;
%rename (transitVar) operations_research::RoutingDimension::TransitVar;
%rename (vehicleHasBreakIntervals) operations_research::RoutingDimension::VehicleHasBreakIntervals;

// RoutingFilteredDecisionBuilder methods.
%rename (getEndChainStart) operations_research::RoutingFilteredDecisionBuilder::GetEndChainStart;
%rename (getStartChainEnd) operations_research::RoutingFilteredDecisionBuilder::GetStartChainEnd;
%rename (initializeRoutes) operations_research::RoutingFilteredDecisionBuilder::InitializeRoutes;
%rename (makeDisjunctionNodesUnperformed) operations_research::RoutingFilteredDecisionBuilder::MakeDisjunctionNodesUnperformed;
%rename (makeUnassignedNodesUnperformed) operations_research::RoutingFilteredDecisionBuilder::MakeUnassignedNodesUnperformed;

// IntVarFilteredDecisionBuilder.
%rename (buildSolution) operations_research::IntVarFilteredDecisionBuilder::BuildSolution;

// GlobalCheapestInsertionFilteredDecisionBuilder.
%rename (buildSolution) operations_research::GlobalCheapestInsertionFilteredDecisionBuilder::BuildSolution;

// LocalCheapestInsertionFilteredDecisionBuilder.
%rename (buildSolution) operations_research::LocalCheapestInsertionFilteredDecisionBuilder::BuildSolution;

// CheapestAdditionFilteredDecisionBuilder.
%rename (buildSolution) operations_research::CheapestAdditionFilteredDecisionBuilder::BuildSolution;

// ChristofidesFilteredDecisionBuilder.
%rename (buildSolution) operations_research::ChristofidesFilteredDecisionBuilder::BuildSolution;

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
%unignore RoutingTransitCallback2;
%unignore RoutingIndexPair;
%unignore RoutingIndexPairs;

// IMPORTANT(viger): These functions from routing_parameters.h are global, so in
// java they are in the main.java (import com.[...].constraintsolver.main).
%rename (defaultRoutingSearchParameters) DefaultRoutingSearchParameters;
%rename (defaultRoutingModelParameters) DefaultRoutingModelParameters;
%rename (findErrorInRoutingSearchParameters) FindErrorInRoutingSearchParameters;
%rename (makeSetValuesFromTargets) operations_research::MakeSetValuesFromTargets;

%include "ortools/constraint_solver/routing_types.h"
%include "ortools/constraint_solver/routing_parameters.h"
%unignoreall

// TODO(user): Use ignoreall/unignoreall for this one. A lot of work.
%include "ortools/constraint_solver/routing.h"
