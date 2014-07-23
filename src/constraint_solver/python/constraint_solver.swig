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

// This .swig file exposes the code declared in ../constraint_solver.h and
// ../constraint_solveri.h.
//
// It is particularly complex for a swig file, mostly because it contains a
// lot of MOE code for the export to the or-tools open-source project at
// powerful python API thanks to the possible use of directors.
//
// USAGE EXAMPLES (most of which are also unit tests):
// - ./pywrapcp_test.py
// - python/appointments.py
// - python/golomb8.py
// - python/hidato_table.py
// - python/jobshop_ft06.py
// - python/magic_sequence_distribute.py
// - python/rabbit_pheasant.py
// - python/simple_meeting.py
// - python/sudoku.py
// - python/zebra.py

%include "base/base.swig"

%import "util/python/data.swig"

// Callback wrapping. See base/python/callbacks.swig.
#define FATAL_CALLBACK_EXCEPTION
%include "base/python/callbacks.swig"

// We *do* need to use SWIGTYPE_... type names directly, because the
// (recommended replacement) $descriptor macro fails, as of 2014-06, with
// types such as operations_research::Solver.
// The absence of whitespace before 'swiglint' is mandatory.
//swiglint: disable swigtype-name

%{
// This #includes constraint_solver.h, and inlines some C++ helpers.
#include "constraint_solver/python/pywrapcp_util.h"
%}

// We need to fully support C++ inheritance, because it is heavily used by the
// exposed C++ classes. Eg:
// class BaseClass {
//   virtual void Foo() { ... }
//   virtual void Bar() { Foo(); ... }
// };
// ...
// class SubClass {
//   // Overrides Foo; and expects the inherited Bar() to use
//   // the overriden Foo().
//   virtual void Foo() { ... }
// };
//
// See the occurrences of "director" in this file.
%module(directors="1") operations_research
// The %feature and %exception below let python exceptions that occur within
// director method propagate to the user as they were originally. See
// http://www.swig.org/Doc1.3/Python.html#Python_nn36 for example.
%feature("director:except") {
    if ($error != NULL) {
        throw Swig::DirectorMethodException();
    }
}
%exception {
  try { $action }
  catch (Swig::DirectorException &e) { SWIG_fail; }
}


// ============= Type conversions ==============

// Conversion utilities, to be able to expose APIs that return a C++ object
// pointer. The PyObjAs template must be able to deal with all such types.
%define PY_CONVERT_HELPER_PTR(CType)
%{
template<>
bool PyObjAs(PyObject *py_obj, operations_research::CType** b) {
  return SWIG_ConvertPtr(py_obj, reinterpret_cast<void**>(b),
                         SWIGTYPE_p_operations_research__ ## CType,
                         SWIG_POINTER_EXCEPTION) >= 0;
}
%}
%enddef
PY_CONVERT_HELPER_PTR(SearchMonitor);
PY_CONVERT_HELPER_PTR(IntervalVar);
PY_CONVERT_HELPER_PTR(SequenceVar);
PY_CONVERT_HELPER_PTR(LocalSearchOperator);
PY_CONVERT_HELPER_PTR(LocalSearchFilter);
#undef PY_CONVERT_HELPER_PTR

// Conversion of IntExpr* and IntVar* are a bit special because of the two
// possible casts from IntExpr and Constraint.
%define PY_CONVERT_HELPER_INTEXPR_OR_INTVAR(Class)
%{
template<>
bool PyObjAs(PyObject *py_obj, operations_research::Class** var) {
  // First, try to interpret the python object as an IntExpr.
  operations_research::IntExpr* t;
  if (SWIG_ConvertPtr(py_obj, reinterpret_cast<void**>(&t),
                      SWIGTYPE_p_operations_research__IntExpr,
                      SWIG_POINTER_EXCEPTION) >= 0) {
    if (t == nullptr) return false;
    *var = t->Var();
    return true;
  }
  // Then, try to interpret it as a Constraint.
  operations_research::Constraint* c;
  if (SWIG_ConvertPtr(py_obj, reinterpret_cast<void**>(&c),
                      SWIGTYPE_p_operations_research__Constraint,
                      SWIG_POINTER_EXCEPTION) >= 0) {
    if (c == nullptr || c->Var() == nullptr) return false;
    *var = c->Var();
    return true;
  }
  // Give up.
  return false;
}
%}
%enddef
PY_CONVERT_HELPER_INTEXPR_OR_INTVAR(IntVar);
PY_CONVERT_HELPER_INTEXPR_OR_INTVAR(IntExpr);
#undef PY_CONVERT_HELPER_INTEXPR_OR_INTVAR

// Conversions of DecisionBuilder* are also special because we redefine
// our own python PyDecisionBuilder object.
// TODO(user): for or-tools, remove this code and use directors instead.
%pythoncode {
class PyDecisionBuilder(object):
  def NextWrapper(self, solver):
    try:
      return self.Next(solver)
    except Exception:
      return solver.FailDecision()

  def DebugString(self):
    return "PyDecisionBuilder"
}  // %pythoncode
%{
template<>
bool PyObjAs(PyObject *py_obj, operations_research::DecisionBuilder** b) {
  if (SWIG_ConvertPtr(py_obj, reinterpret_cast<void**>(b),
                      SWIGTYPE_p_operations_research__DecisionBuilder,
                      SWIG_POINTER_EXCEPTION) == -1) {
    // If the conversion failed, we try to create our own decision builder
    // if it looks like we're given one.
    PyErr_Clear();
    if (PyObject_GetAttrString(py_obj, "Next") == nullptr) return false;
    *b = new CallPyDecisionBuilder(py_obj);
  }
  return true;
}
%}

// Actual conversions. This also includes the conversion to std::vector<Class>.
%define PY_CONVERT(Class)
%{
bool CanConvertTo ## Class(PyObject *py_obj) {
  operations_research::Class* tmp;
  return PyObjAs(py_obj, &tmp);
}
%}
%typemap(in) operations_research::Class* const {
  if (!PyObjAs($input, &$1)) SWIG_fail;
}
%typecheck(SWIG_TYPECHECK_POINTER) operations_research::Class* const {
  $1 = CanConvertTo ## Class($input);
  if ($1 == 0) PyErr_Clear();
}
PY_LIST_OUTPUT_TYPEMAP(operations_research::Class*, CanConvertTo ## Class,
                       PyObjAs<operations_research::Class*>);
%enddef
PY_CONVERT(IntVar);
PY_CONVERT(IntExpr);
PY_CONVERT(DecisionBuilder);
PY_CONVERT(SearchMonitor);
PY_CONVERT(IntervalVar);
PY_CONVERT(SequenceVar);
PY_CONVERT(LocalSearchOperator);
PY_CONVERT(LocalSearchFilter);
#undef PY_CONVERT


// ============= Extensions ==============

// Add display methods on BaseObject and Solver.
%extend operations_research::BaseObject {
  string __str__() {
    return $self->DebugString();
  }
}
%extend operations_research::Solver {
  string __str__() {
    return $self->DebugString();
  }
}

// Extend IntervalVar to provide a nicer pythonic API for precedence
// and scheduling constraints. The macros below help do that concisely.
%define PRECEDENCE_CONSTRAINT(PythonMethodName, CppEnumName)
Constraint* PythonMethodName(IntervalVar* other) {
  return $self->solver()->MakeIntervalVarRelation(
      $self, operations_research::Solver::CppEnumName, other);
}
%enddef
%define SCHEDULING_CONSTRAINT(PythonMethodName, CppEnumName)
Constraint* PythonMethodName(int64 date) {
  return $self->solver()->MakeIntervalVarRelation(
      $self, operations_research::Solver::CppEnumName, date);
}
%enddef
%extend operations_research::IntervalVar {
  PRECEDENCE_CONSTRAINT(EndsAfterEnd, ENDS_AFTER_END)
  PRECEDENCE_CONSTRAINT(EndsAfterStart, ENDS_AFTER_START)
  PRECEDENCE_CONSTRAINT(EndsAtEnd, ENDS_AT_END)
  PRECEDENCE_CONSTRAINT(EndsAtStart, ENDS_AT_START)
  PRECEDENCE_CONSTRAINT(StartsAfterEnd, STARTS_AFTER_END)
  PRECEDENCE_CONSTRAINT(StartsAfterStart, STARTS_AFTER_START)
  PRECEDENCE_CONSTRAINT(StartsAtEnd, STARTS_AT_END)
  PRECEDENCE_CONSTRAINT(StartsAtStart, STARTS_AT_START)
  PRECEDENCE_CONSTRAINT(StaysInSync, STAYS_IN_SYNC)
  SCHEDULING_CONSTRAINT(EndsAfter, ENDS_AFTER)
  SCHEDULING_CONSTRAINT(EndsAt, ENDS_AT)
  SCHEDULING_CONSTRAINT(EndsBefore, ENDS_BEFORE)
  SCHEDULING_CONSTRAINT(StartsAfter, STARTS_AFTER)
  SCHEDULING_CONSTRAINT(StartsAt, STARTS_AT)
  SCHEDULING_CONSTRAINT(StartsBefore, STARTS_BEFORE)
  SCHEDULING_CONSTRAINT(CrossesDate, CROSS_DATE)
  SCHEDULING_CONSTRAINT(AvoidsDate, AVOID_DATE)
}
#undef PRECEDENCE_CONSTRAINT
#undef SCHEDULING_CONSTRAINT

// Use DebugString() for the native string conversion in python, for objects
// that support it.
%define PY_STRINGIFY_DEBUGSTRING(Class)
%extend operations_research::Class {
  string __repr__() {
    return $self->DebugString();
  }
}
%enddef
PY_STRINGIFY_DEBUGSTRING(BaseObject);
PY_STRINGIFY_DEBUGSTRING(IntervalVar);
PY_STRINGIFY_DEBUGSTRING(SequenceVar);
PY_STRINGIFY_DEBUGSTRING(IntVar);
PY_STRINGIFY_DEBUGSTRING(IntExpr);
PY_STRINGIFY_DEBUGSTRING(Constraint);
PY_STRINGIFY_DEBUGSTRING(SearchMonitor);
PY_STRINGIFY_DEBUGSTRING(DecisionBuilder);
PY_STRINGIFY_DEBUGSTRING(Decision);
#undef PY_STRINGIFY_DEBUGSTRING

