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

#include "ortools/math_opt/solvers/glop_solver.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/map_util.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/status_macros.h"
#include "ortools/base/strong_vector.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/parameters_validation.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/inverted_bounds.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/callback_validator.h"
#include "ortools/util/logging.h"
#include "ortools/util/solve_interrupter.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace math_opt {

namespace {

constexpr SupportedProblemStructures kGlopSupportedStructures = {};

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

absl::StatusOr<TerminationProto> BuildTermination(
    const glop::ProblemStatus status,
    const SolveInterrupter* absl_nullable const interrupter,
    const bool is_maximize, const double objective_value) {
  switch (status) {
    case glop::ProblemStatus::OPTIMAL:
      return OptimalTerminationProto(objective_value, objective_value);
    case glop::ProblemStatus::PRIMAL_INFEASIBLE:
      return InfeasibleTerminationProto(
          is_maximize,
          /*dual_feasibility_status=*/FEASIBILITY_STATUS_UNDETERMINED);
    case glop::ProblemStatus::DUAL_UNBOUNDED:
      return InfeasibleTerminationProto(
          is_maximize,
          /*dual_feasibility_status=*/FEASIBILITY_STATUS_FEASIBLE);
    case glop::ProblemStatus::PRIMAL_UNBOUNDED:
      return UnboundedTerminationProto(is_maximize);
    case glop::ProblemStatus::DUAL_INFEASIBLE:
      return InfeasibleOrUnboundedTerminationProto(
          is_maximize,
          /*dual_feasibility_status=*/FEASIBILITY_STATUS_INFEASIBLE);
    case glop::ProblemStatus::INFEASIBLE_OR_UNBOUNDED:
      return InfeasibleOrUnboundedTerminationProto(
          is_maximize,
          /*dual_feasibility_status=*/FEASIBILITY_STATUS_UNDETERMINED);
    case glop::ProblemStatus::INIT:
      // Glop may flip the `interrupt_solve` atomic when it is terminated for a
      // reason other than interruption so we should ignore its value. Instead
      // we use the interrupter.
      // A primal feasible solution is only returned for PRIMAL_FEASIBLE (see
      // comments in FillSolution).
      return NoSolutionFoundTerminationProto(
          is_maximize, interrupter != nullptr && interrupter->IsInterrupted()
                           ? LIMIT_INTERRUPTED
                           : LIMIT_UNDETERMINED);
    case glop::ProblemStatus::DUAL_FEASIBLE:
      // Glop may flip the `interrupt_solve` atomic when it is terminated for a
      // reason other than interruption so we should ignore its value. Instead
      // we use the interrupter.
      // A primal feasible solution is only returned for PRIMAL_FEASIBLE (see
      // comments in FillSolution).
      return NoSolutionFoundTerminationProto(
          is_maximize,
          interrupter != nullptr && interrupter->IsInterrupted()
              ? LIMIT_INTERRUPTED
              : LIMIT_UNDETERMINED,
          objective_value);
    case glop::ProblemStatus::PRIMAL_FEASIBLE:
      // Glop may flip the `interrupt_solve` atomic when it is terminated for a
      // reason other than interruption so we should ignore its value. Instead
      // we use the interrupter.
      // A primal feasible solution is only returned for PRIMAL_FEASIBLE (see
      // comments in FillSolution).
      return FeasibleTerminationProto(
          is_maximize,
          interrupter != nullptr && interrupter->IsInterrupted()
              ? LIMIT_INTERRUPTED
              : LIMIT_UNDETERMINED,
          objective_value);
    case glop::ProblemStatus::IMPRECISE:
      return TerminateForReason(is_maximize, TERMINATION_REASON_IMPRECISE);
    case glop::ProblemStatus::ABNORMAL:
    case glop::ProblemStatus::INVALID_PROBLEM:
      return absl::InternalError(
          absl::StrCat("Unexpected GLOP termination reason: ",
                       glop::GetProblemStatusString(status)));
  }
  LOG(FATAL) << "Unimplemented GLOP termination reason: "
             << glop::GetProblemStatusString(status);
}

// Returns an InvalidArgumentError if the provided parameters are invalid.
absl::Status ValidateGlopParameters(const glop::GlopParameters& parameters) {
  const std::string error = glop::ValidateParameters(parameters);
  if (!error.empty()) {
    return util::InvalidArgumentErrorBuilder()
           << "invalid GlopParameters: " << error;
  }
  return absl::OkStatus();
}

}  // namespace

