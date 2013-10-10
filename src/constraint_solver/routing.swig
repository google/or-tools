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
#endif
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
#endif
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
#endif

// On input, create std::vector<RoutingModel::NodeIndex> from int arrays.
#ifdef SWIGPYTHON
%typemap(in) const std::vector<operations_research::RoutingModel::NodeIndex>&
    (std::vector<operations_research::RoutingModel::NodeIndex> temp) {
  const int size = PyList_Size($input);
  temp.clear();
  temp.reserve(size);
  if (PyList_Check($input)) {
    for (int i = 0; i < size; ++i) {
      PyObject* const o = PyList_GetItem($input, i);
      const operations_research::RoutingModel::NodeIndex nd(PyInt_AsLong(o));
      temp.push_back(nd);
    }
    $1 = &temp;
  } else {
    PyErr_SetString(PyExc_TypeError, "not a list");
    return nullptr;
  }
}

%typecheck(SWIG_TYPECHECK_INT64_ARRAY)
const std::vector<operations_research::RoutingModel::NodeIndex>& {
  if (!PyTuple_Check($input) && !PyList_Check($input)) {
    $1 = 0;
  } else {
    const bool is_tuple = PyTuple_Check($input);
    const int size = is_tuple ? PyTuple_Size($input) : PyList_Size($input);
    bool failed = false;
    for (size_t i = 0; i < size; ++i) {
      PyObject* const obj =
          is_tuple ? PyTuple_GetItem($input, i) : PyList_GetItem($input, i);
      if (!PyInt_Check(obj) && !PyLong_Check(obj)) {
        failed = true;
        break;
      }
    }
    $1 = failed ? 0 : 1;
  }
}
#endif
#ifdef SWIGJAVA
%typemap(jni) const std::vector<operations_research::RoutingModel::NodeIndex>& "jobjectArray"
%typemap(jtype) const std::vector<operations_research::RoutingModel::NodeIndex>& "int[]"
%typemap(jstype) const std::vector<operations_research::RoutingModel::NodeIndex>& "int[]"
%typemap(javain) const std::vector<operations_research::RoutingModel::NodeIndex>& "$javainput"

%typemap(in) const std::vector<operations_research::RoutingModel::NodeIndex>&
    (std::vector<operations_research::RoutingModel::NodeIndex> temp) {
  if ($input) {
    const int size = jenv->GetArrayLength($input);
    temp.clear();
    temp.reserve(size);
    jint* values = jenv->GetIntArrayElements((jintArray)$input, nullptr);
    for (int i = 0; i < size; ++i) {
      const int value = values[i];
      temp.push_back(operations_research::RoutingModel::NodeIndex(value));
    }
    jenv->ReleaseIntArrayElements((jintArray)$input, values, 0);
    $1 = &temp;
  } else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null table");
    return $null;
  }
}

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
#endif    // defined(SWIGJAVA)
#ifdef SWIGCSHARP
%typemap(ctype)    const std::vector<operations_research::RoutingModel::NodeIndex>&  %{ int length$argnum, int* %}
%typemap(imtype)  const std::vector<operations_research::RoutingModel::NodeIndex>&  %{ int length$argnum, int[] %}
%typemap(cstype)  const std::vector<operations_research::RoutingModel::NodeIndex>&  %{ int[] %}
%typemap(csin)    const std::vector<operations_research::RoutingModel::NodeIndex>&  "$csinput.Length, $csinput"
%typemap(freearg)  const std::vector<operations_research::RoutingModel::NodeIndex>&  { delete $1; }
%typemap(in)    const std::vector<operations_research::RoutingModel::NodeIndex>&  %{
  $1 = new std::vector<operations_research::RoutingModel::NodeIndex>;
  $1->reserve(length$argnum);
  for(int i = 0; i < length$argnum; ++i)
    $1->push_back(operations_research::RoutingModel::NodeIndex($input[i]));
  %}

%typemap(ctype)    const std::vector<std::vector<operations_research::RoutingModel::NodeIndex> >&  %{ int len$argnum_1, int len$argnum_2, int* %}
%typemap(imtype)  const std::vector<std::vector<operations_research::RoutingModel::NodeIndex> >&  %{ int len$argnum_1, int len$argnum_2, int[] %}
%typemap(cstype)  const std::vector<std::vector<operations_research::RoutingModel::NodeIndex> >&  %{ int[,] %}
%typemap(csin)    const std::vector<std::vector<operations_research::RoutingModel::NodeIndex> >&  "$csinput.GetLength(0), $csinput.GetLength(1), NestedArrayHelper.GetFlatArray($csinput)"
%typemap(in)    const std::vector<std::vector<operations_research::RoutingModel::NodeIndex> >&  (std::vector<std::vector<operations_research::RoutingModel::NodeIndex> > result) %{

  const int size_x = len$argnum_1;
  const int size_y = len$argnum_2;

  result.clear();
  result.resize(size_x);

  for (int index1 = 0; index1 < size_x; ++index1) {
    result[index1].reserve(size_y);
    for (int index2 = 0; index2 < size_y; ++index2) {
      const operations_research::RoutingModel::NodeIndex val = operations_research::RoutingModel::NodeIndex($input[index1 * size_y + index2]);
      result[index1].push_back(val);
    }
  }

  $1 = &result;
%}

