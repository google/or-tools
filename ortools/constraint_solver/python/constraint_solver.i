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

// This .i file exposes the code declared in ../constraint_solver.h and
// ../constraint_solveri.h.
//
// It is particularly complex for a swig file, mostly because it contains a
// lot of MOE code for the export to the or-tools open-source project at
// https://github.com/google/or-tools, which benefits from a much more
// powerful python API thanks to the possible use of directors.
//
// USAGE EXAMPLES (most of which are also unit tests):
// - ./pywrapcp_test.py
// - examples/python/appointments.py
// - examples/python/golomb8.py
// - examples/python/hidato_table.py
// - examples/python/jobshop_ft06.py
// - examples/python/magic_sequence_distribute.py
// - examples/python/rabbit_pheasant.py
// - examples/python/simple_meeting.py
// - examples/python/sudoku.py
// - examples/python/zebra.py

%include "ortools/base/base.i"

%include "ortools/util/python/proto.i"

// PY_CONVERT_HELPER_* macros.
%include "ortools/constraint_solver/python/constraint_solver_helpers.i"

// std::function utilities.
%include "ortools/util/python/functions.i"

%import "ortools/util/python/vector.i"

// We *do* need to use SWIGTYPE_... type names directly, because the
// (recommended replacement) $descriptor macro fails, as of 2014-06, with
// types such as operations_research::Solver.
// The absence of whitespace before 'swiglint' is mandatory.
//swiglint: disable swigtype-name

// We need to forward-declare the proto here, so that PROTO_INPUT involving it
// works correctly. The order matters very much: this declaration needs to be
// before the %{ #include ".../constraint_solver.h" %}.
namespace operations_research {
class AssignmentProto;
class ConstraintSolverParameters;
class SearchLimitParameters;
}  // namespace operations_research