// Extend the solver with a few nicer pythonic methods.
%extend operations_research::Solver {
  Constraint* TreeNoCycle(const std::vector<IntVar*>& nexts,
                          const std::vector<IntVar*>& active,
                          ResultCallback1<bool, int64>* callback = nullptr) {
    return $self->MakeNoCycle(nexts, active, callback, false);
  }

  SearchMonitor* SearchLogWithCallback(int period,
                                       ResultCallback<string>* callback) {
    return $self->MakeSearchLog(period, callback);
  }

  IntExpr* ElementFunction(ResultCallback1<int64, int64>* values,
                           IntVar* const index) {
    return $self->MakeElement(values, index);
  }

  LocalSearchOperator* LNSOperator(const std::vector<IntVar*>& vars,
                                   PyObject* lns) {
    return $self->RevAlloc(new PyLNS(vars, lns));
  }

  LocalSearchOperator* LNSNoValuesOperator(const std::vector<IntVar*>& vars,
                                           PyObject* lns) {
    return $self->RevAlloc(new PyLNSNoValues(vars, lns));
  }

  DecisionBuilder* VarEvalValStrPhase(
      const std::vector<IntVar*>& vars,
      ResultCallback1<int64, int64>* var_evaluator,
      operations_research::Solver::IntValueStrategy val_str) {
    return $self->MakePhase(vars, var_evaluator, val_str);
  }

  DecisionBuilder* VarStrValEvalPhase(
      const std::vector<IntVar*>& vars,
      operations_research::Solver::IntVarStrategy var_str,
      ResultCallback2<int64, int64, int64>* val_eval) {
    return $self->MakePhase(vars, var_str, val_eval);
  }

  DecisionBuilder* VarEvalValEvalPhase(
      const std::vector<IntVar*>& vars,
      ResultCallback1<int64, int64>* var_eval,
      ResultCallback2<int64, int64, int64>* val_eval) {
    return $self->MakePhase(vars, var_eval, val_eval);
  }

  DecisionBuilder* VarStrValEvalTieBreakPhase(
      const std::vector<IntVar*>& vars,
      operations_research::Solver::IntVarStrategy var_str,
      ResultCallback2<int64, int64, int64>* val_eval,
      ResultCallback1<int64, int64>* tie_breaker) {
    return $self->MakePhase(vars, var_str, val_eval, tie_breaker);
  }

  DecisionBuilder* VarEvalValEvalTieBreakPhase(
      const std::vector<IntVar*>& vars,
      ResultCallback1<int64, int64>* var_eval,
      ResultCallback2<int64, int64, int64>* val_eval,
      ResultCallback1<int64, int64>* tie_breaker) {
    return $self->MakePhase(vars, var_eval, val_eval, tie_breaker);
  }

  DecisionBuilder* EvalEvalStrPhase(
      const std::vector<IntVar*>& vars,
      ResultCallback2<int64, int64, int64>* evaluator,
      operations_research::Solver::EvaluatorStrategy str) {
    return $self->MakePhase(vars, evaluator, str);
  }

  DecisionBuilder* EvalEvalStrTieBreakPhase(
      const std::vector<IntVar*>& vars,
      ResultCallback2<int64, int64, int64>* evaluator,
      ResultCallback1<int64, int64>* tie_breaker,
      operations_research::Solver::EvaluatorStrategy str) {
    return $self->MakePhase(vars, evaluator, tie_breaker, str);
  }

  SearchMonitor* GuidedLocalSearch(
      bool maximize,
      IntVar* const objective,
      ResultCallback2<int64, int64, int64>* objective_function,
      int64 step,
      const std::vector<IntVar*>& vars,
      double penalty_factor) {
    return $self->MakeGuidedLocalSearch(maximize,
                                       objective,
                                       objective_function,
                                       step,
                                       vars,
                                       penalty_factor);
  }

  LocalSearchFilter* LocalSearchObjectiveFilter(
      const std::vector<IntVar*>& vars,
      ResultCallback2<int64, int64, int64>* values,
      IntVar* const objective,
      Solver::LocalSearchFilterBound filter_enum,
      Solver::LocalSearchOperation op_enum) {
    return $self->MakeLocalSearchObjectiveFilter(vars,
                                                values,
                                                objective,
                                                filter_enum,
                                                op_enum);
  }
}

// Add arithmetic operators to integer expressions.
%extend operations_research::IntExpr {
  IntExpr* __add__(IntExpr* other) {
    return $self->solver()->MakeSum($self, other);
  }
  IntExpr* __add__(Constraint* other) {
    return $self->solver()->MakeSum($self, other->Var());
  }
  IntExpr* __add__(int64 v) {
    return $self->solver()->MakeSum($self, v);
  }
  IntExpr* __radd__(int64 v) {
    return $self->solver()->MakeSum($self, v);
  }
  IntExpr* __sub__(IntExpr* other) {
    return $self->solver()->MakeDifference($self, other);
  }
  IntExpr* __sub__(Constraint* other) {
    return $self->solver()->MakeDifference($self, other->Var());
  }
  IntExpr* __sub__(int64 v) {
    return $self->solver()->MakeSum($self, -v);
  }
  IntExpr* __rsub__(int64 v) {
    return $self->solver()->MakeDifference(v, $self);
  }
  IntExpr* __mul__(IntExpr* other) {
    return $self->solver()->MakeProd($self, other);
  }
  IntExpr* __mul__(Constraint* other) {
    return $self->solver()->MakeProd($self, other->Var());
  }
  IntExpr* __mul__(int64 v) {
    return $self->solver()->MakeProd($self, v);
  }
  IntExpr* __rmul__(int64 v) {
    return $self->solver()->MakeProd($self, v);
  }
  IntExpr* __floordiv__(int64 v) {
    return $self->solver()->MakeDiv($self, v);
  }
  IntExpr* __mod__(int64 v) {
    return $self->solver()->MakeModulo($self, v);
  }
  IntExpr* __mod__(IntExpr* e) {
    return $self->solver()->MakeModulo($self, e);
  }
  IntExpr* __neg__() {
    return $self->solver()->MakeOpposite($self);
  }
  IntExpr* __abs__() {
    return $self->solver()->MakeAbs($self);
  }
  IntExpr* Square() {
    return $self->solver()->MakeSquare($self);
  }

  Constraint* __eq__(int64 v) {
    return $self->solver()->MakeEquality($self, v);
  }
  Constraint* __ne__(int64 v) {
    return $self->solver()->MakeNonEquality($self->Var(), v);
  }
  Constraint* __ge__(int64 v) {
    return $self->solver()->MakeGreaterOrEqual($self, v);
  }
  Constraint* __gt__(int64 v) {
    return $self->solver()->MakeGreater($self, v);
  }
  Constraint* __le__(int64 v) {
    return $self->solver()->MakeLessOrEqual($self, v);
  }
  Constraint* __lt__(int64 v) {
    return $self->solver()->MakeLess($self, v);
  }
  Constraint* __eq__(IntExpr* other) {
    return $self->solver()->MakeEquality($self->Var(), other->Var());
  }
  Constraint* __ne__(IntExpr* other) {
    return $self->solver()->MakeNonEquality($self->Var(), other->Var());
  }
  Constraint* __ge__(IntExpr* other) {
    return $self->solver()->MakeGreaterOrEqual($self->Var(), other->Var());
  }
  Constraint* __gt__(IntExpr* other) {
    return $self->solver()->MakeGreater($self->Var(), other->Var());
  }
  Constraint* __le__(IntExpr* other) {
    return $self->solver()->MakeLessOrEqual($self->Var(), other->Var());
  }
  Constraint* __lt__(IntExpr* other) {
    return $self->solver()->MakeLess($self->Var(), other->Var());
  }
  Constraint* __eq__(Constraint* other) {
    return $self->solver()->MakeEquality($self->Var(), other->Var());
  }
  Constraint* __ne__(Constraint* other) {
    return $self->solver()->MakeNonEquality($self->Var(), other->Var());
  }
  Constraint* __ge__(Constraint* other) {
    return $self->solver()->MakeGreaterOrEqual($self->Var(), other->Var());
  }
  Constraint* __gt__(Constraint* other) {
    return $self->solver()->MakeGreater($self->Var(), other->Var());
  }
  Constraint* __le__(Constraint* other) {
    return $self->solver()->MakeLessOrEqual($self->Var(), other->Var());
  }
  Constraint* __lt__(Constraint* other) {
    return $self->solver()->MakeLess($self->Var(), other->Var());
  }
  Constraint* MapTo(const std::vector<IntVar*>& vars) {
    return $self->solver()->MakeMapDomain($self->Var(), vars);
  }
  IntExpr* IndexOf(const std::vector<int64>& vars) {
    return $self->solver()->MakeElement(vars, $self->Var());
  }
  IntExpr* IndexOf(const std::vector<IntVar*>& vars) {
    return $self->solver()->MakeElement(vars, $self->Var());
  }
  IntVar* IsMember(const std::vector<int64>& values) {
    return $self->solver()->MakeIsMemberVar($self->Var(), values);
  }
  Constraint* Member(const std::vector<int64>& values) {
    return $self->solver()->MakeMemberCt($self->Var(), values);
  }
}

