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

%include exception.i
%include util/data.swig
%include util/util.swig

#ifdef SWIGPYTHON
// We *do* need to use SWIGTYPE_... descriptor directly in embedded C++ code:
// the recommended replacement $descriptor(...) is only available within
// SWIG code.
//swiglint: disable swigtype-name

#pragma SWIG nowarn=473 // about Decision* return from Next()
%{
#include <setjmp.h>

#include "constraint_solver/assignment.pb.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "constraint_solver/model.pb.h"
#include "constraint_solver/search_limit.pb.h"

DECLARE_bool(cp_trace_propagation);
DECLARE_bool(cp_trace_search);
DECLARE_bool(cp_print_model);
DECLARE_bool(cp_model_stats);
DECLARE_string(cp_export_file);
DECLARE_bool(cp_no_solve);
DECLARE_string(cp_profile_file);

struct FailureProtect {
  jmp_buf exception_buffer;
  void JumpBack() {
    longjmp(exception_buffer, 1);
  }
};

class CallPyDecisionBuilder : public operations_research::DecisionBuilder {
 public:
  CallPyDecisionBuilder(PyObject* pydb) : pysolver_(nullptr), pyarg_(nullptr) {
    Py_INCREF(pydb);
    pydb_ = pydb;
    func_ = PyObject_GetAttrString(pydb_, "NextWrapper");
    Py_XINCREF(func_);
    str_func_ = PyObject_GetAttrString(pydb_, "DebugString");
    Py_XINCREF(str_func_);
  }

  virtual ~CallPyDecisionBuilder() {
    Py_DECREF(pydb_);
    pydb_ = nullptr;
    Py_XDECREF(func_);
    func_ = nullptr;
    Py_XDECREF(str_func_);
    str_func_ = nullptr;
    Py_XDECREF(pysolver_);
    pysolver_ = nullptr;
    Py_XDECREF(pyarg_);
    pyarg_ = nullptr;
  }

  virtual operations_research::Decision* Next(
      operations_research::Solver* const s) {
    if (pysolver_ == nullptr) {
      pysolver_ = SWIG_NewPointerObj(s,
                                     SWIGTYPE_p_operations_research__Solver,
                                     SWIG_POINTER_EXCEPTION);
      pyarg_ = Py_BuildValue((char*)"(O)", pysolver_);
    }
    operations_research::Decision* result = nullptr;
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

  virtual std::string DebugString() const {
    std::string result = "PyDecisionBuilder";
    if (str_func_) {
      PyObject* pyresult = PyEval_CallObject(str_func_, nullptr);
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

class PyLNSNoValues : public operations_research::BaseLNS {
 public:
  PyLNSNoValues(const std::vector<operations_research::IntVar*>& vars, PyObject* op)
      : BaseLNS(vars), op_(op) {
    Py_INCREF(op_);
    init_func_ = PyObject_GetAttrString(op_, "InitFragments");
    Py_XINCREF(init_func_);
    fragment_func_ = PyObject_GetAttrString(op_, "NextFragment");
    Py_XINCREF(fragment_func_);
  }

  virtual ~PyLNSNoValues() {
    Py_DECREF(op_);
    op_ = nullptr;
    Py_XDECREF(init_func_);
    init_func_ = nullptr;
    Py_XDECREF(fragment_func_);
    fragment_func_ = nullptr;
  }

  virtual void InitFragments() {
    if (init_func_) {
      PyObject* pyresult = PyEval_CallObject(init_func_, nullptr);
      Py_XDECREF(pyresult);
    }
  }

  virtual bool NextFragment(std::vector<int>* fragment) {
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

  virtual std::string DebugString() const {
    return "PyLNSNoValues()";
  }
 private:
  PyObject* op_;
  PyObject* init_func_;
  PyObject* fragment_func_;
};

class PyLNS : public operations_research::BaseLNS {
 public:
  PyLNS(const std::vector<operations_research::IntVar*>& vars, PyObject* op)
      : BaseLNS(vars), op_(op) {
    Py_INCREF(op_);
    init_func_ = PyObject_GetAttrString(op_, "InitFragments");
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
    op_ = nullptr;
    Py_XDECREF(init_func_);
    init_func_ = nullptr;
    Py_XDECREF(fragment_func_);
    fragment_func_ = nullptr;
    Py_DECREF(base_lns_);
    base_lns_ = nullptr;
  }

  virtual void InitFragments() {
    if (init_func_) {
      PyObject* pyresult = PyEval_CallObject(init_func_, nullptr);
      Py_XDECREF(pyresult);
    }
  }

  virtual bool NextFragment(std::vector<int>* fragment) {
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

  virtual std::string DebugString() const {
    return "PyLNS()";
  }
 private:
  PyObject* op_;
  PyObject* init_func_;
  PyObject* fragment_func_;
  PyObject* base_lns_;
};
%}

namespace operations_research {
%pythoncode {
import gflags
import ortools.constraint_solver.assignment_pb2
import ortools.constraint_solver.model_pb2
import ortools.constraint_solver.search_limit_pb2

FLAGS = gflags.FLAGS
gflags.DEFINE_boolean('cp_trace_propagation', False,
                      'trace all propagation events.')
gflags.DEFINE_boolean('cp_trace_search', False,
                      'trace all search events.')
gflags.DEFINE_boolean('cp_print_model', False,
                      'prints the model before solving it.')
gflags.DEFINE_boolean('cp_model_stats', False,
                      'displays model statistics before solving it.')
gflags.DEFINE_string('cp_export_file', '',
                     'exports model to file using CPModelProto.')
gflags.DEFINE_boolean('cp_no_solve', False,
                      'force failures at the beginning of a search.')
gflags.DEFINE_string('cp_profile_file', '',
                     'exports profiling overview to file.')
}
%pythoncode {

class PyDecisionBuilder(object):
  def NextWrapper(self, solver):
    result = None
    try:
      result = self.Next(solver)
    except Exception:
      return solver.FailDecision()
    return result

  def DebugString(self):
    return "PyDecisionBuilder"
}
}  // namespace operations_research

// Rename rules on SolutionCollector.
%rename (Branches) operations_research::SolutionCollector::branches;
%rename (Failures) operations_research::SolutionCollector::failures;
%rename (ObjectiveValue) operations_research::SolutionCollector::objective_value;
%rename (Solution) operations_research::SolutionCollector::solution;
%rename (SolutionCount) operations_research::SolutionCollector::solution_count;
%rename (WallTime) operations_research::SolutionCollector::wall_time;

// Rename rule on OptimizeVar
%rename (Best) operations_research::OptimizeVar::best;

// Rename rule on SearchLimit
%rename (IsCrossed) operations_research::SearchLimit::crossed;

// Rename rules on Solver.
%rename (AcceptedNeighbors) operations_research::Solver::accepted_neighbors;
%rename (Add) operations_research::Solver::AddConstraint;
%rename (Branches) operations_research::Solver::branches;
%rename (DemonRuns) operations_research::Solver::demon_runs;
%rename (FailStamp) operations_research::Solver::fail_stamp;
%rename (Failures) operations_research::Solver::failures;
%rename (FilteredNeighbors) operations_research::Solver::filtered_neighbors;
%rename (Neighbors) operations_research::Solver::neighbors;
%rename (Solutions) operations_research::Solver::solutions;
%rename (Stamp) operations_research::Solver::stamp;
%rename (WallTime) operations_research::Solver::wall_time;

// Generic rule to remove all Make prefixes.
%rename("%(strip:[Make])s", %$isfunction) "";

// Conversion utilities.
namespace operations_research {

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

// Conversion to IntExpr* and IntVar* are a bit special because of the two
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

// Conversions to DecisionBuilder* are also special because we've redefined
// our own python DecisionBuilder object.
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

// Actual conversions. Also include the conversion to std::vector<Class>.
%define PY_CONVERT(Class)
%{
bool CanConvertTo ## Class(PyObject *py_obj) {
  operations_research::Class* tmp;
  return PyObjAs(py_obj, &tmp);
}
%}
%typemap(in) Class* const { if (!PyObjAs($input, &$1)) SWIG_fail; }
%typecheck(SWIG_TYPECHECK_POINTER) Class* const {
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

// Support protobuf.
PY_PROTO_TYPEMAP(ortools.constraint_solver.model_pb2,
                 operations_research::CPModelProto,
                 CPModelProto);
PY_PROTO_TYPEMAP(ortools.constraint_solver.search_limit_pb2,
                 operations_research::SearchLimitProto,
                 SearchLimitProto);
PY_PROTO_TYPEMAP(ortools.constraint_solver.assignment_pb2,
                 operations_research::AssignmentProto,
                 AssignmentProto);

// Add display methods on BaseObject and remove DebugString method.
%ignore BaseObject::DebugString;
%extend BaseObject {
  std::string __str__() {
    return self->DebugString();
  }
  std::string __repr__() {
    return self->DebugString();
  }
}

%extend IntervalVar {
  std::string __repr__() {
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
  Constraint* StaysInSync(IntervalVar* other) {
    return self->solver()->MakeIntervalVarRelation(self,
                                                   operations_research::Solver::STAYS_IN_SYNC,
                                                   other);
  }
  Constraint* EndsAfter(int64 date) {
    return self->solver()->MakeIntervalVarRelation(self,
                                                   operations_research::Solver::ENDS_AFTER,
                                                   date);
  }
  Constraint* EndsAt(int64 date) {
    return self->solver()->MakeIntervalVarRelation(self,
                                                   operations_research::Solver::ENDS_AT,
                                                   date);
  }
  Constraint* EndsBefore(int64 date) {
    return self->solver()->MakeIntervalVarRelation(self,
                                                   operations_research::Solver::ENDS_BEFORE,
                                                   date);
  }
  Constraint* StartsAfter(int64 date) {
    return self->solver()->MakeIntervalVarRelation(self,
                                                   operations_research::Solver::STARTS_AFTER,
                                                   date);
  }
  Constraint* StartsAt(int64 date) {
    return self->solver()->MakeIntervalVarRelation(self,
                                                   operations_research::Solver::STARTS_AT,
                                                   date);
  }
  Constraint* StartsBefore(int64 date) {
    return self->solver()->MakeIntervalVarRelation(self,
                                                   operations_research::Solver::STARTS_BEFORE,
                                                   date);
  }
  Constraint* CrossesDate(int64 date) {
    return self->solver()->MakeIntervalVarRelation(self,
                                                   operations_research::Solver::CROSS_DATE,
                                                   date);
  }
  Constraint* AvoidsDate(int64 date) {
    return self->solver()->MakeIntervalVarRelation(self,
                                                   operations_research::Solver::AVOID_DATE,
                                                   date);
  }
}

%define PY_STRINGIFY_DEBUGSTRING(Class)
%extend Class {
  std::string __repr__() {
    return self->DebugString();
  }
}
%enddef
PY_STRINGIFY_DEBUGSTRING(SequenceVar);
PY_STRINGIFY_DEBUGSTRING(IntVar);
PY_STRINGIFY_DEBUGSTRING(IntExpr);
PY_STRINGIFY_DEBUGSTRING(Constraint);
PY_STRINGIFY_DEBUGSTRING(SearchMonitor);
PY_STRINGIFY_DEBUGSTRING(DecisionBuilder);
PY_STRINGIFY_DEBUGSTRING(Decision);
#undef PY_STRINGIFY_DEBUGSTRING

// Add display methods on Solver and remove DebugString method.
%ignore Solver::DebugString;

// Indentation is critical here as the code is copied verbatim in the
// python code.
%feature("pythonappend") Solver::Solver %{
        Solver.SetPythonFlags(FLAGS.cp_trace_propagation,
                              FLAGS.cp_trace_search,
                              FLAGS.cp_print_model,
                              FLAGS.cp_model_stats,
                              FLAGS.cp_export_file,
                              FLAGS.cp_no_solve,
                              FLAGS.cp_profile_file)
%}


%extend Solver {
static void SetPythonFlags(bool trace_propagation,
                             bool trace_search,
                             bool print_model,
                             bool model_stats,
                             const std::string& export_file,
                             bool no_solve,
                             const std::string& profile_file) {
    FLAGS_cp_trace_propagation = trace_propagation;
    FLAGS_cp_trace_search = trace_search;
    FLAGS_cp_print_model = print_model;
    FLAGS_cp_model_stats = model_stats;
    FLAGS_cp_export_file = export_file;
    FLAGS_cp_no_solve = no_solve;
    FLAGS_cp_profile_file = profile_file;
  }

  Constraint* TreeNoCycle(const std::vector<IntVar*>& nexts,
                          const std::vector<IntVar*>& active,
                          ResultCallback1<bool, int64>* callback = nullptr) {
    return self->MakeNoCycle(nexts, active, callback, false);
  }

  SearchMonitor* SearchLogWithCallback(int period,
                                       ResultCallback<std::string>* callback) {
    return self->MakeSearchLog(period, callback);
  }

  IntExpr* ElementFunction(ResultCallback1<int64, int64>* values,
                           IntVar* const index) {
    return self->MakeElement(values, index);
  }

  std::string __str__() {
    return self->DebugString();
  }

  LocalSearchOperator* LNSOperator(const std::vector<IntVar*>& vars,
                                   PyObject* lns) {
    return self->RevAlloc(new PyLNS(vars, lns));
  }

  LocalSearchOperator* LNSNoValuesOperator(const std::vector<IntVar*>& vars,
                                           PyObject* lns) {
    return self->RevAlloc(new PyLNSNoValues(vars, lns));
  }

  DecisionBuilder* VarEvalValStrPhase(
      const std::vector<IntVar*>& vars,
      ResultCallback1<int64, int64>* var_evaluator,
      operations_research::Solver::IntValueStrategy val_str) {
    return self->MakePhase(vars, var_evaluator, val_str);
  }

  DecisionBuilder* VarStrValEvalPhase(
      const std::vector<IntVar*>& vars,
      operations_research::Solver::IntVarStrategy var_str,
      ResultCallback2<int64, int64, int64>* val_eval) {
    return self->MakePhase(vars, var_str, val_eval);
  }

  DecisionBuilder* VarEvalValEvalPhase(
      const std::vector<IntVar*>& vars,
      ResultCallback1<int64, int64>* var_eval,
      ResultCallback2<int64, int64, int64>* val_eval) {
    return self->MakePhase(vars, var_eval, val_eval);
  }

  DecisionBuilder* VarStrValEvalTieBreakPhase(
      const std::vector<IntVar*>& vars,
      operations_research::Solver::IntVarStrategy var_str,
      ResultCallback2<int64, int64, int64>* val_eval,
      ResultCallback1<int64, int64>* tie_breaker) {
    return self->MakePhase(vars, var_str, val_eval, tie_breaker);
  }

  DecisionBuilder* VarEvalValEvalTieBreakPhase(
      const std::vector<IntVar*>& vars,
      ResultCallback1<int64, int64>* var_eval,
      ResultCallback2<int64, int64, int64>* val_eval,
      ResultCallback1<int64, int64>* tie_breaker) {
    return self->MakePhase(vars, var_eval, val_eval, tie_breaker);
  }

  DecisionBuilder* EvalEvalStrPhase(
      const std::vector<IntVar*>& vars,
      ResultCallback2<int64, int64, int64>* evaluator,
      operations_research::Solver::EvaluatorStrategy str) {
    return self->MakePhase(vars, evaluator, str);
  }

  DecisionBuilder* EvalEvalStrTieBreakPhase(
      const std::vector<IntVar*>& vars,
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
      const std::vector<IntVar*>& vars,
      double penalty_factor) {
    return self->MakeGuidedLocalSearch(maximize,
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
%ignore Solver::MakeElement(ResultCallback1<int64, int64>* values,
                            IntVar* const index);
%ignore Solver::MakeMin(IntExpr* const expr, int val);
%ignore Solver::MakeMax(IntExpr* const expr, int val);
%ignore Solver::MakeEquality;
%ignore Solver::MakeNonEquality;
%ignore Solver::MakeGreaterOrEqual;
%ignore Solver::MakeGreater;
%ignore Solver::MakeLessOrEqual;
%ignore Solver::MakeLess;
%ignore Solver::MakeConstraintInitialPropagateCallback;
%ignore Solver::MakeDelayedConstraintInitialPropagateCallback;
%ignore Solver::MakeLocalSearchObjectiveFilter;
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
  IntExpr* __add__(Constraint* other) {
    return self->solver()->MakeSum(self, other->Var());
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
  IntExpr* __sub__(Constraint* other) {
    return self->solver()->MakeDifference(self, other->Var());
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
  IntExpr* __mul__(Constraint* other) {
    return self->solver()->MakeProd(self, other->Var());
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
  IntExpr* __mod__(int64 v) {
    return self->solver()->MakeModulo(self, v);
  }
  IntExpr* __mod__(IntExpr* e) {
    return self->solver()->MakeModulo(self, e);
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
  Constraint* __eq__(Constraint* other) {
    return self->solver()->MakeEquality(self->Var(), other->Var());
  }
  Constraint* __ne__(Constraint* other) {
    return self->solver()->MakeNonEquality(self->Var(), other->Var());
  }
  Constraint* __ge__(Constraint* other) {
    return self->solver()->MakeGreaterOrEqual(self->Var(), other->Var());
  }
  Constraint* __gt__(Constraint* other) {
    return self->solver()->MakeGreater(self->Var(), other->Var());
  }
  Constraint* __le__(Constraint* other) {
    return self->solver()->MakeLessOrEqual(self->Var(), other->Var());
  }
  Constraint* __lt__(Constraint* other) {
    return self->solver()->MakeLess(self->Var(), other->Var());
  }
  Constraint* MapTo(const std::vector<IntVar*>& vars) {
    return self->solver()->MakeMapDomain(self->Var(), vars);
  }
  IntExpr* IndexOf(const std::vector<int64>& vars) {
    return self->solver()->MakeElement(vars, self->Var());
  }
  IntExpr* IndexOf(const std::vector<IntVar*>& vars) {
    return self->solver()->MakeElement(vars, self->Var());
  }
  IntVar* IsMember(const std::vector<int64>& values) {
    return self->solver()->MakeIsMemberVar(self->Var(), values);
  }
  Constraint* Member(const std::vector<int64>& values) {
    return self->solver()->MakeMemberCt(self->Var(), values);
  }
}

// Add arithmetic operators to integer expressions.
%extend Constraint {
  IntExpr* __add__(IntExpr* other) {
    return self->solver()->MakeSum(self->Var(), other);
  }
  IntExpr* __add__(Constraint* other) {
    return self->solver()->MakeSum(self->Var(), other->Var());
  }
  IntExpr* __add__(int64 v) {
    return self->solver()->MakeSum(self->Var(), v);
  }
  IntExpr* __radd__(int64 v) {
    return self->solver()->MakeSum(self->Var(), v);
  }
  IntExpr* __sub__(IntExpr* other) {
    return self->solver()->MakeDifference(self->Var(), other);
  }
  IntExpr* __sub__(Constraint* other) {
    return self->solver()->MakeDifference(self->Var(), other->Var());
  }
  IntExpr* __sub__(int64 v) {
    return self->solver()->MakeSum(self->Var(), -v);
  }
  IntExpr* __rsub__(int64 v) {
    return self->solver()->MakeDifference(v, self->Var());
  }
  IntExpr* __mul__(IntExpr* other) {
    return self->solver()->MakeProd(self->Var(), other);
  }
  IntExpr* __mul__(Constraint* other) {
    return self->solver()->MakeProd(self->Var(), other->Var());
  }
  IntExpr* __mul__(int64 v) {
    return self->solver()->MakeProd(self->Var(), v);
  }
  IntExpr* __rmul__(int64 v) {
    return self->solver()->MakeProd(self->Var(), v);
  }
  IntExpr* __floordiv__(int64 v) {
    return self->solver()->MakeDiv(self->Var(), v);
  }

  IntExpr* __neg__() {
    return self->solver()->MakeOpposite(self->Var());
  }
  IntExpr* __abs__() {
    return self->solver()->MakeAbs(self->Var());
  }
  IntExpr* Square() {
    return self->solver()->MakeSquare(self->Var());
  }

  Constraint* __eq__(int64 v) {
    return self->solver()->MakeEquality(self->Var(), v);
  }
  Constraint* __ne__(int64 v) {
    return self->solver()->MakeNonEquality(self->Var(), v);
  }
  Constraint* __ge__(int64 v) {
    return self->solver()->MakeGreaterOrEqual(self->Var(), v);
  }
  Constraint* __gt__(int64 v) {
    return self->solver()->MakeGreater(self->Var(), v);
  }
  Constraint* __le__(int64 v) {
    return self->solver()->MakeLessOrEqual(self->Var(), v);
  }
  Constraint* __lt__(int64 v) {
    return self->solver()->MakeLess(self->Var(), v);
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
  Constraint* __eq__(Constraint* other) {
    return self->solver()->MakeEquality(self->Var(), other->Var());
  }
  Constraint* __ne__(Constraint* other) {
    return self->solver()->MakeNonEquality(self->Var(), other->Var());
  }
  Constraint* __ge__(Constraint* other) {
    return self->solver()->MakeGreaterOrEqual(self->Var(), other->Var());
  }
  Constraint* __gt__(Constraint* other) {
    return self->solver()->MakeGreater(self->Var(), other->Var());
  }
  Constraint* __le__(Constraint* other) {
    return self->solver()->MakeLessOrEqual(self->Var(), other->Var());
  }
  Constraint* __lt__(Constraint* other) {
    return self->solver()->MakeLess(self->Var(), other->Var());
  }
  Constraint* MapTo(const std::vector<IntVar*>& vars) {
    return self->solver()->MakeMapDomain(self->Var(), vars);
  }
  IntExpr* IndexOf(const std::vector<int64>& vars) {
    return self->solver()->MakeElement(vars, self->Var());
  }
  IntExpr* IndexOf(const std::vector<IntVar*>& vars) {
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
}  // namespace operations_research
#endif  // SWIGPYTHON

// Protect from failure. Since the code is very similar in Python, Java and C#
// we use the same block.
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
    #if defined(SWIGCSHARP)
    SWIG_CSharpSetPendingException(SWIG_CSharpApplicationException, "fail");
    return $null;
    #elif defined(SWIGJAVA)
    jclass fail_class = jenv->FindClass(
        "com/google/ortools/constraintsolver/"
        "Solver$FailException");
    jenv->ThrowNew(fail_class, "fail");
    return $null;
    #elif defined(SWIGPYTHON)
    PyErr_SetString(PyExc_IndexError, "fail");
    SWIG_fail;
    #endif
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
#undef PROTECT_FROM_FAILURE
}  // namespace operations_research

#if defined(SWIGJAVA)
%module(directors="1") operations_research;
%feature("director") DecisionBuilder;
%feature("director") Decision;
%feature("director") SearchMonitor;
%feature("director") Action;
%feature("director") LocalSearchOperator;
%feature("director") PathOperator;
%feature("director") BaseLns;
%feature("director") IntVarLocalSearchOperator;
%feature("director") SequenceVarLocalSearchOperator;
%feature("director") IntVarLocalSearchFilter;
%include "std_vector.i"

%template(IntVector) std::vector<int>;

%{
#include <setjmp.h>

#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"

namespace operations_research {
class LocalSearchPhaseParameters {
 public:
  LocalSearchPhaseParameters() {}
  ~LocalSearchPhaseParameters() {}
};
}  // namespace operations_research


struct FailureProtect {
  jmp_buf exception_buffer;
  void JumpBack() {
    longjmp(exception_buffer, 1);
  }
};
%}

%ignore operations_research::Solver::MakeIntVarArray;
%ignore operations_research::Solver::MakeBoolVarArray;
%ignore operations_research::Solver::MakeFixedDurationIntervalVarArray;
%ignore operations_research::IntVarLocalSearchFilter::FindIndex;

%rename (nextWrap) operations_research::DecisionBuilder::Next;
%rename (toString) *::DebugString;
%rename (tryDecisions) operations_research::Solver::Try;

%rename("%(lowercamelcase)s", %$isfunction) "";

namespace operations_research {
%define CONVERT_VECTOR(type)
%typemap(jni) const std::vector<type*>& "jobjectArray"
%typemap(jtype) const std::vector<type*>& "type[]"
%typemap(jstype) const std::vector<type*>& "type[]"
%typemap(javain) const std::vector<type*>& "$javainput"
%typemap(in) const std::vector<type*>& (std::vector<type*> result) {
  jclass object_class =
    jenv->FindClass("com/google/ortools/"
                    "constraintsolver/type");
  if (nullptr == object_class)
    return $null;
  jmethodID method_id =
      jenv->GetStaticMethodID(object_class,
                              "getCPtr",
                              "(Lcom/google/ortools/"
                              "constraintsolver/type;)J");
  assert(method_id != nullptr);
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
CONVERT_VECTOR(SequenceVar);
CONVERT_VECTOR(LocalSearchOperator);
CONVERT_VECTOR(LocalSearchFilter);

%typemap(javacode) Solver %{
  /**
   * This exceptions signal that a failure has been raised in the C++ world.
   *
   */
  public static class FailException extends Exception {
    public FailException() {
      super();
    }

    public FailException(String message) {
      super(message);
    }
  }

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

%extend IntVarLocalSearchFilter {
  int Index(IntVar* const var) {
    int64 index = -1;
    self->FindIndex(var, &index);
    return index;
  }
}
}  // namespace operations_research

namespace operations_research {
class LocalSearchPhaseParameters {
 public:
  LocalSearchPhaseParameters();
  ~LocalSearchPhaseParameters();
};
}  // namespace operations_research

#endif  // SWIGJAVA

#if defined(SWIGCSHARP)
%module(directors="1", allprotected="1") operations_research;
%feature("director") Action;
%feature("director") BaseLNS;
%feature("director") Decision;
%feature("director") DecisionBuilder;
%feature("director") IntVarLocalSearchFilter;
%feature("director") IntVarLocalSearchOperator;
%feature("director") SequenceVarLocalSearchOperator;
%feature("director") LocalSearchOperator;
%feature("director") OptimizeVar;
%feature("director") SearchLimit;
%feature("director") SearchMonitor;
%feature("director") SymmetryBreaker;

%include "std_vector.i"

%template(IntVector) std::vector<int>;

%{
#include <setjmp.h>

#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"

namespace operations_research {
class LocalSearchPhaseParameters {
 public:
  LocalSearchPhaseParameters() {}
  ~LocalSearchPhaseParameters() {}
};
}  // namespace operations_research

struct FailureProtect {
  jmp_buf exception_buffer;
  void JumpBack() {
    longjmp(exception_buffer, 1);
  }
};
%}

%ignore operations_research::Solver::MakeIntVarArray;
%ignore operations_research::Solver::MakeBoolVarArray;
%ignore operations_research::Solver::MakeFixedDurationIntervalVarArray;
%ignore operations_research::IntVarLocalSearchFilter::FindIndex;
%ignore operations_research::SequenceVarLocalSearchOperator::Sequence;
%ignore operations_research::SequenceVarLocalSearchOperator::OldSequence;
%ignore operations_research::SolutionCollector::ForwardSequence;
%ignore operations_research::SolutionCollector::BackwardSequence;
%ignore operations_research::SolutionCollector::Unperformed;
%ignore operations_research::SequenceVarElement::ForwardSequence;
%ignore operations_research::SequenceVarElement::BackwardSequence;
%ignore operations_research::SequenceVarElement::Unperformed;
%ignore operations_research::Assignment::ForwardSequence;
%ignore operations_research::Assignment::BackwardSequence;
%ignore operations_research::Assignment::Unperformed;

// Generic rename rule.
%rename("%(camelcase)s", %$isfunction) "";

// Rename rule on DecisionBuilder;
%rename (NextWrap) operations_research::DecisionBuilder::Next;

// Rename rule on SearchLimit
%rename (IsCrossed) operations_research::SearchLimit::crossed;

// Rename rules on Solver.
%rename (Add) operations_research::Solver::AddConstraint;

// Rename rule on DisjunctiveConstraint.
%rename (SequenceVar) operations_research::DisjunctiveConstraint::MakeSequenceVar;

// Generic rename rules.
%rename (ToString) *::DebugString;

// Keep the .solver() API.
%rename (solver) *::solver;

// Rename NewSearch and EndSearch to add pinning.
%rename (NewSearchAux) operations_research::Solver::NewSearch;
%rename (EndSearchAux) operations_research::Solver::EndSearch;

%typemap(csinterfaces_derived) operations_research::Constraint "IConstraintWithStatus";
%typemap(cscode) operations_research::Constraint %{
  public static implicit operator IntVar(Constraint eq)
  {
    return eq.Var();
  }

  public static implicit operator IntExpr(Constraint eq)
  {
    return eq.Var();
  }
  public static IntExpr operator+(Constraint a, Constraint b) {
    return a.solver().MakeSum(a.Var(), b.Var());
  }
  public static IntExpr operator+(Constraint a, long v) {
    return a.solver().MakeSum(a.Var(), v);
  }
  public static IntExpr operator+(long v, Constraint a) {
    return a.solver().MakeSum(a.Var(), v);
  }
  public static IntExpr operator-(Constraint a, Constraint b) {
    return a.solver().MakeDifference(a.Var(), b.Var());
  }
  public static IntExpr operator-(Constraint a, long v) {
    return a.solver().MakeSum(a.Var(), -v);
  }
  public static IntExpr operator-(long v, Constraint a) {
    return a.solver().MakeDifference(v, a.Var());
  }
  public static IntExpr operator*(Constraint a, Constraint b) {
    return a.solver().MakeProd(a.Var(), b.Var());
  }
  public static IntExpr operator*(Constraint a, long v) {
    return a.solver().MakeProd(a.Var(), v);
  }
  public static IntExpr operator*(long v, Constraint a) {
    return a.solver().MakeProd(a.Var(), v);
  }
  public static IntExpr operator/(Constraint a, long v) {
    return a.solver().MakeDiv(a.Var(), v);
  }
  public static IntExpr operator-(Constraint a) {
    return a.solver().MakeOpposite(a.Var());
  }
  public IntExpr Abs() {
    return this.solver().MakeAbs(this.Var());
  }
  public IntExpr Square() {
    return this.solver().MakeSquare(this.Var());
  }
  public static WrappedConstraint operator ==(Constraint a, long v) {
    return new WrappedConstraint(a.solver().MakeEquality(a.Var(), v));
  }
  public static WrappedConstraint operator ==(long v, Constraint a) {
    return new WrappedConstraint(a.solver().MakeEquality(a.Var(), v));
  }
  public static WrappedConstraint operator !=(Constraint a, long v) {
    return new WrappedConstraint(a.solver().MakeNonEquality(a.Var(), v));
  }
  public static WrappedConstraint operator !=(long v, Constraint a) {
    return new WrappedConstraint(a.solver().MakeNonEquality(a.Var(), v));
  }
  public static WrappedConstraint operator >=(Constraint a, long v) {
    return new WrappedConstraint(a.solver().MakeGreaterOrEqual(a.Var(), v));
  }
  public static WrappedConstraint operator >=(long v, Constraint a) {
    return new WrappedConstraint(a.solver().MakeLessOrEqual(a.Var(), v));
  }
  public static WrappedConstraint operator >(Constraint a, long v) {
    return new WrappedConstraint(a.solver().MakeGreater(a.Var(), v));
  }
  public static WrappedConstraint operator >(long v, Constraint a) {
    return new WrappedConstraint(a.solver().MakeLess(a.Var(), v));
  }
  public static WrappedConstraint operator <=(Constraint a, long v) {
    return new WrappedConstraint(a.solver().MakeLessOrEqual(a.Var(), v));
  }
  public static WrappedConstraint operator <=(long v, Constraint a) {
    return new WrappedConstraint(a.solver().MakeGreaterOrEqual(a.Var(), v));
  }
  public static WrappedConstraint operator <(Constraint a, long v) {
    return new WrappedConstraint(a.solver().MakeLess(a.Var(), v));
  }
  public static WrappedConstraint operator <(long v, Constraint a) {
    return new WrappedConstraint(a.solver().MakeGreater(a.Var(), v));
  }
  public static WrappedConstraint operator >=(Constraint a, Constraint b) {
    return new WrappedConstraint(a.solver().MakeGreaterOrEqual(a.Var(), b.Var()));
  }
  public static WrappedConstraint operator >(Constraint a, Constraint b) {
    return new WrappedConstraint(a.solver().MakeGreater(a.Var(), b.Var()));
  }
  public static WrappedConstraint operator <=(Constraint a, Constraint b) {
    return new WrappedConstraint(a.solver().MakeLessOrEqual(a.Var(), b.Var()));
  }
  public static WrappedConstraint operator <(Constraint a, Constraint b) {
    return new WrappedConstraint(a.solver().MakeLess(a.Var(), b.Var()));
  }
  public static ConstraintEquality operator ==(Constraint a, Constraint b) {
    return new ConstraintEquality(a, b, true);
  }
  public static ConstraintEquality operator !=(Constraint a, Constraint b) {
    return new ConstraintEquality(a, b, false);
  }
%}


// Add arithmetic operators to integer expressions.
%typemap(cscode) operations_research::IntExpr %{
  public static IntExpr operator+(IntExpr a, IntExpr b) {
    return a.solver().MakeSum(a, b);
  }
  public static IntExpr operator+(IntExpr a, long v) {
    return a.solver().MakeSum(a, v);
  }
  public static IntExpr operator+(long v, IntExpr a) {
    return a.solver().MakeSum(a, v);
  }
  public static IntExpr operator-(IntExpr a, IntExpr b) {
    return a.solver().MakeDifference(a, b);
  }
  public static IntExpr operator-(IntExpr a, long v) {
    return a.solver().MakeSum(a, -v);
  }
  public static IntExpr operator-(long v, IntExpr a) {
    return a.solver().MakeDifference(v, a);
  }
  public static IntExpr operator*(IntExpr a, IntExpr b) {
    return a.solver().MakeProd(a, b);
  }
  public static IntExpr operator*(IntExpr a, long v) {
    return a.solver().MakeProd(a, v);
  }
  public static IntExpr operator*(long v, IntExpr a) {
    return a.solver().MakeProd(a, v);
  }
  public static IntExpr operator/(IntExpr a, long v) {
    return a.solver().MakeDiv(a, v);
  }
  public static IntExpr operator-(IntExpr a) {
    return a.solver().MakeOpposite(a);
  }
  public IntExpr Abs() {
    return this.solver().MakeAbs(this);
  }
  public IntExpr Square() {
    return this.solver().MakeSquare(this);
  }
  public static IntExprEquality operator ==(IntExpr a, IntExpr b) {
    return new IntExprEquality(a, b, true);
  }
  public static IntExprEquality operator !=(IntExpr a, IntExpr b) {
    return new IntExprEquality(a, b, false);
  }
  public static WrappedConstraint operator ==(IntExpr a, long v) {
    return new WrappedConstraint(a.solver().MakeEquality(a, v));
  }
  public static WrappedConstraint operator !=(IntExpr a, long v) {
    return new WrappedConstraint(a.solver().MakeNonEquality(a.Var(), v));
  }
  public static WrappedConstraint operator >=(IntExpr a, long v) {
    return new WrappedConstraint(a.solver().MakeGreaterOrEqual(a, v));
  }
  public static WrappedConstraint operator >(IntExpr a, long v) {
    return new WrappedConstraint(a.solver().MakeGreater(a, v));
  }
  public static WrappedConstraint operator <=(IntExpr a, long v) {
    return new WrappedConstraint(a.solver().MakeLessOrEqual(a, v));
  }
  public static WrappedConstraint operator <(IntExpr a, long v) {
    return new WrappedConstraint(a.solver().MakeLess(a, v));
  }
  public static WrappedConstraint operator >=(IntExpr a, IntExpr b) {
    return new WrappedConstraint(a.solver().MakeGreaterOrEqual(a.Var(), b.Var()));
  }
  public static WrappedConstraint operator >(IntExpr a, IntExpr b) {
    return new WrappedConstraint(a.solver().MakeGreater(a.Var(), b.Var()));
  }
  public static WrappedConstraint operator <=(IntExpr a, IntExpr b) {
    return new WrappedConstraint(a.solver().MakeLessOrEqual(a.Var(), b.Var()));
  }
  public static WrappedConstraint operator <(IntExpr a, IntExpr b) {
    return new WrappedConstraint(a.solver().MakeLess(a.Var(), b.Var()));
  }
%}

CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::IntVar, IntVar)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::SearchMonitor, SearchMonitor)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::DecisionBuilder, DecisionBuilder)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::IntervalVar, IntervalVar)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::SequenceVar, SequenceVar)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::LocalSearchOperator, LocalSearchOperator)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::LocalSearchFilter, LocalSearchFilter)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::SymmetryBreaker, SymmetryBreaker)

namespace operations_research {
%extend IntervalVar {
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
  Constraint* EndsAfter(int64 date) {
    return self->solver()->MakeIntervalVarRelation(self,
                                                   operations_research::Solver::ENDS_AFTER,
                                                   date);
  }
  Constraint* EndsAt(int64 date) {
    return self->solver()->MakeIntervalVarRelation(self,
                                                   operations_research::Solver::ENDS_AT,
                                                   date);
  }
  Constraint* EndsBefore(int64 date) {
    return self->solver()->MakeIntervalVarRelation(self,
                                                   operations_research::Solver::ENDS_BEFORE,
                                                   date);
  }
  Constraint* StartsAfter(int64 date) {
    return self->solver()->MakeIntervalVarRelation(self,
                                                   operations_research::Solver::STARTS_AFTER,
                                                   date);
  }
  Constraint* StartsAt(int64 date) {
    return self->solver()->MakeIntervalVarRelation(self,
                                                   operations_research::Solver::STARTS_AT,
                                                   date);
  }
  Constraint* StartsBefore(int64 date) {
    return self->solver()->MakeIntervalVarRelation(self,
                                                   operations_research::Solver::STARTS_BEFORE,
                                                   date);
  }
  Constraint* CrossesDate(int64 date) {
    return self->solver()->MakeIntervalVarRelation(self,
                                                   operations_research::Solver::CROSS_DATE,
                                                   date);
  }
  Constraint* AvoidsDate(int64 date) {
    return self->solver()->MakeIntervalVarRelation(self,
                                                   operations_research::Solver::AVOID_DATE,
                                                   date);
  }
  IntervalVar* RelaxedMax() {
    return self->solver()->MakeIntervalRelaxedMax(self);
  }
  IntervalVar* RelaxedMin() {
    return self->solver()->MakeIntervalRelaxedMin(self);
  }
}

%extend IntExpr {
  Constraint* MapTo(const std::vector<IntVar*>& vars) {
    return self->solver()->MakeMapDomain(self->Var(), vars);
  }
  IntExpr* IndexOf(const std::vector<int64>& vars) {
    return self->solver()->MakeElement(vars, self->Var());
  }
  IntExpr* IndexOf(const std::vector<IntVar*>& vars) {
    return self->solver()->MakeElement(vars, self->Var());
  }
  IntVar* IsEqual(int64 value) {
    return self->solver()->MakeIsEqualCstVar(self->Var(), value);
  }
  IntVar* IsDifferent(int64 value) {
    return self->solver()->MakeIsDifferentCstVar(self->Var(), value);
  }
  IntVar* IsGreater(int64 value) {
    return self->solver()->MakeIsGreaterCstVar(self->Var(), value);
  }
  IntVar* IsGreaterOrEqual(int64 value) {
    return self->solver()->MakeIsGreaterOrEqualCstVar(self->Var(), value);
  }
  IntVar* IsLess(int64 value) {
    return self->solver()->MakeIsLessCstVar(self->Var(), value);
  }
  IntVar* IsLessOrEqual(int64 value) {
    return self->solver()->MakeIsLessOrEqualCstVar(self->Var(), value);
  }
  IntVar* IsMember(const std::vector<int64>& values) {
    return self->solver()->MakeIsMemberVar(self->Var(), values);
  }
  IntVar* IsMember(const std::vector<int>& values) {
    return self->solver()->MakeIsMemberVar(self->Var(), values);
  }
  Constraint* Member(const std::vector<int64>& values) {
    return self->solver()->MakeMemberCt(self->Var(), values);
  }
  Constraint* Member(const std::vector<int>& values) {
    return self->solver()->MakeMemberCt(self->Var(), values);
  }
  IntVar* IsEqual(IntExpr* const other) {
    return self->solver()->MakeIsEqualVar(self->Var(), other->Var());
  }
  IntVar* IsDifferent(IntExpr* const other) {
    return self->solver()->MakeIsDifferentVar(self->Var(), other->Var());
  }
  IntVar* IsGreater(IntExpr* const other) {
    return self->solver()->MakeIsGreaterVar(self->Var(), other->Var());
  }
  IntVar* IsGreaterOrEqual(IntExpr* const other) {
    return self->solver()->MakeIsGreaterOrEqualVar(self->Var(), other->Var());
  }
  IntVar* IsLess(IntExpr* const other) {
    return self->solver()->MakeIsLessVar(self->Var(), other->Var());
  }
  IntVar* IsLessOrEqual(IntExpr* const other) {
    return self->solver()->MakeIsLessOrEqualVar(self->Var(), other->Var());
  }
  OptimizeVar* Minimize(long step) {
    return self->solver()->MakeMinimize(self->Var(), step);
  }
  OptimizeVar* Maximize(long step) {
    return self->solver()->MakeMaximize(self->Var(), step);
  }
}

%typemap(cscode) Solver %{
  public IntVar[] MakeIntVarArray(int count, long min, long max) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeIntVar(min, max);
    }
    return array;
  }

  public IntVar[] MakeIntVarArray(int count, long min, long max, string name) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      string var_name = name + i;
      array[i] = MakeIntVar(min, max, var_name);
    }
    return array;
  }

  public IntVar[] MakeIntVarArray(int count, long[] values) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeIntVar(values);
    }
    return array;
  }

  public IntVar[] MakeIntVarArray(int count, long[] values, string name) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      string var_name = name + i;
      array[i] = MakeIntVar(values, var_name);
    }
    return array;
  }

  public IntVar[] MakeIntVarArray(int count, int[] values) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeIntVar(values);
    }
    return array;
  }

  public IntVar[] MakeIntVarArray(int count, int[] values, string name) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      string var_name = name + i;
      array[i] = MakeIntVar(values, var_name);
    }
    return array;
  }

  public IntVar[] MakeBoolVarArray(int count) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeBoolVar();
    }
    return array;
  }

  public IntVar[] MakeBoolVarArray(int count, string name) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      string var_name = name + i;
      array[i] = MakeBoolVar(var_name);
    }
    return array;
  }

  public IntVar[,] MakeIntVarMatrix(int rows, int cols, long min, long max) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        array[i,j] = MakeIntVar(min, max);
      }
    }
    return array;
  }

  public IntVar[,] MakeIntVarMatrix(int rows, int cols,
                                    long min, long max, string name) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        string var_name = name + "["+ i + ", " + j +"]";
        array[i,j] = MakeIntVar(min, max, var_name);
      }
    }
    return array;
  }

  public IntVar[,] MakeIntVarMatrix(int rows, int cols, long[] values) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        array[i,j] = MakeIntVar(values);
      }
    }
    return array;
  }

  public IntVar[,] MakeIntVarMatrix(int rows, int cols,
                                    long[] values, string name) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        string var_name = name + "["+ i + ", " + j +"]";
        array[i,j] = MakeIntVar(values, var_name);
      }
    }
    return array;
  }

  public IntVar[,] MakeIntVarMatrix(int rows, int cols, int[] values) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        array[i,j] = MakeIntVar(values);
      }
    }
    return array;
  }

  public IntVar[,] MakeIntVarMatrix(int rows, int cols,
                                    int[] values, string name) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        string var_name = name + "["+ i + ", " + j +"]";
        array[i,j] = MakeIntVar(values, var_name);
      }
    }
    return array;
  }

  public IntVar[,] MakeBoolVarMatrix(int rows, int cols) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        array[i,j] = MakeBoolVar();
      }
    }
    return array;
  }

  public IntVar[,] MakeBoolVarMatrix(int rows, int cols, string name) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        string var_name = name + "["+ i + ", " + j +"]";
        array[i,j] = MakeBoolVar(var_name);
      }
    }
    return array;
  }

  public IntervalVar[] MakeFixedDurationIntervalVarArray(int count,
                                                         long start_min,
                                                         long start_max,
                                                         long duration,
                                                         bool optional) {
    IntervalVar[] array = new IntervalVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeFixedDurationIntervalVar(start_min,
                                              start_max,
                                              duration,
                                              optional,
                                              "");
    }
    return array;
  }

  public IntervalVar[] MakeFixedDurationIntervalVarArray(int count,
                                                         long start_min,
                                                         long start_max,
                                                         long duration,
                                                         bool optional,
                                                         string name) {
    IntervalVar[] array = new IntervalVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeFixedDurationIntervalVar(start_min,
                                              start_max,
                                              duration,
                                              optional,
                                              name + i);
    }
    return array;
  }

  public IntervalVar[] MakeFixedDurationIntervalVarArray(int count,
                                                         long[] start_min,
                                                         long[] start_max,
                                                         long[] duration,
                                                         bool optional,
                                                         string name) {
    IntervalVar[] array = new IntervalVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeFixedDurationIntervalVar(start_min[i],
                                              start_max[i],
                                              duration[i],
                                              optional,
                                              name + i);
    }
    return array;
  }

  public IntervalVar[] MakeFixedDurationIntervalVarArray(int count,
                                                         int[] start_min,
                                                         int[] start_max,
                                                         int[] duration,
                                                         bool optional,
                                                         string name) {
    IntervalVar[] array = new IntervalVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeFixedDurationIntervalVar(start_min[i],
                                              start_max[i],
                                              duration[i],
                                              optional,
                                              name + i);
    }
    return array;
  }
  public IntervalVar[] MakeFixedDurationIntervalVarArray(IntVar[] starts,
                                                         int[] durations,
                                                         string name) {
    int count = starts.Length;
    IntervalVar[] array = new IntervalVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeFixedDurationIntervalVar(starts[i],
                                              durations[i],
                                              name + i);
    }
    return array;
  }
  public IntervalVar[] MakeFixedDurationIntervalVarArray(IntVar[] starts,
                                                         long[] durations,
                                                         string name) {
    int count = starts.Length;
    IntervalVar[] array = new IntervalVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeFixedDurationIntervalVar(starts[i],
                                              durations[i],
                                              name + i);
    }
    return array;
  }
  public void NewSearch(DecisionBuilder db) {
    pinned_decision_builder_ = db;
    pinned_search_monitors_.Clear();
    NewSearchAux(db);
  }

  public void NewSearch(DecisionBuilder db, SearchMonitor sm1) {
    pinned_decision_builder_ = db;
    pinned_search_monitors_.Clear();
    pinned_search_monitors_.Add(sm1);
    NewSearchAux(db, sm1);
  }


  public void NewSearch(DecisionBuilder db,
                        SearchMonitor sm1,
                        SearchMonitor sm2) {
    pinned_decision_builder_ = db;
    pinned_search_monitors_.Clear();
    pinned_search_monitors_.Add(sm1);
    pinned_search_monitors_.Add(sm2);
    NewSearchAux(db, sm1, sm2);
  }

  public void NewSearch(DecisionBuilder db,
                        SearchMonitor sm1,
                        SearchMonitor sm2,
                        SearchMonitor sm3) {
    pinned_decision_builder_ = db;
    pinned_search_monitors_.Clear();
    pinned_search_monitors_.Add(sm1);
    pinned_search_monitors_.Add(sm2);
    pinned_search_monitors_.Add(sm3);
    NewSearchAux(db, sm1, sm2, sm3);
  }

  public void NewSearch(DecisionBuilder db,
                        SearchMonitor sm1,
                        SearchMonitor sm2,
                        SearchMonitor sm3,
                        SearchMonitor sm4) {
    pinned_decision_builder_ = db;
    pinned_search_monitors_.Clear();
    pinned_search_monitors_.Add(sm1);
    pinned_search_monitors_.Add(sm2);
    pinned_search_monitors_.Add(sm3);
    pinned_search_monitors_.Add(sm4);
    NewSearchAux(db, sm1, sm2, sm3, sm4);
  }

  public void NewSearch(DecisionBuilder db, SearchMonitor[] monitors) {
    pinned_decision_builder_ = db;
    pinned_search_monitors_.Clear();
    pinned_search_monitors_.AddRange(monitors);
    NewSearchAux(db, monitors);
  }

  public void EndSearch() {
    pinned_decision_builder_ = null;
    pinned_search_monitors_.Clear();
    EndSearchAux();
  }

  private System.Collections.Generic.List<SearchMonitor> pinned_search_monitors_
      = new System.Collections.Generic.List<SearchMonitor>();
  private DecisionBuilder pinned_decision_builder_;