%{
#include <setjmp.h>  // For FailureProtect. See below.

// Used in the PROTECT_FROM_FAILURE macro. See below.
struct FailureProtect {
  jmp_buf exception_buffer;
  void JumpBack() { longjmp(exception_buffer, 1); }
};

// This #includes constraint_solver.h, and inlines some C++ helpers.
#include "ortools/constraint_solver/python/pywrapcp_util.h"
#include "ortools/constraint_solver/assignment.pb.h"
#include "ortools/constraint_solver/search_limit.pb.h"
#include "ortools/constraint_solver/solver_parameters.pb.h"
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
// http://www.i.org/Doc1.3/Python.html#Python_nn36 for example.
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

// See ./constraint_solver_helpers.i
PY_CONVERT_HELPER_PTR(Decision);
PY_CONVERT_HELPER_PTR(DecisionBuilder);
PY_CONVERT_HELPER_PTR(SearchMonitor);
PY_CONVERT_HELPER_PTR(IntervalVar);
PY_CONVERT_HELPER_PTR(SequenceVar);
PY_CONVERT_HELPER_PTR(LocalSearchOperator);
PY_CONVERT_HELPER_PTR(LocalSearchFilter);
PY_CONVERT_HELPER_INTEXPR_OR_INTVAR(IntVar);
PY_CONVERT_HELPER_INTEXPR_OR_INTVAR(IntExpr);


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
PY_CONVERT(Decision);
PY_CONVERT(DecisionBuilder);
PY_CONVERT(SearchMonitor);
PY_CONVERT(IntervalVar);
PY_CONVERT(SequenceVar);
PY_CONVERT(LocalSearchOperator);
PY_CONVERT(LocalSearchFilter);
#undef PY_CONVERT

// Support passing std::function<void(Solver*)> as argument.
// See ../utils/python/functions.i, from which this was copied and adapted.

%{
static void PyFunctionSolverToVoid(PyObject* pyfunc,
                                   operations_research::Solver* s) {
  // () needed to force creation of one-element tuple
  PyObject* const pysolver =
      SWIG_NewPointerObj(s, SWIGTYPE_p_operations_research__Solver,
                         SWIG_POINTER_EXCEPTION);
  PyObject* const pyresult = PyEval_CallFunction(pyfunc, "(O)", pysolver);
  if (!pyresult) {
    PyErr_SetString(PyExc_RuntimeError,
                    "std::function<void(Solver*)> invocation failed.");
  } else {
    Py_DECREF(pyresult);
  }
}
%}

%typecheck(SWIG_TYPECHECK_POINTER) std::function<void(
    operations_research::Solver*)> {
  $1 = PyCallable_Check($input);
}

%typemap(in) std::function<void(operations_research::Solver*)> {
  $1 = [$input](operations_research::Solver* s) {
    return PyFunctionSolverToVoid($input, s);
  };
}

// ============= Extensions ==============

// Add display methods on BaseObject and Solver.
%extend operations_research::BaseObject {
  std::string __str__() {
    return $self->DebugString();
  }
}
%extend operations_research::Solver {
  std::string __str__() {
    return $self->DebugString();
  }
%pythoncode {
  def Add(self, ct):
    if isinstance(ct, PyConstraint):
      self.__python_constraints.append(ct)
    self.AddConstraint(ct)
  }  // %pythoncode
}
%feature("pythonappend") operations_research::Solver::Solver %{
  self.__python_constraints = []
%}

// Extend IntervalVar to provide a nicer pythonic API for precedence
// and scheduling constraints. The macros below help do that concisely.
%define PRECEDENCE_CONSTRAINT(PythonMethodName, CppEnumName)
Constraint* PythonMethodName(IntervalVar* other) {
  return $self->solver()->MakeIntervalVarRelation(
      $self, operations_research::Solver::CppEnumName, other);
}

Constraint* PythonMethodName##WithDelay(IntervalVar* other, int64 delay) {
  return $self->solver()->MakeIntervalVarRelationWithDelay(
      $self, operations_research::Solver::CppEnumName, other, delay);
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

// Use DebugString() for the native std::string conversion in python, for objects
// that support it.
%define PY_STRINGIFY_DEBUGSTRING(Class)
%extend operations_research::Class {
  std::string __repr__() {
    return $self->DebugString();
  }

  std::string __str__() {
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
                          Solver::IndexFilter1 callback = nullptr) {
    return $self->MakeNoCycle(nexts, active, callback, false);
  }

  SearchMonitor* SearchLogWithCallback(int period,
                                       std::function<std::string()> callback) {
    return $self->MakeSearchLog(period, callback);
  }

  IntExpr* ElementFunction(std::function<int64(int64)> values,
                           IntVar* const index) {
    return $self->MakeElement(values, index);
  }


  DecisionBuilder* VarEvalValStrPhase(
      const std::vector<IntVar*>& vars,
      std::function<int64(int64)> var_evaluator,
      operations_research::Solver::IntValueStrategy val_str) {
    return $self->MakePhase(vars, var_evaluator, val_str);
  }

  DecisionBuilder* VarStrValEvalPhase(
      const std::vector<IntVar*>& vars,
      operations_research::Solver::IntVarStrategy var_str,
      Solver::IndexEvaluator2 val_eval) {
    return $self->MakePhase(vars, var_str, val_eval);
  }

  DecisionBuilder* VarEvalValEvalPhase(
      const std::vector<IntVar*>& vars,
      std::function<int64(int64)> var_eval,
      Solver::IndexEvaluator2 val_eval) {
    return $self->MakePhase(vars, var_eval, val_eval);
  }

  DecisionBuilder* VarStrValEvalTieBreakPhase(
      const std::vector<IntVar*>& vars,
      operations_research::Solver::IntVarStrategy var_str,
      Solver::IndexEvaluator2 val_eval,
      std::function<int64(int64)> tie_breaker) {
    return $self->MakePhase(vars, var_str, val_eval, tie_breaker);
  }

  DecisionBuilder* VarEvalValEvalTieBreakPhase(
      const std::vector<IntVar*>& vars,
      std::function<int64(int64)> var_eval,
      Solver::IndexEvaluator2 val_eval,
      std::function<int64(int64)> tie_breaker) {
    return $self->MakePhase(vars, var_eval, val_eval, tie_breaker);
  }

  DecisionBuilder* EvalEvalStrPhase(
      const std::vector<IntVar*>& vars,
      Solver::IndexEvaluator2 evaluator,
      operations_research::Solver::EvaluatorStrategy str) {
    return $self->MakePhase(vars, evaluator, str);
  }

  DecisionBuilder* EvalEvalStrTieBreakPhase(
      const std::vector<IntVar*>& vars,
      Solver::IndexEvaluator2 evaluator,
      Solver::IndexEvaluator1 tie_breaker,
      operations_research::Solver::EvaluatorStrategy str) {
    return $self->MakePhase(vars, evaluator, tie_breaker, str);
  }

  SearchMonitor* GuidedLocalSearch(
      bool maximize,
      IntVar* const objective,
      Solver::IndexEvaluator2 objective_function,
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
      Solver::IndexEvaluator2 values,
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
  IntExpr* __floordiv__(IntExpr* e) {
    return $self->solver()->MakeDiv($self, e);
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
  Constraint* NotMember(const std::vector<int64>& starts,
                        const std::vector<int64>& ends) {
    return $self->solver()->MakeNotMemberCt($self, starts, ends);
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

// Add easy variable getters to BaseLns ([i] gets the value of variable #i).
%extend operations_research::BaseLns {
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

  def __next__(self):
    return self.next()
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
// IMPORTANT: keep this code block in sync with the .i
// files in ../java and ../csharp.
// TODO(user): extract this duplicated code into a common, multi-language
// .i file with SWIG_exception.

// Protect from failure.
// TODO(user): document this further.
%define PROTECT_FROM_FAILURE(Method, GetSolver)
%exception Method {
  operations_research::Solver* const solver = GetSolver;
  FailureProtect protect;
  solver->set_fail_intercept([&protect]() { protect.JumpBack(); });
  if (setjmp(protect.exception_buffer) == 0) {
    $action
    solver->clear_fail_intercept();
  } else {
    solver->clear_fail_intercept();
    // IMPORTANT: the type and message of the exception raised matter,
    // because they are caught by the python overrides of some CP classes.
    // See the occurrences of the "PyExc_Exception" std::string below.
    PyErr_SetString(PyExc_Exception, "CP Solver fail");
    SWIG_fail;
  }
}
%enddef

namespace operations_research {
PROTECT_FROM_FAILURE(IntExpr::SetValue(int64 v), arg1->solver());
PROTECT_FROM_FAILURE(IntExpr::SetMin(int64 v), arg1->solver());
PROTECT_FROM_FAILURE(IntExpr::SetMax(int64 v), arg1->solver());
PROTECT_FROM_FAILURE(IntExpr::SetRange(int64 l, int64 u), arg1->solver());
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
PROTECT_FROM_FAILURE(Solver::AddConstraint(Constraint* const c), arg1);
PROTECT_FROM_FAILURE(Solver::Fail(), arg1);
}  // namespace operations_research
#undef PROTECT_FROM_FAILURE

// ############ END DUPLICATED CODE BLOCK ############

// ============= Exposed C++ API : Solver class ==============

%ignoreall

%unignore operations_research;

namespace operations_research {

// Solver: Basic API.
%unignore Solver;
%unignore Solver::Solver;
%unignore Solver::~Solver;
%unignore Solver::AddConstraint;
%unignore Solver::Solve;
%unignore Solver::DefaultSolverParameters;
%rename (Parameters) Solver::parameters;
%unignore Solver::DemonPriority;
%unignore Solver::DELAYED_PRIORITY;
%unignore Solver::VAR_PRIORITY;
%unignore Solver::NORMAL_PRIORITY;

// Solver: Decomposed or specialized Solve() API.
%unignore Solver::NewSearch;
%unignore Solver::NextSolution;
%unignore Solver::RestartSearch;
%unignore Solver::EndSearch;
%unignore Solver::Fail;
%unignore Solver::SolveAndCommit;
%unignore Solver::FinishCurrentSearch;
%unignore Solver::RestartCurrentSearch;
// TOOD(lperron): Support Action in python.
// %unignore Solver::AddBacktrackAction;

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
%unignore Solver::LocalSearchProfile;

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
%rename (IndexExpression) Solver::MakeIndexExpression;
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
%rename (IntervalVar) Solver::MakeIntervalVar;
%rename (FixedInterval) Solver::MakeFixedInterval;
%rename (MirrorInterval) Solver::MakeMirrorInterval;
%rename (FixedDurationStartSyncedOnStartIntervalVar)
    Solver::MakeFixedDurationStartSyncedOnStartIntervalVar;
%rename (FixedDurationStartSyncedOnEndIntervalVar)
    Solver::MakeFixedDurationStartSyncedOnEndIntervalVar;
%rename (FixedDurationEndSyncedOnStartIntervalVar)
    Solver::MakeFixedDurationEndSyncedOnStartIntervalVar;
%rename (FixedDurationEndSyncedOnEndIntervalVar)
    Solver::MakeFixedDurationEndSyncedOnEndIntervalVar;
%rename (IntervalRelaxedMin) Solver::MakeIntervalRelaxedMin;
%rename (IntervalRelaxedMax) Solver::MakeIntervalRelaxedMax;
%rename (TemporalDisjunction) Solver::MakeTemporalDisjunction;
%rename (Cover) Solver::MakeCover;

// Solver: Constraints tying a boolean var to an expression. Model-wise; it is
// equivalent to creating the expression via the python natural API; and then
// declaring its equality to the boolean var.
%rename (IsEqualCstVar) Solver::MakeIsEqualCstVar;
%rename (IsEqualVar) Solver::MakeIsEqualVar;
%rename (IsDifferentCstVar) Solver::MakeIsDifferentCstVar;
%rename (IsDifferentVar) Solver::MakeIsDifferentVar;
%rename (IsGreaterCstVar) Solver::MakeIsGreaterCstVar;
%rename (IsGreaterVar) Solver::MakeIsGreaterVar;
%rename (IsLessCstVar) Solver::MakeIsLessCstVar;
%rename (IsLessVar) Solver::MakeIsLessVar;
%rename (IsGreaterOrEqualCstVar) Solver::MakeIsGreaterOrEqualCstVar;
%rename (IsGreaterOrEqualVar) Solver::MakeIsGreaterOrEqualVar;
%rename (IsLessOrEqualCstVar) Solver::MakeIsLessOrEqualCstVar;
%rename (IsLessOrEqualVar) Solver::MakeIsLessOrEqualVar;
%rename (IsBetweenVar) Solver::MakeIsBetweenVar;
%rename (IsMemberVar) Solver::MakeIsMemberVar;
// The methods below should be avoided: use the *Var versions above if you can.
%rename (IsEqualCstCt) Solver::MakeIsEqualCstCt;
%rename (IsEqualCt) Solver::MakeIsEqualCt;
%rename (IsDifferentCstCt) Solver::MakeIsDifferentCstCt;
%rename (IsDifferentCt) Solver::MakeIsDifferentCt;
%rename (IsGreaterCstCt) Solver::MakeIsGreaterCstCt;
%rename (IsGreaterCt) Solver::MakeIsGreaterCt;
%rename (IsLessCstCt) Solver::MakeIsLessCstCt;
%rename (IsLessCt) Solver::MakeIsLessCt;
%rename (IsGreaterOrEqualCstCt) Solver::MakeIsGreaterOrEqualCstCt;
%rename (IsGreaterOrEqualCt) Solver::MakeIsGreaterOrEqualCt;
%rename (IsLessOrEqualCstCt) Solver::MakeIsLessOrEqualCstCt;
%rename (IsLessOrEqualCt) Solver::MakeIsLessOrEqualCt;
%rename (IsBetweenCt) Solver::MakeIsBetweenCt;
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
%rename (NotMemberCt) Solver::MakeNotMemberCt;
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
%rename (ClosureDemon) Solver::MakeClosureDemon;

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
%unignore Solver::SearchLimitParameters;  // search_limit.proto
%rename (Limit) Solver::MakeLimit;
%rename (TimeLimit) Solver::MakeTimeLimit;
%rename (BranchesLimit) Solver::MakeBranchesLimit;
%rename (FailuresLimit) Solver::MakeFailuresLimit;
%rename (SolutionsLimit) Solver::MakeSolutionsLimit;
%rename (CustomLimit) Solver::MakeCustomLimit;

// Solver: Search logs.
%rename (SearchLog) Solver::MakeSearchLog;
%rename (SearchTrace) Solver::MakeSearchTrace;
%rename (TreeMonitor) Solver::MakeTreeMonitor;

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
%rename (Decision) Solver::MakeDecision;

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
%rename (RandomLnsOperator) Solver::MakeRandomLnsOperator;
%unignore Solver::ConcatenateOperators;
%unignore Solver::RandomConcatenateOperators;
%rename (LocalSearchPhaseParameters) Solver::MakeLocalSearchPhaseParameters;
%rename (MoveTowardTargetOperator) Solver::MakeMoveTowardTargetOperator;
%rename (NeighborhoodLimit) Solver::MakeNeighborhoodLimit;

// Random part.
%unignore Solver::Rand64;
%unignore Solver::Rand32;
%unignore Solver::ReSeed;

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
// - MakeClosureDemon()
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
// - ModelVisitor
// - CastConstraint
// - NoGood
// - NoGoodManager
// - AssignmentElement
// - SolutionPool

// ConstraintSolverParameters
%unignore ConstraintSolverParameters;
%unignore ConstraintSolverParameters::ConstraintSolverParameters;
%unignore ConstraintSolverParameters::~ConstraintSolverParameters;

// ConstraintSolverParameters: Enums.
%unignore ConstraintSolverParameters::TrailCompression;
%unignore ConstraintSolverParameters::NO_COMPRESSION;
%unignore ConstraintSolverParameters::COMPRESS_WITH_ZLIB;

// ConstraintSolverParameters: methods.
%unignore ConstraintSolverParameters::compress_trail;
%unignore ConstraintSolverParameters::set_compress_trail;
%unignore ConstraintSolverParameters::trail_block_size;
%unignore ConstraintSolverParameters::set_trail_block_size;
%unignore ConstraintSolverParameters::array_split_size;
%unignore ConstraintSolverParameters::set_array_split_size;
%unignore ConstraintSolverParameters::store_names;
%unignore ConstraintSolverParameters::set_store_names;
%unignore ConstraintSolverParameters::name_cast_variables;
%unignore ConstraintSolverParameters::set_name_cast_variables;
%unignore ConstraintSolverParameters::name_all_variables;
%unignore ConstraintSolverParameters::set_name_all_variables;
%unignore ConstraintSolverParameters::profile_propagation;
%unignore ConstraintSolverParameters::set_profile_propagation;
%unignore ConstraintSolverParameters::profile_file;
%unignore ConstraintSolverParameters::set_profile_file;
%unignore ConstraintSolverParameters::trace_propagation;
%unignore ConstraintSolverParameters::set_trace_propagation;
%unignore ConstraintSolverParameters::trace_search;
%unignore ConstraintSolverParameters::set_trace_search;
%unignore ConstraintSolverParameters::print_model;
%unignore ConstraintSolverParameters::set_print_model;
%unignore ConstraintSolverParameters::print_model_stats;
%unignore ConstraintSolverParameters::set_print_model_stats;
%unignore ConstraintSolverParameters::print_added_constraints;
%unignore ConstraintSolverParameters::set_print_added_constraints;
%unignore ConstraintSolverParameters::export_file;
%unignore ConstraintSolverParameters::set_export_file;
%unignore ConstraintSolverParameters::disable_solve;
%unignore ConstraintSolverParameters::set_disable_solve;

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

%feature("director") BaseObject;
%unignore BaseObject;
%unignore BaseObject::BaseObject;
%unignore BaseObject::~BaseObject;
%unignore BaseObject::DebugString;

%feature("director") PropagationBaseObject;
%unignore PropagationBaseObject;
%unignore PropagationBaseObject::PropagationBaseObject;
%unignore PropagationBaseObject::~PropagationBaseObject;
%rename (Name) PropagationBaseObject::name;
%rename (solver) PropagationBaseObject::solver;
%unignore PropagationBaseObject::DebugString;
%feature("nodirector") PropagationBaseObject::BaseName;

// Decision
// Ignored:
// - DebugString()
// - Accept()
%feature ("director") Decision;
%unignore Decision;
%unignore Decision::Decision;
%unignore Decision::~Decision;
%rename (ApplyWrapper) Decision::Apply;
%rename (RefuteWrapper) Decision::Refute;
%feature("nodirector") Decision::Accept;

// DecisionBuilder
%feature("director") DecisionBuilder;
%unignore DecisionBuilder;
%unignore DecisionBuilder::DecisionBuilder;
%unignore DecisionBuilder::~DecisionBuilder;
%rename (NextWrapper) DecisionBuilder::Next;
%unignore DecisionBuilder::DebugString;
%feature("nodirector") DecisionBuilder::Accept;
%feature("nodirector") DecisionBuilder::AppendMonitors;


// Constraint
// Ignored:
// - Accept()
// - IsCastConstraint()
// Note(user): we prefer setting the 'director' feature on the individual
// methods of a class that require it, but sometimes we must actually set
// 'director' on the class itself, because it is a C++ abstract class and
// the client needs to construct it. In these cases, we don't bother
// setting the 'director' feature on individual methods, since it is done
// automatically when setting it on the class.
%unignore Constraint;
%feature("director") Constraint;
%unignore Constraint::Constraint;
%unignore Constraint::~Constraint;
%unignore Constraint::Post;
%rename (InitialPropagateWrapper) Constraint::InitialPropagate;
%unignore Constraint::Var;
%unignore Constraint::DebugString;


// SearchMonitor.
// Ignored:
// - kNoProgress
// - PeriodicCheck()
// - ProgressPercent()
// - Accept()
// - Install()
%feature("director") SearchMonitor;
%unignore SearchMonitor;
%unignore SearchMonitor::SearchMonitor;
%unignore SearchMonitor::~SearchMonitor;
%unignore SearchMonitor::EnterSearch;
%unignore SearchMonitor::RestartSearch;
%unignore SearchMonitor::ExitSearch;
%unignore SearchMonitor::BeginNextDecision;
%unignore SearchMonitor::EndNextDecision;
%unignore SearchMonitor::ApplyDecision;
%unignore SearchMonitor::RefuteDecision;
%unignore SearchMonitor::AfterDecision;
%unignore SearchMonitor::BeginFail;
%unignore SearchMonitor::EndFail;
%unignore SearchMonitor::BeginInitialPropagation;
%unignore SearchMonitor::EndInitialPropagation;
%unignore SearchMonitor::AcceptSolution;
%unignore SearchMonitor::AtSolution;
%unignore SearchMonitor::NoMoreSolutions;
%unignore SearchMonitor::LocalOptimum;
%unignore SearchMonitor::AcceptDelta;
%unignore SearchMonitor::AcceptNeighbor;
%rename (solver) SearchMonitor::solver;
%feature("nodirector") SearchMonitor::solver;


// Rev<>
%unignore Rev;
%unignore Rev::Rev;
%unignore Rev::Value;
%unignore Rev::SetValue;

// RevArray<>
%unignore RevArray;
%unignore RevArray::RevArray;
%unignore RevArray::Value;
%unignore RevArray::SetValue;
%rename (__len__) RevArray::size;

// NumericalRev<>
%unignore NumericalRev;
%unignore NumericalRev::NumericalRev;
%unignore NumericalRev::Add;
%unignore NumericalRev::Decr;
%unignore NumericalRev::Incr;
%unignore NumericalRev::SetValue;
%unignore NumericalRev::Value;

// NumericalRevArray<>
%unignore NumericalRevArray;
%unignore NumericalRevArray::NumericalRevArray;
%unignore NumericalRevArray::Add;
%unignore NumericalRevArray::Decr;
%unignore NumericalRevArray::Incr;
%unignore NumericalRevArray::SetValue;
%unignore NumericalRevArray::Value;
%rename (__len__) NumericalRevArray::size;

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
%rename (DomainIteratorAux) IntVar::MakeDomainIterator;
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
// - DebugString()
%feature("director") Demon;
%unignore Demon;
%unignore Demon::Demon;
%unignore Demon::~Demon;
%rename (RunWrapper) Demon::Run;
%rename (Inhibit) Demon::inhibit;
%rename (Desinhibit) Demon::desinhibit;
%rename (Priority) Demon::priority;

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

PY_PROTO_TYPEMAP(ortools.constraint_solver.assignment_pb2,
                 AssignmentProto,
                 operations_research::AssignmentProto)
PY_PROTO_TYPEMAP(ortools.constraint_solver.solver_parameters_pb2,
                 ConstraintSolverParameters,
                 operations_research::ConstraintSolverParameters)
PY_PROTO_TYPEMAP(ortools.constraint_solver_search_limit_pb2,
                 SearchLimitParameters,
                 operations_research::SearchLimitParameters)

%include "ortools/constraint_solver/constraint_solver.h"


// Define templates instantiation after wrapping.
namespace operations_research {
%rename (RevInteger) Rev<int64>;
%rename (RevInteger) Rev<int64>::Rev;
%unignore Rev<int64>::Value;
%unignore Rev<int64>::SetValue;
%template(RevInteger) Rev<int64>;

%rename (NumericalRevInteger) NumericalRev<int64>;
%rename (NumericalRevInteger) NumericalRev<int64>::NumericalRev;
%unignore NumericalRev<int64>::Add;
%unignore NumericalRev<int64>::Decr;
%unignore NumericalRev<int64>::Incr;
%unignore NumericalRev<int64>::SetValue;
%unignore NumericalRev<int64>::Value;
%template(NumericalRevInteger) NumericalRev<int64>;

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

// LocalSearchOperator
// Ignored:
// - LocalSearchOperator()
// - ~LocalSearchOperator()
%feature("director") LocalSearchOperator;
%unignore LocalSearchOperator;
%rename (NextNeighbor) LocalSearchOperator::MakeNextNeighbor;
%unignore LocalSearchOperator::Start;

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
%unignore VarLocalSearchOperator::IsIncremental;
%unignore VarLocalSearchOperator::OnStart;
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
%rename (OneNeighbor) IntVarLocalSearchOperator::MakeOneNeighbor;


// BaseLns.
%unignore BaseLns;
%feature("director") BaseLns;
%unignore BaseLns::BaseLns;
%unignore BaseLns::~BaseLns;
%unignore BaseLns::InitFragments;
%unignore BaseLns::NextFragment;
%feature ("nodirector") BaseLns::OnStart;
%feature ("nodirector") BaseLns::SkipUnchanged;
%feature ("nodirector") BaseLns::MakeOneNeighbor;
%unignore BaseLns::IsIncremental;
%unignore BaseLns::AppendToFragment;
%unignore BaseLns::FragmentSize;

// ChangeValue
%unignore ChangeValue;
%feature ("director") ChangeValue;
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

%include "ortools/constraint_solver/constraint_solveri.h"

%unignoreall

// ============= Custom python wrappers around C++ objects ==============
// (this section must be after the constraint_solver*.h %includes)

%pythoncode {
class PyDecision(Decision):

  def __init__(self):
    Decision.__init__(self)

  def ApplyWrapper(self, solver):
    try:
       self.Apply(solver)
    except Exception as e:
      if 'CP Solver fail' in str(e):
        solver.ShouldFail()
      else:
        raise

  def RefuteWrapper(self, solver):
    try:
       self.Refute(solver)
    except Exception as e:
      if 'CP Solver fail' in str(e):
        solver.ShouldFail()
      else:
        raise

  def DebugString(self):
    return "PyDecision"


class PyDecisionBuilder(DecisionBuilder):

  def __init__(self):
    DecisionBuilder.__init__(self)

  def NextWrapper(self, solver):
    try:
      return self.Next(solver)
    except Exception as e:
      if 'CP Solver fail' in str(e):
        return solver.FailDecision()
      else:
        raise

  def DebugString(self):
    return "PyDecisionBuilder"


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


class PyConstraintDemon(PyDemon):

  def __init__(self, ct, method, delayed, *args):
    PyDemon.__init__(self)
    self.__constraint = ct
    self.__method = method
    self.__delayed = delayed
    self.__args = args

  def Run(self, solver):
    self.__method(self.__constraint, *self.__args)

  def Priority(self):
    return Solver.DELAYED_PRIORITY if self.__delayed else Solver.NORMAL_PRIORITY

  def DebugString(self):
    return 'PyConstraintDemon'


class PyConstraint(Constraint):

  def __init__(self, solver):
    Constraint.__init__(self, solver)
    self.__demons = []

  def Demon(self, method, *args):
    demon = PyConstraintDemon(self, method, False, *args)
    self.__demons.append(demon)
    return demon

  def DelayedDemon(self, method, *args):
    demon = PyConstraintDemon(self, method, True, *args)
    self.__demons.append(demon)
    return demon

  def InitialPropagateDemon(self):
    return self.solver().ConstraintInitialPropagateCallback(self)

  def DelayedInitialPropagateDemon(self):
    return self.solver().DelayedConstraintInitialPropagateCallback(self)

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


}  // %pythoncode

