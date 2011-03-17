// Copyright 2010 Google
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

%include exception.i
%include util/data.swig

// Include the file we want to wrap a first time.
#ifdef SWIGPYTHON
%{
#include <setjmp.h>

#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"

struct FailureProtect {
  jmp_buf exception_buffer;
  void JumpBack() {
    longjmp(exception_buffer, 1);
  }
};

namespace operations_research {
class CallPyDecisionBuilder : public DecisionBuilder {
 public:
  CallPyDecisionBuilder(PyObject* pydb) : pysolver_(NULL), pyarg_(NULL) {
    Py_INCREF(pydb);
    pydb_ = pydb;
    func_ = PyObject_GetAttrString(pydb_, "NextWrapper");
    Py_XINCREF(func_);
    str_func_ = PyObject_GetAttrString(pydb_, "DebugString");
    Py_XINCREF(str_func_);
  }

  virtual ~CallPyDecisionBuilder() {
    Py_DECREF(pydb_);
    pydb_ = NULL;
    Py_XDECREF(func_);
    func_ = NULL;
    Py_XDECREF(str_func_);
    str_func_ = NULL;
    Py_XDECREF(pysolver_);
    pysolver_ = NULL;
    Py_XDECREF(pyarg_);
    pyarg_ = NULL;
  }

  virtual Decision* Next(Solver* const s) {
    if (pysolver_ == NULL) {
      pysolver_ = SWIG_NewPointerObj(s,
                                     SWIGTYPE_p_operations_research__Solver,
                                     SWIG_POINTER_EXCEPTION);
      pyarg_ = Py_BuildValue((char*)"(O)", pysolver_);
    }
    Decision* result = NULL;
    PyObject* pyresult = PyEval_CallObject(func_, pyarg_);
    if (pyresult) {
      if (SWIG_ConvertPtr(pyresult,
                          (void **) &result,
                          SWIGTYPE_p_operations_research__Decision,
                          SWIG_POINTER_EXCEPTION | 0) == -1) {
        LOG(INFO) << "Error in type from python Decision";
      }
      Py_DECREF(pyresult);
    } else {  // something went wrong, we fail.
      s->Fail();
    }
    return result;
  }

  virtual string DebugString() const {
    string result = "PyDecisionBuilder";
    if (str_func_) {
      PyObject* pyresult = PyEval_CallObject(str_func_, NULL);
      if (pyresult) {
        result = PyString_AsString(pyresult);
        Py_DECREF(pyresult);
      }
    }
    return result;
  }
 private:
  PyObject* pysolver_;
  PyObject* pyarg_;
  PyObject* pydb_;
  PyObject* func_;
  PyObject* str_func_;
};

class PyLNSNoValues : public BaseLNS {
 public:
  PyLNSNoValues(const vector<IntVar*>& vars, PyObject* op)
      : BaseLNS(vars.data(), vars.size()), op_(op) {
    Py_INCREF(op_);
    init_func_ = PyObject_GetAttrString(op_, "InitFragment");
    Py_XINCREF(init_func_);
    fragment_func_ = PyObject_GetAttrString(op_, "NextFragment");
    Py_XINCREF(fragment_func_);
  }
  virtual ~PyLNSNoValues() {
    Py_DECREF(op_);
    op_ = NULL;
    Py_XDECREF(init_func_);
    init_func_ = NULL;
    Py_XDECREF(fragment_func_);
    fragment_func_ = NULL;
  }

  virtual void InitFragments() {
    if (init_func_) {
      PyObject* pyresult = PyEval_CallObject(init_func_, NULL);
      Py_XDECREF(pyresult);
    }
  }

  virtual bool NextFragment(vector<int>* fragment) {
    PyObject* list = PyList_New(0);
    PyObject* args = Py_BuildValue((char*)"(O)", list);
    PyObject* pyresult = PyEval_CallObject(fragment_func_, args);
    Py_DECREF(args);
    const int size = PyList_Size(list);
    for (size_t i = 0; i < size; ++i) {
      const int val = PyInt_AsLong(PyList_GetItem(list, i));
      fragment->push_back(val);
    }
    Py_DECREF(list);
    bool result = false;
    if (pyresult) {
      result = PyInt_AsLong(pyresult);
      Py_DECREF(pyresult);
    }
    return result;
  }

  virtual string DebugString() const {
    return "PyLNSNoValues()";
  }
 private:
  PyObject* op_;
  PyObject* init_func_;
  PyObject* fragment_func_;
};

class PyLNS : public BaseLNS {
 public:
  PyLNS(const vector<IntVar*>& vars, PyObject* op)
      : BaseLNS(vars.data(), vars.size()), op_(op) {
    Py_INCREF(op_);
    init_func_ = PyObject_GetAttrString(op_, "InitFragment");
    Py_XINCREF(init_func_);
    fragment_func_ = PyObject_GetAttrString(op_, "NextFragment");
    Py_XINCREF(fragment_func_);
    base_lns_ = SWIG_NewPointerObj(this,
                                   SWIGTYPE_p_operations_research__BaseLNS,
                                   SWIG_POINTER_EXCEPTION);
    Py_INCREF(base_lns_);
  }
  virtual ~PyLNS() {
    Py_DECREF(op_);
    op_ = NULL;
    Py_XDECREF(init_func_);
    init_func_ = NULL;
    Py_XDECREF(fragment_func_);
    fragment_func_ = NULL;
    Py_DECREF(base_lns_);
    base_lns_ = NULL;
  }

  virtual void InitFragments() {
    if (init_func_) {
      PyObject* pyresult = PyEval_CallObject(init_func_, NULL);
      Py_XDECREF(pyresult);
    }
  }

  virtual bool NextFragment(vector<int>* fragment) {
    PyObject* list = PyList_New(0);
    PyObject* args = Py_BuildValue((char*)"(OO)", list, base_lns_);
    PyObject* pyresult = PyEval_CallObject(fragment_func_, args);
    Py_DECREF(args);
    const int size = PyList_Size(list);
    for (size_t i = 0; i < size; ++i) {
      const int val = PyInt_AsLong(PyList_GetItem(list, i));
      fragment->push_back(val);
    }
    Py_DECREF(list);
    bool result = false;
    if (pyresult) {
      result = PyInt_AsLong(pyresult);
      Py_DECREF(pyresult);
    }
    return result;
  }

  virtual string DebugString() const {
    return "PyLNS()";
  }
 private:
  PyObject* op_;
  PyObject* init_func_;
  PyObject* fragment_func_;
  PyObject* base_lns_;
};

}  // namespace operations_research
%}

namespace operations_research {
%pythoncode {
class PyDecisionBuilder(object):
  def NextWrapper(self, solver):
    result = None
    try:
      result = self.Next(solver)
    except Exception:
      return solver.FailDecision()
    return result
}
}  // namespace operations_research

