// Copyright 2010-2017 Google
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

// Wrapper for std::function<int64(int64, int64)>
WRAP_STD_FUNCTION_JAVA(
    LongLongToLong,
    "com/google/ortools/constraintsolver/",
    int64, Long, int64, int64)

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
%rename (solve) Solve;
%rename (solveWithParameters) SolveWithParameters;
%rename (solveFromAssignmentWithParameters) SolveFromAssignmentWithParameters;
%rename (registerTransitCallback) RegisterTransitCallback;
%rename (registerUnaryTransitCallback) RegisterUnaryTransitCallback;
%rename (setArcCostEvaluatorOfAllVehicles) SetArcCostEvaluatorOfAllVehicles;
%rename (setArcCostEvaluatorOfVehicle) SetArcCostEvaluatorOfVehicle;
%rename (addDimension) AddDimension;
%rename (addDimensionWithVehicleTransits) AddDimensionWithVehicleTransits;
%rename (addDimensionWithVehicleCapacity) AddDimensionWithVehicleCapacity;
%rename (addDimensionWithVehicleTransitAndCapacity) AddDimensionWithVehicleTransitAndCapacity;
%rename (addConstantDimension) AddConstantDimension;
%rename (addVectorDimension) AddVectorDimension;
%rename (getDimensionOrDie) GetDimensionOrDie;
%rename (getMutableDimension) GetMutableDimension;
%rename (addAllActive) AddAllActive;
%rename (addDisjunction) AddDisjunction;
%rename (addLocalSearchOperator) AddLocalSearchOperator;
%rename (addSearchMonitor) AddSearchMonitor;
%rename (applyLocks) ApplyLocks;
%rename (writeAssignment) WriteAssignment;
%rename (readAssignment) ReadAssignment;
%rename (start) Start;
%rename (end) End;
%rename (isStart) IsStart;
%rename (isEnd) IsEnd;
%rename (getArcCostForVehicle) GetArcCostForVehicle;
%rename (nexts) Nexts;
%rename (nextVar) NextVar;
%rename (vehicleVar) VehicleVar;
%rename (vehicleVars) VehicleVars;
%rename (activeVar) ActiveVar;
%rename (addToAssignment) AddToAssignment;
%rename (isVehicleUsed) IsVehicleUsed;
%rename (next) Next;
%rename (compactAssignment) CompactAssignment;
%rename (size) Size;
%rename (costVar) CostVar;
%rename (preAssignment) PreAssignment;
%rename (setFirstSolutionEvaluator) SetFirstSolutionEvaluator;
%rename (routesToAssignment) RoutesToAssignment;
%rename (closeModel) CloseModel;

// RoutingDimension methods.
%rename (cumulVar) CumulVar;
%rename (transitVar) TransitVar;
%rename (slackVar) SlackVar;
%rename (setSpanCostCoefficientForAllVehicles) SetSpanCostCoefficientForAllVehicles;
%rename (setSpanCostCoefficientForVehicle) SetSpanCostCoefficientForVehicle;
%rename (getSpanCostCoefficientForVehicle) GetSpanCostCoefficientForVehicle;
%rename (setGlobalSpanCostCoefficient) SetGlobalSpanCostCoefficient;
%rename (getGlobalSpanCostCoefficient) global_span_cost_coefficient;
%rename (setCumulVarSoftUpperBound) SetCumulVarSoftUpperBound;
%rename (getCumulVarSoftUpperBound) GetCumulVarSoftUpperBound;
%rename (getCumulVarSoftUpperBoundCoefficient) GetCumulVarSoftUpperBoundCoefficient;

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

%include "ortools/constraint_solver/routing_types.h"
%include "ortools/constraint_solver/routing_parameters.h"
%unignoreall

// TODO(user): Use ignoreall/unignoreall for this one. A lot of work.
%include "ortools/constraint_solver/routing.h"
