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

%include base/base.swig
%include exception.i


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
}

// Define the renaming of methods.
%rename (Add) AddConstraint;
%rename (AllDifferent) MakeAllDifferent;
%rename (AllSolutionCollector) MakeAllSolutionCollector;
%rename (AllowedAssignments) MakeAllowedAssignments;
%rename (AssignVariableValue) MakeAssignVariableValue;
%rename (AssignVariableValueOrFail) MakeAssignVariableValueOrFail;
%rename (AssignVariableValues) MakeAssignVariableValues;
%rename (Assignment) MakeAssignment;
%rename (BestValueSolutionCollector) MakeBestValueSolutionCollector;
%rename (BetweenCt) MakeBetweenCt;
%rename (BoolVar) MakeBoolVar;
%rename (ConstantRestart) MakeConstantRestart;
%rename (ConvexPiecewiseExpr) MakeConvexPiecewiseExpr;
%rename (Count) MakeCount;
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
%rename (NoCycle) MakeNoCycle;
%rename (Operator) MakeOperator;
%rename (RandomLNSOperator) MakeRandomLNSOperator;
%rename (Optimize) MakeOptimize;
%rename (Pack) MakePack;
%rename (PathCumul) MakePathCumul;
%rename (Phase) MakePhase;
%rename (RestoreAssignment) MakeRestoreAssignment;
%rename (ScalProd) MakeScalProd;
%rename (ScalProdEquality) MakeScalProdEquality;
%rename (ScalProdGreaterOrEqual) MakeScalProdGreaterOrEqual;
%rename (ScalProdLessOrEqual) MakeScalProdLessOrEqual;
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
%rename (TrueConstraint) MakeTrueConstraint;