// Define the renaming of methods.
%rename (Add) AddConstraint;
%rename (AllDifferent) MakeAllDifferent;
%rename (AllSolutionCollector) MakeAllSolutionCollector;
%rename (AllowedAssignments) MakeAllowedAssignments;
%rename (AssignVariableValue) MakeAssignVariableValue;
%rename (AssignVariableValueOrFail) MakeAssignVariableValueOrFail;
%rename (AssignVariablesValues) MakeAssignVariablesValues;
%rename (Assignment) MakeAssignment;
%rename (BestValueSolutionCollector) MakeBestValueSolutionCollector;
%rename (BetweenCt) MakeBetweenCt;
%rename (BoolVar) MakeBoolVar;
%rename (ConstantRestart) MakeConstantRestart;
%rename (ConvexPiecewiseExpr) MakeConvexPiecewiseExpr;
%rename (Count) MakeCount;
%rename (Cumulative) MakeCumulative;
%rename (CustomLimit) MakeCustomLimit;
%rename (DecisionBuilderFromAssignment) MakeDecisionBuilderFromAssignment;
%rename (DefaultPhase) MakeDefaultPhase;
%rename (Distribute) MakeDistribute;
%rename (Element) MakeElement;
%rename (FailDecision) MakeFailDecision;
%rename (FalseConstraint) MakeFalseConstraint;
%rename (FirstSolutionCollector) MakeFirstSolutionCollector;
%rename (FixedDurationIntervalVar) MakeFixedDurationIntervalVar;
%rename (FixedInterval) MakeFixedInterval;
%rename (IntConst) MakeIntConst;
%rename (IntVar) MakeIntVar;
%rename (IntervalVarRelation) MakeIntervalVarRelation;
%rename (IsBetweenCt) MakeIsBetweenCt;
%rename (IsDifferentCstCt) MakeIsDifferentCstCt;
%rename (IsDifferentCstVar) MakeIsDifferentCstVar;
%rename (IsDifferentCt) MakeIsDifferentCt;
%rename (IsDifferentVar) MakeIsDifferentVar;
%rename (IsEqualCstCt) MakeIsEqualCstCt;
%rename (IsEqualCstVar) MakeIsEqualCstVar;
%rename (IsEqualCt) MakeIsEqualCt;
%rename (IsEqualVar) MakeIsEqualVar;
%rename (IsGreaterCstCt) MakeIsGreaterCstCt;
%rename (IsGreaterCstVar) MakeIsGreaterCstVar;
%rename (IsGreaterOrEqualCstCt) MakeIsGreaterOrEqualCstCt;
%rename (IsGreaterOrEqualCstVar) MakeIsGreaterOrEqualCstVar;
%rename (IsLessCstCt) MakeIsLessCstCt;
%rename (IsLessCstVar) MakeIsLessCstVar;
%rename (IsLessOrEqualCstCt) MakeIsLessOrEqualCstCt;
%rename (IsLessOrEqualCstVar) MakeIsLessOrEqualCstVar;
%rename (IsMemberCt) MakeIsMemberCt;
%rename (IsMemberVar) MakeIsMemberVar;
%rename (LastSolutionCollector) MakeLastSolutionCollector;
%rename (Limit) MakeLimit;
// Do not define MakeLocalSearchObjectiveFilter
%rename (LocalSearchPhase) MakeLocalSearchPhase;
%rename (LocalSearchPhaseParameters) MakeLocalSearchPhaseParameters;
%rename (LubyRestart) MakeLubyRestart;
%rename (MapDomain) MakeMapDomain;
%rename (Max) MakeMax;
%rename (Maximize) MakeMaximize;
%rename (MemberCt) MakeMemberCt;
%rename (Min) MakeMin;
%rename (Minimize) MakeMinimize;
%rename (NeighborhoodLimit) MakeNeighborhoodLimit;
%rename (NoCycle) MakeNoCycle;
%rename (Operator) MakeOperator;
%rename (Optimize) MakeOptimize;
%rename (Pack) MakePack;
%rename (PathCumul) MakePathCumul;
%rename (Phase) MakePhase;
%rename (RandomLNSOperator) MakeRandomLNSOperator;
%rename (RestoreAssignment) MakeRestoreAssignment;
%rename (ScalProd) MakeScalProd;
%rename (ScalProdEquality) MakeScalProdEquality;
%rename (ScalProdGreaterOrEqual) MakeScalProdGreaterOrEqual;
%rename (ScalProdLessOrEqual) MakeScalProdLessOrEqual;
%rename (ScheduleOrPostpone) MakeScheduleOrPostpone;
%rename (SearchLog) MakeSearchLog;
%rename (SearchTrace) MakeSearchTrace;
%rename (SemiContinuousExpr) MakeSemiContinuousExpr;
%rename (Sequence) MakeSequence;
%rename (SimulatedAnnealing) MakeSimulatedAnnealing;
%rename (SolveOnce) MakeSolveOnce;
%rename (SplitVariableDomain) MakeSplitVariableDomain;
%rename (StoreAssignment) MakeStoreAssignment;
%rename (Sum) MakeSum;
%rename (SumEquality) MakeSumEquality;
%rename (SumGreaterOrEqual) MakeSumGreaterOrEqual;
%rename (SumLessOrEqual) MakeSumLessOrEqual;
%rename (SymmetryManager) MakeSymmetryManager;
%rename (TabuSearch) MakeTabuSearch;
%rename (TemporalDisjunction) MakeTemporalDisjunction;
%rename (TransitionConstraint) MakeTransitionConstraint;
%rename (TreeMonitor) MakeTreeMonitor;
%rename (TrueConstraint) MakeTrueConstraint;
%rename (TryRankFirst) MakeTryRankFirst;
%rename (WeightedMaximize) MakeWeightedMaximize;
%rename (WeightedMinimize) MakeWeightedMinimize;
%rename (WeightedOptimize) MakeWeightedOptimize;


namespace operations_research {
%typemap(in) IntVar* const {
  operations_research::IntExpr* t;
  if (SWIG_ConvertPtr($input,
                      (void**)&t,
                      SWIGTYPE_p_operations_research__IntExpr,
                      SWIG_POINTER_EXCEPTION | 0 ) == -1) {
    SWIG_fail;
  }
  $1 = t->Var();
}

%typecheck(SWIG_TYPECHECK_POINTER) IntVar* const {
  operations_research::IntExpr* t;
  $1 = SWIG_ConvertPtr($input,
                       (void**)&t,
                       SWIGTYPE_p_operations_research__IntExpr,
                       SWIG_POINTER_EXCEPTION | 0 ) != -1;
  PyErr_Clear();
}

%typemap(in) const IntVar* const {
  operations_research::IntExpr* t;
  if (SWIG_ConvertPtr($input,
                      (void**)&t,
                      SWIGTYPE_p_operations_research__IntExpr,
                      SWIG_POINTER_EXCEPTION | 0 ) == -1) {
    SWIG_fail;
  }
  $1 = t->Var();
}

%typecheck(SWIG_TYPECHECK_POINTER) const IntVar* const {
  operations_research::IntExpr* t;
  $1 = SWIG_ConvertPtr($input,
                       (void**)&t,
                       SWIGTYPE_p_operations_research__IntExpr,
                       SWIG_POINTER_EXCEPTION | 0 ) != -1;
  if ($1 == 0) {
    PyErr_Clear();
  }
}

// Add conversion rules for vector<IntExpr*> and vector<IntVar*>. Please
// note that list of expressions will be converted to vectors of
// IntVar.
%typemap(in) const vector<IntVar*>& (vector<IntVar*> temp) {
  if (!PyTuple_Check($input) && !PyList_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "Expecting a sequence");
    SWIG_fail;
  }
  bool is_tuple = PyTuple_Check($input);
  temp.resize(is_tuple ? PyTuple_Size($input) : PyList_Size($input));
  for (size_t i = 0; i < temp.size(); ++i) {
    PyObject *obj = (is_tuple ? PyTuple_GetItem($input, i) :
                     PyList_GetItem($input, i));
    operations_research::IntExpr* t;
    if ((SWIG_ConvertPtr(obj, (void**)&t,
                         SWIGTYPE_p_operations_research__IntExpr,
                         SWIG_POINTER_EXCEPTION | 0 )) == -1) {
      SWIG_fail;
    }
    if (t == NULL) {
      PyErr_SetString(PyExc_TypeError, "Invalid LanguageCode");
      SWIG_fail;
    }
    temp[i] = t->Var();
  }
  $1 = &temp;
}

%typecheck(SWIG_TYPECHECK_POINTER) const vector<IntVar*>& {
  if (!PyTuple_Check($input) && !PyList_Check($input)) {
    $1 = 0;
  } else {
    const bool is_tuple = PyTuple_Check($input);
    const int size = is_tuple ? PyTuple_Size($input) : PyList_Size($input);
    bool failed = false;
    for (size_t i = 0; i < size; ++i) {
      PyObject* const obj =
          is_tuple ? PyTuple_GetItem($input, i) : PyList_GetItem($input, i);
      operations_research::IntExpr* t;
      if ((SWIG_ConvertPtr(obj, (void**)&t,
                           SWIGTYPE_p_operations_research__IntExpr,
                           SWIG_POINTER_EXCEPTION | 0 )) == -1) {
        PyErr_Clear();
        failed = true;
        break;
      }
    }
    $1 = failed ? 0 : 1;
  }
}

