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

#include "ortools/linear_solver/wrappers/model_builder_helper.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "ortools/base/helpers.h"
#include "ortools/base/logging.h"
#include "ortools/base/options.h"
#include "ortools/gurobi/environment.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_exporter.h"
#include "ortools/linear_solver/proto_solver/glop_proto_solver.h"
#include "ortools/linear_solver/proto_solver/gurobi_proto_solver.h"
#include "ortools/linear_solver/proto_solver/sat_proto_solver.h"
#include "ortools/linear_solver/proto_solver/xpress_proto_solver.h"
#include "ortools/linear_solver/solve_mp_model.h"
#if defined(USE_SCIP)
#include "ortools/linear_solver/proto_solver/scip_proto_solver.h"
#endif  // defined(USE_SCIP)
#if defined(USE_HIGHS)
#include "ortools/linear_solver/proto_solver/highs_proto_solver.h"
#endif  // defined(USE_HIGHS)
#if defined(USE_PDLP)
#include "ortools/linear_solver/proto_solver/pdlp_proto_solver.h"
#endif  // defined(USE_PDLP)
#if defined(USE_LP_PARSER)
#include "ortools/lp_data/lp_parser.h"
#endif  // defined(USE_LP_PARSER)
#include "ortools/lp_data/mps_reader.h"
#include "ortools/xpress/environment.h"

