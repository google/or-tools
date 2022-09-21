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

#include "ortools/linear_solver/wrappers/model_builder_helper.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/proto_solver/sat_proto_solver.h"
#if defined(USE_SCIP)
#include "ortools/linear_solver/proto_solver/scip_proto_solver.h"
#endif  // defined(USE_SCIP)
#if defined(USE_LP_PARSER)
#include "ortools/lp_data/lp_parser.h"
#endif  // defined(USE_LP_PARSER)
#include "ortools/lp_data/mps_reader.h"

namespace operations_research {

std::string ModelBuilderHelper::ExportToMpsString(
    const MPModelExportOptions& options) {
  return operations_research::ExportModelAsMpsFormat(model_, options)
      .value_or("");
}

std::string ModelBuilderHelper::ExportToLpString(
    const MPModelExportOptions& options) {
  return operations_research::ExportModelAsLpFormat(model_, options)
      .value_or("");
}

bool ModelBuilderHelper::WriteModelToFile(const std::string& filename) {
  if (absl::EndsWith(filename, "txt")) {
    return file::SetTextProto(filename, model_, file::Defaults()).ok();
  } else {
    return file::SetBinaryProto(filename, model_, file::Defaults()).ok();
  }
}

// See comment in the header file why we need to wrap absl::Status code with
// code having simpler APIs.
bool ModelBuilderHelper::ImportFromMpsString(const std::string& mps_string) {
  absl::StatusOr<MPModelProto> model_or =
      operations_research::glop::MpsDataToMPModelProto(mps_string);
  if (!model_or.ok()) return false;
  model_ = model_or.value();
  return true;
}

bool ModelBuilderHelper::ImportFromMpsFile(const std::string& mps_file) {
  absl::StatusOr<MPModelProto> model_or =
      operations_research::glop::MpsFileToMPModelProto(mps_file);
  if (!model_or.ok()) return false;
  model_ = model_or.value();
  return true;
}

#if defined(USE_LP_PARSER)
bool ModelBuilderHelper::ImportFromLpString(const std::string& lp_string) {
  absl::StatusOr<MPModelProto> model_or = ModelProtoFromLpFormat(lp_string);
  if (!model_or.ok()) return false;
  model_ = model_or.value();
  return true;
}

bool ModelBuilderHelper::ImportFromLpFile(const std::string& lp_file) {
  std::string lp_data;
  if (!file::GetContents(lp_file, &lp_data, file::Defaults()).ok()) {
    return false;
  }
  absl::StatusOr<MPModelProto> model_or = ModelProtoFromLpFormat(lp_data);
  if (!model_or.ok()) return false;
  model_ = model_or.value();
  return true;
}
#endif  // #if defined(USE_LP_PARSER)

const MPModelProto& ModelBuilderHelper::model() const { return model_; }

MPModelProto* ModelBuilderHelper::mutable_model() { return &model_; }

int ModelBuilderHelper::AddVar() {
  const int index = model_.variable_size();
  model_.add_variable();
  return index;
}

void ModelBuilderHelper::SetVarLowerBound(int var_index, double lb) {
  model_.mutable_variable(var_index)->set_lower_bound(lb);
}

void ModelBuilderHelper::SetVarUpperBound(int var_index, double ub) {
  model_.mutable_variable(var_index)->set_upper_bound(ub);
}

void ModelBuilderHelper::SetVarIntegrality(int var_index, bool is_integer) {
  model_.mutable_variable(var_index)->set_is_integer(is_integer);
}

void ModelBuilderHelper::SetVarObjectiveCoefficient(int var_index,
                                                    double coeff) {
  model_.mutable_variable(var_index)->set_objective_coefficient(coeff);
}

void ModelBuilderHelper::SetVarName(int var_index, const std::string& name) {
  model_.mutable_variable(var_index)->set_name(name);
}

int ModelBuilderHelper::AddLinearConstraint() {
  const int index = model_.constraint_size();
  model_.add_constraint();
  return index;
}

void ModelBuilderHelper::SetConstraintLowerBound(int ct_index, double lb) {
  model_.mutable_constraint(ct_index)->set_lower_bound(lb);
}

void ModelBuilderHelper::SetConstraintUpperBound(int ct_index, double ub) {
  model_.mutable_constraint(ct_index)->set_upper_bound(ub);
}

void ModelBuilderHelper::AddConstraintTerm(int ct_index, int var_index,
                                           double coeff) {
  MPConstraintProto* ct_proto = model_.mutable_constraint(ct_index);
  ct_proto->add_var_index(var_index);
  ct_proto->add_coefficient(coeff);
}

void ModelBuilderHelper::SetConstraintName(int ct_index,
                                           const std::string& name) {
  model_.mutable_constraint(ct_index)->set_name(name);
}

int ModelBuilderHelper::num_variables() const { return model_.variable_size(); }

double ModelBuilderHelper::VarLowerBound(int var_index) const {
  return model_.variable(var_index).lower_bound();
}

double ModelBuilderHelper::VarUpperBound(int var_index) const {
  return model_.variable(var_index).upper_bound();
}

bool ModelBuilderHelper::VarIsIntegral(int var_index) const {
  return model_.variable(var_index).is_integer();
}

double ModelBuilderHelper::VarObjectiveCoefficient(int var_index) const {
  return model_.variable(var_index).objective_coefficient();
}

std::string ModelBuilderHelper::VarName(int var_index) const {
  return model_.variable(var_index).name();
}

int ModelBuilderHelper::num_constraints() const {
  return model_.constraint_size();
}

double ModelBuilderHelper::ConstraintLowerBound(int ct_index) const {
  return model_.constraint(ct_index).lower_bound();
}

double ModelBuilderHelper::ConstraintUpperBound(int ct_index) const {
  return model_.constraint(ct_index).upper_bound();
}

std::string ModelBuilderHelper::ConstraintName(int ct_index) const {
  return model_.constraint(ct_index).name();
}

std::vector<int> ModelBuilderHelper::ConstraintVarIndices(int ct_index) const {
  const MPConstraintProto& ct_proto = model_.constraint(ct_index);
  return {ct_proto.var_index().begin(), ct_proto.var_index().end()};
}

std::vector<double> ModelBuilderHelper::ConstraintCoefficients(
    int ct_index) const {
  const MPConstraintProto& ct_proto = model_.constraint(ct_index);
  return {ct_proto.coefficient().begin(), ct_proto.coefficient().end()};
}

std::string ModelBuilderHelper::name() const { return model_.name(); }

void ModelBuilderHelper::SetName(const std::string& name) {
  model_.set_name(name);
}
void ModelBuilderHelper::ClearObjective() {
  for (MPVariableProto& var : *model_.mutable_variable()) {
    var.clear_objective_coefficient();
  }
}

bool ModelBuilderHelper::maximize() const { return model_.maximize(); }

void ModelBuilderHelper::SetMaximize(bool maximize) {
  model_.set_maximize(maximize);
}

double ModelBuilderHelper::ObjectiveOffset() const {
  return model_.objective_offset();
}

void ModelBuilderHelper::SetObjectiveOffset(double offset) {
  model_.set_objective_offset(offset);
}

std::optional<MPSolutionResponse> ModelSolverHelper::SolveRequest(
    const MPModelRequest& request) {
  MPSolutionResponse temp;
  MPSolver::SolveWithProto(request, &temp, &interrupt_solve_);
  return temp;
}

namespace {
SolveStatus MPSolverResponseStatusToSolveStatus(MPSolverResponseStatus s) {
  switch (s) {
    case MPSOLVER_OPTIMAL:
      return SolveStatus::OPTIMAL;
    case MPSOLVER_FEASIBLE:
      return SolveStatus::FEASIBLE;
    case MPSOLVER_INFEASIBLE:
      return SolveStatus::INFEASIBLE;
    case MPSOLVER_UNBOUNDED:
      return SolveStatus::UNBOUNDED;
    case MPSOLVER_ABNORMAL:
      return SolveStatus::ABNORMAL;
    case MPSOLVER_NOT_SOLVED:
      return SolveStatus::NOT_SOLVED;
    case MPSOLVER_MODEL_IS_VALID:
      return SolveStatus::MODEL_IS_VALID;
    case MPSOLVER_CANCELLED_BY_USER:
      return SolveStatus::CANCELLED_BY_USER;
    case MPSOLVER_UNKNOWN_STATUS:
      return SolveStatus::UNKNOWN_STATUS;
    case MPSOLVER_MODEL_INVALID:
      return SolveStatus::MODEL_INVALID;
    case MPSOLVER_MODEL_INVALID_SOLUTION_HINT:
      return SolveStatus::MODEL_INVALID;
    case MPSOLVER_MODEL_INVALID_SOLVER_PARAMETERS:
      return SolveStatus::INVALID_SOLVER_PARAMETERS;
    case MPSOLVER_SOLVER_TYPE_UNAVAILABLE:
      return SolveStatus::SOLVER_TYPE_UNAVAILABLE;
    case MPSOLVER_INCOMPATIBLE_OPTIONS:
      return SolveStatus::INCOMPATIBLE_OPTIONS;
    default:
      return SolveStatus::UNKNOWN_STATUS;
  }
}
}  // namespace

ModelSolverHelper::ModelSolverHelper(const std::string& solver_name) {
  if (solver_name.empty()) return;
  MPSolver::OptimizationProblemType parsed_type;
  if (!MPSolver::ParseSolverType(solver_name, &parsed_type)) {
    VLOG(1) << "Unsupported type " << solver_name;
  } else {
    solver_type_ = static_cast<MPModelRequest::SolverType>(parsed_type);
  }
}

bool ModelSolverHelper::SolverIsSupported() const {
  return solver_type_.has_value();
}

void ModelSolverHelper::Solve(const ModelBuilderHelper& model) {
  response_.reset();
  if (!solver_type_.has_value()) {
    response_->set_status(
        MPSolverResponseStatus::MPSOLVER_SOLVER_TYPE_UNAVAILABLE);
    return;
  }

  MPModelRequest request;
  *request.mutable_model() = model.model();
  request.set_solver_type(solver_type_.value());
  request.set_enable_internal_solver_output(solver_output_);
  if (time_limit_in_second_.has_value()) {
    request.set_solver_time_limit_seconds(time_limit_in_second_.value());
  }
  if (!solver_specific_parameters_.empty()) {
    request.set_solver_specific_parameters(solver_specific_parameters_);
  }
  switch (solver_type_.value()) {
    case MPModelRequest::GLOP_LINEAR_PROGRAMMING: {
      // TODO(user): Enable log_callback support.
      MPSolutionResponse temp;
      MPSolver::SolveWithProto(request, &temp, &interrupt_solve_);
      response_ = std::move(temp);
      break;
    }
    case MPModelRequest::SAT_INTEGER_PROGRAMMING: {
      const auto temp =
          SatSolveProto(request, &interrupt_solve_, log_callback_, nullptr);
      if (temp.ok()) {
        response_ = std::move(temp.value());
      }
      break;
    }
#if defined(USE_SCIP)
    case MPModelRequest::SCIP_MIXED_INTEGER_PROGRAMMING: {
      // TODO(user): Enable log_callback support.
      // TODO(user): Enable interrupt_solve.
      const auto temp = ScipSolveProto(request);
      if (temp.ok()) {
        response_ = std::move(temp.value());
      }
      break;
    }
#endif  // defined(USE_SCIP)
    default: {
      response_->set_status(
          MPSolverResponseStatus::MPSOLVER_SOLVER_TYPE_UNAVAILABLE);
    }
  }
}

void ModelSolverHelper::SetLogCallback(
    std::function<void(const std::string&)> log_callback) {
  log_callback_ = std::move(log_callback);
}

void ModelSolverHelper::SetLogCallbackFromDirectorClass(
    LogCallback* log_callback) {
  log_callback_ = [log_callback](const std::string& message) {
    log_callback->NewMessage(message);
  };
}

void ModelSolverHelper::ClearLogCallback() { log_callback_ = nullptr; }

bool ModelSolverHelper::InterruptSolve() {
  interrupt_solve_ = true;
  return true;
}

bool ModelSolverHelper::has_response() const { return response_.has_value(); }

bool ModelSolverHelper::has_solution() const {
  return response_.has_value() &&
         (response_.value().status() ==
              MPSolverResponseStatus::MPSOLVER_OPTIMAL ||
          response_.value().status() ==
              MPSolverResponseStatus::MPSOLVER_FEASIBLE);
}

const MPSolutionResponse& ModelSolverHelper::response() const {
  return response_.value();
}

SolveStatus ModelSolverHelper::status() const {
  if (!response_.has_value()) {
    return SolveStatus::UNKNOWN_STATUS;
  }
  return MPSolverResponseStatusToSolveStatus(response_.value().status());
}

double ModelSolverHelper::objective_value() const {
  if (!has_response()) return 0.0;
  return response_.value().objective_value();
}

double ModelSolverHelper::best_objective_bound() const {
  if (!has_response()) return 0.0;
  return response_.value().best_objective_bound();
}

double ModelSolverHelper::variable_value(int var_index) const {
  if (!has_response()) return 0.0;
  if (var_index >= response_.value().variable_value_size()) return 0.0;
  return response_.value().variable_value(var_index);
}

double ModelSolverHelper::reduced_cost(int var_index) const {
  if (!has_response()) return 0.0;
  if (var_index >= response_.value().reduced_cost_size()) return 0.0;
  return response_.value().reduced_cost(var_index);
}

double ModelSolverHelper::dual_value(int ct_index) const {
  if (!has_response()) return 0.0;
  if (ct_index >= response_.value().dual_value_size()) return 0.0;
  return response_.value().dual_value(ct_index);
}

std::string ModelSolverHelper::status_string() const {
  if (!has_response()) return "";
  return response_.value().status_str();
}

double ModelSolverHelper::wall_time() const {
  if (!response_.has_value()) return 0.0;
  if (!response_.value().has_solve_info()) return 0.0;
  return response_.value().solve_info().solve_wall_time_seconds();
}

double ModelSolverHelper::user_time() const {
  if (!response_.has_value()) return 0.0;
  if (!response_.value().has_solve_info()) return 0.0;
  return response_.value().solve_info().solve_user_time_seconds();
}

void ModelSolverHelper::SetTimeLimitInSeconds(double limit) {
  time_limit_in_second_ = limit;
}

void ModelSolverHelper::SetSolverSpecificParameters(
    const std::string& solver_specific_parameters) {
  solver_specific_parameters_ = solver_specific_parameters;
}

void ModelSolverHelper::EnableOutput(bool enabled) { solver_output_ = enabled; }

}  // namespace operations_research
