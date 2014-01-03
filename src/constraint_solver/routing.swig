// Copyright 2010-2013 Google
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

%include "constraint_solver/constraint_solver.swig"

// Include the file we want to wrap a first time.
%{
#include "constraint_solver/routing.h"
%}

// Convert RoutingModel::NodeIndex to (32-bit signed) integers.
#ifdef SWIGPYTHON
%typemap(in) operations_research::RoutingModel::NodeIndex {
  $1 = operations_research::RoutingModel::NodeIndex(PyInt_AsLong($input));
}
%typemap(out) operations_research::RoutingModel::NodeIndex {
  $result = PyInt_FromLong($1.value());
}
#endif  // SWIGPYTHON
#ifdef SWIGJAVA
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
#endif  // SWIGJAVA
#ifdef SWIGCSHARP
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
#endif  // SWIGCSHARP

// Convert std::vector<RoutingModel::NodeIndex> to/from int arrays.
#ifdef SWIGPYTHON
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
// TODO(user): also support std::vector<std::vector<>> <-> list of list for python.
#endif  // SWIGPYTHON
#ifdef SWIGJAVA
VECTOR_AS_JAVA_ARRAY(operations_research::RoutingModel::NodeIndex, int, Int);

// TODO(user): define a macro in util/data.swig for std::vector<std::vector<>> and reuse
// it here.
%typemap(jni) const std::vector<std::vector<operations_research::RoutingModel::NodeIndex> >& "jobjectArray"
%typemap(jtype) const std::vector<std::vector<operations_research::RoutingModel::NodeIndex> >& "int[][]"
%typemap(jstype) const std::vector<std::vector<operations_research::RoutingModel::NodeIndex> >& "int[][]"
%typemap(javain) const std::vector<std::vector<operations_research::RoutingModel::NodeIndex> >& "$javainput"

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
#endif  // SWIGJAVA
#ifdef SWIGCSHARP
CS_TYPEMAP_STDVECTOR(operations_research::RoutingModel::NodeIndex, int, int);
CS_TYPEMAP_STDVECTOR_IN1(operations_research::RoutingModel::NodeIndex, int, int);
#endif  // SWIGCSHARP

// Create input mapping for NodeEvaluator2
#ifdef SWIGPYTHON
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
#endif
#ifdef SWIGJAVA
%module(directors="1") main
%feature("director") NodeEvaluator2;
%{
// When created, instances of NodeEvaluator2 must be used in a context where
// someone takes ownership of the C++ part of the NodeEvaluator2 and deletes
// it when no longer needed. Otherwise, the object would remain on the heap
// forever. See the comment for the LongResultCallback* classes in
// ../util/data.swig for more details.
class NodeEvaluator2 : private operations_research::RoutingModel::NodeEvaluator2 {
 public:
  NodeEvaluator2() : used_as_permanent_handler_(false) {}
  virtual int64 run(int i, int j) = 0;
  operations_research::RoutingModel::NodeEvaluator2* getPermanentCallback() {
    CHECK(!used_as_permanent_handler_);
    used_as_permanent_handler_ = true;
    return this;
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
  operations_research::RoutingModel::NodeEvaluator2* getPermanentCallback() {
    CHECK(!used_as_permanent_handler_);
    used_as_permanent_handler_ = true;
    return this;
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

%typemap(jstype) operations_research::RoutingModel::NodeEvaluator2* "NodeEvaluator2";
%typemap(javain) operations_research::RoutingModel::NodeEvaluator2* "$descriptor(ResultCallback2<int64, _RoutingModel_NodeIndex, _RoutingModel_NodeIndex>*).getCPtr($javainput.getPermanentCallback())";
#endif  // SWIGJAVA

#ifdef SWIGCSHARP
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

#endif  // SWIGCSHARP
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

#ifdef SWIGJAVA
// RoutingModel methods.
%rename (solve) Solve;
%rename (setArcCostEvaluatorOfAllVehicles) SetArcCostEvaluatorOfAllVehicles;
%rename (setArcCostEvaluatorOfVehicle) SetArcCostEvaluatorOfVehicle;
%rename (addDimension) AddDimension;
%rename (addDimensionWithVehicleCapacity) AddDimensionWithVehicleCapacity;
%rename (addConstantDimension) AddConstantDimension;
%rename (addVectorDimension) AddVectorDimension;
%rename (getDimensionOrDie) GetDimensionOrDie;
%rename (getMutableDimension) GetMutableDimension;
%rename (addAllActive) AddAllActive;
%rename (addDisjunction) AddDisjunction;
%rename (addLocalSearchOperator) AddLocalSearchOperator;
%rename (setDepot) SetDepot;
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
%rename (activeVar) ActiveVar;
%rename (addToAssignment) AddToAssignment;
%rename (isVehicleUsed) IsVehicleUsed;
%rename (next) Next;
%rename (compactAssignment) CompactAssignment;
%rename (size) Size;
%rename (costVar) CostVar;
%rename (updateTimeLimit) UpdateTimeLimit;
%rename (preAssignment) PreAssignment;
%rename (getFirstSolutionStrategy) first_solution_strategy;
%rename (setFirstSolutionStrategy) set_first_solution_strategy;
%rename (setFirstSolutionEvaluator) SetFirstSolutionEvaluator;
%rename (getMetaheuristic) metaheuristic;
%rename (setMetaheuristic) set_metaheuristic;
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

// DEPRECATED METHODS. See ./routing.h for how to replace them.
%rename (setCost) SetCost;
%rename (setVehicleCost) SetVehicleCost;
%rename (setDimensionTransitCost) SetDimensionTransitCost;
%rename (getDimensionTransitCost) GetDimensionTransitCost;
%rename (setDimensionSpanCost) SetDimensionSpanCost;
%rename (getDimensionSpanCost) GetDimensionSpanCost;

#endif // SWIGJAVA

#ifdef SWIGCSHARP
%rename("%(camelcase)s", %$isfunction) "";
#endif  // SWIGCSHARP

// Wrap cp includes
%include constraint_solver/routing.h