namespace operations_research {
%{
static string PyCallbackString(PyObject* pyfunc) {
   string result;
   PyObject* arglist = Py_BuildValue("()");
   PyObject* pyresult = PyEval_CallObject(pyfunc, arglist);
   Py_DECREF(arglist);
   if (pyresult) {
     result = PyString_AsString(pyresult);
     Py_DECREF(pyresult);
   }
   return result;
}
%}

%typemap(in) ResultCallback<string>* {
  if (!PyCallable_Check($input)) {
      PyErr_SetString(PyExc_TypeError, "Need a callable object!");
      SWIG_fail;
  }
  $1 = NewPermanentCallback(&PyCallbackString, $input);
}

%{
static int64 PyCallback1Int64Int64(PyObject* pyfunc, int64 i) {
   int64 result = 0;
   // Cast to int needed, no int64 support
   // () needed to force creation of one-element tuple
   PyObject* arglist = Py_BuildValue("(l)", static_cast<int>(i));
   PyObject* pyresult = PyEval_CallObject(pyfunc, arglist);
   Py_DECREF(arglist);
   if (pyresult) {
     result = PyInt_AsLong(pyresult);
   }
   Py_XDECREF(pyresult);
   return result;
}
%}

%typemap(in) ResultCallback1<int64, int64>* {
  if (!PyCallable_Check($input)) {
      PyErr_SetString(PyExc_TypeError, "Need a callable object!");
      SWIG_fail;
  }
  $1 = NewPermanentCallback(&PyCallback1Int64Int64, $input);
}

%{
static int64 PyCallback2Int64Int64Int64(PyObject* pyfunc, int64 i, int64 j) {
   int64 result = 0;
   // Cast to int needed, no int64 support
   PyObject* arglist = Py_BuildValue("ll",
                                     static_cast<int>(i),
                                     static_cast<int>(j));
   PyObject* pyresult = PyEval_CallObject(pyfunc, arglist);
   Py_DECREF(arglist);
   if (pyresult) {
     result = PyInt_AsLong(pyresult);
   }
   Py_XDECREF(pyresult);
   return result;
}
%}

%typemap(in) ResultCallback2<int64, int64, int64>* {
  if (!PyCallable_Check($input)) {
      PyErr_SetString(PyExc_TypeError, "Need a callable object!");
      SWIG_fail;
  }
  $1 = NewPermanentCallback(&PyCallback2Int64Int64Int64, $input);
}

%{
  static int64 PyCallback3Int64Int64Int64Int64(PyObject* pyfunc,
                                               int64 i, int64 j, int64 k) {
   int64 result = 0;
   // Cast to int needed, no int64 support
   PyObject* arglist = Py_BuildValue("lll",
                                     static_cast<int>(i),
                                     static_cast<int>(j),
                                     static_cast<int>(k));
   PyObject* pyresult = PyEval_CallObject(pyfunc, arglist);
   Py_DECREF(arglist);
   if (pyresult) {
     result = PyInt_AsLong(pyresult);
   }
   Py_XDECREF(pyresult);
   return result;
}
%}

%typemap(in) ResultCallback3<int64, int64, int64, int64>* {
  if (!PyCallable_Check($input)) {
      PyErr_SetString(PyExc_TypeError, "Need a callable object!");
      SWIG_fail;
  }
  $1 = NewPermanentCallback(&PyCallback3Int64Int64Int64Int64, $input);
}

%{
static bool PyCallbackBool(PyObject* pyfunc) {
   bool result = false;
   // "()" needed to force creation of empty argument list
   PyObject* arglist = Py_BuildValue("()");
   PyObject* pyresult = PyEval_CallObject(pyfunc, arglist);
   Py_DECREF(arglist);
   if (pyresult) {
     // no PyBool_AsBool so do this instead:
     if (pyresult == Py_True) {
       result = true;
     } else {
       result = false;
     }
   }
   Py_XDECREF(pyresult);
   return result;
}
%}

%typemap(in) ResultCallback<bool>* {
  if (!PyCallable_Check($input)) {
      PyErr_SetString(PyExc_TypeError, "Need a callable object!");
      SWIG_fail;
  }
  $1 = NewPermanentCallback(&PyCallbackBool, $input);
}

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

// Add conversion rules for vector<int64>.
%typemap(in) const vector<int64>& (vector<int64> temp) {
  if (!PyTuple_Check($input) && !PyList_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "Expecting a sequence");
    SWIG_fail;
  }
  bool is_tuple = PyTuple_Check($input);
  temp.resize(is_tuple ? PyTuple_Size($input) : PyList_Size($input));
  for (size_t i = 0; i < temp.size(); ++i) {
    temp[i] = PyInt_AsLong(is_tuple ? PyTuple_GetItem($input, i) :
                           PyList_GetItem($input, i));
  }
  $1 = &temp;
}

%typecheck(SWIG_TYPECHECK_POINTER) const vector<int64>& {
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

// Add conversion list(tuple(int)) -> tuple set for AllowedAssignments.
%typemap(in) const vector<vector<int64> >&
    (vector<vector<int64> > temp) {
  if (!PyList_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "Expecting a list of tuples");
    SWIG_fail;
  }
  int len = PyList_Size($input);
  int arity = -1;
  if (len > 0) {
    temp.resize(len);
    for (size_t i = 0; i < len; ++i) {
      PyObject *tuple = PyList_GetItem($input, i);
      if (!PyTuple_Check(tuple) && !PyList_Check(tuple)) {
        PyErr_SetString(PyExc_TypeError, "Expecting a sequence");
        SWIG_fail;
      }
      bool is_tuple = PyTuple_Check(tuple);
      int local_arity = is_tuple ? PyTuple_Size(tuple) : PyList_Size(tuple);
      if (arity != -1 && arity != local_arity) {
        PyErr_SetString(PyExc_TypeError, "Tuples should have the same arity");
        SWIG_fail;
      }
      if (arity == -1) {
        arity = local_arity;
      }
      temp[i].resize(arity);
      for (size_t j = 0; j < local_arity; ++j) {
        temp[i][j] = PyInt_AsLong(is_tuple ?
                                  PyTuple_GetItem(tuple, j) :
                                  PyList_GetItem(tuple, j));
      }
    }
  }
  $1 = &temp;
}