// Add conversion rules for vector<SearchMonitor*>.
%typemap(in) const vector<SearchMonitor*>&
    (vector<SearchMonitor*> temp) {
  if (!PyTuple_Check($input) && !PyList_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "Expecting a sequence");
    SWIG_fail;
  }
  bool is_tuple = PyTuple_Check($input);
  temp.resize(is_tuple ? PyTuple_Size($input) : PyList_Size($input));
  for (size_t i = 0; i < temp.size(); ++i) {
    PyObject *obj = (is_tuple ? PyTuple_GetItem($input, i) :
                     PyList_GetItem($input, i));
    operations_research::SearchMonitor* t;
    if ((SWIG_ConvertPtr(obj, (void**)&t,
                         SWIGTYPE_p_operations_research__SearchMonitor,
                         SWIG_POINTER_EXCEPTION | 0 )) == -1) {
      SWIG_fail;
    }
    if (t == NULL) {
      PyErr_SetString(PyExc_TypeError, "Invalid LanguageCode");
      SWIG_fail;
    }
    temp[i] = t;
  }
  $1 = &temp;
}

%typecheck(SWIG_TYPECHECK_POINTER) const vector<SearchMonitor*>& {
  if (!PyTuple_Check($input) && !PyList_Check($input)) {
    $1 = 0;
  } else {
    const bool is_tuple = PyTuple_Check($input);
    const int size = is_tuple ? PyTuple_Size($input) : PyList_Size($input);
    bool failed = false;
    for (size_t i = 0; i < size; ++i) {
      PyObject* const obj =
          is_tuple ? PyTuple_GetItem($input, i) : PyList_GetItem($input, i);
      operations_research::SearchMonitor* t;
      if ((SWIG_ConvertPtr(obj, (void**)&t,
                           SWIGTYPE_p_operations_research__SearchMonitor,
                           SWIG_POINTER_EXCEPTION | 0 )) == -1) {
        PyErr_Clear();
        failed = true;
        break;
      }
    }
    $1 = failed ? 0 : 1;
  }
}

// Add conversion rules for vector<DecisionBuilder*>.
%typemap(in) const vector<DecisionBuilder*>&
    (vector<DecisionBuilder*> temp) {
  if (!PyTuple_Check($input) && !PyList_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "Expecting a sequence");
    SWIG_fail;
  }
  bool is_tuple = PyTuple_Check($input);
  temp.resize(is_tuple ? PyTuple_Size($input) : PyList_Size($input));
  for (size_t i = 0; i < temp.size(); ++i) {
    PyObject *obj = (is_tuple ? PyTuple_GetItem($input, i) :
                     PyList_GetItem($input, i));
    operations_research::DecisionBuilder* t;
    if ((SWIG_ConvertPtr(obj, (void**)&t,
                         SWIGTYPE_p_operations_research__DecisionBuilder,
                         SWIG_POINTER_EXCEPTION | 0 )) == -1) {
      PyErr_Clear();
      t = new operations_research::CallPyDecisionBuilder(obj);
    }
    if (t == NULL) {
      PyErr_SetString(PyExc_TypeError, "Invalid LanguageCode");
      SWIG_fail;
    }
    temp[i] = t;
  }
  $1 = &temp;
}

%typecheck(SWIG_TYPECHECK_POINTER) const vector<DecisionBuilder*>& {
  if (!PyTuple_Check($input) && !PyList_Check($input)) {
    $1 = 0;
  } else {
    const bool is_tuple = PyTuple_Check($input);
    const int size = is_tuple ? PyTuple_Size($input) : PyList_Size($input);
    bool failed = false;
    for (size_t i = 0; i < size; ++i) {
      PyObject* const obj =
          is_tuple ? PyTuple_GetItem($input, i) : PyList_GetItem($input, i);
      operations_research::DecisionBuilder* t;
      if ((SWIG_ConvertPtr(obj, (void**)&t,
                           SWIGTYPE_p_operations_research__DecisionBuilder,
                           SWIG_POINTER_EXCEPTION | 0 )) == -1) {
        PyErr_Clear();
        PyObject* next_func = PyObject_GetAttrString(obj, "Next");
        failed = next_func == NULL;
        if (failed) {
          break;
        }
      }
    }
    $1 = failed ? 0 : 1;
  }
}

// Add conversion rules for vector<IntervalVar*>.
%typemap(in) const vector<IntervalVar*>&
    (vector<IntervalVar*> temp) {
  if (!PyTuple_Check($input) && !PyList_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "Expecting a sequence");
    SWIG_fail;
  }
  bool is_tuple = PyTuple_Check($input);
  temp.resize(is_tuple ? PyTuple_Size($input) : PyList_Size($input));
  for (size_t i = 0; i < temp.size(); ++i) {
    PyObject *obj = (is_tuple ? PyTuple_GetItem($input, i) :
                     PyList_GetItem($input, i));
    operations_research::IntervalVar* t;
    if ((SWIG_ConvertPtr(obj, (void**)&t,
                         SWIGTYPE_p_operations_research__IntervalVar,
                         SWIG_POINTER_EXCEPTION | 0 )) == -1) {
      SWIG_fail;
    }
    if (t == NULL) {
      PyErr_SetString(PyExc_TypeError, "Invalid LanguageCode");
      SWIG_fail;
    }
    temp[i] = t;
  }
  $1 = &temp;
}

%typecheck(SWIG_TYPECHECK_POINTER) const vector<IntervalVar*>& {
  if (!PyTuple_Check($input) && !PyList_Check($input)) {
    $1 = 0;
  } else {
    const bool is_tuple = PyTuple_Check($input);
    const int size = is_tuple ? PyTuple_Size($input) : PyList_Size($input);
    bool failed = false;
    for (size_t i = 0; i < size; ++i) {
      PyObject* const obj =
          is_tuple ? PyTuple_GetItem($input, i) : PyList_GetItem($input, i);
      operations_research::IntervalVar* t;
      if ((SWIG_ConvertPtr(obj, (void**)&t,
                           SWIGTYPE_p_operations_research__IntervalVar,
                           SWIG_POINTER_EXCEPTION | 0 )) == -1) {
        PyErr_Clear();
        failed = true;
        break;
      }
    }
    $1 = failed ? 0 : 1;
  }
}

// Add conversion rules for vector<Sequence*>.
%typemap(in) const vector<Sequence*>&
    (vector<Sequence*> temp) {
  if (!PyTuple_Check($input) && !PyList_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "Expecting a sequence");
    SWIG_fail;
  }
  bool is_tuple = PyTuple_Check($input);
  temp.resize(is_tuple ? PyTuple_Size($input) : PyList_Size($input));
  for (size_t i = 0; i < temp.size(); ++i) {
    PyObject *obj = (is_tuple ? PyTuple_GetItem($input, i) :
                     PyList_GetItem($input, i));
    operations_research::Sequence* t;
    if ((SWIG_ConvertPtr(obj, (void**)&t,
                         SWIGTYPE_p_operations_research__Sequence,
                         SWIG_POINTER_EXCEPTION | 0 )) == -1) {
      SWIG_fail;
    }
    if (t == NULL) {
      PyErr_SetString(PyExc_TypeError, "Invalid LanguageCode");
      SWIG_fail;
    }
    temp[i] = t;
  }
  $1 = &temp;
}