GlopSolver::GlopSolver() : linear_program_(), lp_solver_() {}

void GlopSolver::AddVariables(const VariablesProto& variables) {
  for (int i = 0; i < NumVariables(variables); ++i) {
    const glop::ColIndex col_index = linear_program_.CreateNewVariable();
    linear_program_.SetVariableBounds(col_index, variables.lower_bounds(i),
                                      variables.upper_bounds(i));
    linear_program_.SetVariableName(col_index, SafeName(variables, i));
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
  util_intops::StrongVector<IndexType, IndexType> new_indices(
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

absl::StatusOr<glop::GlopParameters> GlopSolver::MergeSolveParameters(
    const SolveParametersProto& solve_parameters,
    const bool setting_initial_basis, const bool has_message_callback,
    const bool is_maximization) {
  // Validate first the user specific Glop parameters.
  RETURN_IF_ERROR(ValidateGlopParameters(solve_parameters.glop()))
      << "invalid SolveParametersProto.glop value";

  glop::GlopParameters result = solve_parameters.glop();
  std::vector<std::string> warnings;
  if (!result.has_max_time_in_seconds() && solve_parameters.has_time_limit()) {
    const absl::Duration time_limit =
        util_time::DecodeGoogleApiProto(solve_parameters.time_limit()).value();
    result.set_max_time_in_seconds(absl::ToDoubleSeconds(time_limit));
  }
  if (has_message_callback) {
    // If we have a message callback, we must set log_search_progress to get any
    // logs. We ignore the user's input on specific solver parameters here since
    // it would be confusing to accept a callback but never call it.
    result.set_log_search_progress(true);

    // We don't want the logs to be also printed to stdout when we have a
    // message callback. Here we ignore the user input since message callback
    // can be used in the context of a server and printing to stdout could be a
    // problem.
    result.set_log_to_stdout(false);
  } else if (!result.has_log_search_progress()) {
    result.set_log_search_progress(solve_parameters.enable_output());
  }
  if (!result.has_num_omp_threads() && solve_parameters.has_threads()) {
    result.set_num_omp_threads(solve_parameters.threads());
  }
  if (!result.has_random_seed() && solve_parameters.has_random_seed()) {
    const int random_seed = std::max(0, solve_parameters.random_seed());
    result.set_random_seed(random_seed);
  }
  if (!result.has_max_number_of_iterations() &&
      solve_parameters.iteration_limit()) {
    result.set_max_number_of_iterations(solve_parameters.iteration_limit());
  }
  if (solve_parameters.has_node_limit()) {
    warnings.emplace_back("GLOP does snot support 'node_limit' parameter");
  }
  if (!result.has_use_dual_simplex() &&
      solve_parameters.lp_algorithm() != LP_ALGORITHM_UNSPECIFIED) {
    switch (solve_parameters.lp_algorithm()) {
      case LP_ALGORITHM_PRIMAL_SIMPLEX:
        result.set_use_dual_simplex(false);
        break;
      case LP_ALGORITHM_DUAL_SIMPLEX:
        result.set_use_dual_simplex(true);
        break;
      default:
        warnings.emplace_back(absl::StrCat(
            "GLOP does not support the 'lp_algorithm' parameter value: ",
            LPAlgorithmProto_Name(solve_parameters.lp_algorithm())));
    }
  }
  if (!result.has_use_scaling() && !result.has_scaling_method() &&
      solve_parameters.scaling() != EMPHASIS_UNSPECIFIED) {
    switch (solve_parameters.scaling()) {
      case EMPHASIS_OFF:
        result.set_use_scaling(false);
        break;
      case EMPHASIS_LOW:
      case EMPHASIS_MEDIUM:
      case EMPHASIS_HIGH:
      case EMPHASIS_VERY_HIGH:
        result.set_use_scaling(true);
        result.set_scaling_method(glop::GlopParameters::EQUILIBRATION);
        break;
      default:
        LOG(FATAL) << "Scaling emphasis: "
                   << EmphasisProto_Name(solve_parameters.scaling())
                   << " unknown, error setting GLOP parameters";
    }
  }
  if (setting_initial_basis) {
    result.set_use_preprocessing(false);
  } else if (!result.has_use_preprocessing() &&
             solve_parameters.presolve() != EMPHASIS_UNSPECIFIED) {
    switch (solve_parameters.presolve()) {
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
                   << EmphasisProto_Name(solve_parameters.presolve())
                   << " unknown, error setting GLOP parameters";
    }
  }
  if (solve_parameters.cuts() != EMPHASIS_UNSPECIFIED) {
    warnings.push_back(absl::StrCat(
        "GLOP does not support 'cuts' parameters, but cuts was set to: ",
        EmphasisProto_Name(solve_parameters.cuts())));
  }
  if (solve_parameters.heuristics() != EMPHASIS_UNSPECIFIED) {
    warnings.push_back(
        absl::StrCat("GLOP does not support 'heuristics' parameter, but "
                     "heuristics was set to: ",
                     EmphasisProto_Name(solve_parameters.heuristics())));
  }
  if (solve_parameters.has_cutoff_limit()) {
    warnings.push_back("GLOP does not support 'cutoff_limit' parameter");
  }
  // Solver stops once optimal objective is proven strictly greater than limit.
  // limit.
  const auto set_upper_limit_if_missing = [&result](const double limit) {
    if (!result.has_objective_upper_limit()) {
      result.set_objective_upper_limit(limit);
    }
  };
  // Solver stops once optimal objective is proven strictly less than limit.
  const auto set_lower_limit_if_missing = [&result](const double limit) {
    if (!result.has_objective_lower_limit()) {
      result.set_objective_lower_limit(limit);
    }
  };
  if (solve_parameters.has_objective_limit()) {
    if (is_maximization) {
      set_upper_limit_if_missing(solve_parameters.objective_limit());
    } else {
      set_lower_limit_if_missing(solve_parameters.objective_limit());
    }
  }
  if (solve_parameters.has_best_bound_limit()) {
    if (is_maximization) {
      set_lower_limit_if_missing(solve_parameters.best_bound_limit());
    } else {
      set_upper_limit_if_missing(solve_parameters.best_bound_limit());
    }
  }
  if (solve_parameters.has_solution_limit()) {
    warnings.push_back("GLOP does not support 'solution_limit' parameter");
  }
  if (!warnings.empty()) {
    return absl::InvalidArgumentError(absl::StrJoin(warnings, "; "));
  }

  // Validate the result of the merge. If the parameters are not valid, this is
  // an internal error from MathOpt as user specified Glop parameters have been
  // validated at the beginning of this function. Thus the invalid values are
  // values translated from solve_parameters and this code should not produce
  // invalid parameters.
  RETURN_IF_ERROR(ValidateGlopParameters(result))
      << "invalid GlopParameters generated from SolveParametersProto";

  return result;
}

template <typename IndexType>
SparseDoubleVectorProto FillSparseDoubleVector(
    absl::Span<const int64_t> ids_in_order,
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

// ValueType should be glop's VariableStatus or ConstraintStatus.
template <typename ValueType>
BasisStatusProto FromGlopBasisStatus(const ValueType glop_basis_status) {
  switch (glop_basis_status) {
    case ValueType::BASIC:
      return BasisStatusProto::BASIS_STATUS_BASIC;
    case ValueType::FIXED_VALUE:
      return BasisStatusProto::BASIS_STATUS_FIXED_VALUE;
    case ValueType::AT_LOWER_BOUND:
      return BasisStatusProto::BASIS_STATUS_AT_LOWER_BOUND;
    case ValueType::AT_UPPER_BOUND:
      return BasisStatusProto::BASIS_STATUS_AT_UPPER_BOUND;
    case ValueType::FREE:
      return BasisStatusProto::BASIS_STATUS_FREE;
  }
  return BasisStatusProto::BASIS_STATUS_UNSPECIFIED;
}

template <typename IndexType, typename ValueType>
SparseBasisStatusVector FillSparseBasisStatusVector(
    absl::Span<const int64_t> ids_in_order,
    const absl::flat_hash_map<int64_t, IndexType>& id_map,
    const glop::StrictITIVector<IndexType, ValueType>& values) {
  SparseBasisStatusVector result;
  for (const int64_t variable_id : ids_in_order) {
    const ValueType value = values[id_map.at(variable_id)];
    result.add_ids(variable_id);
    result.add_values(FromGlopBasisStatus(value));
  }
  return result;
}

// ValueType should be glop's VariableStatus or ConstraintStatus.
template <typename ValueType>
ValueType ToGlopBasisStatus(const BasisStatusProto basis_status) {
  switch (basis_status) {
    case BASIS_STATUS_BASIC:
      return ValueType::BASIC;
    case BASIS_STATUS_FIXED_VALUE:
      return ValueType::FIXED_VALUE;
    case BASIS_STATUS_AT_LOWER_BOUND:
      return ValueType::AT_LOWER_BOUND;
    case BASIS_STATUS_AT_UPPER_BOUND:
      return ValueType::AT_UPPER_BOUND;
    case BASIS_STATUS_FREE:
      return ValueType::FREE;
    default:
      LOG(FATAL) << "Unexpected invalid initial_basis.";
      return ValueType::FREE;
  }
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

// Returns a vector of containing the MathOpt id of each row or column. Here T
// is either (Col|Row)Index and id_map is expected to be
// GlopSolver::(linear_constraints_|variables_).
template <typename T>
glop::StrictITIVector<T, int64_t> IndexToId(
    const absl::flat_hash_map<int64_t, T>& id_map) {
  // Guard value used to identify not-yet-set elements of index_to_id.
  constexpr int64_t kEmptyId = -1;
  glop::StrictITIVector<T, int64_t> index_to_id(T(id_map.size()), kEmptyId);
  for (const auto& [id, index] : id_map) {
    CHECK(index >= 0 && index < index_to_id.size()) << index;
    CHECK_EQ(index_to_id[index], kEmptyId);
    index_to_id[index] = id;
  }

  // At this point, index_to_id can't contain any kEmptyId values since
  // index_to_id.size() == id_map.size() and we modified id_map.size() elements
  // in the loop, after checking that the modified element was changed by a
  // previous iteration.
  return index_to_id;
}

InvertedBounds GlopSolver::ListInvertedBounds() const {
  // Identify rows and columns by index first.
  std::vector<glop::ColIndex> inverted_columns;
  const glop::ColIndex num_cols = linear_program_.num_variables();
  for (glop::ColIndex col(0); col < num_cols; ++col) {
    if (linear_program_.variable_lower_bounds()[col] >
        linear_program_.variable_upper_bounds()[col]) {
      inverted_columns.push_back(col);
    }
  }
  std::vector<glop::RowIndex> inverted_rows;
  const glop::RowIndex num_rows = linear_program_.num_constraints();
  for (glop::RowIndex row(0); row < num_rows; ++row) {
    if (linear_program_.constraint_lower_bounds()[row] >
        linear_program_.constraint_upper_bounds()[row]) {
      inverted_rows.push_back(row);
    }
  }

  // Convert column/row indices into MathOpt ids. We avoid calling the expensive
  // IndexToId() when not necessary.
  InvertedBounds inverted_bounds;
  if (!inverted_columns.empty()) {
    const glop::StrictITIVector<glop::ColIndex, int64_t> ids =
        IndexToId(variables_);
    CHECK_EQ(ids.size(), num_cols);
    inverted_bounds.variables.reserve(inverted_columns.size());
    for (const glop::ColIndex col : inverted_columns) {
      inverted_bounds.variables.push_back(ids[col]);
    }
  }
  if (!inverted_rows.empty()) {
    const glop::StrictITIVector<glop::RowIndex, int64_t> ids =
        IndexToId(linear_constraints_);
    CHECK_EQ(ids.size(), num_rows);
    inverted_bounds.linear_constraints.reserve(inverted_rows.size());
    for (const glop::RowIndex row : inverted_rows) {
      inverted_bounds.linear_constraints.push_back(ids[row]);
    }
  }

  return inverted_bounds;
}

void GlopSolver::FillSolution(const glop::ProblemStatus status,
                              const ModelSolveParametersProto& model_parameters,
                              SolveResultProto& solve_result) {
  // Meaningful solutions are available if optimality is proven in
  // preprocessing or after 1 simplex iteration.
  // TODO(b/195295177): Discuss what to do with glop::ProblemStatus::IMPRECISE
  // looks like it may be set also when rays are imprecise.
  const bool phase_I_solution_available =
      (status == glop::ProblemStatus::INIT) &&
      (lp_solver_.GetNumberOfSimplexIterations() > 0);
  if (status != glop::ProblemStatus::OPTIMAL &&
      status != glop::ProblemStatus::PRIMAL_FEASIBLE &&
      status != glop::ProblemStatus::DUAL_FEASIBLE &&
      status != glop::ProblemStatus::PRIMAL_UNBOUNDED &&
      status != glop::ProblemStatus::DUAL_UNBOUNDED &&
      !phase_I_solution_available) {
    return;
  }
  auto sorted_variables = GetSortedIs(variables_);
  auto sorted_constraints = GetSortedIs(linear_constraints_);
  SolutionProto* const solution = solve_result.add_solutions();
  BasisProto* const basis = solution->mutable_basis();
  PrimalSolutionProto* const primal_solution =
      solution->mutable_primal_solution();
  DualSolutionProto* const dual_solution = solution->mutable_dual_solution();

  // Fill in feasibility statuses
  // Note: if we reach here and status != OPTIMAL, then at least 1 simplex
  // iteration has been executed.
  if (status == glop::ProblemStatus::OPTIMAL) {
    primal_solution->set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
    basis->set_basic_dual_feasibility(SOLUTION_STATUS_FEASIBLE);
    dual_solution->set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
  } else if (status == glop::ProblemStatus::PRIMAL_FEASIBLE) {
    // Solve reached phase II of primal simplex and current basis is not
    // optimal. Hence basis is primal feasible, but cannot be dual feasible.
    // Dual solution could still be feasible.
    primal_solution->set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
    dual_solution->set_feasibility_status(SOLUTION_STATUS_UNDETERMINED);
    basis->set_basic_dual_feasibility(SOLUTION_STATUS_INFEASIBLE);
  } else if (status == glop::ProblemStatus::DUAL_FEASIBLE) {
    // Solve reached phase II of dual simplex and current basis is not optimal.
    // Hence basis is dual feasible, but cannot be primal feasible. In addition,
    // glop applies dual feasibility correction in dual simplex so feasibility
    // of the dual solution matches dual feasibility of the basis.
    // TODO(b/195295177): confirm with fdid
    primal_solution->set_feasibility_status(SOLUTION_STATUS_INFEASIBLE);
    dual_solution->set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
    basis->set_basic_dual_feasibility(SOLUTION_STATUS_FEASIBLE);
  } else {  // status == INIT
    // Phase I of primal or dual simplex ran for at least one iteration
    if (lp_solver_.GetParameters().use_dual_simplex()) {
      // Phase I did not finish so basis is not dual feasible. In addition,
      // glop applies dual feasibility correction so feasibility of the dual
      // solution matches dual feasibility of the basis.
      primal_solution->set_feasibility_status(SOLUTION_STATUS_UNDETERMINED);
      dual_solution->set_feasibility_status(SOLUTION_STATUS_INFEASIBLE);
      basis->set_basic_dual_feasibility(SOLUTION_STATUS_INFEASIBLE);
    } else {
      // Phase I did not finish so basis is not primal feasible.
      primal_solution->set_feasibility_status(SOLUTION_STATUS_INFEASIBLE);
      dual_solution->set_feasibility_status(SOLUTION_STATUS_UNDETERMINED);
      basis->set_basic_dual_feasibility(SOLUTION_STATUS_UNDETERMINED);
    }
  }

  // Fill in objective values
  primal_solution->set_objective_value(lp_solver_.GetObjectiveValue());
  if (basis->basic_dual_feasibility() == SOLUTION_STATUS_FEASIBLE) {
    // Primal and dual objectives are the same for a dual feasible basis.
    dual_solution->set_objective_value(primal_solution->objective_value());
  }

  // Fill solution and basis
  *basis->mutable_constraint_status() = *basis->mutable_variable_status() =
      FillSparseBasisStatusVector(sorted_variables, variables_,
                                  lp_solver_.variable_statuses());
  *basis->mutable_constraint_status() =
      FillSparseBasisStatusVector(sorted_constraints, linear_constraints_,
                                  lp_solver_.constraint_statuses());

  *primal_solution->mutable_variable_values() = FillSparseDoubleVector(
      sorted_variables, variables_, lp_solver_.variable_values(),
      model_parameters.variable_values_filter());

  *dual_solution->mutable_dual_values() = FillSparseDoubleVector(
      sorted_constraints, linear_constraints_, lp_solver_.dual_values(),
      model_parameters.dual_values_filter());
  *dual_solution->mutable_reduced_costs() = FillSparseDoubleVector(
      sorted_variables, variables_, lp_solver_.reduced_costs(),
      model_parameters.reduced_costs_filter());

  if (!lp_solver_.primal_ray().empty()) {
    PrimalRayProto* const primal_ray = solve_result.add_primal_rays();

    *primal_ray->mutable_variable_values() = FillSparseDoubleVector(
        sorted_variables, variables_, lp_solver_.primal_ray(),
        model_parameters.variable_values_filter());
  }
  if (!lp_solver_.constraints_dual_ray().empty() &&
      !lp_solver_.variable_bounds_dual_ray().empty()) {
    DualRayProto* const dual_ray = solve_result.add_dual_rays();
    *dual_ray->mutable_dual_values() =
        FillSparseDoubleVector(sorted_constraints, linear_constraints_,
                               lp_solver_.constraints_dual_ray(),
                               model_parameters.dual_values_filter());
    *dual_ray->mutable_reduced_costs() = FillSparseDoubleVector(
        sorted_variables, variables_, lp_solver_.variable_bounds_dual_ray(),
        model_parameters.reduced_costs_filter());
  }
}

absl::Status GlopSolver::FillSolveStats(const absl::Duration solve_time,
                                        SolveStatsProto& solve_stats) {
  // Fill remaining stats
  solve_stats.set_simplex_iterations(lp_solver_.GetNumberOfSimplexIterations());
  RETURN_IF_ERROR(util_time::EncodeGoogleApiProto(
      solve_time, solve_stats.mutable_solve_time()));

  return absl::OkStatus();
}

absl::StatusOr<SolveResultProto> GlopSolver::MakeSolveResult(
    const glop::ProblemStatus status,
    const ModelSolveParametersProto& model_parameters,
    const SolveInterrupter* absl_nullable const interrupter,
    const absl::Duration solve_time) {
  SolveResultProto solve_result;
  ASSIGN_OR_RETURN(*solve_result.mutable_termination(),
                   BuildTermination(status, interrupter,
                                    linear_program_.IsMaximizationProblem(),
                                    lp_solver_.GetObjectiveValue()));
  FillSolution(status, model_parameters, solve_result);
  RETURN_IF_ERROR(
      FillSolveStats(solve_time, *solve_result.mutable_solve_stats()));
  return solve_result;
}

void GlopSolver::SetGlopBasis(const BasisProto& basis) {
  glop::VariableStatusRow variable_statuses(linear_program_.num_variables());
  for (const auto [id, value] : MakeView(basis.variable_status())) {
    variable_statuses[variables_.at(id)] =
        ToGlopBasisStatus<glop::VariableStatus>(
            static_cast<BasisStatusProto>(value));
  }
  glop::ConstraintStatusColumn constraint_statuses(
      linear_program_.num_constraints());
  for (const auto [id, value] : MakeView(basis.constraint_status())) {
    constraint_statuses[linear_constraints_.at(id)] =
        ToGlopBasisStatus<glop::ConstraintStatus>(
            static_cast<BasisStatusProto>(value));
  }
  lp_solver_.SetInitialBasis(variable_statuses, constraint_statuses);
}

absl::StatusOr<SolveResultProto> GlopSolver::Solve(
    const SolveParametersProto& parameters,
    const ModelSolveParametersProto& model_parameters,
    const MessageCallback message_cb,
    const CallbackRegistrationProto& callback_registration, const Callback,
    const SolveInterrupter* absl_nullable const interrupter) {
  RETURN_IF_ERROR(ModelSolveParametersAreSupported(
      model_parameters, kGlopSupportedStructures, "Glop"));
  RETURN_IF_ERROR(CheckRegisteredCallbackEvents(callback_registration,
                                                /*supported_events=*/{}));

  const absl::Time start = absl::Now();
  ASSIGN_OR_RETURN(
      const glop::GlopParameters glop_parameters,
      MergeSolveParameters(
          parameters,
          /*setting_initial_basis=*/model_parameters.has_initial_basis(),
          /*has_message_callback=*/message_cb != nullptr,
          linear_program_.IsMaximizationProblem()));
  lp_solver_.SetParameters(glop_parameters);

  if (model_parameters.has_initial_basis()) {
    SetGlopBasis(model_parameters.initial_basis());
  }

  std::atomic<bool> interrupt_solve = false;
  const std::unique_ptr<TimeLimit> time_limit =
      TimeLimit::FromParameters(lp_solver_.GetParameters());
  time_limit->RegisterExternalBooleanAsLimit(&interrupt_solve);

  const ScopedSolveInterrupterCallback scoped_interrupt_cb(interrupter, [&]() {
    CHECK_NE(interrupter, nullptr);
    interrupt_solve = true;
  });

  if (message_cb != nullptr) {
    // Please note that the logging is enabled in MergeSolveParameters() where
    // we also disable logging to stdout. We can't modify the SolverLogger here
    // since the values are overwritten from the parameters at the beginning of
    // the solve.
    //
    // Here we test that there are no other callbacks since we will clear them
    // all in the cleanup below.
    CHECK_EQ(lp_solver_.GetSolverLogger().NumInfoLoggingCallbacks(), 0);
    lp_solver_.GetSolverLogger().AddInfoLoggingCallback(
        [&](absl::string_view message) {
          message_cb(absl::StrSplit(message, '\n'));
        });
  }
  const auto message_cb_cleanup = absl::MakeCleanup([&]() {
    if (message_cb != nullptr) {
      // Check that no other callbacks have been added to the logger.
      CHECK_EQ(lp_solver_.GetSolverLogger().NumInfoLoggingCallbacks(), 1);
      lp_solver_.GetSolverLogger().ClearInfoLoggingCallbacks();
    }
  });

  // Glop returns an error when bounds are inverted and does not list the
  // offending variables/constraints. Here we want to return a more detailed
  // status.
  RETURN_IF_ERROR(ListInvertedBounds().ToStatus());

  const glop::ProblemStatus status =
      lp_solver_.SolveWithTimeLimit(linear_program_, time_limit.get());
  const absl::Duration solve_time = absl::Now() - start;
  return MakeSolveResult(status, model_parameters, interrupter, solve_time);
}

absl::StatusOr<std::unique_ptr<SolverInterface>> GlopSolver::New(
    const ModelProto& model, const InitArgs&) {
  RETURN_IF_ERROR(ModelIsSupported(model, kGlopSupportedStructures, "Glop"));
  auto solver = absl::WrapUnique(new GlopSolver);
  // By default Glop CHECKs that bounds are always consistent (lb < ub); thus it
  // would fail if the initial model or later updates temporarily set inverted
  // bounds.
  solver->linear_program_.SetDcheckBounds(false);

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

absl::StatusOr<bool> GlopSolver::Update(const ModelUpdateProto& model_update) {
  if (!UpdateIsSupported(model_update, kGlopSupportedStructures)) {
    return false;
  }

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

  return true;
}

absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
GlopSolver::ComputeInfeasibleSubsystem(const SolveParametersProto&,
                                       MessageCallback,
                                       const SolveInterrupter* absl_nullable) {
  return absl::UnimplementedError(
      "GLOP does not implement a method to compute an infeasible subsystem");
}

MATH_OPT_REGISTER_SOLVER(SOLVER_TYPE_GLOP, GlopSolver::New)

}  // namespace math_opt
}  // namespace operations_research