// Add arithmetic operators to integer expressions.
%extend operations_research::Constraint {
  IntExpr* __add__(IntExpr* other) {
    return $self->solver()->MakeSum($self->Var(), other);
  }
  IntExpr* __add__(Constraint* other) {
    return $self->solver()->MakeSum($self->Var(), other->Var());
  }
  IntExpr* __add__(int64 v) {
    return $self->solver()->MakeSum($self->Var(), v);
  }
  IntExpr* __radd__(int64 v) {
    return $self->solver()->MakeSum($self->Var(), v);
  }
  IntExpr* __sub__(IntExpr* other) {
    return $self->solver()->MakeDifference($self->Var(), other);
  }
  IntExpr* __sub__(Constraint* other) {
    return $self->solver()->MakeDifference($self->Var(), other->Var());
  }
  IntExpr* __sub__(int64 v) {
    return $self->solver()->MakeSum($self->Var(), -v);
  }
  IntExpr* __rsub__(int64 v) {
    return $self->solver()->MakeDifference(v, $self->Var());
  }
  IntExpr* __mul__(IntExpr* other) {
    return $self->solver()->MakeProd($self->Var(), other);
  }
  IntExpr* __mul__(Constraint* other) {
    return $self->solver()->MakeProd($self->Var(), other->Var());
  }
  IntExpr* __mul__(int64 v) {
    return $self->solver()->MakeProd($self->Var(), v);
  }
  IntExpr* __rmul__(int64 v) {
    return $self->solver()->MakeProd($self->Var(), v);
  }
  IntExpr* __floordiv__(int64 v) {
    return $self->solver()->MakeDiv($self->Var(), v);
  }

  IntExpr* __neg__() {
    return $self->solver()->MakeOpposite($self->Var());
  }
  IntExpr* __abs__() {
    return $self->solver()->MakeAbs($self->Var());
  }
  IntExpr* Square() {
    return $self->solver()->MakeSquare($self->Var());
  }

  Constraint* __eq__(int64 v) {
    return $self->solver()->MakeEquality($self->Var(), v);
  }
  Constraint* __ne__(int64 v) {
    return $self->solver()->MakeNonEquality($self->Var(), v);
  }
  Constraint* __ge__(int64 v) {
    return $self->solver()->MakeGreaterOrEqual($self->Var(), v);
  }
  Constraint* __gt__(int64 v) {
    return $self->solver()->MakeGreater($self->Var(), v);
  }
  Constraint* __le__(int64 v) {
    return $self->solver()->MakeLessOrEqual($self->Var(), v);
  }
  Constraint* __lt__(int64 v) {
    return $self->solver()->MakeLess($self->Var(), v);
  }
  Constraint* __eq__(IntExpr* other) {
    return $self->solver()->MakeEquality($self->Var(), other->Var());
  }
  Constraint* __ne__(IntExpr* other) {
    return $self->solver()->MakeNonEquality($self->Var(), other->Var());
  }
  Constraint* __ge__(IntExpr* other) {
    return $self->solver()->MakeGreaterOrEqual($self->Var(), other->Var());
  }
  Constraint* __gt__(IntExpr* other) {
    return $self->solver()->MakeGreater($self->Var(), other->Var());
  }
  Constraint* __le__(IntExpr* other) {
    return $self->solver()->MakeLessOrEqual($self->Var(), other->Var());
  }
  Constraint* __lt__(IntExpr* other) {
    return $self->solver()->MakeLess($self->Var(), other->Var());
  }
  Constraint* __eq__(Constraint* other) {
    return $self->solver()->MakeEquality($self->Var(), other->Var());
  }
  Constraint* __ne__(Constraint* other) {
    return $self->solver()->MakeNonEquality($self->Var(), other->Var());
  }
  Constraint* __ge__(Constraint* other) {
    return $self->solver()->MakeGreaterOrEqual($self->Var(), other->Var());
  }
  Constraint* __gt__(Constraint* other) {
    return $self->solver()->MakeGreater($self->Var(), other->Var());
  }
  Constraint* __le__(Constraint* other) {
    return $self->solver()->MakeLessOrEqual($self->Var(), other->Var());
  }
  Constraint* __lt__(Constraint* other) {
    return $self->solver()->MakeLess($self->Var(), other->Var());
  }
  Constraint* MapTo(const std::vector<IntVar*>& vars) {
    return $self->solver()->MakeMapDomain($self->Var(), vars);
  }
  IntExpr* IndexOf(const std::vector<int64>& vars) {
    return $self->solver()->MakeElement(vars, $self->Var());
  }
  IntExpr* IndexOf(const std::vector<IntVar*>& vars) {
    return $self->solver()->MakeElement(vars, $self->Var());
  }
}

// Add easy variable getters to BaseLNS ([i] gets the value of variable #i).
%extend operations_research::BaseLNS {
  int64 __getitem__(int index) {
    return $self->Value(index);
  }

  int __len__() {
    return $self->Size();
  }
}

// Extend IntVarIterator to make it iterable in python.
%extend operations_research::IntVarIterator {
  %pythoncode {
  def __iter__(self):
    self.Init()
    return self

  def next(self):
    if self.Ok():
      result = self.Value()
      self.Next()
      return result
    else:
      raise StopIteration()
  }  // %pythoncode
}

// Extend IntVar to provide natural iteration over its domains.
%extend operations_research::IntVar {
  %pythoncode {
  def DomainIterator(self):
    return iter(self.DomainIteratorAux(False))

  def HoleIterator(self):
    return iter(self.HoleIteratorAux(False))
  }  // %pythoncode
}

%extend operations_research::IntVarLocalSearchFilter {
  int64 IndexFromVar(IntVar* const var) const {
    int64 index = -1;
    $self->FindIndex(var, &index);
    return index;
  }
}


// ############ BEGIN DUPLICATED CODE BLOCK ############
// IMPORTANT: keep this code block in sync with the .swig
// files in ../java and ../csharp.
// TODO(user): extract this duplicated code into a common, multi-language
// .swig file with SWIG_exception.

// Protect from failure.
// TODO(user): document this further.
%define PROTECT_FROM_FAILURE(Method, GetSolver)
%exception Method {
  operations_research::Solver* const solver = GetSolver;
  FailureProtect protect;
  Closure* const intercept = NewCallback<>(&protect, &FailureProtect::JumpBack);
  solver->set_fail_intercept(intercept);
  if (setjmp(protect.exception_buffer) == 0) {
    $action
    solver->clear_fail_intercept();
    delete intercept;
  } else {
    solver->clear_fail_intercept();
    // IMPORTANT: the type and message of the exception raised matter,
    // because they are caught by the python overrides of some CP classes.
    // See the occurrences of the "PyExc_Exception" string below.
    PyErr_SetString(PyExc_Exception, "CP Solver fail");
    SWIG_fail;
  }
}
%enddef

namespace operations_research {
PROTECT_FROM_FAILURE(IntExpr::SetValue(int64 v), arg1->solver());
PROTECT_FROM_FAILURE(IntExpr::SetMin(int64 v), arg1->solver());
PROTECT_FROM_FAILURE(IntExpr::SetMax(int64 v), arg1->solver());
PROTECT_FROM_FAILURE(IntExpr::SetRange(int64 mi, int64 ma), arg1->solver());
PROTECT_FROM_FAILURE(IntVar::RemoveValue(int64 v), arg1->solver());
PROTECT_FROM_FAILURE(IntVar::RemoveValues(const std::vector<int64>& values),
                     arg1->solver());
PROTECT_FROM_FAILURE(IntervalVar::SetStartMin(int64 m), arg1->solver());
PROTECT_FROM_FAILURE(IntervalVar::SetStartMax(int64 m), arg1->solver());
PROTECT_FROM_FAILURE(IntervalVar::SetStartRange(int64 mi, int64 ma),
                     arg1->solver());
PROTECT_FROM_FAILURE(IntervalVar::SetDurationMin(int64 m), arg1->solver());
PROTECT_FROM_FAILURE(IntervalVar::SetDurationMax(int64 m), arg1->solver());
PROTECT_FROM_FAILURE(IntervalVar::SetDurationRange(int64 mi, int64 ma),
                     arg1->solver());
PROTECT_FROM_FAILURE(IntervalVar::SetEndMin(int64 m), arg1->solver());
PROTECT_FROM_FAILURE(IntervalVar::SetEndMax(int64 m), arg1->solver());
PROTECT_FROM_FAILURE(IntervalVar::SetEndRange(int64 mi, int64 ma),
                     arg1->solver());
PROTECT_FROM_FAILURE(IntervalVar::SetPerformed(bool val), arg1->solver());
PROTECT_FROM_FAILURE(Solver::AddConstraint(Constraint* const ct), arg1);
PROTECT_FROM_FAILURE(Solver::Fail(), arg1);
}  // namespace operations_research
#undef PROTECT_FROM_FAILURE

// ############ END DUPLICATED CODE BLOCK ############

// ============= Handling C++ flags ==============

// We "convert" the C++ flags to python gflags. We also need to actually
// pass the value from the python flags to the C++ ones, upon the Solver
// construction.
// TODO(user): cp_trace_propagation is set too late to be useful. Investigate
// and fix it.
// Phase 1: declare the C++ flags.
%import "base/commandlineflags.h"
%{
DECLARE_bool(cp_trace_propagation);
DECLARE_bool(cp_trace_search);
DECLARE_bool(cp_print_model);
DECLARE_bool(cp_model_stats);
DECLARE_string(cp_export_file);
DECLARE_bool(cp_no_solve);
DECLARE_string(cp_profile_file);
%}
// Phase 2: define the python flags.
%pythoncode {
import gflags as flags
FLAGS = flags.FLAGS
flags.DEFINE_boolean('cp_trace_propagation', False,
                      'trace all propagation events.')
flags.DEFINE_boolean('cp_trace_search', False,
                      'trace all search events.')
flags.DEFINE_boolean('cp_print_model', False,
                      'prints the model before solving it.')
flags.DEFINE_boolean('cp_model_stats', False,
                      'displays model statistics before solving it.')
flags.DEFINE_string('cp_export_file', '',
                     'exports model to file using CPModelProto.')
flags.DEFINE_boolean('cp_no_solve', False,
                      'force failures at the beginning of a search.')
flags.DEFINE_string('cp_profile_file', '',
                     'exports profiling overview to file.')
}  // %pythoncode
// Phase 3: extend the Solver C++ object with a SetFlags() method that sets
// all the important flags from its input parameters.
%extend operations_research::Solver {
  static void SetFlags(bool trace_propagation,
                       bool trace_search,
                       bool print_model,
                       bool model_stats,
                       const string& export_file,
                       bool no_solve,
                       const string& profile_file) {
    FLAGS_cp_trace_propagation = trace_propagation;
    FLAGS_cp_trace_search = trace_search;
    FLAGS_cp_print_model = print_model;
    FLAGS_cp_model_stats = model_stats;
    FLAGS_cp_export_file = export_file;
    FLAGS_cp_no_solve = no_solve;
    FLAGS_cp_profile_file = profile_file;
  }
}
// Phase 4: at the end of the Solver's constructor code (in python), call
// SetFlags() with the python flags as parameters.
// Note: Indentation is critical here as the code is copied verbatim in the
// python code.
%feature("pythonappend") operations_research::Solver::Solver %{
        Solver.SetFlags(FLAGS.cp_trace_propagation,
                        FLAGS.cp_trace_search,
                        FLAGS.cp_print_model,
                        FLAGS.cp_model_stats,
                        FLAGS.cp_export_file,
                        FLAGS.cp_no_solve,
                        FLAGS.cp_profile_file)
%}


// ============= Exposed C++ API : Solver class ==============

%ignoreall

%unignore operations_research;

