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

#include <setjmp.h>  // For FailureProtect. See below.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "ortools/constraint_solver/assignment.pb.h"
#include "ortools/constraint_solver/python/constraint_solver_doc.h"
#include "pybind11/cast.h"
#include "pybind11/functional.h"
#include "pybind11/gil.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11_protobuf/native_proto_caster.h"

using ::operations_research::Assignment;
using ::operations_research::AssignmentProto;
using ::operations_research::BaseObject;
using ::operations_research::Constraint;
using ::operations_research::ConstraintSolverParameters;
using ::operations_research::DecisionBuilder;
using ::operations_research::IntervalVar;
using ::operations_research::IntExpr;
using ::operations_research::IntVar;
using ::operations_research::ModelVisitor;
using ::operations_research::PropagationBaseObject;
using ::operations_research::Solver;
using ::pybind11::arg;

// Used in the PROTECT_FROM_FAILURE macro. See below.
namespace {

struct FailureProtect {
  jmp_buf exception_buffer;
  void JumpBack() { longjmp(exception_buffer, 1); }
};

}  // namespace

#define PROTECT_FROM_FAILURE(this_, action)                         \
  Solver* solver = this_->solver();                                 \
  FailureProtect protect;                                           \
  solver->set_fail_intercept([&protect]() { protect.JumpBack(); }); \
  if (setjmp(protect.exception_buffer) == 0) {                      \
    this_->action;                                                  \
    solver->clear_fail_intercept();                                 \
  } else {                                                          \
    solver->clear_fail_intercept();                                 \
    throw pybind11::value_error("Solver fails outside of solve()"); \
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
    PROTECT_FROM_FAILURE(this_, SetMin(m));
  }
  static void SetMax(IntExpr* this_, int64_t m) {
    PROTECT_FROM_FAILURE(this_, SetMax(m));
  }
  static void SetRange(IntExpr* this_, int64_t mi, int64_t ma) {
    PROTECT_FROM_FAILURE(this_, SetRange(mi, ma));
  }
  static void SetValue(IntExpr* this_, int64_t v) {
    PROTECT_FROM_FAILURE(this_, SetValue(v));
  }
  static bool Bound(IntExpr* this_) { return this_->Bound(); }
};

class IntVarPythonHelper : IntExprPythonHelper {
 public:
  static std::string name(IntVar* this_) { return this_->name(); }
  static int64_t Value(IntVar* this_) { return this_->Value(); }
  static void RemoveValue(IntVar* this_, int64_t v) {
    PROTECT_FROM_FAILURE(this_, RemoveValue(v));
  }
  static int64_t Size(IntVar* this_) { return this_->Size(); }
};

