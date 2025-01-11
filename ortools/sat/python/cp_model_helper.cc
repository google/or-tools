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
#include <limits>
#include <string>
#include <vector>

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

using ::py::arg;

void ThrowError(PyObject* py_exception, const std::string& message) {
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
      ThrowError(PyExc_TypeError,
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
BoundedLinearExpression* CheckBoundedLinearExpression(
    BoundedLinearExpression* result, LinearExpr* lhs,
    LinearExpr* rhs = nullptr) {
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

void RaiseIfNone(LinearExpr* expr) {
  if (expr == nullptr) {
    ThrowError(PyExc_TypeError,
               "Linear constraints do not accept None as argument.");
  }
}

void ProcessExprArg(const py::handle& arg, LinearExpr*& expr,
                    int64_t& int_value, double& float_value) {
  if (py::isinstance<LinearExpr>(arg)) {
    expr = arg.cast<LinearExpr*>();
  } else if (py::isinstance<py::int_>(arg)) {
    int_value = arg.cast<int64_t>();
  } else if (py::isinstance<py::float_>(arg)) {
    float_value = arg.cast<double>();
  } else if (hasattr(arg, "dtype") && hasattr(arg, "is_integer")) {
    if (getattr(arg, "is_integer")().cast<bool>()) {
      int_value = arg.cast<int64_t>();
    } else {
      float_value = arg.cast<double>();
    }
  } else {
    ThrowError(PyExc_TypeError,
               absl::StrCat("LinearExpr::sum() only accept linear "
                            "expressions and constants as argument: ",
                            arg.cast<std::string>()));
  }
}

void ProcessConstantArg(const py::handle& arg, int64_t& int_value,
                        double& float_value) {
  if (py::isinstance<py::int_>(arg)) {
    int_value = arg.cast<int64_t>();
  } else if (py::isinstance<py::float_>(arg)) {
    float_value = arg.cast<double>();
  } else if (hasattr(arg, "dtype") && hasattr(arg, "is_integer")) {
    if (getattr(arg, "is_integer")().cast<bool>()) {
      int_value = arg.cast<int64_t>();
    } else {
      float_value = arg.cast<double>();
    }
  } else {
    ThrowError(PyExc_TypeError,
               absl::StrCat("LinearExpr::weighted_sum() only accept  constants "
                            "as coefficients: ",
                            arg.cast<std::string>()));
  }
}

LinearExpr* SumArguments(py::args expressions) {
  std::vector<LinearExpr*> linear_exprs;
  int64_t int_offset = 0;
  double float_offset = 0.0;
  bool has_floats = false;

  const auto process_arg = [&](const py::handle& arg) -> void {
    int64_t int_value = 0;
    double float_value = 0.0;
    LinearExpr* expr = nullptr;
    ProcessExprArg(arg, expr, int_value, float_value);
    if (expr != nullptr) {
      linear_exprs.push_back(expr);
      return;
    } else if (int_value != 0) {
      int_offset += int_value;
      float_offset += static_cast<double>(int_value);
    } else if (float_value != 0.0) {
      float_offset += float_value;
      has_floats = true;
    }
  };

  if (expressions.size() == 0) {
    return new IntConstant(0);
  } else if (expressions.size() == 1 &&
             py::isinstance<py::sequence>(expressions[0])) {
    // Normal list or tuple argument.
    py::sequence elements = expressions[0].cast<py::sequence>();
    linear_exprs.reserve(elements.size());
    for (const py::handle& arg : elements) {
      process_arg(arg);
    }
  } else {  // Direct sum(x, y, 3, ..) without [].
    linear_exprs.reserve(expressions.size());
    for (const py::handle arg : expressions) {
      process_arg(arg);
    }
  }

  if (linear_exprs.empty()) {
    if (has_floats) {
      return new FloatConstant(float_offset);
    } else {
      return new IntConstant(int_offset);
    }
  } else if (linear_exprs.size() == 1) {
    if (has_floats) {
      if (float_offset == 0.0) {
        return linear_exprs[0];
      } else {
        return new FloatAffine(linear_exprs[0], 1.0, float_offset);
      }
    } else if (int_offset != 0) {
      return new IntAffine(linear_exprs[0], 1, int_offset);
    } else {
      return linear_exprs[0];
    }
  } else {
    if (has_floats) {
      return new SumArray(linear_exprs, 0, float_offset);
    } else {
      return new SumArray(linear_exprs, int_offset, 0.0);
    }
  }
}

LinearExpr* WeightedSumArguments(py::sequence expressions,
                                 py::sequence coefficients) {
  if (expressions.size() != coefficients.size()) {
    ThrowError(PyExc_ValueError,
               absl::StrCat("LinearExpr::weighted_sum() requires the same "
                            "number of arguments and coefficients: ",
                            expressions.size(), " != ", coefficients.size()));
  }

  std::vector<LinearExpr*> linear_exprs;
  std::vector<int64_t> int_coeffs;
  std::vector<double> float_coeffs;
  linear_exprs.reserve(expressions.size());
  int_coeffs.reserve(expressions.size());
  float_coeffs.reserve(expressions.size());
  int64_t int_offset = 0;
  double float_offset = 0.0;
  bool has_floats = false;

  for (int i = 0; i < expressions.size(); ++i) {
    py::handle arg = expressions[i];
    py::handle coeff = coefficients[i];
    LinearExpr* expr = nullptr;
    int64_t int_value = 0;
    double float_value = 0.0;
    int64_t int_mult = 0;
    double float_mult = 0.0;
    ProcessExprArg(arg, expr, int_value, float_value);
    ProcessConstantArg(coeff, int_mult, float_mult);
    has_floats |= float_mult != 0.0;
    has_floats |= float_value != 0.0;
    if (expr != nullptr && (int_mult != 0 || float_mult != 0.0)) {
      linear_exprs.push_back(expr);
      int_coeffs.push_back(int_mult);
      float_coeffs.push_back(static_cast<double>(int_mult) + float_mult);
      continue;
    } else if (int_value != 0) {
      int_offset += int_mult * int_value;
      float_offset += (float_mult + static_cast<double>(int_mult)) *
                      static_cast<double>(int_value);
    } else if (float_value != 0.0) {
      float_offset +=
          (float_mult + static_cast<double>(int_mult)) * float_value;
    }
  }

  if (linear_exprs.empty()) {
    if (has_floats) {
      return new FloatConstant(float_offset);
    } else {
      return new IntConstant(int_offset);
    }
  } else if (linear_exprs.size() == 1) {
    if (has_floats) {
      return new FloatAffine(linear_exprs[0], float_coeffs[0], float_offset);
    } else if (int_offset != 0 || int_coeffs[0] != 1) {
      return new IntAffine(linear_exprs[0], int_coeffs[0], int_offset);
    } else {
      return linear_exprs[0];
    }
  } else {
    if (has_floats) {
      return new FloatWeightedSum(linear_exprs, float_coeffs, float_offset);
    } else {
      return new IntWeightedSum(linear_exprs, int_coeffs, int_offset);
    }
  }
}

PYBIND11_MODULE(cp_model_helper, m) {
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
              ThrowError(PyExc_TypeError,
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

  py::class_<LinearExpr>(m, "LinearExpr",
                         DOC(operations_research, sat, python, LinearExpr))
      .def_static("sum", &SumArguments, py::return_value_policy::automatic,
                  "Returns the sum(expressions).", py::keep_alive<0, 1>())
      .def_static("weighted_sum", &WeightedSumArguments, arg("expressions"),
                  arg("coefficients"),
                  "Returns the sum of (expressions[i] * coefficients[i])",
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("term", &LinearExpr::TermInt, arg("expr").none(false),
                  arg("coeff"),
                  DOC(operations_research, sat, python, LinearExpr, TermInt),
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("term", &LinearExpr::TermFloat, arg("expr").none(false),
                  arg("coeff"),
                  DOC(operations_research, sat, python, LinearExpr, TermFloat),
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("affine", &LinearExpr::AffineInt, arg("expr").none(false),
                  arg("coeff"), arg("offset"),
                  DOC(operations_research, sat, python, LinearExpr, AffineInt),
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static(
          "affine", &LinearExpr::AffineFloat, arg("expr").none(false),
          arg("coeff"), arg("offset"),
          DOC(operations_research, sat, python, LinearExpr, AffineFloat),
          py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static(
          "constant", &LinearExpr::ConstantInt, arg("value"),
          DOC(operations_research, sat, python, LinearExpr, ConstantInt),
          py::return_value_policy::automatic)
      .def_static(
          "constant", &LinearExpr::ConstantFloat, arg("value"),
          DOC(operations_research, sat, python, LinearExpr, ConstantFloat),
          py::return_value_policy::automatic)
      // Pre PEP8 compatibility layer.
      .def_static("Sum", &SumArguments, py::return_value_policy::automatic,
                  py::keep_alive<0, 1>())
      .def_static("WeightedSum", &WeightedSumArguments, arg("expressions"),
                  arg("coefficients"), py::return_value_policy::automatic,
                  py::keep_alive<0, 1>())
      .def_static("Term", &LinearExpr::TermInt, arg("expr").none(false),
                  arg("coeff"), "Returns expr * coeff.",
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def_static("Term", &LinearExpr::TermFloat, arg("expr").none(false),
                  arg("coeff"), "Returns expr * coeff.",
                  py::return_value_policy::automatic, py::keep_alive<0, 1>())
      // Methods.
      .def("__str__", &LinearExpr::ToString)
      .def("__repr__", &LinearExpr::DebugString)
      .def("is_integer", &LinearExpr::IsInteger,
           DOC(operations_research, sat, python, LinearExpr, IsInteger))
      // Operators.
      // Note that we keep the 3 APIS (expr, int, double) instead of using
      // an ExprOrValue argument as this is more efficient.
      .def("__add__", &LinearExpr::Add, arg("other").none(false),
           py::return_value_policy::automatic, py::keep_alive<0, 1>(),
           DOC(operations_research, sat, python, LinearExpr, Add),
           py::keep_alive<0, 2>())
      .def("__add__", &LinearExpr::AddInt, arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, AddInt),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__add__", &LinearExpr::AddFloat, arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, AddFloat),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__radd__", &LinearExpr::AddInt, arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, AddInt),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__radd__", &LinearExpr::AddFloat, arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, AddInt),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__sub__", &LinearExpr::Sub, arg("other").none(false),
           DOC(operations_research, sat, python, LinearExpr, Sub),
           py::return_value_policy::automatic, py::keep_alive<0, 1>(),
           py::keep_alive<0, 2>())
      .def("__sub__", &LinearExpr::SubInt, arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, SubInt),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__sub__", &LinearExpr::SubFloat, arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, SubFloat),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__rsub__", &LinearExpr::RSubInt, arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, RSubInt),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__rsub__", &LinearExpr::RSubFloat, arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, RSubFloat),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__mul__", &LinearExpr::MulInt, arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, MulInt),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__mul__", &LinearExpr::MulFloat, arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, MulFloat),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__rmul__", &LinearExpr::MulInt, arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, MulInt),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__rmul__", &LinearExpr::MulFloat, arg("cst"),
           DOC(operations_research, sat, python, LinearExpr, MulFloat),
           py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def("__neg__", &LinearExpr::Neg, py::return_value_policy::automatic,
           DOC(operations_research, sat, python, LinearExpr, Neg),
           py::keep_alive<0, 1>())
      .def(
          "__eq__",
          [](LinearExpr* lhs, LinearExpr* rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Eq(rhs), lhs, rhs);
          },
          DOC(operations_research, sat, python, LinearExpr, Eq),
          py::return_value_policy::automatic, py::keep_alive<0, 1>(),
          py::keep_alive<0, 2>())
      .def(
          "__eq__",
          [](LinearExpr* lhs, int64_t rhs) {
            if (rhs == std::numeric_limits<int64_t>::max() ||
                rhs == std::numeric_limits<int64_t>::min()) {
              ThrowError(PyExc_ArithmeticError,
                         "== INT_MIN or INT_MAX is not supported");
            }
            return CheckBoundedLinearExpression(lhs->EqCst(rhs), lhs);
          },
          DOC(operations_research, sat, python, LinearExpr, EqCst),
          py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def(
          "__ne__",
          [](LinearExpr* lhs, LinearExpr* rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Ne(rhs), lhs, rhs);
          },
          DOC(operations_research, sat, python, LinearExpr, Ne),
          py::return_value_policy::automatic, py::keep_alive<0, 1>(),
          py::keep_alive<0, 2>())
      .def(
          "__ne__",
          [](LinearExpr* lhs, int64_t rhs) {
            return CheckBoundedLinearExpression(lhs->NeCst(rhs), lhs);
          },
          DOC(operations_research, sat, python, LinearExpr, NeCst),
          py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def(
          "__le__",
          [](LinearExpr* lhs, LinearExpr* rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Le(rhs), lhs, rhs);
          },
          DOC(operations_research, sat, python, LinearExpr, Le),
          py::return_value_policy::automatic, py::keep_alive<0, 1>(),
          py::keep_alive<0, 2>())
      .def(
          "__le__",
          [](LinearExpr* lhs, int64_t rhs) {
            if (rhs == std::numeric_limits<int64_t>::min()) {
              ThrowError(PyExc_ArithmeticError, "<= INT_MIN is not supported");
            }
            return CheckBoundedLinearExpression(lhs->LeCst(rhs), lhs);
          },
          DOC(operations_research, sat, python, LinearExpr, LeCst),
          py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def(
          "__lt__",
          [](LinearExpr* lhs, LinearExpr* rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Lt(rhs), lhs, rhs);
          },
          DOC(operations_research, sat, python, LinearExpr, Lt),
          py::return_value_policy::automatic, py::keep_alive<0, 1>(),
          py::keep_alive<0, 2>())
      .def(
          "__lt__",
          [](LinearExpr* lhs, int64_t rhs) {
            if (rhs == std::numeric_limits<int64_t>::min()) {
              ThrowError(PyExc_ArithmeticError, "< INT_MIN is not supported");
            }
            return CheckBoundedLinearExpression(lhs->LtCst(rhs), lhs);
          },
          DOC(operations_research, sat, python, LinearExpr, LtCst),
          py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def(
          "__ge__",
          [](LinearExpr* lhs, LinearExpr* rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Ge(rhs), lhs, rhs);
          },
          DOC(operations_research, sat, python, LinearExpr, Ge),
          py::return_value_policy::automatic, py::keep_alive<0, 1>(),
          py::keep_alive<0, 2>())
      .def(
          "__ge__",
          [](LinearExpr* lhs, int64_t rhs) {
            if (rhs == std::numeric_limits<int64_t>::max()) {
              ThrowError(PyExc_ArithmeticError, ">= INT_MAX is not supported");
            }
            return CheckBoundedLinearExpression(lhs->GeCst(rhs), lhs);
          },
          DOC(operations_research, sat, python, LinearExpr, GeCst),
          py::return_value_policy::automatic, py::keep_alive<0, 1>())
      .def(
          "__gt__",
          [](LinearExpr* lhs, LinearExpr* rhs) {
            RaiseIfNone(rhs);
            return CheckBoundedLinearExpression(lhs->Gt(rhs), lhs, rhs);
          },
          DOC(operations_research, sat, python, LinearExpr, Gt),
          py::return_value_policy::automatic, py::keep_alive<0, 1>(),
          py::keep_alive<0, 2>())
      .def(
          "__gt__",
          [](LinearExpr* lhs, int64_t rhs) {
            if (rhs == std::numeric_limits<int64_t>::max()) {
              ThrowError(PyExc_ArithmeticError, "> INT_MAX is not supported");
            }
            return CheckBoundedLinearExpression(lhs->GtCst(rhs), lhs);
          },
          DOC(operations_research, sat, python, LinearExpr, GtCst),
          py::return_value_policy::automatic, py::keep_alive<0, 1>())
      // Disable other operators as they are not supported.
      .def("__div__",
           [](LinearExpr* /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling / on a linear expression is not supported, "
                        "please use CpModel.add_division_equality");
           })
      .def("__truediv__",
           [](LinearExpr* /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling // on a linear expression is not supported, "
                        "please use CpModel.add_division_equality");
           })
      .def("__mod__",
           [](LinearExpr* /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling %% on a linear expression is not supported, "
                        "please use CpModel.add_modulo_equality");
           })
      .def("__pow__",
           [](LinearExpr* /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling ** on a linear expression is not supported, "
                        "please use CpModel.add_multiplication_equality");
           })
      .def("__lshift__",
           [](LinearExpr* /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling left shift on a linear expression is not "
                        "supported");
           })
      .def("__rshift__",
           [](LinearExpr* /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling right shift on a linear expression is "
                        "not supported");
           })
      .def("__and__",
           [](LinearExpr* /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling and on a linear expression is not supported");
           })
      .def("__or__",
           [](LinearExpr* /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling or on a linear expression is not supported");
           })
      .def("__xor__",
           [](LinearExpr* /*self*/, py::handle /*other*/) {
             ThrowError(PyExc_NotImplementedError,
                        "calling xor on a linear expression is not supported");
           })
      .def("__abs__",
           [](LinearExpr* /*self*/) {
             ThrowError(
                 PyExc_NotImplementedError,
                 "calling abs() on a linear expression is not supported, "
                 "please use CpModel.add_abs_equality");
           })
      .def("__bool__", [](LinearExpr* /*self*/) {
        ThrowError(PyExc_NotImplementedError,
                   "Evaluating a LinearExpr instance as a Boolean is "
                   "not supported.");
      });

  // Expose Internal classes, mostly for testing.
  py::class_<FlatFloatExpr>(
      m, "FlatFloatExpr", DOC(operations_research, sat, python, FlatFloatExpr))
      .def(py::init<LinearExpr*>(), py::keep_alive<1, 2>())
      .def_property_readonly("vars", &FlatFloatExpr::vars)
      .def_property_readonly("coeffs", &FlatFloatExpr::coeffs)
      .def_property_readonly("offset", &FlatFloatExpr::offset);

  py::class_<FlatIntExpr>(m, "FlatIntExpr",
                          DOC(operations_research, sat, python, FlatIntExpr))
      .def(py::init([](LinearExpr* expr) {
             FlatIntExpr* result = new FlatIntExpr(expr);
             if (!result->ok()) {
               ThrowError(
                   PyExc_TypeError,
                   absl::StrCat("Tried to build a FlatIntExpr from a linear "
                                "expression with "
                                "floating point coefficients or constants:  ",
                                expr->DebugString()));
             }
             return result;
           }),
           py::keep_alive<1, 2>())
      .def_property_readonly("vars", &FlatIntExpr::vars)
      .def_property_readonly("coeffs", &FlatIntExpr::coeffs)
      .def_property_readonly("offset", &FlatIntExpr::offset)
      .def_property_readonly("ok", &FlatIntExpr::ok);

  py::class_<FloatAffine, LinearExpr>(
      m, "FloatAffine", DOC(operations_research, sat, python, FloatAffine))
      .def(py::init<LinearExpr*, double, double>(), py::keep_alive<1, 2>())
      .def_property_readonly("expression", &FloatAffine::expression)
      .def_property_readonly("coefficient", &FloatAffine::coefficient)
      .def_property_readonly("offset", &FloatAffine::offset);

  py::class_<IntAffine, LinearExpr>(
      m, "IntAffine", DOC(operations_research, sat, python, IntAffine))
      .def(py::init<LinearExpr*, int64_t, int64_t>(), py::keep_alive<1, 2>())
      .def_property_readonly("expression", &IntAffine::expression)
      .def_property_readonly("coefficient", &IntAffine::coefficient)
      .def_property_readonly("offset", &IntAffine::offset);

  py::class_<Literal, LinearExpr>(
      m, "Literal", DOC(operations_research, sat, python, Literal))
      .def_property_readonly(
          "index", &Literal::index,
          DOC(operations_research, sat, python, Literal, index))
      .def("negated", &Literal::negated,
           DOC(operations_research, sat, python, Literal, negated))
      .def("__invert__", &Literal::negated,
           DOC(operations_research, sat, python, Literal, negated))
      .def("__bool__",
           [](Literal* /*self*/) {
             ThrowError(PyExc_NotImplementedError,
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
  py::class_<BaseIntVar, PyBaseIntVar, Literal>(
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
          [](BaseIntVar* self) {
            if (!self->is_boolean()) {
              ThrowError(PyExc_TypeError,
                         "negated() is only supported for Boolean variables.");
            }
            return self->negated();
          },
          DOC(operations_research, sat, python, BaseIntVar, negated),
          py::return_value_policy::reference_internal)
      .def(
          "__invert__",
          [](BaseIntVar* self) {
            if (!self->is_boolean()) {
              ThrowError(PyExc_TypeError,
                         "negated() is only supported for Boolean variables.");
            }
            return self->negated();
          },
          DOC(operations_research, sat, python, BaseIntVar, negated),
          py::return_value_policy::reference_internal)
      // PEP8 Compatibility.
      .def(
          "Not",
          [](BaseIntVar* self) {
            if (!self->is_boolean()) {
              ThrowError(PyExc_TypeError,
                         "negated() is only supported for Boolean variables.");
            }
            return self->negated();
          },
          py::return_value_policy::reference_internal);

  // Memory management:
  // - Do we need a reference_internal (that add a py::keep_alive<1, 0>() rule)
  //   or just a reference ?
  py::class_<NotBooleanVariable, Literal>(
      m, "NotBooleanVariable",
      DOC(operations_research, sat, python, NotBooleanVariable))
      .def_property_readonly(
          "index", &NotBooleanVariable::index,
          DOC(operations_research, sat, python, NotBooleanVariable, index))
      .def("__str__", &NotBooleanVariable::ToString)
      .def("__repr__", &NotBooleanVariable::DebugString)
      .def("negated", &NotBooleanVariable::negated,
           DOC(operations_research, sat, python, NotBooleanVariable, negated),
           py::return_value_policy::reference_internal)
      .def("__invert__", &NotBooleanVariable::negated,
           DOC(operations_research, sat, python, NotBooleanVariable, negated),
           py::return_value_policy::reference_internal)
      .def("Not", &NotBooleanVariable::negated,
           "Returns the negation of the current Boolean variable.",
           py::return_value_policy::reference_internal);

  py::class_<BoundedLinearExpression>(
      m, "BoundedLinearExpression",
      DOC(operations_research, sat, python, BoundedLinearExpression))
      .def(py::init<const LinearExpr*, Domain>(), py::keep_alive<1, 2>())
      .def(py::init<const LinearExpr*, const LinearExpr*, Domain>(),
           py::keep_alive<1, 2>(), py::keep_alive<1, 3>())
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