namespace operations_research {

// Solver: Basic API.
%unignore Solver;
%unignore Solver::Solver;
%unignore Solver::~Solver;
%rename (Add) Solver::AddConstraint;
%unignore Solver::Solve;

// Solver: Decomposed or specialized Solve() API.
%unignore Solver::NewSearch;
%unignore Solver::NextSolution;
%unignore Solver::RestartSearch;
%unignore Solver::EndSearch;
%unignore Solver::Fail;
%unignore Solver::SolveAndCommit;
%unignore Solver::FinishCurrentSearch;
%unignore Solver::RestartCurrentSearch;

// Solver: Debug and performance counters.
%rename (WallTime) Solver::wall_time;
%rename (Branches) Solver::branches;
%rename (Solutions) Solver::solutions;
%rename (Failures) Solver::failures;
%rename (AcceptedNeighbors) Solver::accepted_neighbors;
%rename (Stamp) Solver::stamp;
%rename (FailStamp) Solver::fail_stamp;
%unignore Solver::SearchDepth;
%unignore Solver::SearchLeftDepth;
%unignore Solver::SolveDepth;
%rename (Constraints) Solver::constraints;
%unignore Solver::CheckAssignment;
%unignore Solver::CheckConstraint;
%unignore Solver::MemoryUsage;

// Solver: IntVar creation. We always strip the "Make" prefix in python.
%rename (IntVar) Solver::MakeIntVar;
%rename (BoolVar) Solver::MakeBoolVar;
%rename (IntConst) Solver::MakeIntConst;

// Solver: IntExpr creation. Most methods creating IntExpr are not exposed
// directly in python, but rather via the "Natural language" API. See examples.
%rename (Sum) Solver::MakeSum(const std::vector<IntVar*>&);
%rename (ScalProd) Solver::MakeScalProd;
%rename (Max) Solver::MakeMax;
%rename (Min) Solver::MakeMin;
%rename (SemiContinuousExpr) Solver::MakeSemiContinuousExpr;
%rename (MonotonicElement) Solver::MakeMonotonicElement;
%rename (IndexEpression) Solver::MakeIndexEpression;
%rename (ConvexPiecewiseExpr) Solver::MakeConvexPiecewiseExpr;
%rename (ConditionalExpression) Solver::MakeConditionalExpression;
%rename (Element) Solver::MakeElement;

// Solver: Basic constraints.
%rename (TrueConstraint) Solver::MakeTrueConstraint;
%rename (FalseConstraint) Solver::MakeFalseConstraint;
%rename (AllDifferent) Solver::MakeAllDifferent;
%rename (AllDifferentExcept) Solver::MakeAllDifferentExcept;
%rename (AllowedAssignments) Solver::MakeAllowedAssignments;
%rename (BetweenCt) Solver::MakeBetweenCt;
%rename (DisjunctiveConstraint) Solver::MakeDisjunctiveConstraint;
%rename (Distribute) Solver::MakeDistribute;
%rename (Cumulative) Solver::MakeCumulative;

// Solver: Constraints extracted from expressions.
%rename (SumLessOrEqual) Solver::MakeSumLessOrEqual;
%rename (SumGreaterOrEqual) Solver::MakeSumGreaterOrEqual;
%rename (SumEquality) Solver::MakeSumEquality;
%rename (ScalProdEquality) Solver::MakeScalProdEquality;
%rename (ScalProdGreaterOrEqual) Solver::MakeScalProdGreaterOrEqual;
%rename (ScalProdLessOrEqual) Solver::MakeScalProdLessOrEqual;
%rename (MinEquality) Solver::MakeMinEquality;
%rename (MaxEquality) Solver::MakeMaxEquality;
%rename (ElementEquality) Solver::MakeElementEquality;
%rename (AbsEquality) Solver::MakeAbsEquality;
%rename (IndexOfConstraint) Solver::MakeIndexOfConstraint;

// Solver: Constraints about interval variables.
%rename (FixedDurationIntervalVar) Solver::MakeFixedDurationIntervalVar;
%rename (FixedInterval) Solver::MakeFixedInterval;
%rename (MirrorInterval) Solver::MakeMirrorInterval;
%rename (FixedDurationStartSyncedOnStartIntervalVar)
    Solver::MakeFixedDurationStartSyncedOnStartIntervalVar;
%rename (FixedDurationEndSyncedOnEndIntervalVar)
    Solver::MakeFixedDurationEndSyncedOnEndIntervalVar;
%rename (IntervalRelaxedMin) Solver::MakeIntervalRelaxedMin;
%rename (IntervalRelaxedMax) Solver::MakeIntervalRelaxedMax;
%rename (TemporalDisjunction) Solver::MakeTemporalDisjunction;
%rename (Cover) Solver::MakeCover;

// Solver: Constraints tying a boolean var to an expression. Model-wise; it is
// equivalent to creating the expression via the python natural API; and then
// declaring its equality to the boolean var.
%rename (IsBetweenVar) Solver::MakeIsBetweenVar;
%rename (IsDifferentCstVar) Solver::MakeIsDifferentCstVar;
%rename (IsDifferentVar) Solver::MakeIsDifferentVar;
%rename (IsEqualCstVar) Solver::MakeIsEqualCstVar;
%rename (IsEqualVar) Solver::MakeIsEqualVar;
%rename (IsGreaterCstVar) Solver::MakeIsGreaterCstVar;
%rename (IsGreaterOrEqualCstVar) Solver::MakeIsGreaterOrEqualCstVar;
%rename (IsGreaterOrEqualVar) Solver::MakeIsGreaterOrEqualVar;
%rename (IsGreaterVar) Solver::MakeIsGreaterVar;
%rename (IsLessCstVar) Solver::MakeIsLessCstVar;
%rename (IsLessOrEqualCstVar) Solver::MakeIsLessOrEqualCstVar;
%rename (IsLessOrEqualVar) Solver::MakeIsLessOrEqualVar;
%rename (IsLessVar) Solver::MakeIsLessVar;
%rename (IsMemberVar) Solver::MakeIsMemberVar;
// The methods below should be avoided: use the *Var versions above if you can.
%rename (IsBetweenCt) Solver::MakeIsBetweenCt;
%rename (IsDifferentCstCt) Solver::MakeIsDifferentCstCt;
%rename (IsDifferentCt) Solver::MakeIsDifferentCt;
%rename (IsEqualCstCt) Solver::MakeIsEqualCstCt;
%rename (IsEqualCt) Solver::MakeIsEqualCt;
%rename (IsGreaterCstCt) Solver::MakeIsGreaterCstCt;
%rename (IsGreaterCt) Solver::MakeIsGreaterCt;
%rename (IsGreaterOrEqualCstCt) Solver::MakeIsGreaterOrEqualCstCt;
%rename (IsGreaterOrEqualCt) Solver::MakeIsGreaterOrEqualCt;
%rename (IsLessCstCt) Solver::MakeIsLessCstCt;
%rename (IsLessCt) Solver::MakeIsLessCt;
%rename (IsLessOrEqualCstCt) Solver::MakeIsLessOrEqualCstCt;
%rename (IsLessOrEqualCt) Solver::MakeIsLessOrEqualCt;
%rename (IsMemberCt) Solver::MakeIsMemberCt;


// Solver: Elaborate constraint creation.
%rename (Count) Solver::MakeCount;
%rename (Deviation) Solver::MakeDeviation;
%rename (SortingConstraint) Solver::MakeSortingConstraint;
%rename (LexicalLess) Solver::MakeLexicalLess;
%rename (LexicalLessOrEqual) Solver::MakeLexicalLessOrEqual;
%rename (InversePermutationConstraint) Solver::MakeInversePermutationConstraint;
%rename (NullIntersect) Solver::MakeNullIntersect;
%rename (NullIntersectExcept) Solver::MakeNullIntersectExcept;
%rename (Circuit) Solver::MakeCircuit;
%rename (MemberCt) Solver::MakeMemberCt;
%rename (SubCircuit) Solver::MakeSubCircuit;
%rename (PathCumul) Solver::MakePathCumul;
%rename (DelayedPathCumul) Solver::MakeDelayedPathCumul;
%rename (TransitionConstraint) Solver::MakeTransitionConstraint;
%rename (NonOverlappingBoxesConstraint)
    Solver::MakeNonOverlappingBoxesConstraint;
%rename (Pack) Solver::MakePack;

// Solver: Other object creation.
%rename (Assignment) Solver::MakeAssignment;

// Solver: Demon creation and demon-related methods.
%unignore Solver::ShouldFail;
%rename (ConstraintInitialPropagateCallback)
    Solver::MakeConstraintInitialPropagateCallback;
%rename (DelayedConstraintInitialPropagateCallback)
    Solver::MakeDelayedConstraintInitialPropagateCallback;


// Solver: Solution Collectors
%rename (BestValueSolutionCollector) Solver::MakeBestValueSolutionCollector;
%rename (FirstSolutionCollector) Solver::MakeFirstSolutionCollector;
%rename (LastSolutionCollector) Solver::MakeLastSolutionCollector;
%rename (AllSolutionCollector) Solver::MakeAllSolutionCollector;

// Solver: Objective variables, i.e. OptimizeVar creation.
%rename (Minimize) Solver::MakeMinimize;
%rename (Maximize) Solver::MakeMaximize;
%rename (Optimize) Solver::MakeOptimize;
%rename (WeightedMinimize) Solver::MakeWeightedMinimize;
%rename (WeightedMaximize) Solver::MakeWeightedMaximize;
%rename (WeightedOptimize) Solver::MakeWeightedOptimize;

// Solver: Meta-heuristics.
%rename (TabuSearch) Solver::MakeTabuSearch;
%rename (SimulatedAnnealing) Solver::MakeSimulatedAnnealing;
%rename (GuidedLocalSearch) Solver::MakeGuidedLocalSearch;
%rename (LubyRestart) Solver::MakeLubyRestart;
%rename (ConstantRestart) Solver::MakeConstantRestart;

// Solver: Search Limits.
%unignore Solver::SearchLimitProto;  // search_limit.proto
%rename (Limit) Solver::MakeLimit;
%rename (TimeLimit) Solver::MakeTimeLimit;
%rename (BranchesLimit) Solver::MakeBranchesLimit;
%rename (FailuresLimit) Solver::MakeFailuresLimit;
%rename (SolutionsLimit) Solver::MakeSolutionsLimit;
%rename (CustomLimit) Solver::MakeCustomLimit;

// Solver: Search logs.
%rename (SearchLog) Solver::MakeSearchLog;
%rename (SearchTrace) Solver::MakeSearchTrace;

// Solver: Model visitors.
%unignore Solver::Accept;
%rename (PrintModelVisitor) Solver::MakePrintModelVisitor;
%rename (StatisticsModelVisitor) Solver::MakeStatisticsModelVisitor;

// Solver: Decisions
%rename (SplitVariableDomain) Solver::MakeSplitVariableDomain;
%rename (AssignVariableValue) Solver::MakeAssignVariableValue;
%rename (VariableLessOrEqualValue) Solver::MakeVariableLessOrEqualValue;
%rename (VariableGreaterOrEqualValue) Solver::MakeVariableGreaterOrEqualValue;
%rename (AssignVariableValueOrFail) Solver::MakeAssignVariableValueOrFail;
%rename (AssignVariablesValues) Solver::MakeAssignVariablesValues;
%rename (FailDecision) Solver::MakeFailDecision;

// Solver: Decision builders. Many versions of MakePhase() are not exposed
// directly; instead there are python-specific shortcuts provided above.
// See the occurrences of "DecisionBuilder*" in this file.
%unignore Solver::Try(const std::vector<DecisionBuilder*>&);
%unignore Solver::Compose(const std::vector<DecisionBuilder*>&);
%rename (SolveOnce) Solver::MakeSolveOnce(DecisionBuilder* const,
                                          const std::vector<SearchMonitor*>&);
%rename (Phase) Solver::MakePhase(const std::vector<IntVar*>&,
                                  IntVarStrategy, IntValueStrategy);
%rename (Phase) Solver::MakePhase(const std::vector<IntervalVar*>&,
                                  IntervalStrategy);
%rename (Phase) Solver::MakePhase(const std::vector<SequenceVar*>&,
                                  SequenceStrategy);
%rename (DefaultPhase) Solver::MakeDefaultPhase;
%rename (LocalSearchPhase) Solver::MakeLocalSearchPhase;
%rename (ScheduleOrPostpone) Solver::MakeScheduleOrPostpone;
%rename (ScheduleOrExpedite) Solver::MakeScheduleOrExpedite;
%rename (RankFirstInterval) Solver::MakeRankFirstInterval;
%rename (RankLastInterval) Solver::MakeRankLastInterval;
%rename (DecisionBuilderFromAssignment)
    Solver::MakeDecisionBuilderFromAssignment;
%rename (ConstraintAdder) Solver::MakeConstraintAdder;
%rename (NestedOptimize) Solver::MakeNestedOptimize;
%rename (StoreAssignment) Solver::MakeStoreAssignment;
%rename (RestoreAssignment) Solver::MakeRestoreAssignment;

// Solver: Local search operators.
%rename (Operator) Solver::MakeOperator;
%rename (RandomLNSOperator) Solver::MakeRandomLNSOperator;
%unignore Solver::ConcatenateOperators;
%unignore Solver::RandomConcatenateOperators;
%rename (LocalSearchPhaseParameters) Solver::MakeLocalSearchPhaseParameters;
%rename (MoveTowardTargetOperator) Solver::MakeMoveTowardTargetOperator;
%rename (NeighborhoodLimit) Solver::MakeNeighborhoodLimit;

// Enums. Each section below exposes one enum, with all its exposed values.
%unignore Solver::IntVarStrategy;
%unignore Solver::INT_VAR_DEFAULT;
%unignore Solver::INT_VAR_SIMPLE;
%unignore Solver::CHOOSE_FIRST_UNBOUND;
%unignore Solver::CHOOSE_RANDOM;
%unignore Solver::CHOOSE_MIN_SIZE_LOWEST_MIN;
%unignore Solver::CHOOSE_MIN_SIZE_HIGHEST_MIN;
%unignore Solver::CHOOSE_MIN_SIZE_LOWEST_MAX;
%unignore Solver::CHOOSE_MIN_SIZE_HIGHEST_MAX;
%unignore Solver::CHOOSE_LOWEST_MIN;
%unignore Solver::CHOOSE_HIGHEST_MAX;
%unignore Solver::CHOOSE_MIN_SIZE;
%unignore Solver::CHOOSE_MAX_SIZE;
%unignore Solver::CHOOSE_MAX_REGRET_ON_MIN;
%unignore Solver::CHOOSE_PATH;

%unignore Solver::IntValueStrategy;
%unignore Solver::INT_VALUE_DEFAULT;
%unignore Solver::INT_VALUE_SIMPLE;
%unignore Solver::ASSIGN_MIN_VALUE;
%unignore Solver::ASSIGN_MAX_VALUE;
%unignore Solver::ASSIGN_RANDOM_VALUE;
%unignore Solver::ASSIGN_CENTER_VALUE;
%unignore Solver::SPLIT_LOWER_HALF;
%unignore Solver::SPLIT_UPPER_HALF;

%unignore Solver::SequenceStrategy;
%unignore Solver::SEQUENCE_DEFAULT;
%unignore Solver::SEQUENCE_SIMPLE;
%unignore Solver::CHOOSE_MIN_SLACK_RANK_FORWARD;
%unignore Solver::CHOOSE_RANDOM_RANK_FORWARD;

%unignore Solver::IntervalStrategy;
%unignore Solver::INTERVAL_DEFAULT;
%unignore Solver::INTERVAL_SIMPLE;
%unignore Solver::INTERVAL_SET_TIMES_FORWARD;
%unignore Solver::INTERVAL_SET_TIMES_BACKWARD;

%unignore Solver::LocalSearchOperators;
%unignore Solver::TWOOPT;
%unignore Solver::OROPT;
%unignore Solver::RELOCATE;
%unignore Solver::EXCHANGE;
%unignore Solver::CROSS;
%unignore Solver::MAKEACTIVE;
%unignore Solver::MAKEINACTIVE;
%unignore Solver::MAKECHAININACTIVE;
%unignore Solver::SWAPACTIVE;
%unignore Solver::EXTENDEDSWAPACTIVE;
%unignore Solver::PATHLNS;
%unignore Solver::FULLPATHLNS;
%unignore Solver::UNACTIVELNS;
%unignore Solver::INCREMENT;
%unignore Solver::DECREMENT;
%unignore Solver::SIMPLELNS;

%unignore Solver::LocalSearchFilterBound;
%unignore Solver::GE;
%unignore Solver::LE;
%unignore Solver::EQ;

%unignore Solver::LocalSearchOperation;
%unignore Solver::SUM;
%unignore Solver::PROD;
%unignore Solver::MAX;
%unignore Solver::MIN;

}  // namespace operations_research

