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

#include <Python.h>

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/python/linear_expr.h"
#include "ortools/sat/python/linear_expr_doc.h"
#include "ortools/sat/swig_helper.h"
#include "ortools/util/sorted_interval_list.h"
#include "pybind11/attr.h"
#include "pybind11/cast.h"
#include "pybind11/functional.h"
#include "pybind11/gil.h"
#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "pybind11/pytypes.h"
#include "pybind11/stl.h"
#include "pybind11_protobuf/native_proto_caster.h"

namespace py = pybind11;

namespace operations_research::sat::python {

void ThrowError(PyObject* py_exception, const std::string& message) {
  PyErr_SetString(py_exception, message.c_str());
  throw py::error_already_set();
}

// We extend the SolverWrapper class to keep track of the local error already
// set.
class ExtSolveWrapper : public SolveWrapper {
 public:
  mutable std::optional<py::error_already_set> local_error_already_set_;
};

// A trampoline class to override the OnSolutionCallback method to acquire the
// GIL.
class PySolutionCallback : public SolutionCallback {
 public:
  using SolutionCallback::SolutionCallback; /* Inherit constructors */
  void OnSolutionCallback() const override {
    ::py::gil_scoped_acquire acquire;
    try {
      PYBIND11_OVERRIDE_PURE(
          void,               /* Return type */
          SolutionCallback,   /* Parent class */
          OnSolutionCallback, /* Name of function */
          /* This function has no arguments. The trailing comma
             in the previous line is needed for some compilers */
      );
    } catch (py::error_already_set& e) {
      // We assume this code is serialized as the gil is held.
      ExtSolveWrapper* solve_wrapper = static_cast<ExtSolveWrapper*>(wrapper());
      if (!solve_wrapper->local_error_already_set_.has_value()) {
        solve_wrapper->local_error_already_set_ = e;
      }
      StopSearch();
    }
  }
};

// A trampoline class to override the __str__ and __repr__ methods.
class PyBaseIntVar : public BaseIntVar {
 public:
  using BaseIntVar::BaseIntVar; /* Inherit constructors */

  std::string ToString() const override {
    PYBIND11_OVERRIDE_PURE_NAME(std::string,  // Return type (ret_type)
                                BaseIntVar,   // Parent class (cname)
                                "__str__",    // Name of method in Python (name)
                                ToString,     // Name of function in C++ (fn)
    );
  }

