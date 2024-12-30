// Copyright 2010-2024 Google LLC
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

// This file wraps the swig_helper.h classes in python using pybind11.
#include "ortools/sat/swig_helper.h"

#include <Python.h>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/python/linear_expr.h"
#include "ortools/util/sorted_interval_list.h"
#include "pybind11/cast.h"
#include "pybind11/functional.h"
#include "pybind11/gil.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11_protobuf/native_proto_caster.h"

namespace py = pybind11;

namespace operations_research {
namespace sat {
namespace python {

using ::py::arg;

void throw_error(PyObject* py_exception, const std::string& message) {
  PyErr_SetString(py_exception, message.c_str());
  throw py::error_already_set();
}

// A trampoline class to override the OnSolutionCallback method to acquire the
// GIL.
class PySolutionCallback : public SolutionCallback {
 public:
  using SolutionCallback::SolutionCallback; /* Inherit constructors */

  void OnSolutionCallback() const override {
    ::py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE_PURE(
        void,               /* Return type */
        SolutionCallback,   /* Parent class */
        OnSolutionCallback, /* Name of function */
        /* This function has no arguments. The trailing comma
           in the previous line is needed for some compilers */
    );
  }
};

// A trampoline class to override the __str__ and __repr__ methods.
class PyBaseIntVar : public BaseIntVar {
 public:
  using BaseIntVar::BaseIntVar; /* Inherit constructors */

  std::string ToString() const override {
    PYBIND11_OVERRIDE_NAME(std::string,  // Return type (ret_type)
                           BaseIntVar,   // Parent class (cname)
                           "__str__",    // Name of method in Python (name)
                           ToString,     // Name of function in C++ (fn)
    );
  }

  std::string DebugString() const override {
    PYBIND11_OVERRIDE_NAME(std::string,  // Return type (ret_type)
                           BaseIntVar,   // Parent class (cname)
                           "__repr__",   // Name of method in Python (name)
                           DebugString,  // Name of function in C++ (fn)
    );
  }
};

// A class to wrap a C++ CpSolverResponse in a Python object, avoid the proto
// conversion back to python.
class ResponseWrapper {
 public:
  explicit ResponseWrapper(const CpSolverResponse& response)
      : response_(response) {}

  double BestObjectiveBound() const { return response_.best_objective_bound(); }

  bool BooleanValue(Literal* lit) const {
    const int index = lit->index();
    if (index >= 0) {
      return response_.solution(index) != 0;
    } else {
      return response_.solution(NegatedRef(index)) == 0;
    }
  }

  bool FixedBooleanValue(bool lit) const { return lit; }

  double DeterministicTime() const { return response_.deterministic_time(); }

  int64_t NumBinaryPropagations() const {
    return response_.num_binary_propagations();
  }

  int64_t NumBooleans() const { return response_.num_booleans(); }

  int64_t NumBranches() const { return response_.num_branches(); }

  int64_t NumConflicts() const { return response_.num_conflicts(); }

  int64_t NumIntegerPropagations() const {
    return response_.num_integer_propagations();
  }

  int64_t NumRestarts() const { return response_.num_restarts(); }

  double ObjectiveValue() const { return response_.objective_value(); }

  const CpSolverResponse& Response() const { return response_; }

  std::string ResponseStats() const {
    return CpSatHelper::SolverResponseStats(response_);
  }

  std::string SolutionInfo() const { return response_.solution_info(); }

  std::vector<int> SufficientAssumptionsForInfeasibility() const {
    return std::vector<int>(
        response_.sufficient_assumptions_for_infeasibility().begin(),
        response_.sufficient_assumptions_for_infeasibility().end());
  }

  CpSolverStatus Status() const { return response_.status(); }

  double UserTime() const { return response_.user_time(); }

  int64_t Value(LinearExpr* expr) const {
    IntExprVisitor visitor;
    int64_t value;
    if (!visitor.Evaluate(expr, response_, &value)) {
      throw_error(PyExc_TypeError,
                  absl::StrCat("Failed to evaluate linear expression: ",
                               expr->DebugString()));
    }
    return value;
  }

  int64_t FixedValue(int64_t value) const { return value; }

  double WallTime() const { return response_.wall_time(); }

 private:
  const CpSolverResponse response_;
};

const char* kLinearExprClassDoc = R"doc(
  Holds an integer linear expression.

