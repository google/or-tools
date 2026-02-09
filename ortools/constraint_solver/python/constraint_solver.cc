// Copyright 2010-2025 Google LLC
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

#include "ortools/constraint_solver/constraint_solver.h"

#include <setjmp.h>

#include <cstdint>
#include <string>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/functional/function_ref.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/assignment.pb.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/python/constraint_solver_doc.h"
#include "ortools/constraint_solver/search_limit.pb.h"
#include "ortools/util/piecewise_linear_function.h"
#include "ortools/util/tuple_set.h"
#include "pybind11/cast.h"
#include "pybind11/functional.h"
#include "pybind11/gil.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11_protobuf/native_proto_caster.h"

namespace py = ::pybind11;

using ::operations_research::Assignment;
using ::operations_research::AssignmentElement;
using ::operations_research::AssignmentProto;
using ::operations_research::BaseObject;
using ::operations_research::Constraint;
using ::operations_research::ConstraintSolverParameters;
using ::operations_research::Decision;
using ::operations_research::DecisionBuilder;
using ::operations_research::DecisionVisitor;
using ::operations_research::DefaultPhaseParameters;
using ::operations_research::Demon;
using ::operations_research::DisjunctiveConstraint;
using ::operations_research::IntervalVar;
using ::operations_research::IntervalVarElement;
using ::operations_research::IntExpr;
using ::operations_research::IntTupleSet;
using ::operations_research::IntVar;
using ::operations_research::IntVarElement;
using ::operations_research::IntVarIterator;
using ::operations_research::LocalSearchFilterManager;
using ::operations_research::LocalSearchOperator;
using ::operations_research::LocalSearchPhaseParameters;
using ::operations_research::ModelVisitor;
using ::operations_research::NumericalRev;
using ::operations_research::OptimizeVar;
using ::operations_research::Pack;
using ::operations_research::PropagationBaseObject;
using ::operations_research::RegularLimit;
using ::operations_research::RegularLimitParameters;
using ::operations_research::Rev;
using ::operations_research::SearchLimit;
using ::operations_research::SearchMonitor;
using ::operations_research::SequenceVar;
using ::operations_research::SequenceVarElement;
using ::operations_research::SolutionCollector;
using ::operations_research::Solver;

// There is no proper error propagation in `constraint_solver` but some
// operation may fail and end-up calling `Solver::Fail()`. `Solver` offers a
// `set_fail_intercept` method we can use to _break_ and return control flow to
// the caller in this case. Theoretically, we could just `set_fail_intercept` to
// `throw` but `Solver` is not compiled with exception enabled, this would
// result in a crash. Instead we use `setjmp`/`longjump` to resume the control
// flow in the function below and throw from here. This is quite convoluted but
// a cleaner solution would require rewriting the API.
template <typename T>
void ThrowOnFailure(T* this_, Solver* solver,
                    absl::FunctionRef<void(T*)> action) {
  jmp_buf buffer;
  solver->set_fail_intercept([&buffer]() { longjmp(buffer, 1); });
  auto cleanup =
      absl::MakeCleanup([solver]() { solver->clear_fail_intercept(); });
  if (setjmp(buffer) == 0) {
    // If the statement below end-up calling `Solver::Fail()`, `longjmp` is
    // executed and resets execution to the if-condition above. The code then
    // branches in the `else` clause below and throws.
    action(this_);
  } else {
    throw py::value_error("Solver fails outside of solve()");
  }
}

template <typename T, typename R>
R* ThrowOnFailureOrReturn(T* this_, Solver* solver,
                          absl::FunctionRef<R*(T*)> action) {
  jmp_buf buffer;
  solver->set_fail_intercept([&buffer]() { longjmp(buffer, 1); });
  auto cleanup =
      absl::MakeCleanup([solver]() { solver->clear_fail_intercept(); });
  if (setjmp(buffer) == 0) {
    // If the statement below end-up calling `Solver::Fail()`, `longjmp` is
    // executed and resets execution to the if-condition above. The code then
    // branches in the `else` clause below and throws.
    return action(this_);
  } else {
    throw py::value_error("Solver fails outside of solve()");
  }
}

class BaseObjectPythonHelper {
 public:
  static std::string DebugString(BaseObject* this_) {
    return this_->DebugString();
  }
};

class PropagationBaseObjectPythonHelper : BaseObjectPythonHelper {
 public:
  static std::string DebugString(PropagationBaseObject* this_) {
    return this_->DebugString();
  }

  static Solver* solver(PropagationBaseObject* this_) {
    return this_->solver();
  }

  static std::string name(PropagationBaseObject* this_) {
    return this_->name();
  }

  static void SetName(PropagationBaseObject* this_, absl::string_view name) {
    this_->set_name(name);
  }
};

class IntExprPythonHelper : PropagationBaseObjectPythonHelper {
 public:
  static int64_t Min(IntExpr* this_) { return this_->Min(); }

  static int64_t Max(IntExpr* this_) { return this_->Max(); }

  static void SetMin(IntExpr* this_, int64_t m) {
    ThrowOnFailure<IntExpr>(this_, this_->solver(),
                            [m](IntExpr* this_) { this_->SetMin(m); });
  }

  static void SetMax(IntExpr* this_, int64_t m) {
    ThrowOnFailure<IntExpr>(this_, this_->solver(),
                            [m](IntExpr* this_) { this_->SetMax(m); });
  }

  static void SetRange(IntExpr* this_, int64_t mi, int64_t ma) {
    ThrowOnFailure<IntExpr>(this_, this_->solver(), [mi, ma](IntExpr* this_) {
      this_->SetRange(mi, ma);
    });
  }

  static void SetValue(IntExpr* this_, int64_t v) {
    ThrowOnFailure<IntExpr>(this_, this_->solver(),
                            [v](IntExpr* this_) { this_->SetValue(v); });
  }

  static bool Bound(IntExpr* this_) { return this_->Bound(); }
};

class IntVarPythonHelper : IntExprPythonHelper {
 public:
  static std::string name(IntVar* this_) { return this_->name(); }

  static int64_t Value(IntVar* this_) { return this_->Value(); }

  static void RemoveValue(IntVar* this_, int64_t v) {
    ThrowOnFailure<IntVar>(this_, this_->solver(),
                           [v](IntVar* this_) { this_->RemoveValue(v); });
  }

  static void RemoveValues(IntVar* this_, const std::vector<int64_t>& values) {
    ThrowOnFailure<IntVar>(this_, this_->solver(), [&values](IntVar* this_) {
      this_->RemoveValues(values);
    });
  }

  static int64_t Size(IntVar* this_) { return this_->Size(); }
};

class PyDecision : public Decision {
 public:
  using Decision::Decision;  // Inherit constructors
  virtual ~PyDecision() = default;
  virtual void apply(Solver* s) {}
  virtual void refute(Solver* s) {}
  virtual void accept(DecisionVisitor* v) const {}
  virtual std::string debug_string() const { return "PyDecision"; }
  void Apply(Solver* s) override {
    try {
      ThrowOnFailure<PyDecision>(this, s,
                                 [s](PyDecision* db) { db->apply(s); });
    } catch (const py::error_already_set& e) {
      if (e.matches(PyExc_ValueError) &&
          absl::StrContains(e.what(), "Solver fails outside of solve")) {
        s->Fail();
      }
      throw;
    }
  }
  void Refute(Solver* s) override {
    try {
      ThrowOnFailure<PyDecision>(this, s,
                                 [s](PyDecision* db) { db->refute(s); });
    } catch (const py::error_already_set& e) {
      if (e.matches(PyExc_ValueError) &&
          absl::StrContains(e.what(), "Solver fails outside of solve")) {
        s->Fail();
      }
      throw;
    }
  }
  void Accept(DecisionVisitor* visitor) const override { accept(visitor); }
  std::string DebugString() const override { return debug_string(); }
};

class PyDecisionHelper : public PyDecision {
 public:
  using PyDecision::PyDecision;  // Inherit constructors

  void apply(Solver* s) override {
    PYBIND11_OVERRIDE(void, PyDecision, apply, s);
  }
  void refute(Solver* s) override {
    PYBIND11_OVERRIDE(void, PyDecision, refute, s);
  }
  void accept(DecisionVisitor* visitor) const override {
    PYBIND11_OVERRIDE(void, PyDecision, accept, visitor);
  }
  std::string debug_string() const override {
    PYBIND11_OVERRIDE(std::string, PyDecision, debug_string, );
  }
};

class PyDecisionBuilder : public DecisionBuilder {
 public:
  using DecisionBuilder::DecisionBuilder;
  virtual ~PyDecisionBuilder() = default;
  virtual Decision* next(Solver* s) { return nullptr; }
  virtual std::string debug_string() const { return "PyDecisionBuilder"; }
  Decision* Next(Solver* s) override {
    try {
      return ThrowOnFailureOrReturn<PyDecisionBuilder, Decision>(
          this, s, [s](PyDecisionBuilder* db) { return db->next(s); });
    } catch (const py::error_already_set& e) {
      if (e.matches(PyExc_ValueError) &&
          absl::StrContains(e.what(), "Solver fails outside of solve")) {
        s->Fail();
      }
      throw;
    }
  }
  std::string DebugString() const override { return debug_string(); }
};

class PyDecisionBuilderHelper : public PyDecisionBuilder {
 public:
  using PyDecisionBuilder::PyDecisionBuilder;
  Decision* next(Solver* s) override {
    auto override = pybind11::get_override(this, "next");
    if (override) {
      try {
        return override(s).cast<Decision*>();
      } catch (const py::error_already_set& e) {
        if (e.matches(PyExc_ValueError) &&
            absl::StrContains(e.what(), "Solver fails outside of solve")) {
          s->Fail();
        }
        throw;
      }
    }
    return PyDecisionBuilder::next(s);
  }
  std::string debug_string() const override {
    PYBIND11_OVERRIDE(std::string, PyDecisionBuilder, debug_string, );
  }
};

class PySearchMonitor : public SearchMonitor {
 public:
  using SearchMonitor::SearchMonitor;
  void EnterSearch() override {
    PYBIND11_OVERRIDE_NAME(void, SearchMonitor, "enter_search", EnterSearch, );
  }
  void RestartSearch() override {
    PYBIND11_OVERRIDE_NAME(void, SearchMonitor, "restart_search",
                           RestartSearch, );
  }
  void ExitSearch() override {
    PYBIND11_OVERRIDE_NAME(void, SearchMonitor, "exit_search", ExitSearch, );
  }
  void BeginNextDecision(DecisionBuilder* b) override {
    PYBIND11_OVERRIDE_NAME(void, SearchMonitor, "begin_next_decision",
                           BeginNextDecision, b);
  }
  void EndNextDecision(DecisionBuilder* b, Decision* d) override {
    PYBIND11_OVERRIDE_NAME(void, SearchMonitor, "end_next_decision",
                           EndNextDecision, b, d);
  }
  void ApplyDecision(Decision* d) override {
    PYBIND11_OVERRIDE_NAME(void, SearchMonitor, "apply_decision", ApplyDecision,
                           d);
  }
  void RefuteDecision(Decision* d) override {
    PYBIND11_OVERRIDE_NAME(void, SearchMonitor, "refute_decision",
                           RefuteDecision, d);
  }
  void AfterDecision(Decision* d, bool apply) override {
    PYBIND11_OVERRIDE_NAME(void, SearchMonitor, "after_decision", AfterDecision,
                           d, apply);
  }
  void BeginFail() override {
    PYBIND11_OVERRIDE_NAME(void, SearchMonitor, "begin_fail", BeginFail, );
  }
  void EndFail() override {
    PYBIND11_OVERRIDE_NAME(void, SearchMonitor, "end_fail", EndFail, );
  }
  void BeginInitialPropagation() override {
    PYBIND11_OVERRIDE_NAME(void, SearchMonitor, "begin_initial_propagation",
                           BeginInitialPropagation, );
  }
  void EndInitialPropagation() override {
    PYBIND11_OVERRIDE_NAME(void, SearchMonitor, "end_initial_propagation",
                           EndInitialPropagation, );
  }
  bool AcceptSolution() override {
    PYBIND11_OVERRIDE_NAME(bool, SearchMonitor, "accept_solution",
                           AcceptSolution, );
  }
  bool AtSolution() override {
    PYBIND11_OVERRIDE_NAME(bool, SearchMonitor, "at_solution", AtSolution, );
  }
  void NoMoreSolutions() override {
    PYBIND11_OVERRIDE_NAME(void, SearchMonitor, "no_more_solutions",
                           NoMoreSolutions, );
  }
  bool AtLocalOptimum() override {
    PYBIND11_OVERRIDE_NAME(bool, SearchMonitor, "at_local_optimum",
                           AtLocalOptimum, );
  }
  bool AcceptDelta(Assignment* delta, Assignment* deltadelta) override {
    PYBIND11_OVERRIDE_NAME(bool, SearchMonitor, "accept_delta", AcceptDelta,
                           delta, deltadelta);
  }
  void AcceptNeighbor() override {
    PYBIND11_OVERRIDE_NAME(void, SearchMonitor, "accept_neighbor",
                           AcceptNeighbor, );
  }
  void AcceptUncheckedNeighbor() override {
    PYBIND11_OVERRIDE_NAME(void, SearchMonitor, "accept_unchecked_neighbor",
                           AcceptUncheckedNeighbor, );
  }
  bool IsUncheckedSolutionLimitReached() override {
    PYBIND11_OVERRIDE_NAME(bool, SearchMonitor,
                           "is_unchecked_solution_limit_reached",
                           IsUncheckedSolutionLimitReached, );
  }
  void PeriodicCheck() override {
    PYBIND11_OVERRIDE_NAME(void, SearchMonitor, "periodic_check",
                           PeriodicCheck, );
  }
  int ProgressPercent() override {
    PYBIND11_OVERRIDE_NAME(int, SearchMonitor, "progress_percent",
                           ProgressPercent, );
  }
  void Accept(ModelVisitor* visitor) const override {
    PYBIND11_OVERRIDE_NAME(void, SearchMonitor, "accept", Accept, visitor);
  }
  void Install() override {
    PYBIND11_OVERRIDE_NAME(void, SearchMonitor, "install", Install, );
  }
  std::string DebugString() const override {
    PYBIND11_OVERRIDE_NAME(std::string, SearchMonitor, "__str__",
                           DebugString, );
  }
};

std::vector<IntVar*> ToIntVarArray(
    const std::vector<PropagationBaseObject*>& arguments) {
  std::vector<IntVar*> vars;
  vars.reserve(arguments.size());
  for (BaseObject* arg : arguments) {
    IntExpr* expr = dynamic_cast<IntExpr*>(arg);
    if (expr != nullptr) {
      vars.push_back(expr->Var());
    } else {
      Constraint* constraint = dynamic_cast<Constraint*>(arg);
      if (constraint != nullptr) {
        IntVar* var = constraint->Var();
        if (var != nullptr) {
          vars.push_back(var);
        } else {
          PyErr_SetString(
              PyExc_TypeError,
              absl::StrCat("Constraint cannot be cast to an IntVar: '",
                           arg->DebugString(), "'")
                  .c_str());
          throw py::error_already_set();
        }
      } else {
        PyErr_SetString(
            PyExc_TypeError,
            absl::StrCat(
                "Model argument should be castable to an IntVar, got: '",
                arg->DebugString(), "'")
                .c_str());
        throw py::error_already_set();
      }
    }
  }
  return vars;
}

