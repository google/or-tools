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

%include "constraint_solver/csharp/constraint_solver.swig"

// Include the file we want to wrap a first time.
%{
#include "constraint_solver/routing.h"
%}

// Convert RoutingModel::NodeIndex to (32-bit signed) integers.
%typemap(ctype) operations_research::RoutingModel::NodeIndex "int"
%typemap(imtype) operations_research::RoutingModel::NodeIndex "int"
%typemap(cstype) operations_research::RoutingModel::NodeIndex "int"
%typemap(csin) operations_research::RoutingModel::NodeIndex "$csinput"
%typemap(csout) operations_research::RoutingModel::NodeIndex {
  return $imcall;
}
%typemap(in) operations_research::RoutingModel::NodeIndex {
  $1 = operations_research::RoutingModel::NodeIndex($input);
}
%typemap(out) operations_research::RoutingModel::NodeIndex {
  $result = $1.value();
}
%typemap(csvarin)  operations_research::RoutingModel::NodeIndex
%{
        set { $imcall; }
%}
%typemap(csvarout, excode=SWIGEXCODE)  operations_research::RoutingModel::NodeIndex
%{
  get {
        return $imcall;
  }
%}

// Convert std::vector<RoutingModel::NodeIndex> to/from int arrays.
CS_TYPEMAP_STDVECTOR(operations_research::RoutingModel::NodeIndex, int, int);
CS_TYPEMAP_STDVECTOR_IN1(operations_research::RoutingModel::NodeIndex, int, int);

// Create input mapping for NodeEvaluator2
%module(directors="1") main
%feature("director") NodeEvaluator2;
%{
class NodeEvaluator2 : private operations_research::RoutingModel::NodeEvaluator2 {
 public:
  NodeEvaluator2() : used_as_permanent_handler_(false) {}
  virtual int64 Run(int i, int j) = 0;
  operations_research::RoutingModel::NodeEvaluator2* GetPermanentCallback() {
    CHECK(!used_as_permanent_handler_);
    used_as_permanent_handler_ = true;
    return this;
  }

  virtual ~NodeEvaluator2() {}

 private:
  virtual bool IsRepeatable() const { return true; }

  virtual int64 Run(operations_research::RoutingModel::NodeIndex i,
                    operations_research::RoutingModel::NodeIndex j) {
    return Run(i.value(), j.value());
  }

  bool used_as_permanent_handler_;
};
%}

%typemap(cscode) NodeEvaluator2 %{
  public SWIGTYPE_p_ResultCallback2T_long_long__RoutingModel_NodeIndex__RoutingModel_NodeIndex_t DisownAndGetPermanentCallback() {
    swigCMemOwn = false;
    return GetPermanentCallback();
  }
%}

class NodeEvaluator2 : private operations_research::RoutingModel::NodeEvaluator2 {
 public:
  NodeEvaluator2() : used_as_permanent_handler_(false) {}

  virtual ~NodeEvaluator2() {}

  virtual int64 Run(int i, int j) = 0;

  operations_research::RoutingModel::NodeEvaluator2* GetPermanentCallback() {
    CHECK(!used_as_permanent_handler_);
    used_as_permanent_handler_ = true;
    return this;
  }

 private:
  virtual bool IsRepeatable() const { return true; }

  virtual int64 Run(operations_research::RoutingModel::NodeIndex i,
                    operations_research::RoutingModel::NodeIndex j) {
    return Run(i.value(), j.value());
  }

  bool used_as_permanent_handler_;
};

%typemap(cstype) operations_research::RoutingModel::NodeEvaluator2* "NodeEvaluator2";
%typemap(csin) operations_research::RoutingModel::NodeEvaluator2* "$descriptor(ResultCallback2<int64, _RoutingModel_NodeIndex, _RoutingModel_NodeIndex>*).getCPtr($csinput.DisownAndGetPermanentCallback())";

// Add a reference to callbacks on the routing model such that they
// are not garbaged out.

%typemap(cscode) operations_research::RoutingModel %{
  public bool AddDimension(NodeEvaluator2 evaluator, long slack_max,
                           long capacity, bool fix_start_cumul_to_zero,
                           string name) {
    pinned_node_evaluator2_.Add(evaluator);
    return AddDimensionAux(evaluator, slack_max, capacity,
                           fix_start_cumul_to_zero, name);
  }

  public bool AddDimensionWithVehicleCapacity(
      NodeEvaluator2 evaluator, long slack_max,
      LongResultCallback1 vehicle_capacity, bool fix_start_cumul_to_zero,
      string name) {
    pinned_node_evaluator2_.Add(evaluator);
    pinned_long_result_callback_1_.Add(vehicle_capacity);
    return AddDimensionWithVehicleCapacityAux(
        evaluator, slack_max, vehicle_capacity, fix_start_cumul_to_zero, name);
  }

  public void SetArcCostEvaluatorOfAllVehicles(NodeEvaluator2 evaluator) {
    pinned_node_evaluator2_.Add(evaluator);
    SetArcCostEvaluatorOfAllVehiclesAux(evaluator);
  }

  public void SetArcCostEvaluatorOfVehicle(NodeEvaluator2 evaluator,
                                           int vehicle) {
    pinned_node_evaluator2_.Add(evaluator);
    SetArcCostEvaluatorOfVehicleAux(evaluator, vehicle);
  }

  private System.Collections.Generic.List<NodeEvaluator2>
      pinned_node_evaluator2_ =
          new System.Collections.Generic.List<NodeEvaluator2>();
  private System.Collections.Generic.List<LongResultCallback1>
      pinned_long_result_callback_1_ =
          new System.Collections.Generic.List<LongResultCallback1>();
%}

%rename (AddDimensionAux) operations_research::RoutingModel::AddDimension;
%rename (AddDimensionWithVehicleCapacityAux) operations_research::RoutingModel::AddDimensionWithVehicleCapacity;
%rename (SetArcCostEvaluatorOfAllVehiclesAux) operations_research::RoutingModel::SetArcCostEvaluatorOfAllVehicles;
%rename (SetArcCostEvaluatorOfVehicleAux) operations_research::RoutingModel::SetArcCostEvaluatorOfVehicle;

%ignore operations_research::RoutingModel::AddVectorDimension(
    const int64* values,
    int64 capacity,
    const std::string& name);

%ignore operations_research::RoutingModel::AddMatrixDimension(
    const int64* const* values,
    int64 capacity,
    const std::string& name);

%extend operations_research::RoutingModel {
  void AddVectorDimension(const std::vector<int64>& values,
                          int64 capacity,
                          bool fix_start_cumul_to_zero,
                          const std::string& name) {
    DCHECK_EQ(values.size(), self->nodes());
    self->AddVectorDimension(values.data(), capacity,
                             fix_start_cumul_to_zero, name);
  }
}

%ignore operations_research::RoutingModel::WrapIndexEvaluator(
    Solver::IndexEvaluator2* evaluator);

%ignore operations_research::RoutingModel::RoutingModel(
    int nodes, int vehicles,
    const std::vector<std::pair<NodeIndex, NodeIndex> >& start_end);

%rename("%(camelcase)s", %$isfunction) "";

// Wrap cp includes
%include constraint_solver/routing.h