%typecheck(SWIG_TYPECHECK_POINTER) const vector<Sequence*>& {
  if (!PyTuple_Check($input) && !PyList_Check($input)) {
    $1 = 0;
  } else {
    const bool is_tuple = PyTuple_Check($input);
    const int size = is_tuple ? PyTuple_Size($input) : PyList_Size($input);
    bool failed = false;
    for (size_t i = 0; i < size; ++i) {
      PyObject* const obj =
          is_tuple ? PyTuple_GetItem($input, i) : PyList_GetItem($input, i);
      operations_research::Sequence* t;
      if ((SWIG_ConvertPtr(obj, (void**)&t,
                           SWIGTYPE_p_operations_research__Sequence,
                           SWIG_POINTER_EXCEPTION | 0 )) == -1) {
        PyErr_Clear();
        failed = true;
        break;
      }
    }
    $1 = failed ? 0 : 1;
  }
}

// Add conversion rules for vector<LocalSearchOperator*>.
%typemap(in) const vector<LocalSearchOperator*>&
    (vector<LocalSearchOperator*> temp) {
  if (!PyTuple_Check($input) && !PyList_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "Expecting a sequence");
    SWIG_fail;
  }
  bool is_tuple = PyTuple_Check($input);
  temp.resize(is_tuple ? PyTuple_Size($input) : PyList_Size($input));
  for (size_t i = 0; i < temp.size(); ++i) {
    PyObject *obj = (is_tuple ? PyTuple_GetItem($input, i) :
                     PyList_GetItem($input, i));
    operations_research::LocalSearchOperator* t;
    if ((SWIG_ConvertPtr(obj, (void**)&t,
                         SWIGTYPE_p_operations_research__LocalSearchOperator,
                         SWIG_POINTER_EXCEPTION | 0 )) == -1) {
      SWIG_fail;
    }
    if (t == NULL) {
      PyErr_SetString(PyExc_TypeError, "Invalid LanguageCode");
      SWIG_fail;
    }
    temp[i] = t;
  }
  $1 = &temp;
}

%typecheck(SWIG_TYPECHECK_POINTER) const vector<LocalSearchOperator*>& {
  if (!PyTuple_Check($input) && !PyList_Check($input)) {
    $1 = 0;
  } else {
    const bool is_tuple = PyTuple_Check($input);
    const int size = is_tuple ? PyTuple_Size($input) : PyList_Size($input);
    bool failed = false;
    for (size_t i = 0; i < size; ++i) {
      PyObject* const obj =
          is_tuple ? PyTuple_GetItem($input, i) : PyList_GetItem($input, i);
      operations_research::LocalSearchOperator* t;
      if ((SWIG_ConvertPtr(obj, (void**)&t,
                           SWIGTYPE_p_operations_research__LocalSearchOperator,
                           SWIG_POINTER_EXCEPTION | 0 )) == -1) {
        PyErr_Clear();
        failed = true;
        break;
      }
    }
    $1 = failed ? 0 : 1;
  }
}

// Conversion for vector<LocalSearchFilter*>
%typemap(in) const vector<LocalSearchFilter*>&
    (vector<LocalSearchFilter*> temp) {
  if (!PyTuple_Check($input) && !PyList_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "Expecting a sequence");
    SWIG_fail;
  }
  bool is_tuple = PyTuple_Check($input);
  temp.resize(is_tuple ? PyTuple_Size($input) : PyList_Size($input));
  for (size_t i = 0; i < temp.size(); ++i) {
    PyObject *obj = (is_tuple ? PyTuple_GetItem($input, i) :
                     PyList_GetItem($input, i));
    operations_research::LocalSearchFilter* t;
    if ((SWIG_ConvertPtr(obj, (void**)&t,
                         SWIGTYPE_p_operations_research__LocalSearchFilter,
                         SWIG_POINTER_EXCEPTION | 0 )) == -1) {
      SWIG_fail;
    }
    if (t == NULL) {
      PyErr_SetString(PyExc_TypeError, "Invalid LanguageCode");
      SWIG_fail;
    }
    temp[i] = t;
  }
  $1 = &temp;
}

%typecheck(SWIG_TYPECHECK_POINTER) const vector<LocalSearchFilter*>& {
  if (!PyTuple_Check($input) && !PyList_Check($input)) {
    $1 = 0;
  } else {
    const bool is_tuple = PyTuple_Check($input);
    const int size = is_tuple ? PyTuple_Size($input) : PyList_Size($input);
    bool failed = false;
    for (size_t i = 0; i < size; ++i) {
      PyObject* const obj =
          is_tuple ? PyTuple_GetItem($input, i) : PyList_GetItem($input, i);
      operations_research::LocalSearchFilter* t;
      if ((SWIG_ConvertPtr(obj, (void**)&t,
                           SWIGTYPE_p_operations_research__LocalSearchFilter,
                           SWIG_POINTER_EXCEPTION | 0 )) == -1) {
        PyErr_Clear();
        failed = true;
        break;
      }
    }
    $1 = failed ? 0 : 1;
  }
}

%typemap(in) DecisionBuilder* const {
  operations_research::DecisionBuilder* tmp;
  if (SWIG_ConvertPtr($input,
                      (void **) &tmp,
                      SWIGTYPE_p_operations_research__DecisionBuilder,
                      SWIG_POINTER_EXCEPTION | 0) == -1) {
    PyErr_Clear();
    $1 = new operations_research::CallPyDecisionBuilder($input);
  } else {
    $1 = tmp;
  }
}

%typecheck(SWIG_TYPECHECK_POINTER) DecisionBuilder* const {
  operations_research::DecisionBuilder* tmp;
  if (SWIG_ConvertPtr($input,
                      (void **) &tmp,
                      SWIGTYPE_p_operations_research__DecisionBuilder,
                      SWIG_POINTER_EXCEPTION | 0) == -1) {
    PyErr_Clear();
    PyObject* next_func = PyObject_GetAttrString($input, "Next");
    $1 =  next_func != NULL;
  } else {
    $1 = 1;
  }
}

// Add display methods on BaseObject and remove DebugString method.
%ignore BaseObject::DebugString;
%extend BaseObject {
  string __str__() {
    return self->DebugString();
  }
  string __repr__() {
    return self->DebugString();
  }
}

%extend IntervalVar {
  string __repr__() {
    return self->DebugString();
  }
  Constraint* EndsAfterEnd(IntervalVar* other) {
    return self->solver()->MakeIntervalVarRelation(self, operations_research::Solver::ENDS_AFTER_END, other);
  }
  Constraint* EndsAfterStart(IntervalVar* other) {
    return self->solver()->MakeIntervalVarRelation(self, operations_research::Solver::ENDS_AFTER_START, other);
  }
  Constraint* EndsAtEnd(IntervalVar* other) {
    return self->solver()->MakeIntervalVarRelation(self, operations_research::Solver::ENDS_AT_END, other);
  }
  Constraint* EndsAtStart(IntervalVar* other) {
    return self->solver()->MakeIntervalVarRelation(self, operations_research::Solver::ENDS_AT_START, other);
  }
  Constraint* StartsAfterEnd(IntervalVar* other) {
    return self->solver()->MakeIntervalVarRelation(self, operations_research::Solver::STARTS_AFTER_END, other);
  }
  Constraint* StartsAfterStart(IntervalVar* other) {
    return self->solver()->MakeIntervalVarRelation(self,
                                                   operations_research::Solver::STARTS_AFTER_START,
                                                   other);
  }
  Constraint* StartsAtEnd(IntervalVar* other) {
    return self->solver()->MakeIntervalVarRelation(self, operations_research::Solver::STARTS_AT_END, other);
  }
  Constraint* StartsAtStart(IntervalVar* other) {
    return self->solver()->MakeIntervalVarRelation(self,
                                                   operations_research::Solver::STARTS_AT_START,
                                                   other);
  }
}

%extend Sequence {
  string __repr__() {
    return self->DebugString();
  }
}

%extend IntVar {
  string __repr__() {
    return self->DebugString();
  }
}

%extend IntExpr {
  string __repr__() {
    return self->DebugString();
  }
}