// ============= Unexposed C++ API : Solver class ==============
// TODO(user): remove this listing of unexposed methods (which was exhaustive
// as of 2014-07-01) when we are confident with the stability of the or-tools
// export. Until then, it is an extremely useful reference for development.
//
// Unexposed Solver:: methods (grouped semantically):
//
// - parameters()
// - SaveValue()
// - RevAlloc()
// - RevAllocArray()
//
// - AddCastConstraint()
//
// - state()
//
// - ExportModel()
// - LoadModel()
// - UpgradeModel()
//
// TODO(user): implement backtrack actions with std::function and experiment
// swig with that.
// - AddBacktrackAction()
//
// - DebugString()
// - VirtualMemorySize()
// - SetClock()
//
// - demon_runs()
// - neighbors()
// - filtered_neighbors()
//
// - MakeIntVarArray()
// - MakeBoolVarArray()
//
// - MakeCallbackDemon()
//
// - MakeFixedDurationIntervalVarArray()
// - MakeIntervalVarArray()
//
// - MakeEquality()  // On IntervalVar.
//
// - UpdateLimits()
// - GetTime()
//
// - MakeNoGoodManager()
//
// - MakeTreeMonitor()
//
// - MakeVariableDegreeVisitor()
//
// - MakeSymmetryManager()
//
// - MakeSimplexConstraint()
//
// - MakeDefaultSolutionPool()
// - MakeVariableDomainFilter()
//
// - TopPeriodicCheck()
// - TopProgressPercent()
// - PushState()
// - PopState()
//
// - SetBranchSelector()
// - MakeApplyBranchSelector()
//
// - SaveAndSetValue()
// - SaveAndAdd()
//
// - Rand64()
// - Rand32()
// - ReSeed()
//
// - AddFailHook()
// - ExportProfilingOverview()
// - CurrentlyInSolve()
// - balancing_decision()
// - set_fail_intercept()
// - clear_fail_intercept()
// - demon_profiler()
// - HasName()
//
// - RegisterDemon()
// - RegisterIntExpr()
// - RegisterIntVar()
// - RegisterIntervalVar()
//
// - ActiveSearch()
// - Cache()
// - InstrumentsDemons()
// - IsProfilingEnabled()
// - InstrumentsVariables()
// - NameAllVariables()
// - model_name()
// - Graph()
// - GetPropagationMonitor()
// - AddPropagationMonitor()
// - tmp_vector_
// - IsBooleanVar()
// - IsProduct()
// - CastExpression()

// Unexposed Solver enums:
// - EvaluatorLocalSearchOperators
// - DemonPriority
// - BinaryIntervalRelation (replaced by an ad-hoc API on IntervalVar).
// - UnaryIntervalRelation (ditto)
// - DecisionModification
// - MarkerType
// - SolverState

// ============= Exposed C++ API : classes other than "Solver" ==============

