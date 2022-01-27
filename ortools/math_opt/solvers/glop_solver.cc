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

#include "ortools/math_opt/solvers/glop_solver.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/int_type.h"
#include "ortools/base/map_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/port/proto_utils.h"
#include "absl/status/status.h"
#include "ortools/base/protoutil.h"

namespace operations_research {
namespace math_opt {

namespace {

absl::string_view SafeName(const VariablesProto& variables, int index) {
  if (variables.names().empty()) {
    return {};
  }
  return variables.names(index);
}

absl::string_view SafeName(const LinearConstraintsProto& linear_constraints,
                           int index) {
  if (linear_constraints.names().empty()) {
    return {};
  }
  return linear_constraints.names(index);
}

glop::LinearProgram::VariableType GlopVarTypeFromIsInteger(
    const bool is_integer) {
  return is_integer ? glop::LinearProgram::VariableType::INTEGER
                    : glop::LinearProgram::VariableType::CONTINUOUS;
}

}  // namespace

GlopSolver::GlopSolver() : linear_program_(), lp_solver_() {}

void GlopSolver::AddVariables(const VariablesProto& variables) {
  for (int i = 0; i < NumVariables(variables); ++i) {
    const glop::ColIndex col_index = linear_program_.CreateNewVariable();
    linear_program_.SetVariableBounds(col_index, variables.lower_bounds(i),
                                      variables.upper_bounds(i));
    linear_program_.SetVariableName(col_index, SafeName(variables, i));
    linear_program_.SetVariableType(
        col_index, GlopVarTypeFromIsInteger(variables.integers(i)));
    gtl::InsertOrDie(&variables_, variables.ids(i), col_index);
  }
}

// Note that this relies on the fact that when variable/constraint
// are deleted, Glop re-index everything by compacting the
// index domain in a stable way.
template <typename IndexType>
void UpdateIdIndexMap(glop::StrictITIVector<IndexType, bool> indices_to_delete,

                      IndexType num_indices,
                      absl::flat_hash_map<int64_t, IndexType>& id_index_map) {
  absl::StrongVector<IndexType, IndexType> new_indices(
      num_indices.value(), IndexType(0));
  IndexType new_index(0);
  for (IndexType index(0); index < num_indices; ++index) {
    if (indices_to_delete[index]) {
      // Mark deleted index
      new_indices[index] = IndexType(-1);
    } else {
      new_indices[index] = new_index;
      ++new_index;
    }
  }
  for (auto it = id_index_map.begin(); it != id_index_map.end();) {
    IndexType index = it->second;
    if (indices_to_delete[index]) {
      // This safely deletes the entry and moves the iterator one step ahead.
      id_index_map.erase(it++);
    } else {
      it->second = new_indices[index];
      ++it;
    }
  }
}

void GlopSolver::DeleteVariables(absl::Span<const int64_t> ids_to_delete) {
  const glop::ColIndex num_cols = linear_program_.num_variables();
  glop::StrictITIVector<glop::ColIndex, bool> columns_to_delete(num_cols,
                                                                false);
  for (const int64_t deleted_variable_id : ids_to_delete) {
    columns_to_delete[variables_.at(deleted_variable_id)] = true;
  }
  linear_program_.DeleteColumns(columns_to_delete);
  UpdateIdIndexMap<glop::ColIndex>(columns_to_delete, num_cols, variables_);
}

void GlopSolver::DeleteLinearConstraints(
    absl::Span<const int64_t> ids_to_delete) {
  const glop::RowIndex num_rows = linear_program_.num_constraints();
  glop::DenseBooleanColumn rows_to_delete(num_rows, false);
  for (const int64_t deleted_constraint_id : ids_to_delete) {
    rows_to_delete[linear_constraints_.at(deleted_constraint_id)] = true;
  }
  linear_program_.DeleteRows(rows_to_delete);
  UpdateIdIndexMap<glop::RowIndex>(rows_to_delete, num_rows,
                                   linear_constraints_);
}

void GlopSolver::AddLinearConstraints(
    const LinearConstraintsProto& linear_constraints) {
  for (int i = 0; i < NumConstraints(linear_constraints); ++i) {
    const glop::RowIndex row_index = linear_program_.CreateNewConstraint();
    linear_program_.SetConstraintBounds(row_index,
                                        linear_constraints.lower_bounds(i),
                                        linear_constraints.upper_bounds(i));
    linear_program_.SetConstraintName(row_index,
                                      SafeName(linear_constraints, i));
    gtl::InsertOrDie(&linear_constraints_, linear_constraints.ids(i),
                     row_index);
  }
}

void GlopSolver::SetOrUpdateObjectiveCoefficients(
    const SparseDoubleVectorProto& linear_objective_coefficients) {
  for (int i = 0; i < linear_objective_coefficients.ids_size(); ++i) {
    const glop::ColIndex col_index =
        variables_.at(linear_objective_coefficients.ids(i));
    linear_program_.SetObjectiveCoefficient(
        col_index, linear_objective_coefficients.values(i));
  }
}

void GlopSolver::SetOrUpdateConstraintMatrix(
    const SparseDoubleMatrixProto& linear_constraint_matrix) {
  for (int j = 0; j < NumMatrixNonzeros(linear_constraint_matrix); ++j) {
    const glop::ColIndex col_index =
        variables_.at(linear_constraint_matrix.column_ids(j));
    const glop::RowIndex row_index =
        linear_constraints_.at(linear_constraint_matrix.row_ids(j));
    const double coefficient = linear_constraint_matrix.coefficients(j);
    linear_program_.SetCoefficient(row_index, col_index, coefficient);
  }
}

void GlopSolver::UpdateVariableBounds(
    const VariableUpdatesProto& variable_updates) {
  for (const auto [id, lb] : MakeView(variable_updates.lower_bounds())) {
    const auto col_index = variables_.at(id);
    linear_program_.SetVariableBounds(
        col_index, lb, linear_program_.variable_upper_bounds()[col_index]);
  }
  for (const auto [id, ub] : MakeView(variable_updates.upper_bounds())) {
    const auto col_index = variables_.at(id);
    linear_program_.SetVariableBounds(
        col_index, linear_program_.variable_lower_bounds()[col_index], ub);
  }
}

void GlopSolver::UpdateLinearConstraintBounds(
    const LinearConstraintUpdatesProto& linear_constraint_updates) {
  for (const auto [id, lb] :
       MakeView(linear_constraint_updates.lower_bounds())) {
    const auto row_index = linear_constraints_.at(id);
    linear_program_.SetConstraintBounds(
        row_index, lb, linear_program_.constraint_upper_bounds()[row_index]);
  }
  for (const auto [id, ub] :
       MakeView(linear_constraint_updates.upper_bounds())) {
    const auto row_index = linear_constraints_.at(id);
    linear_program_.SetConstraintBounds(
        row_index, linear_program_.constraint_lower_bounds()[row_index], ub);
  }
}

std::pair<glop::GlopParameters, std::vector<std::string>>
GlopSolver::MergeCommonParameters(
    const CommonSolveParametersProto& common_solver_parameters,
    const glop::GlopParameters& glop_parameters) {
  glop::GlopParameters result = glop_parameters;
  std::vector<std::string> warnings;
  if (!result.has_max_time_in_seconds() &&
      common_solver_parameters.has_time_limit()) {
    const absl::Duration time_limit =
        util_time::DecodeGoogleApiProto(common_solver_parameters.time_limit())
            .value();
    result.set_max_time_in_seconds(absl::ToDoubleSeconds(time_limit));
  }
  if (!result.has_log_search_progress()) {
    result.set_log_search_progress(common_solver_parameters.enable_output());
  }
  if (!result.has_num_omp_threads() && common_solver_parameters.has_threads()) {
    result.set_num_omp_threads(common_solver_parameters.threads());
  }
  if (!result.has_random_seed() && common_solver_parameters.has_random_seed()) {
    const int random_seed = std::max(0, common_solver_parameters.random_seed());
    result.set_random_seed(random_seed);
  }
  if (!result.has_use_dual_simplex() &&
      common_solver_parameters.lp_algorithm() != LP_ALGORITHM_UNSPECIFIED) {
    switch (common_solver_parameters.lp_algorithm()) {
      case LP_ALGORITHM_PRIMAL_SIMPLEX:
        result.set_use_dual_simplex(false);
        break;
      case LP_ALGORITHM_DUAL_SIMPLEX:
        result.set_use_dual_simplex(true);
        break;
      case LP_ALGORITHM_BARRIER:
        warnings.push_back(
            "GLOP does not support 'LP_ALGORITHM_BARRIER' value for "
            "'lp_algorithm' parameter.");
        break;
      default:
        LOG(FATAL) << "LPAlgorithm: "
                   << ProtoEnumToString(common_solver_parameters.lp_algorithm())
                   << " unknown, error setting GLOP parameters";
    }
  }
  if (!result.has_use_scaling() && !result.has_scaling_method() &&
      common_solver_parameters.scaling() != EMPHASIS_UNSPECIFIED) {
    switch (common_solver_parameters.scaling()) {
      case EMPHASIS_OFF:
        result.set_use_scaling(false);
        break;
      case EMPHASIS_LOW:
      case EMPHASIS_MEDIUM:
        result.set_use_scaling(true);
        result.set_scaling_method(glop::GlopParameters::EQUILIBRATION);
        break;
      case EMPHASIS_HIGH:
      case EMPHASIS_VERY_HIGH:
        result.set_use_scaling(true);
        result.set_scaling_method(glop::GlopParameters::LINEAR_PROGRAM);
        break;
      default:
        LOG(FATAL) << "Scaling emphasis: "
                   << ProtoEnumToString(common_solver_parameters.scaling())
                   << " unknown, error setting GLOP parameters";
    }
  }
  if (!result.has_use_preprocessing() &&
      common_solver_parameters.presolve() != EMPHASIS_UNSPECIFIED) {
    switch (common_solver_parameters.presolve()) {
      case EMPHASIS_OFF:
        result.set_use_preprocessing(false);
        break;
      case EMPHASIS_LOW:
      case EMPHASIS_MEDIUM:
      case EMPHASIS_HIGH:
      case EMPHASIS_VERY_HIGH:
        result.set_use_preprocessing(true);
        break;
      default:
        LOG(FATAL) << "Presolve emphasis: "
                   << ProtoEnumToString(common_solver_parameters.presolve())
                   << " unknown, error setting GLOP parameters";
    }
  }
  if (common_solver_parameters.cuts() != EMPHASIS_UNSPECIFIED) {
    warnings.push_back(absl::StrCat(
        "GLOP does not support 'cuts' parameters, but cuts was set to: ",
        ProtoEnumToString(common_solver_parameters.cuts())));
  }
  if (common_solver_parameters.heuristics() != EMPHASIS_UNSPECIFIED) {
    warnings.push_back(
        absl::StrCat("GLOP does not support 'heuristics' parameter, but "
                     "heuristics was set to: ",
                     ProtoEnumToString(common_solver_parameters.heuristics())));
  }
  return std::make_pair(std::move(result), std::move(warnings));
}

bool GlopSolver::CanUpdate(const ModelUpdateProto& model_update) {
  return true;
}

template <typename IndexType>
SparseDoubleVectorProto FillSparseDoubleVector(
    const std::vector<int64_t>& ids_in_order,
    const absl::flat_hash_map<int64_t, IndexType>& id_map,
    const glop::StrictITIVector<IndexType, glop::Fractional>& values,
    const SparseVectorFilterProto& filter) {
  SparseVectorFilterPredicate predicate(filter);
  SparseDoubleVectorProto result;
  for (const int64_t variable_id : ids_in_order) {
    const double value = values[id_map.at(variable_id)];
    if (predicate.AcceptsAndUpdate(variable_id, value)) {
      result.add_ids(variable_id);
      result.add_values(value);
    }
  }
  return result;
}

template <typename T>
std::vector<int64_t> GetSortedIs(
    const absl::flat_hash_map<int64_t, T>& id_map) {
  std::vector<int64_t> sorted;
  sorted.reserve(id_map.size());
  for (const auto& entry : id_map) {
    sorted.emplace_back(entry.first);
  }
  std::sort(sorted.begin(), sorted.end());
  return sorted;
}

void GlopSolver::FillSolveResult(
    const glop::ProblemStatus status,
    const ModelSolveParametersProto& model_parameters,
    SolveResultProto& solve_result) {
  solve_result.mutable_solve_stats()->set_simplex_iterations(
      lp_solver_.GetNumberOfSimplexIterations());
  // TODO(b/168374742): this needs to be properly filled in. In particular, we
  // can give better primal and dual bounds when the status is not OPTIMAL.
  const bool is_maximize = linear_program_.IsMaximizationProblem();
  constexpr double kInf = std::numeric_limits<double>::infinity();
  solve_result.mutable_solve_stats()->set_best_primal_bound(is_maximize ? -kInf
                                                                        : kInf);
  solve_result.mutable_solve_stats()->set_best_dual_bound(is_maximize ? kInf
                                                                      : -kInf);
  if (status == glop::ProblemStatus::OPTIMAL) {
    solve_result.set_termination_reason(SolveResultProto::OPTIMAL);
    solve_result.mutable_solve_stats()->set_best_primal_bound(
        lp_solver_.GetObjectiveValue());
    solve_result.mutable_solve_stats()->set_best_dual_bound(
        lp_solver_.GetObjectiveValue());

    auto sorted_variables = GetSortedIs(variables_);
    auto sorted_constraints = GetSortedIs(linear_constraints_);

    PrimalSolutionProto* const primal_solution =
        solve_result.add_primal_solutions();
    primal_solution->set_objective_value(lp_solver_.GetObjectiveValue());
    *primal_solution->mutable_variable_values() = FillSparseDoubleVector(
        sorted_variables, variables_, lp_solver_.variable_values(),
        model_parameters.primal_variables_filter());

    DualSolutionProto* const dual_solution = solve_result.add_dual_solutions();
    dual_solution->set_objective_value(lp_solver_.GetObjectiveValue());
    *dual_solution->mutable_dual_values() = FillSparseDoubleVector(
        sorted_constraints, linear_constraints_, lp_solver_.dual_values(),
        model_parameters.dual_linear_constraints_filter());
    *dual_solution->mutable_reduced_costs() = FillSparseDoubleVector(
        sorted_variables, variables_, lp_solver_.reduced_costs(),
        model_parameters.dual_variables_filter());
    // TODO(user): consider pulling these out to a separate method once we
    // support all statuses
  } else if (status == glop::ProblemStatus::PRIMAL_INFEASIBLE ||
             status == glop::ProblemStatus::DUAL_UNBOUNDED) {
    solve_result.set_termination_reason(SolveResultProto::INFEASIBLE);
  } else if (status == glop::ProblemStatus::PRIMAL_UNBOUNDED) {
    solve_result.set_termination_reason(SolveResultProto::UNBOUNDED);
  } else if (status == glop::ProblemStatus::DUAL_INFEASIBLE ||
             status == glop::ProblemStatus::INFEASIBLE_OR_UNBOUNDED) {
    solve_result.set_termination_reason(SolveResultProto::DUAL_INFEASIBLE);
  } else {
    LOG(DFATAL) << "Termination not implemented.";
    solve_result.set_termination_reason(
        SolveResultProto::TERMINATION_REASON_UNSPECIFIED);
    solve_result.set_termination_detail(absl::StrCat("Glop status: ", status));
  }
}

absl::StatusOr<SolveResultProto> GlopSolver::Solve(
    const SolveParametersProto& parameters,
    const ModelSolveParametersProto& model_parameters,
    const CallbackRegistrationProto& callback_registration, const Callback cb) {
  const absl::Time start = absl::Now();
  SolveResultProto result;
  {
    auto [glop_parameters, warnings] = MergeCommonParameters(
        parameters.common_parameters(), parameters.glop_parameters());
    if (!warnings.empty()) {
      if (parameters.common_parameters().strictness().bad_parameter()) {
        return absl::InvalidArgumentError(absl::StrJoin(warnings, "; "));
      } else {
        for (std::string& warning : warnings) {
          result.add_warnings(std::move(warning));
        }
      }
    }
    lp_solver_.SetParameters(glop_parameters);
  }

  const glop::ProblemStatus status = lp_solver_.Solve(linear_program_);
  FillSolveResult(status, model_parameters, result);
  CHECK_OK(util_time::EncodeGoogleApiProto(
      absl::Now() - start, result.mutable_solve_stats()->mutable_solve_time()));
  return result;
}

absl::StatusOr<std::unique_ptr<SolverInterface>> GlopSolver::New(
    const ModelProto& model, const SolverInitializerProto& initializer) {
  auto solver = absl::WrapUnique(new GlopSolver);
  solver->linear_program_.SetName(model.name());
  solver->linear_program_.SetMaximizationProblem(model.objective().maximize());
  solver->linear_program_.SetObjectiveOffset(model.objective().offset());

  solver->AddVariables(model.variables());
  solver->SetOrUpdateObjectiveCoefficients(
      model.objective().linear_coefficients());

  solver->AddLinearConstraints(model.linear_constraints());
  solver->SetOrUpdateConstraintMatrix(model.linear_constraint_matrix());
  solver->linear_program_.CleanUp();
  return solver;
}

absl::Status GlopSolver::Update(const ModelUpdateProto& model_update) {
  if (model_update.objective_updates().has_direction_update()) {
    linear_program_.SetMaximizationProblem(
        model_update.objective_updates().direction_update());
  }
  if (model_update.objective_updates().has_offset_update()) {
    linear_program_.SetObjectiveOffset(
        model_update.objective_updates().offset_update());
  }

  DeleteVariables(model_update.deleted_variable_ids());
  AddVariables(model_update.new_variables());

  SetOrUpdateObjectiveCoefficients(
      model_update.objective_updates().linear_coefficients());
  UpdateVariableBounds(model_update.variable_updates());

  DeleteLinearConstraints(model_update.deleted_linear_constraint_ids());
  AddLinearConstraints(model_update.new_linear_constraints());
  UpdateLinearConstraintBounds(model_update.linear_constraint_updates());

  SetOrUpdateConstraintMatrix(model_update.linear_constraint_matrix_updates());

  linear_program_.CleanUp();

  return absl::OkStatus();
}

MATH_OPT_REGISTER_SOLVER(SOLVER_TYPE_GLOP, GlopSolver::New)

}  // namespace math_opt
}  // namespace operations_research