%extend Constraint {
  string __repr__() {
    return self->DebugString();
  }
}

%extend SearchMonitor {
  string __repr__() {
    return self->DebugString();
  }
}

%extend DecisionBuilder {
  string __repr__() {
    return self->DebugString();
  }
}

%extend Decision {
  string __repr__() {
    return self->DebugString();
  }
}

// Add display methods on Solver and remove DebugString method.
%ignore Solver::DebugString;

%extend Solver {
  Constraint* TreeNoCycle(const vector<IntVar*>& nexts,
                          const vector<IntVar*>& active,
                          ResultCallback1<bool, int64>* callback = NULL) {
    return self->MakeNoCycle(nexts, active, callback, false);
  }

  SearchMonitor* SearchLogWithCallback(int period,
                                       ResultCallback<string>* callback) {
    return self->MakeSearchLog(period, callback);
  }

  IntExpr* ElementFunction(ResultCallback1<int64, int64>* values,
                           IntVar* const index) {
    return self->MakeElement(values, index);
  }
  string __str__() {
    return self->DebugString();
  }

  LocalSearchOperator* LNSOperator(const vector<IntVar*>& vars,
                                   PyObject* lns) {
    return self->RevAlloc(new operations_research::PyLNS(vars, lns));
  }

  LocalSearchOperator* LNSNoValuesOperator(const vector<IntVar*>& vars,
                                           PyObject* lns) {
    return self->RevAlloc(new operations_research::PyLNSNoValues(vars, lns));
  }

  DecisionBuilder* VarEvalValStrPhase(
      const vector<IntVar*>& vars,
      ResultCallback1<int64, int64>* var_evaluator,
      operations_research::Solver::IntValueStrategy val_str) {
    return self->MakePhase(vars, var_evaluator, val_str);
  }

  DecisionBuilder* VarStrValEvalPhase(
      const vector<IntVar*>& vars,
      operations_research::Solver::IntVarStrategy var_str,
      ResultCallback2<int64, int64, int64>* val_eval) {
    return self->MakePhase(vars, var_str, val_eval);
  }

  DecisionBuilder* VarEvalValEvalPhase(
      const vector<IntVar*>& vars,
      ResultCallback1<int64, int64>* var_eval,
      ResultCallback2<int64, int64, int64>* val_eval) {
    return self->MakePhase(vars, var_eval, val_eval);
  }

  DecisionBuilder* VarStrValEvalTieBreakPhase(
      const vector<IntVar*>& vars,
      operations_research::Solver::IntVarStrategy var_str,
      ResultCallback2<int64, int64, int64>* val_eval,
      ResultCallback1<int64, int64>* tie_breaker) {
    return self->MakePhase(vars, var_str, val_eval, tie_breaker);
  }

  DecisionBuilder* VarEvalValEvalTieBreakPhase(
      const vector<IntVar*>& vars,
      ResultCallback1<int64, int64>* var_eval,
      ResultCallback2<int64, int64, int64>* val_eval,
      ResultCallback1<int64, int64>* tie_breaker) {
    return self->MakePhase(vars, var_eval, val_eval, tie_breaker);
  }

  DecisionBuilder* EvalEvalStrPhase(
      const vector<IntVar*>& vars,
      ResultCallback2<int64, int64, int64>* evaluator,
      operations_research::Solver::EvaluatorStrategy str) {
    return self->MakePhase(vars, evaluator, str);
  }

  DecisionBuilder* EvalEvalStrTieBreakPhase(
      const vector<IntVar*>& vars,
      ResultCallback2<int64, int64, int64>* evaluator,
      ResultCallback1<int64, int64>* tie_breaker,
      operations_research::Solver::EvaluatorStrategy str) {
    return self->MakePhase(vars, evaluator, tie_breaker, str);
  }

  SearchMonitor* GuidedLocalSearch(
      bool maximize,
      IntVar* const objective,
      ResultCallback2<int64, int64, int64>* objective_function,
      int64 step,
      const vector<IntVar*>& vars,
      double penalty_factor) {
    return self->MakeGuidedLocalSearch(maximize,
                                       objective,
                                       objective_function,
                                       step,
                                       vars,
                                       penalty_factor);
  }

  LocalSearchFilter* LocalSearchObjectiveFilter(
      const vector<IntVar*>& vars,
      ResultCallback2<int64, int64, int64>* values,
      const IntVar* const objective,
      Solver::LocalSearchFilterBound filter_enum,
      Solver::LocalSearchOperation op_enum) {
    return self->MakeLocalSearchObjectiveFilter(vars,
                                                values,
                                                objective,
                                                filter_enum,
                                                op_enum);
  }
}

// Ignore wrapping of some solver methods.
%ignore Solver::MakeIntVarArray;
%ignore Solver::MakeBoolVarArray;
%ignore Solver::MakeSum(IntExpr* const left, IntExpr* const right);
%ignore Solver::MakeSum(IntExpr* const expr, int64 value);
%ignore Solver::MakeSum(IntVar* const * vars, int size);
%ignore Solver::MakeDifference;
%ignore Solver::MakeOpposite;
%ignore Solver::MakeProd;
%ignore Solver::MakeSquare;
%ignore Solver::MakeElement(const int64* const vals,
                            int size,
                            IntVar* const index);
%ignore Solver::MakeElement(ResultCallback1<int64, int64>* values,
                            IntVar* const index);
%ignore Solver::MakeElement(const IntVar* const * vars, int size,
                            IntVar* const index);
%ignore Solver::MakeMin(IntExpr* const expr, int val);
%ignore Solver::MakeMax(IntExpr* const expr, int val);
%ignore Solver::MakeEquality;
%ignore Solver::MakeNonEquality;
%ignore Solver::MakeGreaterOrEqual;
%ignore Solver::MakeGreater;
%ignore Solver::MakeLessOrEqual;
%ignore Solver::MakeLess;
%ignore Solver::MakeAllDifferent(const IntVar* const* vars,
                                 int size,
                                 bool range);
%ignore Solver::MakeConstraintInitialPropagateCallback;
%ignore Solver::MakeDelayedConstraintInitialPropagateCallback;
%ignore Solver::MakePhase(const IntVar* const* vars,
                          int size,
                          PhaseStrategy str);
%ignore Solver::MakePhase(IntVar* const v0, PhaseStrategy str);
%ignore Solver::MakePhase(IntVar* const v0,
                          IntVar* const v1,
                          PhaseStrategy str);
%ignore Solver::MakePhase(IntVar* const v0,
                          IntVar* const v1,
                          IntVar* const v2,
                          PhaseStrategy str);
%ignore Solver::MakePhase(IntVar* const v0,
                          IntVar* const v1,
                          IntVar* const v3,
                          IntVar* const v4,
                          PhaseStrategy str);
%ignore Solver::Solve(DecisionBuilder* const db,
                      SearchMonitor* const * monitors, int size);
%ignore Solver::NestedSolve;
%ignore Solver::Compose(DecisionBuilder* const db1,
                        DecisionBuilder* const db2);
%ignore Solver::Compose(DecisionBuilder* const db1,
                        DecisionBuilder* const db2,
                        DecisionBuilder* const db3);
%ignore Solver::Compose(DecisionBuilder* const db1,
                        DecisionBuilder* const db2,
                        DecisionBuilder* const db3,
                        DecisionBuilder* const db4);
%ignore Solver::MakeCumulative(IntervalVar* const * intervals,
                               const int64 * demands,
                               int size,
                               int64 capacity,
                               const string& name);

