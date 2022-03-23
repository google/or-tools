// Copyright 2010-2021 Google LLC
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

#include "ortools/model_builder/wrappers/model_builder_helper.h"

#include <functional>
#include <string>
#include <utility>

#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_parser.h"
#include "ortools/lp_data/mps_reader.h"

namespace operations_research {

std::string ModelBuilderHelper::ExportToMpsString(
    const MPModelExportOptions& options) {
  return operations_research::ExportModelAsMpsFormat(request_.model(), options)
      .value_or("");
}

std::string ModelBuilderHelper::ExportToLpString(
    const MPModelExportOptions& options) {
  return operations_research::ExportModelAsLpFormat(request_.model(), options)
      .value_or("");
}

bool ModelBuilderHelper::WriteModelToFile(const std::string& filename) {
  if (absl::EndsWith(filename, "txt")) {
    return file::SetTextProto(filename, request_.model(), file::Defaults())
        .ok();
  } else {
    return file::SetBinaryProto(filename, request_.model(), file::Defaults())
        .ok();
  }
}

bool ModelBuilderHelper::WriteRequestToFile(const std::string& filename) {
  if (absl::EndsWith(filename, "txt")) {
    return file::SetTextProto(filename, request_, file::Defaults()).ok();
  } else {
    return file::SetBinaryProto(filename, request_, file::Defaults()).ok();
  }
}

// See comment in the header file why we need to wrap absl::Status code with
// code having simpler APIs.
bool ModelBuilderHelper::ImportFromMpsString(const std::string& mps_string) {
  absl::StatusOr<MPModelProto> model_or =
      operations_research::glop::MpsDataToMPModelProto(mps_string);
  if (!model_or.ok()) return false;
  *request_.mutable_model() = model_or.value();
  return true;
}

bool ModelBuilderHelper::ImportFromMpsFile(const std::string& mps_file) {
  absl::StatusOr<MPModelProto> model_or =
      operations_research::glop::MpsDataToMPModelProto(mps_file);
  if (!model_or.ok()) return false;
  *request_.mutable_model() = model_or.value();
  return true;
}

bool ModelBuilderHelper::ImportFromLpString(const std::string& lp_string) {
  absl::StatusOr<MPModelProto> model_or = ModelProtoFromLpFormat(lp_string);
  if (!model_or.ok()) return false;
  *request_.mutable_model() = model_or.value();
  return true;
}

bool ModelBuilderHelper::ImportFromLpFile(const std::string& lp_file) {
  absl::StatusOr<MPModelProto> model_or = ModelProtoFromLpFormat(lp_file);
  if (!model_or.ok()) return false;
  *request_.mutable_model() = model_or.value();
  return true;
}

const MPModelRequest& ModelBuilderHelper::request() const { return request_; }
MPModelProto* ModelBuilderHelper::mutable_model() {
  return request_.mutable_model();
}
MPModelRequest* ModelBuilderHelper::mutable_request() { return &request_; }

int ModelBuilderHelper::AddVar() {
  const int index = request_.model().variable_size();
  request_.mutable_model()->add_variable();
  return index;
}

void ModelBuilderHelper::SetVarLowerBound(int var_index, double lb) {
  request_.mutable_model()->mutable_variable(var_index)->set_lower_bound(lb);
}

void ModelBuilderHelper::SetVarUpperBound(int var_index, double ub) {
  request_.mutable_model()->mutable_variable(var_index)->set_upper_bound(ub);
}

void ModelBuilderHelper::SetVarIntegrality(int var_index, bool is_integer) {
  request_.mutable_model()->mutable_variable(var_index)->set_is_integer(
      is_integer);
}

void ModelBuilderHelper::SetVarObjectiveCoefficient(int var_index,
                                                    double coeff) {
  request_.mutable_model()
      ->mutable_variable(var_index)
      ->set_objective_coefficient(coeff);
}

void ModelBuilderHelper::SetVarName(int var_index, const std::string& name) {
  request_.mutable_model()->mutable_variable(var_index)->set_name(name);
}

int ModelBuilderHelper::AddLinearConstraint() {
  const int index = request_.model().constraint_size();
  request_.mutable_model()->add_constraint();
  return index;
}

void ModelBuilderHelper::SetConstraintLowerBound(int ct_index, double lb) {
  request_.mutable_model()->mutable_constraint(ct_index)->set_lower_bound(lb);
}

void ModelBuilderHelper::SetConstraintUpperBound(int ct_index, double ub) {
  request_.mutable_model()->mutable_constraint(ct_index)->set_upper_bound(ub);
}

void ModelBuilderHelper::AddConstraintTerm(int ct_index, int var_index,
                                           double coeff) {
  MPConstraintProto* ct_proto =
      request_.mutable_model()->mutable_constraint(ct_index);
  ct_proto->add_var_index(var_index);
  ct_proto->add_coefficient(coeff);
}

void ModelBuilderHelper::SetConstraintName(int ct_index,
                                           const std::string& name) {
  request_.mutable_model()->mutable_constraint(ct_index)->set_name(name);
}

int ModelBuilderHelper::num_variables() const {
  return request_.model().variable_size();
}

double ModelBuilderHelper::VarLowerBound(int var_index) const {
  return request_.model().variable(var_index).lower_bound();
}

double ModelBuilderHelper::VarUpperBound(int var_index) const {
  return request_.model().variable(var_index).upper_bound();
}

bool ModelBuilderHelper::VarIsIntegral(int var_index) const {
  return request_.model().variable(var_index).is_integer();
}

double ModelBuilderHelper::VarObjectiveCoefficient(int var_index) const {
  return request_.model().variable(var_index).objective_coefficient();
}

std::string ModelBuilderHelper::VarName(int var_index) const {
  return request_.model().variable(var_index).name();
}

int ModelBuilderHelper::num_constraints() const {
  return request_.model().constraint_size();
}

double ModelBuilderHelper::ConstraintLowerBound(int ct_index) const {
  return request_.model().constraint(ct_index).lower_bound();
}

double ModelBuilderHelper::ConstraintUpperBound(int ct_index) const {
  return request_.model().constraint(ct_index).upper_bound();
}

std::string ModelBuilderHelper::ConstraintName(int ct_index) const {
  return request_.model().constraint(ct_index).name();
}

std::vector<int> ModelBuilderHelper::ConstraintVarIndices(int ct_index) const {
  const MPConstraintProto& ct_proto = request_.model().constraint(ct_index);
  return {ct_proto.var_index().begin(), ct_proto.var_index().end()};
}

std::vector<double> ModelBuilderHelper::ConstraintCoefficients(
    int ct_index) const {
  const MPConstraintProto& ct_proto = request_.model().constraint(ct_index);
  return {ct_proto.coefficient().begin(), ct_proto.coefficient().end()};
}

std::string ModelBuilderHelper::name() const { return request_.model().name(); }

void ModelBuilderHelper::SetName(const std::string& name) {
  request_.mutable_model()->set_name(name);
}

bool ModelBuilderHelper::maximize() const {
  return request_.model().maximize();
}

void ModelBuilderHelper::SetMaximize(bool maximize) {
  request_.mutable_model()->set_maximize(maximize);
}

double ModelBuilderHelper::ObjectiveOffset() const {
  return request_.model().objective_offset();
}

void ModelBuilderHelper::SetObjectiveOffset(double offset) {
  request_.mutable_model()->set_objective_offset(offset);
}

bool ModelBuilderHelper::SetSolverName(const std::string& solver_name) {
  MPSolver::OptimizationProblemType parsed_type;
  if (!MPSolver::ParseSolverType(solver_name, &parsed_type)) return false;
  MPModelRequest::SolverType type =
      static_cast<MPModelRequest::SolverType>(parsed_type);
  request_.set_solver_type(type);
  return true;
}

void ModelSolverHelper::Solve(const ModelBuilderHelper& model) {
  // TODO(user): Make compatible with solvers that don't support interrupt.
  // TODO(user): Register the log callback.
  response_.reset();
  MPSolutionResponse temp;
  MPSolver::SolveWithProto(model.request(), &temp, &interrupt_solve_);
  response_ = std::move(temp);
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

bool ModelSolverHelper::InterruptSolve() {
  interrupt_solve_ = true;
  return true;
}

bool ModelSolverHelper::has_response() const { return response_.has_value(); }

const MPSolutionResponse& ModelSolverHelper::response() const {
  return response_.value();
}

MPSolverResponseStatus ModelSolverHelper::status() const {
  if (!has_response()) return MPSolverResponseStatus::MPSOLVER_UNKNOWN_STATUS;
  return response_.value().status();
}

double ModelSolverHelper::objective_value() const {
  if (!response_.has_value()) return 0.0;
  return response_.value().objective_value();
}

double ModelSolverHelper::best_objective_bound() const {
  if (!response_.has_value()) return 0.0;
  return response_.value().best_objective_bound();
}

double ModelSolverHelper::variable_value(int var_index) const {
  if (!response_.has_value()) return 0.0;
  if (var_index >= response_.value().variable_value_size()) return 0.0;
  return response_.value().variable_value(var_index);
}

double ModelSolverHelper::reduced_cost(int var_index) const {
  if (!response_.has_value()) return 0.0;
  if (var_index >= response_.value().reduced_cost_size()) return 0.0;
  return response_.value().reduced_cost(var_index);
}

double ModelSolverHelper::dual_value(int ct_index) const {
  if (!response_.has_value()) return 0.0;
  if (ct_index >= response_.value().dual_value_size()) return 0.0;
  return response_.value().dual_value(ct_index);
}

std::string ModelSolverHelper::status_string() const {
  if (!response_.has_value()) return "";
  return response_.value().status_str();
}

}  // namespace operations_research