namespace operations_research {
namespace mb {

// ModelBuilderHelper.

void ModelBuilderHelper::OverwriteModel(
    const ModelBuilderHelper& other_helper) {
  model_ = other_helper.model();
}

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

bool ModelBuilderHelper::WriteToMpsFile(const std::string& filename,
                                        const MPModelExportOptions& options) {
  return WriteModelToMpsFile(filename, model_, options).ok();
}

bool ModelBuilderHelper::ReadModelFromProtoFile(const std::string& filename) {
  if (file::GetTextProto(filename, &model_, file::Defaults()).ok() ||
      file::GetBinaryProto(filename, &model_, file::Defaults()).ok()) {
    return true;
  }
  MPModelRequest request;
  if (file::GetTextProto(filename, &request, file::Defaults()).ok() ||
      file::GetBinaryProto(filename, &request, file::Defaults()).ok()) {
    model_ = request.model();
    return true;
  }
  return false;
}

bool ModelBuilderHelper::WriteModelToProtoFile(const std::string& filename) {
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

void ModelBuilderHelper::ClearConstraintTerms(int ct_index) {
  MPConstraintProto* ct_proto = model_.mutable_constraint(ct_index);
  ct_proto->clear_var_index();
  ct_proto->clear_coefficient();
}

void ModelBuilderHelper::AddConstraintTerm(int ct_index, int var_index,
                                           double coeff) {
  if (coeff == 0.0) return;
  MPConstraintProto* ct_proto = model_.mutable_constraint(ct_index);
  ct_proto->add_var_index(var_index);
  ct_proto->add_coefficient(coeff);
}

void ModelBuilderHelper::SafeAddConstraintTerm(int ct_index, int var_index,
                                               double coeff) {
  if (coeff == 0.0) return;
  MPConstraintProto* ct_proto = model_.mutable_constraint(ct_index);
  for (int i = 0; i < ct_proto->var_index_size(); ++i) {
    if (ct_proto->var_index(i) == var_index) {
      ct_proto->set_coefficient(i, coeff + ct_proto->coefficient(i));
      return;
    }
  }
  // If we reach this point, the variable does not exist in the constraint yet,
  // so we add it to the constraint as a new term.
  ct_proto->add_var_index(var_index);
  ct_proto->add_coefficient(coeff);
}

void ModelBuilderHelper::SetConstraintName(int ct_index,
                                           const std::string& name) {
  model_.mutable_constraint(ct_index)->set_name(name);
}

void ModelBuilderHelper::SetConstraintCoefficient(int ct_index, int var_index,
                                                  double coeff) {
  MPConstraintProto* ct_proto = model_.mutable_constraint(ct_index);
  for (int i = 0; i < ct_proto->var_index_size(); ++i) {
    if (ct_proto->var_index(i) == var_index) {
      ct_proto->set_coefficient(i, coeff);
      return;
    }
  }
  // If we reach this point, the variable does not exist in the constraint yet,
  // so we add it to the constraint as a new term.
  ct_proto->add_var_index(var_index);
  ct_proto->add_coefficient(coeff);
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

int ModelBuilderHelper::AddEnforcedLinearConstraint() {
  const int index = model_.general_constraint_size();
  // Create the new general constraint, and force the type to indicator ct.
  model_.add_general_constraint()->mutable_indicator_constraint();
  return index;
}

bool ModelBuilderHelper::IsEnforcedConstraint(int ct_index) const {
  const MPGeneralConstraintProto& gen = model_.general_constraint(ct_index);
  return gen.general_constraint_case() ==
         MPGeneralConstraintProto::kIndicatorConstraint;
}

void ModelBuilderHelper::SetEnforcedConstraintLowerBound(int ct_index,
                                                         double lb) {
  DCHECK(IsEnforcedConstraint(ct_index));
  MPGeneralConstraintProto* gen = model_.mutable_general_constraint(ct_index);
  MPConstraintProto* ct_proto =
      gen->mutable_indicator_constraint()->mutable_constraint();
  ct_proto->set_lower_bound(lb);
}

void ModelBuilderHelper::SetEnforcedConstraintUpperBound(int ct_index,
                                                         double ub) {
  DCHECK(IsEnforcedConstraint(ct_index));
  MPGeneralConstraintProto* gen = model_.mutable_general_constraint(ct_index);
  MPConstraintProto* ct_proto =
      gen->mutable_indicator_constraint()->mutable_constraint();
  ct_proto->set_upper_bound(ub);
}

void ModelBuilderHelper::ClearEnforcedConstraintTerms(int ct_index) {
  MPConstraintProto* ct_proto = model_.mutable_general_constraint(ct_index)
                                    ->mutable_indicator_constraint()
                                    ->mutable_constraint();
  ct_proto->clear_var_index();
  ct_proto->clear_coefficient();
}

void ModelBuilderHelper::AddEnforcedConstraintTerm(int ct_index, int var_index,
                                                   double coeff) {
  DCHECK(IsEnforcedConstraint(ct_index));
  if (coeff == 0.0) return;
  MPGeneralConstraintProto* gen = model_.mutable_general_constraint(ct_index);
  MPConstraintProto* ct_proto =
      gen->mutable_indicator_constraint()->mutable_constraint();
  ct_proto->add_var_index(var_index);
  ct_proto->add_coefficient(coeff);
}

void ModelBuilderHelper::SafeAddEnforcedConstraintTerm(int ct_index,
                                                       int var_index,
                                                       double coeff) {
  DCHECK(IsEnforcedConstraint(ct_index));
  if (coeff == 0.0) return;
  MPGeneralConstraintProto* gen = model_.mutable_general_constraint(ct_index);
  MPConstraintProto* ct_proto =
      gen->mutable_indicator_constraint()->mutable_constraint();
  for (int i = 0; i < ct_proto->var_index_size(); ++i) {
    if (ct_proto->var_index(i) == var_index) {
      ct_proto->set_coefficient(i, coeff + ct_proto->coefficient(i));
      return;
    }
  }
  // If we reach this point, the variable does not exist in the constraint yet,
  // so we add it to the constraint as a new term.
  ct_proto->add_var_index(var_index);
  ct_proto->add_coefficient(coeff);
}

void ModelBuilderHelper::SetEnforcedConstraintName(int ct_index,
                                                   const std::string& name) {
  model_.mutable_general_constraint(ct_index)->set_name(name);
}

void ModelBuilderHelper::SetEnforcedConstraintCoefficient(int ct_index,
                                                          int var_index,
                                                          double coeff) {
  DCHECK(IsEnforcedConstraint(ct_index));
  MPGeneralConstraintProto* gen = model_.mutable_general_constraint(ct_index);
  MPConstraintProto* ct_proto =
      gen->mutable_indicator_constraint()->mutable_constraint();
  for (int i = 0; i < ct_proto->var_index_size(); ++i) {
    if (ct_proto->var_index(i) == var_index) {
      ct_proto->set_coefficient(i, coeff);
      return;
    }
  }
  // If we reach this point, the variable does not exist in the constraint yet,
  // so we add it to the constraint as a new term.
  ct_proto->add_var_index(var_index);
  ct_proto->add_coefficient(coeff);
}

void ModelBuilderHelper::SetEnforcedIndicatorVariableIndex(int ct_index,
                                                           int var_index) {
  DCHECK(IsEnforcedConstraint(ct_index));
  MPGeneralConstraintProto* gen = model_.mutable_general_constraint(ct_index);
  gen->mutable_indicator_constraint()->set_var_index(var_index);
}

void ModelBuilderHelper::SetEnforcedIndicatorValue(int ct_index,
                                                   bool positive) {
  DCHECK(IsEnforcedConstraint(ct_index));
  MPGeneralConstraintProto* gen = model_.mutable_general_constraint(ct_index);
  gen->mutable_indicator_constraint()->set_var_value(positive);
}

double ModelBuilderHelper::EnforcedConstraintLowerBound(int ct_index) const {
  DCHECK(IsEnforcedConstraint(ct_index));
  return model_.general_constraint(ct_index)
      .indicator_constraint()
      .constraint()
      .lower_bound();
}

double ModelBuilderHelper::EnforcedConstraintUpperBound(int ct_index) const {
  DCHECK(IsEnforcedConstraint(ct_index));
  return model_.general_constraint(ct_index)
      .indicator_constraint()
      .constraint()
      .upper_bound();
}

std::string ModelBuilderHelper::EnforcedConstraintName(int ct_index) const {
  DCHECK(IsEnforcedConstraint(ct_index));
  return model_.general_constraint(ct_index).name();
}

std::vector<int> ModelBuilderHelper::EnforcedConstraintVarIndices(
    int ct_index) const {
  DCHECK(IsEnforcedConstraint(ct_index));
  const MPConstraintProto& ct_proto =
      model_.general_constraint(ct_index).indicator_constraint().constraint();
  return {ct_proto.var_index().begin(), ct_proto.var_index().end()};
}

std::vector<double> ModelBuilderHelper::EnforcedConstraintCoefficients(
    int ct_index) const {
  DCHECK(IsEnforcedConstraint(ct_index));
  const MPConstraintProto& ct_proto =
      model_.general_constraint(ct_index).indicator_constraint().constraint();
  return {ct_proto.coefficient().begin(), ct_proto.coefficient().end()};
}

int ModelBuilderHelper::EnforcedIndicatorVariableIndex(int ct_index) const {
  DCHECK(IsEnforcedConstraint(ct_index));
  return model_.general_constraint(ct_index).indicator_constraint().var_index();
}

bool ModelBuilderHelper::EnforcedIndicatorValue(int ct_index) const {
  DCHECK(IsEnforcedConstraint(ct_index));
  return model_.general_constraint(ct_index)
             .indicator_constraint()
             .var_value() != 0;
}

int ModelBuilderHelper::num_variables() const { return model_.variable_size(); }

int ModelBuilderHelper::num_constraints() const {
  return model_.constraint_size() + model_.general_constraint_size();
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

void ModelBuilderHelper::ClearHints() { model_.clear_solution_hint(); }

void ModelBuilderHelper::AddHint(int var_index, double var_value) {
  model_.mutable_solution_hint()->add_var_index(var_index);
  model_.mutable_solution_hint()->add_var_value(var_value);
}

std::optional<MPSolutionResponse> ModelSolverHelper::SolveRequest(
    const MPModelRequest& request) {
  if (!MPSolver::SupportsProblemType(
          static_cast<MPSolver::OptimizationProblemType>(
              request.solver_type()))) {
    return std::nullopt;
  }
  return SolveMPModel(request, &interrupter_);
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

ModelSolverHelper::ModelSolverHelper(const std::string& solver_name)
    : evaluator_(this) {
  if (solver_name.empty()) return;
  MPSolver::OptimizationProblemType parsed_type;
  if (!MPSolver::ParseSolverType(solver_name, &parsed_type)) {
    VLOG(1) << "Unsupported type " << solver_name;
  } else {
    solver_type_ = static_cast<MPModelRequest::SolverType>(parsed_type);
  }
}

bool ModelSolverHelper::SolverIsSupported() const {
  if (!solver_type_.has_value()) return false;
  if (solver_type_.value() == MPModelRequest::GLOP_LINEAR_PROGRAMMING) {
    return true;
  }
#ifdef USE_PDLP
  if (solver_type_.value() == MPModelRequest::PDLP_LINEAR_PROGRAMMING) {
    return true;
  }
#endif  // USE_PDLP
  if (solver_type_.value() == MPModelRequest::SAT_INTEGER_PROGRAMMING) {
    return true;
  }
#ifdef USE_SCIP
  if (solver_type_.value() == MPModelRequest::SCIP_MIXED_INTEGER_PROGRAMMING) {
    return true;
  }
#endif  // USE_SCIP
#ifdef USE_HIGHS
  if (solver_type_.value() == MPModelRequest::HIGHS_LINEAR_PROGRAMMING ||
      solver_type_.value() == MPModelRequest::HIGHS_MIXED_INTEGER_PROGRAMMING) {
    return true;
  }
#endif  // USE_HIGHS
  if (solver_type_.value() ==
          MPModelRequest::GUROBI_MIXED_INTEGER_PROGRAMMING ||
      solver_type_.value() == MPModelRequest::GUROBI_LINEAR_PROGRAMMING) {
    return GurobiIsCorrectlyInstalled();
  }
  if (solver_type_.value() ==
          MPModelRequest::XPRESS_MIXED_INTEGER_PROGRAMMING ||
      solver_type_.value() == MPModelRequest::XPRESS_LINEAR_PROGRAMMING) {
    return XpressIsCorrectlyInstalled();
  }
  return false;
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
      response_ =
          GlopSolveProto(std::move(request), &interrupt_solve_, log_callback_);
      break;
    }
    case MPModelRequest::SAT_INTEGER_PROGRAMMING: {
      response_ = SatSolveProto(std::move(request), &interrupt_solve_,
                                log_callback_, nullptr);
      break;
    }
#if defined(USE_SCIP)
    case MPModelRequest::SCIP_MIXED_INTEGER_PROGRAMMING: {
      // TODO(user): Enable log_callback support.
      // TODO(user): Enable interrupt_solve.
      const auto temp = ScipSolveProto(std::move(request));
      if (temp.ok()) {
        response_ = std::move(temp.value());
      }
      break;
    }
#endif  // defined(USE_SCIP)
#if defined(USE_PDLP)
    case MPModelRequest::PDLP_LINEAR_PROGRAMMING: {
      const auto temp = PdlpSolveProto(std::move(request));
      if (temp.ok()) {
        response_ = std::move(temp.value());
      }
      break;
    }
#endif  // defined(USE_PDLP)
    case MPModelRequest::
        GUROBI_LINEAR_PROGRAMMING:  // ABSL_FALLTHROUGH_INTENDED
    case MPModelRequest::GUROBI_MIXED_INTEGER_PROGRAMMING: {
      const auto temp = GurobiSolveProto(std::move(request));
      if (temp.ok()) {
        response_ = std::move(temp.value());
      }
      break;
    }
#if defined(USE_HIGHS)
    case MPModelRequest::HIGHS_LINEAR_PROGRAMMING:  // ABSL_FALLTHROUGH_INTENDED
    case MPModelRequest::HIGHS_MIXED_INTEGER_PROGRAMMING: {
      // TODO(user): Enable log_callback support.
      // TODO(user): Enable interrupt_solve.
      const auto temp = HighsSolveProto(std::move(request));
      if (temp.ok()) {
        response_ = std::move(temp.value());
      }
      break;
    }
#endif  // defined(USE_HIGHS)
    case MPModelRequest::
        XPRESS_LINEAR_PROGRAMMING:  // ABSL_FALLTHROUGH_INTENDED
    case MPModelRequest::XPRESS_MIXED_INTEGER_PROGRAMMING: {
      response_ = XPressSolveProto(request);
      break;
    }
    default: {
      response_->set_status(
          MPSolverResponseStatus::MPSOLVER_SOLVER_TYPE_UNAVAILABLE);
    }
  }
  if (response_->status() == MPSOLVER_OPTIMAL ||
      response_->status() == MPSOLVER_FEASIBLE) {
    model_of_last_solve_ = &model.model();
    activities_.assign(model.num_constraints(),
                       std::numeric_limits<double>::quiet_NaN());
  } else {
    activities_.clear();
  }
}

void ModelSolverHelper::SetLogCallback(
    std::function<void(const std::string&)> log_callback) {
  log_callback_ = std::move(log_callback);
}

void ModelSolverHelper::SetLogCallbackFromDirectorClass(
    MbLogCallback* log_callback) {
  log_callback_ = [log_callback](const std::string& message) {
    log_callback->NewMessage(message);
  };
}

void ModelSolverHelper::ClearLogCallback() { log_callback_ = nullptr; }

bool ModelSolverHelper::InterruptSolve() {
  interrupter_.Interrupt();
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

double ModelSolverHelper::expression_value(LinearExpr* expr) const {
  if (!has_response()) return 0.0;
  evaluator_.Clear();
  evaluator_.AddToProcess(expr, 1.0);
  return evaluator_.Evaluate();
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

double ModelSolverHelper::activity(int ct_index) {
  if (!has_response() || ct_index >= activities_.size() ||
      !model_of_last_solve_.has_value()) {
    return 0.0;
  }

  if (std::isnan(activities_[ct_index])) {
    const MPConstraintProto& ct_proto =
        model_of_last_solve_.value()->constraint(ct_index);
    double result = 0.0;
    for (int i = 0; i < ct_proto.var_index_size(); ++i) {
      result += response_->variable_value(ct_proto.var_index(i)) *
                ct_proto.coefficient(i);
    }
    activities_[ct_index] = result;
  }
  return activities_[ct_index];
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

// Expressions.
LinearExpr* LinearExpr::Term(LinearExpr* expr, double coeff) {
  return new AffineExpr(expr, coeff, 0.0);
}

LinearExpr* LinearExpr::Affine(LinearExpr* expr, double coeff,
                               double constant) {
  if (coeff == 1.0 && constant == 0.0) return expr;
  return new AffineExpr(expr, coeff, constant);
}

LinearExpr* LinearExpr::AffineCst(double value, double coeff, double constant) {
  return new FixedValue(value * coeff + constant);
}

LinearExpr* LinearExpr::Constant(double value) { return new FixedValue(value); }

LinearExpr* LinearExpr::Add(LinearExpr* expr) {
  return new SumArray({this, expr}, 0.0);
}

LinearExpr* LinearExpr::AddFloat(double cst) {
  if (cst == 0.0) return this;
  return new AffineExpr(this, 1.0, cst);
}

LinearExpr* LinearExpr::Sub(LinearExpr* expr) {
  return new WeightedSumArray({this, expr}, {1, -1}, 0.0);
}

LinearExpr* LinearExpr::SubFloat(double cst) {
  if (cst == 0.0) return this;
  return new AffineExpr(this, 1.0, -cst);
}

LinearExpr* LinearExpr::RSubFloat(double cst) {
  return new AffineExpr(this, -1.0, cst);
}

LinearExpr* LinearExpr::MulFloat(double cst) {
  if (cst == 0.0) return new FixedValue(0.0);
  if (cst == 1.0) return this;
  return new AffineExpr(this, cst, 0.0);
}

LinearExpr* LinearExpr::Neg() { return new AffineExpr(this, -1, 0); }

// Expression visitors.

void ExprVisitor::AddToProcess(const LinearExpr* expr, double coeff) {
  to_process_.push_back(std::make_pair(expr, coeff));
}

void ExprVisitor::AddConstant(double constant) { offset_ += constant; }

void ExprVisitor::Clear() {
  to_process_.clear();
  offset_ = 0.0;
}

void ExprFlattener::AddVarCoeff(const Variable* var, double coeff) {
  canonical_terms_[var] += coeff;
}

double ExprFlattener::Flatten(std::vector<const Variable*>* vars,
                              std::vector<double>* coeffs) {
  while (!to_process_.empty()) {
    const auto [expr, coeff] = to_process_.back();
    to_process_.pop_back();
    expr->Visit(*this, coeff);
  }

  vars->clear();
  coeffs->clear();
  for (const auto& [var, coeff] : canonical_terms_) {
    if (coeff == 0.0) continue;
    vars->push_back(var);
    coeffs->push_back(coeff);
  }

  return offset_;
}

void ExprEvaluator::AddVarCoeff(const Variable* var, double coeff) {
  offset_ += coeff * helper_->variable_value(var->index());
}

double ExprEvaluator::Evaluate() {
  offset_ = 0.0;
  while (!to_process_.empty()) {
    const auto [expr, coeff] = to_process_.back();
    to_process_.pop_back();
    expr->Visit(*this, coeff);
  }
  return offset_;
}

FlatExpr::FlatExpr(const LinearExpr* expr) {
  ExprFlattener lin;
  lin.AddToProcess(expr, 1.0);
  offset_ = lin.Flatten(&vars_, &coeffs_);
}

FlatExpr::FlatExpr(const LinearExpr* pos, const LinearExpr* neg) {
  ExprFlattener lin;
  lin.AddToProcess(pos, 1.0);
  lin.AddToProcess(neg, -1.0);
  offset_ = lin.Flatten(&vars_, &coeffs_);
}

FlatExpr::FlatExpr(const std::vector<const Variable*>& vars,
                   const std::vector<double>& coeffs, double offset)
    : vars_(vars), coeffs_(coeffs), offset_(offset) {}

FlatExpr::FlatExpr(double offset) : offset_(offset) {}

std::vector<int> FlatExpr::VarIndices() const {
  std::vector<int> var_indices;
  var_indices.reserve(vars_.size());
  for (const Variable* var : vars_) {
    var_indices.push_back(var->index());
  }
  return var_indices;
}

void FlatExpr::Visit(ExprVisitor& lin, double c) const {
  for (int i = 0; i < vars_.size(); ++i) {
    lin.AddVarCoeff(vars_[i], coeffs_[i] * c);
  }
  lin.AddConstant(offset_ * c);
}

std::string FlatExpr::ToString() const {
  if (vars_.empty()) {
    return absl::StrCat(offset_);
  }
  std::string s;
  int num_printed = 0;
  for (int i = 0; i < vars_.size(); ++i) {
    DCHECK_NE(coeffs_[i], 0.0);
    ++num_printed;
    if (num_printed > 5) {
      absl::StrAppend(&s, " + ...");
      break;
    }
    if (num_printed == 1) {
      if (coeffs_[i] == 1.0) {
        absl::StrAppend(&s, vars_[i]->ToString());
      } else if (coeffs_[i] == -1.0) {
        absl::StrAppend(&s, "-", vars_[i]->ToString());
      } else {
        absl::StrAppend(&s, coeffs_[i], " * ", vars_[i]->ToString());
      }
    } else {
      if (coeffs_[i] == 1.0) {
        absl::StrAppend(&s, " + ", vars_[i]->ToString());
      } else if (coeffs_[i] == -1.0) {
        absl::StrAppend(&s, " - ", vars_[i]->ToString());
      } else if (coeffs_[i] > 0.0) {
        absl::StrAppend(&s, " + ", coeffs_[i], " * ", vars_[i]->ToString());
      } else {
        absl::StrAppend(&s, " - ", -coeffs_[i], " * ", vars_[i]->ToString());
      }
    }
  }
  // If there are no terms, just print the offset.
  if (num_printed == 0) {
    return absl::StrCat(offset_);
  }

  // If there is an offset, print it.
  if (offset_ != 0.0) {
    if (offset_ > 0.0) {
      absl::StrAppend(&s, " + ", offset_);
    } else {
      absl::StrAppend(&s, " - ", -offset_);
    }
  }
  return s;
}

std::string FlatExpr::DebugString() const {
  std::string s = absl::StrCat(
      "FlatExpr(",
      absl::StrJoin(vars_, ", ", [](std::string* out, const Variable* expr) {
        absl::StrAppend(out, expr->DebugString());
      }));
  if (offset_ != 0.0) {
    absl::StrAppend(&s, ", offset=", offset_);
  }
  absl::StrAppend(&s, ")");
  return s;
}

void FixedValue::Visit(ExprVisitor& lin, double c) const {
  lin.AddConstant(value_ * c);
}

std::string FixedValue::ToString() const { return absl::StrCat(value_); }

std::string FixedValue::DebugString() const {
  return absl::StrCat("FixedValue(", value_, ")");
}

WeightedSumArray::WeightedSumArray(const std::vector<LinearExpr*>& exprs,
                                   const std::vector<double>& coeffs,
                                   double offset)
    : exprs_(exprs.begin(), exprs.end()),
      coeffs_(coeffs.begin(), coeffs.end()),
      offset_(offset) {}

void WeightedSumArray::Visit(ExprVisitor& lin, double c) const {
  for (int i = 0; i < exprs_.size(); ++i) {
    lin.AddToProcess(exprs_[i], coeffs_[i] * c);
  }
  lin.AddConstant(offset_ * c);
}

std::string WeightedSumArray::ToString() const {
  if (exprs_.empty()) {
    return absl::StrCat(offset_);
  }
  std::string s = "(";
  bool first_printed = true;
  for (int i = 0; i < exprs_.size(); ++i) {
    if (coeffs_[i] == 0.0) continue;
    if (first_printed) {
      first_printed = false;
      if (coeffs_[i] == 1.0) {
        absl::StrAppend(&s, exprs_[i]->ToString());
      } else if (coeffs_[i] == -1.0) {
        absl::StrAppend(&s, "-", exprs_[i]->ToString());
      } else {
        absl::StrAppend(&s, coeffs_[i], " * ", exprs_[i]->ToString());
      }
    } else {
      if (coeffs_[i] == 1.0) {
        absl::StrAppend(&s, " + ", exprs_[i]->ToString());
      } else if (coeffs_[i] == -1.0) {
        absl::StrAppend(&s, " - ", exprs_[i]->ToString());
      } else if (coeffs_[i] > 0.0) {
        absl::StrAppend(&s, " + ", coeffs_[i], " * ", exprs_[i]->ToString());
      } else {
        absl::StrAppend(&s, " - ", -coeffs_[i], " * ", exprs_[i]->ToString());
      }
    }
  }
  // If there are no terms, just print the offset.
  if (first_printed) {
    return absl::StrCat(offset_);
  }

  // If there is an offset, print it.
  if (offset_ != 0.0) {
    if (offset_ > 0.0) {
      absl::StrAppend(&s, " + ", offset_);
    } else {
      absl::StrAppend(&s, " - ", -offset_);
    }
  }
  absl::StrAppend(&s, ")");
  return s;
}

std::string WeightedSumArray::DebugString() const {
  return absl::StrCat("WeightedSumArray([",
                      absl::StrJoin(exprs_, ", ",
                                    [](std::string* out, const LinearExpr* e) {
                                      absl::StrAppend(out, e->DebugString());
                                    }),
                      "], [", absl::StrJoin(coeffs_, "], "), offset_, ")");
}

AffineExpr::AffineExpr(LinearExpr* expr, double coeff, double offset)
    : expr_(expr), coeff_(coeff), offset_(offset) {}

void AffineExpr::Visit(ExprVisitor& lin, double c) const {
  lin.AddToProcess(expr_, c * coeff_);
  lin.AddConstant(offset_ * c);
}

std::string AffineExpr::ToString() const {
  std::string s = "(";
  if (coeff_ == 1.0) {
    absl::StrAppend(&s, expr_->ToString());
  } else if (coeff_ == -1.0) {
    absl::StrAppend(&s, "-", expr_->ToString());
  } else {
    absl::StrAppend(&s, coeff_, " * ", expr_->ToString());
  }
  if (offset_ > 0.0) {
    absl::StrAppend(&s, " + ", offset_);
  } else if (offset_ < 0.0) {
    absl::StrAppend(&s, " - ", -offset_);
  }
  absl::StrAppend(&s, ")");
  return s;
}

std::string AffineExpr::DebugString() const {
  return absl::StrCat("AffineExpr(expr=", expr_->DebugString(),
                      ", coeff=", coeff_, ", offset=", offset_, ")");
}
BoundedLinearExpression* LinearExpr::Eq(LinearExpr* rhs) {
  return new BoundedLinearExpression(this, rhs, 0.0, 0.0);
}

BoundedLinearExpression* LinearExpr::EqCst(double rhs) {
  return new BoundedLinearExpression(this, rhs, rhs);
}

BoundedLinearExpression* LinearExpr::Le(LinearExpr* rhs) {
  return new BoundedLinearExpression(
      this, rhs, -std::numeric_limits<double>::infinity(), 0.0);
}

BoundedLinearExpression* LinearExpr::LeCst(double rhs) {
  return new BoundedLinearExpression(
      this, -std::numeric_limits<double>::infinity(), rhs);
}

BoundedLinearExpression* LinearExpr::Ge(LinearExpr* rhs) {
  return new BoundedLinearExpression(this, rhs, 0.0,
                                     std::numeric_limits<double>::infinity());
}

BoundedLinearExpression* LinearExpr::GeCst(double rhs) {
  return new BoundedLinearExpression(this, rhs,
                                     std::numeric_limits<double>::infinity());
}

bool VariableComparator::operator()(const Variable* lhs,
                                    const Variable* rhs) const {
  return lhs->index() < rhs->index();
}

Variable::Variable(ModelBuilderHelper* helper, int index)
    : helper_(helper), index_(index) {}

Variable::Variable(ModelBuilderHelper* helper, double lb, double ub,
                   bool is_integral)
    : helper_(helper) {
  index_ = helper_->AddVar();
  helper_->SetVarLowerBound(index_, lb);
  helper_->SetVarUpperBound(index_, ub);
  helper_->SetVarIntegrality(index_, is_integral);
}

Variable::Variable(ModelBuilderHelper* helper, double lb, double ub,
                   bool is_integral, const std::string& name)
    : helper_(helper) {
  index_ = helper_->AddVar();
  helper_->SetVarLowerBound(index_, lb);
  helper_->SetVarUpperBound(index_, ub);
  helper_->SetVarIntegrality(index_, is_integral);
  helper_->SetVarName(index_, name);
}

Variable::Variable(ModelBuilderHelper* helper, int64_t lb, int64_t ub,
                   bool is_integral)
    : helper_(helper) {
  index_ = helper_->AddVar();
  helper_->SetVarLowerBound(index_, lb);
  helper_->SetVarUpperBound(index_, ub);
  helper_->SetVarIntegrality(index_, is_integral);
}

Variable::Variable(ModelBuilderHelper* helper, int64_t lb, int64_t ub,
                   bool is_integral, const std::string& name)
    : helper_(helper) {
  index_ = helper_->AddVar();
  helper_->SetVarLowerBound(index_, lb);
  helper_->SetVarUpperBound(index_, ub);
  helper_->SetVarIntegrality(index_, is_integral);
  helper_->SetVarName(index_, name);
}

std::string Variable::ToString() const {
  if (!helper_->VarName(index_).empty()) {
    return helper_->VarName(index_);
  } else {
    return absl::StrCat("Variable(", index_, ")");
  }
}

std::string Variable::DebugString() const {
  return absl::StrCat("Variable(index=", index_,
                      ", lb=", helper_->VarLowerBound(index_),
                      ", ub=", helper_->VarUpperBound(index_),
                      ", is_integral=", helper_->VarIsIntegral(index_),
                      ", name=\'", helper_->VarName(index_), "')");
}

std::string Variable::name() const {
  const std::string& var_name = helper_->VarName(index_);
  if (!var_name.empty()) return var_name;
  return absl::StrCat("variable#", index_);
}

void Variable::SetName(const std::string& name) {
  helper_->SetVarName(index_, name);
}

double Variable::lower_bounds() const { return helper_->VarLowerBound(index_); }

void Variable::SetLowerBound(double lb) {
  helper_->SetVarLowerBound(index_, lb);
}

double Variable::upper_bound() const { return helper_->VarUpperBound(index_); }

void Variable::SetUpperBound(double ub) {
  helper_->SetVarUpperBound(index_, ub);
}

bool Variable::is_integral() const { return helper_->VarIsIntegral(index_); }

void Variable::SetIsIntegral(bool is_integral) {
  helper_->SetVarIntegrality(index_, is_integral);
}

double Variable::objective_coefficient() const {
  return helper_->VarObjectiveCoefficient(index_);
}

void Variable::SetObjectiveCoefficient(double coeff) {
  helper_->SetVarObjectiveCoefficient(index_, coeff);
}

BoundedLinearExpression::BoundedLinearExpression(const LinearExpr* expr,
                                                 double lower_bound,
                                                 double upper_bound) {
  FlatExpr flat_expr(expr);
  vars_ = flat_expr.vars();
  coeffs_ = flat_expr.coeffs();
  lower_bound_ = lower_bound - flat_expr.offset();
  upper_bound_ = upper_bound - flat_expr.offset();
}

BoundedLinearExpression::BoundedLinearExpression(const LinearExpr* pos,
                                                 const LinearExpr* neg,
                                                 double lower_bound,
                                                 double upper_bound) {
  FlatExpr flat_expr(pos, neg);
  vars_ = flat_expr.vars();
  coeffs_ = flat_expr.coeffs();
  lower_bound_ = lower_bound - flat_expr.offset();
  upper_bound_ = upper_bound - flat_expr.offset();
}

BoundedLinearExpression::BoundedLinearExpression(const LinearExpr* expr,
                                                 int64_t lower_bound,
                                                 int64_t upper_bound) {
  FlatExpr flat_expr(expr);
  vars_ = flat_expr.vars();
  coeffs_ = flat_expr.coeffs();
  lower_bound_ = lower_bound - flat_expr.offset();
  upper_bound_ = upper_bound - flat_expr.offset();
}

BoundedLinearExpression::BoundedLinearExpression(const LinearExpr* pos,
                                                 const LinearExpr* neg,
                                                 int64_t lower_bound,
                                                 int64_t upper_bound) {
  FlatExpr flat_expr(pos, neg);
  vars_ = flat_expr.vars();
  coeffs_ = flat_expr.coeffs();
  lower_bound_ = lower_bound - flat_expr.offset();
  upper_bound_ = upper_bound - flat_expr.offset();
}

double BoundedLinearExpression::lower_bound() const { return lower_bound_; }
double BoundedLinearExpression::upper_bound() const { return upper_bound_; }
const std::vector<const Variable*>& BoundedLinearExpression::vars() const {
  return vars_;
}
const std::vector<double>& BoundedLinearExpression::coeffs() const {
  return coeffs_;
}
std::string BoundedLinearExpression::ToString() const {
  std::string s;
  if (vars_.empty()) {
    s = absl::StrCat(0.0);
  } else if (vars_.size() == 1) {
    const std::string var_name = vars_[0]->ToString();
    if (coeffs_[0] == 1) {
      s = var_name;
    } else if (coeffs_[0] == -1) {
      s = absl::StrCat("-", var_name);
    } else {
      s = absl::StrCat(coeffs_[0], " * ", var_name);
    }
  } else {
    s = "(";
    for (int i = 0; i < vars_.size(); ++i) {
      const std::string var_name = vars_[i]->ToString();
      if (i == 0) {
        if (coeffs_[i] == 1) {
          absl::StrAppend(&s, var_name);
        } else if (coeffs_[i] == -1) {
          absl::StrAppend(&s, "-", var_name);
        } else {
          absl::StrAppend(&s, coeffs_[i], " * ", var_name);
        }
      } else {
        if (coeffs_[i] == 1) {
          absl::StrAppend(&s, " + ", var_name);
        } else if (coeffs_[i] == -1) {
          absl::StrAppend(&s, " - ", var_name);
        } else if (coeffs_[i] > 1) {
          absl::StrAppend(&s, " + ", coeffs_[i], " * ", var_name);
        } else {
          absl::StrAppend(&s, " - ", -coeffs_[i], " * ", var_name);
        }
      }
    }
    absl::StrAppend(&s, ")");
  }
  if (lower_bound_ == upper_bound_) {
    return absl::StrCat(s, " == ", lower_bound_);
  } else if (lower_bound_ == std::numeric_limits<double>::min()) {
    if (upper_bound_ == std::numeric_limits<double>::max()) {
      return absl::StrCat("True (unbounded expr ", s, ")");
    } else {
      return absl::StrCat(s, " <= ", upper_bound_);
    }
  } else if (upper_bound_ == std::numeric_limits<double>::max()) {
    return absl::StrCat(s, " >= ", lower_bound_);
  } else {
    return absl::StrCat(lower_bound_, " <= ", s, " <= ", upper_bound_);
  }
}

std::string BoundedLinearExpression::DebugString() const {
  return absl::StrCat("BoundedLinearExpression(vars=[",
                      absl::StrJoin(vars_, ", ",
                                    [](std::string* out, const Variable* var) {
                                      absl::StrAppend(out, var->DebugString());
                                    }),
                      "], coeffs=[", absl::StrJoin(coeffs_, ", "),
                      "], lower_bound=", lower_bound_,
                      ", upper_bound=", upper_bound_, ")");
}

bool BoundedLinearExpression::CastToBool(bool* result) const {
  const bool is_zero = lower_bound_ == 0.0 && upper_bound_ == 0.0;
  if (is_zero) {
    if (vars_.empty()) {
      *result = true;
      return true;
    } else if (vars_.size() == 2 && coeffs_[0] + coeffs_[1] == 0 &&
               std::abs(coeffs_[0]) == 1) {
      *result = false;
      return true;
    }
  }
  return false;
}

}  // namespace mb
}  // namespace operations_research