namespace operations_research {

// Unexposed top-level classes and methods:
// - Zero()
// - BaseObject
// - DecisionVisitor
// - Action
// - ModelVisitor
// - CastConstraint
// - NumericalRev<>
// - RevArray<>
// - NumericalRevArray<>
// - NoGood
// - NoGoodManager
// - AssignmentElement
// - SolutionPool

// SolverParameters
// Ignored:
// - Constants:
//   - kDefaultTrailCompression
//   - kDefaultTrailBlockSize
//   - kDefaultArraySplitSize
//   - kDefaultNameStoring
//   - kDefaultProfileLevel
//   - kDefaultTraceLevel
//   - kDefaultNameAllVariables
%unignore SolverParameters;
%unignore SolverParameters::SolverParameters;

// SolverParameters: Enums.
%unignore SolverParameters::TrailCompression;
%unignore SolverParameters::NO_COMPRESSION;
%unignore SolverParameters::COMPRESS_WITH_ZLIB;
%unignore SolverParameters::ProfileLevel;
%unignore SolverParameters::NO_PROFILING;
%unignore SolverParameters::NORMAL_PROFILING;
%unignore SolverParameters::TraceLevel;
%unignore SolverParameters::NO_TRACE;
%unignore SolverParameters::NORMAL_TRACE;

// SolverParameters: data members.
%unignore SolverParameters::compress_trail;
%unignore SolverParameters::trail_block_size;
%unignore SolverParameters::array_split_size;
%unignore SolverParameters::store_names;
%unignore SolverParameters::profile_level;
%unignore SolverParameters::trace_level;
%unignore SolverParameters::name_all_variables;

// DefaultPhaseParameters
// Ignored:
// - Constants:
//   - kDefaultNumberOfSplits
//   - kDefaultHeuristicPeriod
//   - kDefaultHeuristicNumFailuresLimit
//   - kDefaultSeed
//   - kDefaultRestartLogSize
//   - kDefaultUseNoGoods
%unignore DefaultPhaseParameters;
%unignore DefaultPhaseParameters::DefaultPhaseParameters;

// DefaultPhaseParameters: Enums.
%unignore DefaultPhaseParameters::VariableSelection;
%unignore DefaultPhaseParameters::CHOOSE_MAX_SUM_IMPACT;
%unignore DefaultPhaseParameters::CHOOSE_MAX_AVERAGE_IMPACT;
%unignore DefaultPhaseParameters::CHOOSE_MAX_VALUE_IMPACT;
%unignore DefaultPhaseParameters::ValueSelection;
%unignore DefaultPhaseParameters::SELECT_MIN_IMPACT;
%unignore DefaultPhaseParameters::SELECT_MAX_IMPACT;
%unignore DefaultPhaseParameters::DisplayLevel;
%unignore DefaultPhaseParameters::NONE;
%unignore DefaultPhaseParameters::NORMAL;
%unignore DefaultPhaseParameters::VERBOSE;

// DefaultPhaseParameters: data members.
%unignore DefaultPhaseParameters::var_selection_schema;
%unignore DefaultPhaseParameters::value_selection_schema;
%unignore DefaultPhaseParameters::initialization_splits;
%unignore DefaultPhaseParameters::run_all_heuristics;
%unignore DefaultPhaseParameters::heuristic_period;
%unignore DefaultPhaseParameters::heuristic_num_failures_limit;
%unignore DefaultPhaseParameters::persistent_impact;
%unignore DefaultPhaseParameters::random_seed;
%unignore DefaultPhaseParameters::restart_log_size;
%unignore DefaultPhaseParameters::display_level;
%unignore DefaultPhaseParameters::use_no_goods;
%unignore DefaultPhaseParameters::decision_builder;

// PropagationBaseObject
// Ignored:
// - PropagationBaseObject()
// - ~PropagationBaseObject()
// - DebugString()
// - FreezeQueue()
// - UnfreezeQueue()
// - EnqueueDelayedDemon()
// - EnqueueVar()
// - Execute()
// - ExecuteAll()
// - EnqueueAll()
// - set_queue_action_on_fail()
// - clear_queue_action_on_fail()
// - set_name()
// - HasName()
// - Basename()
%unignore PropagationBaseObject;
%rename (Name) PropagationBaseObject::name;
%rename (solver) PropagationBaseObject::solver;

// Decision
// Ignored:
// - DebugString()
// - Accept()
%unignore Decision;
%unignore Decision::Decision;
%unignore Decision::~Decision;
%unignore Decision::Apply;
%unignore Decision::Refute;


// DecisionBuilder
// Ignored:
// - DebugString()
%unignore DecisionBuilder;
%unignore DecisionBuilder::DecisionBuilder;
%unignore DecisionBuilder::~DecisionBuilder;
%unignore DecisionBuilder::Next;


// Constraint
// Ignored:
// - Post()
// - DebugString()
// - PostAndPropagate()
// - Accept()
// - IsCastConstraint()
%unignore Constraint;
%unignore Constraint::Var;
// Note(user): we prefer setting the 'director' feature on the individual
// methods of a class that require it, but sometimes we must actually set
// 'director' on the class itself, because it is a C++ abstract class and
// the client needs to construct it. In these cases, we don't bother
// setting the 'director' feature on individual methods, since it is done
// automatically when setting it on the class.
%feature("director") Constraint;
%unignore Constraint::Constraint;
%unignore Constraint::~Constraint;
%unignore Constraint::solver;
%unignore Constraint::Post;
%unignore Constraint::InitialPropagate;
%rename (InitialPropagateWrapper) Constraint::InitialPropagate;


// SearchMonitor.
// Ignored:
// - kNoProgress
// - PeriodicCheck()
// - ProgressPercent()
// - Accept()
// - Install()
%unignore SearchMonitor;
%unignore SearchMonitor::SearchMonitor;
%unignore SearchMonitor::~SearchMonitor;
%unignore SearchMonitor::EnterSearch;
%feature("director") SearchMonitor::EnterSearch;
%unignore SearchMonitor::RestartSearch;
%feature("director") SearchMonitor::RestartSearch;
%unignore SearchMonitor::ExitSearch;
%feature("director") SearchMonitor::ExitSearch;
%unignore SearchMonitor::BeginNextDecision;
%feature("director") SearchMonitor::BeginNextDecision;
%unignore SearchMonitor::EndNextDecision;
%feature("director") SearchMonitor::EndNextDecision;
%unignore SearchMonitor::ApplyDecision;
%feature("director") SearchMonitor::ApplyDecision;
%unignore SearchMonitor::RefuteDecision;
%feature("director") SearchMonitor::RefuteDecision;
%unignore SearchMonitor::AfterDecision;
%feature("director") SearchMonitor::AfterDecision;
%unignore SearchMonitor::BeginFail;
%feature("director") SearchMonitor::BeginFail;
%unignore SearchMonitor::EndFail;
%feature("director") SearchMonitor::EndFail;
%unignore SearchMonitor::BeginInitialPropagation;
%feature("director") SearchMonitor::BeginInitialPropagation;
%unignore SearchMonitor::EndInitialPropagation;
%feature("director") SearchMonitor::EndInitialPropagation;
%unignore SearchMonitor::AcceptSolution;
%feature("director") SearchMonitor::AcceptSolution;
%unignore SearchMonitor::AtSolution;
%feature("director") SearchMonitor::AtSolution;
%unignore SearchMonitor::NoMoreSolutions;
%feature("director") SearchMonitor::NoMoreSolutions;
%unignore SearchMonitor::LocalOptimum;
%feature("director") SearchMonitor::LocalOptimum;
%unignore SearchMonitor::AcceptDelta;
%feature("director") SearchMonitor::AcceptDelta;
%unignore SearchMonitor::AcceptNeighbor;
%feature("director") SearchMonitor::AcceptNeighbor;
%rename (solver) SearchMonitor::solver;


// Rev<>
%unignore Rev;
%unignore Rev::Rev;
%unignore Rev::Value;
%unignore Rev::SetValue;

// IntExpr
// Ignored:
// - IntExpr()
// - ~IntExpr()
// - Accept()
%unignore IntExpr;
%unignore IntExpr::Min;
%unignore IntExpr::Max;
%unignore IntExpr::Bound;
%unignore IntExpr::SetValue;
%unignore IntExpr::SetMin;
%unignore IntExpr::SetMax;
%unignore IntExpr::SetRange;
%unignore IntExpr::Var;
%unignore IntExpr::IsVar;
%unignore IntExpr::VarWithName;
%unignore IntExpr::WhenRange;

// IntVar
// Ignored:
// - IntVar()
// - ~IntVar()
// - VarType()
// - Accept()
// - IsEqual()
// - IsDifferent()
// - IsGreaterOrEqual()
// - IsLessOrEqual()
%unignore IntVar;
%unignore IntVar::Value;
%unignore IntVar::Size;
%unignore IntVar::Contains;
%unignore IntVar::RemoveValue;
%unignore IntVar::RemoveValues;
%unignore IntVar::RemoveInterval;
%unignore IntVar::SetValues;
%unignore IntVar::Var;
%unignore IntVar::IsVar;
%unignore IntVar::WhenBound;
%unignore IntVar::WhenDomain;
%rename (HoleIteratorAux) IntVar::MakeHoleIterator;
%feature("director") IntVar::MakeHoleIterator;
%rename (DomainIteratorAux) IntVar::MakeDomainIterator;
%feature("director") IntVar::MakeDomainIterator;
%unignore IntVar::OldMin;
%unignore IntVar::OldMax;


// SolutionCollector.
// Ignored:
// - SolutionCollector()
// - ~SolutionCollector()
// - EnterSearch()
%unignore SolutionCollector;
%unignore SolutionCollector::Value;
%unignore SolutionCollector::StartValue;
%unignore SolutionCollector::EndValue;
%unignore SolutionCollector::DurationValue;
%unignore SolutionCollector::PerformedValue;
%unignore SolutionCollector::ForwardSequence;
%unignore SolutionCollector::BackwardSequence;
%unignore SolutionCollector::Unperformed;
%rename (Solution) SolutionCollector::solution;
%rename (SolutionCount) SolutionCollector::solution_count;
%rename (ObjectiveValue) SolutionCollector::objective_value;
%rename (WallTime) SolutionCollector::wall_time;
%rename (Branches) SolutionCollector::branches;
%rename (Failures) SolutionCollector::failures;
%unignore SolutionCollector::Add;
%unignore SolutionCollector::AddObjective;


// OptimizeVar.
// Ignored:
// - OptimizeVar()
// - ~OptimizeVat()
// - EnterSearch()
// - BeginNextDecision()
// - RefuteDecision()
// - AtSolution()
// - AcceptSolution()
// - Print()
// - DebugString()
// - Accept()
// - ApplyBound()
%unignore OptimizeVar;
%rename (Best) OptimizeVar::best;
%unignore OptimizeVar::Var;

// SearchLimit.
// Ignored:
// - SearchLimit()
// - ~SearchLimit()
// - crossed()
// - Check()
// - Init()
// - Copy()
// - MakeClone()
// - EnterSearch()
// - BeginNextDecision()
// - PeriodicCheck()
// - RefuteDecision()
// - DebugString()
%unignore SearchLimit;
%rename (Crossed) SearchLimit::crossed;
%unignore SearchLimit::SearchLimit;
%unignore SearchLimit::~SearchLimit;
%unignore SearchLimit::Check;
%unignore SearchLimit::Init;


// IntervalVar
// Ignored:
// - kMinValidValue
// - kMaxValidValue
// - IntervalVar()
// - ~IntervalVar()
// - Accept()
%unignore IntervalVar;
%unignore IntervalVar::StartExpr;
%unignore IntervalVar::DurationExpr;
%unignore IntervalVar::EndExpr;
%unignore IntervalVar::SafeStartExpr;
%unignore IntervalVar::SafeDurationExpr;
%unignore IntervalVar::SafeEndExpr;
%unignore IntervalVar::PerformedExpr;
%unignore IntervalVar::StartMin;
%unignore IntervalVar::StartMax;
%unignore IntervalVar::SetStartMin;
%unignore IntervalVar::SetStartMax;
%unignore IntervalVar::SetStartRange;
%unignore IntervalVar::DurationMin;
%unignore IntervalVar::DurationMax;
%unignore IntervalVar::SetDurationMin;
%unignore IntervalVar::SetDurationMax;
%unignore IntervalVar::SetDurationRange;
%unignore IntervalVar::EndMin;
%unignore IntervalVar::EndMax;
%unignore IntervalVar::SetEndMin;
%unignore IntervalVar::SetEndMax;
%unignore IntervalVar::SetEndRange;
%unignore IntervalVar::MustBePerformed;
%unignore IntervalVar::MayBePerformed;
%unignore IntervalVar::CannotBePerformed;
%unignore IntervalVar::SetPerformed;
%unignore IntervalVar::IsPerformedBound;
%unignore IntervalVar::OldStartMin;
%unignore IntervalVar::OldStartMax;
%unignore IntervalVar::OldDurationMin;
%unignore IntervalVar::OldDurationMax;
%unignore IntervalVar::OldEndMin;
%unignore IntervalVar::OldEndMax;
%unignore IntervalVar::WhenStartRange;
%unignore IntervalVar::WhenStartBound;
%unignore IntervalVar::WhenDurationRange;
%unignore IntervalVar::WhenDurationBound;
%unignore IntervalVar::WhenEndRange;
%unignore IntervalVar::WhenEndBound;
%unignore IntervalVar::WasPerformedBound;
%unignore IntervalVar::WhenPerformedBound;
%unignore IntervalVar::WhenAnything;


// SequenceVar.
// Ignored:
// - SequenceVar()
// - ~SequenceVar()
// - DebugString()
// - DurationRange()
// - HorizonRange()
// - ActiveHorizonRange()
// - ComputeStatistics()
// - ComputePossibleFirstsAndLasts()
// - RankSequence()
// - FillSequence()
// - Accept()
%unignore SequenceVar;
%unignore SequenceVar::RankFirst;
%unignore SequenceVar::RankNotFirst;
%unignore SequenceVar::RankLast;
%unignore SequenceVar::RankNotLast;
%unignore SequenceVar::Interval;
%unignore SequenceVar::Next;
%rename (Size) SequenceVar::size;

// Assignment
// Ignored:
// - Assignment()
// - ~Assignment()
// - FastAdd()
// - DebugString()
// - Contains()
// - Copy()
// - ==()
// - !=()
%unignore Assignment;
%unignore Assignment::Clear;
%unignore Assignment::Size;
%unignore Assignment::Empty;
%unignore Assignment::NumIntVars;
%unignore Assignment::NumIntervalVars;
%unignore Assignment::NumSequenceVars;
%unignore Assignment::Add;
%unignore Assignment::Store;
%unignore Assignment::Restore;
%unignore Assignment::Load;
%unignore Assignment::Save;

// Assignment: activate/deactivate.
%unignore Assignment::Activate;
%unignore Assignment::Deactivate;
%unignore Assignment::Activated;
%unignore Assignment::ObjectiveActivate;
%unignore Assignment::ObjectiveDeactivate;
%unignore Assignment::ObjectiveActivated;

// Assignment: Objective API.
%unignore Assignment::HasObjective;
%unignore Assignment::Objective;
%unignore Assignment::AddObjective;
%unignore Assignment::ObjectiveMin;
%unignore Assignment::ObjectiveMax;
%unignore Assignment::ObjectiveValue;
%unignore Assignment::ObjectiveBound;
%unignore Assignment::SetObjectiveMin;
%unignore Assignment::SetObjectiveMax;
%unignore Assignment::SetObjectiveValue;
%unignore Assignment::SetObjectiveRange;

// Assignment: IntVar API.
%unignore Assignment::Min;
%unignore Assignment::Max;
%unignore Assignment::Value;
%unignore Assignment::Bound;
%unignore Assignment::SetMin;
%unignore Assignment::SetMax;
%unignore Assignment::SetRange;

// Assignment: IntervalVar API.
%unignore Assignment::SetValue;
%unignore Assignment::StartMin;
%unignore Assignment::StartMax;
%unignore Assignment::StartValue;
%unignore Assignment::DurationMin;
%unignore Assignment::DurationMax;
%unignore Assignment::DurationValue;
%unignore Assignment::EndMin;
%unignore Assignment::EndMax;
%unignore Assignment::EndValue;
%unignore Assignment::PerformedMin;
%unignore Assignment::PerformedMax;
%unignore Assignment::PerformedValue;
%unignore Assignment::SetStartMin;
%unignore Assignment::SetStartMax;
%unignore Assignment::SetStartRange;
%unignore Assignment::SetStartValue;
%unignore Assignment::SetDurationMin;
%unignore Assignment::SetDurationMax;
%unignore Assignment::SetDurationRange;
%unignore Assignment::SetDurationValue;
%unignore Assignment::SetEndMin;
%unignore Assignment::SetEndMax;
%unignore Assignment::SetEndValue;
%unignore Assignment::SetEndRange;
%unignore Assignment::SetPerformedMin;
%unignore Assignment::SetPerformedMax;
%unignore Assignment::SetPerformedRange;
%unignore Assignment::SetPerformedValue;

// Assignment: SequenceVar API.
%unignore Assignment::ForwardSequence;
%unignore Assignment::BackwardSequence;
%unignore Assignment::Unperformed;
%unignore Assignment::SetForwardSequence;
%unignore Assignment::SetBackwardSequence;
%unignore Assignment::SetUnperformed;
%unignore Assignment::SetSequence;

// Assignment: underlying containers.
%unignore Assignment::IntVarContainer;
%unignore Assignment::MutableIntVarContainer;
%unignore Assignment::IntervalVarContainer;
%unignore Assignment::MutableIntervalVarContainer;
%unignore Assignment::SequenceVarContainer;
%unignore Assignment::MutableSequenceVarContainer;


// DisjunctiveConstraint
// Ignored:
// - DisjunctiveConstraint()
// - ~DisjunctiveConstraint()
%unignore DisjunctiveConstraint;
%rename (SequenceVar) DisjunctiveConstraint::MakeSequenceVar;
%unignore DisjunctiveConstraint::SetTransitionTime;
%unignore DisjunctiveConstraint::TransitionTime;

// Pack (Constraint)
// Ignored:
// - typedefs:
//   - ItemUsageEvaluator
//   - ItemUsagePerBinEvaluator
// - Pack()
// - ~Pack()
// - Post()
// - ClearAll()
// - PropagateDelayed()
// - InitialPropagate()
// - Propagate()
// - OneDomain()
// - DebugString()
// - IsUndecided()
// - SetImpossible()
// - Assign()
// - IsAssignedStatusKnown()
// - IsPossible()
// - AssignVar()
// - SetAssigned()
// - SetUnassigned()
// - RemoveAllPossibleFromBin()
// - AssignAllPossibleToBin()
// - AssignFirstPossibleToBin()
// - AssignAllRemainingItems()
// - UnassignAllRemainingItems()
// - Accept()
%unignore Pack;
%unignore Pack::AddWeightedSumLessOrEqualConstantDimension;
%unignore Pack::AddWeightedSumLessOrEqualConstantDimension;
%unignore Pack::AddWeightedSumLessOrEqualConstantDimension;
%unignore Pack::AddWeightedSumEqualVarDimension;
%unignore Pack::AddWeightedSumEqualVarDimension;
%unignore Pack::AddSumVariableWeightsLessOrEqualConstantDimension;
%unignore Pack::AddWeightedSumOfAssignedDimension;
%unignore Pack::AddCountUsedBinDimension;
%unignore Pack::AddCountAssignedItemsDimension;

// LocalSearchPhaseParameters
%unignore LocalSearchPhaseParameters;

// Demon
// Ignored:
// - priority()
// - DebugString()
%feature("director") Demon;
%unignore Demon;
%unignore Demon::Demon;
%unignore Demon::~Demon;
%rename (RunWrapper) Demon::Run;
%rename (Inhibit) Demon::inhibit;
%rename (Desinhibit) Demon::desinhibit;

// AssignmentElement
// Ignored:
// - AssignmentElement()
%unignore AssignmentElement;
%unignore AssignmentElement::Activate;
%unignore AssignmentElement::Deactivate;
%unignore AssignmentElement::Activated;

// IntVarElement
// Ignored:
// - IntVarElement()
// - ~IntVarElement()
// - Reset()
// - Clone()
// - Copy()
// - Store()
// - Restore()
// - LoadFromProto()
// - WriteToProto()
// - DebugString()
// - operator==()
// - operator!=()
%unignore IntVarElement;
%unignore IntVarElement::Min;
%unignore IntVarElement::SetMin;
%unignore IntVarElement::Max;
%unignore IntVarElement::SetMax;
%unignore IntVarElement::Value;
%unignore IntVarElement::Bound;
%unignore IntVarElement::SetRange;
%unignore IntVarElement::SetValue;
%unignore IntVarElement::Var;

// IntervalVarElement
// Ignored:
// - IntervalVarElement()
// - ~IntervalVarElement()
// - Reset()
// - Clone()
// - Copy()
// - Var()
// - Store()
// - Restore()
// - LoadFromProto()
// - WriteToProto()
// - DebugString()
// - operator==()
// - operator!=()
%unignore IntervalVarElement;
%unignore IntervalVarElement::StartMin;
%unignore IntervalVarElement::StartMax;
%unignore IntervalVarElement::StartValue;
%unignore IntervalVarElement::DurationMin;
%unignore IntervalVarElement::DurationMax;
%unignore IntervalVarElement::DurationValue;
%unignore IntervalVarElement::EndMin;
%unignore IntervalVarElement::EndMax;
%unignore IntervalVarElement::EndValue;
%unignore IntervalVarElement::PerformedMin;
%unignore IntervalVarElement::PerformedMax;
%unignore IntervalVarElement::PerformedValue;
%unignore IntervalVarElement::SetStartMin;
%unignore IntervalVarElement::SetStartMax;
%unignore IntervalVarElement::SetStartRange;
%unignore IntervalVarElement::SetStartValue;
%unignore IntervalVarElement::SetDurationMin;
%unignore IntervalVarElement::SetDurationMax;
%unignore IntervalVarElement::SetDurationRange;
%unignore IntervalVarElement::SetDurationValue;
%unignore IntervalVarElement::SetEndMin;
%unignore IntervalVarElement::SetEndMax;
%unignore IntervalVarElement::SetEndRange;
%unignore IntervalVarElement::SetEndValue;
%unignore IntervalVarElement::SetPerformedMin;
%unignore IntervalVarElement::SetPerformedMax;
%unignore IntervalVarElement::SetPerformedRange;
%unignore IntervalVarElement::SetPerformedValue;

// SequenceVarElement
// Ignored:
// - SequenceVarElement()
// - ~SequenceVarElement()
// - Reset()
// - Clone()
// - Copy()
// - Var()
// - Store()
// - Restore()
// - LoadFromProto()
// - WriteToProto()
// - DebugString()
// - operator==()
// - operator!=()
%unignore SequenceVarElement;
%unignore SequenceVarElement::ForwardSequence;
%unignore SequenceVarElement::BackwardSequence;
%unignore SequenceVarElement::Unperformed;
%unignore SequenceVarElement::SetSequence;
%unignore SequenceVarElement::SetForwardSequence;
%unignore SequenceVarElement::SetBackwardSequence;
%unignore SequenceVarElement::SetUnperformed;

// AssignmentContainer<>
// Ignored:
// - AssignmentContainer()
// - Add()
// - FastAdd()
// - AddAtPosition()
// - Clear()
// - Resize()
// - Empty()
// - Copy()
// - elements()
// - All Element() method taking (const V* const var)
// - operator==()
// - operator!=()
%unignore AssignmentContainer;
%unignore AssignmentContainer::Contains;
%rename (Element) AssignmentContainer::MutableElement(int);
%unignore AssignmentContainer::Size;
%unignore AssignmentContainer::Store;
%unignore AssignmentContainer::Restore;

// IntVarIterator
// Ignored:
// - ~IntVarIterator()
// - DebugString()
%unignore IntVarIterator;
%unignore IntVarIterator::Init;
%unignore IntVarIterator::Value;
%unignore IntVarIterator::Next;
%unignore IntVarIterator::Ok;


}  // namespace operations_research