%}

%extend IntVarLocalSearchFilter {
  int Index(IntVar* const var) {
    int64 index = -1;
    self->FindIndex(var, &index);
    return index;
  }
}
%extend SequenceVarLocalSearchOperator {
  int SequenceSize(int index) const {
    return self->Sequence(index).size();
  }
  int SequenceValueAt(int index, int position) const {
    return self->Sequence(index)[position];
  }
  int OldSequenceSize(int index) const {
    return self->OldSequence(index).size();
  }
  int OldSequenceValueAt(int index, int position) const {
    return self->OldSequence(index)[position];
  }
}

%typemap(cscode) SequenceVarLocalSearchOperator %{
  public int[] Sequence(int index) {
    int size = SequenceSize(index);
    int[] result = new int[size];
    for (int position = 0; position < size; ++position) {
      result[position] = SequenceValueAt(index, position);
    }
    return result;
  }

  public int[] OldSequence(int index) {
    int size = OldSequenceSize(index);
    int[] result = new int[size];
    for (int position = 0; position < size; ++position) {
      result[position] = OldSequenceValueAt(index, position);
    }
    return result;
  }
%}
}  // namespace operations_research

namespace operations_research {
class LocalSearchPhaseParameters {
 public:
  LocalSearchPhaseParameters();
  ~LocalSearchPhaseParameters();
};
}  // namespace operations_research

#endif  // SWIGCSHARP

// Wrap cp includes
%include constraint_solver/constraint_solver.h
%include constraint_solver/constraint_solveri.h

// Define templates instantiation after wrapping.
namespace operations_research {
%template(RevInteger) Rev<int64>;
%template(RevBool) Rev<bool>;
typedef Assignment::AssignmentContainer AssignmentContainer;
%template(AssignmentIntContainer) AssignmentContainer<IntVar, IntVarElement>;
}
