// Copyright 2010-2014 Google
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
%}

// Convert RoutingModel::NodeIndex to (32-bit signed) integers.
%typemap(jni) operations_research::RoutingModel::NodeIndex "jint"
%typemap(jtype) operations_research::RoutingModel::NodeIndex "int"
%typemap(jstype) operations_research::RoutingModel::NodeIndex "int"
%typemap(javain) operations_research::RoutingModel::NodeIndex "$javainput"
%typemap(javaout) operations_research::RoutingModel::NodeIndex {
  return $jnicall;
}
%typemap(in) operations_research::RoutingModel::NodeIndex {
  $1 = operations_research::RoutingModel::NodeIndex($input);
}
%typemap(out) operations_research::RoutingModel::NodeIndex {
  $result = (jlong)$1.value();
}

// Convert std::vector<RoutingModel::NodeIndex> to/from int arrays.
VECTOR_AS_JAVA_ARRAY(operations_research::RoutingModel::NodeIndex, int, Int);

// TODO(user): define a macro in util/java/vector.i for std::vector<std::vector<>> and
// reuse it here.
%typemap(jni) const std::vector<std::vector<operations_research::RoutingModel::NodeIndex> >& "jobjectArray"
%typemap(jtype) const std::vector<std::vector<operations_research::RoutingModel::NodeIndex> >& "int[][]"
%typemap(jstype) const std::vector<std::vector<operations_research::RoutingModel::NodeIndex> >& "int[][]"
%typemap(javain) const std::vector<std::vector<operations_research::RoutingModel::NodeIndex> >& "$javainput"

// Useful directors.
%feature("director") NodeEvaluator2;

%{
#include <vector>
#include "ortools/base/callback.h"
#include "ortools/base/integral_types.h"

// When a director is created for a class with SWIG, the C++ part of the
// director keeps a JNI global reference to the Java part. This global reference
// only gets deleted in the destructor of the C++ part, but by default, this
// only happens when the Java part is processed by the GC (however, this never
// happens, because there is the JNI global reference...).
//
// To break the cycle, it is necessary to delete the C++ part manually. For the
// callback classes, this is done by deriving them from the respective C++
// ResultCallback classes. When the java callback class is asked for a C++
// callback class, it hands over its C++ part. It is expected, that whoever
// receives the C++ callback class, owns it and destroys it after they no longer
// need it. But by destroying it, they also break the reference cycle and the
// Java part may be processed by the GC.
//
// When created, instances of NodeEvaluator2 must thus be used in a context
// where someone takes ownership of the C++ part of the NodeEvaluator2 and
// deletes it when no longer needed. Otherwise, the object would remain on the
// heap forever.
class NodeEvaluator2 : private operations_research::RoutingModel::NodeEvaluator2 {
 public:
  NodeEvaluator2() : used_as_permanent_handler_(false) {}
  virtual int64 run(int i, int j) = 0;
  operations_research::RoutingModel::NodeEvaluator2* getPermanentCallback() {
    CHECK(!used_as_permanent_handler_);
    used_as_permanent_handler_ = true;
    // The evaluator is wrapped to avoid having its ownership shared between
    // jni/java and C++. C++ will take care of handling the wrapper while the
    // actual evaluator will be handled by java. Refer to the typemap for
    // NodeEvaluator2 below to see how this method is called.
    return NewPermanentCallback(this, &NodeEvaluator2::Run);
  }
  virtual ~NodeEvaluator2() {}

 private:
  virtual bool IsRepeatable() const { return true; }
  virtual int64 Run(operations_research::RoutingModel::NodeIndex i,
                    operations_research::RoutingModel::NodeIndex j) {
    return run(i.value(), j.value());
  }
  bool used_as_permanent_handler_;
};
%}

class NodeEvaluator2 : private operations_research::RoutingModel::NodeEvaluator2 {
 public:
  NodeEvaluator2() : used_as_permanent_handler_(false) {}
  virtual int64 run(int i, int j) = 0;
  operations_research::RoutingModel::NodeEvaluator2* getPermanentCallback();
  virtual ~NodeEvaluator2() {}
};

// Typemaps for callbacks in java.
%typemap(jstype) operations_research::RoutingModel::NodeEvaluator2* "NodeEvaluator2";
%typemap(javain) operations_research::RoutingModel::NodeEvaluator2* "$descriptor(ResultCallback2<int64, RoutingNodeIndex, RoutingNodeIndex>*).getCPtr($javainput.getPermanentCallback())";

namespace operations_research {
%define NODE_EVALUATOR_CAST(CType, ptr)
reinterpret_cast<NodeEvaluator2*>(ptr)->getPermanentCallback()
%enddef

CONVERT_VECTOR_WITH_CAST(RoutingModel::NodeEvaluator2, NodeEvaluator2, NODE_EVALUATOR_CAST);
}  // namespace operations_research

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

%ignore operations_research::RoutingModel::MakeStateDependentTransit;
%ignore operations_research::RoutingModel::AddDimensionDependentDimensionWithVehicleCapacity;
%ignore operations_research::RoutingModel::RoutingModel(
    int nodes, int vehicles,
    const std::vector<std::pair<NodeIndex, NodeIndex> >& start_end);

// RoutingModel methods.
%rename (solve) Solve;
%rename (solveWithParameters) SolveWithParameters;
%rename (solveFromAssignmentWithParameters) SolveFromAssignmentWithParameters;
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
%rename (defaultSearchParameters) DefaultSearchParameters;
%rename (defaultModelParameters) DefaultModelParameters;

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

// DEPRECATED METHODS. See ./routing.h for how to replace them.
%rename (setCost) SetCost;
%rename (setVehicleCost) SetVehicleCost;
%rename (setDimensionTransitCost) SetDimensionTransitCost;
%rename (getDimensionTransitCost) GetDimensionTransitCost;
%rename (setDimensionSpanCost) SetDimensionSpanCost;
%rename (getDimensionSpanCost) GetDimensionSpanCost;


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

%ignoreall
%unignore RoutingNodeIndex;
%unignore RoutingCostClassIndex;
%unignore RoutingDimensionIndex;
%unignore RoutingDisjunctionIndex;
%unignore RoutingVehicleClassIndex;
%unignore RoutingNodeEvaluator2;
%unignore RoutingTransitEvaluator2;
%unignore RoutingNodePair;
%unignore RoutingNodePairs;
%include "ortools/constraint_solver/routing_types.h"
%unignoreall

// TODO(user): Use ignoreall/unignoreall for this one. A lot of work.
%include "ortools/constraint_solver/routing.h"