%include "constraint_solver/constraint_solver.h"

// Define templates instantiation after wrapping.
namespace operations_research {
%rename (RevInteger) Rev<int64>;
%rename (RevInteger) Rev<int64>::Rev;
%unignore Rev<int64>::Value;
%unignore Rev<int64>::SetValue;
%template(RevInteger) Rev<int64>;

%rename (RevBool) Rev<bool>;
%rename (RevBool) Rev<bool>::Rev;
%unignore Rev<bool>::Value;
%unignore Rev<bool>::SetValue;
%template(RevBool) Rev<bool>;

%rename (IntContainer) AssignmentContainer<IntVar, IntVarElement>;
%rename (Element)
    AssignmentContainer<IntVar, IntVarElement>::MutableElement(int);
%unignore AssignmentContainer<IntVar, IntVarElement>::Size;
%template (IntContainer) AssignmentContainer<IntVar, IntVarElement>;
%rename (IntervalContainer)
    AssignmentContainer<IntervalVar, IntervalVarElement>;
%template (IntervalContainer)
    AssignmentContainer<IntervalVar, IntervalVarElement>;
%rename (SequenceContainer)
    AssignmentContainer<SequenceVar, SequenceVarElement>;
%template (SequenceContainer)
    AssignmentContainer<SequenceVar, SequenceVarElement>;

}  // namespace operations_research