  A linear expression is built from integer constants and variables.
  For example, `x + 2 * (y - z + 1)`.

  Linear expressions are used in CP-SAT models in constraints and in the
  objective:

  * You can define linear constraints as in:

  ```
  model.add(x + 2 * y <= 5)
  model.add(sum(array_of_vars) == 5)
  ```

  * In CP-SAT, the objective is a linear expression:

  ```
  model.minimize(x + 2 * y + z)
  ```

  * For large arrays, using the LinearExpr class is faster that using the python
  `sum()` function. You can create constraints and the objective from lists of
  linear expressions or coefficients as follows:

  ```
  model.minimize(cp_model.LinearExpr.sum(expressions))
  model.add(cp_model.LinearExpr.weighted_sum(expressions, coefficients) >= 0)
  ```)doc";

const char* kLiteralClassDoc = R"doc(
  Holds a Boolean literal.

  A literal is a Boolean variable or its negation.

  Literals are used in CP-SAT models in constraints and in the
  objective:

  * You can define literal as in:

  ```
  b1 = model.new_bool_var()
  b2 = model.new_bool_var()
  # Simple Boolean constraint.
  model.add_bool_or(b1, b2.negated())
  # We can use the ~ operator to negate a literal.
  model.add_bool_or(b1, ~b2)
  # Enforcement literals must be literals.
  x = model.new_int_var(0, 10, 'x')
  model.add(x == 5).only_enforced_if(~b1)
  ```

  * Literals can be used directly in linear constraints or in the objective:

  ```
  model.minimize(b1  + 2 * ~b2)
  ```)doc";

// Checks that the result is not null and throws an error if it is.
BoundedLinearExpression* CheckBoundedLinearExpression(
    BoundedLinearExpression* result, LinearExpr* lhs,
    LinearExpr* rhs = nullptr) {
  if (result == nullptr) {
    if (rhs == nullptr) {
      throw_error(PyExc_TypeError,
                  absl::StrCat("Linear constraints only accept integer values "
                               "and coefficients: ",
                               lhs->DebugString()));
    } else {
      throw_error(
          PyExc_TypeError,
          absl::StrCat("Linear constraints only accept integer values "
                       "and coefficients: ",
                       lhs->DebugString(), " and ", rhs->DebugString()));
    }
  }
  return result;
}

void RaiseIfNone(LinearExpr* expr) {
  if (expr == nullptr) {
    throw_error(PyExc_TypeError,
                "Linear constraints do not accept None as argument.");
  }
}

PYBIND11_MODULE(swig_helper, m) {
  pybind11_protobuf::ImportNativeProtoCasters();
  py::module::import("ortools.util.python.sorted_interval_list");

  // We keep the CamelCase name for the SolutionCallback class to be compatible
  // with the pre PEP8 python code.
  py::class_<SolutionCallback, PySolutionCallback>(m, "SolutionCallback")
      .def(py::init<>())
      .def("OnSolutionCallback", &SolutionCallback::OnSolutionCallback)
      .def("BestObjectiveBound", &SolutionCallback::BestObjectiveBound)
      .def("DeterministicTime", &SolutionCallback::DeterministicTime)
      .def("HasResponse", &SolutionCallback::HasResponse)
      .def("NumBinaryPropagations", &SolutionCallback::NumBinaryPropagations)
      .def("NumBooleans", &SolutionCallback::NumBooleans)
      .def("NumBranches", &SolutionCallback::NumBranches)
      .def("NumConflicts", &SolutionCallback::NumConflicts)
      .def("NumIntegerPropagations", &SolutionCallback::NumIntegerPropagations)
      .def("ObjectiveValue", &SolutionCallback::ObjectiveValue)
      .def("Response", &SolutionCallback::Response)
      .def("SolutionBooleanValue", &SolutionCallback::SolutionBooleanValue,
           arg("index"))
      .def("SolutionIntegerValue", &SolutionCallback::SolutionIntegerValue,
           arg("index"))
      .def("StopSearch", &SolutionCallback::StopSearch)
      .def("UserTime", &SolutionCallback::UserTime)
      .def("WallTime", &SolutionCallback::WallTime)
      .def(
          "Value",
          [](const SolutionCallback& callback, LinearExpr* expr) {
            IntExprVisitor visitor;
            int64_t value;
            if (!visitor.Evaluate(expr, callback.Response(), &value)) {
              throw_error(PyExc_TypeError,
                          absl::StrCat("Failed to evaluate linear expression: ",
                                       expr->DebugString()));
            }
            return value;
          },
          "Returns the value of a linear expression after solve.")
      .def(
          "Value", [](const SolutionCallback&, int64_t value) { return value; },
          "Returns the value of a linear expression after solve.")
      .def(
          "BooleanValue",
          [](const SolutionCallback& callback, Literal* lit) {
            return callback.SolutionBooleanValue(lit->index());
          },
          "Returns the Boolean value of a literal after solve.")
      .def(
          "BooleanValue", [](const SolutionCallback&, bool lit) { return lit; },
          "Returns the Boolean value of a literal after solve.");

  py::class_<ResponseWrapper>(m, "ResponseWrapper")
      .def(py::init<const CpSolverResponse&>())
      .def("best_objective_bound", &ResponseWrapper::BestObjectiveBound)
      .def("boolean_value", &ResponseWrapper::BooleanValue, arg("lit"))
      .def("boolean_value", &ResponseWrapper::FixedBooleanValue, arg("lit"))
      .def("deterministic_time", &ResponseWrapper::DeterministicTime)
      .def("num_binary_propagations", &ResponseWrapper::NumBinaryPropagations)
      .def("num_booleans", &ResponseWrapper::NumBooleans)
      .def("num_branches", &ResponseWrapper::NumBranches)
      .def("num_conflicts", &ResponseWrapper::NumConflicts)
      .def("num_integer_propagations", &ResponseWrapper::NumIntegerPropagations)
      .def("num_restarts", &ResponseWrapper::NumRestarts)
      .def("objective_value", &ResponseWrapper::ObjectiveValue)
      .def("response", &ResponseWrapper::Response)
      .def("response_stats", &ResponseWrapper::ResponseStats)
      .def("solution_info", &ResponseWrapper::SolutionInfo)
      .def("status", &ResponseWrapper::Status)
      .def("sufficient_assumptions_for_infeasibility",
           &ResponseWrapper::SufficientAssumptionsForInfeasibility)
      .def("user_time", &ResponseWrapper::UserTime)
      .def("value", &ResponseWrapper::Value, arg("expr"))
      .def("value", &ResponseWrapper::FixedValue, arg("value"))
      .def("wall_time", &ResponseWrapper::WallTime);

  py::class_<SolveWrapper>(m, "SolveWrapper")
      .def(py::init<>())
      .def("add_log_callback", &SolveWrapper::AddLogCallback,
           arg("log_callback"))
      .def("add_solution_callback", &SolveWrapper::AddSolutionCallback,
           arg("callback"))
      .def("clear_solution_callback", &SolveWrapper::ClearSolutionCallback)
      .def("add_best_bound_callback", &SolveWrapper::AddBestBoundCallback,
           arg("best_bound_callback"))
      .def("set_parameters", &SolveWrapper::SetParameters, arg("parameters"))
      .def("solve",
           [](SolveWrapper* solve_wrapper,
              const CpModelProto& model_proto) -> CpSolverResponse {
             ::pybind11::gil_scoped_release release;
             return solve_wrapper->Solve(model_proto);
           })
      .def("solve_and_return_response_wrapper",
           [](SolveWrapper* solve_wrapper,
              const CpModelProto& model_proto) -> ResponseWrapper {
             ::py::gil_scoped_release release;
             return ResponseWrapper(solve_wrapper->Solve(model_proto));
           })
      .def("stop_search", &SolveWrapper::StopSearch);

  py::class_<CpSatHelper>(m, "CpSatHelper")
      .def_static("model_stats", &CpSatHelper::ModelStats, arg("model_proto"))
      .def_static("solver_response_stats", &CpSatHelper::SolverResponseStats,
                  arg("response"))
      .def_static("validate_model", &CpSatHelper::ValidateModel,
                  arg("model_proto"))
      .def_static("variable_domain", &CpSatHelper::VariableDomain,
                  arg("variable_proto"))
      .def_static("write_model_to_file", &CpSatHelper::WriteModelToFile,
                  arg("model_proto"), arg("filename"));

  py::class_<ExprOrValue>(m, "ExprOrValue")
      .def(py::init<int64_t>())  // Needs to be before the double init.
      .def(py::init<double>())
      .def(py::init<LinearExpr*>())
      .def_readonly("double_value", &ExprOrValue::double_value)
      .def_readonly("expr", &ExprOrValue::expr)
      .def_readonly("int_value", &ExprOrValue::int_value);

  py::implicitly_convertible<int64_t, ExprOrValue>();
  py::implicitly_convertible<double, ExprOrValue>();
  py::implicitly_convertible<LinearExpr*, ExprOrValue>();

  py::class_<LinearExpr>(m, "LinearExpr", kLinearExprClassDoc)
      // We make sure to keep the order of the overloads: LinearExpr* before
      // ExprOrValue as this is faster to parse and type check.
      .def_static("sum", (&LinearExpr::Sum), arg("exprs"),
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("sum", &LinearExpr::MixedSum, arg("exprs"),
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("weighted_sum", &LinearExpr::WeightedSumInt, arg("exprs"),
                  arg("coeffs"), py::return_value_policy::automatic,
                  py::keep_alive<0, 1>())
      .def_static("weighted_sum", &LinearExpr::WeightedSumDouble, arg("exprs"),
                  arg("coeffs"), py::return_value_policy::automatic,
                  py::keep_alive<0, 1>())
      .def_static("weighted_sum", &LinearExpr::MixedWeightedSumInt,
                  arg("exprs"), arg("coeffs"),
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("weighted_sum", &LinearExpr::MixedWeightedSumDouble,
                  arg("exprs"), arg("coeffs"),
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      // Make sure to keep the order of the overloads: int before float as an
      // an integer value will be silently converted to a float.
      .def_static("term", &LinearExpr::TermInt, arg("expr").none(false),
                  arg("coeff"), "Returns expr * coeff.",
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("term", &LinearExpr::TermDouble, arg("expr").none(false),
                  arg("coeff"), "Returns expr * coeff.",
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("affine", &LinearExpr::AffineInt, arg("expr").none(false),
                  arg("coeff"), arg("offset"), "Returns expr * coeff + offset.",
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("affine", &LinearExpr::AffineDouble, arg("expr").none(false),
                  arg("coeff"), arg("offset"), "Returns expr * coeff + offset.",
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("constant", &LinearExpr::ConstantInt, arg("value"),
                  "Returns a constant linear expression.",
                  py::return_value_policy::automatic)
      .def_static("constant", &LinearExpr::ConstantDouble, arg("value"),
                  "Returns a constant linear expression.",
                  py::return_value_policy::automatic)
      // Pre PEP8 compatibility layer.
      .def_static("Sum", &LinearExpr::Sum, arg("exprs"),
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("Sum", &LinearExpr::MixedSum, arg("exprs"),
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("WeightedSum", &LinearExpr::MixedWeightedSumInt, arg("exprs"),
                  arg("coeffs"), py::return_value_policy::automatic,
                  py::keep_alive<0, 1>())
      .def_static("WeightedSum", &LinearExpr::MixedWeightedSumDouble,
                  arg("exprs"), arg("coeffs"),
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("Term", &LinearExpr::TermInt, arg("expr").none(false),
                  arg("coeff"), "Returns expr * coeff.",
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("Term", &LinearExpr::TermDouble, arg("expr").none(false),
                  arg("coeff"), "Returns expr * coeff.",
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      // Methods.
      .def("__str__", &LinearExpr::ToString)
      .def("__repr__", &LinearExpr::DebugString)
      .def("is_integer", &LinearExpr::IsInteger)
      // Operators.
      // Note that we keep the 3 APIS (expr, int, double) instead of using an
      // ExprOrValue argument as this is more efficient.
      .def("__add__", &LinearExpr::Add, arg("other").none(false),
           py::return_value_policy::automatic, py::keep_alive<0, 1>(),
           py::keep_alive<0, 2>())
      .def("__add__", &LinearExpr::AddInt, arg("cst"),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__add__", &LinearExpr::AddDouble, arg("cst"),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__radd__", &LinearExpr::AddInt, arg("cst"),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__radd__", &LinearExpr::AddDouble, arg("cst"),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__sub__", &LinearExpr::Sub, arg("other").none(false),
           py::return_value_policy::automatic, py::keep_alive<0, 1>(),
           py::keep_alive<0, 2>())
      .def("__sub__", &LinearExpr::SubInt, arg("cst"),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__sub__", &LinearExpr::SubDouble, arg("cst"),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__rsub__", &LinearExpr::RSubInt, arg("cst"),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__rsub__", &LinearExpr::RSubDouble, arg("cst"),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__mul__", &LinearExpr::MulInt, arg("cst"),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__mul__", &LinearExpr::MulDouble, arg("cst"),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__rmul__", &LinearExpr::MulInt, arg("cst"),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__rmul__", &LinearExpr::MulDouble, arg("cst"),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__neg__", &LinearExpr::Neg, py::return_value_policy::automatic,
           py::keep_alive<0, 1>())
      .def(
          "__eq__",
          [](LinearExpr* lhs, LinearExpr* rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Eq(rhs), lhs, rhs);
          },
          py::return_value_policy::automatic, py::keep_alive<0, 1>(),
          py::keep_alive<0, 2>())
      .def(
          "__eq__",
          [](LinearExpr* lhs, int64_t rhs) {
            if (rhs == std::numeric_limits<int64_t>::max() ||
                rhs == std::numeric_limits<int64_t>::min()) {
              throw_error(PyExc_ArithmeticError,
                          "== INT_MIN or INT_MAX is not supported");
            }
            return CheckBoundedLinearExpression(lhs->EqCst(rhs), lhs);
          },
          py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def(
          "__ne__",
          [](LinearExpr* lhs, LinearExpr* rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Ne(rhs), lhs, rhs);
          },
          py::return_value_policy::automatic, py::keep_alive<0, 1>(),
          py::keep_alive<0, 2>())
      .def(
          "__ne__",
          [](LinearExpr* lhs, int64_t rhs) {
            return CheckBoundedLinearExpression(lhs->NeCst(rhs), lhs);
          },
          py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def(
          "__le__",
          [](LinearExpr* lhs, LinearExpr* rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Le(rhs), lhs, rhs);
          },
          py::return_value_policy::automatic, py::keep_alive<0, 1>(),
          py::keep_alive<0, 2>())
      .def(
          "__le__",
          [](LinearExpr* lhs, int64_t rhs) {
            if (rhs == std::numeric_limits<int64_t>::min()) {
              throw_error(PyExc_ArithmeticError, "<= INT_MIN is not supported");
            }
            return CheckBoundedLinearExpression(lhs->LeCst(rhs), lhs);
          },
          py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def(
          "__lt__",
          [](LinearExpr* lhs, LinearExpr* rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Lt(rhs), lhs, rhs);
          },
          py::return_value_policy::automatic, py::keep_alive<0, 1>(),
          py::keep_alive<0, 2>())
      .def(
          "__lt__",
          [](LinearExpr* lhs, int64_t rhs) {
            if (rhs == std::numeric_limits<int64_t>::min()) {
              throw_error(PyExc_ArithmeticError, "< INT_MIN is not supported");
            }
            return CheckBoundedLinearExpression(lhs->LtCst(rhs), lhs);
          },
          py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def(
          "__ge__",
          [](LinearExpr* lhs, LinearExpr* rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Ge(rhs), lhs, rhs);
          },
          py::return_value_policy::automatic, py::keep_alive<0, 1>(),
          py::keep_alive<0, 2>())
      .def(
          "__ge__",
          [](LinearExpr* lhs, int64_t rhs) {
            if (rhs == std::numeric_limits<int64_t>::max()) {
              throw_error(PyExc_ArithmeticError, ">= INT_MAX is not supported");
            }
            return CheckBoundedLinearExpression(lhs->GeCst(rhs), lhs);
          },
          py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def(
          "__gt__",
          [](LinearExpr* lhs, LinearExpr* rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Gt(rhs), lhs, rhs);
          },
          py::return_value_policy::automatic, py::keep_alive<0, 1>(),
          py::keep_alive<0, 2>())
      .def(
          "__gt__",
          [](LinearExpr* lhs, int64_t rhs) {
            if (rhs == std::numeric_limits<int64_t>::max()) {
              throw_error(PyExc_ArithmeticError, "> INT_MAX is not supported");
            }
            return CheckBoundedLinearExpression(lhs->GtCst(rhs), lhs);
          },
          py::return_value_policy::automatic, py::keep_alive<0, 1>())
      // Disable other operators as they are not supported.
      .def("__div__",
           [](LinearExpr* /*self*/, ExprOrValue /*other*/) {
             throw_error(PyExc_NotImplementedError,
                         "calling / on a linear expression is not supported, "
                         "please use CpModel.add_division_equality");
           })
      .def("__truediv__",
           [](LinearExpr* /*self*/, ExprOrValue /*other*/) {
             throw_error(PyExc_NotImplementedError,
                         "calling // on a linear expression is not supported, "
                         "please use CpModel.add_division_equality");
           })
      .def("__mod__",
           [](LinearExpr* /*self*/, ExprOrValue /*other*/) {
             throw_error(PyExc_NotImplementedError,
                         "calling %% on a linear expression is not supported, "
                         "please use CpModel.add_modulo_equality");
           })
      .def("__pow__",
           [](LinearExpr* /*self*/, ExprOrValue /*other*/) {
             throw_error(PyExc_NotImplementedError,
                         "calling ** on a linear expression is not supported, "
                         "please use CpModel.add_multiplication_equality");
           })
      .def("__lshift__",
           [](LinearExpr* /*self*/, ExprOrValue /*other*/) {
             throw_error(
                 PyExc_NotImplementedError,
                 "calling left shift on a linear expression is not supported");
           })
      .def("__rshift__",
           [](LinearExpr* /*self*/, ExprOrValue /*other*/) {
             throw_error(
                 PyExc_NotImplementedError,
                 "calling right shift on a linear expression is not supported");
           })
      .def("__and__",
           [](LinearExpr* /*self*/, ExprOrValue /*other*/) {
             throw_error(PyExc_NotImplementedError,
                         "calling and on a linear expression is not supported");
           })
      .def("__or__",
           [](LinearExpr* /*self*/, ExprOrValue /*other*/) {
             throw_error(PyExc_NotImplementedError,
                         "calling or on a linear expression is not supported");
           })
      .def("__xor__",
           [](LinearExpr* /*self*/, ExprOrValue /*other*/) {
             throw_error(PyExc_NotImplementedError,
                         "calling xor on a linear expression is not supported");
           })
      .def("__abs__",
           [](LinearExpr* /*self*/) {
             throw_error(
                 PyExc_NotImplementedError,
                 "calling abs() on a linear expression is not supported, "
                 "please use CpModel.add_abs_equality");
           })
      .def("__bool__", [](LinearExpr* /*self*/) {
        throw_error(PyExc_NotImplementedError,
                    "Evaluating a LinearExpr instance as a Boolean is "
                    "not supported.");
      });

  // Expose Internal classes, mostly for testing.
  py::class_<CanonicalFloatExpression>(m, "CanonicalFloatExpression")
      .def(py::init<LinearExpr*>())
      .def_property_readonly("vars", &CanonicalFloatExpression::vars)
      .def_property_readonly("coeffs", &CanonicalFloatExpression::coeffs)
      .def_property_readonly("offset", &CanonicalFloatExpression::offset);

  py::class_<CanonicalIntExpression>(m, "CanonicalIntExpression")
      .def(py::init<LinearExpr*>())
      .def_property_readonly("vars", &CanonicalIntExpression::vars)
      .def_property_readonly("coeffs", &CanonicalIntExpression::coeffs)
      .def_property_readonly("offset", &CanonicalIntExpression::offset)
      .def_property_readonly("ok", &CanonicalIntExpression::ok);

  py::class_<FloatAffine, LinearExpr>(m, "FloatAffine")
      .def(py::init<LinearExpr*, double, double>())
      .def_property_readonly("expression", &FloatAffine::expression)
      .def_property_readonly("coefficient", &FloatAffine::coefficient)
      .def_property_readonly("offset", &FloatAffine::offset);

  py::class_<IntAffine, LinearExpr>(m, "IntAffine")
      .def(py::init<LinearExpr*, int64_t, int64_t>())
      .def_property_readonly("expression", &IntAffine::expression)
      .def_property_readonly("coefficient", &IntAffine::coefficient)
      .def_property_readonly("offset", &IntAffine::offset);

  py::class_<Literal, LinearExpr>(m, "Literal", kLiteralClassDoc)
      .def_property_readonly("index", &Literal::index,
                             "The index of the literal in the model.")
      .def("negated", &Literal::negated,
           R"doc(
    Returns the negation of a literal (a Boolean variable or its negation).

    This method implements the logical negation of a Boolean variable.
    It is only valid if the variable has a Boolean domain (0 or 1).

    Note that this method is nilpotent: `x.negated().negated() == x`.          
          )doc")
      .def("__invert__", &Literal::negated,
           "Returns the negation of the current literal.")
      .def("__bool__",
           [](Literal* /*self*/) {
             throw_error(PyExc_NotImplementedError,
                         "Evaluating a Literal as a Boolean valueis "
                         "not supported.");
           })
      // PEP8 Compatibility.
      .def("Not", &Literal::negated)
      .def("Index", &Literal::index);

  // Memory management:
  // - The BaseIntVar owns the NotBooleanVariable.
  // - The NotBooleanVariable is created at the same time as the base variable
  //   when the variable is Boolean, and is deleted when the base variable is
  //   deleted.
  // - The negated() methods return an internal reference to the negated
  //   object. That means memory of the negated variable is onwed by the C++
  //   layer, but a reference is kept in python to link the lifetime of the
  //   negated variable to the base variable.
  py::class_<BaseIntVar, PyBaseIntVar, Literal>(m, "BaseIntVar")
      .def(py::init<int>())        // Integer variable.
      .def(py::init<int, bool>())  // Potential Boolean variable.
      .def_property_readonly("index", &BaseIntVar::index,
                             "The index of the variable in the model.")
      .def_property_readonly("is_boolean", &BaseIntVar::is_boolean,
                             "Whether the variable is Boolean.")
      .def("__str__", &BaseIntVar::ToString)
      .def("__repr__", &BaseIntVar::DebugString)
      .def(
          "negated",
          [](BaseIntVar* self) {
            if (!self->is_boolean()) {
              throw_error(PyExc_TypeError,
                          "negated() is only supported for Boolean variables.");
            }
            return self->negated();
          },
          "Returns the negation of the current Boolean variable.",
          py::return_value_policy::reference_internal)
      .def(
          "__invert__",
          [](BaseIntVar* self) {
            if (!self->is_boolean()) {
              throw_error(PyExc_TypeError,
                          "negated() is only supported for Boolean variables.");
            }
            return self->negated();
          },
          "Returns the negation of the current Boolean variable.",
          py::return_value_policy::reference_internal)
      // PEP8 Compatibility.
      .def(
          "Not",
          [](BaseIntVar* self) {
            if (!self->is_boolean()) {
              throw_error(PyExc_TypeError,
                          "negated() is only supported for Boolean variables.");
            }
            return self->negated();
          },
          py::return_value_policy::reference_internal);

  // Memory management:
  // - Do we need a reference_internal (that add a py::keep_alive<1, 0>() rule)
  //   or just a reference ?
  py::class_<NotBooleanVariable, Literal>(m, "NotBooleanVariable")
      .def(py::init<BaseIntVar*>())
      .def_property_readonly("index", &NotBooleanVariable::index,
                             "The index of the variable in the model.")
      .def("__str__", &NotBooleanVariable::ToString)
      .def("__repr__", &NotBooleanVariable::DebugString)
      .def("negated", &NotBooleanVariable::negated,
           "Returns the negation of the current Boolean variable.",
           py::return_value_policy::reference_internal)
      .def("__invert__", &NotBooleanVariable::negated,
           "Returns the negation of the current Boolean variable.",
           py::return_value_policy::reference_internal)
      .def("Not", &NotBooleanVariable::negated,
           "Returns the negation of the current Boolean variable.",
           py::return_value_policy::reference_internal);

  py::class_<BoundedLinearExpression>(m, "BoundedLinearExpression")
      .def(py::init<std::vector<BaseIntVar*>, std::vector<int64_t>, int64_t,
                    Domain>())
      .def_property_readonly("bounds", &BoundedLinearExpression::bounds)
      .def_property_readonly("vars", &BoundedLinearExpression::vars)
      .def_property_readonly("coeffs", &BoundedLinearExpression::coeffs)
      .def_property_readonly("offset", &BoundedLinearExpression::offset)
      .def("__str__", &BoundedLinearExpression::ToString)
      .def("__repr__", &BoundedLinearExpression::DebugString)
      .def("__bool__", [](const BoundedLinearExpression& self) {
        bool result;
        if (self.CastToBool(&result)) return result;
        throw_error(PyExc_NotImplementedError,
                    absl::StrCat("Evaluating a BoundedLinearExpression '",
                                 self.ToString(),
                                 "'instance as a Boolean is "
                                 "not supported.")
                        .c_str());
        return false;
      });
}  // NOLINT(readability/fn_size)

}  // namespace python
}  // namespace sat
}  // namespace operations_research
