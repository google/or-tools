// Copyright 2010-2011 Google
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

namespace operations_research {

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

// On input, create std::vector<RoutingModel::NodeIndex> from int arrays.
#ifdef SWIGPYTHON
%typemap(in) const std::vector<RoutingModel::NodeIndex>&
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
    return NULL;
  }
}
#endif
#ifdef SWIGJAVA
%typemap(jni) const std::vector<RoutingModel::NodeIndex>& "jobjectArray"
%typemap(jtype) const std::vector<RoutingModel::NodeIndex>& "int[]"
%typemap(jstype) const std::vector<RoutingModel::NodeIndex>& "int[]"
%typemap(javain) const std::vector<RoutingModel::NodeIndex>& "$javainput"

%typemap(in) const std::vector<RoutingModel::NodeIndex>&
    (std::vector<operations_research::RoutingModel::NodeIndex> temp) {
  if ($input) {
    const int size = jenv->GetArrayLength($input);
    temp.clear();
    temp.reserve(size);
    jint* values = jenv->GetIntArrayElements((jintArray)$input, NULL);
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
#endif    // defined(SWIGJAVA)

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
%typemap(in) RoutingModel::NodeEvaluator2* {
  if (!PyCallable_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "Need a callable object!");
    SWIG_fail;
  }
  $1 = NewPermanentCallback(&PyCallback2NodeIndexNodeIndex, $input);
}
#endif
#ifdef SWIGJAVA
%{
class NodeIndexResultCallback2
  : public ResultCallback2<int64,
                           operations_research::RoutingModel::NodeIndex,
                           operations_research::RoutingModel::NodeIndex> {
 public:
  NodeIndexResultCallback2(ResultCallback2<int64, int64, int64>* callback)
      : callback_(callback) {
    CHECK_NOTNULL(callback_);
  }
  virtual ~NodeIndexResultCallback2() {}
  virtual bool IsRepeatable() const { return true; }
  virtual int64 Run(operations_research::RoutingModel::NodeIndex i,
                    operations_research::RoutingModel::NodeIndex j) {
    return callback_->Run(i.value(), j.value());
  }
  scoped_ptr<ResultCallback2<int64, int64, int64> > callback_;
};
%}

%typemap(jstype) RoutingModel::NodeEvaluator2* "LongResultCallback2";
%typemap(javain) RoutingModel::NodeEvaluator2* "SWIGTYPE_p_ResultCallback2Tlong_long_long_long_long_long_t.getCPtr($javainput.GetPermanentCallback())";
%typemap(in) RoutingModel::NodeEvaluator2* {
  if ($input) {
    ResultCallback2<int64, int64, int64>* const callback =
        (ResultCallback2<int64, int64, int64>*)$input;
    $1 = new NodeIndexResultCallback2(callback);
  } else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException,
                            "null evaluator");
    return $null;
  }
}
#endif

%ignore RoutingModel::AddVectorDimension(const int64* values,
                                         int64 capacity,
                                         const string& name);

%ignore RoutingModel::AddMatrixDimension(const int64* const* values,
                                         int64 capacity,
                                         const string& name);
%ignore RoutingModel::AddNoCycleConstraint();

%extend RoutingModel {
  void AddVectorDimension(const std::vector<int64>& values,
                          int64 capacity,
                          const string& name) {
    DCHECK_EQ(values.size(), self->nodes());
    self->AddVectorDimension(values.data(), capacity, name);
  }
}

#ifdef SWIGPYTHON
// Disambiguate AddDisjunction.
%ignore RoutingModel::AddDisjunction(const std::vector<NodeIndex>& nodes,
                                     int64 penalty);
#endif

%ignore RoutingModel::WrapIndexEvaluator(Solver::IndexEvaluator2* evaluator);

%ignore RoutingModel::RoutingModel(int nodes, int vehicles,
                                   const std::vector<pair<NodeIndex,
                                                     NodeIndex> >& start_end);
}  // namespace operations_research

#ifdef SWIGJAVA
%rename (solve) Solve;
%rename (addDimension) AddDimension;
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
%rename (activeVar) ActiveVar;
%rename (cumulVar) CumulVar;
%rename (transitVar) TransitVar;
%rename (addToAssignment) AddToAssignment;
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
#endif // SWIGJAVA

// Wrap cp includes
%include constraint_solver/routing.h