// Add arithmetic operators to integer expressions.
%extend IntExpr {
  IntExpr* __add__(IntExpr* other) {
    return self->solver()->MakeSum(self, other);
  }
  IntExpr* __add__(int64 v) {
    return self->solver()->MakeSum(self, v);
  }
  IntExpr* __radd__(int64 v) {
    return self->solver()->MakeSum(self, v);
  }
  IntExpr* __sub__(IntExpr* other) {
    return self->solver()->MakeDifference(self, other);
  }
  IntExpr* __sub__(int64 v) {
    return self->solver()->MakeSum(self, -v);
  }
  IntExpr* __rsub__(int64 v) {
    return self->solver()->MakeDifference(v, self);
  }
  IntExpr* __mul__(IntExpr* other) {
    return self->solver()->MakeProd(self, other);
  }
  IntExpr* __mul__(int64 v) {
    return self->solver()->MakeProd(self, v);
  }
  IntExpr* __rmul__(int64 v) {
    return self->solver()->MakeProd(self, v);
  }
  IntExpr* __floordiv__(int64 v) {
    return self->solver()->MakeDiv(self, v);
  }

  IntExpr* __neg__() {
    return self->solver()->MakeOpposite(self);
  }
  IntExpr* __abs__() {
    return self->solver()->MakeAbs(self);
  }
  IntExpr* Square() {
    return self->solver()->MakeSquare(self);
  }

  Constraint* __eq__(int64 v) {
    return self->solver()->MakeEquality(self, v);
  }
  Constraint* __ne__(int64 v) {
    return self->solver()->MakeNonEquality(self->Var(), v);
  }
  Constraint* __ge__(int64 v) {
    return self->solver()->MakeGreaterOrEqual(self, v);
  }
  Constraint* __gt__(int64 v) {
    return self->solver()->MakeGreater(self, v);
  }
  Constraint* __le__(int64 v) {
    return self->solver()->MakeLessOrEqual(self, v);
  }
  Constraint* __lt__(int64 v) {
    return self->solver()->MakeLess(self, v);
  }
  Constraint* __eq__(IntExpr* other) {
    return self->solver()->MakeEquality(self->Var(), other->Var());
  }
  Constraint* __ne__(IntExpr* other) {
    return self->solver()->MakeNonEquality(self->Var(), other->Var());
  }
  Constraint* __ge__(IntExpr* other) {
    return self->solver()->MakeGreaterOrEqual(self->Var(), other->Var());
  }
  Constraint* __gt__(IntExpr* other) {
    return self->solver()->MakeGreater(self->Var(), other->Var());
  }
  Constraint* __le__(IntExpr* other) {
    return self->solver()->MakeLessOrEqual(self->Var(), other->Var());
  }
  Constraint* __lt__(IntExpr* other) {
    return self->solver()->MakeLess(self->Var(), other->Var());
  }
  Constraint* MapTo(const vector<IntVar*>& vars) {
    return self->solver()->MakeMapDomain(self->Var(), vars);
  }
  IntExpr* IndexOf(const vector<int64>& vars) {
    return self->solver()->MakeElement(vars, self->Var());
  }
  IntExpr* IndexOf(const vector<IntVar*>& vars) {
    return self->solver()->MakeElement(vars, self->Var());
  }
}

%extend BaseLNS {
  int64 __getitem__(int index) {
    return self->Value(index);
  }

  int __len__() {
    return self->Size();
  }
}

// Protect from failure
#define PREPROTECT\
  FailureProtect protect;\
  Closure* const intercept = \
      NewCallback<>(&protect, &FailureProtect::JumpBack);\
  solver->set_fail_intercept(intercept);\
  if (setjmp(protect.exception_buffer) == 0) {

#define POSTPROTECT\
    solver->clear_fail_intercept();\
    delete intercept;\
  } else {\
    solver->clear_fail_intercept();\
    PyErr_SetString(PyExc_IndexError, "fail");\
    SWIG_fail;\
  }

%exception IntExpr::SetValue(int64 v) {
  operations_research::Solver* const solver = arg1->solver();
  PREPROTECT
    $action
  POSTPROTECT
}
%exception IntExpr::SetMin(int64 v) {
  operations_research::Solver* const solver = arg1->solver();
  PREPROTECT
    $action
  POSTPROTECT
}
%exception IntExpr::SetMax(int64 v) {
  operations_research::Solver* const solver = arg1->solver();
  PREPROTECT
    $action
  POSTPROTECT
}
%exception IntExpr::SetRange(int64 mi, int64 ma) {
  operations_research::Solver* const solver = arg1->solver();
  PREPROTECT
    $action
  POSTPROTECT
}
%exception IntVar::RemoveValue(int64 v) {
  operations_research::Solver* const solver = arg1->solver();
  PREPROTECT
    $action
  POSTPROTECT
}
%exception Solver::AddConstraint(Constraint* const ct) {
  operations_research::Solver* const solver = arg1;
  PREPROTECT
    $action
  POSTPROTECT
}
%exception Solver::Fail() {
  operations_research::Solver* const solver = arg1;
  PREPROTECT
    $action
  POSTPROTECT
}
}  // namespace operations_research
#endif  // SWIGPYTHON

#if defined(SWIGJAVA)
%module(directors="1") operations_research;
%feature("director") DecisionBuilder;
%feature("director") Decision;
%feature("director") SearchMonitor;
%feature("director") Action;
%feature("director") BaseLns;

%{
#include <setjmp.h>

#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"

struct FailureProtect {
  jmp_buf exception_buffer;
  void JumpBack() {
    longjmp(exception_buffer, 1);
  }
};
%}

%ignore Solver::MakeScalProd(IntVar* const* vars,
                             const int64* const coefs,
                             int size);
%ignore Solver::MakeScalProd(IntVar* const* vars,
                             const int* const coefs,
                             int size);
%ignore Solver::MakeScalProdEquality(IntVar* const* vars,
                                     const int64* const coefs,
                                     int size);
%ignore Solver::MakeScalProdEquality(IntVar* const* vars,
                                     const int* const coefs,
                                     int size);
%ignore Solver::MakeScalProdLessOrEqual(IntVar* const* vars,
                                        const int64* const coefs,
                                        int size);
%ignore Solver::MakeScalProdLessOrEqual(IntVar* const* vars,
                                        const int* const coefs,
                                        int size);
%ignore Solver::MakeScalProdGreaterOrEqual(IntVar* const* vars,
                                           const int64* const coefs,
                                           int size);
%ignore Solver::MakeScalProdGreaterOrEqual(IntVar* const* vars,
                                           const int* const coefs,
                                           int size);
%ignore Solver::MakeRandomLNSOperator(const IntVar* const* vars,
                                      int size,
                                      int number_of_variables);
%ignore Solver::MakeRandomLNSOperator(const IntVar* const* vars,
                                      int size,
                                      int number_of_variables,
                                      int32 seed);
%ignore Solver::MakeAllowedAssignments(const IntVar* const * vars,
                                       const int64* const * tuples,
                                       int tuple_count,
                                       int arity);

%ignore operations_research::Solver::MakeIntVarArray;
%ignore operations_research::Solver::MakeBoolVarArray;
%ignore operations_research::Solver::MakeFixedDurationIntervalVarArray;

