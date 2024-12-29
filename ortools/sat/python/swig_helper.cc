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
      return response_.solution(-index - 1) == 0;
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

  int64_t Value(IntLinExpr* expr) const {
    IntExprVisitor visitor;
    return visitor.Evaluate(expr, response_);
  }

  int64_t FixedValue(int64_t value) const { return value; }

  double WallTime() const { return response_.wall_time(); }

 private:
  const CpSolverResponse response_;
};

void throw_error(PyObject* py_exception, const std::string& message) {
  PyErr_SetString(py_exception, message.c_str());
  throw py::error_already_set();
}

const char* kIntLinExprClassDoc = R"doc(
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

PYBIND11_MODULE(swig_helper, m) {
  pybind11_protobuf::ImportNativeProtoCasters();
  py::module::import("ortools.util.python.sorted_interval_list");

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
          [](const SolutionCallback& callback, IntLinExpr* expr) {
            IntExprVisitor visitor;
            return visitor.Evaluate(expr, callback.Response());
          },
          "Returns the value of a linear expression after solve.")
      .def(
          "Value", [](const SolutionCallback&, int64_t value) { return value; },
          "Returns the value of a linear expression after solve.")
      .def(
          "BooleanValue",
          [](const SolutionCallback& callback, Literal* lit) {
            const int index = lit->index();
            if (index >= 0) {
              return callback.Response().solution(index) != 0;
            } else {
              return callback.Response().solution(-index - 1) == 0;
            }
          },
          "Returns the boolean value of a literal after solve.")
      .def(
          "BooleanValue", [](const SolutionCallback&, bool lit) { return lit; },
          "Returns the boolean value of a literal after solve.");

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

  py::class_<FloatExprOrValue>(m, "FloatExprOrValue")
      .def(py::init<FloatLinearExpr*>())
      .def(py::init<double>())
      .def_readonly("value", &FloatExprOrValue::value)
      .def_readonly("expr", &FloatExprOrValue::expr);

  py::implicitly_convertible<FloatLinearExpr*, FloatExprOrValue>();
  py::implicitly_convertible<double, FloatExprOrValue>();

  py::class_<FloatLinearExpr>(m, "FloatLinearExpr")
      .def(py::init<>())
      .def_static("sum",
                  py::overload_cast<const std::vector<FloatExprOrValue>&>(
                      &FloatLinearExpr::Sum),
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static(
          "sum",
          py::overload_cast<const std::vector<FloatExprOrValue>&, double>(
              &FloatLinearExpr::Sum),
          py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("weighted_sum",
                  py::overload_cast<const std::vector<FloatExprOrValue>&,
                                    const std::vector<double>&>(
                      &FloatLinearExpr::WeightedSum),
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("weighted_sum",
                  py::overload_cast<const std::vector<FloatExprOrValue>&,
                                    const std::vector<double>&, double>(
                      &FloatLinearExpr::WeightedSum),
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("WeightedSum",
                  py::overload_cast<const std::vector<FloatExprOrValue>&,
                                    const std::vector<double>&>(
                      &FloatLinearExpr::WeightedSum),
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("term", &FloatLinearExpr::Term, arg("expr"), arg("coeff"),
                  "Returns expr * coeff.", py::return_value_policy::automatic,
                  py::keep_alive<0, 1>())
      .def_static("affine", &FloatLinearExpr::Affine, arg("expr"), arg("coeff"),
                  arg("offset"), "Returns expr * coeff + offset.",
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("constant", FloatLinearExpr::Constant, arg("value"),
                  "Returns a constant linear expression.",
                  py::return_value_policy::automatic)
      .def("__str__", &FloatLinearExpr::ToString)
      .def("__repr__", &FloatLinearExpr::DebugString)
      .def("is_integer", &FloatLinearExpr::is_integer)
      .def("__add__", &FloatLinearExpr::FloatAddCst,
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__add__", &FloatLinearExpr::FloatAdd,
           py::return_value_policy::automatic, py::keep_alive<0, 1>(),
           py::keep_alive<0, 2>())
      .def("__radd__", &FloatLinearExpr::FloatAddCst,
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__radd__", &FloatLinearExpr::FloatAdd,
           py::return_value_policy::automatic, py::keep_alive<0, 1>(),
           py::keep_alive<0, 2>())
      .def("__sub__", &FloatLinearExpr::FloatSub,
           py::return_value_policy::automatic, py::keep_alive<0, 1>(),
           py::keep_alive<0, 2>())
      .def("__sub__", &FloatLinearExpr::FloatSubCst,
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__rsub__", &FloatLinearExpr::FloatRSub,
           py::return_value_policy::automatic, py::keep_alive<0, 1>(),
           py::keep_alive<0, 2>())
      .def("__rsub__", &FloatLinearExpr::FloatRSubCst,
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__mul__", &FloatLinearExpr::FloatMulCst,
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__rmul__", &FloatLinearExpr::FloatMulCst,
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__neg__", &FloatLinearExpr::FloatNeg,
           py::return_value_policy::automatic, py::keep_alive<0, 1>());

  py::class_<FloatAffine, FloatLinearExpr>(m, "FloatAffine")
      .def(py::init<FloatLinearExpr*, double, double>())
      .def_property_readonly("expression", &FloatAffine::expression)
      .def_property_readonly("coefficient", &FloatAffine::coefficient)
      .def_property_readonly("offset", &FloatAffine::offset);

  py::class_<CanonicalFloatExpression>(m, "CanonicalFloatExpression")
      .def(py::init<FloatLinearExpr*>())
      .def_property_readonly("vars", &CanonicalFloatExpression::vars)
      .def_property_readonly("coeffs", &CanonicalFloatExpression::coeffs)
      .def_property_readonly("offset", &CanonicalFloatExpression::offset);

  py::class_<IntExprOrValue>(m, "IntExprOrValue")
      .def(py::init<IntLinExpr*>())
      .def(py::init<int64_t>())
      .def_readonly("value", &IntExprOrValue::value)
      .def_readonly("expr", &IntExprOrValue::expr);

  py::implicitly_convertible<IntLinExpr*, IntExprOrValue>();
  py::implicitly_convertible<int64_t, IntExprOrValue>();

  py::class_<IntLinExpr, FloatLinearExpr>(m, "LinearExpr", kIntLinExprClassDoc)
      .def(py::init<>())
      .def_static("sum",
                  py::overload_cast<const std::vector<IntExprOrValue>&>(
                      &IntLinExpr::Sum),
                  "Returns sum(exprs)", py::return_value_policy::automatic,
                  py::keep_alive<0, 1>())
      .def_static(
          "sum",
          py::overload_cast<const std::vector<IntExprOrValue>&, int64_t>(
              &IntLinExpr::Sum),
          "Returns sum(exprs) + cst", py::return_value_policy::automatic,
          py::keep_alive<0, 1>())
      .def_static("weighted_sum",
                  py::overload_cast<const std::vector<IntExprOrValue>&,
                                    const std::vector<int64_t>&>(
                      &IntLinExpr::WeightedSum),
                  "Returns sum(exprs[i] * coeffs[i]",
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("weighted_sum",
                  py::overload_cast<const std::vector<IntExprOrValue>&,
                                    const std::vector<int64_t>&, int64_t>(
                      &IntLinExpr::WeightedSum),
                  "Returns sum(exprs[i] * coeffs[i]) + cst",
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("weighted_sum",
                  py::overload_cast<const std::vector<FloatExprOrValue>&,
                                    const std::vector<double>&>(
                      &FloatLinearExpr::WeightedSum),
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("term", &IntLinExpr::Term, arg("expr"), arg("coeff"),
                  "Returns expr * coeff.", py::return_value_policy::automatic,
                  py::keep_alive<0, 1>())
      .def_static("affine", &IntLinExpr::Affine, arg("expr"), arg("coeff"),
                  arg("offset"), "Returns expr * coeff.",
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("constant", IntLinExpr::Constant, arg("value"),
                  "Returns a constant linear expression.",
                  py::return_value_policy::automatic)
      .def("is_integer", &IntLinExpr::is_integer)
      .def("__add__", &IntLinExpr::IntAddCst,
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__add__", &FloatLinearExpr::FloatAddCst,
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__add__", &IntLinExpr::IntAdd, py::return_value_policy::automatic,
           py::keep_alive<0, 1>(), py::keep_alive<0, 2>())
      .def("__add__", &FloatLinearExpr::FloatAdd,
           py::return_value_policy::automatic, py::keep_alive<0, 1>(),
           py::keep_alive<0, 2>())
      .def("__radd__", &IntLinExpr::IntAddCst,
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__radd__", &FloatLinearExpr::FloatAddCst,
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__radd__", &FloatLinearExpr::FloatAdd,
           py::return_value_policy::automatic, py::keep_alive<0, 1>(),
           py::keep_alive<0, 2>())
      .def("__sub__", &IntLinExpr::IntSubCst,
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__sub__", &FloatLinearExpr::FloatSubCst,
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__sub__", &FloatLinearExpr::FloatSubCst,
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__sub__", &IntLinExpr::IntSub, py::return_value_policy::automatic,
           py::keep_alive<0, 1>(), py::keep_alive<0, 2>())
      .def("__sub__", &FloatLinearExpr::FloatSub,
           py::return_value_policy::automatic, py::keep_alive<0, 1>(),
           py::keep_alive<0, 2>())
      .def("__rsub__", &IntLinExpr::IntRSubCst,
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__rsub__", &FloatLinearExpr::FloatRSubCst,
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__rsub__", &FloatLinearExpr::FloatRSub,
           py::return_value_policy::automatic, py::keep_alive<0, 1>(),
           py::keep_alive<0, 2>())
      .def("__mul__", &IntLinExpr::IntMulCst,
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__rmul__", &IntLinExpr::IntMulCst,
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__neg__", &IntLinExpr::IntNeg, py::return_value_policy::automatic,
           py::keep_alive<0, 1>())
      .def("__mul__", &FloatLinearExpr::FloatMulCst,
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__rmul__", &FloatLinearExpr::FloatMulCst,
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__eq__", &IntLinExpr::Eq, py::return_value_policy::automatic,
           py::keep_alive<0, 1>(), py::keep_alive<0, 2>())
      .def("__eq__", &IntLinExpr::EqCst, py::return_value_policy::automatic,
           py::keep_alive<0, 1>())
      .def("__ne__", &IntLinExpr::Ne, py::return_value_policy::automatic,
           py::keep_alive<0, 1>(), py::keep_alive<0, 2>())
      .def("__ne__", &IntLinExpr::NeCst, py::return_value_policy::automatic,
           py::keep_alive<0, 1>())
      .def("__lt__", &IntLinExpr::Lt, py::return_value_policy::automatic,
           py::keep_alive<0, 1>(), py::keep_alive<0, 2>())
      .def(
          "__lt__",
          [](IntLinExpr* expr, int64_t bound) {
            if (bound == std::numeric_limits<int64_t>::min()) {
              throw_error(PyExc_ArithmeticError, "< INT_MIN is not supported");
            }
            return expr->LtCst(bound);
          },
          py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__le__", &IntLinExpr::Le, py::return_value_policy::automatic,
           py::keep_alive<0, 1>(), py::keep_alive<0, 2>())
      .def(
          "__le__",
          [](IntLinExpr* expr, int64_t bound) {
            if (bound == std::numeric_limits<int64_t>::min()) {
              throw_error(PyExc_ArithmeticError, "<= INT_MIN is not supported");
            }
            return expr->LeCst(bound);
          },
          py::return_value_policy::automatic,

          py::keep_alive<0, 1>())
      .def("__gt__", &IntLinExpr::Gt, py::return_value_policy::automatic,
           py::keep_alive<0, 1>(), py::keep_alive<0, 2>())
      .def(
          "__gt__",
          [](IntLinExpr* expr, int64_t bound) {
            if (bound == std::numeric_limits<int64_t>::max()) {
              throw_error(PyExc_ArithmeticError, "> INT_MAX is not supported");
            }
            return expr->GtCst(bound);
          },
          py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__ge__", &IntLinExpr::Ge, py::return_value_policy::automatic,
           py::keep_alive<0, 1>(), py::keep_alive<0, 2>())
      .def(
          "__ge__",
          [](IntLinExpr* expr, int64_t bound) {
            if (bound == std::numeric_limits<int64_t>::max()) {
              throw_error(PyExc_ArithmeticError, ">= INT_MAX is not supported");
            }
            return expr->GeCst(bound);
          },
          py::return_value_policy::automatic, py::keep_alive<0, 1>())

      .def("__div__",
           [](IntLinExpr* /*self*/, IntLinExpr* /*other*/) {
             throw_error(PyExc_NotImplementedError,
                         "calling / on a linear expression is not supported, "
                         "please use CpModel.add_division_equality");
           })
      .def("__div__",
           [](IntLinExpr* /*self*/, int64_t /*cst*/) {
             throw_error(PyExc_NotImplementedError,
                         "calling / on a linear expression is not supported, "
                         "please use CpModel.add_division_equality");
           })
      .def("__truediv__",
           [](IntLinExpr* /*self*/, IntLinExpr* /*other*/) {
             throw_error(PyExc_NotImplementedError,
                         "calling // on a linear expression is not supported, "
                         "please use CpModel.add_division_equality");
           })
      .def("__truediv__",
           [](IntLinExpr* /*self*/, int64_t /*cst*/) {
             throw_error(PyExc_NotImplementedError,
                         "calling // on a linear expression is not supported, "
                         "please use CpModel.add_division_equality");
           })
      .def("__mod__",
           [](IntLinExpr* /*self*/, IntLinExpr* /*other*/) {
             throw_error(PyExc_NotImplementedError,
                         "calling %% on a linear expression is not supported, "
                         "please use CpModel.add_modulo_equality");
           })
      .def("__mod__",
           [](IntLinExpr* /*self*/, int64_t /*cst*/) {
             throw_error(PyExc_NotImplementedError,
                         "calling %% on a linear expression is not supported, "
                         "please use CpModel.add_modulo_equality");
           })
      .def("__pow__",
           [](IntLinExpr* /*self*/, IntLinExpr* /*other*/) {
             throw_error(PyExc_NotImplementedError,
                         "calling ** on a linear expression is not supported, "
                         "please use CpModel.add_multiplication_equality");
           })
      .def("__pow__",
           [](IntLinExpr* /*self*/, int64_t /*cst*/) {
             throw_error(PyExc_NotImplementedError,
                         "calling ** on a linear expression is not supported, "
                         "please use CpModel.add_multiplication_equality");
           })
      .def("__lshift__",
           [](IntLinExpr* /*self*/, IntLinExpr* /*other*/) {
             throw_error(
                 PyExc_NotImplementedError,
                 "calling left shift on a linear expression is not supported");
           })
      .def("__lshift__",
           [](IntLinExpr* /*self*/, int64_t /*cst*/) {
             throw_error(
                 PyExc_NotImplementedError,
                 "calling left shift on a linear expression is not supported");
           })
      .def("__rshift__",
           [](IntLinExpr* /*self*/, IntLinExpr* /*other*/) {
             throw_error(
                 PyExc_NotImplementedError,
                 "calling right shift on a linear expression is not supported");
           })
      .def("__rshift__",
           [](IntLinExpr* /*self*/, int64_t /*cst*/) {
             throw_error(
                 PyExc_NotImplementedError,
                 "calling right shift on a linear expression is not supported");
           })
      .def("__and__",
           [](IntLinExpr* /*self*/, IntLinExpr* /*other*/) {
             throw_error(PyExc_NotImplementedError,
                         "calling and on a linear expression is not supported");
           })
      .def("__and__",
           [](IntLinExpr* /*self*/, int64_t /*cst*/) {
             throw_error(PyExc_NotImplementedError,
                         "calling and on a linear expression is not supported");
           })
      .def("__or__",
           [](IntLinExpr* /*self*/, IntLinExpr* /*other*/) {
             throw_error(PyExc_NotImplementedError,
                         "calling or on a linear expression is not supported");
           })
      .def("__or__",
           [](IntLinExpr* /*self*/, int64_t /*cst*/) {
             throw_error(PyExc_NotImplementedError,
                         "calling or on a linear expression is not supported");
           })
      .def("__xor__",
           [](IntLinExpr* /*self*/, IntLinExpr* /*other*/) {
             throw_error(PyExc_NotImplementedError,
                         "calling xor on a linear expression is not supported");
           })
      .def("__xor__",
           [](IntLinExpr* /*self*/, int64_t /*cst*/) {
             throw_error(PyExc_NotImplementedError,
                         "calling xor on a linear expression is not supported");
           })
      .def("__abs__",
           [](IntLinExpr* /*self*/) {
             throw_error(
                 PyExc_NotImplementedError,
                 "calling abs() on a linear expression is not supported, "
                 "please use CpModel.add_abs_equality");
           })
      .def("__bool__",
           [](IntLinExpr* /*self*/) {
             throw_error(PyExc_NotImplementedError,
                         "Evaluating a LinearExpr instance as a Boolean is "
                         "not implemented.");
           })
      .def_static("Sum",
                  py::overload_cast<const std::vector<IntExprOrValue>&>(
                      &IntLinExpr::Sum),
                  "Returns sum(exprs)", py::return_value_policy::automatic,
                  py::keep_alive<0, 1>())
      .def_static(
          "Sum",
          py::overload_cast<const std::vector<IntExprOrValue>&, int64_t>(
              &IntLinExpr::Sum),
          "Returns sum(exprs) + cst", py::return_value_policy::automatic,
          py::keep_alive<0, 1>())
      .def_static("WeightedSum",
                  py::overload_cast<const std::vector<IntExprOrValue>&,
                                    const std::vector<int64_t>&>(
                      &IntLinExpr::WeightedSum),
                  "Returns sum(exprs[i] * coeffs[i]",
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("WeightedSum",
                  py::overload_cast<const std::vector<FloatExprOrValue>&,
                                    const std::vector<double>&>(
                      &FloatLinearExpr::WeightedSum),
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("Term", &IntLinExpr::Term, arg("expr"), arg("coeff"),
                  "Returns expr * coeff.", py::return_value_policy::automatic);

  py::class_<IntAffine, IntLinExpr>(m, "IntAffine")
      .def(py::init<IntLinExpr*, int64_t, int64_t>())
      .def_property_readonly("expression", &IntAffine::expression)
      .def_property_readonly("coefficient", &IntAffine::coefficient)
      .def_property_readonly("offset", &IntAffine::offset);

  py::class_<Literal>(m, "Literal")
      .def_property_readonly("index", &Literal::index,
                             "The index of the variable in the model.")
      .def("negated", &Literal::negated,
           R"doc(
    Returns the negation of a literal (a Boolean variable or its negation).

    This method implements the logical negation of a Boolean variable.
    It is only valid if the variable has a Boolean domain (0 or 1).

    Note that this method is nilpotent: `x.negated().negated() == x`.          
          )doc",
           py::return_value_policy::automatic, py::keep_alive<1, 0>())
      .def("__invert__", &Literal::negated,
           "Returns the negation of the current literal.",
           py::return_value_policy::automatic)
      .def("__bool__",
           [](Literal* /*self*/) {
             throw_error(PyExc_NotImplementedError,
                         "Evaluating a Literal instance as a Boolean is "
                         "not implemented.");
           })
      // PEP8 Compatibility.
      .def("Not", &Literal::negated, py::return_value_policy::automatic)
      .def("Index", &Literal::index);

  py::class_<BaseIntVar, PyBaseIntVar, IntLinExpr, Literal>(m, "BaseIntVar")
      .def(py::init<int>())
      .def(py::init<int, bool>())
      .def_property_readonly("index", &BaseIntVar::index,
                             "The index of the variable in the model.")
      .def_property_readonly("is_boolean", &BaseIntVar::is_boolean,
                             "Whether the variable is boolean.")
      .def("__str__", &BaseIntVar::ToString)
      .def("__repr__", &BaseIntVar::DebugString)
      .def(
          "negated",
          [](BaseIntVar* self) {
            if (!self->is_boolean()) {
              throw_error(PyExc_TypeError,
                          "negated() is only supported for boolean variables.");
            }
            return self->negated();
          },
          "Returns the negation of the current Boolean variable.",
          py::return_value_policy::automatic, py::keep_alive<1, 0>())
      .def(
          "__invert__",
          [](BaseIntVar* self) {
            if (!self->is_boolean()) {
              throw_error(PyExc_ValueError,
                          "negated() is only supported for boolean variables.");
            }
            return self->negated();
          },
          "Returns the negation of the current Boolean variable.",
          py::return_value_policy::automatic, py::keep_alive<1, 0>())
      // PEP8 Compatibility.
      .def(
          "Not",
          [](BaseIntVar* self) {
            if (!self->is_boolean()) {
              throw_error(PyExc_ValueError,
                          "negated() is only supported for boolean variables.");
            }
            return self->negated();
          },
          py::return_value_policy::automatic, py::keep_alive<1, 0>());

  py::class_<NotBooleanVariable, IntLinExpr, Literal>(m, "NotBooleanVariable")
      .def(py::init<BaseIntVar*>())
      .def_property_readonly("index", &NotBooleanVariable::index,
                             "The index of the variable in the model.")
      .def("__str__", &NotBooleanVariable::ToString)
      .def("__repr__", &NotBooleanVariable::DebugString)
      .def("negated", &NotBooleanVariable::negated,
           "Returns the negation of the current Boolean variable.",
           py::return_value_policy::automatic)
      .def("__invert__", &NotBooleanVariable::negated,
           "Returns the negation of the current Boolean variable.",
           py::return_value_policy::automatic)
      .def("Not", &NotBooleanVariable::negated,
           "Returns the negation of the current Boolean variable.",
           py::return_value_policy::automatic);

  py::class_<BoundedLinearExpression>(m, "BoundedLinearExpression")
      .def(py::init<IntLinExpr*, const Domain&>())
      .def(py::init<int64_t, const Domain&>())
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
                                 "not implemented.")
                        .c_str());
        return false;
      });
}  // NOLINT(readability/fn_size)

}  // namespace python
}  // namespace sat
}  // namespace operations_research