PYBIND11_MODULE(constraint_solver, m) {
  pybind11_protobuf::ImportNativeProtoCasters();

  py::class_<Rev<bool>>(m, "RevBool")
      .def(py::init<bool>(), py::arg("val"))
      .def("value", &Rev<bool>::Value)
      .def("set_value", &Rev<bool>::SetValue, py::arg("s"), py::arg("val"));

  py::class_<NumericalRev<int64_t>>(m, "RevInteger")
      .def(py::init<int64_t>(), py::arg("val"))
      .def("value", &NumericalRev<int64_t>::Value)
      .def("set_value", &NumericalRev<int64_t>::SetValue, py::arg("s"),
           py::arg("val"))
      .def("add", &NumericalRev<int64_t>::Add, py::arg("s"), py::arg("to_add"))
      .def("incr", &NumericalRev<int64_t>::Incr, py::arg("s"))
      .def("decr", &NumericalRev<int64_t>::Decr, py::arg("s"));

  py::class_<DefaultPhaseParameters> default_phase_parameters(
      m, "DefaultPhaseParameters");
  default_phase_parameters.def(py::init<>())
      .def_readwrite("var_selection_schema",
                     &DefaultPhaseParameters::var_selection_schema)
      .def_readwrite("value_selection_schema",
                     &DefaultPhaseParameters::value_selection_schema)
      .def_readwrite("initialization_splits",
                     &DefaultPhaseParameters::initialization_splits)
      .def_readwrite("run_all_heuristics",
                     &DefaultPhaseParameters::run_all_heuristics)
      .def_readwrite("heuristic_period",
                     &DefaultPhaseParameters::heuristic_period)
      .def_readwrite("heuristic_num_failures_limit",
                     &DefaultPhaseParameters::heuristic_num_failures_limit)
      .def_readwrite("persistent_impact",
                     &DefaultPhaseParameters::persistent_impact)
      .def_readwrite("random_seed", &DefaultPhaseParameters::random_seed)
      .def_readwrite("display_level", &DefaultPhaseParameters::display_level)
      .def_readwrite("use_last_conflict",
                     &DefaultPhaseParameters::use_last_conflict)
      .def_readwrite("decision_builder",
                     &DefaultPhaseParameters::decision_builder);

  py::enum_<DefaultPhaseParameters::VariableSelection>(default_phase_parameters,
                                                       "VariableSelection")
      .value("CHOOSE_MAX_SUM_IMPACT",
             DefaultPhaseParameters::CHOOSE_MAX_SUM_IMPACT)
      .value("CHOOSE_MAX_AVERAGE_IMPACT",
             DefaultPhaseParameters::CHOOSE_MAX_AVERAGE_IMPACT)
      .value("CHOOSE_MAX_VALUE_IMPACT",
             DefaultPhaseParameters::CHOOSE_MAX_VALUE_IMPACT)
      .export_values();

  py::enum_<DefaultPhaseParameters::ValueSelection>(default_phase_parameters,
                                                    "ValueSelection")
      .value("SELECT_MIN_IMPACT", DefaultPhaseParameters::SELECT_MIN_IMPACT)
      .value("SELECT_MAX_IMPACT", DefaultPhaseParameters::SELECT_MAX_IMPACT)
      .export_values();

  py::enum_<DefaultPhaseParameters::DisplayLevel>(default_phase_parameters,
                                                  "DisplayLevel")
      .value("NONE", DefaultPhaseParameters::NONE)
      .value("NORMAL", DefaultPhaseParameters::NORMAL)
      .value("VERBOSE", DefaultPhaseParameters::VERBOSE)
      .export_values();

  py::enum_<Solver::DemonPriority>(m, "DemonPriority")
      .value("DELAYED_PRIORITY", Solver::DELAYED_PRIORITY)
      .value("VAR_PRIORITY", Solver::VAR_PRIORITY)
      .value("NORMAL_PRIORITY", Solver::NORMAL_PRIORITY)
      .export_values();

  py::enum_<Solver::SequenceStrategy>(m, "SequenceStrategy")
      .value("SEQUENCE_DEFAULT", Solver::SEQUENCE_DEFAULT)
      .value("SEQUENCE_SIMPLE", Solver::SEQUENCE_SIMPLE)
      .value("CHOOSE_MIN_SLACK_RANK_FORWARD",
             Solver::CHOOSE_MIN_SLACK_RANK_FORWARD)
      .value("CHOOSE_RANDOM_RANK_FORWARD", Solver::CHOOSE_RANDOM_RANK_FORWARD)
      .export_values();

  py::enum_<Solver::IntervalStrategy>(m, "IntervalStrategy")
      .value("INTERVAL_DEFAULT", Solver::INTERVAL_DEFAULT)
      .value("INTERVAL_SIMPLE", Solver::INTERVAL_SIMPLE)
      .value("INTERVAL_SET_TIMES_FORWARD", Solver::INTERVAL_SET_TIMES_FORWARD)
      .value("INTERVAL_SET_TIMES_BACKWARD", Solver::INTERVAL_SET_TIMES_BACKWARD)
      .export_values();

  py::enum_<Solver::LocalSearchOperators>(m, "LocalSearchOperators")
      .value("TWOOPT", Solver::TWOOPT)
      .value("OROPT", Solver::OROPT)
      .value("RELOCATE", Solver::RELOCATE)
      .value("EXCHANGE", Solver::EXCHANGE)
      .value("CROSS", Solver::CROSS)
      .value("MAKEACTIVE", Solver::MAKEACTIVE)
      .value("MAKEINACTIVE", Solver::MAKEINACTIVE)
      .value("MAKECHAININACTIVE", Solver::MAKECHAININACTIVE)
      .value("SWAPACTIVE", Solver::SWAPACTIVE)
      .value("EXTENDEDSWAPACTIVE", Solver::EXTENDEDSWAPACTIVE)
      .value("PATHLNS", Solver::PATHLNS)
      .value("FULLPATHLNS", Solver::FULLPATHLNS)
      .value("UNACTIVELNS", Solver::UNACTIVELNS)
      .value("INCREMENT", Solver::INCREMENT)
      .value("DECREMENT", Solver::DECREMENT)
      .value("SIMPLELNS", Solver::SIMPLELNS)
      .export_values();

  py::enum_<Solver::LocalSearchFilterBound>(m, "LocalSearchFilterBound")
      .value("GE", Solver::GE)
      .value("LE", Solver::LE)
      .value("EQ", Solver::EQ)
      .export_values();

  py::enum_<Solver::IntVarStrategy>(m, "IntVarStrategy")
      .value("INT_VAR_DEFAULT", Solver::INT_VAR_DEFAULT)
      .value("INT_VAR_SIMPLE", Solver::INT_VAR_SIMPLE)
      .value("CHOOSE_FIRST_UNBOUND", Solver::CHOOSE_FIRST_UNBOUND)
      .value("CHOOSE_RANDOM", Solver::CHOOSE_RANDOM)
      .value("CHOOSE_MIN_SIZE_LOWEST_MIN", Solver::CHOOSE_MIN_SIZE_LOWEST_MIN)
      .value("CHOOSE_MIN_SIZE_HIGHEST_MIN", Solver::CHOOSE_MIN_SIZE_HIGHEST_MIN)
      .value("CHOOSE_MIN_SIZE_LOWEST_MAX", Solver::CHOOSE_MIN_SIZE_LOWEST_MAX)
      .value("CHOOSE_MIN_SIZE_HIGHEST_MAX", Solver::CHOOSE_MIN_SIZE_HIGHEST_MAX)
      .value("CHOOSE_LOWEST_MIN", Solver::CHOOSE_LOWEST_MIN)
      .value("CHOOSE_HIGHEST_MAX", Solver::CHOOSE_HIGHEST_MAX)
      .value("CHOOSE_MIN_SIZE", Solver::CHOOSE_MIN_SIZE)
      .value("CHOOSE_MAX_SIZE", Solver::CHOOSE_MAX_SIZE)
      .value("CHOOSE_MAX_REGRET_ON_MIN", Solver::CHOOSE_MAX_REGRET_ON_MIN)
      .value("CHOOSE_PATH", Solver::CHOOSE_PATH)
      .export_values();

  py::enum_<Solver::IntValueStrategy>(m, "IntValueStrategy")
      .value("INT_VALUE_DEFAULT", Solver::INT_VALUE_DEFAULT)
      .value("INT_VALUE_SIMPLE", Solver::INT_VALUE_SIMPLE)
      .value("ASSIGN_MIN_VALUE", Solver::ASSIGN_MIN_VALUE)
      .value("ASSIGN_MAX_VALUE", Solver::ASSIGN_MAX_VALUE)
      .value("ASSIGN_RANDOM_VALUE", Solver::ASSIGN_RANDOM_VALUE)
      .value("ASSIGN_CENTER_VALUE", Solver::ASSIGN_CENTER_VALUE)
      .value("SPLIT_LOWER_HALF", Solver::SPLIT_LOWER_HALF)
      .value("SPLIT_UPPER_HALF", Solver::SPLIT_UPPER_HALF)
      .export_values();

  pybind11::enum_<Solver::UnaryIntervalRelation>(m, "UnaryIntervalRelation")
      .value("ENDS_AFTER", Solver::ENDS_AFTER)
      .value("ENDS_AT", Solver::ENDS_AT)
      .value("ENDS_BEFORE", Solver::ENDS_BEFORE)
      .value("STARTS_AFTER", Solver::STARTS_AFTER)
      .value("STARTS_AT", Solver::STARTS_AT)
      .value("STARTS_BEFORE", Solver::STARTS_BEFORE)
      .value("CROSS_DATE", Solver::CROSS_DATE)
      .value("AVOID_DATE", Solver::AVOID_DATE)
      .export_values();

  pybind11::enum_<Solver::BinaryIntervalRelation>(m, "BinaryIntervalRelation")
      .value("ENDS_AFTER_END", Solver::ENDS_AFTER_END)
      .value("ENDS_AFTER_START", Solver::ENDS_AFTER_START)
      .value("ENDS_AT_END", Solver::ENDS_AT_END)
      .value("ENDS_AT_START", Solver::ENDS_AT_START)
      .value("STARTS_AFTER_END", Solver::STARTS_AFTER_END)
      .value("STARTS_AFTER_START", Solver::STARTS_AFTER_START)
      .value("STARTS_AT_END", Solver::STARTS_AT_END)
      .value("STARTS_AT_START", Solver::STARTS_AT_START)
      .value("STAYS_IN_SYNC", Solver::STAYS_IN_SYNC)
      .export_values();

  py::class_<IntVarIterator>(m, "IntVarIterator")
      .def("init", &IntVarIterator::Init)
      .def("ok", &IntVarIterator::Ok)
      .def("value", &IntVarIterator::Value)
      .def("next", &IntVarIterator::Next)
      .def("__iter__",
           [](IntVarIterator* it) {
             it->Init();
             return it;
           })
      .def("__next__", [](IntVarIterator* it) {
        if (it->Ok()) {
          int64_t v = it->Value();
          it->Next();
          return v;
        }
        throw py::stop_iteration();
      });

  py::class_<BaseObject>(m, "BaseObject", DOC(operations_research, BaseObject))
      .def("__str__", &BaseObjectPythonHelper::DebugString);

  py::class_<SearchMonitor, BaseObject, PySearchMonitor>(
      m, "SearchMonitor", DOC(operations_research, SearchMonitor))
      .def(py::init<Solver*>(), py::arg("solver"))
      .def("enter_search", &SearchMonitor::EnterSearch,
           DOC(operations_research, SearchMonitor, EnterSearch))
      .def("restart_search", &SearchMonitor::RestartSearch,
           DOC(operations_research, SearchMonitor, RestartSearch))
      .def("exit_search", &SearchMonitor::ExitSearch,
           DOC(operations_research, SearchMonitor, ExitSearch))
      .def("begin_next_decision", &SearchMonitor::BeginNextDecision,
           py::arg("b"),
           DOC(operations_research, SearchMonitor, BeginNextDecision))
      .def("end_next_decision", &SearchMonitor::EndNextDecision, py::arg("b"),
           py::arg("d"),
           DOC(operations_research, SearchMonitor, EndNextDecision))
      .def("apply_decision", &SearchMonitor::ApplyDecision, py::arg("d"),
           DOC(operations_research, SearchMonitor, ApplyDecision))
      .def("refute_decision", &SearchMonitor::RefuteDecision, py::arg("d"),
           DOC(operations_research, SearchMonitor, RefuteDecision))
      .def("after_decision", &SearchMonitor::AfterDecision, py::arg("d"),
           py::arg("apply"),
           DOC(operations_research, SearchMonitor, AfterDecision))
      .def("begin_fail", &SearchMonitor::BeginFail,
           DOC(operations_research, SearchMonitor, BeginFail))
      .def("end_fail", &SearchMonitor::EndFail,
           DOC(operations_research, SearchMonitor, EndFail))
      .def("begin_initial_propagation", &SearchMonitor::BeginInitialPropagation,
           DOC(operations_research, SearchMonitor, BeginInitialPropagation))
      .def("end_initial_propagation", &SearchMonitor::EndInitialPropagation,
           DOC(operations_research, SearchMonitor, EndInitialPropagation))
      .def("accept_solution", &SearchMonitor::AcceptSolution,
           DOC(operations_research, SearchMonitor, AcceptSolution))
      .def("at_solution", &SearchMonitor::AtSolution,
           DOC(operations_research, SearchMonitor, AtSolution))
      .def("no_more_solutions", &SearchMonitor::NoMoreSolutions,
           DOC(operations_research, SearchMonitor, NoMoreSolutions))
      .def("at_local_optimum", &SearchMonitor::AtLocalOptimum,
           DOC(operations_research, SearchMonitor, AtLocalOptimum))
      .def("accept_delta", &SearchMonitor::AcceptDelta, py::arg("delta"),
           py::arg("deltadelta"),
           DOC(operations_research, SearchMonitor, AcceptDelta))
      .def("accept_neighbor", &SearchMonitor::AcceptNeighbor,
           DOC(operations_research, SearchMonitor, AcceptNeighbor))
      .def("accept_unchecked_neighbor", &SearchMonitor::AcceptUncheckedNeighbor,
           DOC(operations_research, SearchMonitor, AcceptUncheckedNeighbor))
      .def("is_unchecked_solution_limit_reached",
           &SearchMonitor::IsUncheckedSolutionLimitReached,
           DOC(operations_research, SearchMonitor,
               IsUncheckedSolutionLimitReached))
      .def("periodic_check", &SearchMonitor::PeriodicCheck,
           DOC(operations_research, SearchMonitor, PeriodicCheck))
      .def("progress_percent", &SearchMonitor::ProgressPercent,
           DOC(operations_research, SearchMonitor, ProgressPercent))
      .def("accept", &SearchMonitor::Accept, py::arg("visitor"),
           DOC(operations_research, SearchMonitor, Accept))
      .def("install", &SearchMonitor::Install,
           DOC(operations_research, SearchMonitor, Install))
      .def("__str__", &SearchMonitor::DebugString);

  py::class_<SolutionCollector, SearchMonitor>(m, "SolutionCollector")
      .def(py::init<Solver*, const Assignment*>(), py::arg("solver"),
           py::arg("assignment"))
      .def(py::init<Solver*>(), py::arg("solver"))
      .def("add", py::overload_cast<IntVar*>(&SolutionCollector::Add),
           py::arg("var"))
      .def("add",
           py::overload_cast<const std::vector<IntVar*>&>(
               &SolutionCollector::Add),
           py::arg("vars"))
      .def("add", py::overload_cast<IntervalVar*>(&SolutionCollector::Add),
           py::arg("var"))
      .def("add",
           py::overload_cast<const std::vector<IntervalVar*>&>(
               &SolutionCollector::Add),
           py::arg("vars"))
      .def("add", py::overload_cast<SequenceVar*>(&SolutionCollector::Add),
           py::arg("var"))
      .def("add",
           py::overload_cast<const std::vector<SequenceVar*>&>(
               &SolutionCollector::Add),
           py::arg("vars"))
      .def("add_objective", &SolutionCollector::AddObjective,
           py::arg("objective"))
      .def("add_objectives", &SolutionCollector::AddObjectives,
           py::arg("objectives"))
      .def_property_readonly("solution_count",
                             &SolutionCollector::solution_count)
      .def("has_solution", &SolutionCollector::has_solution)
      .def("solution", &SolutionCollector::solution, py::arg("n"),
           py::return_value_policy::reference_internal)
      .def("last_solution_or_null", &SolutionCollector::last_solution_or_null,
           py::return_value_policy::reference_internal)
      .def("wall_time_ms", &SolutionCollector::wall_time, py::arg("n"))
      .def("branches", &SolutionCollector::branches, py::arg("n"))
      .def("failures", &SolutionCollector::failures, py::arg("n"))
      .def("objective_value", &SolutionCollector::objective_value, py::arg("n"))
      .def("objective_value_from_index",
           &SolutionCollector::ObjectiveValueFromIndex, py::arg("n"),
           py::arg("index"))
      .def("value", &SolutionCollector::Value, py::arg("n"), py::arg("var"))
      .def("start_value", &SolutionCollector::StartValue, py::arg("n"),
           py::arg("var"))
      .def("end_value", &SolutionCollector::EndValue, py::arg("n"),
           py::arg("var"))
      .def("duration_value", &SolutionCollector::DurationValue, py::arg("n"),
           py::arg("var"))
      .def("performed_value", &SolutionCollector::PerformedValue, py::arg("n"),
           py::arg("var"))
      .def("forward_sequence", &SolutionCollector::ForwardSequence,
           py::arg("n"), py::arg("var"))
      .def("backward_sequence", &SolutionCollector::BackwardSequence,
           py::arg("n"), py::arg("var"))
      .def("unperformed", &SolutionCollector::Unperformed, py::arg("n"),
           py::arg("var"));

  py::class_<OptimizeVar, SearchMonitor>(m, "OptimizeVar")
      .def("best", &OptimizeVar::best)
      .def("var", &OptimizeVar::var,
           py::return_value_policy::reference_internal)
      .def("apply_bound", &OptimizeVar::ApplyBound)
      .def("__str__", &OptimizeVar::DebugString);

  py::class_<SearchLimit, SearchMonitor>(m, "SearchLimit")
      .def("crossed", &SearchLimit::crossed)
      .def("check", &SearchLimit::Check)
      .def("init", &SearchLimit::Init)
      .def("__str__", &SearchLimit::DebugString);

  py::class_<RegularLimit, SearchLimit>(m, "RegularLimit")
      .def_property_readonly("wall_time_ms", &RegularLimit::wall_time)
      .def_property_readonly("branches", &RegularLimit::branches)
      .def_property_readonly("failures", &RegularLimit::failures)
      .def_property_readonly("solutions", &RegularLimit::solutions);

  py::class_<AssignmentElement>(m, "AssignmentElement")
      .def(py::init<>())
      .def("activate", &AssignmentElement::Activate)
      .def("deactivate", &AssignmentElement::Deactivate)
      .def("activated", &AssignmentElement::Activated);

  py::class_<IntVarElement, AssignmentElement>(m, "IntVarElement")
      .def(py::init<>())
      .def(py::init<IntVar*>())
      .def("var", &IntVarElement::Var,
           py::return_value_policy::reference_internal)
      .def("min", &IntVarElement::Min)
      .def("set_min", &IntVarElement::SetMin, py::arg("m"))
      .def("max", &IntVarElement::Max)
      .def("set_max", &IntVarElement::SetMax, py::arg("m"))
      .def("value", &IntVarElement::Value)
      .def("bound", &IntVarElement::Bound)
      .def("set_range", &IntVarElement::SetRange, py::arg("l"), py::arg("u"))
      .def("set_value", &IntVarElement::SetValue, py::arg("v"))
      .def("__str__", &IntVarElement::DebugString)
      .def("__eq__", &IntVarElement::operator==)
      .def("__ne__", &IntVarElement::operator!=);

  py::class_<IntervalVarElement, AssignmentElement>(m, "IntervalVarElement")
      .def(py::init<>())
      .def(py::init<IntervalVar*>())
      .def("var", &IntervalVarElement::Var,
           py::return_value_policy::reference_internal)
      .def("start_min", &IntervalVarElement::StartMin)
      .def("start_max", &IntervalVarElement::StartMax)
      .def("start_value", &IntervalVarElement::StartValue)
      .def("duration_min", &IntervalVarElement::DurationMin)
      .def("duration_max", &IntervalVarElement::DurationMax)
      .def("duration_value", &IntervalVarElement::DurationValue)
      .def("end_min", &IntervalVarElement::EndMin)
      .def("end_max", &IntervalVarElement::EndMax)
      .def("end_value", &IntervalVarElement::EndValue)
      .def("performed_min", &IntervalVarElement::PerformedMin)
      .def("performed_max", &IntervalVarElement::PerformedMax)
      .def("performed_value", &IntervalVarElement::PerformedValue)
      .def("set_start_min", &IntervalVarElement::SetStartMin, py::arg("m"))
      .def("set_start_max", &IntervalVarElement::SetStartMax, py::arg("m"))
      .def("set_start_range", &IntervalVarElement::SetStartRange, py::arg("mi"),
           py::arg("ma"))
      .def("set_start_value", &IntervalVarElement::SetStartValue, py::arg("v"))
      .def("set_duration_min", &IntervalVarElement::SetDurationMin,
           py::arg("m"))
      .def("set_duration_max", &IntervalVarElement::SetDurationMax,
           py::arg("m"))
      .def("set_duration_range", &IntervalVarElement::SetDurationRange,
           py::arg("mi"), py::arg("ma"))
      .def("set_duration_value", &IntervalVarElement::SetDurationValue,
           py::arg("v"))
      .def("set_end_min", &IntervalVarElement::SetEndMin, py::arg("m"))
      .def("set_end_max", &IntervalVarElement::SetEndMax, py::arg("m"))
      .def("set_end_range", &IntervalVarElement::SetEndRange, py::arg("mi"),
           py::arg("ma"))
      .def("set_end_value", &IntervalVarElement::SetEndValue, py::arg("v"))
      .def("set_performed_min", &IntervalVarElement::SetPerformedMin,
           py::arg("m"))
      .def("set_performed_max", &IntervalVarElement::SetPerformedMax,
           py::arg("m"))
      .def("set_performed_range", &IntervalVarElement::SetPerformedRange,
           py::arg("mi"), py::arg("ma"))
      .def("set_performed_value", &IntervalVarElement::SetPerformedValue,
           py::arg("v"))
      .def("bound", &IntervalVarElement::Bound)
      .def("__str__", &IntervalVarElement::DebugString)
      .def("__eq__", &IntervalVarElement::operator==)
      .def("__ne__", &IntervalVarElement::operator!=);

  py::class_<SequenceVarElement, AssignmentElement>(m, "SequenceVarElement")
      .def(py::init<>())
      .def(py::init<SequenceVar*>())
      .def("var", &SequenceVarElement::Var,
           py::return_value_policy::reference_internal)
      .def("forward_sequence", &SequenceVarElement::ForwardSequence)
      .def("backward_sequence", &SequenceVarElement::BackwardSequence)
      .def("unperformed", &SequenceVarElement::Unperformed)
      .def("set_sequence", &SequenceVarElement::SetSequence,
           py::arg("forward_sequence"), py::arg("backward_sequence"),
           py::arg("unperformed"))
      .def("set_forward_sequence", &SequenceVarElement::SetForwardSequence,
           py::arg("forward_sequence"))
      .def("set_backward_sequence", &SequenceVarElement::SetBackwardSequence,
           py::arg("backward_sequence"))
      .def("set_unperformed", &SequenceVarElement::SetUnperformed,
           py::arg("unperformed"))
      .def("bound", &SequenceVarElement::Bound)
      .def("__str__", &SequenceVarElement::DebugString)
      .def("__eq__", &SequenceVarElement::operator==)
      .def("__ne__", &SequenceVarElement::operator!=);

  py::class_<Assignment::IntContainer>(m, "AssignmentIntContainer")
      .def("add", py::overload_cast<IntVar*>(&Assignment::IntContainer::Add),
           py::arg("var"), py::return_value_policy::reference_internal)
      .def("fast_add", &Assignment::IntContainer::FastAdd, py::arg("var"),
           py::return_value_policy::reference_internal)
      .def("add_at_position", &Assignment::IntContainer::AddAtPosition,
           py::arg("var"), py::arg("position"),
           py::return_value_policy::reference_internal)
      .def("clear", &Assignment::IntContainer::Clear)
      .def("resize", &Assignment::IntContainer::Resize, py::arg("size"))
      .def("empty", &Assignment::IntContainer::Empty)
      .def("copy_intersection", &Assignment::IntContainer::CopyIntersection,
           py::arg("container"))
      .def("copy", &Assignment::IntContainer::Copy, py::arg("container"))
      .def("contains", &Assignment::IntContainer::Contains, py::arg("var"))
      .def("mutable_element",
           py::overload_cast<const IntVar*>(
               &Assignment::IntContainer::MutableElementOrNull),
           py::arg("var"), py::return_value_policy::reference_internal)
      .def("element",
           py::overload_cast<const IntVar*>(
               &Assignment::IntContainer::ElementPtrOrNull, py::const_),
           py::arg("var"), py::return_value_policy::reference_internal)
      .def("mutable_element",
           py::overload_cast<int>(&Assignment::IntContainer::MutableElement),
           py::arg("index"), py::return_value_policy::reference_internal)
      .def("element",
           py::overload_cast<int>(&Assignment::IntContainer::Element,
                                  py::const_),
           py::arg("index"), py::return_value_policy::reference_internal)
      .def("size", &Assignment::IntContainer::Size)
      .def("store", &Assignment::IntContainer::Store)
      .def("restore", &Assignment::IntContainer::Restore)
      .def("are_all_elements_bound",
           &Assignment::IntContainer::AreAllElementsBound)
      .def("__eq__", &Assignment::IntContainer::operator==)
      .def("__ne__", &Assignment::IntContainer::operator!=);

  py::class_<Assignment::IntervalContainer>(m, "AssignmentIntervalContainer")
      .def("add",
           py::overload_cast<IntervalVar*>(&Assignment::IntervalContainer::Add),
           py::arg("var"), py::return_value_policy::reference_internal)
      .def("fast_add", &Assignment::IntervalContainer::FastAdd, py::arg("var"),
           py::return_value_policy::reference_internal)
      .def("add_at_position", &Assignment::IntervalContainer::AddAtPosition,
           py::arg("var"), py::arg("position"),
           py::return_value_policy::reference_internal)
      .def("clear", &Assignment::IntervalContainer::Clear)
      .def("resize", &Assignment::IntervalContainer::Resize, py::arg("size"))
      .def("empty", &Assignment::IntervalContainer::Empty)
      .def("copy_intersection",
           &Assignment::IntervalContainer::CopyIntersection,
           py::arg("container"))
      .def("copy", &Assignment::IntervalContainer::Copy, py::arg("container"))
      .def("contains", &Assignment::IntervalContainer::Contains, py::arg("var"))
      .def("mutable_element",
           py::overload_cast<const IntervalVar*>(
               &Assignment::IntervalContainer::MutableElementOrNull),
           py::arg("var"), py::return_value_policy::reference_internal)
      .def("element",
           py::overload_cast<const IntervalVar*>(
               &Assignment::IntervalContainer::ElementPtrOrNull, py::const_),
           py::arg("var"), py::return_value_policy::reference_internal)
      .def("mutable_element",
           py::overload_cast<int>(
               &Assignment::IntervalContainer::MutableElement),
           py::arg("index"), py::return_value_policy::reference_internal)
      .def("element",
           py::overload_cast<int>(&Assignment::IntervalContainer::Element,
                                  py::const_),
           py::arg("index"), py::return_value_policy::reference_internal)
      .def("size", &Assignment::IntervalContainer::Size)
      .def("store", &Assignment::IntervalContainer::Store)
      .def("restore", &Assignment::IntervalContainer::Restore)
      .def("are_all_elements_bound",
           &Assignment::IntervalContainer::AreAllElementsBound)
      .def("__eq__", &Assignment::IntervalContainer::operator==)
      .def("__ne__", &Assignment::IntervalContainer::operator!=);

  py::class_<Assignment::SequenceContainer>(m, "AssignmentSequenceContainer")
      .def("add",
           py::overload_cast<SequenceVar*>(&Assignment::SequenceContainer::Add),
           py::arg("var"), py::return_value_policy::reference_internal)
      .def("fast_add", &Assignment::SequenceContainer::FastAdd, py::arg("var"),
           py::return_value_policy::reference_internal)
      .def("add_at_position", &Assignment::SequenceContainer::AddAtPosition,
           py::arg("var"), py::arg("position"),
           py::return_value_policy::reference_internal)
      .def("clear", &Assignment::SequenceContainer::Clear)
      .def("resize", &Assignment::SequenceContainer::Resize, py::arg("size"))
      .def("empty", &Assignment::SequenceContainer::Empty)
      .def("copy_intersection",
           &Assignment::SequenceContainer::CopyIntersection,
           py::arg("container"))
      .def("copy", &Assignment::SequenceContainer::Copy, py::arg("container"))
      .def("contains", &Assignment::SequenceContainer::Contains, py::arg("var"))
      .def("mutable_element",
           py::overload_cast<const SequenceVar*>(
               &Assignment::SequenceContainer::MutableElementOrNull),
           py::arg("var"), py::return_value_policy::reference_internal)
      .def("element",
           py::overload_cast<const SequenceVar*>(
               &Assignment::SequenceContainer::ElementPtrOrNull, py::const_),
           py::arg("var"), py::return_value_policy::reference_internal)
      .def("mutable_element",
           py::overload_cast<int>(
               &Assignment::SequenceContainer::MutableElement),
           py::arg("index"), py::return_value_policy::reference_internal)
      .def("element",
           py::overload_cast<int>(&Assignment::SequenceContainer::Element,
                                  py::const_),
           py::arg("index"), py::return_value_policy::reference_internal)
      .def("size", &Assignment::SequenceContainer::Size)
      .def("store", &Assignment::SequenceContainer::Store)
      .def("restore", &Assignment::SequenceContainer::Restore)
      .def("are_all_elements_bound",
           &Assignment::SequenceContainer::AreAllElementsBound)
      .def("__eq__", &Assignment::SequenceContainer::operator==)
      .def("__ne__", &Assignment::SequenceContainer::operator!=);

  py::class_<Solver>(m, "Solver", DOC(operations_research, Solver))
      .def(py::init<const std::string&>())
      .def(py::init<const std::string&,
                          const ConstraintSolverParameters&>())
      .def("new_int_var",
           py::overload_cast<int64_t, int64_t, const std::string&>(
               &Solver::MakeIntVar),
           DOC(operations_research, Solver, MakeIntVar),
           py::return_value_policy::reference_internal)
      .def("new_int_var",
           py::overload_cast<int64_t, int64_t>(&Solver::MakeIntVar),
           DOC(operations_research, Solver, MakeIntVar),
           py::return_value_policy::reference_internal)
      .def("new_int_var",
           py::overload_cast<const std::vector<int64_t>&,
                                   const std::string&>(&Solver::MakeIntVar),
           DOC(operations_research, Solver, MakeIntVar_2),
           py::return_value_policy::reference_internal)
      .def("new_int_var",
           py::overload_cast<const std::vector<int64_t>&>(
               &Solver::MakeIntVar),
           DOC(operations_research, Solver, MakeIntVar_2),
           py::return_value_policy::reference_internal)
      .def("new_bool_var",
           [](Solver* s, const std::string& name) {
             return s->MakeBoolVar(name);
           },
           py::arg("name"), DOC(operations_research, Solver, MakeBoolVar),
           py::return_value_policy::reference_internal)
      .def("new_interval_var",
           py::overload_cast<int64_t, int64_t, int64_t, int64_t, int64_t,
                                   int64_t, bool, const std::string&>(
               &Solver::MakeIntervalVar),
           py::arg("start_min"),
           py::arg("start_max"),
           py::arg("duration_min"),
           py::arg("duration_max"),
           py::arg("end_min"),
           py::arg("end_max"),
           py::arg("optional"),
           py::arg("name"),
           DOC(operations_research, Solver, MakeIntervalVar),
           py::return_value_policy::reference_internal)
      .def("new_fixed_duration_interval_var",
           py::overload_cast<int64_t, int64_t, int64_t, bool,
                                   const std::string&>(
               &Solver::MakeFixedDurationIntervalVar),
           DOC(operations_research, Solver, MakeFixedDurationIntervalVar),
           py::return_value_policy::reference_internal)
      .def("new_fixed_duration_interval_var",
        [](Solver* s, IntExpr* start, int64_t duration,
               const std::string& name) {
             return s->MakeFixedDurationIntervalVar(
                 start->Var(), duration, name);
           },
           py::arg("start"), py::arg("duration"), py::arg("name"),
           DOC(operations_research, Solver, MakeFixedDurationIntervalVar_2),
           py::return_value_policy::reference_internal)
      .def("new_fixed_duration_interval_var",
        [](Solver* s, IntExpr* start, int64_t duration,
           IntExpr* performed_variable, const std::string& name) {
             return s->MakeFixedDurationIntervalVar(
                 start->Var(), duration, performed_variable->Var(), name);
           },
           py::arg("start"), py::arg("duration"), py::arg("performed_variable"),
           py::arg("name"),
           DOC(operations_research, Solver, MakeFixedDurationIntervalVar_3),
           py::return_value_policy::reference_internal)
      .def("new_fixed_interval",
           py::overload_cast<int64_t, int64_t, const std::string&>(
               &Solver::MakeFixedInterval),
           py::arg("start"), py::arg("duration"), py::arg("name"),
           DOC(operations_research, Solver, MakeFixedInterval),
           py::return_value_policy::reference_internal)
      .def("new_mirror_interval",
           py::overload_cast<IntervalVar*>(&Solver::MakeMirrorInterval),
           py::arg("interval_var"),
           DOC(operations_research, Solver, MakeMirrorInterval),
           py::return_value_policy::reference_internal)
      .def("new_interval_relaxed_min",
           py::overload_cast<IntervalVar*>(&Solver::MakeIntervalRelaxedMin),
           py::arg("interval_var"),
           DOC(operations_research, Solver, MakeIntervalRelaxedMin),
           py::return_value_policy::reference_internal)
      .def("new_interval_relaxed_max",
           py::overload_cast<IntervalVar*>(&Solver::MakeIntervalRelaxedMax),
           py::arg("interval_var"),
           DOC(operations_research, Solver, MakeIntervalRelaxedMax),
           py::return_value_policy::reference_internal)
      .def("add_abs_equality",
           [](Solver* s, IntVar* var, IntVar* abs_var) {
             s->AddConstraint(s->MakeAbsEquality(var, abs_var));
           })
      .def("add_all_different",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs) {
             s->AddConstraint(s->MakeAllDifferent(ToIntVarArray(exprs)));
           })
      .def("add_all_different",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              bool stronger) {
             s->AddConstraint(
                 s->MakeAllDifferent(ToIntVarArray(exprs), stronger));
           })
      .def("add_all_different_except",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              int64_t v) {
             s->AddConstraint(
                 s->MakeAllDifferentExcept(ToIntVarArray(exprs), v));
           })
      .def("add_between_ct",
           [](Solver* s, IntExpr* expr, int64_t l, int64_t u) {
             s->AddConstraint(s->MakeBetweenCt(expr, l, u));
           })
      .def("add_circuit",
           [](Solver* s, const std::vector<PropagationBaseObject*>& nexts) {
             s->AddConstraint(s->MakeCircuit(ToIntVarArray(nexts)));
           })
      .def("add_count",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              int64_t value, int64_t max_count) {
             s->AddConstraint(
                 s->MakeCount(ToIntVarArray(exprs), value, max_count));
           })
      .def("add_count",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              int64_t value, IntVar* max_count) {
             s->AddConstraint(
                 s->MakeCount(ToIntVarArray(exprs), value, max_count));
           })
      .def("add_cover",
           [](Solver* s, const std::vector<IntervalVar*>& vars,
              IntervalVar* target_var) {
             s->AddConstraint(s->MakeCover(vars, target_var));
           })
      .def("add_cumulative",
           [](Solver* s, const std::vector<IntervalVar*>& intervals,
              const std::vector<int64_t>& demands, int64_t capacity,
              const std::string& name) {
             s->AddConstraint(
                  s->MakeCumulative(intervals, demands, capacity, name));
           })
      .def("add_cumulative",
           [](Solver* s, const std::vector<IntervalVar*>& intervals,
              const std::vector<int64_t>& demands, IntVar* capacity,
              absl::string_view name) {
             s->AddConstraint(
                 s->MakeCumulative(intervals, demands, capacity, name));
           })
      .def("add_cumulative",
           [](Solver* s, const std::vector<IntervalVar*>& intervals,
              const std::vector<PropagationBaseObject*>& demands,
              int64_t capacity, const std::string& name) {
             s->AddConstraint(s->MakeCumulative(
                 intervals, ToIntVarArray(demands), capacity, name));
           })
      .def("add_cumulative",
           [](Solver* s, const std::vector<IntervalVar*>& intervals,
              const std::vector<PropagationBaseObject*>& demands,
              IntVar* capacity, const std::string& name) {
             s->AddConstraint(s->MakeCumulative(
                 intervals, ToIntVarArray(demands), capacity, name));
           })
      .def("add_delayed_path_cumul",
           [](Solver* s, const std::vector<PropagationBaseObject*>& nexts,
              const std::vector<PropagationBaseObject*>& active,
              const std::vector<PropagationBaseObject*>& cumuls,
              const std::vector<PropagationBaseObject*>& transits) {
             s->AddConstraint(s->MakeDelayedPathCumul(
                 ToIntVarArray(nexts), ToIntVarArray(active),
                 ToIntVarArray(cumuls), ToIntVarArray(transits)));
           })
      .def("add_deviation",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              IntVar* deviation_var, int64_t total_sum) {
             s->AddConstraint(s->MakeDeviation(
                 ToIntVarArray(exprs), deviation_var, total_sum));
           })
      .def("add_distribute",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              const std::vector<int64_t>& values,
              const std::vector<PropagationBaseObject*>& cards) {
             s->AddConstraint(s->MakeDistribute(ToIntVarArray(exprs), values,
                                                ToIntVarArray(cards)));
           })
      .def("add_distribute",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              const std::vector<PropagationBaseObject*>& cards) {
             s->AddConstraint(
                 s->MakeDistribute(ToIntVarArray(exprs), ToIntVarArray(cards)));
           })
      .def("add_distribute",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              int64_t card_min, int64_t card_max, int64_t card_size) {
             s->AddConstraint(s->MakeDistribute(
                 ToIntVarArray(exprs), card_min, card_max, card_size));
           })
      .def("add_distribute",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              const std::vector<int64_t>& values,
              const std::vector<int64_t>& cards) {
             s->AddConstraint(s->MakeDistribute(
                 ToIntVarArray(exprs), values, cards));
           })
      .def("add_element_equality",
           [](Solver* s, const std::vector<int64_t>& vals, IntVar* index,
              IntVar* target) {
             s->AddConstraint(s->MakeElementEquality(vals, index, target));
           })
      .def("add_element_equality",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              IntVar* index, IntVar* target) {
             s->AddConstraint(s->MakeElementEquality(
                 ToIntVarArray(exprs), index, target));
           })
      .def("add_element_equality",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              IntVar* index, int64_t target) {
             s->AddConstraint(s->MakeElementEquality(
                 ToIntVarArray(exprs), index, target));
           })
      .def("add_false_constraint",
           [](Solver* s) { s->AddConstraint(s->MakeFalseConstraint()); })
      .def("add_index_of_constraint",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              IntVar* index, int64_t target) {
             s->AddConstraint(
                 s->MakeIndexOfConstraint(ToIntVarArray(exprs), index, target));
           })
      .def("add_interval_var_relation",
           [](Solver* s, IntervalVar* t, Solver::UnaryIntervalRelation r,
              int64_t d) {
             s->AddConstraint(s->MakeIntervalVarRelation(t, r, d));
           })
      .def("add_interval_var_relation",
           [](Solver* s, IntervalVar* t1, Solver::BinaryIntervalRelation r,
              IntervalVar* t2) {
             s->AddConstraint(s->MakeIntervalVarRelation(t1, r, t2));
           })
      .def("add_interval_var_relation",
           [](Solver* s, IntervalVar* t1, Solver::BinaryIntervalRelation r,
              IntervalVar* t2, int64_t delay) {
             s->AddConstraint(s->MakeIntervalVarRelationWithDelay(t1, r, t2,
                                                                  delay));
           })
      .def("add_inverse_permutation_constraint",
           [](Solver* s, const std::vector<PropagationBaseObject*>& left,
              const std::vector<PropagationBaseObject*>& right) {
             s->AddConstraint(s->MakeInversePermutationConstraint(
                 ToIntVarArray(left), ToIntVarArray(right)));
           })
      .def("add_is_between_ct",
           [](Solver* s, IntExpr* var, int64_t l, int64_t u, IntVar* boolvar) {
             s->AddConstraint(s->MakeIsBetweenCt(var, l, u, boolvar));
           })
      .def("add_is_different_cst_ct",
           [](Solver* s, IntExpr* var, int64_t value, IntVar* boolvar) {
             s->AddConstraint(s->MakeIsDifferentCstCt(var, value, boolvar));
           })
      .def("add_is_different_ct",
           [](Solver* s, IntExpr* var, IntExpr* other, IntVar* boolvar) {
             s->AddConstraint(s->MakeIsDifferentCt(var, other, boolvar));
           })
      .def("add_is_equal_cst_ct",
           [](Solver* s, IntExpr* var, int64_t value, IntVar* boolvar) {
             s->AddConstraint(s->MakeIsEqualCstCt(var, value, boolvar));
           })
      .def("add_is_equal_ct",
           [](Solver* s, IntExpr* var, IntExpr* other, IntVar* boolvar) {
             s->AddConstraint(s->MakeIsEqualCt(var, other, boolvar));
           })
      .def("add_is_greater_cst_ct",
           [](Solver* s, IntExpr* var, int64_t value, IntVar* boolvar) {
             s->AddConstraint(s->MakeIsGreaterCstCt(var, value, boolvar));
           })
      .def("add_is_greater_ct",
           [](Solver* s, IntExpr* var, IntExpr* other, IntVar* boolvar) {
             s->AddConstraint(s->MakeIsGreaterCt(var, other, boolvar));
           })
      .def("add_is_greater_or_equal_cst_ct",
           [](Solver* s, IntExpr* var, int64_t value, IntVar* boolvar) {
             s->AddConstraint(
                 s->MakeIsGreaterOrEqualCstCt(var, value, boolvar));
           })
      .def("add_is_greater_or_equal_ct",
           [](Solver* s, IntExpr* var, IntExpr* other, IntVar* boolvar) {
             s->AddConstraint(
                 s->MakeIsGreaterOrEqualCt(var, other, boolvar));
           })
      .def("add_is_less_cst_ct",
           [](Solver* s, IntExpr* var, int64_t value, IntVar* boolvar) {
             s->AddConstraint(s->MakeIsLessCstCt(var, value, boolvar));
           })
      .def("add_is_less_ct",
           [](Solver* s, IntExpr* var, IntExpr* other, IntVar* boolvar) {
             s->AddConstraint(s->MakeIsLessCt(var, other, boolvar));
           })
      .def("add_is_less_or_equal_cst_ct",
           [](Solver* s, IntExpr* var, int64_t value, IntVar* boolvar) {
             s->AddConstraint(
                 s->MakeIsLessOrEqualCstCt(var, value, boolvar));
           })
      .def("add_is_less_or_equal_ct",
           [](Solver* s, IntExpr* var, IntExpr* other, IntVar* boolvar) {
             s->AddConstraint(s->MakeIsLessOrEqualCt(var, other, boolvar));
           })
      .def("add_is_member_ct",
           [](Solver* s, IntExpr* var, const std::vector<int64_t>& values,
              IntVar* boolvar) {
             s->AddConstraint(s->MakeIsMemberCt(var, values, boolvar));
           })
      .def("add_lexical_less",
           [](Solver* s, const std::vector<PropagationBaseObject*>& left,
              const std::vector<PropagationBaseObject*>& right) {
             s->AddConstraint(s->MakeLexicalLess(
                 ToIntVarArray(left), ToIntVarArray(right)));
           })
      .def("add_lexical_less_or_equal",
           [](Solver* s, const std::vector<PropagationBaseObject*>& left,
              const std::vector<PropagationBaseObject*>& right) {
             s->AddConstraint(s->MakeLexicalLessOrEqual(
                 ToIntVarArray(left), ToIntVarArray(right)));
           })
      .def("add_max_equality",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              IntVar* var) {
             s->AddConstraint(s->MakeMaxEquality(ToIntVarArray(exprs), var));
           })
      .def("add_member_ct",
           [](Solver* s, IntExpr* expr, const std::vector<int64_t>& values) {
             s->AddConstraint(s->MakeMemberCt(expr, values));
           })
      .def("add_min_equality",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              IntVar* var) {
             s->AddConstraint(s->MakeMinEquality(ToIntVarArray(exprs), var));
           })
      .def("add_non_overlapping_boxes_constraint",
           [](Solver* s, const std::vector<PropagationBaseObject*>& x_vars,
              const std::vector<PropagationBaseObject*>& y_vars,
              const std::vector<PropagationBaseObject*>& x_size,
              const std::vector<PropagationBaseObject*>& y_size) {
             s->AddConstraint(s->MakeNonOverlappingBoxesConstraint(
                 ToIntVarArray(x_vars),
                 ToIntVarArray(y_vars),
                 ToIntVarArray(x_size),
                 ToIntVarArray(y_size)));
           })
      .def("add_non_overlapping_boxes_constraint",
           [](Solver* s, const std::vector<PropagationBaseObject*>& x_vars,
              const std::vector<PropagationBaseObject*>& y_vars,
              const std::vector<int64_t>& x_size,
              const std::vector<int64_t>& y_size) {
             s->AddConstraint(s->MakeNonOverlappingBoxesConstraint(
                 ToIntVarArray(x_vars), ToIntVarArray(y_vars), x_size, y_size));
           })
      .def("add_not_member_ct",
           [](Solver* s, IntExpr* expr, const std::vector<int64_t>& values) {
             s->AddConstraint(s->MakeNotMemberCt(expr, values));
           })
      .def("add_null_intersect",
           [](Solver* s, const std::vector<PropagationBaseObject*>& first_exprs,
              const std::vector<PropagationBaseObject*>& second_exprs) {
             s->AddConstraint(
                 s->MakeNullIntersect(
                    ToIntVarArray(first_exprs), ToIntVarArray(second_exprs)));
           })
      .def("add_null_intersect_except",
           [](Solver* s, const std::vector<PropagationBaseObject*>& first_exprs,
              const std::vector<PropagationBaseObject*>& second_exprs,
              int64_t escape_value) {
             s->AddConstraint(s->MakeNullIntersectExcept(
                 ToIntVarArray(first_exprs),
                 ToIntVarArray(second_exprs),
                 escape_value));
           })
      .def("add_pack",
           [](Solver* s,
              const std::vector<PropagationBaseObject*>& exprs,
              int64_t number_of_bins) {
             s->AddConstraint(
                 s->MakePack(ToIntVarArray(exprs), number_of_bins));
           })
      .def("add_path_cumul",
           [](Solver* s, const std::vector<PropagationBaseObject*>& nexts,
              const std::vector<PropagationBaseObject*>& active,
              const std::vector<PropagationBaseObject*>& cumuls,
              const std::vector<PropagationBaseObject*>& transits) {
             s->AddConstraint(
                 s->MakePathCumul(ToIntVarArray(nexts),
                  ToIntVarArray(active),
                   ToIntVarArray(cumuls),
                    ToIntVarArray(transits)));
           })
      .def("add_path_cumul",
           [](Solver* s, const std::vector<PropagationBaseObject*>& nexts,
              const std::vector<PropagationBaseObject*>& active,
              const std::vector<PropagationBaseObject*>& cumuls,
              Solver::IndexEvaluator2 transit_evaluator) {
             s->AddConstraint(
                 s->MakePathCumul(ToIntVarArray(nexts),
                   ToIntVarArray(active),
                    ToIntVarArray(cumuls),
                       transit_evaluator));
           })
      .def("add_weighted_sum_equality",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              const std::vector<int64_t>& coeffs, int64_t cst) {
             s->AddConstraint(
                s->MakeScalProdEquality(ToIntVarArray(exprs), coeffs, cst));
           })
      .def("add_weighted_sum_equality",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              const std::vector<int64_t>& coeffs, IntVar* target) {
             s->AddConstraint(
                s->MakeScalProdEquality(ToIntVarArray(exprs), coeffs, target));
           })
      .def("add_weighted_sum_greater_or_equal",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              const std::vector<int64_t>& coeffs, int64_t cst) {
             s->AddConstraint(s->MakeScalProdGreaterOrEqual(
              ToIntVarArray(exprs), coeffs, cst));
           })
      .def("add_weighted_sum_less_or_equal",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              const std::vector<int64_t>& coeffs, int64_t cst) {
             s->AddConstraint(
                 s->MakeScalProdLessOrEqual(ToIntVarArray(exprs), coeffs, cst));
           })
      .def("add_sorting_constraint",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              const std::vector<PropagationBaseObject*>& sorted) {
             s->AddConstraint(s->MakeSortingConstraint(
                 ToIntVarArray(exprs), ToIntVarArray(sorted)));
           })
      .def("add_sub_circuit",
           [](Solver* s, const std::vector<PropagationBaseObject*>& nexts) {
             s->AddConstraint(s->MakeSubCircuit(ToIntVarArray(nexts)));
           })
      .def("add_sum_equality",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              int64_t cst) {
             s->AddConstraint(s->MakeSumEquality(ToIntVarArray(exprs), cst));
           })
      .def("add_sum_equality",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              IntVar* var) {
             s->AddConstraint(s->MakeSumEquality(ToIntVarArray(exprs), var));
           })
      .def("add_sum_greater_or_equal",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              int64_t cst) {
             s->AddConstraint(
                 s->MakeSumGreaterOrEqual(ToIntVarArray(exprs), cst));
           })
      .def("add_sum_less_or_equal",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              int64_t cst) {
             s->AddConstraint(s->MakeSumLessOrEqual(ToIntVarArray(exprs), cst));
           })
      .def("add_temporal_disjunction",
           [](Solver* s, IntervalVar* t1, IntervalVar* t2) {
             s->AddConstraint(s->MakeTemporalDisjunction(t1, t2));
           })
      .def("add_allowed_assignments",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              const std::vector<std::vector<int64_t>>& raw_tuples) {
             IntTupleSet tuples(exprs.size());
             tuples.InsertAll(raw_tuples);
             s->AddConstraint(
                 s->MakeAllowedAssignments(ToIntVarArray(exprs), tuples));
           },
           py::arg("vars"), py::arg("tuples"),
           DOC(operations_research, Solver, MakeAllowedAssignments))
      .def("add_true_constraint",
           [](Solver* s) { s->AddConstraint(s->MakeTrueConstraint()); })
      .def("add_transition_constraint",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              const std::vector<std::vector<int64_t>>& raw_transitions,
              int64_t initial_state, const std::vector<int64_t>& final_states) {
             IntTupleSet transitions(3);
             transitions.InsertAll(raw_transitions);
             s->AddConstraint(s->MakeTransitionConstraint(
                 ToIntVarArray(exprs), transitions, initial_state,
                 final_states));
           },
           py::arg("exprs"), py::arg("transitions"), py::arg("initial_state"),
           py::arg("final_states"),
           DOC(operations_research, Solver, MakeTransitionConstraint))
      .def("add_disjunctive_constraint",
           [](Solver* s, const std::vector<IntervalVar*>& intervals,
              const std::string& name) {
             auto* ct = s->MakeDisjunctiveConstraint(intervals, name);
             s->AddConstraint(ct);
             return ct;
           }, py::return_value_policy::reference_internal)
      .def("__str__", &Solver::DebugString)
      .def("local_search_profile", &Solver::LocalSearchProfile)
      .def("add", &Solver::AddConstraint,
           DOC(operations_research, Solver, AddConstraint), py::arg("c"),
           py::keep_alive<1, 2>())
      .def("fail",
           [](Solver* s) {
             jmp_buf buffer;
             s->set_fail_intercept([&buffer]() { longjmp(buffer, 1); });
             auto cleanup =
                 absl::MakeCleanup([s]() { s->clear_fail_intercept(); });
             if (setjmp(buffer) == 0) {
               s->Fail();
             } else {
               throw py::value_error("Solver fails outside of solve()");
             }
           })
      .def("accept", &Solver::Accept, DOC(operations_research, Solver, Accept),
           py::arg("visitor"))
      .def("solve",
           py::overload_cast<DecisionBuilder*,
                             const std::vector<SearchMonitor*>&>(
               &Solver::Solve),
           py::arg("db"), py::arg("monitors"),
           DOC(operations_research, Solver, Solve))
      .def("solve",
           py::overload_cast<DecisionBuilder*, SearchMonitor*>(
               &Solver::Solve),
           py::arg("db"), py::arg("m1"),
           DOC(operations_research, Solver, Solve))
      .def("solve",
           py::overload_cast<DecisionBuilder*, SearchMonitor*, SearchMonitor*>(
               &Solver::Solve),
           py::arg("db"), py::arg("m1"), py::arg("m2"),
           DOC(operations_research, Solver, Solve))
      .def("solve",
           py::overload_cast<DecisionBuilder*, SearchMonitor*, SearchMonitor*,
                             SearchMonitor*>(
               &Solver::Solve),
           py::arg("db"), py::arg("m1"), py::arg("m2"), py::arg("m3"),
           DOC(operations_research, Solver, Solve))
      .def("solve",
           py::overload_cast<DecisionBuilder*, SearchMonitor*, SearchMonitor*,
                             SearchMonitor*, SearchMonitor*>(
               &Solver::Solve),
           py::arg("db"), py::arg("m1"), py::arg("m2"), py::arg("m3"),
           py::arg("m4"), DOC(operations_research, Solver, Solve))
      .def("solve", py::overload_cast<DecisionBuilder*>(&Solver::Solve),
           py::arg("db"), DOC(operations_research, Solver, Solve_2))
      .def("solve_and_commit",
           py::overload_cast<DecisionBuilder*,
                             const std::vector<SearchMonitor*>&>(
               &Solver::SolveAndCommit),
           py::arg("db"), py::arg("monitors"),
           DOC(operations_research, Solver, SolveAndCommit))
      .def("solve_and_commit",
           py::overload_cast<DecisionBuilder*>(&Solver::SolveAndCommit),
           py::arg("db"), DOC(operations_research, Solver, SolveAndCommit_2))
      .def("new_search",
           py::overload_cast<DecisionBuilder*,
                                   const std::vector<SearchMonitor*>&>(  // NOLINT
               &Solver::NewSearch),
           DOC(operations_research, Solver, NewSearch),
           py::keep_alive<1, 3>())
      .def("new_search",
           [](Solver* s, DecisionBuilder* db) {
             std::vector<SearchMonitor*> monitors;
             s->NewSearch(db, monitors);
           },
           DOC(operations_research, Solver, NewSearch))
      .def("next_solution", &Solver::NextSolution,
           DOC(operations_research, Solver, NextSolution))
      .def("finish_current_search", &Solver::FinishCurrentSearch,
           DOC(operations_research, Solver, FinishCurrentSearch))
      .def("end_search", &Solver::EndSearch,
           DOC(operations_research, Solver, EndSearch))
      .def_property_readonly("fail_stamp", &Solver::fail_stamp)
      .def_property_readonly("num_accepted_neighbors",
                             &Solver::accepted_neighbors)
      .def_property_readonly("num_branches", &Solver::branches)
      .def_property_readonly("num_constraints", &Solver::constraints)
      .def_property_readonly("num_failures", &Solver::failures)
      .def_property_readonly("num_solutions", &Solver::solutions)
      .def_property_readonly("search_depth", &Solver::SearchDepth)
      .def_property_readonly("search_left_depth", &Solver::SearchLeftDepth)
      .def_property_readonly("solve_depth", &Solver::SolveDepth)
      .def_property_readonly("stamp", &Solver::stamp)
      .def_property_readonly("wall_time_ms", &Solver::wall_time)
      .def_static("memory_usage", &Solver::MemoryUsage)
      .def("local_search_phase_parameters",
           [](Solver* s, IntVar* objective, LocalSearchOperator* ls_operator,
              DecisionBuilder* sub_decision_builder,
              RegularLimit* limit,
              LocalSearchFilterManager* filter_manager) {
             return py::capsule(
                 s->MakeLocalSearchPhaseParameters(objective, ls_operator,
                                                   sub_decision_builder, limit,
                                                   filter_manager),
                 "LocalSearchPhaseParameters");
           },
           py::arg("objective"), py::arg("ls_operator"),
           py::arg("sub_decision_builder"), py::arg("limit") = nullptr,
           py::arg("filter_manager") = nullptr)
      .def("default_solver_parameters", &Solver::DefaultSolverParameters)
      .def_property_readonly("parameters", &Solver::parameters)
      .def("assignment", py::overload_cast<>(&Solver::MakeAssignment),
           DOC(operations_research, Solver, MakeAssignment_2),
           py::return_value_policy::reference_internal)
      .def("limit",
           py::overload_cast<const RegularLimitParameters&>(&Solver::MakeLimit),
           py::arg("proto"),
           py::return_value_policy::reference_internal)
      .def("limit",
           [](Solver* s, int64_t time, int64_t branches, int64_t failures,
              int64_t solutions, bool smart_time_check, bool cumulative) {
             return s->MakeLimit(time, branches, failures, solutions,
                                 smart_time_check, cumulative);
           },
           py::arg("time"), py::arg("branches"), py::arg("failures"),
           py::arg("solutions"), py::arg("smart_time_check") = false,
           py::arg("cumulative") = false,
           py::return_value_policy::reference_internal)
      .def("element_function",
           py::overload_cast<Solver::IndexEvaluator1, IntVar*>(
               &Solver::MakeElement),
           py::arg("values"), py::arg("index"),
           DOC(operations_research, Solver, MakeElement_3),
           py::return_value_policy::reference_internal)
      .def("first_solution_collector",
           py::overload_cast<const Assignment*>(
               &Solver::MakeFirstSolutionCollector),
           py::arg("assignment"),
           DOC(operations_research, Solver, MakeFirstSolutionCollector),
           py::return_value_policy::reference_internal)
      .def("first_solution_collector",
           py::overload_cast<>(&Solver::MakeFirstSolutionCollector),
           DOC(operations_research, Solver, MakeFirstSolutionCollector_2),
           py::return_value_policy::reference_internal)
      .def("last_solution_collector",
           py::overload_cast<const Assignment*>(
               &Solver::MakeLastSolutionCollector),
           py::arg("assignment"),
           DOC(operations_research, Solver, MakeLastSolutionCollector),
           py::return_value_policy::reference_internal)
      .def("last_solution_collector",
           py::overload_cast<>(&Solver::MakeLastSolutionCollector),
           DOC(operations_research, Solver, MakeLastSolutionCollector_2),
           py::return_value_policy::reference_internal)
      .def("best_value_solution_collector",
           py::overload_cast<const Assignment*, bool>(
               &Solver::MakeBestValueSolutionCollector),
           py::arg("assignment"), py::arg("maximize"),
           DOC(operations_research, Solver, MakeBestValueSolutionCollector),
           py::return_value_policy::reference_internal)
      .def("best_value_solution_collector",
           py::overload_cast<bool>(&Solver::MakeBestValueSolutionCollector),
           py::arg("maximize"),
           DOC(operations_research, Solver, MakeBestValueSolutionCollector_2),
           py::return_value_policy::reference_internal)
      .def("best_lexicographic_value_solution_collector",
           py::overload_cast<const Assignment*, std::vector<bool>>(
               &Solver::MakeBestLexicographicValueSolutionCollector),
           py::arg("assignment"), py::arg("maximize"),
           DOC(operations_research, Solver,
               MakeBestLexicographicValueSolutionCollector),
           py::return_value_policy::reference_internal)
      .def("best_lexicographic_value_solution_collector",
           py::overload_cast<std::vector<bool>>(
               &Solver::MakeBestLexicographicValueSolutionCollector),
           py::arg("maximize"),
           DOC(operations_research, Solver,
               MakeBestLexicographicValueSolutionCollector_2),
           py::return_value_policy::reference_internal)
      .def("n_best_value_solution_collector",
           py::overload_cast<const Assignment*, int, bool>(
               &Solver::MakeNBestValueSolutionCollector),
           py::arg("assignment"), py::arg("solution_count"),
           py::arg("maximize"),
           DOC(operations_research, Solver,
               MakeNBestValueSolutionCollector),
           py::return_value_policy::reference_internal)
      .def("n_best_value_solution_collector",
           py::overload_cast<int, bool>(
               &Solver::MakeNBestValueSolutionCollector),
           py::arg("solution_count"), py::arg("maximize"),
           DOC(operations_research, Solver, MakeNBestValueSolutionCollector_2),
           py::return_value_policy::reference_internal)
      .def("n_best_lexicographic_value_solution_collector",
           py::overload_cast<const Assignment*, int, std::vector<bool>>(
               &Solver::MakeNBestLexicographicValueSolutionCollector),
           py::arg("assignment"), py::arg("solution_count"),
           py::arg("maximize"),
           DOC(operations_research, Solver,
               MakeNBestLexicographicValueSolutionCollector),
           py::return_value_policy::reference_internal)
      .def("n_best_lexicographic_value_solution_collector",
           py::overload_cast<int, std::vector<bool>>(
               &Solver::MakeNBestLexicographicValueSolutionCollector),
           py::arg("solution_count"), py::arg("maximize"),
           DOC(operations_research, Solver,
               MakeNBestLexicographicValueSolutionCollector_2),
           py::return_value_policy::reference_internal)
      .def("all_solution_collector",
           py::overload_cast<const Assignment*>(
               &Solver::MakeAllSolutionCollector),
           py::arg("assignment"),
           DOC(operations_research, Solver, MakeAllSolutionCollector),
           py::return_value_policy::reference_internal)
      .def("all_solution_collector",
           py::overload_cast<>(&Solver::MakeAllSolutionCollector),
           DOC(operations_research, Solver, MakeAllSolutionCollector_2),
           py::return_value_policy::reference_internal)
      .def("minimize", &Solver::MakeMinimize, py::arg("v"), py::arg("step"),
           DOC(operations_research, Solver, MakeMinimize),
           py::return_value_policy::reference_internal)
      .def("maximize", &Solver::MakeMaximize, py::arg("v"), py::arg("step"),
           DOC(operations_research, Solver, MakeMaximize),
           py::return_value_policy::reference_internal)
      .def("optimize", &Solver::MakeOptimize, py::arg("maximize"), py::arg("v"),
           py::arg("step"), DOC(operations_research, Solver, MakeOptimize),
           py::return_value_policy::reference_internal)
      .def("weighted_minimize",
         [](Solver* s,
            const std::vector<PropagationBaseObject*>& sub_objectives,
            const std::vector<int64_t>& weights, int64_t step) {
             return s->MakeWeightedMinimize(
                 ToIntVarArray(sub_objectives), weights, step);
           },
           py::arg("sub_objectives"), py::arg("weights"), py::arg("step"),
           DOC(operations_research, Solver, MakeWeightedMinimize),
           py::return_value_policy::reference_internal)
      .def("weighted_maximize",
           [](Solver* s,
              const std::vector<PropagationBaseObject*>& sub_objectives,
              const std::vector<int64_t>& weights, int64_t step) {
             return s->MakeWeightedMaximize(ToIntVarArray(sub_objectives),
                                           weights, step);
           },
           py::arg("sub_objectives"), py::arg("weights"), py::arg("step"),
           DOC(operations_research, Solver, MakeWeightedMaximize),
           py::return_value_policy::reference_internal)
      .def("weighted_optimize",
           [](Solver* s, bool maximize,
              const std::vector<PropagationBaseObject*>& sub_objectives,
              const std::vector<int64_t>& weights, int64_t step) {
             return s->MakeWeightedOptimize(
                 maximize, ToIntVarArray(sub_objectives), weights, step);
           },
           py::arg("maximize"), py::arg("sub_objectives"), py::arg("weights"),
           py::arg("step"),
           DOC(operations_research, Solver, MakeWeightedOptimize),
           py::return_value_policy::reference_internal)
      .def("lexicographic_optimize", &Solver::MakeLexicographicOptimize,
           py::arg("maximize"), py::arg("variables"), py::arg("steps"),
           DOC(operations_research, Solver, MakeLexicographicOptimize),
           py::return_value_policy::reference_internal)
      .def(
          "sum",
          [](Solver* s, const std::vector<PropagationBaseObject*>& exprs) {
             return s->MakeSum(ToIntVarArray(exprs));
           },
           DOC(operations_research, Solver, MakeSum),
           py::return_value_policy::reference_internal)
      .def("weighted_sum",
           [](Solver* s,
              const std::vector<PropagationBaseObject*>& exprs,
              const std::vector<int64_t>& coeffs) {
             return s->MakeScalProd(ToIntVarArray(exprs), coeffs);
           },
           DOC(operations_research, Solver, MakeSum),
           py::return_value_policy::reference_internal)
      .def("element",
           py::overload_cast<const std::vector<int64_t>&, IntVar*>(
               &Solver::MakeElement),
           py::arg("values"), py::arg("index"),
           DOC(operations_research, Solver, MakeElement),
           py::return_value_policy::reference_internal)
      .def("min",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs) {
             return s->MakeMin(ToIntVarArray(exprs));
           },
           DOC(operations_research, Solver, MakeMin),
           py::return_value_policy::reference_internal)
      .def("min", py::overload_cast<IntExpr*, IntExpr*>(&Solver::MakeMin),
           DOC(operations_research, Solver, MakeMin_2),
           py::return_value_policy::reference_internal)
      .def("min", py::overload_cast<IntExpr*, int64_t>(&Solver::MakeMin),
           DOC(operations_research, Solver, MakeMin_3),
           py::return_value_policy::reference_internal)
      .def("max",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs) {
             return s->MakeMax(ToIntVarArray(exprs));
           },
           DOC(operations_research, Solver, MakeMax),
           py::return_value_policy::reference_internal)
      .def("max", py::overload_cast<IntExpr*, IntExpr*>(&Solver::MakeMax),
           DOC(operations_research, Solver, MakeMax_2),
           py::return_value_policy::reference_internal)
      .def("max", py::overload_cast<IntExpr*, int64_t>(&Solver::MakeMax),
           DOC(operations_research, Solver, MakeMax_3),
           py::return_value_policy::reference_internal)
      .def("convex_piecewise_expr", &Solver::MakeConvexPiecewiseExpr,
           py::arg("expr"), py::arg("early_cost"), py::arg("early_date"),
           py::arg("late_date"), py::arg("late_cost"),
           DOC(operations_research, Solver, MakeConvexPiecewiseExpr),
           py::return_value_policy::reference_internal)
      .def("semi_continuous_expr", &Solver::MakeSemiContinuousExpr,
           py::arg("expr"), py::arg("fixed_charge"), py::arg("step"),
           DOC(operations_research, Solver, MakeSemiContinuousExpr),
           py::return_value_policy::reference_internal)
      .def("piecewise_linear_expr", &Solver::MakePiecewiseLinearExpr,
           py::arg("expr"), py::arg("f"),
           DOC(operations_research, Solver, MakePiecewiseLinearExpr),
           py::return_value_policy::reference_internal)
      .def("modulo", py::overload_cast<IntExpr*, int64_t>(&Solver::MakeModulo),
           py::arg("x"), py::arg("mod"),
           DOC(operations_research, Solver, MakeModulo),
           py::return_value_policy::reference_internal)
      .def("modulo", py::overload_cast<IntExpr*, IntExpr*>(&Solver::MakeModulo),
           py::arg("x"), py::arg("mod"),
           DOC(operations_research, Solver, MakeModulo_2),
           py::return_value_policy::reference_internal)
      .def("conditional_expression", &Solver::MakeConditionalExpression,
           py::arg("condition"), py::arg("expr"), py::arg("unperformed_value"),
           DOC(operations_research, Solver, MakeConditionalExpression),
           py::return_value_policy::reference_internal)
      .def("print_model_visitor", &Solver::MakePrintModelVisitor,
           DOC(operations_research, Solver, MakePrintModelVisitor),
           py::return_value_policy::reference_internal)
      .def("phase",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
                       Solver::IntVarStrategy var_strategy,
                       Solver::IntValueStrategy val_strategy) {
             return s->MakePhase(ToIntVarArray(exprs), var_strategy,
                                 val_strategy);
           },
           DOC(operations_research, Solver, MakePhase),
           py::return_value_policy::reference_internal)
      .def("assign_variable_value", &Solver::MakeAssignVariableValue,
           py::arg("var"), py::arg("val"),
           DOC(operations_research, Solver, MakeAssignVariableValue),
           py::return_value_policy::reference_internal)
      .def("local_search_phase",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              DecisionBuilder* db, py::capsule ls_params) {
             void* ptr = ls_params;
             return s->MakeLocalSearchPhase(
                 ToIntVarArray(exprs), db,
                 reinterpret_cast<LocalSearchPhaseParameters*>(ptr));
           },
           py::arg("vars"), py::arg("db"), py::arg("ls_params"),
           py::return_value_policy::reference_internal)
      .def("local_search_phase",
           [](Solver* s, Assignment* assignment, py::capsule ls_params) {
             void* ptr = ls_params;
             return s->MakeLocalSearchPhase(
                 assignment,
                 reinterpret_cast<
                     LocalSearchPhaseParameters*>(ptr));
           },
           py::arg("assignment"), py::arg("ls_params"),
           py::return_value_policy::reference_internal)
      .def("random_lns_operator",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              int number_of_variables) {
             return s->MakeRandomLnsOperator(ToIntVarArray(exprs),
                                              number_of_variables);
           },
           py::arg("exprs"), py::arg("number_of_variables"),
           py::return_value_policy::reference_internal)
      .def("operator",
           [](Solver* s, const std::vector<PropagationBaseObject*>& exprs,
              Solver::LocalSearchOperators op) {
             return s->MakeOperator(ToIntVarArray(exprs), op);
           },
           py::arg("vars"), py::arg("op"),
           py::return_value_policy::reference_internal)
      .def("concatenate_operators",
           [](Solver* s, const std::vector<LocalSearchOperator*>& ops) {
             return s->ConcatenateOperators(ops);
           },
           py::arg("ops"), py::return_value_policy::reference_internal)
      .def("compose",
           [](Solver* s, const std::vector<DecisionBuilder*>& dbs) {
             return s->Compose(dbs);
           },
           py::arg("dbs"), py::return_value_policy::reference_internal)
      .def("search_log",
           [](Solver* s, int64_t period, IntVar* var) {
             return s->MakeSearchLog(period, var);
           },
           py::arg("period"), py::arg("var"),
           py::return_value_policy::reference_internal)
      .def("split_variable_domain", &Solver::MakeSplitVariableDomain,
           py::arg("var"), py::arg("val"), py::arg("start_with_lower_half"),
           py::return_value_policy::reference_internal)
      .def("fail_decision", &Solver::MakeFailDecision,
           DOC(operations_research, Solver, MakeFailDecision),
           py::return_value_policy::reference_internal)
      .def("phase",
           py::overload_cast<const std::vector<IntervalVar*>&,
                             Solver::IntervalStrategy>(&Solver::MakePhase),
           DOC(operations_research, Solver, MakePhase),
           py::return_value_policy::reference_internal)
      .def("phase",
           py::overload_cast<const std::vector<SequenceVar*>&,
                             Solver::SequenceStrategy>(&Solver::MakePhase),
           DOC(operations_research, Solver, MakePhase),
           py::return_value_policy::reference_internal);

  py::class_<PropagationBaseObject, BaseObject>(
      m, "PropagationBaseObject",
      DOC(operations_research, PropagationBaseObject))
      .def_property("name", &PropagationBaseObjectPythonHelper::name,
                    &PropagationBaseObjectPythonHelper::SetName)
      .def_property_readonly("solver",
                             &PropagationBaseObjectPythonHelper::solver);

  // Note: no ctor.
  py::class_<IntExpr, PropagationBaseObject>(m, "IntExpr",
                                             DOC(operations_research, IntExpr))
      .def("min", &IntExprPythonHelper::Min,
           DOC(operations_research, IntExpr, Min))
      .def("max", &IntExprPythonHelper::Max,
           DOC(operations_research, IntExpr, Max))
      .def("set_min", &IntExprPythonHelper::SetMin,
           DOC(operations_research, IntExpr, SetMin), py::arg("m"))
      .def("set_max", &IntExprPythonHelper::SetMax,
           DOC(operations_research, IntExpr, SetMax), py::arg("m"))
      .def("set_range", &IntExprPythonHelper::SetRange,
           DOC(operations_research, IntExpr, SetRange), py::arg("mi"),
           py::arg("ma"))
      .def("set_value", &IntExprPythonHelper::SetValue,
           DOC(operations_research, IntExpr, SetValue), py::arg("v"))
      .def("bound", &IntExprPythonHelper::Bound,
           DOC(operations_research, IntExpr, Bound))
      .def("var", &IntExpr::Var, DOC(operations_research, IntExpr, Var),
           py::return_value_policy::reference_internal)
      .def(
          "__add__",
          [](IntExpr* e, int64_t arg) { return e->solver()->MakeSum(e, arg); },
          py::return_value_policy::reference_internal)
      .def(
          "__add__",
          [](IntExpr* e, IntExpr* arg) { return e->solver()->MakeSum(e, arg); },
          py::return_value_policy::reference_internal)
      .def(
          "__add__",
          [](IntExpr* e, Constraint* arg) {
            return e->solver()->MakeSum(e, arg->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__radd__",
          [](IntExpr* e, int64_t arg) { return e->solver()->MakeSum(e, arg); },
          py::return_value_policy::reference_internal)
      .def(
          "__radd__",
          [](IntExpr* e, IntExpr* arg) { return e->solver()->MakeSum(e, arg); },
          py::return_value_policy::reference_internal)
      .def(
          "__radd__",
          [](IntExpr* e, Constraint* arg) {
            return e->solver()->MakeSum(e, arg->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__mul__",
          [](IntExpr* e, int64_t arg) { return e->solver()->MakeProd(e, arg); },
          py::return_value_policy::reference_internal)
      .def(
          "__mul__",
          [](IntExpr* e, IntExpr* arg) {
            return e->solver()->MakeProd(e, arg);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__mul__",
          [](IntExpr* e, Constraint* arg) {
            return e->solver()->MakeProd(e, arg->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__rmul__",
          [](IntExpr* e, int64_t arg) { return e->solver()->MakeProd(e, arg); },
          py::return_value_policy::reference_internal)
      .def(
          "__rmul__",
          [](IntExpr* e, IntExpr* arg) {
            return e->solver()->MakeProd(e, arg);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__rmul__",
          [](IntExpr* e, Constraint* arg) {
            return e->solver()->MakeProd(e, arg->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__sub__",
          [](IntExpr* e, int64_t v) { return e->solver()->MakeSum(e, -v); },
          py::return_value_policy::reference_internal)
      .def(
          "__sub__",
          [](IntExpr* e, IntExpr* other) {
            return e->solver()->MakeDifference(e, other);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__sub__",
          [](IntExpr* e, Constraint* other) {
            return e->solver()->MakeDifference(e, other->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__rsub__",
          [](IntExpr* e, int64_t v) {
            return e->solver()->MakeDifference(v, e);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__rsub__",
          [](IntExpr* e, IntExpr* other) {
            return e->solver()->MakeDifference(other, e);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__rsub__",
          [](IntExpr* e, Constraint* other) {
            return e->solver()->MakeDifference(other->Var(), e);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__floordiv__",
          [](IntExpr* e, int64_t v) { return e->solver()->MakeDiv(e, v); },
          py::return_value_policy::reference_internal)
      .def(
          "__floordiv__",
          [](IntExpr* e, IntExpr* other) {
            return e->solver()->MakeDiv(e, other);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__floordiv__",
          [](IntExpr* e, Constraint* other) {
            return e->solver()->MakeDiv(e, other->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__mod__",
          [](IntExpr* e, int64_t v) { return e->solver()->MakeModulo(e, v); },
          py::return_value_policy::reference_internal)
      .def(
          "__mod__",
          [](IntExpr* e, IntExpr* other) {
            return e->solver()->MakeModulo(e, other);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__mod__",
          [](IntExpr* e, Constraint* other) {
            return e->solver()->MakeModulo(e, other->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__neg__", [](IntExpr* e) { return e->solver()->MakeOpposite(e); },
          py::return_value_policy::reference_internal)
      .def(
          "__abs__", [](IntExpr* e) { return e->solver()->MakeAbs(e); },
          py::return_value_policy::reference_internal)
      .def(
          "square", [](IntExpr* e) { return e->solver()->MakeSquare(e); },
          py::return_value_policy::reference_internal)
      .def(
          "__eq__",
          [](IntExpr* left, IntExpr* right) {
            return left->solver()->MakeEquality(left, right);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__eq__",
          [](IntExpr* left, int64_t right) {
            return left->solver()->MakeEquality(left, right);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__eq__",
          [](IntExpr* left, Constraint* right) {
            return left->solver()->MakeEquality(left, right->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__ne__",
          [](IntExpr* left, IntExpr* right) {
            return left->solver()->MakeNonEquality(left, right);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__ne__",
          [](IntExpr* left, int64_t right) {
            return left->solver()->MakeNonEquality(left, right);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__ne__",
          [](IntExpr* left, Constraint* right) {
            return left->solver()->MakeNonEquality(left, right->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__ge__",
          [](IntExpr* left, IntExpr* right) {
            return left->solver()->MakeGreaterOrEqual(left, right);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__ge__",
          [](IntExpr* left, int64_t right) {
            return left->solver()->MakeGreaterOrEqual(left, right);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__ge__",
          [](IntExpr* left, Constraint* right) {
            return left->solver()->MakeGreaterOrEqual(left, right->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__gt__",
          [](IntExpr* left, IntExpr* right) {
            return left->solver()->MakeGreater(left, right);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__gt__",
          [](IntExpr* left, int64_t right) {
            return left->solver()->MakeGreater(left, right);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__gt__",
          [](IntExpr* left, Constraint* right) {
            return left->solver()->MakeGreater(left, right->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__le__",
          [](IntExpr* left, IntExpr* right) {
            return left->solver()->MakeLessOrEqual(left, right);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__le__",
          [](IntExpr* left, int64_t right) {
            return left->solver()->MakeLessOrEqual(left, right);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__le__",
          [](IntExpr* left, Constraint* right) {
            return left->solver()->MakeLessOrEqual(left, right->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__lt__",
          [](IntExpr* left, IntExpr* right) {
            return left->solver()->MakeLess(left, right);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__lt__",
          [](IntExpr* left, int64_t right) {
            return left->solver()->MakeLess(left, right);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__lt__",
          [](IntExpr* left, Constraint* right) {
            return left->solver()->MakeLess(left, right->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "index_of",
          [](IntExpr* expr, const std::vector<int64_t>& values) {
            return expr->solver()->MakeElement(values, expr->Var());
          },
          py::return_value_policy::reference);

  // Note: no ctor.
  py::class_<IntVar, IntExpr>(m, "IntVar", DOC(operations_research, IntVar))
      .def(
          "domain_iterator",
          [](IntVar* var) { return var->MakeDomainIterator(false); },
          py::return_value_policy::take_ownership)
      .def(
          "hole_iterator",
          [](IntVar* var) { return var->MakeHoleIterator(false); },
          py::return_value_policy::take_ownership)
      .def("value", &IntVarPythonHelper::Value,
           DOC(operations_research, IntVar, Value))
      .def("remove_value", &IntVarPythonHelper::RemoveValue,
           DOC(operations_research, IntVar, RemoveValue), py::arg("v"))
      .def("remove_values", &IntVarPythonHelper::RemoveValues,
           DOC(operations_research, IntVar, RemoveValues), py::arg("values"))
      .def("size", &IntVarPythonHelper::Size,
           DOC(operations_research, IntVar, Size))
      .def("contains", &IntVar::Contains,
           DOC(operations_research, IntVar, Contains))
      .def("remove_interval", &IntVar::RemoveInterval,
           DOC(operations_research, IntVar, RemoveInterval))
      .def("set_values", &IntVar::SetValues,
           DOC(operations_research, IntVar, SetValues))
      .def("var_type", &IntVar::VarType,
           DOC(operations_research, IntVar, VarType));

  // Note: no ctor.
  py::class_<IntervalVar, PropagationBaseObject>(
      m, "IntervalVar", DOC(operations_research, IntervalVar))
      .def_property_readonly(
          "name", &IntervalVar::name,
          DOC(operations_research, PropagationBaseObject, name))
      .def("start_min", &IntervalVar::StartMin,
           DOC(operations_research, IntervalVar, StartMin))
      .def("start_max", &IntervalVar::StartMax,
           DOC(operations_research, IntervalVar, StartMax))
      .def("end_min", &IntervalVar::EndMin,
           DOC(operations_research, IntervalVar, EndMin))
      .def("end_max", &IntervalVar::EndMax,
           DOC(operations_research, IntervalVar, EndMax))
      .def("duration_min", &IntervalVar::DurationMin,
           DOC(operations_research, IntervalVar, DurationMin))
      .def("duration_max", &IntervalVar::DurationMax,
           DOC(operations_research, IntervalVar, DurationMax))
      .def("must_be_performed", &IntervalVar::MustBePerformed,
           DOC(operations_research, IntervalVar, MustBePerformed))
      .def("may_be_performed", &IntervalVar::MayBePerformed,
           DOC(operations_research, IntervalVar, MayBePerformed))
      .def("start_expr", &IntervalVar::StartExpr,
           DOC(operations_research, IntervalVar, StartExpr),
           py::return_value_policy::reference_internal)
      .def("duration_expr", &IntervalVar::DurationExpr,
           DOC(operations_research, IntervalVar, DurationExpr),
           py::return_value_policy::reference_internal)
      .def("end_expr", &IntervalVar::EndExpr,
           DOC(operations_research, IntervalVar, EndExpr),
           py::return_value_policy::reference_internal)
      .def("performed_expr", &IntervalVar::PerformedExpr,
           DOC(operations_research, IntervalVar, PerformedExpr),
           py::return_value_policy::reference_internal)
      .def("set_start_min", &IntervalVar::SetStartMin,
           DOC(operations_research, IntervalVar, SetStartMin))
      .def("set_start_max", &IntervalVar::SetStartMax,
           DOC(operations_research, IntervalVar, SetStartMax))
      .def("set_start_range", &IntervalVar::SetStartRange,
           DOC(operations_research, IntervalVar, SetStartRange))
      .def("set_duration_min", &IntervalVar::SetDurationMin,
           DOC(operations_research, IntervalVar, SetDurationMin))
      .def("set_duration_max", &IntervalVar::SetDurationMax,
           DOC(operations_research, IntervalVar, SetDurationMax))
      .def("set_duration_range", &IntervalVar::SetDurationRange,
           DOC(operations_research, IntervalVar, SetDurationRange))
      .def("set_end_min", &IntervalVar::SetEndMin,
           DOC(operations_research, IntervalVar, SetEndMin))
      .def("set_end_max", &IntervalVar::SetEndMax,
           DOC(operations_research, IntervalVar, SetEndMax))
      .def("set_end_range", &IntervalVar::SetEndRange,
           DOC(operations_research, IntervalVar, SetEndRange))
      .def("set_performed", &IntervalVar::SetPerformed,
           DOC(operations_research, IntervalVar, SetPerformed))
      .def("when_start_range",
           py::overload_cast<Demon*>(&IntervalVar::WhenStartRange),
           DOC(operations_research, IntervalVar, WhenStartRange))
      .def("when_start_range",
           py::overload_cast<Solver::Closure>(&IntervalVar::WhenStartRange),
           DOC(operations_research, IntervalVar, WhenStartRange))
      .def("when_start_range",
           py::overload_cast<Solver::Action>(&IntervalVar::WhenStartRange),
           DOC(operations_research, IntervalVar, WhenStartRange))
      .def("when_start_bound",
           py::overload_cast<Demon*>(&IntervalVar::WhenStartBound),
           DOC(operations_research, IntervalVar, WhenStartBound))
      .def("when_start_bound",
           py::overload_cast<Solver::Closure>(&IntervalVar::WhenStartBound),
           DOC(operations_research, IntervalVar, WhenStartBound))
      .def("when_start_bound",
           py::overload_cast<Solver::Action>(&IntervalVar::WhenStartBound),
           DOC(operations_research, IntervalVar, WhenStartBound))
      .def("when_duration_range",
           py::overload_cast<Demon*>(&IntervalVar::WhenDurationRange),
           DOC(operations_research, IntervalVar, WhenDurationRange))
      .def("when_duration_range",
           py::overload_cast<Solver::Closure>(&IntervalVar::WhenDurationRange),
           DOC(operations_research, IntervalVar, WhenDurationRange))
      .def("when_duration_range",
           py::overload_cast<Solver::Action>(&IntervalVar::WhenDurationRange),
           DOC(operations_research, IntervalVar, WhenDurationRange))
      .def("when_duration_bound",
           py::overload_cast<Demon*>(&IntervalVar::WhenDurationBound),
           DOC(operations_research, IntervalVar, WhenDurationBound))
      .def("when_duration_bound",
           py::overload_cast<Solver::Closure>(&IntervalVar::WhenDurationBound),
           DOC(operations_research, IntervalVar, WhenDurationBound))
      .def("when_duration_bound",
           py::overload_cast<Solver::Action>(&IntervalVar::WhenDurationBound),
           DOC(operations_research, IntervalVar, WhenDurationBound))
      .def("when_end_range",
           py::overload_cast<Demon*>(&IntervalVar::WhenEndRange),
           DOC(operations_research, IntervalVar, WhenEndRange))
      .def("when_end_range",
           py::overload_cast<Solver::Closure>(&IntervalVar::WhenEndRange),
           DOC(operations_research, IntervalVar, WhenEndRange))
      .def("when_end_range",
           py::overload_cast<Solver::Action>(&IntervalVar::WhenEndRange),
           DOC(operations_research, IntervalVar, WhenEndRange))
      .def("when_end_bound",
           py::overload_cast<Demon*>(&IntervalVar::WhenEndBound),
           DOC(operations_research, IntervalVar, WhenEndBound))
      .def("when_end_bound",
           py::overload_cast<Solver::Closure>(&IntervalVar::WhenEndBound),
           DOC(operations_research, IntervalVar, WhenEndBound))
      .def("when_end_bound",
           py::overload_cast<Solver::Action>(&IntervalVar::WhenEndBound),
           DOC(operations_research, IntervalVar, WhenEndBound))
      .def("when_performed_bound",
           py::overload_cast<Demon*>(&IntervalVar::WhenPerformedBound),
           DOC(operations_research, IntervalVar, WhenPerformedBound))
      .def("when_performed_bound",
           py::overload_cast<Solver::Closure>(&IntervalVar::WhenPerformedBound),
           DOC(operations_research, IntervalVar, WhenPerformedBound))
      .def("when_performed_bound",
           py::overload_cast<Solver::Action>(&IntervalVar::WhenPerformedBound),
           DOC(operations_research, IntervalVar, WhenPerformedBound))
      .def("when_anything",
           py::overload_cast<Demon*>(&IntervalVar::WhenAnything),
           DOC(operations_research, IntervalVar, WhenAnything))
      .def("when_anything",
           py::overload_cast<Solver::Closure>(&IntervalVar::WhenAnything),
           DOC(operations_research, IntervalVar, WhenAnything))
      .def("when_anything",
           py::overload_cast<Solver::Action>(&IntervalVar::WhenAnything),
           DOC(operations_research, IntervalVar, WhenAnything));

  py::class_<Constraint, PropagationBaseObject>(
      m, "Constraint", DOC(operations_research, Constraint))
      .def("__str__", &Constraint::DebugString)
      .def("var", &Constraint::Var, DOC(operations_research, Constraint, Var),
           py::return_value_policy::reference_internal)
      .def(
          "__add__",
          [](Constraint* c, int64_t v) {
            return c->solver()->MakeSum(c->Var(), v);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__add__",
          [](Constraint* c, IntExpr* e) {
            return c->solver()->MakeSum(c->Var(), e);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__add__",
          [](Constraint* c, Constraint* other) {
            return c->solver()->MakeSum(c->Var(), other->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__radd__",
          [](Constraint* c, int64_t v) {
            return c->solver()->MakeSum(c->Var(), v);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__mul__",
          [](Constraint* c, int64_t v) {
            return c->solver()->MakeProd(c->Var(), v);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__mul__",
          [](Constraint* c, IntExpr* e) {
            return c->solver()->MakeProd(c->Var(), e);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__mul__",
          [](Constraint* c, Constraint* other) {
            return c->solver()->MakeProd(c->Var(), other->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__rmul__",
          [](Constraint* c, int64_t v) {
            return c->solver()->MakeProd(c->Var(), v);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__sub__",
          [](Constraint* c, int64_t v) {
            return c->solver()->MakeSum(c->Var(), -v);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__sub__",
          [](Constraint* c, IntExpr* e) {
            return c->solver()->MakeDifference(c->Var(), e);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__sub__",
          [](Constraint* c, Constraint* other) {
            return c->solver()->MakeDifference(c->Var(), other->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__rsub__",
          [](Constraint* c, int64_t v) {
            return c->solver()->MakeDifference(v, c->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__floordiv__",
          [](Constraint* c, int64_t v) {
            return c->solver()->MakeDiv(c->Var(), v);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__floordiv__",
          [](Constraint* c, IntExpr* e) {
            return c->solver()->MakeDiv(c->Var(), e);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__floordiv__",
          [](Constraint* c, Constraint* other) {
            return c->solver()->MakeDiv(c->Var(), other->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__mod__",
          [](Constraint* c, int64_t v) {
            return c->solver()->MakeModulo(c->Var(), v);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__mod__",
          [](Constraint* c, IntExpr* e) {
            return c->solver()->MakeModulo(c->Var(), e);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__mod__",
          [](Constraint* c, Constraint* other) {
            return c->solver()->MakeModulo(c->Var(), other->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__neg__",
          [](Constraint* c) { return c->solver()->MakeOpposite(c->Var()); },
          py::return_value_policy::reference_internal)
      .def(
          "__abs__",
          [](Constraint* c) { return c->solver()->MakeAbs(c->Var()); },
          py::return_value_policy::reference_internal)
      .def(
          "square",
          [](Constraint* c) { return c->solver()->MakeSquare(c->Var()); },
          py::return_value_policy::reference_internal)
      .def(
          "__eq__",
          [](Constraint* c, int64_t v) {
            return c->solver()->MakeEquality(c->Var(), v);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__eq__",
          [](Constraint* c, IntExpr* e) {
            return c->solver()->MakeEquality(c->Var(), e);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__eq__",
          [](Constraint* c, Constraint* other) {
            return c->solver()->MakeEquality(c->Var(), other->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__ne__",
          [](Constraint* c, int64_t v) {
            return c->solver()->MakeNonEquality(c->Var(), v);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__ne__",
          [](Constraint* c, IntExpr* e) {
            return c->solver()->MakeNonEquality(c->Var(), e);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__ne__",
          [](Constraint* c, Constraint* other) {
            return c->solver()->MakeNonEquality(c->Var(), other->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__ge__",
          [](Constraint* c, int64_t v) {
            return c->solver()->MakeGreaterOrEqual(c->Var(), v);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__ge__",
          [](Constraint* c, IntExpr* e) {
            return c->solver()->MakeGreaterOrEqual(c->Var(), e);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__ge__",
          [](Constraint* c, Constraint* other) {
            return c->solver()->MakeGreaterOrEqual(c->Var(), other->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__gt__",
          [](Constraint* c, int64_t v) {
            return c->solver()->MakeGreater(c->Var(), v);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__gt__",
          [](Constraint* c, IntExpr* e) {
            return c->solver()->MakeGreater(c->Var(), e);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__gt__",
          [](Constraint* c, Constraint* other) {
            return c->solver()->MakeGreater(c->Var(), other->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__le__",
          [](Constraint* c, int64_t v) {
            return c->solver()->MakeLessOrEqual(c->Var(), v);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__le__",
          [](Constraint* c, IntExpr* e) {
            return c->solver()->MakeLessOrEqual(c->Var(), e);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__le__",
          [](Constraint* c, Constraint* other) {
            return c->solver()->MakeLessOrEqual(c->Var(), other->Var());
          },
          py::return_value_policy::reference_internal)
      .def(
          "__lt__",
          [](Constraint* c, int64_t v) {
            return c->solver()->MakeLess(c->Var(), v);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__lt__",
          [](Constraint* c, IntExpr* e) {
            return c->solver()->MakeLess(c->Var(), e);
          },
          py::return_value_policy::reference_internal)
      .def(
          "__lt__",
          [](Constraint* c, Constraint* other) {
            return c->solver()->MakeLess(c->Var(), other->Var());
          },
          py::return_value_policy::reference_internal);

  py::class_<DecisionBuilder, BaseObject>(
      m, "DecisionBuilderBase", DOC(operations_research, DecisionBuilder))
      .def("__str__", &DecisionBuilder::DebugString)
      .def_property("name", &DecisionBuilder::GetName,
                    &DecisionBuilder::set_name);

  py::class_<PyDecisionBuilder, DecisionBuilder, PyDecisionBuilderHelper>(
      m, "DecisionBuilder", DOC(operations_research, DecisionBuilder))
      .def(py::init<>())
      .def("next", &PyDecisionBuilder::next,
           DOC(operations_research, DecisionBuilder, Next))
      .def("__str__", &DecisionBuilder::DebugString)
      .def_property("name", &PyDecisionBuilder::GetName,
                    &PyDecisionBuilder::set_name);

  py::class_<Decision, BaseObject>(m, "DecisionBase",
                                   DOC(operations_research, Decision))
      .def("__str__", &Decision::DebugString);

  py::class_<PyDecision, Decision, PyDecisionHelper>(
      m, "Decision", DOC(operations_research, Decision))
      .def(py::init<>())
      .def("__str__", &Decision::DebugString)
      .def("apply", &PyDecision::apply,
           DOC(operations_research, Decision, Apply))
      .def("refute", &PyDecision::refute,
           DOC(operations_research, Decision, Refute))
      .def("accept", &PyDecision::accept,
           DOC(operations_research, Decision, Accept));

  // Note: no ctor.
  py::class_<ModelVisitor, BaseObject>(m, "ModelVisitor",
                                       DOC(operations_research, ModelVisitor));

  py::class_<Assignment, PropagationBaseObject>(
      m, "Assignment", DOC(operations_research, Assignment))
      .def(py::init<Solver*>())
      .def("clear", &Assignment::Clear)
      .def("empty", &Assignment::Empty)
      .def("size", &Assignment::Size)
      .def("num_int_vars", &Assignment::NumIntVars)
      .def("num_interval_vars", &Assignment::NumIntervalVars)
      .def("num_sequence_vars", &Assignment::NumSequenceVars)
      .def("int_var_container", &Assignment::IntVarContainer,
           py::return_value_policy::reference_internal)
      .def("interval_var_container", &Assignment::IntervalVarContainer,
           py::return_value_policy::reference_internal)
      .def("sequence_var_container", &Assignment::SequenceVarContainer,
           py::return_value_policy::reference_internal)
      .def("store", &Assignment::Store)
      .def("restore", &Assignment::Restore)
      .def("load", py::overload_cast<const std::string&>(&Assignment::Load),
           py::arg("filename"))
      .def("load", py::overload_cast<const AssignmentProto&>(&Assignment::Load),
           py::arg("assignment_proto"))
      .def("add_objective", &Assignment::AddObjective, py::arg("v"))
      .def("add_objectives", &Assignment::AddObjectives, py::arg("vars"))
      .def("clear_objective", &Assignment::ClearObjective)
      .def("num_objectives", &Assignment::NumObjectives)
      .def("objective", &Assignment::Objective)
      .def("objective_from_index", &Assignment::ObjectiveFromIndex,
           py::arg("index"))
      .def("has_objective", &Assignment::HasObjective)
      .def("has_objective_from_index", &Assignment::HasObjectiveFromIndex,
           py::arg("index"))
      .def("objective_min", &Assignment::ObjectiveMin)
      .def("objective_max", &Assignment::ObjectiveMax)
      .def("objective_value", &Assignment::ObjectiveValue)
      .def("objective_bound", &Assignment::ObjectiveBound)
      .def("set_objective_min", &Assignment::SetObjectiveMin, py::arg("m"))
      .def("set_objective_max", &Assignment::SetObjectiveMax, py::arg("m"))
      .def("set_objective_value", &Assignment::SetObjectiveValue,
           py::arg("value"))
      .def("set_objective_range", &Assignment::SetObjectiveRange, py::arg("l"),
           py::arg("u"))
      .def("objective_min_from_index", &Assignment::ObjectiveMinFromIndex,
           py::arg("index"))
      .def("objective_max_from_index", &Assignment::ObjectiveMaxFromIndex,
           py::arg("index"))
      .def("objective_value_from_index", &Assignment::ObjectiveValueFromIndex,
           py::arg("index"))
      .def("objective_bound_from_index", &Assignment::ObjectiveBoundFromIndex,
           py::arg("index"))
      .def("set_objective_min_from_index",
           &Assignment::SetObjectiveMinFromIndex, py::arg("index"),
           py::arg("m"))
      .def("set_objective_max_from_index",
           &Assignment::SetObjectiveMaxFromIndex, py::arg("index"),
           py::arg("m"))
      .def("set_objective_range_from_index",
           &Assignment::SetObjectiveRangeFromIndex, py::arg("index"),
           py::arg("l"), py::arg("u"))
      .def("add", py::overload_cast<IntVar*>(&Assignment::Add), py::arg("var"),
           py::return_value_policy::reference_internal)
      .def("add",
           py::overload_cast<const std::vector<IntVar*>&>(&Assignment::Add),
           py::arg("var"), py::return_value_policy::reference_internal)
      .def("min", &Assignment::Min, py::arg("var"))
      .def("max", &Assignment::Max, py::arg("var"))
      .def("value", &Assignment::Value, py::arg("var"))
      .def("bound", &Assignment::Bound, py::arg("var"))
      .def("set_min", &Assignment::SetMin, py::arg("var"), py::arg("m"))
      .def("set_max", &Assignment::SetMax, py::arg("var"), py::arg("m"))
      .def("set_range", &Assignment::SetRange, py::arg("var"), py::arg("l"),
           py::arg("u"))
      .def("set_value", &Assignment::SetValue, py::arg("var"), py::arg("value"))
      .def("add", py::overload_cast<IntervalVar*>(&Assignment::Add),
           py::arg("var"), py::return_value_policy::reference_internal)
      .def(
          "add",
          py::overload_cast<const std::vector<IntervalVar*>&>(&Assignment::Add),
          py::arg("var"), py::return_value_policy::reference_internal)
      .def("add", py::overload_cast<SequenceVar*>(&Assignment::Add),
           py::arg("var"), py::return_value_policy::reference_internal)
      .def(
          "add",
          py::overload_cast<const std::vector<SequenceVar*>&>(&Assignment::Add),
          py::arg("var"), py::return_value_policy::reference_internal)
      .def("forward_sequence", &Assignment::ForwardSequence, py::arg("var"))
      .def("backward_sequence", &Assignment::BackwardSequence, py::arg("var"))
      .def("unperformed", &Assignment::Unperformed, py::arg("var"))
      .def("set_forward_sequence", &Assignment::SetForwardSequence,
           py::arg("var"), py::arg("forward_sequence"))
      .def("set_backward_sequence", &Assignment::SetBackwardSequence,
           py::arg("var"), py::arg("backward_sequence"))
      .def("set_unperformed", &Assignment::SetUnperformed, py::arg("var"),
           py::arg("unperformed"));

  py::class_<SequenceVar, PropagationBaseObject>(
      m, "SequenceVar", DOC(operations_research, SequenceVar))
      .def("rank_first", &SequenceVar::RankFirst,
           DOC(operations_research, SequenceVar, RankFirst))
      .def("rank_not_first", &SequenceVar::RankNotFirst,
           DOC(operations_research, SequenceVar, RankNotFirst))
      .def("rank_last", &SequenceVar::RankLast,
           DOC(operations_research, SequenceVar, RankLast))
      .def("rank_not_last", &SequenceVar::RankNotLast,
           DOC(operations_research, SequenceVar, RankNotLast))
      .def("interval", &SequenceVar::Interval,
           DOC(operations_research, SequenceVar, Interval))
      .def("next", &SequenceVar::Next,
           DOC(operations_research, SequenceVar, Next))
      .def("size", &SequenceVar::size,
           DOC(operations_research, SequenceVar, size))
      .def("__str__", &SequenceVar::DebugString);

  py::class_<DisjunctiveConstraint, Constraint>(
      m, "DisjunctiveConstraint",
      DOC(operations_research, DisjunctiveConstraint))
      .def("make_sequence_var", &DisjunctiveConstraint::MakeSequenceVar,
           DOC(operations_research, DisjunctiveConstraint, MakeSequenceVar),
           py::return_value_policy::reference_internal)
      .def("set_transition_time", &DisjunctiveConstraint::SetTransitionTime,
           DOC(operations_research, DisjunctiveConstraint, SetTransitionTime))
      .def_property(
          "transition_time", &DisjunctiveConstraint::TransitionTime,
          &DisjunctiveConstraint::SetTransitionTime,
          DOC(operations_research, DisjunctiveConstraint, TransitionTime));

  py::class_<Pack, Constraint>(m, "Pack", DOC(operations_research, Pack))
      .def("add_weighted_sum_less_or_equal_constant_dimension",
           py::overload_cast<const std::vector<int64_t>&,
                             const std::vector<int64_t>&>(
               &Pack::AddWeightedSumLessOrEqualConstantDimension),
           DOC(operations_research, Pack,
               AddWeightedSumLessOrEqualConstantDimension))
      .def("add_weighted_sum_less_or_equal_constant_dimension",
           py::overload_cast<Solver::IndexEvaluator1,
                             const std::vector<int64_t>&>(
               &Pack::AddWeightedSumLessOrEqualConstantDimension),
           DOC(operations_research, Pack,
               AddWeightedSumLessOrEqualConstantDimension))
      .def("add_weighted_sum_less_or_equal_constant_dimension",
           py::overload_cast<Solver::IndexEvaluator2,
                             const std::vector<int64_t>&>(
               &Pack::AddWeightedSumLessOrEqualConstantDimension),
           DOC(operations_research, Pack,
               AddWeightedSumLessOrEqualConstantDimension))
      .def(
          "add_weighted_sum_equal_var_dimension",
          [](Pack* pack, const std::vector<int64_t>& weights,
             const std::vector<PropagationBaseObject*>& exprs) {
            return pack->AddWeightedSumEqualVarDimension(weights,
                                                         ToIntVarArray(exprs));
          },
          DOC(operations_research, Pack, AddWeightedSumEqualVarDimension))
      .def(
          "add_weighted_sum_equal_var_dimension",
          [](Pack* pack, Solver::IndexEvaluator2 weights,
             const std::vector<PropagationBaseObject*>& exprs) {
            return pack->AddWeightedSumEqualVarDimension(weights,
                                                         ToIntVarArray(exprs));
          },
          DOC(operations_research, Pack, AddWeightedSumEqualVarDimension))
      .def("add_sum_variable_weights_less_or_equal_constant_dimension",
           &Pack::AddSumVariableWeightsLessOrEqualConstantDimension,
           DOC(operations_research, Pack,
               AddSumVariableWeightsLessOrEqualConstantDimension))
      .def("add_weighted_sum_of_assigned_dimension",
           &Pack::AddWeightedSumOfAssignedDimension,
           DOC(operations_research, Pack, AddWeightedSumOfAssignedDimension))
      .def("add_count_used_bin_dimension", &Pack::AddCountUsedBinDimension,
           DOC(operations_research, Pack, AddCountUsedBinDimension))
      .def("add_count_assigned_items_dimension",
           &Pack::AddCountAssignedItemsDimension,
           DOC(operations_research, Pack, AddCountAssignedItemsDimension));
}  // NOLINT(readability/fn_size)