%rename (addBacktrackAction) operations_research::Solver::AddBacktrackAction;
%rename (addConstraint) operations_research::Solver::AddConstraint;
%rename (addFailHook) operations_research::Solver::AddFailHook;
%rename (checkAssignment) operations_research::Solver::CheckAssignment;
%rename (compose) operations_research::Solver::Compose;
%rename (concatenateOperators) operations_research::Solver::ConcatenateOperators;
%rename (endSearch) operations_research::Solver::EndSearch;
%rename (exportProfilingOverview) operations_research::Solver::ExportProfilingOverview;
%rename (fail) operations_research::Solver::Fail;
%rename (getTime) operations_research::Solver::GetTime;
%rename (makeAbs) operations_research::Solver::MakeAbs;
%rename (makeAllDifferent) operations_research::Solver::MakeAllDifferent;
%rename (makeAllSolutionCollector) operations_research::Solver::MakeAllSolutionCollector;
%rename (makeAllowedAssignment) operations_research::Solver::MakeAllowedAssignments;
%rename (makeAssignVariableValue) operations_research::Solver::MakeAssignVariableValue;
%rename (makeAssignVariableValueOrFail) operations_research::Solver::MakeAssignVariableValueOrFail;
%rename (makeAssignVariablesValues) operations_research::Solver::MakeAssignVariablesValues;
%rename (makeAssignment) operations_research::Solver::MakeAssignment;
%rename (makeBestValueSolutionCollector) operations_research::Solver::MakeBestValueSolutionCollector;
%rename (makeBetweenCt) operations_research::Solver::MakeBetweenCt;
%rename (makeBoolVar) operations_research::Solver::MakeBoolVar;
%rename (makeConstantRestart) operations_research::Solver::MakeConstantRestart;
%rename (makeConvexPiecewiseExpr) operations_research::Solver::MakeConvexPiecewiseExpr;
%rename (makeCount) operations_research::Solver::MakeCount;
%rename (makeCumulative) operations_research::Solver::MakeCumulative;
%rename (makeCustomLimit) operations_research::Solver::MakeCustomLimit;
%rename (makeDecisionBuilderFromAssignment) operations_research::Solver::MakeDecisionBuilderFromAssignment;
%rename (makeDefaultPhase) operations_research::Solver::MakeDefaultPhase;
%rename (makeDifference) operations_research::Solver::MakeDifference;
%rename (makeDistribute) operations_research::Solver::MakeDistribute;
%rename (makeDiv) operations_research::Solver::MakeDiv;
%rename (makeElement) operations_research::Solver::MakeElement;
%rename (makeEquality) operations_research::Solver::MakeEquality;
%rename (makeFailDecision) operations_research::Solver::MakeFailDecision;
%rename (makeFalseConstraint) operations_research::Solver::MakeFalseConstraint;
%rename (makeFirstSolutionCollector) operations_research::Solver::MakeFirstSolutionCollector;
%rename (makeFixedDurationIntervalVar) operations_research::Solver::MakeFixedDurationIntervalVar;
%rename (makeFixedInterval) operations_research::Solver::MakeFixedInterval;
%rename (makeGreater) operations_research::Solver::MakeGreater;
%rename (makeGreaterOrEqual) operations_research::Solver::MakeGreaterOrEqual;
%rename (makeGuidedLocalSearch) operations_research::Solver::MakeGuidedLocalSearch;
%rename (makeIntConst) operations_research::Solver::MakeIntConst;
%rename (makeIntVar) operations_research::Solver::MakeIntVar;
%rename (makeIntervalVarRelation) operations_research::Solver::MakeIntervalVarRelation;
%rename (makeIsBetweenCt) operations_research::Solver::MakeIsBetweenCt;
%rename (makeIsDifferentCstCt) operations_research::Solver::MakeIsDifferentCstCt;
%rename (makeIsDifferentCstCt) operations_research::Solver::MakeIsDifferentCt;
%rename (makeIsDifferentCstVar) operations_research::Solver::MakeIsDifferentCstVar;
%rename (makeIsDifferentCstVar) operations_research::Solver::MakeIsDifferentVar;
%rename (makeIsEqualCstCt) operations_research::Solver::MakeIsEqualCstCt;
%rename (makeIsEqualCstVar) operations_research::Solver::MakeIsEqualCstVar;
%rename (makeIsEqualVar) operations_research::Solver::MakeIsEqualCt;
%rename (makeIsEqualVar) operations_research::Solver::MakeIsEqualVar;
%rename (makeIsGreaterCstCt) operations_research::Solver::MakeIsGreaterCstCt;
%rename (makeIsGreaterCstVar) operations_research::Solver::MakeIsGreaterCstVar;
%rename (makeIsGreaterCt) operations_research::Solver::MakeIsGreaterCt;
%rename (makeIsGreaterOrEqualCstCt) operations_research::Solver::MakeIsGreaterOrEqualCstCt;
%rename (makeIsGreaterOrEqualCstVar) operations_research::Solver::MakeIsGreaterOrEqualCstVar;
%rename (makeIsGreaterOrEqualCt) operations_research::Solver::MakeIsGreaterOrEqualCt;
%rename (makeIsGreaterOrEqualVar) operations_research::Solver::MakeIsGreaterOrEqualVar;
%rename (makeIsGreaterVar) operations_research::Solver::MakeIsGreaterVar;
%rename (makeIsLessCstCt) operations_research::Solver::MakeIsLessCstCt;
%rename (makeIsLessCstVar) operations_research::Solver::MakeIsLessCstVar;
%rename (makeIsLessCt) operations_research::Solver::MakeIsLessCt;
%rename (makeIsLessOrEqualCstCt) operations_research::Solver::MakeIsLessOrEqualCstCt;
%rename (makeIsLessOrEqualCstVar) operations_research::Solver::MakeIsLessOrEqualCstVar;
%rename (makeIsLessOrEqualCt) operations_research::Solver::MakeIsLessOrEqualCt;
%rename (makeIsLessOrEqualVar) operations_research::Solver::MakeIsLessOrEqualVar;
%rename (makeIsLessVar) operations_research::Solver::MakeIsLessVar;
%rename (makeIsMemberCt) operations_research::Solver::MakeIsMemberCt;
%rename (makeIsMemberVar) operations_research::Solver::MakeIsMemberVar;
%rename (makeLastSolutionCollector) operations_research::Solver::MakeLastSolutionCollector;
%rename (makeLess) operations_research::Solver::MakeLess;
%rename (makeLessOrEqual) operations_research::Solver::MakeLessOrEqual;
%rename (makeLimit) operations_research::Solver::MakeLimit;
%rename (makeLocalSearchObjectiveFilter) operations_research::Solver::MakeLocalSearchObjectiveFilter;
%rename (makeLocalSearchPhase) operations_research::Solver::MakeLocalSearchPhase;
%rename (makeLocalSearchPhaseParameters) operations_research::Solver::MakeLocalSearchPhaseParameters;
%rename (makeLubyRestart) operations_research::Solver::MakeLubyRestart;
%rename (makeMapDomain) operations_research::Solver::MakeMapDomain;
%rename (makeMax) operations_research::Solver::MakeMax;
%rename (makeMaximize) operations_research::Solver::MakeMaximize;
%rename (makeMemberCt) operations_research::Solver::MakeMemberCt;
%rename (makeMin) operations_research::Solver::MakeMin;
%rename (makeMinimize) operations_research::Solver::MakeMinimize;
%rename (makeMirrorInterval) operations_research::Solver::MakeMirrorInterval;
%rename (makeNeighborhoodLimit) operations_research::Solver::MakeNeighborhoodLimit;
%rename (makeNoCycle) operations_research::Solver::MakeNoCycle;
%rename (makeNonEquality) operations_research::Solver::MakeNonEquality;
%rename (makeOperator) operations_research::Solver::MakeOperator;
%rename (makeOpposite) operations_research::Solver::MakeOpposite;
%rename (makeOptimize) operations_research::Solver::MakeOptimize;
%rename (makePack) operations_research::Solver::MakePack;
%rename (makePathCumul) operations_research::Solver::MakePathCumul;
%rename (makePhase) operations_research::Solver::MakePhase;
%rename (makeProd) operations_research::Solver::MakeProd;
%rename (makeRandomLNSOperator) operations_research::Solver::MakeRandomLNSOperator;
%rename (makeRestoreAssignment) operations_research::Solver::MakeRestoreAssignment;
%rename (makeScalProd) operations_research::Solver::MakeScalProd;
%rename (makeScalProdEquality) operations_research::Solver::MakeScalProdEquality;
%rename (makeScalProdGreaterOrEqual) operations_research::Solver::MakeScalProdGreaterOrEqual;
%rename (makeScalProdLessOrEqual) operations_research::Solver::MakeScalProdLessOrEqual;
%rename (makeScheduleOrPostpone) operations_research::Solver::MakeScheduleOrPostpone;
%rename (makeSearchLog) operations_research::Solver::MakeSearchLog;
%rename (makeSearchTrace) operations_research::Solver::MakeSearchTrace;
%rename (makeSemiContinuousExpr) operations_research::Solver::MakeSemiContinuousExpr;
%rename (makeSequence) operations_research::Solver::MakeSequence;
%rename (makeSimulatedAnnealing) operations_research::Solver::MakeSimulatedAnnealing;
%rename (makeSolveOnce) operations_research::Solver::MakeSolveOnce;
%rename (makeSplitVariableDomain) operations_research::Solver::MakeSplitVariableDomain;
%rename (makeSquare) operations_research::Solver::MakeSquare;
%rename (makeStoreAssignment) operations_research::Solver::MakeStoreAssignment;
%rename (makeSum) operations_research::Solver::MakeSum;
%rename (makeSumEquality) operations_research::Solver::MakeSumEquality;
%rename (makeSumGreaterOrEqual) operations_research::Solver::MakeSumGreaterOrEqual;
%rename (makeSumLessOrEqual) operations_research::Solver::MakeSumLessOrEqual;
%rename (makeSymmetryManager) operations_research::Solver::MakeSymmetryManager;
%rename (makeTabuSearch) operations_research::Solver::MakeTabuSearch;
%rename (makeTemporalDisjunction) operations_research::Solver::MakeTemporalDisjunction;
%rename (makeTransitionConstraint) operations_research::Solver::MakeTransitionConstraint;
%rename (makeTreeMonitor) operations_research::Solver::MakeTreeMonitor;
%rename (makeTrueConstraint) operations_research::Solver::MakeTrueConstraint;
%rename (makeTryRankFirst) operations_research::Solver::MakeTryRankFirst;
%rename (makeWeightedMaximize) operations_research::Solver::MakeWeightedMaximize;
%rename (makeWeightedMinimize) operations_research::Solver::MakeWeightedMinimize;
%rename (makeWeightedOptimize) operations_research::Solver::MakeWeightedOptimize;
%rename (max) *::Max;
%rename (min) *::Min;
%rename (nestedSolve) operations_research::Solver::NestedSolve;
%rename (newSearch) operations_research::Solver::NewSearch;
%rename (nextWrap) operations_research::DecisionBuilder::Next;
%rename (nextSolution) operations_research::Solver::NextSolution;
%rename (rand32) operations_research::Solver::Rand32;
%rename (rand64) operations_research::Solver::Rand64;
%rename (randomConcatenateOperators) RandomConcatenateOperators;
%rename (reSeed) operations_research::Solver::ReSeed;
%rename (searchDepth) operations_research::Solver::SearchDepth;
%rename (searchLeftDepth) operations_research::Solver::SearchLeftDepth;
%rename (setMax) *::SetMax;
%rename (setMin) *::SetMin;
%rename (setRange) *::SetRange;
%rename (solve) operations_research::Solver::Solve;
%rename (solveDepth) operations_research::Solver::SolveDepth;
%rename (toString) *::DebugString;
%rename (updateLimits) operations_research::Solver::UpdateLimits;
%rename (value) *::Value;