#endif    // defined(SWIGCSHARP)

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

class NodeEvaluator2 {
 public:
  virtual ~NodeEvaluator2();
  virtual int64 run(int i, int j) = 0;
  operations_research::RoutingModel::NodeEvaluator2* getPermanentCallback();
};

%typemap(jstype) operations_research::RoutingModel::NodeEvaluator2* "NodeEvaluator2";
%typemap(javain) operations_research::RoutingModel::NodeEvaluator2* "$descriptor(ResultCallback2<int64, _RoutingModel_NodeIndex, _RoutingModel_NodeIndex>*).getCPtr($javainput.getPermanentCallback())";
#endif  // SWIGJAVA
#ifdef SWIGCSHARP
%module(directors="1") main
%feature("director") NodeEvaluator2;
%{
class NodeEvaluator2 {
 public:
  int64 RunAux(operations_research::RoutingModel::NodeIndex i,
               operations_research::RoutingModel::NodeIndex j) {
    return Run(i.value(), j.value());
  }
  virtual int64 Run(int i, int j) = 0;
  operations_research::RoutingModel::NodeEvaluator2* GetPermanentCallback() {
    return NewPermanentCallback(this, &NodeEvaluator2::RunAux);
  }
  virtual ~NodeEvaluator2() {}
};
%}

class NodeEvaluator2 {
 public:
  virtual ~NodeEvaluator2();
  virtual int64 Run(int i, int j) = 0;
  operations_research::RoutingModel::NodeEvaluator2* GetPermanentCallback();
};

%typemap(cstype) operations_research::RoutingModel::NodeEvaluator2* "NodeEvaluator2";
%typemap(csin) operations_research::RoutingModel::NodeEvaluator2* "$descriptor(ResultCallback2<int64, _RoutingModel_NodeIndex, _RoutingModel_NodeIndex>*).getCPtr($csinput.GetPermanentCallback())";
#endif  // SWIGCSHARP
%ignore operations_research::RoutingModel::AddVectorDimension(
    const int64* values,
    int64 capacity,
    const string& name);

%ignore operations_research::RoutingModel::AddMatrixDimension(
    const int64* const* values,
    int64 capacity,
    const string& name);

%extend operations_research::RoutingModel {
  void AddVectorDimension(const std::vector<int64>& values,
                          int64 capacity,
                          bool fix_start_cumul_to_zero,
                          const string& name) {
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
%rename (solve) Solve;
%rename (addDimension) AddDimension;
%rename (addDimensionWithVehicleCapacity) AddDimensionWithVehicleCapacity;
%rename (addConstantDimension) AddConstantDimension;
%rename (addVectorDimension) AddVectorDimension;
%rename (addAllActive) AddAllActive;
%rename (addDisjunction) AddDisjunction;
%rename (addLocalSearchOperator) AddLocalSearchOperator;
%rename (setCost) SetCost;
%rename (setVehicleCost) SetVehicleCost;
%rename (setDepot) SetDepot;
%rename (addSearchMonitor) AddSearchMonitor;
%rename (applyLocks) ApplyLocks;
%rename (writeAssignment) WriteAssignment;
%rename (readAssignment) ReadAssignment;
%rename (start) Start;
%rename (end) End;
%rename (isStart) IsStart;
%rename (isEnd) IsEnd;
%rename (getCost) GetCost;
%rename (getFirstSolutionCost) GetFirstSolutionCost;
%rename (nexts) Nexts;
%rename (nextVar) NextVar;
%rename (vehicleVar) VehicleVar;
%rename (activeVar) ActiveVar;
%rename (cumulVar) CumulVar;
%rename (transitVar) TransitVar;
%rename (slackVar) SlackVar;
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
%rename (setCommandLineOption) SetCommandLineOption;
%rename (routesToAssignment) RoutesToAssignment;
%rename (closeModel) CloseModel;
%rename (setDimensionTransitCost) SetDimensionTransitCost;
%rename (getDimensionTransitCost) GetDimensionTransitCost;
%rename (setDimensionSpanCost) SetDimensionSpanCost;
%rename (getDimensionSpanCost) GetDimensionSpanCost;
%rename (setCumulVarSoftUpperBound) SetCumulVarSoftUpperBound;
%rename (getCumulVarSoftUpperBound) GetCumulVarSoftUpperBound;
%rename (getCumulVarSoftUpperBoundCoefficient) GetCumulVarSoftUpperBoundCoefficient;
#endif // SWIGJAVA

#ifdef SWIGCSHARP
%rename("%(camelcase)s", %$isfunction) "";
#endif  // SWIGCSHARP

// Wrap cp includes
%include constraint_solver/routing.h
