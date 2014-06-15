// TODO(user): Refactor this file to adhere to the SWIG style guide.

%include exception.i
%include util/python/data.swig
%include util/python/tuple_set.swig

// ----- Callback Wrapping -----
%{
#include "base/callback.h"  // TODO(user): switch to std::function
%}
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
}  // namespace operations_research

%module(directors="1") operations_research

%feature("director") operations_research::SearchMonitor;
%feature("director") operations_research::Constraint;
%feature("director") operations_research::Demon;
%feature("director") operations_research::BaseLNS;
%feature("director") operations_research::LocalSearchOperator;
%feature("director") operations_research::PathOperator;
%feature("director") operations_research::IntVarLocalSearchOperator;
%feature("director") operations_research::SequenceVarLocalSearchOperator;
%feature("director") operations_research::IntVarLocalSearchFilter;

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
import gflags as flags
import ortools.constraint_solver.assignment_pb2
import ortools.constraint_solver.model_pb2
import ortools.constraint_solver.search_limit_pb2

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
}
}  // namespace operations_research

// Rename rules for directors
%rename (InitialPropagateWrapper)
    operations_research::Constraint::InitialPropagate;
%rename (RunWrapper) operations_research::Demon::Run;
%rename (NextFragmentWrapper) operations_research::BaseLNS::NextFragment;

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

%rename (DomainIteratorAux) IntVar::MakeDomainIterator;
%rename (HoleIteratorAux) IntVar::MakeHoleIterator;

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

%extend IntVarIterator {
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
  }
}

%extend IntVar {
  %pythoncode {
  def DomainIterator(self):
    return iter(self.DomainIteratorAux(False))

  def HoleIterator(self):
    return iter(self.HoleIteratorAux(False))
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

// ############ BEGIN DUPLICATED CODE BLOCK ############
// IMPORTANT: keep this code block in sync with the .swig
// files in ../java and ../csharp.

// Protect from failure.
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
    PyErr_SetString(PyExc_IndexError, "fail");
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
#undef PROTECT_FROM_FAILURE
}  // namespace operations_research

// ############ END DUPLICATED CODE BLOCK ############

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


class PyConstraint(Constraint):

  def __init__(self, solver):
    Constraint.__init__(self, solver)

  def InitialPropagateWrapper(self):
    try:
      self.InitialPropagate()
    except:
      self.solver().ShouldFail()

  def DebugString(self):
    return "PyConstraint"


class PyDemon(Demon):

  def __init__(self):
    Demon.__init__(self)

  def RunWrapper(self, solver):
    try:
      self.Run(solver)
    except:
      solver.ShouldFail()

  def DebugString(self):
    return "PyDemon"


class PyLns(BaseLNS):

  def __init__(self, vars):
    BaseLNS.__init__(self, vars)

  def NextFragment(self):
    print 'in PyLns.NextFragment'
    return []

  def InitFragments(self):
    print 'in PyLns.InitFragments'
    pass

  def NextFragmentWrapper(self, fragment):
    print 'in PyLns.NextFragmentWrapper'
    print 'in NextFragmentWrapper'
    fragment.clear()
    f = self.NextFragment()
    if not f:
      return False
    else:
      for v in f:
        fragment.push_back(v)
      return True
}
