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

%include "ortools/base/base.i"

%include "ortools/constraint_solver/python/constraint_solver.i"
// TODO(user): remove this when we no longer use callbacks in the routing.
#define FATAL_CALLBACK_EXCEPTION
%include "ortools/base/python/callbacks.i"

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
%typemap(in) operations_research::RoutingModel::NodeIndex {
  $1 = operations_research::RoutingModel::NodeIndex(PyInt_AsLong($input));
}
%typemap(out) operations_research::RoutingModel::NodeIndex {
  $result = PyInt_FromLong($1.value());
}

// Convert std::vector<RoutingModel::NodeIndex> to/from int arrays.
%{
template<>
bool PyObjAs(PyObject *py, operations_research::RoutingModel::NodeIndex* i) {
  int temp;
  if (!PyObjAs(py, &temp)) return false;
  *i = operations_research::RoutingModel::NodeIndex(temp);
  return true;
}
%}
PY_LIST_OUTPUT_TYPEMAP(operations_research::RoutingModel::NodeIndex,
                       PyInt_Check, PyInt_FromLong);
PY_LIST_LIST_INPUT_TYPEMAP(operations_research::RoutingModel::NodeIndex,
                           PyInt_Check);
// TODO(user): also support std::vector<std::vector<>> <-> list of list.

// Create input mapping for NodeEvaluator2
%{
static int64 PyCallback2NodeIndexNodeIndex(
    PyObject* pyfunc,
    operations_research::RoutingModel::NodeIndex i,
    operations_research::RoutingModel::NodeIndex j) {
  int64 result = 0;
  // Cast to int needed, no int64 support
  PyObject* arglist = Py_BuildValue("ll",
                                    i.value<int>(),
                                    j.value<int>());
  PyObject* pyresult = PyEval_CallObject(pyfunc, arglist);
  Py_DECREF(arglist);
  if (pyresult) {
    result = PyInt_AsLong(pyresult);
  }
  Py_XDECREF(pyresult);
  return result;
}
%}
%typemap(in) operations_research::RoutingModel::NodeEvaluator2* {
  if (!PyCallable_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "Need a callable object!");
    SWIG_fail;
  }
  $1 = NewPermanentCallback(&PyCallback2NodeIndexNodeIndex, $input);
}
// Create conversion of vectors of NodeEvaluator2
%{
template<>
bool PyObjAs(PyObject* py_obj,
             operations_research::RoutingModel::NodeEvaluator2** b) {
  if (!PyCallable_Check(py_obj)) {
    PyErr_SetString(PyExc_TypeError, "Need a callable object!");
    return false;
  }
  *b = NewPermanentCallback(&PyCallback2NodeIndexNodeIndex, py_obj);
  return true;
}
%}
// Passing an empty parameter as converter is ok here since no API outputs
// a vector of NodeEvaluator2*.
PY_LIST_OUTPUT_TYPEMAP(operations_research::RoutingModel::NodeEvaluator2*,
                       PyCallable_Check, );

%ignore operations_research::RoutingModel::AddMatrixDimension(
    std::vector<std::vector<int64> > values,
    int64 capacity,
    const std::string& name);

%extend operations_research::RoutingModel {
  void AddMatrixDimension(
    const std::vector<std::vector<int64> >& values,
    int64 capacity,
    bool fix_start_cumul_to_zero,
    const std::string& name) {
    $self->AddMatrixDimension(values, capacity, fix_start_cumul_to_zero, name);
  }
}

%ignore operations_research::RoutingModel::MakeStateDependentTransit;
%ignore operations_research::RoutingModel::AddDimensionDependentDimensionWithVehicleCapacity;

PY_PROTO_TYPEMAP(ortools.constraint_solver.routing_parameters_pb2,
                 RoutingModelParameters,
                 operations_research::RoutingModelParameters)
PY_PROTO_TYPEMAP(ortools.constraint_solver.routing_parameters_pb2,
                 RoutingSearchParameters,
                 operations_research::RoutingSearchParameters)

%ignore operations_research::RoutingModel::WrapIndexEvaluator(
    Solver::IndexEvaluator2* evaluator);

%ignore operations_research::RoutingModel::RoutingModel(
    int nodes, int vehicles, NodeIndex depot);

%ignore operations_research::RoutingModel::RoutingModel(
    int nodes, int vehicles, NodeIndex depot,
    const RoutingModelParameters& parameters);

%extend operations_research::RoutingModel {
  RoutingModel(int nodes, int vehicles, int depot) {
    operations_research::RoutingModel* model =
        new operations_research::RoutingModel(
            nodes, vehicles,
            operations_research::RoutingModel::NodeIndex(depot));
    return model;
  }
  RoutingModel(int nodes, int vehicles, int depot,
               const RoutingModelParameters& parameters) {
      operations_research::RoutingModel* model =
          new operations_research::RoutingModel(
              nodes, vehicles,
              operations_research::RoutingModel::NodeIndex(depot), parameters);
      return model;
  }
}

%ignore operations_research::RoutingModel::RoutingModel(
    int nodes, int vehicles,
    const std::vector<std::pair<NodeIndex, NodeIndex> >& start_end);

%ignore operations_research::RoutingModel::RoutingModel(
    int nodes, int vehicles,
    const std::vector<std::pair<NodeIndex, NodeIndex> >& start_end,
    const RoutingModelParameters& parameters);

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