  std::string DebugString() const override {
    PYBIND11_OVERRIDE_PURE_NAME(std::string,  // Return type (ret_type)
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

  bool BooleanValue(std::shared_ptr<Literal> lit) const {
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

  double FloatValue(std::shared_ptr<LinearExpr> expr) const {
    FloatExprVisitor visitor;
    visitor.AddToProcess(expr, 1);
    return visitor.Evaluate(response_);
  }

  double FixedFloatValue(double value) const { return value; }

  int64_t Value(std::shared_ptr<LinearExpr> expr) const {
    int64_t value;
    IntExprVisitor visitor;
    visitor.AddToProcess(expr, 1);
    if (!visitor.Evaluate(response_, &value)) {
      ThrowError(PyExc_ValueError,
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

// Checks that the result is not null and throws an error if it is.
std::shared_ptr<BoundedLinearExpression> CheckBoundedLinearExpression(
    std::shared_ptr<BoundedLinearExpression> result,
    std::shared_ptr<LinearExpr> lhs,
    std::shared_ptr<LinearExpr> rhs = nullptr) {
  if (!result->ok()) {
    if (rhs == nullptr) {
      ThrowError(PyExc_TypeError,
                 absl::StrCat("Linear constraints only accept integer values "
                              "and coefficients: ",
                              lhs->DebugString()));
    } else {
      ThrowError(PyExc_TypeError,
                 absl::StrCat("Linear constraints only accept integer values "
                              "and coefficients: ",
                              lhs->DebugString(), " and ", rhs->DebugString()));
    }
  }
  return result;
}

void RaiseIfNone(std::shared_ptr<LinearExpr> expr) {
  if (expr == nullptr) {
    ThrowError(PyExc_TypeError,
               "Linear constraints do not accept None as argument.");
  }
}

void ProcessExprArg(
    const py::handle& arg,
    absl::AnyInvocable<void(std::shared_ptr<LinearExpr>)> on_linear_expr,
    absl::AnyInvocable<void(int64_t)> on_int_constant,
    absl::AnyInvocable<void(double)> on_float_constant) {
  if (py::isinstance<LinearExpr>(arg)) {
    on_linear_expr(arg.cast<std::shared_ptr<LinearExpr>>());
  } else if (py::isinstance<py::int_>(arg)) {
    on_int_constant(arg.cast<int64_t>());
  } else if (py::isinstance<py::float_>(arg)) {
    on_float_constant(arg.cast<double>());
  } else if (hasattr(arg, "dtype") && hasattr(arg, "is_integer")) {
    if (getattr(arg, "is_integer")().cast<bool>()) {
      on_int_constant(arg.cast<int64_t>());
    } else {
      on_float_constant(arg.cast<double>());
    }
  } else {
    py::type objtype = py::type::of(arg);
    const std::string type_name = objtype.attr("__name__").cast<std::string>();
    ThrowError(PyExc_TypeError,
               absl::StrCat("LinearExpr::sum() only accept linear "
                            "expressions and constants as argument: '",
                            absl::CEscape(type_name), "'"));
  }
}

void ProcessConstantArg(const py::handle& arg,
                        absl::AnyInvocable<void(int64_t)> on_int_constant,
                        absl::AnyInvocable<void(double)> on_float_constant) {
  if (py::isinstance<py::int_>(arg)) {
    on_int_constant(arg.cast<int64_t>());
  } else if (py::isinstance<py::float_>(arg)) {
    on_float_constant(arg.cast<double>());
  } else if (hasattr(arg, "dtype") && hasattr(arg, "is_integer")) {
    if (getattr(arg, "is_integer")().cast<bool>()) {
      on_int_constant(arg.cast<int64_t>());
    } else {
      on_float_constant(arg.cast<double>());
    }
  } else {
    py::type objtype = py::type::of(arg);
    const std::string type_name = objtype.attr("__name__").cast<std::string>();
    ThrowError(PyExc_TypeError,
               absl::StrCat("LinearExpr::weighted_sum() only accept constants "
                            "as coefficients: '",
                            absl::CEscape(type_name), "'"));
  }
}

std::shared_ptr<LinearExpr> SumArguments(py::args expressions) {
  std::vector<std::shared_ptr<LinearExpr>> linear_exprs;
  int64_t int_offset = 0;
  double float_offset = 0.0;
  bool has_floats = false;

  const auto process_arg = [&](const py::handle& arg) -> void {
    ProcessExprArg(
        arg,
        [&](std::shared_ptr<LinearExpr> expr) { linear_exprs.push_back(expr); },
        [&](int64_t value) { int_offset += value; },
        [&](double value) {
          if (value != 0.0) {
            float_offset += value;
            has_floats = true;
          }
        });
  };

  if (expressions.size() == 1 && py::isinstance<py::sequence>(expressions[0])) {
    // Normal list or tuple argument.
    py::sequence elements = expressions[0].cast<py::sequence>();
    linear_exprs.reserve(elements.size());
    for (const py::handle& expr : elements) {
      process_arg(expr);
    }
  } else {  // Direct sum(x, y, 3, ..) without [].
    linear_exprs.reserve(expressions.size());
    for (const py::handle expr : expressions) {
      process_arg(expr);
    }
  }

  // If there are floats, we add the int offset to the float offset.
  if (has_floats) {
    float_offset += static_cast<double>(int_offset);
    int_offset = 0;
  }

  if (linear_exprs.empty()) {
    if (has_floats) {
      return std::make_shared<FloatConstant>(float_offset);
    } else {
      return std::make_shared<IntConstant>(int_offset);
    }
  } else if (linear_exprs.size() == 1) {
    if (has_floats) {
      if (float_offset == 0.0) {
        return linear_exprs[0];
      } else {
        return std::make_shared<FloatAffine>(linear_exprs[0], 1.0,
                                             float_offset);
      }
    } else if (int_offset != 0) {
      return std::make_shared<IntAffine>(linear_exprs[0], 1, int_offset);
    } else {
      return linear_exprs[0];
    }
  } else {
    if (has_floats) {
      return std::make_shared<SumArray>(linear_exprs, 0, float_offset);
    } else {
      return std::make_shared<SumArray>(linear_exprs, int_offset, 0.0);
    }
  }
}

std::shared_ptr<LinearExpr> WeightedSumArguments(py::sequence expressions,
                                                 py::sequence coefficients) {
  if (expressions.size() != coefficients.size()) {
    ThrowError(PyExc_ValueError,
               absl::StrCat("LinearExpr::weighted_sum() requires the same "
                            "number of arguments and coefficients: ",
                            expressions.size(), " != ", coefficients.size()));
  }

  std::vector<std::shared_ptr<LinearExpr>> linear_exprs;
  std::vector<int64_t> int_coeffs;
  std::vector<double> float_coeffs;
  linear_exprs.reserve(expressions.size());
  int_coeffs.reserve(expressions.size());
  float_coeffs.reserve(expressions.size());
  int64_t int_offset = 0;
  double float_offset = 0.0;
  bool has_floats = false;

  for (int i = 0; i < expressions.size(); ++i) {
    auto on_expr = [&](std::shared_ptr<LinearExpr> expr) {
      ProcessConstantArg(
          coefficients[i],
          [&](int64_t value) {
            if (value == 0) return;
            linear_exprs.push_back(expr);
            int_coeffs.push_back(value);
            float_coeffs.push_back(static_cast<double>(value));
          },
          [&](double value) {
            if (value == 0.0) return;
            linear_exprs.push_back(expr);
            float_coeffs.push_back(value);
            has_floats = true;
          });
    };
    auto on_int = [&](int64_t expr_value) {
      if (expr_value == 0) return;
      ProcessConstantArg(
          coefficients[i],
          [&](int64_t coeff_value) { int_offset += coeff_value * expr_value; },
          [&](double coeff_value) {
            has_floats = true;
            float_offset += coeff_value * static_cast<double>(expr_value);
          });
    };
    auto on_float = [&](double expr_value) {
      if (expr_value == 0.0) return;
      has_floats = true;
      ProcessConstantArg(
          coefficients[i],
          [&](int64_t coeff_value) {
            float_offset += static_cast<double>(coeff_value) * expr_value;
          },
          [&](double coeff_value) {
            if (coeff_value == 0.0) return;
            float_offset += coeff_value * expr_value;
          });
    };
    ProcessExprArg(expressions[i], std::move(on_expr), std::move(on_int),
                   std::move(on_float));
  }

  // Correct the float offset if there are int offsets.
  if (has_floats) {
    float_offset += static_cast<double>(int_offset);
    int_offset = 0;
  }

  if (linear_exprs.empty()) {
    if (has_floats) {
      return std::make_shared<FloatConstant>(float_offset);
    } else {
      return std::make_shared<IntConstant>(int_offset);
    }
  } else if (linear_exprs.size() == 1) {
    if (has_floats) {
      return std::make_shared<FloatAffine>(linear_exprs[0], float_coeffs[0],
                                           float_offset);
    } else if (int_offset != 0 || int_coeffs[0] != 1) {
      return std::make_shared<IntAffine>(linear_exprs[0], int_coeffs[0],
                                         int_offset);
    } else {
      return linear_exprs[0];
    }
  } else {
    if (has_floats) {
      return std::make_shared<FloatWeightedSum>(linear_exprs, float_coeffs,
                                                float_offset);
    } else {
      return std::make_shared<IntWeightedSum>(linear_exprs, int_coeffs,
                                              int_offset);
    }
  }
}

PYBIND11_MODULE(cp_model_helper, m) {
  pybind11_protobuf::ImportNativeProtoCasters();
  py::module::import("ortools.util.python.sorted_interval_list");

  // We keep the CamelCase name for the SolutionCallback class to be
  // compatible with the pre PEP8 python code.
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
           py::arg("index"))
      .def("SolutionIntegerValue", &SolutionCallback::SolutionIntegerValue,
           py::arg("index"))
      .def("StopSearch", &SolutionCallback::StopSearch)
      .def("UserTime", &SolutionCallback::UserTime)
      .def("WallTime", &SolutionCallback::WallTime)
      .def(
          "Value",
          [](const SolutionCallback& callback,
             std::shared_ptr<LinearExpr> expr) {
            int64_t value;
            IntExprVisitor visitor;
            visitor.AddToProcess(expr, 1);
            if (!visitor.Evaluate(callback.Response(), &value)) {
              ThrowError(PyExc_ValueError,
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
          "FloatValue",
          [](const SolutionCallback& callback,
             std::shared_ptr<LinearExpr> expr) {
            FloatExprVisitor visitor;
            visitor.AddToProcess(expr, 1.0);
            return visitor.Evaluate(callback.Response());
          },
          "Returns the value of a floating point linear expression after "
          "solve.")
      .def(
          "FloatValue",
          [](const SolutionCallback&, double value) { return value; },
          "Returns the value of a floating point linear expression after "
          "solve.")
      .def(
          "BooleanValue",
          [](const SolutionCallback& callback, std::shared_ptr<Literal> lit) {
            return callback.SolutionBooleanValue(lit->index());
          },
          "Returns the Boolean value of a literal after solve.")
      .def(
          "BooleanValue", [](const SolutionCallback&, bool lit) { return lit; },
          "Returns the Boolean value of a literal after solve.");

  py::class_<ResponseWrapper>(m, "ResponseWrapper")
      .def("best_objective_bound", &ResponseWrapper::BestObjectiveBound)
      .def("boolean_value", &ResponseWrapper::BooleanValue, py::arg("lit"))
      .def("boolean_value", &ResponseWrapper::FixedBooleanValue, py::arg("lit"))
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
      .def("float_value", &ResponseWrapper::FloatValue, py::arg("expr"))
      .def("float_value", &ResponseWrapper::FixedFloatValue, py::arg("value"))
      .def("value", &ResponseWrapper::Value, py::arg("expr"))
      .def("value", &ResponseWrapper::FixedValue, py::arg("value"))
      .def("wall_time", &ResponseWrapper::WallTime);

  py::class_<ExtSolveWrapper>(m, "SolveWrapper")
      .def(py::init<>())
      .def(
          "add_log_callback",
          [](ExtSolveWrapper* solve_wrapper,
             std::function<void(const std::string&)> log_callback) {
            std::function<void(const std::string&)> safe_log_callback =
                [solve_wrapper, log_callback](std::string message) -> void {
              ::py::gil_scoped_acquire acquire;
              try {
                log_callback(message);
              } catch (py::error_already_set& e) {
                // We assume this code is serialized as the gil is held.
                if (!solve_wrapper->local_error_already_set_.has_value()) {
                  solve_wrapper->local_error_already_set_ = e;
                }
                solve_wrapper->StopSearch();
              }
            };
            solve_wrapper->AddLogCallback(safe_log_callback);
          },
          py::arg("log_callback").none(false))
      .def("add_solution_callback", &SolveWrapper::AddSolutionCallback,
           py::arg("callback"))
      .def("clear_solution_callback", &SolveWrapper::ClearSolutionCallback)
      .def(
          "add_best_bound_callback",
          [](ExtSolveWrapper* solve_wrapper,
             std::function<void(double)> best_bound_callback) {
            std::function<void(double)> safe_best_bound_callback =
                [solve_wrapper, best_bound_callback](double bound) -> void {
              ::py::gil_scoped_acquire acquire;
              try {
                best_bound_callback(bound);
              } catch (py::error_already_set& e) {
                // We assume this code is serialized as the gil is held.
                if (!solve_wrapper->local_error_already_set_.has_value()) {
                  solve_wrapper->local_error_already_set_ = e;
                }
                solve_wrapper->StopSearch();
              }
            };
            solve_wrapper->AddBestBoundCallback(safe_best_bound_callback);
          },
          py::arg("best_bound_callback").none(false))
      .def("set_parameters", &SolveWrapper::SetParameters,
           py::arg("parameters"))
      .def("solve",
           [](ExtSolveWrapper* solve_wrapper,
              const CpModelProto& model_proto) -> CpSolverResponse {
             const auto result = [&]() -> CpSolverResponse {
               ::py::gil_scoped_release release;
               return solve_wrapper->Solve(model_proto);
             }();
             if (solve_wrapper->local_error_already_set_.has_value()) {
               solve_wrapper->local_error_already_set_->restore();
               solve_wrapper->local_error_already_set_.reset();
               throw py::error_already_set();
             }
             return result;
           })
      .def("solve_and_return_response_wrapper",
           [](ExtSolveWrapper* solve_wrapper,
              const CpModelProto& model_proto) -> ResponseWrapper {
             const auto result = [&]() -> ResponseWrapper {
               ::py::gil_scoped_release release;
               return ResponseWrapper(solve_wrapper->Solve(model_proto));
             }();
             if (solve_wrapper->local_error_already_set_.has_value()) {
               solve_wrapper->local_error_already_set_->restore();
               solve_wrapper->local_error_already_set_.reset();
               throw py::error_already_set();
             }
             return result;
           })
      .def("stop_search", &SolveWrapper::StopSearch);

  py::class_<CpSatHelper>(m, "CpSatHelper")
      .def_static("model_stats", &CpSatHelper::ModelStats,
                  py::arg("model_proto"))
      .def_static("solver_response_stats", &CpSatHelper::SolverResponseStats,
                  py::arg("response"))
      .def_static("validate_model", &CpSatHelper::ValidateModel,
                  py::arg("model_proto"))
      .def_static("variable_domain", &CpSatHelper::VariableDomain,
                  py::arg("variable_proto"))
      .def_static("write_model_to_file", &CpSatHelper::WriteModelToFile,
                  py::arg("model_proto"), py::arg("filename"));

  py::class_<LinearExpr, std::shared_ptr<LinearExpr>>(
      m, "LinearExpr", DOC(operations_research, sat, python, LinearExpr))
      .def_static("sum", &SumArguments, "Returns the sum(expressions).")
      .def_static("weighted_sum", &WeightedSumArguments, py::arg("expressions"),
                  py::arg("coefficients"),
                  "Returns the sum of (expressions[i] * coefficients[i])")
      .def_static("term", &LinearExpr::TermInt, py::arg("expr").none(false),
                  py::arg("coeff"),
                  DOC(operations_research, sat, python, LinearExpr, TermInt))
      .def_static("term", &LinearExpr::TermFloat, py::arg("expr").none(false),
                  py::arg("coeff"),
                  DOC(operations_research, sat, python, LinearExpr, TermFloat))
      .def_static("affine", &LinearExpr::AffineInt, py::arg("expr").none(false),
                  py::arg("coeff"), py::arg("offset"),
                  DOC(operations_research, sat, python, LinearExpr, AffineInt))
      .def_static(
          "affine", &LinearExpr::AffineFloat, py::arg("expr").none(false),
          py::arg("coeff"), py::arg("offset"),
          DOC(operations_research, sat, python, LinearExpr, AffineFloat))
      .def_static(
          "constant", &LinearExpr::ConstantInt, py::arg("value"),
          DOC(operations_research, sat, python, LinearExpr, ConstantInt))
      .def_static(
          "constant", &LinearExpr::ConstantFloat, py::arg("value"),
          DOC(operations_research, sat, python, LinearExpr, ConstantFloat))
      // Pre PEP8 compatibility layer.
      .def_static("Sum", &SumArguments)
      .def_static("WeightedSum", &WeightedSumArguments, py::arg("expressions"),
                  py::arg("coefficients"))
      .def_static("Term", &LinearExpr::TermInt, py::arg("expr").none(false),
                  py::arg("coeff"), "Returns expr * coeff.")
      .def_static("Term", &LinearExpr::TermFloat, py::arg("expr").none(false),
                  py::arg("coeff"), "Returns expr * coeff.")
      // Methods.
      .def("__str__",
           [](std::shared_ptr<LinearExpr> expr) -> std::string {
             return expr->ToString();
           })
      .def("__repr__",
           [](std::shared_ptr<LinearExpr> expr) -> std::string {
             return expr->DebugString();
           })
      .def(
          "is_integer",
          [](std::shared_ptr<LinearExpr> expr) -> bool {
            return expr->IsInteger();
          },
          DOC(operations_research, sat, python, LinearExpr, IsInteger))
      // Operators.
      .def("__add__", &LinearExpr::Add, py::arg("other").none(false),
           DOC(operations_research, sat, python, LinearExpr, Add))
      .def("__add__", &LinearExpr::AddInt, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, AddInt))
      .def("__add__", &LinearExpr::AddFloat, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, AddFloat))
      .def("__radd__", &LinearExpr::AddInt, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, AddInt))
      .def("__radd__", &LinearExpr::AddFloat, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, AddFloat))
      .def("__sub__", &LinearExpr::Sub, py::arg("other").none(false),
           DOC(operations_research, sat, python, LinearExpr, Sub))
      .def("__sub__", &LinearExpr::SubInt, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, SubInt))
      .def("__sub__", &LinearExpr::SubFloat, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, SubFloat))
      .def("__rsub__", &LinearExpr::RSubInt, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, RSubInt))
      .def("__rsub__", &LinearExpr::RSubFloat, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, RSubFloat))
      .def("__mul__", &LinearExpr::MulInt, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, MulInt))
      .def("__mul__", &LinearExpr::MulFloat, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, MulFloat))
      .def("__rmul__", &LinearExpr::MulInt, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, MulInt))
      .def("__rmul__", &LinearExpr::MulFloat, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, MulFloat))
      .def("__neg__", &LinearExpr::Neg,
           DOC(operations_research, sat, python, LinearExpr, Neg))
      .def(
          "__eq__",
          [](std::shared_ptr<LinearExpr> lhs, std::shared_ptr<LinearExpr> rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Eq(rhs), lhs, rhs);
          },
          DOC(operations_research, sat, python, LinearExpr, Eq))
      .def(
          "__eq__",
          [](std::shared_ptr<LinearExpr> lhs, int64_t rhs) {
            if (rhs == std::numeric_limits<int64_t>::max() ||
                rhs == std::numeric_limits<int64_t>::min()) {
              ThrowError(PyExc_ValueError,
                         "== INT_MIN or INT_MAX is not supported");
            }
            return CheckBoundedLinearExpression(lhs->EqCst(rhs), lhs);
          },
          DOC(operations_research, sat, python, LinearExpr, EqCst))
      .def(
          "__ne__",
          [](std::shared_ptr<LinearExpr> lhs, std::shared_ptr<LinearExpr> rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Ne(rhs), lhs, rhs);
          },
          DOC(operations_research, sat, python, LinearExpr, Ne))
      .def(
          "__ne__",
          [](std::shared_ptr<LinearExpr> lhs, int64_t rhs) {
            return CheckBoundedLinearExpression(lhs->NeCst(rhs), lhs);
          },
          DOC(operations_research, sat, python, LinearExpr, NeCst))
      .def(
          "__le__",
          [](std::shared_ptr<LinearExpr> lhs, std::shared_ptr<LinearExpr> rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Le(rhs), lhs, rhs);
          },
          DOC(operations_research, sat, python, LinearExpr, Le))
      .def(
          "__le__",
          [](std::shared_ptr<LinearExpr> lhs, int64_t rhs) {
            if (rhs == std::numeric_limits<int64_t>::min()) {
              ThrowError(PyExc_ArithmeticError, "<= INT_MIN is not supported");
            }
            return CheckBoundedLinearExpression(lhs->LeCst(rhs), lhs);
          },
          DOC(operations_research, sat, python, LinearExpr, LeCst))
      .def(
          "__lt__",
          [](std::shared_ptr<LinearExpr> lhs, std::shared_ptr<LinearExpr> rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Lt(rhs), lhs, rhs);
          },
          DOC(operations_research, sat, python, LinearExpr, Lt))
      .def(
          "__lt__",
          [](std::shared_ptr<LinearExpr> lhs, int64_t rhs) {
            if (rhs == std::numeric_limits<int64_t>::min()) {
              ThrowError(PyExc_ArithmeticError, "< INT_MIN is not supported");
            }
            return CheckBoundedLinearExpression(lhs->LtCst(rhs), lhs);
          },
          DOC(operations_research, sat, python, LinearExpr, LtCst))
      .def(
          "__ge__",
          [](std::shared_ptr<LinearExpr> lhs, std::shared_ptr<LinearExpr> rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Ge(rhs), lhs, rhs);
          },
          DOC(operations_research, sat, python, LinearExpr, Ge))
      .def(
          "__ge__",
          [](std::shared_ptr<LinearExpr> lhs, int64_t rhs) {
            if (rhs == std::numeric_limits<int64_t>::max()) {
              ThrowError(PyExc_ArithmeticError, ">= INT_MAX is not supported");
            }
            return CheckBoundedLinearExpression(lhs->GeCst(rhs), lhs);
          },
          DOC(operations_research, sat, python, LinearExpr, GeCst))
      .def(
          "__gt__",
          [](std::shared_ptr<LinearExpr> lhs, std::shared_ptr<LinearExpr> rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Gt(rhs), lhs, rhs);
          },
          DOC(operations_research, sat, python, LinearExpr, Gt))
      .def(
          "__gt__",
          [](std::shared_ptr<LinearExpr> lhs, int64_t rhs) {
            if (rhs == std::numeric_limits<int64_t>::max()) {
              ThrowError(PyExc_ArithmeticError, "> INT_MAX is not supported");
            }
            return CheckBoundedLinearExpression(lhs->GtCst(rhs), lhs);
          },
          DOC(operations_research, sat, python, LinearExpr, GtCst))
      // Disable other operators as they are not supported.
      .def("__div__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling / on a linear expression is not supported, "
                        "please use CpModel.add_division_equality");
           })
      .def("__truediv__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling // on a linear expression is not supported, "
                        "please use CpModel.add_division_equality");
           })
      .def("__mod__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling %% on a linear expression is not supported, "
                        "please use CpModel.add_modulo_equality");
           })
      .def("__pow__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling ** on a linear expression is not supported, "
                        "please use CpModel.add_multiplication_equality");
           })
      .def("__lshift__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling left shift on a linear expression is not "
                        "supported");
           })
      .def("__rshift__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling right shift on a linear expression is "
                        "not supported");
           })
      .def("__and__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling and on a linear expression is not supported");
           })
      .def("__or__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling or on a linear expression is not supported");
           })
      .def("__xor__",
           [](std::shared_ptr<LinearExpr> /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling xor on a linear expression is not supported");
           })
      .def("__abs__",
           [](std::shared_ptr<LinearExpr> /*self*/) {
             ThrowError(
                 PyExc_NotImplementedError,
                 "calling abs() on a linear expression is not supported, "
                 "please use CpModel.add_abs_equality");
           })
      .def("__bool__", [](std::shared_ptr<LinearExpr> /*self*/) {
        ThrowError(PyExc_NotImplementedError,
                   "Evaluating a LinearExpr instance as a Boolean is "
                   "not supported.");
      });

  // Expose Internal classes, mostly for testing.
  py::class_<FlatFloatExpr, std::shared_ptr<FlatFloatExpr>, LinearExpr>(
      m, "FlatFloatExpr", DOC(operations_research, sat, python, FlatFloatExpr))
      .def(py::init<std::shared_ptr<LinearExpr>>())
      .def_property_readonly("vars", &FlatFloatExpr::vars)
      .def_property_readonly("coeffs", &FlatFloatExpr::coeffs)
      .def_property_readonly("offset", &FlatFloatExpr::offset);

  py::class_<FlatIntExpr, std::shared_ptr<FlatIntExpr>, LinearExpr>(
      m, "FlatIntExpr", DOC(operations_research, sat, python, FlatIntExpr))
      .def(py::init([](std::shared_ptr<LinearExpr> expr) {
        FlatIntExpr* result = new FlatIntExpr(expr);
        if (!result->ok()) {
          ThrowError(PyExc_TypeError,
                     absl::StrCat("Tried to build a FlatIntExpr from a linear "
                                  "expression with "
                                  "floating point coefficients or constants:  ",
                                  expr->DebugString()));
        }
        return result;
      }))
      .def_property_readonly("vars", &FlatIntExpr::vars)
      .def_property_readonly("coeffs", &FlatIntExpr::coeffs)
      .def_property_readonly("offset", &FlatIntExpr::offset)
      .def_property_readonly("ok", &FlatIntExpr::ok);

  py::class_<SumArray, std::shared_ptr<SumArray>, LinearExpr>(
      m, "SumArray", DOC(operations_research, sat, python, SumArray))
      .def(
          py::init<std::vector<std::shared_ptr<LinearExpr>>, int64_t, double>())
      .def(
          "__add__",
          [](py::object self,
             std::shared_ptr<LinearExpr> other) -> std::shared_ptr<LinearExpr> {
            const int num_uses = Py_REFCNT(self.ptr());
            std::shared_ptr<SumArray> expr =
                self.cast<std::shared_ptr<SumArray>>();
            if (num_uses == 4) {
              expr->AddInPlace(other);
              return expr;
            }
            return expr->Add(other);
          },
          py::arg("other").none(false),
          DOC(operations_research, sat, python, LinearExpr, Add))
      .def(
          "__add__",
          [](py::object self, int64_t cst) -> std::shared_ptr<LinearExpr> {
            const int num_uses = Py_REFCNT(self.ptr());
            std::shared_ptr<SumArray> expr =
                self.cast<std::shared_ptr<SumArray>>();
            if (num_uses == 4) {
              expr->AddIntInPlace(cst);
              return expr;
            }
            return expr->AddInt(cst);
          },
          DOC(operations_research, sat, python, LinearExpr, AddInt))
      .def(
          "__add__",
          [](py::object self, double cst) -> std::shared_ptr<LinearExpr> {
            const int num_uses = Py_REFCNT(self.ptr());
            std::shared_ptr<SumArray> expr =
                self.cast<std::shared_ptr<SumArray>>();
            if (num_uses == 4) {
              expr->AddFloatInPlace(cst);
              return expr;
            }
            return expr->AddFloat(cst);
          },
          py::arg("other").none(false),
          DOC(operations_research, sat, python, LinearExpr, AddFloat))
      .def(
          "__radd__",
          [](py::object self, int64_t cst) -> std::shared_ptr<LinearExpr> {
            const int num_uses = Py_REFCNT(self.ptr());
            std::shared_ptr<SumArray> expr =
                self.cast<std::shared_ptr<SumArray>>();
            if (num_uses == 4) {
              expr->AddIntInPlace(cst);
              return expr;
            }
            return expr->AddInt(cst);
          },
          py::arg("other").none(false),
          DOC(operations_research, sat, python, LinearExpr, AddInt))
      .def(
          "__radd__",
          [](py::object self, double cst) -> std::shared_ptr<LinearExpr> {
            const int num_uses = Py_REFCNT(self.ptr());
            std::shared_ptr<SumArray> expr =
                self.cast<std::shared_ptr<SumArray>>();
            if (num_uses == 4) {
              expr->AddFloatInPlace(cst);
              return expr;
            }
            return expr->AddFloat(cst);
          },
          DOC(operations_research, sat, python, LinearExpr, AddFloat))
      .def(
          "__sub__",
          [](py::object self,
             std::shared_ptr<LinearExpr> other) -> std::shared_ptr<LinearExpr> {
            const int num_uses = Py_REFCNT(self.ptr());
            std::shared_ptr<SumArray> expr =
                self.cast<std::shared_ptr<SumArray>>();
            if (num_uses == 4) {
              expr->AddInPlace(other->Neg());
              return expr;
            }
            return expr->Sub(other);
          },
          py::arg("other").none(false),
          DOC(operations_research, sat, python, LinearExpr, Sub))
      .def(
          "__sub__",
          [](py::object self, int64_t cst) -> std::shared_ptr<LinearExpr> {
            const int num_uses = Py_REFCNT(self.ptr());
            std::shared_ptr<SumArray> expr =
                self.cast<std::shared_ptr<SumArray>>();
            if (num_uses == 4) {
              expr->AddIntInPlace(-cst);
              return expr;
            }
            return expr->SubInt(cst);
          },
          DOC(operations_research, sat, python, LinearExpr, SubInt))
      .def(
          "__sub__",
          [](py::object self, double cst) -> std::shared_ptr<LinearExpr> {
            const int num_uses = Py_REFCNT(self.ptr());
            std::shared_ptr<SumArray> expr =
                self.cast<std::shared_ptr<SumArray>>();
            if (num_uses == 4) {
              expr->AddFloatInPlace(-cst);
              return expr;
            }
            return expr->SubFloat(cst);
          },
          DOC(operations_research, sat, python, LinearExpr, SubFloat))
      .def_property_readonly("num_exprs", &SumArray::num_exprs)
      .def_property_readonly("int_offset", &SumArray::int_offset)
      .def_property_readonly("double_offset", &SumArray::double_offset);

  py::class_<FloatAffine, std::shared_ptr<FloatAffine>, LinearExpr>(
      m, "FloatAffine", DOC(operations_research, sat, python, FloatAffine))
      .def(py::init<std::shared_ptr<LinearExpr>, double, double>())
      .def_property_readonly("expression", &FloatAffine::expression)
      .def_property_readonly("coefficient", &FloatAffine::coefficient)
      .def_property_readonly("offset", &FloatAffine::offset);

  // We adding an operator like __add__(int), we need to add all overloads,
  // otherwise they are not found.
  py::class_<IntAffine, std::shared_ptr<IntAffine>, LinearExpr>(
      m, "IntAffine", DOC(operations_research, sat, python, IntAffine))
      .def(py::init<std::shared_ptr<LinearExpr>, int64_t, int64_t>())
      .def("__add__", &LinearExpr::Add, py::arg("other").none(false),
           DOC(operations_research, sat, python, LinearExpr, Add))
      .def("__add__", &IntAffine::AddInt, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, AddInt))
      .def("__add__", &LinearExpr::AddFloat, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, AddFloat))
      .def("__radd__", &LinearExpr::Add, py::arg("other").none(false),
           DOC(operations_research, sat, python, LinearExpr, Add))
      .def("__radd__", &IntAffine::AddInt, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, AddInt))
      .def("__radd__", &LinearExpr::AddFloat, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, AddFloat))
      .def("__sub__", &LinearExpr::Sub, py::arg("other").none(false),
           DOC(operations_research, sat, python, LinearExpr, Sub))
      .def("__sub__", &IntAffine::SubInt, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, SubInt))
      .def("__sub__", &LinearExpr::SubFloat, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, SubFloat))
      .def("__rsub__", &IntAffine::RSubInt, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, RSubInt))
      .def("__rsub__", &LinearExpr::SubFloat, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, RSubFloat))
      .def("__mul__", &IntAffine::MulInt, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, MulInt))
      .def("__mul__", &LinearExpr::MulFloat, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, MulFloat))
      .def("__rmul__", &IntAffine::MulInt, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, MulInt))
      .def("__rmul__", &LinearExpr::MulFloat, py::arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, MulFloat))
      .def("__neg__", &IntAffine::Neg,
           DOC(operations_research, sat, python, LinearExpr, Neg))
      .def_property_readonly("expression", &IntAffine::expression,
                             "Returns the linear expression.")
      .def_property_readonly("coefficient", &IntAffine::coefficient,
                             "Returns the coefficient.")
      .def_property_readonly("offset", &IntAffine::offset,
                             "Returns the offset.");

  py::class_<Literal, std::shared_ptr<Literal>, LinearExpr>(
      m, "Literal", DOC(operations_research, sat, python, Literal))
      .def_property_readonly(
          "index", &Literal::index,
          DOC(operations_research, sat, python, Literal, index))
      .def("negated", &Literal::negated,
           DOC(operations_research, sat, python, Literal, negated))
      .def("__invert__", &Literal::negated,
           DOC(operations_research, sat, python, Literal, negated))
      .def("__bool__",
           [](std::shared_ptr<Literal> /*self*/) {
             ThrowError(PyExc_NotImplementedError,
                        "Evaluating a Literal as a Boolean valueis "
                        "not supported.");
           })
      .def("__hash__", &Literal::Hash)
      // PEP8 Compatibility.
      .def("Not", &Literal::negated)
      .def("Index", &Literal::index);

  // Memory management:
  // - The BaseIntVar owns the NotBooleanVariable and keeps a shared_ptr to it.
  // - The NotBooleanVariable is created on demand, and is deleted when the base
  //   variable is deleted. It holds a weak_ptr to the base variable.
  py::class_<BaseIntVar, PyBaseIntVar, std::shared_ptr<BaseIntVar>, Literal>(
      m, "BaseIntVar", DOC(operations_research, sat, python, BaseIntVar))
      .def(py::init<int>())        // Integer variable.
      .def(py::init<int, bool>())  // Potential Boolean variable.
      .def_property_readonly(
          "index", &BaseIntVar::index,
          DOC(operations_research, sat, python, BaseIntVar, index))
      .def_property_readonly(
          "is_boolean", &BaseIntVar::is_boolean,
          DOC(operations_research, sat, python, BaseIntVar, is_boolean))
      .def("__str__", &BaseIntVar::ToString)
      .def("__repr__", &BaseIntVar::DebugString)
      .def(
          "negated",
          [](std::shared_ptr<BaseIntVar> self) {
            if (!self->is_boolean()) {
              ThrowError(PyExc_TypeError,
                         "negated() is only supported for Boolean variables.");
            }
            return self->negated();
          },
          DOC(operations_research, sat, python, BaseIntVar, negated))
      .def(
          "__invert__",
          [](std::shared_ptr<BaseIntVar> self) {
            if (!self->is_boolean()) {
              ThrowError(PyExc_TypeError,
                         "negated() is only supported for Boolean variables.");
            }
            return self->negated();
          },
          DOC(operations_research, sat, python, BaseIntVar, negated))
      // PEP8 Compatibility.
      .def("Not",
           [](std::shared_ptr<BaseIntVar> self) {
             if (!self->is_boolean()) {
               ThrowError(PyExc_TypeError,
                          "negated() is only supported for Boolean variables.");
             }
             return self->negated();
           })
      .def("Index", &BaseIntVar::index);

  py::class_<NotBooleanVariable, std::shared_ptr<NotBooleanVariable>, Literal>(
      m, "NotBooleanVariable",
      DOC(operations_research, sat, python, NotBooleanVariable))
      .def_property_readonly(
          "index",
          [](std::shared_ptr<NotBooleanVariable> not_var) -> int {
            if (!not_var->ok()) {
              ThrowError(PyExc_ReferenceError,
                         "The base variable is not valid.");
            }
            return not_var->index();
          },
          DOC(operations_research, sat, python, NotBooleanVariable, index))
      .def("__str__",
           [](std::shared_ptr<NotBooleanVariable> not_var) -> std::string {
             if (!not_var->ok()) {
               ThrowError(PyExc_ReferenceError,
                          "The base variable is not valid.");
             }
             return not_var->ToString();
           })
      .def("__repr__",
           [](std::shared_ptr<NotBooleanVariable> not_var) -> std::string {
             if (!not_var->ok()) {
               ThrowError(PyExc_ReferenceError,
                          "The base variable is not valid.");
             }
             return not_var->DebugString();
           })
      .def(
          "negated",
          [](std::shared_ptr<NotBooleanVariable> not_var)
              -> std::shared_ptr<Literal> {
            if (!not_var->ok()) {
              ThrowError(PyExc_ReferenceError,
                         "The base variable is not valid.");
            }
            return not_var->negated();
          },
          DOC(operations_research, sat, python, NotBooleanVariable, negated))
      .def(
          "__invert__",
          [](std::shared_ptr<NotBooleanVariable> not_var)
              -> std::shared_ptr<Literal> {
            if (!not_var->ok()) {
              ThrowError(PyExc_ReferenceError,
                         "The base variable is not valid.");
            }
            return not_var->negated();
          },
          DOC(operations_research, sat, python, NotBooleanVariable, negated))
      .def(
          "Not",
          [](std::shared_ptr<NotBooleanVariable> not_var)
              -> std::shared_ptr<Literal> {
            if (!not_var->ok()) {
              ThrowError(PyExc_ReferenceError,
                         "The base variable is not valid.");
            }
            return not_var->negated();
          },
          DOC(operations_research, sat, python, NotBooleanVariable, negated));

  py::class_<BoundedLinearExpression, std::shared_ptr<BoundedLinearExpression>>(
      m, "BoundedLinearExpression",
      DOC(operations_research, sat, python, BoundedLinearExpression))
      .def(py::init<const std::shared_ptr<LinearExpr>, Domain>())
      .def(py::init<const std::shared_ptr<LinearExpr>,
                    const std::shared_ptr<LinearExpr>, Domain>())
      .def_property_readonly("bounds", &BoundedLinearExpression::bounds)
      .def_property_readonly("vars", &BoundedLinearExpression::vars)
      .def_property_readonly("coeffs", &BoundedLinearExpression::coeffs)
      .def_property_readonly("offset", &BoundedLinearExpression::offset)
      .def_property_readonly("ok", &BoundedLinearExpression::ok)
      .def("__str__", &BoundedLinearExpression::ToString)
      .def("__repr__", &BoundedLinearExpression::DebugString)
      .def("__bool__", [](const BoundedLinearExpression& self) {
        bool result;
        if (self.CastToBool(&result)) return result;
        ThrowError(PyExc_NotImplementedError,
                   absl::StrCat("Evaluating a BoundedLinearExpression '",
                                self.ToString(),
                                "'instance as a Boolean is "
                                "not supported."));
        return false;
      });
}  // NOLINT(readability/fn_size)

}  // namespace operations_research::sat::python