// ================= constraint_solver.i API =====================

namespace operations_research {

// Ignored top-level classes, enums and methods:
// - BaseIntExpr
// - VarTypes (enum)
// - IntVarLocalSearchHandler
// - SequenceVarLocalSearchHandler
// - ChangeValue
// - PathOperator
// - MakeLocalSearchOperator()
// - PropagationMonitor
// - SymmetryBreaker
// - SearchLog
// - ModelCache
//
// - RevGrowingArray<>
// - RevIntSet<>
// - RevPartialSequence
// - IsArrayConstant<>()
// - IsArrayBoolean<>()
// - AreAllOnes<>()
// - AreAllNull<>()
// - AreAllGreaterOrEqual<>()
// - AreAllPositive<>()
// - AreAllStrictlyPositive<>()
// - IsIncreasingContiguous<>()
// - IsIncreasing<>()
// - IsArrayInRange<>()
// - AreAllBound()
// - AreAllBooleans()
// - AreAllBoundOrNull<>()
// - AreAllBoundTo()
// - MaxVarArray()
// - MinVarArray()
// - FillValues()
// - PosIntDivUp()
// - PosIntDivDown()
// - AreAllBoundTo()
// - MaxVarArray()
// - MinVarArray()
// - FillValues()
// - PosIntDivUp()
// - PosIntDivDown()
// - ToInt64Vector()
// - SortedNoDuplicates()

// LocalSearchOperator
// Ignored:
// - LocalSearchOperator()
// - ~LocalSearchOperator()
%unignore LocalSearchOperator;
%rename (NextNeighbor) LocalSearchOperator::MakeNextNeighbor;
%unignore LocalSearchOperator::Start;
%feature("director") LocalSearchOperator::MakeNextNeighbor;
%feature("director") LocalSearchOperator::Start;


// VarLocalSearchOperator<>
// Ignored:
// - VarLocalSearchOperator()
// - ~VarLocalSearchOperator()
// - Start()
// - Var()
// - SkipUnchanged()
// - Activated()
// - Activate()
// - Deactivate()
// - ApplyChanges()
// - RevertChanges()
// - AddVars()
%unignore VarLocalSearchOperator;
%unignore VarLocalSearchOperator::Size;
%unignore VarLocalSearchOperator::Value;
%unignore VarLocalSearchOperator::IsIncremental;
%feature("director") VarLocalSearchOperator::IsIncremental;
%unignore VarLocalSearchOperator::IsIncremental;
%unignore VarLocalSearchOperator::OnStart;
%feature("director") VarLocalSearchOperator::OnStart;
%unignore VarLocalSearchOperator::OnStart;
%unignore VarLocalSearchOperator::OldValue;
%unignore VarLocalSearchOperator::SetValue;


// IntVarLocalSearchOperator
// Ignored:
// - MakeNextNeighbor()
%unignore IntVarLocalSearchOperator;
%feature("director") IntVarLocalSearchOperator;
%unignore IntVarLocalSearchOperator::IntVarLocalSearchOperator;
%unignore IntVarLocalSearchOperator::~IntVarLocalSearchOperator;
%unignore IntVarLocalSearchOperator::Size;
%feature("director") IntVarLocalSearchOperator::MakeOneNeighbor;
%rename (OneNeighbor) IntVarLocalSearchOperator::MakeOneNeighbor;


// BaseLNS.
%unignore BaseLNS;
%feature("director") BaseLNS;
%unignore BaseLNS::BaseLNS;
%unignore BaseLNS::~BaseLNS;
%unignore BaseLNS::InitFragments;
%rename (NextFragmentWrapper) BaseLNS::NextFragment;

// ChangeValue
%unignore ChangeValue;
%unignore ChangeValue::ChangeValue;
%unignore ChangeValue::~ChangeValue;
%unignore ChangeValue::ModifyValue;

// SequenceVarLocalSearchOperator
// Ignored:
// - SequenceVarLocalSearchOperator()
// - ~SequenceVarLocalSearchOperator()
// - Sequence()
// - OldSequence()
// - SetForwardSequence()
// - SetBackwardSequence()
%unignore SequenceVarLocalSearchOperator;
%unignore SequenceVarLocalSearchOperator::Start;

// PathOperator
// Ignored:
// - PathOperator()
// - ~PathOperator()
// - SkipUnchanged()
// - Next()
// - Path()
// - number_of_nexts()
%unignore PathOperator;
%rename (Neighbor) PathOperator::MakeNeighbor;
%feature("director") PathOperator::MakeNeighbor;


// LocalSearchFilter
%unignore LocalSearchFilter;
%unignore LocalSearchFilter::Accept;
%unignore LocalSearchFilter::Synchronize;
%unignore LocalSearchFilter::IsIncremental;


// IntVarLocalSearchFilter
// Ignored:
// - Synchronize()
// - FindIndex()
// - AddVars()
// - Var()
// - IsVarSynced()
%feature("director") IntVarLocalSearchFilter;
%feature("nodirector") IntVarLocalSearchFilter::Start;  // Inherited.
%unignore IntVarLocalSearchFilter;
%unignore IntVarLocalSearchFilter::IntVarLocalSearchFilter;
%unignore IntVarLocalSearchFilter::~IntVarLocalSearchFilter;
%unignore IntVarLocalSearchFilter::Value;
%unignore IntVarLocalSearchFilter::Size;


// BooleanVar
// Ignored:
// - kUnboundBooleanVarValue
// - BooleanVar()
// - ~BooleanVar()
// - SetMin()
// - SetMax()
// - SetRange()
// - Bound()
// - RemoveValue()
// - RemoveInterval()
// - WhenBound()
// - WhenRange()
// - WhenDomain()
// - Size()
// - MakeHoleIterator()
// - MakeDomainIterator()
// - DebugString()
// - VarType()
// - IsEqual()
// - IsDifferent()
// - IsGreaterOrEqual()
// - IsLessOrEqual()
// - RestoreValue()
// - BaseName()
// - RawValue()
%unignore BooleanVar;
%unignore BooleanVar::Value;
%unignore BooleanVar::Min;
%unignore BooleanVar::Max;
%unignore BooleanVar::Contains;

}  // namespace operations_research

%include "constraint_solver/constraint_solveri.h"

%unignoreall

// ============= Custom python wrappers around C++ objects ==============
// (this section must be after the constraint_solver*.h %includes)

// Helpers to manipulate a wrapped C++ std::vector<int>*. Their wrapped versions
// are used in the python PyLNS.NextFragmentWrapper() method below.
// Note(user): We attempted to use %typemaps instead, but didn't make it work.
%inline %{
inline void ClearIntVector(std::vector<int>* v) { v->clear(); }
inline void PushBackIntVector(int x, std::vector<int>* v) { v->push_back(x); }
%}
%pythoncode {
class PyDecisionBuilder(object):

  def NextWrapper(self, solver):
    try:
      return self.Next(solver)
    except Exception as e:
      # TODO(user): use a custom Exception class in PROTECT_FROM_FAILURE,
      # instead of PyExc_Exception, and catch it here. Also fix all other
      # occurrences (see below).
      if 'CP Solver fail' in str(e):
        return solver.FailDecision()
      else:
        raise

  def DebugString(self):
    return "PyDecisionBuilder"


class PyConstraint(Constraint):

  def InitialPropagateWrapper(self):
    try:
      self.InitialPropagate()
    except Exception as e:
      if 'CP Solver fail' in str(e):
        self.solver().ShouldFail()
      else:
        raise

  def DebugString(self):
    return "PyConstraint"


class PyDemon(Demon):

  def RunWrapper(self, solver):
    try:
      self.Run(solver)
    except Exception as e:
      if 'CP Solver fail' in str(e):
        solver.ShouldFail()
      else:
        raise

  def DebugString(self):
    return "PyDemon"


class PyLns(BaseLNS):

  def NextFragment(self):
    return []

  def InitFragments(self):
    pass

  def NextFragmentWrapper(self, output_cpp_vector_fragments):
    ClearIntVector(output_cpp_vector_fragments);
    f = self.NextFragment()
    if not f:
      return False
    for x in f:
      PushBackIntVector(x, output_cpp_vector_fragments);
    return True
}  // %pythoncode
