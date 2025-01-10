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

// A pybind11 wrapper for model_builder_helper.

#include "ortools/linear_solver/wrappers/model_builder_helper.h"

#include <algorithm>
#include <complex>
#include <cstdlib>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ortools/base/logging.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_exporter.h"
#include "pybind11/eigen.h"
#include "pybind11/pybind11.h"
#include "pybind11/pytypes.h"
#include "pybind11/stl.h"
#include "pybind11_protobuf/native_proto_caster.h"

using ::Eigen::SparseMatrix;
using ::Eigen::VectorXd;
using ::operations_research::ModelBuilderHelper;
using ::operations_research::ModelSolverHelper;
using ::operations_research::MPConstraintProto;
using ::operations_research::MPModelExportOptions;
using ::operations_research::MPModelProto;
using ::operations_research::MPModelRequest;
using ::operations_research::MPSolutionResponse;
using ::operations_research::MPVariableProto;
using ::operations_research::SolveStatus;

namespace py = pybind11;
using ::py::arg;

const MPModelProto& ToMPModelProto(ModelBuilderHelper* helper) {
  return helper->model();
}

// TODO(user): The interface uses serialized protos because of issues building
// pybind11_protobuf. See
// https://github.com/protocolbuffers/protobuf/issues/9464. After
// pybind11_protobuf is working, this workaround can be removed.
void BuildModelFromSparseData(
    const Eigen::Ref<const VectorXd>& variable_lower_bounds,
    const Eigen::Ref<const VectorXd>& variable_upper_bounds,
    const Eigen::Ref<const VectorXd>& objective_coefficients,
    const Eigen::Ref<const VectorXd>& constraint_lower_bounds,
    const Eigen::Ref<const VectorXd>& constraint_upper_bounds,
    const SparseMatrix<double, Eigen::RowMajor>& constraint_matrix,
    MPModelProto* model_proto) {
  const int num_variables = variable_lower_bounds.size();
  const int num_constraints = constraint_lower_bounds.size();

  if (variable_upper_bounds.size() != num_variables) {
    throw std::invalid_argument(
        absl::StrCat("Invalid size ", variable_upper_bounds.size(),
                     " for variable_upper_bounds. Expected: ", num_variables));
  }
  if (objective_coefficients.size() != num_variables) {
    throw std::invalid_argument(absl::StrCat(
        "Invalid size ", objective_coefficients.size(),
        " for linear_objective_coefficients. Expected: ", num_variables));
  }
  if (constraint_upper_bounds.size() != num_constraints) {
    throw std::invalid_argument(absl::StrCat(
        "Invalid size ", constraint_upper_bounds.size(),
        " for constraint_upper_bounds. Expected: ", num_constraints));
  }
  if (constraint_matrix.cols() != num_variables) {
    throw std::invalid_argument(
        absl::StrCat("Invalid number of columns ", constraint_matrix.cols(),
                     " in constraint_matrix. Expected: ", num_variables));
  }
  if (constraint_matrix.rows() != num_constraints) {
    throw std::invalid_argument(
        absl::StrCat("Invalid number of rows ", constraint_matrix.rows(),
                     " in constraint_matrix. Expected: ", num_constraints));
  }

  for (int i = 0; i < num_variables; ++i) {
    MPVariableProto* variable = model_proto->add_variable();
    variable->set_lower_bound(variable_lower_bounds[i]);
    variable->set_upper_bound(variable_upper_bounds[i]);
    variable->set_objective_coefficient(objective_coefficients[i]);
  }

  for (int row = 0; row < num_constraints; ++row) {
    MPConstraintProto* constraint = model_proto->add_constraint();
    constraint->set_lower_bound(constraint_lower_bounds[row]);
    constraint->set_upper_bound(constraint_upper_bounds[row]);
    for (SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(
             constraint_matrix, row);
         it; ++it) {
      constraint->add_coefficient(it.value());
      constraint->add_var_index(it.col());
    }
  }
}

