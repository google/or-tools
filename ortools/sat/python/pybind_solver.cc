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

#include "ortools/sat/python/pybind_solver.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/python/linear_expr.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/swig_helper.h"
#include "pybind11/functional.h"
#include "pybind11/gil.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace operations_research::sat::python {

namespace {

namespace py = pybind11;

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
    py::gil_scoped_acquire acquire;
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

class ResponseHelper {
 public:
  static bool BooleanValue(std::shared_ptr<CpSolverResponse> response,
                           std::shared_ptr<Literal> lit) {
    const int index = lit->index();
    if (index >= 0) {
      return response->solution(index) != 0;
    } else {
      return response->solution(NegatedRef(index)) == 0;
    }
  }

  static bool FixedBooleanValue(std::shared_ptr<CpSolverResponse>, bool lit) {
    return lit;
  }

  static std::vector<int> SufficientAssumptionsForInfeasibility(
      std::shared_ptr<CpSolverResponse> response) {
    return std::vector<int>(
        response->sufficient_assumptions_for_infeasibility().begin(),
        response->sufficient_assumptions_for_infeasibility().end());
  }

  static double FloatValue(std::shared_ptr<CpSolverResponse> response,
                           std::shared_ptr<LinearExpr> expr) {
    FloatExprVisitor visitor;
    visitor.AddToProcess(expr, 1);
    return visitor.Evaluate(*response);
  }

  static double FixedFloatValue(std::shared_ptr<CpSolverResponse>,
                                double value) {
    return value;
  }

  static int64_t Value(std::shared_ptr<CpSolverResponse> response,
                       std::shared_ptr<LinearExpr> expr) {
    int64_t value;
    IntExprVisitor visitor;
    visitor.AddToProcess(expr, 1);
    if (!visitor.Evaluate(*response, &value)) {
      ThrowError(PyExc_ValueError,
                 absl::StrCat("Failed to evaluate linear expression: ",
                              expr->DebugString()));
    }
    return value;
  }

  static int64_t FixedValue(std::shared_ptr<CpSolverResponse>, int64_t value) {
    return value;
  }
};

}  // namespace

void DefinePybindWrapperForSolver(py::module& m) {
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
      .def("Response", &SolutionCallback::SharedResponse)
      .def("SolutionBooleanValue", &SolutionCallback::SolutionBooleanValue,
           py::arg("index"))
      .def("SolutionIntegerValue", &SolutionCallback::SolutionIntegerValue,
           py::arg("index"))
      .def("StopSearch", &SolutionCallback::StopSearch)
      .def("UserTime", &SolutionCallback::UserTime)
      .def("WallTime", &SolutionCallback::WallTime)
      .def(
          "Value",
          [](const SolutionCallback& self, std::shared_ptr<LinearExpr> expr) {
            return ResponseHelper::Value(self.SharedResponse(), expr);
          },
          "Returns the value of a linear expression after solve.")
      .def(
          "Value", [](const SolutionCallback&, int64_t value) { return value; },
          "Returns the value of a linear expression after solve.")
      .def(
          "FloatValue",
          [](const SolutionCallback& self, std::shared_ptr<LinearExpr> expr) {
            return ResponseHelper::FloatValue(self.SharedResponse(), expr);
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
          [](const SolutionCallback& self, std::shared_ptr<Literal> lit) {
            return ResponseHelper::BooleanValue(self.SharedResponse(), lit);
          },
          "Returns the Boolean value of a literal after solve.")
      .def(
          "BooleanValue", [](const SolutionCallback&, bool lit) { return lit; },
          "Returns the Boolean value of a literal after solve.");

  py::class_<ResponseHelper>(m, "ResponseHelper")
      .def_static("boolean_value", &ResponseHelper::BooleanValue,
                  py::arg("response").none(false), py::arg("lit").none(false))
      .def_static("boolean_value", &ResponseHelper::FixedBooleanValue,
                  py::arg("response").none(false), py::arg("lit").none(false))
      .def_static("float_value", &ResponseHelper::FloatValue,
                  py::arg("response").none(false), py::arg("expr").none(false))
      .def_static("float_value", &ResponseHelper::FixedFloatValue,
                  py::arg("response").none(false), py::arg("value").none(false))
      .def_static("sufficient_assumptions_for_infeasibility",
                  &ResponseHelper::SufficientAssumptionsForInfeasibility,
                  py::arg("response").none(false))
      .def_static("value", &ResponseHelper::Value,
                  py::arg("response").none(false), py::arg("expr").none(false))
      .def_static("value", &ResponseHelper::FixedValue,
                  py::arg("response").none(false),
                  py::arg("value").none(false));

  py::class_<ExtSolveWrapper>(m, "SolveWrapper")
      .def(py::init<>())
      .def(
          "add_log_callback",
          [](ExtSolveWrapper* solve_wrapper,
             std::function<void(const std::string&)> log_callback) {
            std::function<void(const std::string&)> safe_log_callback =
                [solve_wrapper, log_callback](std::string message) -> void {
              py::gil_scoped_acquire acquire;
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
              py::gil_scoped_acquire acquire;
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
      .def(
          "set_parameters",
          [](ExtSolveWrapper* solve_wrapper,
             std::shared_ptr<SatParameters> parameters) {
            solve_wrapper->SetParameters(*parameters);
          },
          py::arg("parameters").none(false))
      .def(
          "solve",
          [](ExtSolveWrapper* solve_wrapper,
             std::shared_ptr<CpModelProto> model_proto) -> CpSolverResponse {
            const auto result = [=]() -> CpSolverResponse {
              py::gil_scoped_release release;
              return solve_wrapper->Solve(*model_proto);
            }();
            if (solve_wrapper->local_error_already_set_.has_value()) {
              solve_wrapper->local_error_already_set_->restore();
              solve_wrapper->local_error_already_set_.reset();
              throw py::error_already_set();
            }
            return result;
          },
          py::arg("model_proto").none(false))
      .def("stop_search", &SolveWrapper::StopSearch);

  py::class_<CpSatHelper>(m, "CpSatHelper")
      .def_static("model_stats", &CpSatHelper::ModelStats,
                  py::arg("model_proto"))
      .def_static("solver_response_stats", &CpSatHelper::SolverResponseStats,
                  py::arg("response"))
      .def_static("validate_model", &CpSatHelper::ValidateModel,
                  py::arg("model_proto"))
      .def_static("write_model_to_file", &CpSatHelper::WriteModelToFile,
                  py::arg("model_proto"), py::arg("filename"));
}

}  // namespace operations_research::sat::python