%typecheck(SWIG_TYPECHECK_POINTER) const vector<vector<int64> >& {
  if (!PyList_Check($input)) {
    $1 = 0;
  } else {
    const int size = PyList_Size($input);
    bool failed = false;
    for (size_t i = 0; i < size; ++i) {
      PyObject* const tuple = PyList_GetItem($input, i);
      if (!PyTuple_Check(tuple) && !PyList_Check(tuple)) {
        $1 = 0;
        break;
      } else {
        const bool is_tuple = PyTuple_Check(tuple);
        const int arity = is_tuple ? PyTuple_Size(tuple) : PyList_Size(tuple);
        for (size_t j = 0; j < arity; ++j) {
          PyObject* const entry =
              is_tuple ? PyTuple_GetItem(tuple, j) : PyList_GetItem(tuple, j);
          if (!PyInt_Check(entry) && !PyLong_Check(entry)) {
            failed = true;
            break;
          }
        }
      }
      if (failed) {
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
};

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

#ifdef SWIGJAVA
%module(directors="1") main
%feature("director") LongResultCallback1;
%feature("director") LongResultCallback2;
%{
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"

class LongResultCallback1 {
 public:
  virtual int64 run(int64) = 0;
  ResultCallback1<int64, int64>* GetPermanentCallback() {
    return NewPermanentCallback(this, &LongResultCallback1::run);
  }
  virtual ~LongResultCallback1() {}
};
class LongResultCallback2 {
 public:
  virtual int64 run(int64, int64) = 0;
  ResultCallback2<int64, int64, int64>* GetPermanentCallback() {
    return NewPermanentCallback(this, &LongResultCallback2::run);
  }
  virtual ~LongResultCallback2() {}
};
class LongResultCallback3 {
 public:
  virtual int64 run(int64, int64, int64) = 0;
  ResultCallback3<int64, int64, int64, int64>* GetPermanentCallback() {
    return NewPermanentCallback(this, &LongResultCallback3::run);
  }
  virtual ~LongResultCallback3() {}
};
%}

class LongResultCallback1 {
 public:
  virtual int64 run(int64) = 0;
  ResultCallback1<int64, int64>* GetPermanentCallback();
  virtual ~LongResultCallback1();
};
class LongResultCallback2 {
 public:
  virtual int64 run(int64, int64) = 0;
  ResultCallback2<int64, int64, int64>* GetPermanentCallback();
  virtual ~LongResultCallback2();
};
class LongResultCallback3 {
 public:
  virtual int64 run(int64, int64, int64) = 0;
  ResultCallback3<int64, int64, int64, int64>* GetPermanentCallback();
  virtual ~LongResultCallback3();
};

// Typemaps for callbacks in java.
%typemap(jstype) ResultCallback1<int64, int64>* "LongResultCallback1";
%typemap(javain) ResultCallback1<int64, int64>* "SWIGTYPE_p_ResultCallback1Tlong_long_long_long_t.getCPtr($javainput.GetPermanentCallback())";

%typemap(jstype) ResultCallback2<int64, int64, int64>* "LongResultCallback2";
%typemap(javain) ResultCallback2<int64, int64, int64>* "SWIGTYPE_p_ResultCallback2Tlong_long_long_long_long_long_t.getCPtr($javainput.GetPermanentCallback())";

%typemap(jstype) ResultCallback3<int64, int64, int64, int64>*
"LongResultCallback3";
%typemap(javain) ResultCallback3<int64, int64, int64, int64>*
 "SWIGTYPE_p_ResultCallback3Tlong_long_long_long_long_long_long_long_t.getCPtr($javainput.GetPermanentCallback())";

// Typemaps to represent const vector<int64>& arguments as arrays of long.
%typemap(jni) const vector<int64>& "jobjectArray"
%typemap(jtype) const vector<int64>& "long[]"
%typemap(jstype) const vector<int64>& "long[]"
%typemap(javain) const vector<int64>& "$javainput"
%typemap(in) const vector<int64>& %{
  if($input) {
    $1 = new vector<int64>;
    int size = jenv->GetArrayLength($input);
    jlong *values = jenv->GetLongArrayElements((jlongArray)$input, NULL);
    for (int i = 0; i < size; ++i) {
      long value = values[i];
      $1->push_back(value);
    }
  }
  else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null table");
    return $null;
  }
%}
%typemap(freearg) const vector<int64>& {
  delete $1;
}

// Typemaps to represent const vector<int>& arguments as arrays of long.
%typemap(jni) const vector<int>& "jobjectArray"
%typemap(jtype) const vector<int>& "int[]"
%typemap(jstype) const vector<int>& "int[]"
%typemap(javain) const vector<int>& "$javainput"

%typemap(in) const vector<int>& %{
  if($input) {
    $1 = new vector<int>;
    int size = jenv->GetArrayLength($input);
    jint *values = jenv->GetIntArrayElements((jintArray)$input, NULL);
    for (int i = 0; i < size; ++i) {
      int value = values[i];
      $1->push_back(value);
    }
  }
  else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null table");
    return $null;
  }
%}
%typemap(freearg) const vector<int>& {
  delete $1;
}

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

%rename (addConstraint) AddConstraint;
%rename (makeAllDifferent) MakeAllDifferent;
%rename (makeAllSolutionCollector) MakeAllSolutionCollector;
%rename (makeAllowedAssignment) MakeAllowedAssignments;
%rename (makeAssignVariableValue) MakeAssignVariableValue;
%rename (makeAssignVariableValues) MakeAssignVariableValues;
%rename (makeAssignment) MakeAssignment;
%rename (makeBestValueSolutionCollector) MakeBestValueSolutionCollector;
%rename (makeBetweenCt) MakeBetweenCt;
%rename (makeBoolVar) MakeBoolVar;
%rename (makeConstantRestart) MakeConstantRestart;
%rename (makeConvexPiecewiseExpr) MakeConvexPiecewiseExpr;
%rename (makeCount) MakeCount;
%rename (makeCustomLimit) MakeCustomLimit;
%rename (makeDecisionBuilderFromAssignment) MakeDecisionBuilderFromAssignment;
%rename (makeDistribute) MakeDistribute;
%rename (makeElement) MakeElement;
%rename (makeFailDecision) MakeFailDecision;
%rename (makeFalseConstraint) MakeFalseConstraint;
%rename (makeFirstSolutionCollector) MakeFirstSolutionCollector;
%rename (makeFixedDurationIntervalVar) MakeFixedDurationIntervalVar;
%rename (makeFixedInterval) MakeFixedInterval;
%rename (makeIntConst) MakeIntConst;
%rename (makeIntVar) MakeIntVar;
%rename (makeIntervalVarRelation) MakeIntervalVarRelation;
%rename (makeIsBetweenCt) MakeIsBetweenCt;
%rename (makeIsDifferentCstCt) MakeIsDifferentCstCt;
%rename (makeIsDifferentCstVar) MakeIsDifferentCstVar;
%rename (makeIsDifferentCstCt) MakeIsDifferentCt;
%rename (makeIsDifferentCstVar) MakeIsDifferentVar;
%rename (makeIsEqualCstCt) MakeIsEqualCstCt;
%rename (makeIsEqualCstVar) MakeIsEqualCstVar;
%rename (makeIsEqualVar) MakeIsEqualCt;
%rename (makeIsEqualVar) MakeIsEqualVar;
%rename (makeIsGreaterCstCt) MakeIsGreaterCstCt;
%rename (makeIsGreaterCstVar) MakeIsGreaterCstVar;
%rename (makeIsGreaterOrEqualCstCt) MakeIsGreaterOrEqualCstCt;
%rename (makeIsGreaterOrEqualCstVar) MakeIsGreaterOrEqualCstVar;
%rename (makeIsLessCstCt) MakeIsLessCstCt;
%rename (makeIsLessCstVar) MakeIsLessCstVar;
%rename (makeIsLessOrEqualCstCt) MakeIsLessOrEqualCstCt;
%rename (makeIsLessOrEqualCstVar) MakeIsLessOrEqualCstVar;
%rename (makeIsMemberCt) MakeIsMemberCt;
%rename (makeIsMemberVar) MakeIsMemberVar;
%rename (makeLastSolutionCollector) MakeLastSolutionCollector;
%rename (makeLimit) MakeLimit;
%rename (makeLocalSearchObjectiveFilter) MakeLocalSearchObjectiveFilter;
%rename (makeLocalSearchPhase) MakeLocalSearchPhase;
%rename (makeLocalSearchPhaseParameters) MakeLocalSearchPhaseParameters;
%rename (makeLubyRestart) MakeLubyRestart;
%rename (makeMapDomain) MakeMapDomain;
%rename (makeMax) MakeMax;
%rename (makeMaximize) MakeMaximize;
%rename (makeMemberCt) MakeMemberCt;
%rename (makeMin) MakeMin;
%rename (makeMinimize) MakeMinimize;
%rename (makeNoCycle) MakeNoCycle;
%rename (makeOperator) MakeOperator;
%rename (makeRandomLNSOperator) MakeRandomLNSOperator;
%rename (makeOptimize) MakeOptimize;
%rename (makePack) MakePack;
%rename (makePathCumul) MakePathCumul;
%rename (makePhase) MakePhase;
%rename (makeRestoreAssignment) MakeRestoreAssignment;
%rename (makeScalProd) MakeScalProd;
%rename (makeScalProdEquality) MakeScalProdEquality;
%rename (makeScalProdGreaterOrEqual) MakeScalProdGreaterOrEqual;
%rename (makeScalProdLessOrEqual) MakeScalProdLessOrEqual;
%rename (makeSearchLog) MakeSearchLog;
%rename (makeSearchTrace) MakeSearchTrace;
%rename (makeSemiContinuousExpr) MakeSemiContinuousExpr;
%rename (makeSequence) MakeSequence;
%rename (makeSimulatedAnnealing) MakeSimulatedAnnealing;
%rename (makeSolveOnce) MakeSolveOnce;
%rename (makeSplitVariableDomain) MakeSplitVariableDomain;
%rename (makeStoreAssignment) MakeStoreAssignment;
%rename (makeSum) MakeSum;
%rename (makeSumEquality) MakeSumEquality;
%rename (makeSumGreaterOrEqual) MakeSumGreaterOrEqual;
%rename (makeSumLessOrEqual) MakeSumLessOrEqual;
%rename (makeSymmetryManager) MakeSymmetryManager;
%rename (makeTabuSearch) MakeTabuSearch;
%rename (makeTemporalDisjunction) MakeTemporalDisjunction;
%rename (makeTrueConstraint) MakeTrueConstraint;
%rename (makeGuidedLocalSearch) MakeGuidedLocalSearch;
%rename (value) *::Value;
%rename (min) *::Min;
%rename (max) *::Max;
%rename (setMin) *::SetMin;
%rename (setMax) *::SetMax;
%rename (setRange) *::SetRange;

#endif  // SWIGJAVA

// Wrap cp includes
%include constraint_solver/constraint_solver.h
%include constraint_solver/constraint_solveri.h

// Define templates instantiation after wrapping.
namespace operations_research {
%template(RevInteger) Rev<int64>;
%template(RevBool) Rev<bool>;
}