std::vector<std::pair<int, double>> SortedGroupedTerms(
    absl::Span<const int> indices, absl::Span<const double> coefficients) {
  CHECK_EQ(indices.size(), coefficients.size());
  std::vector<std::pair<int, double>> terms;
  terms.reserve(indices.size());
  for (int i = 0; i < indices.size(); ++i) {
    terms.emplace_back(indices[i], coefficients[i]);
  }
  std::sort(
      terms.begin(), terms.end(),
      [](const std::pair<int, double>& a, const std::pair<int, double>& b) {
        if (a.first != b.first) return a.first < b.first;
        if (std::abs(a.second) != std::abs(b.second)) {
          return std::abs(a.second) < std::abs(b.second);
        }
        return a.second < b.second;
      });
  int pos = 0;
  for (int i = 0; i < terms.size(); ++i) {
    const int var = terms[i].first;
    double coeff = terms[i].second;
    while (i + 1 < terms.size() && terms[i + 1].first == var) {
      coeff += terms[i + 1].second;
      ++i;
    }
    if (coeff == 0.0) continue;
    terms[pos] = {var, coeff};
    ++pos;
  }
  terms.resize(pos);
  return terms;
}

PYBIND11_MODULE(model_builder_helper, m) {
  pybind11_protobuf::ImportNativeProtoCasters();

  m.def("to_mpmodel_proto", &ToMPModelProto, arg("helper"));

  py::class_<MPModelExportOptions>(m, "MPModelExportOptions")
      .def(py::init<>())
      .def_readwrite("obfuscate", &MPModelExportOptions::obfuscate)
      .def_readwrite("log_invalid_names",
                     &MPModelExportOptions::log_invalid_names)
      .def_readwrite("show_unused_variables",
                     &MPModelExportOptions::show_unused_variables)
      .def_readwrite("max_line_length", &MPModelExportOptions::max_line_length);

  py::class_<ModelBuilderHelper>(m, "ModelBuilderHelper")
      .def(py::init<>())
      .def("overwrite_model", &ModelBuilderHelper::OverwriteModel,
           arg("other_helper"))
      .def("export_to_mps_string", &ModelBuilderHelper::ExportToMpsString,
           arg("options") = MPModelExportOptions())
      .def("export_to_lp_string", &ModelBuilderHelper::ExportToLpString,
           arg("options") = MPModelExportOptions())
      .def("write_to_mps_file", &ModelBuilderHelper::WriteToMpsFile,
           arg("filename"), arg("options") = MPModelExportOptions())
      .def("read_model_from_proto_file",
           &ModelBuilderHelper::ReadModelFromProtoFile, arg("filename"))
      .def("write_model_to_proto_file",
           &ModelBuilderHelper::WriteModelToProtoFile, arg("filename"))
      .def("import_from_mps_string", &ModelBuilderHelper::ImportFromMpsString,
           arg("mps_string"))
      .def("import_from_mps_file", &ModelBuilderHelper::ImportFromMpsFile,
           arg("mps_file"))
#if defined(USE_LP_PARSER)
      .def("import_from_lp_string", &ModelBuilderHelper::ImportFromLpString,
           arg("lp_string"))
      .def("import_from_lp_file", &ModelBuilderHelper::ImportFromLpFile,
           arg("lp_file"))
#else
            .def("import_from_lp_string", [](const std::string& lp_string) {
              LOG(INFO) << "Parsing LP string is not compiled in";
            })
            .def("import_from_lp_file", [](const std::string& lp_file) {
              LOG(INFO) << "Parsing LP file is not compiled in";
            })
#endif
      .def(
          "fill_model_from_sparse_data",
          [](ModelBuilderHelper* helper,
             const Eigen::Ref<const VectorXd>& variable_lower_bounds,
             const Eigen::Ref<const VectorXd>& variable_upper_bounds,
             const Eigen::Ref<const VectorXd>& objective_coefficients,
             const Eigen::Ref<const VectorXd>& constraint_lower_bounds,
             const Eigen::Ref<const VectorXd>& constraint_upper_bounds,
             const SparseMatrix<double, Eigen::RowMajor>& constraint_matrix) {
            BuildModelFromSparseData(
                variable_lower_bounds, variable_upper_bounds,
                objective_coefficients, constraint_lower_bounds,
                constraint_upper_bounds, constraint_matrix,
                helper->mutable_model());
          },
          arg("variable_lower_bound"), arg("variable_upper_bound"),
          arg("objective_coefficients"), arg("constraint_lower_bounds"),
          arg("constraint_upper_bounds"), arg("constraint_matrix"))
      .def("add_var", &ModelBuilderHelper::AddVar)
      .def("add_var_array",
           [](ModelBuilderHelper* helper, std::vector<size_t> shape, double lb,
              double ub, bool is_integral, absl::string_view name_prefix) {
             int size = shape[0];
             for (int i = 1; i < shape.size(); ++i) {
               size *= shape[i];
             }
             py::array_t<int> result(size);
             py::buffer_info info = result.request();
             result.resize(shape);
             auto ptr = static_cast<int*>(info.ptr);
             for (int i = 0; i < size; ++i) {
               const int index = helper->AddVar();
               ptr[i] = index;
               helper->SetVarLowerBound(index, lb);
               helper->SetVarUpperBound(index, ub);
               helper->SetVarIntegrality(index, is_integral);
               if (!name_prefix.empty()) {
                 helper->SetVarName(index, absl::StrCat(name_prefix, i));
               }
             }
             return result;
           })
      .def("add_var_array_with_bounds",
           [](ModelBuilderHelper* helper, py::array_t<double> lbs,
              py::array_t<double> ubs, py::array_t<bool> are_integral,
              absl::string_view name_prefix) {
             py::buffer_info buf_lbs = lbs.request();
             py::buffer_info buf_ubs = ubs.request();
             py::buffer_info buf_are_integral = are_integral.request();
             const int size = buf_lbs.size;
             if (size != buf_ubs.size || size != buf_are_integral.size) {
               throw std::runtime_error("Input sizes must match");
             }
             const auto shape = buf_lbs.shape;
             if (shape != buf_ubs.shape || shape != buf_are_integral.shape) {
               throw std::runtime_error("Input shapes must match");
             }

             auto lower_bounds = static_cast<double*>(buf_lbs.ptr);
             auto upper_bounds = static_cast<double*>(buf_ubs.ptr);
             auto integers = static_cast<bool*>(buf_are_integral.ptr);
             py::array_t<int> result(size);
             result.resize(shape);
             py::buffer_info result_info = result.request();
             auto ptr = static_cast<int*>(result_info.ptr);
             for (int i = 0; i < size; ++i) {
               const int index = helper->AddVar();
               ptr[i] = index;
               helper->SetVarLowerBound(index, lower_bounds[i]);
               helper->SetVarUpperBound(index, upper_bounds[i]);
               helper->SetVarIntegrality(index, integers[i]);
               if (!name_prefix.empty()) {
                 helper->SetVarName(index, absl::StrCat(name_prefix, i));
               }
             }
             return result;
           })
      .def("set_var_lower_bound", &ModelBuilderHelper::SetVarLowerBound,
           arg("var_index"), arg("lb"))
      .def("set_var_upper_bound", &ModelBuilderHelper::SetVarUpperBound,
           arg("var_index"), arg("ub"))
      .def("set_var_integrality", &ModelBuilderHelper::SetVarIntegrality,
           arg("var_index"), arg("is_integer"))
      .def("set_var_objective_coefficient",
           &ModelBuilderHelper::SetVarObjectiveCoefficient, arg("var_index"),
           arg("coeff"))
      .def("set_objective_coefficients",
           [](ModelBuilderHelper* helper, const std::vector<int>& indices,
              const std::vector<double>& coefficients) {
             for (const auto& [i, c] :
                  SortedGroupedTerms(indices, coefficients)) {
               helper->SetVarObjectiveCoefficient(i, c);
             }
           })
      .def("set_var_name", &ModelBuilderHelper::SetVarName, arg("var_index"),
           arg("name"))
      .def("var_lower_bound", &ModelBuilderHelper::VarLowerBound,
           arg("var_index"))
      .def("var_upper_bound", &ModelBuilderHelper::VarUpperBound,
           arg("var_index"))
      .def("var_is_integral", &ModelBuilderHelper::VarIsIntegral,
           arg("var_index"))
      .def("var_objective_coefficient",
           &ModelBuilderHelper::VarObjectiveCoefficient, arg("var_index"))
      .def("var_name", &ModelBuilderHelper::VarName, arg("var_index"))
      .def("add_linear_constraint", &ModelBuilderHelper::AddLinearConstraint)
      .def("set_constraint_lower_bound",
           &ModelBuilderHelper::SetConstraintLowerBound, arg("ct_index"),
           arg("lb"))
      .def("set_constraint_upper_bound",
           &ModelBuilderHelper::SetConstraintUpperBound, arg("ct_index"),
           arg("ub"))
      .def("add_term_to_constraint", &ModelBuilderHelper::AddConstraintTerm,
           arg("ct_index"), arg("var_index"), arg("coeff"))
      .def("add_terms_to_constraint",
           [](ModelBuilderHelper* helper, int ct_index,
              const std::vector<int>& indices,
              const std::vector<double>& coefficients) {
             for (const auto& [i, c] :
                  SortedGroupedTerms(indices, coefficients)) {
               helper->AddConstraintTerm(ct_index, i, c);
             }
           })
      .def("safe_add_term_to_constraint",
           &ModelBuilderHelper::SafeAddConstraintTerm, arg("ct_index"),
           arg("var_index"), arg("coeff"))
      .def("set_constraint_name", &ModelBuilderHelper::SetConstraintName,
           arg("ct_index"), arg("name"))
      .def("set_constraint_coefficient",
           &ModelBuilderHelper::SetConstraintCoefficient, arg("ct_index"),
           arg("var_index"), arg("coeff"))
      .def("constraint_lower_bound", &ModelBuilderHelper::ConstraintLowerBound,
           arg("ct_index"))
      .def("constraint_upper_bound", &ModelBuilderHelper::ConstraintUpperBound,
           arg("ct_index"))
      .def("constraint_name", &ModelBuilderHelper::ConstraintName,
           arg("ct_index"))
      .def("constraint_var_indices", &ModelBuilderHelper::ConstraintVarIndices,
           arg("ct_index"))
      .def("constraint_coefficients",
           &ModelBuilderHelper::ConstraintCoefficients, arg("ct_index"))
      .def("add_enforced_linear_constraint",
           &ModelBuilderHelper::AddEnforcedLinearConstraint)
      .def("is_enforced_linear_constraint",
           &ModelBuilderHelper::IsEnforcedConstraint)
      .def("set_enforced_constraint_lower_bound",
           &ModelBuilderHelper::SetEnforcedConstraintLowerBound,
           arg("ct_index"), arg("lb"))
      .def("set_enforced_constraint_upper_bound",
           &ModelBuilderHelper::SetEnforcedConstraintUpperBound,
           arg("ct_index"), arg("ub"))
      .def("add_term_to_enforced_constraint",
           &ModelBuilderHelper::AddEnforcedConstraintTerm, arg("ct_index"),
           arg("var_index"), arg("coeff"))
      .def("add_terms_to_enforced_constraint",
           [](ModelBuilderHelper* helper, int ct_index,
              const std::vector<int>& indices,
              const std::vector<double>& coefficients) {
             for (const auto& [i, c] :
                  SortedGroupedTerms(indices, coefficients)) {
               helper->AddEnforcedConstraintTerm(ct_index, i, c);
             }
           })
      .def("safe_add_term_to_enforced_constraint",
           &ModelBuilderHelper::SafeAddEnforcedConstraintTerm, arg("ct_index"),
           arg("var_index"), arg("coeff"))
      .def("set_enforced_constraint_name",
           &ModelBuilderHelper::SetEnforcedConstraintName, arg("ct_index"),
           arg("name"))
      .def("set_enforced_constraint_coefficient",
           &ModelBuilderHelper::SetEnforcedConstraintCoefficient,
           arg("ct_index"), arg("var_index"), arg("coeff"))
      .def("enforced_constraint_lower_bound",
           &ModelBuilderHelper::EnforcedConstraintLowerBound, arg("ct_index"))
      .def("enforced_constraint_upper_bound",
           &ModelBuilderHelper::EnforcedConstraintUpperBound, arg("ct_index"))
      .def("enforced_constraint_name",
           &ModelBuilderHelper::EnforcedConstraintName, arg("ct_index"))
      .def("enforced_constraint_var_indices",
           &ModelBuilderHelper::EnforcedConstraintVarIndices, arg("ct_index"))
      .def("enforced_constraint_coefficients",
           &ModelBuilderHelper::EnforcedConstraintCoefficients, arg("ct_index"))
      .def("set_enforced_constraint_indicator_variable_index",
           &ModelBuilderHelper::SetEnforcedIndicatorVariableIndex,
           arg("ct_index"), arg("var_index"))
      .def("set_enforced_constraint_indicator_value",
           &ModelBuilderHelper::SetEnforcedIndicatorValue, arg("ct_index"),
           arg("positive"))
      .def("enforced_constraint_indicator_variable_index",
           &ModelBuilderHelper::EnforcedIndicatorVariableIndex, arg("ct_index"))
      .def("enforced_constraint_indicator_value",
           &ModelBuilderHelper::EnforcedIndicatorValue, arg("ct_index"))
      .def("num_variables", &ModelBuilderHelper::num_variables)
      .def("num_constraints", &ModelBuilderHelper::num_constraints)
      .def("name", &ModelBuilderHelper::name)
      .def("set_name", &ModelBuilderHelper::SetName, arg("name"))
      .def("clear_objective", &ModelBuilderHelper::ClearObjective)
      .def("maximize", &ModelBuilderHelper::maximize)
      .def("set_maximize", &ModelBuilderHelper::SetMaximize, arg("maximize"))
      .def("set_objective_offset", &ModelBuilderHelper::SetObjectiveOffset,
           arg("offset"))
      .def("objective_offset", &ModelBuilderHelper::ObjectiveOffset)
      .def("clear_hints", &ModelBuilderHelper::ClearHints)
      .def("add_hint", &ModelBuilderHelper::AddHint, arg("var_index"),
           arg("var_value"))
      .def("sort_and_regroup_terms",
           [](ModelBuilderHelper* helper, py::array_t<int> indices,
              py::array_t<double> coefficients) {
             const std::vector<std::pair<int, double>> terms =
                 SortedGroupedTerms(indices, coefficients);
             std::vector<int> sorted_indices;
             std::vector<double> sorted_coefficients;
             sorted_indices.reserve(terms.size());
             sorted_coefficients.reserve(terms.size());
             for (const auto& [i, c] : terms) {
               sorted_indices.push_back(i);
               sorted_coefficients.push_back(c);
             }
             return std::make_pair(sorted_indices, sorted_coefficients);
           });

  py::enum_<SolveStatus>(m, "SolveStatus")
      .value("OPTIMAL", SolveStatus::OPTIMAL)
      .value("FEASIBLE", SolveStatus::FEASIBLE)
      .value("INFEASIBLE", SolveStatus::INFEASIBLE)
      .value("UNBOUNDED", SolveStatus::UNBOUNDED)
      .value("ABNORMAL", SolveStatus::ABNORMAL)
      .value("NOT_SOLVED", SolveStatus::NOT_SOLVED)
      .value("MODEL_IS_VALID", SolveStatus::MODEL_IS_VALID)
      .value("CANCELLED_BY_USER", SolveStatus::CANCELLED_BY_USER)
      .value("UNKNOWN_STATUS", SolveStatus::UNKNOWN_STATUS)
      .value("MODEL_INVALID", SolveStatus::MODEL_INVALID)
      .value("INVALID_SOLVER_PARAMETERS",
             SolveStatus::INVALID_SOLVER_PARAMETERS)
      .value("SOLVER_TYPE_UNAVAILABLE", SolveStatus::SOLVER_TYPE_UNAVAILABLE)
      .value("INCOMPATIBLE_OPTIONS", SolveStatus::INCOMPATIBLE_OPTIONS)
      .export_values();

  py::class_<ModelSolverHelper>(m, "ModelSolverHelper")
      .def(py::init<const std::string&>())
      .def("solver_is_supported", &ModelSolverHelper::SolverIsSupported)
      .def("solve", &ModelSolverHelper::Solve, arg("model"),
           // The GIL is released during the solve to allow Python threads to do
           // other things in parallel, e.g., log and interrupt.
           py::call_guard<py::gil_scoped_release>())
      .def("solve_serialized_request",
           [](ModelSolverHelper* solver, absl::string_view request_str) {
             std::string result;
             {
               // The GIL is released during the solve to allow Python threads
               // to do other things in parallel, e.g., log and interrupt.
               py::gil_scoped_release release;
               MPModelRequest request;
               if (!request.ParseFromString(std::string(request_str))) {
                 throw std::invalid_argument(
                     "Unable to parse request as MPModelRequest.");
               }
               std::optional<MPSolutionResponse> solution =
                   solver->SolveRequest(request);
               if (solution.has_value()) {
                 result = solution.value().SerializeAsString();
               }
             }
             return py::bytes(result);
           })
      .def("interrupt_solve", &ModelSolverHelper::InterruptSolve,
           "Returns true if the interrupt signal was correctly sent, that is, "
           "if the underlying solver supports it.")
      .def("set_log_callback", &ModelSolverHelper::SetLogCallback)
      .def("clear_log_callback", &ModelSolverHelper::ClearLogCallback)
      .def("set_time_limit_in_seconds",
           &ModelSolverHelper::SetTimeLimitInSeconds, arg("limit"))
      .def("set_solver_specific_parameters",
           &ModelSolverHelper::SetSolverSpecificParameters,
           arg("solver_specific_parameters"))
      .def("enable_output", &ModelSolverHelper::EnableOutput, arg("output"))
      .def("has_solution", &ModelSolverHelper::has_solution)
      .def("has_response", &ModelSolverHelper::has_response)
      .def("response", &ModelSolverHelper::response)
      .def("status", &ModelSolverHelper::status)
      .def("status_string", &ModelSolverHelper::status_string)
      .def("wall_time", &ModelSolverHelper::wall_time)
      .def("user_time", &ModelSolverHelper::user_time)
      .def("objective_value", &ModelSolverHelper::objective_value)
      .def("best_objective_bound", &ModelSolverHelper::best_objective_bound)
      .def("var_value", &ModelSolverHelper::variable_value, arg("var_index"))
      .def("reduced_cost", &ModelSolverHelper::reduced_cost, arg("var_index"))
      .def("dual_value", &ModelSolverHelper::dual_value, arg("ct_index"))
      .def("activity", &ModelSolverHelper::activity, arg("ct_index"))
      .def("variable_values",
           [](const ModelSolverHelper& helper) {
             if (!helper.has_response()) {
               throw std::logic_error(
                   "Accessing a solution value when none has been found.");
             }
             const MPSolutionResponse& response = helper.response();
             Eigen::VectorXd vec(response.variable_value_size());
             for (int i = 0; i < response.variable_value_size(); ++i) {
               vec[i] = response.variable_value(i);
             }
             return vec;
           })
      .def("expression_value",
           [](const ModelSolverHelper& helper, const std::vector<int>& indices,
              const std::vector<double>& coefficients, double constant) {
             if (!helper.has_response()) {
               throw std::logic_error(
                   "Accessing a solution value when none has been found.");
             }
             const MPSolutionResponse& response = helper.response();
             for (int i = 0; i < indices.size(); ++i) {
               constant +=
                   response.variable_value(indices[i]) * coefficients[i];
             }
             return constant;
           })
      .def("reduced_costs",
           [](const ModelSolverHelper& helper) {
             if (!helper.has_response()) {
               throw std::logic_error(
                   "Accessing a solution value when none has been found.");
             }
             const MPSolutionResponse& response = helper.response();
             Eigen::VectorXd vec(response.reduced_cost_size());
             for (int i = 0; i < response.reduced_cost_size(); ++i) {
               vec[i] = response.reduced_cost(i);
             }
             return vec;
           })
      .def("dual_values", [](const ModelSolverHelper& helper) {
        if (!helper.has_response()) {
          throw std::logic_error(
              "Accessing a solution value when none has been found.");
        }
        const MPSolutionResponse& response = helper.response();
        Eigen::VectorXd vec(response.dual_value_size());
        for (int i = 0; i < response.dual_value_size(); ++i) {
          vec[i] = response.dual_value(i);
        }
        return vec;
      });
}