namespace operations_research {
%define CONVERT_VECTOR(type)
%typemap(jni) const vector<type*>& "jobjectArray"
%typemap(jtype) const vector<type*>& "type[]"
%typemap(jstype) const vector<type*>& "type[]"
%typemap(javain) const vector<type*>& "$javainput"
%typemap(in) const vector<type*>& (vector<type*> result) {
  jclass object_class =
    jenv->FindClass("com/google/ortools/constraintsolver/type");
  if (NULL == object_class)
    return $null;
  jmethodID method_id = jenv->GetStaticMethodID(object_class, "getCPtr", "(Lcom/google/ortools/constraintsolver/type;)J");
  assert(method_id != NULL);
  for (int i = 0; i < jenv->GetArrayLength($input); i++) {
    jobject elem = jenv->GetObjectArrayElement($input, i);
    jlong ptr_value = jenv->CallStaticLongMethod(object_class, method_id, elem);
    result.push_back(reinterpret_cast<operations_research::type*>(ptr_value));
  }
  $1 = &result;
}
%enddef
CONVERT_VECTOR(IntVar);
CONVERT_VECTOR(SearchMonitor);
CONVERT_VECTOR(DecisionBuilder);
CONVERT_VECTOR(IntervalVar);
CONVERT_VECTOR(Sequence);
CONVERT_VECTOR(LocalSearchOperator);
CONVERT_VECTOR(LocalSearchFilter);

%typemap(javacode) Solver %{
  public IntVar[] makeIntVarArray(int count, long min, long max) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = makeIntVar(min, max);
    }
    return array;
  }

  public IntVar[] makeIntVarArray(int count, long min, long max, String name) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      String var_name = name + i;
      array[i] = makeIntVar(min, max, var_name);
    }
    return array;
  }

  public IntVar[] makeBoolVarArray(int count) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = makeBoolVar();
    }
    return array;
  }

  public IntVar[] makeBoolVarArray(int count, String name) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      String var_name = name + i;
      array[i] = makeBoolVar(var_name);
    }
    return array;
  }

  public IntervalVar[] makeFixedDurationIntervalVarArray(int count,
                                                         long start_min,
                                                         long start_max,
                                                         long duration,
                                                         boolean optional) {
    IntervalVar[] array = new IntervalVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = makeFixedDurationIntervalVar(start_min,
                                              start_max,
                                              duration,
                                              optional,
                                              "");
    }
    return array;
  }

  public IntervalVar[] makeFixedDurationIntervalVarArray(int count,
                                                         long start_min,
                                                         long start_max,
                                                         long duration,
                                                         boolean optional,
                                                         String name) {
    IntervalVar[] array = new IntervalVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = makeFixedDurationIntervalVar(start_min,
                                              start_max,
                                              duration,
                                              optional,
                                              name + i);
    }
    return array;
  }

%}

// Protect from failure
#define PREPROTECT\
  FailureProtect protect;\
  Closure* const intercept = \
      NewCallback<>(&protect, &FailureProtect::JumpBack);\
  solver->set_fail_intercept(intercept);\
  if (setjmp(protect.exception_buffer) == 0) {

#define POSTPROTECT\
    solver->clear_fail_intercept();\
    delete intercept;\
  } else {\
    solver->clear_fail_intercept();\
    jclass fail_class = jenv->FindClass(\
        "com/google/ortools/constraintsolver/"\
        "FailException");\
    jenv->ThrowNew(fail_class, "fail");\
    return $null;\
  }

%exception IntExpr::SetValue(int64 v) {
  operations_research::Solver* const solver = arg1->solver();
  PREPROTECT
    $action
  POSTPROTECT
}
%exception IntExpr::SetMin(int64 v) {
  operations_research::Solver* const solver = arg1->solver();
  PREPROTECT
    $action
  POSTPROTECT
}
%exception IntExpr::SetMax(int64 v) {
  operations_research::Solver* const solver = arg1->solver();
  PREPROTECT
    $action
  POSTPROTECT
}
%exception IntExpr::SetRange(int64 mi, int64 ma) {
  operations_research::Solver* const solver = arg1->solver();
  PREPROTECT
    $action
  POSTPROTECT
}
%exception IntVar::RemoveValue(int64 v) {
  operations_research::Solver* const solver = arg1->solver();
  PREPROTECT
    $action
  POSTPROTECT
}
%exception Solver::AddConstraint(Constraint* const ct) {
  operations_research::Solver* const solver = arg1;
  PREPROTECT
    $action
  POSTPROTECT
}
%exception Solver::Fail() {
  operations_research::Solver* const solver = arg1;
  PREPROTECT
    $action
  POSTPROTECT
}
}  // namespace operations_research

#endif  // SWIGJAVA

// Wrap cp includes
%include constraint_solver/constraint_solver.h
%include constraint_solver/constraint_solveri.h

// Define templates instantiation after wrapping.
namespace operations_research {
%template(RevInteger) Rev<int64>;
%template(RevBool) Rev<bool>;
}
