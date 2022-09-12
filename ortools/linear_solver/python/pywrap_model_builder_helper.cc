// Copyright 2010-2022 Google LLC
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

#include <optional>
#include <stdexcept>
#include <string>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "absl/strings/str_cat.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_exporter.h"
#include "ortools/linear_solver/wrappers/model_builder_helper.h"
#include "pybind11/eigen.h"
#include "pybind11/pybind11.h"
#include "pybind11/pytypes.h"
#include "pybind11/stl.h"

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
using ::pybind11::arg;

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

PYBIND11_MODULE(pywrap_model_builder_helper, m) {
  pybind11::class_<MPModelExportOptions>(m, "MPModelExportOptions")
      .def(pybind11::init<>())
      .def_readwrite("obfuscate", &MPModelExportOptions::obfuscate)
      .def_readwrite("log_invalid_names",
                     &MPModelExportOptions::log_invalid_names)
      .def_readwrite("show_unused_variables",
                     &MPModelExportOptions::show_unused_variables)
      .def_readwrite("max_line_length", &MPModelExportOptions::max_line_length);

  pybind11::class_<ModelBuilderHelper>(m, "ModelBuilderHelper")
      .def(pybind11::init<>())
      .def("export_to_mps_string", &ModelBuilderHelper::ExportToMpsString,
           arg("options") = MPModelExportOptions())
      .def("export_to_lp_string", &ModelBuilderHelper::ExportToLpString,
           arg("options") = MPModelExportOptions())
      .def("write_model_to_file", &ModelBuilderHelper::WriteModelToFile,
           arg("filename"))
      .def("import_from_mps_string", &ModelBuilderHelper::ImportFromMpsString,
           arg("mps_string"))
      .def("import_from_mps_file", &ModelBuilderHelper::ImportFromMpsFile,
           arg("mps_file"))
      .def("import_from_lp_string", &ModelBuilderHelper::ImportFromLpString,
           arg("lp_string"))
      .def("import_from_lp_file", &ModelBuilderHelper::ImportFromLpFile,
           arg("lp_file"))
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
      .def("set_var_lower_bound", &ModelBuilderHelper::SetVarLowerBound,
           arg("var_index"), arg("lb"))
      .def("set_var_upper_bound", &ModelBuilderHelper::SetVarUpperBound,
           arg("var_index"), arg("ub"))
      .def("set_var_integrality", &ModelBuilderHelper::SetVarIntegrality,
           arg("var_index"), arg("is_integer"))
      .def("set_var_objective_coefficient",
           &ModelBuilderHelper::SetVarObjectiveCoefficient, arg("var_index"),
           arg("coeff"))
      .def("set_var_name", &ModelBuilderHelper::SetVarName, arg("var_index"),
           arg("name"))
      .def("add_linear_constraint", &ModelBuilderHelper::AddLinearConstraint)
      .def("set_constraint_lower_bound",
           &ModelBuilderHelper::SetConstraintLowerBound, arg("ct_index"),
           arg("lb"))
      .def("set_constraint_upper_bound",
           &ModelBuilderHelper::SetConstraintUpperBound, arg("ct_index"),
           arg("ub"))
      .def("add_term_to_constraint", &ModelBuilderHelper::AddConstraintTerm,
           arg("ct_index"), arg("var_index"), arg("coeff"))
      .def("set_constraint_name", &ModelBuilderHelper::SetConstraintName,
           arg("ct_index"), arg("name"))
      .def("num_variables", &ModelBuilderHelper::num_variables)
      .def("var_lower_bound", &ModelBuilderHelper::VarLowerBound,
           arg("var_index"))
      .def("var_upper_bound", &ModelBuilderHelper::VarUpperBound,
           arg("var_index"))
      .def("var_is_integral", &ModelBuilderHelper::VarIsIntegral,
           arg("var_index"))
      .def("var_objective_coefficient",
           &ModelBuilderHelper::VarObjectiveCoefficient, arg("var_index"))
      .def("var_name", &ModelBuilderHelper::VarName, arg("var_index"))
      .def("num_constraints", &ModelBuilderHelper::num_constraints)
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
      .def("name", &ModelBuilderHelper::name)
      .def("set_name", &ModelBuilderHelper::SetName, arg("name"))
      .def("clear_objective", &ModelBuilderHelper::ClearObjective)
      .def("maximize", &ModelBuilderHelper::maximize)
      .def("set_maximize", &ModelBuilderHelper::SetMaximize, arg("maximize"))
      .def("set_objective_offset", &ModelBuilderHelper::SetObjectiveOffset,
           arg("offset"))
      .def("objective_offset", &ModelBuilderHelper::ObjectiveOffset);

  pybind11::enum_<SolveStatus>(m, "SolveStatus")
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

  pybind11::class_<ModelSolverHelper>(m, "ModelSolverHelper")
      .def(pybind11::init<const std::string&>())
      .def("solver_is_supported", &ModelSolverHelper::SolverIsSupported)
      .def("solve", &ModelSolverHelper::Solve, arg("model"),
           // The GIL is released during the solve to allow Python threads to do
           // other things in parallel, e.g., log and interrupt.
           pybind11::call_guard<pybind11::gil_scoped_release>())
      .def("solve_serialized_request",
           [](ModelSolverHelper* solver, const std::string& request_str) {
             std::string result;
             {
               // The GIL is released during the solve to allow Python threads
               // to do other things in parallel, e.g., log and interrupt.
               pybind11::gil_scoped_release release;
               MPModelRequest request;

               if (!request.ParseFromString(request_str)) {
                 throw std::invalid_argument(
                     "Unable to parse request as MPModelRequest.");
               }
               std::optional<MPSolutionResponse> solution =
                   solver->SolveRequest(request);
               if (solution.has_value()) {
                 result = solution.value().SerializeAsString();
               }
             }
             return pybind11::bytes(result);
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
      .def("status",
           [](const ModelSolverHelper& solver) {
             // TODO(user):
             //    - Return the true enum when pybind11_protobuf is working.
             //    - Return the response proto
             return static_cast<int>(solver.status());
           })
      .def("status_string", &ModelSolverHelper::status_string)
      .def("wall_time", &ModelSolverHelper::wall_time)
      .def("user_time", &ModelSolverHelper::user_time)
      .def("objective_value", &ModelSolverHelper::objective_value)
      .def("best_objective_bound", &ModelSolverHelper::best_objective_bound)
      .def("var_value", &ModelSolverHelper::variable_value, arg("var_index"))
      .def("reduced_cost", &ModelSolverHelper::reduced_cost, arg("var_index"))
      .def("dual_value", &ModelSolverHelper::dual_value, arg("ct_index"))
      .def("variable_values",
           [](const ModelSolverHelper& helper) {
             if (!helper.has_response()) return Eigen::VectorXd();
             const MPSolutionResponse& response = helper.response();
             Eigen::VectorXd vec(response.variable_value_size());
             for (int i = 0; i < response.variable_value_size(); ++i) {
               vec[i] = response.variable_value(i);
             }
             return vec;
           })
      .def("reduced_costs",
           [](const ModelSolverHelper& helper) {
             if (!helper.has_response()) return Eigen::VectorXd();
             const MPSolutionResponse& response = helper.response();
             Eigen::VectorXd vec(response.reduced_cost_size());
             for (int i = 0; i < response.reduced_cost_size(); ++i) {
               vec[i] = response.reduced_cost(i);
             }
             return vec;
           })
      .def("dual_values", [](const ModelSolverHelper& helper) {
        if (!helper.has_response()) return Eigen::VectorXd();
        const MPSolutionResponse& response = helper.response();
        Eigen::VectorXd vec(response.dual_value_size());
        for (int i = 0; i < response.dual_value_size(); ++i) {
          vec[i] = response.dual_value(i);
        }
        return vec;
      });
}