PYBIND11_MODULE(constraint_solver, m) {
  pybind11_protobuf::ImportNativeProtoCasters();

  pybind11::class_<Solver>(m, "Solver", DOC(operations_research, Solver))
      .def(pybind11::init<const std::string&>())
      .def(pybind11::init<const std::string&,
                          const ConstraintSolverParameters&>())
      .def("__str__", &Solver::DebugString)
      .def("default_solver_parameters", &Solver::DefaultSolverParameters)
      .def("parameters", &Solver::parameters)
      .def("local_search_profile", &Solver::LocalSearchProfile)
      .def("new_int_var",
           pybind11::overload_cast<int64_t, int64_t, const std::string&>(
               &Solver::MakeIntVar),
           DOC(operations_research, Solver, MakeIntVar),
           pybind11::return_value_policy::reference_internal)
      .def("new_int_var",
           pybind11::overload_cast<int64_t, int64_t>(&Solver::MakeIntVar),
           DOC(operations_research, Solver, MakeIntVar),
           pybind11::return_value_policy::reference_internal)
      .def("new_int_var",
           pybind11::overload_cast<const std::vector<int64_t>&,
                                   const std::string&>(&Solver::MakeIntVar),
           DOC(operations_research, Solver, MakeIntVar_2),
           pybind11::return_value_policy::reference_internal)
      .def("new_int_var",
           pybind11::overload_cast<const std::vector<int64_t>&>(
               &Solver::MakeIntVar),
           DOC(operations_research, Solver, MakeIntVar_2),
           pybind11::return_value_policy::reference_internal)
      .def("add", &Solver::AddConstraint,
           DOC(operations_research, Solver, AddConstraint), arg("c"))
      .def("accept", &Solver::Accept, DOC(operations_research, Solver, Accept),
           arg("visitor"))
      .def("print_model_visitor", &Solver::MakePrintModelVisitor,
           DOC(operations_research, Solver, MakePrintModelVisitor),
           pybind11::return_value_policy::reference_internal);

  pybind11::class_<BaseObject>(m, "BaseObject",
                               DOC(operations_research, BaseObject))
      .def("__str__", &BaseObjectPythonHelper::DebugString);

  pybind11::class_<PropagationBaseObject, BaseObject>(
      m, "PropagationBaseObject",
      DOC(operations_research, PropagationBaseObject))
      .def_property("name", &PropagationBaseObjectPythonHelper::name,
                    &PropagationBaseObjectPythonHelper::SetName);

  // Note: no ctor.
  pybind11::class_<IntExpr, PropagationBaseObject>(
      m, "IntExpr", DOC(operations_research, IntExpr))
      .def_property_readonly("min", &IntExprPythonHelper::Min,
                             DOC(operations_research, IntExpr, Min))
      .def_property_readonly("max", &IntExprPythonHelper::Max,
                             DOC(operations_research, IntExpr, Max))
      .def("set_min", &IntExprPythonHelper::SetMin,
           DOC(operations_research, IntExpr, SetMin), arg("m"))
      .def("set_max", &IntExprPythonHelper::SetMax,
           DOC(operations_research, IntExpr, SetMax), arg("m"))
      .def("set_range", &IntExprPythonHelper::SetRange,
           DOC(operations_research, IntExpr, SetRange), arg("mi"), arg("ma"))
      .def("set_value", &IntExprPythonHelper::SetValue,
           DOC(operations_research, IntExpr, SetValue), arg("v"))
      .def("bound", &IntExprPythonHelper::Bound,
           DOC(operations_research, IntExpr, Bound))
      .def(
          "__add__",
          [](IntExpr* e, int64_t arg) { return e->solver()->MakeSum(e, arg); },
          pybind11::return_value_policy::reference_internal)
      .def(
          "__add__",
          [](IntExpr* e, IntExpr* arg) { return e->solver()->MakeSum(e, arg); },
          pybind11::return_value_policy::reference_internal)
      .def(
          "__radd__",
          [](IntExpr* e, int64_t arg) { return e->solver()->MakeSum(e, arg); },
          pybind11::return_value_policy::reference_internal)
      .def(
          "__radd__",
          [](IntExpr* e, IntExpr* arg) { return e->solver()->MakeSum(e, arg); },
          pybind11::return_value_policy::reference_internal)
      .def(
          "__mul__",
          [](IntExpr* e, int64_t arg) { return e->solver()->MakeProd(e, arg); },
          pybind11::return_value_policy::reference_internal)
      .def(
          "__mul__",
          [](IntExpr* e, IntExpr* arg) {
            return e->solver()->MakeProd(e, arg);
          },
          pybind11::return_value_policy::reference_internal)
      .def(
          "__rmul__",
          [](IntExpr* e, int64_t arg) { return e->solver()->MakeProd(e, arg); },
          pybind11::return_value_policy::reference_internal)
      .def(
          "__rmul__",
          [](IntExpr* e, IntExpr* arg) {
            return e->solver()->MakeProd(e, arg);
          },
          pybind11::return_value_policy::reference_internal)
      .def(
          "__eq__",
          [](IntExpr* left, IntExpr* right) {
            return left->solver()->MakeEquality(left, right);
          },
          pybind11::return_value_policy::reference_internal)
      .def(
          "__eq__",
          [](IntExpr* left, int64_t right) {
            return left->solver()->MakeEquality(left, right);
          },
          pybind11::return_value_policy::reference_internal);

  // Note: no ctor.
  pybind11::class_<IntVar, IntExpr>(m, "IntVar",
                                    DOC(operations_research, IntVar))
      .def("value", &IntVarPythonHelper::Value,
           DOC(operations_research, IntVar, Value))
      .def("remove_value", &IntVarPythonHelper::RemoveValue,
           DOC(operations_research, IntVar, RemoveValue), arg("v"))
      .def("size", &IntVarPythonHelper::Size,
           DOC(operations_research, IntVar, Size));

  // Note: no ctor.
  pybind11::class_<Constraint>(m, "Constraint",
                               DOC(operations_research, Constraint))
      .def("var", &Constraint::Var, DOC(operations_research, Constraint, Var));

  // Note: no ctor.
  pybind11::class_<DecisionBuilder, BaseObject>(
      m, "DecisionBuilder", DOC(operations_research, DecisionBuilder))
      .def_property("name", &DecisionBuilder::GetName,
                    &DecisionBuilder::set_name);

  // Note: no ctor.
  pybind11::class_<ModelVisitor, BaseObject>(
      m, "ModelVisitor", DOC(operations_research, ModelVisitor));

  pybind11::class_<Assignment, PropagationBaseObject>(
      m, "Assignment", DOC(operations_research, Assignment))
      .def(pybind11::init<Solver*>())
      .def("clear", &Assignment::Clear)
      .def("empty", &Assignment::Empty)
      .def("size", &Assignment::Size)
      .def("num_int_vars", &Assignment::NumIntVars)
      .def("num_interval_vars", &Assignment::NumIntervalVars)
      .def("num_sequence_vars", &Assignment::NumSequenceVars)
      .def("store", &Assignment::Store)
      .def("restore", &Assignment::Restore)
      .def("load",
           pybind11::overload_cast<const std::string&>(&Assignment::Load),
           arg("filename"))
      .def("load",
           pybind11::overload_cast<const AssignmentProto&>(&Assignment::Load),
           arg("assignment_proto"))
      .def("add_objective", &Assignment::AddObjective, arg("v"))
      .def("add_objectives", &Assignment::AddObjectives, arg("vars"))
      .def("clear_objective", &Assignment::ClearObjective)
      .def("num_objectives", &Assignment::NumObjectives)
      .def("objective", &Assignment::Objective)
      .def("objective_from_index", &Assignment::ObjectiveFromIndex,
           arg("index"))
      .def("has_objective", &Assignment::HasObjective)
      .def("has_objective_from_index", &Assignment::HasObjectiveFromIndex,
           arg("index"))
      .def("objective_min", &Assignment::ObjectiveMin)
      .def("objective_max", &Assignment::ObjectiveMax)
      .def("objective_value", &Assignment::ObjectiveValue)
      .def("objective_bound", &Assignment::ObjectiveBound)
      .def("set_objective_min", &Assignment::SetObjectiveMin, arg("m"))
      .def("set_objective_max", &Assignment::SetObjectiveMax, arg("m"))
      .def("set_objective_value", &Assignment::SetObjectiveValue, arg("value"))
      .def("set_objective_range", &Assignment::SetObjectiveRange, arg("l"),
           arg("u"))
      .def("objective_min_from_index", &Assignment::ObjectiveMinFromIndex,
           arg("index"))
      .def("objective_max_from_index", &Assignment::ObjectiveMaxFromIndex,
           arg("index"))
      .def("objective_value_from_index", &Assignment::ObjectiveValueFromIndex,
           arg("index"))
      .def("objective_bound_from_index", &Assignment::ObjectiveBoundFromIndex,
           arg("index"))
      .def("set_objective_min_from_index",
           &Assignment::SetObjectiveMinFromIndex, arg("index"), arg("m"))
      .def("set_objective_max_from_index",
           &Assignment::SetObjectiveMaxFromIndex, arg("index"), arg("m"))
      .def("set_objective_range_from_index",
           &Assignment::SetObjectiveRangeFromIndex, arg("index"), arg("l"),
           arg("u"))
      .def("add", pybind11::overload_cast<IntVar*>(&Assignment::Add),
           arg("var"))
      .def("add",
           pybind11::overload_cast<const std::vector<IntVar*>&>(
               &Assignment::Add),
           arg("var"))
      .def("min", &Assignment::Min, arg("var"))
      .def("max", &Assignment::Max, arg("var"))
      .def("value", &Assignment::Value, arg("var"))
      .def("bound", &Assignment::Bound, arg("var"))
      .def("set_min", &Assignment::SetMin, arg("var"), arg("m"))
      .def("set_max", &Assignment::SetMax, arg("var"), arg("m"))
      .def("set_range", &Assignment::SetRange, arg("var"), arg("l"), arg("u"))
      .def("set_value", &Assignment::SetValue, arg("var"), arg("value"))
      .def("add", pybind11::overload_cast<IntervalVar*>(&Assignment::Add),
           arg("var"))
      .def("add",
           pybind11::overload_cast<const std::vector<IntervalVar*>&>(
               &Assignment::Add),
           arg("var"));
  // missing IntervalVar, SequenceVar, active/deactivate, contains, copy
}
