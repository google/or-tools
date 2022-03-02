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

#include "ortools/math_opt/solvers/gscip_solver.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/cleanup.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/status_macros.h"
#include "ortools/gscip/gscip.h"
#include "ortools/gscip/gscip.pb.h"
#include "ortools/gscip/gscip_event_handler.h"
#include "ortools/gscip/gscip_parameters.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/solve_interrupter.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/core/sparse_submatrix.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/solvers/gscip_solver_callback.h"
#include "ortools/math_opt/solvers/gscip_solver_message_callback_handler.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/callback_validator.h"
#include "ortools/port/proto_utils.h"
#include "scip/scip.h"
#include "scip/type_cons.h"
#include "scip/type_event.h"
#include "scip/type_var.h"

namespace operations_research {
namespace math_opt {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

int64_t SafeId(const VariablesProto& variables, int index) {
  if (variables.ids().empty()) {
    return index;
  }
  return variables.ids(index);
}

const std::string& EmptyString() {
  static const std::string* const empty_string = new std::string;
  return *empty_string;
}

const std::string& SafeName(const VariablesProto& variables, int index) {
  if (variables.names().empty()) {
    return EmptyString();
  }
  return variables.names(index);
}

int64_t SafeId(const LinearConstraintsProto& linear_constraints, int index) {
  if (linear_constraints.ids().empty()) {
    return index;
  }
  return linear_constraints.ids(index);
}

const std::string& SafeName(const LinearConstraintsProto& linear_constraints,
                            int index) {
  if (linear_constraints.names().empty()) {
    return EmptyString();
  }
  return linear_constraints.names(index);
}

absl::flat_hash_map<int64_t, double> SparseDoubleVectorAsMap(
    const SparseDoubleVectorProto& vector) {
  CHECK_EQ(vector.ids_size(), vector.values_size());
  absl::flat_hash_map<int64_t, double> result;
  result.reserve(vector.ids_size());
  for (int i = 0; i < vector.ids_size(); ++i) {
    result[vector.ids(i)] = vector.values(i);
  }
  return result;
}

// Viewing matrix as a list of (row, column, value) tuples stored in row major
// order, does a linear scan from index scan_start to find the index of the
// first entry with row >= row_id. Returns the size the tuple list if there is
// no such entry.
inline int FindRowStart(const SparseDoubleMatrixProto& matrix,
                        const int64_t row_id, const int scan_start) {
  int result = scan_start;
  while (result < matrix.row_ids_size() && matrix.row_ids(result) < row_id) {
    ++result;
  }
  return result;
}

struct LinearConstraintView {
  int64_t linear_constraint_id;
  double lower_bound;
  double upper_bound;
  absl::string_view name;
  absl::Span<const int64_t> variable_ids;
  absl::Span<const double> coefficients;
};

// Iterates over the constraints from a LinearConstraints, outputting a
// LinearConstraintView for each constraint. Requires a SparseDoubleMatrixProto
// which may have data from additional constraints not in LinearConstraints.
//
// The running time to iterate through and read each element once is
// O(Size(*linear_constraints) + Size(*linear_constraint_matrix)).
class LinearConstraintIterator {
 public:
  LinearConstraintIterator(
      const LinearConstraintsProto* linear_constraints,
      const SparseDoubleMatrixProto* linear_constraint_matrix)
      : linear_constraints_(ABSL_DIE_IF_NULL(linear_constraints)),
        linear_constraint_matrix_(ABSL_DIE_IF_NULL(linear_constraint_matrix)) {
    if (NumConstraints(*linear_constraints_) > 0) {
      const int64_t first_constraint = SafeId(*linear_constraints_, 0);
      matrix_start_ =
          FindRowStart(*linear_constraint_matrix_, first_constraint, 0);
      matrix_end_ = FindRowStart(*linear_constraint_matrix_,
                                 first_constraint + 1, matrix_start_);
    } else {
      matrix_start_ = NumMatrixNonzeros(*linear_constraint_matrix_);
      matrix_end_ = matrix_start_;
    }
  }

  bool IsDone() const {
    return current_con_ >= NumConstraints(*linear_constraints_);
  }

  // Call only if !IsDone(). Runs in O(1).
  LinearConstraintView Current() const {
    CHECK(!IsDone());
    LinearConstraintView result;
    result.lower_bound = linear_constraints_->lower_bounds(current_con_);
    result.upper_bound = linear_constraints_->upper_bounds(current_con_);
    result.name = SafeName(*linear_constraints_, current_con_);
    result.linear_constraint_id = SafeId(*linear_constraints_, current_con_);

    const auto vars_begin = linear_constraint_matrix_->column_ids().data();
    result.variable_ids = absl::MakeConstSpan(vars_begin + matrix_start_,
                                              vars_begin + matrix_end_);
    const auto coefficients_begins =
        linear_constraint_matrix_->coefficients().data();
    result.coefficients = absl::MakeConstSpan(
        coefficients_begins + matrix_start_, coefficients_begins + matrix_end_);
    return result;
  }

  // Call only if !IsDone().
  void Next() {
    CHECK(!IsDone());
    ++current_con_;
    if (IsDone()) {
      matrix_start_ = NumMatrixNonzeros(*linear_constraint_matrix_);
      matrix_end_ = matrix_start_;
      return;
    }
    const int64_t current_row_id = SafeId(*linear_constraints_, current_con_);
    matrix_start_ =
        FindRowStart(*linear_constraint_matrix_, current_row_id, matrix_end_);

    matrix_end_ = FindRowStart(*linear_constraint_matrix_, current_row_id + 1,
                               matrix_start_);
  }

 private:
  // NOT OWNED
  const LinearConstraintsProto* const linear_constraints_;
  // NOT OWNED
  const SparseDoubleMatrixProto* const linear_constraint_matrix_;
  // An index into linear_constraints_, the constraint currently being viewed,
  // or Size(linear_constraints_) when IsDone().
  int current_con_ = 0;

  // Informal: the interval [matrix_start_, matrix_end_) gives the indices in
  // linear_constraint_matrix_ for linear_constraints_[current_con_]
  //
  // Invariant: if !IsDone():
  //   * matrix_start_: the first index in linear_constraint_matrix_ with row id
  //     >= RowId(linear_constraints_[current_con_])
  //   * matrix_end_: the first index in linear_constraint_matrix_ with row id
  //     >= RowId(linear_constraints_[current_con_]) + 1
  //
  // Implementation note: matrix_start_ and matrix_end_ equal
  // Size(linear_constraint_matrix_) when IsDone().
  int matrix_start_ = 0;
  int matrix_end_ = 0;
};

inline GScipVarType GScipVarTypeFromIsInteger(const bool is_integer) {
  return is_integer ? GScipVarType::kInteger : GScipVarType::kContinuous;
}

// Used to delay the evaluation of a costly computation until the first time it
// is actually needed.
//
// The typical use is when we have two independent branches that need the same
// data but we don't want to compute these data if we don't enter any of those
// branches.
//
// Usage:
//   LazyInitialized<Xxx> xxx([&]() {
//     return Xxx(...);
//   });
//
//   if (predicate_1) {
//     ...
//     f(xxx.GetOrCreate());
//     ...
//   }
//   if (predicate_2) {
//     ...
//     f(xxx.GetOrCreate());
//     ...
//   }
template <typename T>
class LazyInitialized {
 public:
  // Checks that the input initializer is not nullptr.
  explicit LazyInitialized(std::function<T()> initializer)
      : initializer_(ABSL_DIE_IF_NULL(initializer)) {}

  // Returns the value returned by initializer(), calling it the first time.
  const T& GetOrCreate() {
    if (!value_) {
      value_ = initializer_();
    }
    return *value_;
  }

 private:
  const std::function<T()> initializer_;
  std::optional<T> value_;
};

template <typename T>
SparseDoubleVectorProto FillSparseDoubleVector(
    const std::vector<int64_t>& ids_in_order,
    const absl::flat_hash_map<int64_t, T>& id_map,
    const absl::flat_hash_map<T, double>& value_map,
    const SparseVectorFilterProto& filter) {
  SparseVectorFilterPredicate predicate(filter);
  SparseDoubleVectorProto result;
  for (const int64_t variable_id : ids_in_order) {
    const double value = value_map.at(id_map.at(variable_id));
    if (predicate.AcceptsAndUpdate(variable_id, value)) {
      result.add_ids(variable_id);
      result.add_values(value);
    }
  }
  return result;
}

}  // namespace

absl::Status GScipSolver::AddVariables(
    const VariablesProto& variables,
    const absl::flat_hash_map<int64_t, double>& linear_objective_coefficients) {
  for (int i = 0; i < NumVariables(variables); ++i) {
    const int64_t id = SafeId(variables, i);
    // SCIP is failing with an assert in SCIPcreateVar() when input bounds are
    // inverted. That said, it is not an issue if the bounds are created
    // non-inverted and later changed. Thus here we use this hack to bypass the
    // assertion in this corner case.
    const bool inverted_bounds =
        variables.lower_bounds(i) > variables.upper_bounds(i);
    ASSIGN_OR_RETURN(
        SCIP_VAR* const v,
        gscip_->AddVariable(
            variables.lower_bounds(i),
            inverted_bounds ? variables.lower_bounds(i)
                            : variables.upper_bounds(i),
            gtl::FindWithDefault(linear_objective_coefficients, id),
            GScipVarTypeFromIsInteger(variables.integers(i)),
            SafeName(variables, i)));
    if (inverted_bounds) {
      RETURN_IF_ERROR(gscip_->SetUb(v, variables.upper_bounds(i)));
    }
    gtl::InsertOrDie(&variables_, id, v);
  }
  return absl::OkStatus();
}

absl::Status GScipSolver::UpdateVariables(
    const VariableUpdatesProto& variable_updates) {
  for (const auto [id, lb] : MakeView(variable_updates.lower_bounds())) {
    RETURN_IF_ERROR(gscip_->SetLb(variables_.at(id), lb));
  }
  for (const auto [id, ub] : MakeView(variable_updates.upper_bounds())) {
    RETURN_IF_ERROR(gscip_->SetUb(variables_.at(id), ub));
  }
  for (const auto [id, is_integer] : MakeView(variable_updates.integers())) {
    RETURN_IF_ERROR(gscip_->SetVarType(variables_.at(id),
                                       GScipVarTypeFromIsInteger(is_integer)));
  }
  return absl::OkStatus();
}

absl::Status GScipSolver::AddLinearConstraints(
    const LinearConstraintsProto& linear_constraints,
    const SparseDoubleMatrixProto& linear_constraint_matrix) {
  for (LinearConstraintIterator lin_con_it(&linear_constraints,
                                           &linear_constraint_matrix);
       !lin_con_it.IsDone(); lin_con_it.Next()) {
    const LinearConstraintView current = lin_con_it.Current();

    GScipLinearRange range;
    range.lower_bound = current.lower_bound;
    range.upper_bound = current.upper_bound;
    range.coefficients = std::vector<double>(current.coefficients.begin(),
                                             current.coefficients.end());
    range.variables.reserve(current.variable_ids.size());
    for (const int64_t var_id : current.variable_ids) {
      range.variables.push_back(variables_.at(var_id));
    }
    ASSIGN_OR_RETURN(
        SCIP_CONS* const scip_con,
        gscip_->AddLinearConstraint(range, std::string(current.name)));
    gtl::InsertOrDie(&linear_constraints_, current.linear_constraint_id,
                     scip_con);
  }
  return absl::OkStatus();
}

absl::Status GScipSolver::UpdateLinearConstraints(
    const LinearConstraintUpdatesProto linear_constraint_updates,
    const SparseDoubleMatrixProto& linear_constraint_matrix,
    const std::optional<int64_t> first_new_var_id,
    const std::optional<int64_t> first_new_cstr_id) {
  for (const auto [id, lb] :
       MakeView(linear_constraint_updates.lower_bounds())) {
    RETURN_IF_ERROR(
        gscip_->SetLinearConstraintLb(linear_constraints_.at(id), lb));
  }
  for (const auto [id, ub] :
       MakeView(linear_constraint_updates.upper_bounds())) {
    RETURN_IF_ERROR(
        gscip_->SetLinearConstraintUb(linear_constraints_.at(id), ub));
  }
  for (const auto& [lin_con_id, var_coeffs] : SparseSubmatrixByRows(
           linear_constraint_matrix, /*start_row_id=*/0,
           /*end_row_id=*/first_new_cstr_id, /*start_col_id=*/0,
           /*end_col_id=*/first_new_var_id)) {
    for (const auto& [var_id, value] : var_coeffs) {
      RETURN_IF_ERROR(gscip_->SetLinearConstraintCoef(
          linear_constraints_.at(lin_con_id), variables_.at(var_id), value));
    }
  }
  return absl::OkStatus();
}

GScipParameters::MetaParamValue ConvertMathOptEmphasis(EmphasisProto emphasis) {
  switch (emphasis) {
    case EMPHASIS_OFF:
      return GScipParameters::OFF;
    case EMPHASIS_LOW:
      return GScipParameters::FAST;
    case EMPHASIS_MEDIUM:
    case EMPHASIS_UNSPECIFIED:
      return GScipParameters::DEFAULT_META_PARAM_VALUE;
    case EMPHASIS_HIGH:
    case EMPHASIS_VERY_HIGH:
      return GScipParameters::AGGRESSIVE;
    default:
      LOG(FATAL) << "Unsupported MathOpt Emphasis value: "
                 << ProtoEnumToString(emphasis)
                 << " unknown, error setting gSCIP parameters";
  }
}

absl::StatusOr<GScipParameters> GScipSolver::MergeParameters(
    const SolveParametersProto& solve_parameters) {
  // First build the result by translating common parameters to a
  // GScipParameters, and then merging with user provided gscip_parameters.
  // This results in user provided solver specific parameters overwriting
  // common parameters should there be any conflict.
  GScipParameters result;
  std::vector<std::string> warnings;

  // By default SCIP catches Ctrl-C but we don't want this behavior when the
  // users uses SCIP through MathOpt.
  GScipSetCatchCtrlC(false, &result);

  if (solve_parameters.has_time_limit()) {
    GScipSetTimeLimit(
        util_time::DecodeGoogleApiProto(solve_parameters.time_limit()).value(),
        &result);
  }

  if (solve_parameters.has_threads()) {
    GScipSetMaxNumThreads(solve_parameters.threads(), &result);
  }

  if (solve_parameters.has_relative_gap_tolerance()) {
    (*result.mutable_real_params())["limits/gap"] =
        solve_parameters.relative_gap_tolerance();
  }

  if (solve_parameters.has_absolute_gap_tolerance()) {
    (*result.mutable_real_params())["limits/absgap"] =
        solve_parameters.absolute_gap_tolerance();
  }
  if (solve_parameters.has_node_limit()) {
    (*result.mutable_long_params())["limits/totalnodes"] =
        solve_parameters.node_limit();
  }

  if (solve_parameters.has_objective_limit()) {
    warnings.push_back("parameter objective_limit not supported for gSCIP.");
  }
  if (solve_parameters.has_best_bound_limit()) {
    warnings.push_back("parameter best_bound_limit not supported for gSCIP.");
  }

  if (solve_parameters.has_cutoff_limit()) {
    result.set_objective_limit(solve_parameters.cutoff_limit());
  }

  if (solve_parameters.has_solution_limit()) {
    (*result.mutable_int_params())["limits/solutions"] =
        solve_parameters.solution_limit();
  }

  // GScip has also GScipSetOutputEnabled() but this changes the log
  // level. Setting `silence_output` sets the `quiet` field on the default
  // message handler of SCIP which removes the output. Here it is important to
  // use this rather than changing the log level so that if the user registers
  // for CALLBACK_EVENT_MESSAGE they do get some messages even when
  // `enable_output` is false.
  result.set_silence_output(!solve_parameters.enable_output());

  if (solve_parameters.has_random_seed()) {
    GScipSetRandomSeed(&result, solve_parameters.random_seed());
  }

  if (solve_parameters.lp_algorithm() != LP_ALGORITHM_UNSPECIFIED) {
    char alg;
    switch (solve_parameters.lp_algorithm()) {
      case LP_ALGORITHM_PRIMAL_SIMPLEX:
        alg = 'p';
        break;
      case LP_ALGORITHM_DUAL_SIMPLEX:
        alg = 'd';
        break;
      case LP_ALGORITHM_BARRIER:
        alg = 'c';
        break;
      default:
        LOG(FATAL) << "LPAlgorithm: "
                   << ProtoEnumToString(solve_parameters.lp_algorithm())
                   << " unknown, error setting gSCIP parameters";
    }
    (*result.mutable_char_params())["lp/initalgorithm"] = alg;
  }

  if (solve_parameters.cuts() != EMPHASIS_UNSPECIFIED) {
    result.set_separating(ConvertMathOptEmphasis(solve_parameters.cuts()));
  }
  if (solve_parameters.heuristics() != EMPHASIS_UNSPECIFIED) {
    result.set_heuristics(
        ConvertMathOptEmphasis(solve_parameters.heuristics()));
  }
  if (solve_parameters.presolve() != EMPHASIS_UNSPECIFIED) {
    result.set_presolve(ConvertMathOptEmphasis(solve_parameters.presolve()));
  }
  if (solve_parameters.scaling() != EMPHASIS_UNSPECIFIED) {
    int scaling_value;
    switch (solve_parameters.scaling()) {
      case EMPHASIS_OFF:
        scaling_value = 0;
        break;
      case EMPHASIS_LOW:
      case EMPHASIS_MEDIUM:
        scaling_value = 1;
        break;
      case EMPHASIS_HIGH:
      case EMPHASIS_VERY_HIGH:
        scaling_value = 2;
        break;
      default:
        LOG(FATAL) << "Scaling emphasis: "
                   << ProtoEnumToString(solve_parameters.scaling())
                   << " unknown, error setting gSCIP parameters";
    }
    (*result.mutable_int_params())["lp/scaling"] = scaling_value;
  }

  result.MergeFrom(solve_parameters.gscip());

  if (!warnings.empty()) {
    return absl::InvalidArgumentError(absl::StrJoin(warnings, "; "));
  }
  return result;
}

namespace {

std::string JoinDetails(const std::string& gscip_detail,
                        const std::string& math_opt_detail) {
  if (gscip_detail.empty()) {
    return math_opt_detail;
  }
  if (math_opt_detail.empty()) {
    return gscip_detail;
  }
  return absl::StrCat(gscip_detail, "; ", math_opt_detail);
}

ProblemStatusProto GetProblemStatusProto(const GScipOutput::Status gscip_status,
                                         const bool has_feasible_solution,
                                         const bool has_finite_dual_bound,
                                         const bool was_cutoff) {
  ProblemStatusProto problem_status;
  if (has_feasible_solution) {
    problem_status.set_primal_status(FEASIBILITY_STATUS_FEASIBLE);
  } else {
    problem_status.set_primal_status(FEASIBILITY_STATUS_UNDETERMINED);
  }
  problem_status.set_dual_status(FEASIBILITY_STATUS_UNDETERMINED);

  switch (gscip_status) {
    case GScipOutput::OPTIMAL:
      problem_status.set_dual_status(FEASIBILITY_STATUS_FEASIBLE);
      break;
    case GScipOutput::INFEASIBLE:
      if (!was_cutoff) {
        problem_status.set_primal_status(FEASIBILITY_STATUS_INFEASIBLE);
      }
      break;
    case GScipOutput::UNBOUNDED:
      problem_status.set_dual_status(FEASIBILITY_STATUS_INFEASIBLE);
      break;
    case GScipOutput::INF_OR_UNBD:
      problem_status.set_primal_or_dual_infeasible(true);
      break;
    default:
      break;
  }
  if (has_finite_dual_bound) {
    problem_status.set_dual_status(FEASIBILITY_STATUS_FEASIBLE);
  }
  return problem_status;
}

absl::StatusOr<TerminationProto> ConvertTerminationReason(
    const GScipOutput::Status gscip_status,
    const std::string& gscip_status_detail, const bool has_feasible_solution,
    const bool had_cutoff) {
  switch (gscip_status) {
    case GScipOutput::USER_INTERRUPT:
      return TerminateForLimit(
          LIMIT_INTERRUPTED, /*feasible=*/has_feasible_solution,
          JoinDetails(gscip_status_detail,
                      "underlying gSCIP status: USER_INTERRUPT"));
    case GScipOutput::NODE_LIMIT:
      return TerminateForLimit(
          LIMIT_NODE, /*feasible=*/has_feasible_solution,
          JoinDetails(gscip_status_detail,
                      "underlying gSCIP status: NODE_LIMIT"));
    case GScipOutput::TOTAL_NODE_LIMIT:
      return TerminateForLimit(
          LIMIT_NODE, /*feasible=*/has_feasible_solution,
          JoinDetails(gscip_status_detail,
                      "underlying gSCIP status: TOTAL_NODE_LIMIT"));
    case GScipOutput::STALL_NODE_LIMIT:
      return TerminateForLimit(LIMIT_SLOW_PROGRESS,
                               /*feasible=*/has_feasible_solution,
                               gscip_status_detail);
    case GScipOutput::TIME_LIMIT:
      return TerminateForLimit(LIMIT_TIME, /*feasible=*/has_feasible_solution,
                               gscip_status_detail);
    case GScipOutput::MEM_LIMIT:
      return TerminateForLimit(LIMIT_MEMORY, /*feasible=*/has_feasible_solution,
                               gscip_status_detail);
    case GScipOutput::SOL_LIMIT:
      return TerminateForLimit(
          LIMIT_SOLUTION, /*feasible=*/has_feasible_solution,
          JoinDetails(gscip_status_detail,
                      "underlying gSCIP status: SOL_LIMIT"));
    case GScipOutput::BEST_SOL_LIMIT:
      return TerminateForLimit(
          LIMIT_SOLUTION, /*feasible=*/has_feasible_solution,
          JoinDetails(gscip_status_detail,
                      "underlying gSCIP status: BEST_SOL_LIMIT"));
    case GScipOutput::RESTART_LIMIT:
      return TerminateForLimit(
          LIMIT_OTHER, /*feasible=*/has_feasible_solution,
          JoinDetails(gscip_status_detail,
                      "underlying gSCIP status: RESTART_LIMIT"));
    case GScipOutput::OPTIMAL:
      return TerminateForReason(
          TERMINATION_REASON_OPTIMAL,
          JoinDetails(gscip_status_detail, "underlying gSCIP status: OPTIMAL"));
    case GScipOutput::GAP_LIMIT:
      return TerminateForReason(
          TERMINATION_REASON_OPTIMAL,
          JoinDetails(gscip_status_detail,
                      "underlying gSCIP status: GAP_LIMIT"));
    case GScipOutput::INFEASIBLE:
      if (had_cutoff) {
        return TerminateForLimit(LIMIT_CUTOFF,
                                 /*feasible=*/false, gscip_status_detail);
      } else {
        return TerminateForReason(TERMINATION_REASON_INFEASIBLE,
                                  gscip_status_detail);
      }
    case GScipOutput::UNBOUNDED: {
      if (has_feasible_solution) {
        return TerminateForReason(
            TERMINATION_REASON_UNBOUNDED,
            JoinDetails(gscip_status_detail,
                        "underlying gSCIP status was UNBOUNDED, both primal "
                        "ray and feasible solution are present"));
      } else {
        return TerminateForReason(
            TERMINATION_REASON_INFEASIBLE_OR_UNBOUNDED,
            JoinDetails(
                gscip_status_detail,
                "underlying gSCIP status was UNBOUNDED, but only primal ray "
                "was given, no feasible solution was found"));
      }
    }

    case GScipOutput::INF_OR_UNBD:
      return TerminateForReason(
          TERMINATION_REASON_INFEASIBLE_OR_UNBOUNDED,
          JoinDetails(gscip_status_detail,
                      "underlying gSCIP status: INF_OR_UNBD"));

    case GScipOutput::TERMINATE:
      return TerminateForLimit(
          LIMIT_INTERRUPTED, /*feasible=*/has_feasible_solution,
          JoinDetails(gscip_status_detail,
                      "underlying gSCIP status: TERMINATE"));
    case GScipOutput::INVALID_SOLVER_PARAMETERS:
      return absl::InvalidArgumentError(gscip_status_detail);
    case GScipOutput::UNKNOWN:
      return absl::InternalError(JoinDetails(
          gscip_status_detail, "Unexpected GScipOutput.status: UNKNOWN"));
    default:
      return absl::InternalError(JoinDetails(
          gscip_status_detail, absl::StrCat("Missing GScipOutput.status case: ",
                                            ProtoEnumToString(gscip_status))));
  }
}

}  // namespace

absl::StatusOr<SolveResultProto> GScipSolver::CreateSolveResultProto(
    GScipResult gscip_result, const ModelSolveParametersProto& model_parameters,
    const std::optional<double> cutoff) {
  SolveResultProto solve_result;
  const bool is_maximize = gscip_->ObjectiveIsMaximize();
  // When an objective limit is set, SCIP returns the solutions worse than the
  // limit, we need to filter these out manually.
  const auto meets_cutoff = [cutoff, is_maximize](const double obj_value) {
    if (!cutoff.has_value()) {
      return true;
    }
    if (is_maximize) {
      return obj_value >= *cutoff;
    } else {
      return obj_value <= *cutoff;
    }
  };

  LazyInitialized<std::vector<int64_t>> sorted_variables([&]() {
    std::vector<int64_t> sorted;
    sorted.reserve(variables_.size());
    for (const auto& entry : variables_) {
      sorted.emplace_back(entry.first);
    }
    std::sort(sorted.begin(), sorted.end());
    return sorted;
  });
  CHECK_EQ(gscip_result.solutions.size(), gscip_result.objective_values.size());
  for (int i = 0; i < gscip_result.solutions.size(); ++i) {
    // GScip ensures the solutions are returned best objective first.
    if (!meets_cutoff(gscip_result.objective_values[i])) {
      break;
    }
    SolutionProto* const solution = solve_result.add_solutions();
    PrimalSolutionProto* const primal_solution =
        solution->mutable_primal_solution();
    primal_solution->set_objective_value(gscip_result.objective_values[i]);
    primal_solution->set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
    *primal_solution->mutable_variable_values() = FillSparseDoubleVector(
        sorted_variables.GetOrCreate(), variables_, gscip_result.solutions[i],
        model_parameters.variable_values_filter());
  }
  if (!gscip_result.primal_ray.empty()) {
    *solve_result.add_primal_rays()->mutable_variable_values() =
        FillSparseDoubleVector(sorted_variables.GetOrCreate(), variables_,
                               gscip_result.primal_ray,
                               model_parameters.variable_values_filter());
  }
  const bool has_feasible_solution = solve_result.solutions_size() > 0;
  ASSIGN_OR_RETURN(
      *solve_result.mutable_termination(),
      ConvertTerminationReason(gscip_result.gscip_output.status(),
                               gscip_result.gscip_output.status_detail(),
                               /*has_feasible_solution=*/has_feasible_solution,
                               /*had_cutoff=*/cutoff.has_value()));
  *solve_result.mutable_solve_stats()->mutable_problem_status() =
      GetProblemStatusProto(
          gscip_result.gscip_output.status(),
          /*has_feasible_solution=*/has_feasible_solution,
          /*has_finite_dual_bound=*/
          std::isfinite(gscip_result.gscip_output.stats().best_bound()),
          /*was_cutoff=*/solve_result.termination().limit() == LIMIT_CUTOFF);
  SolveStatsProto* const common_stats = solve_result.mutable_solve_stats();
  const GScipSolvingStats& gscip_stats = gscip_result.gscip_output.stats();
  common_stats->set_best_dual_bound(gscip_stats.best_bound());
  // If we found no solutions meeting the cutoff, we have no primal bound.
  if (has_feasible_solution) {
    common_stats->set_best_primal_bound(gscip_stats.best_objective());
  } else {
    common_stats->set_best_primal_bound(is_maximize ? -kInf : kInf);
  }

  common_stats->set_node_count(gscip_stats.node_count());
  common_stats->set_simplex_iterations(gscip_stats.primal_simplex_iterations() +
                                       gscip_stats.dual_simplex_iterations());
  common_stats->set_barrier_iterations(gscip_stats.total_lp_iterations() -
                                       common_stats->simplex_iterations());
  *solve_result.mutable_gscip_output() = std::move(gscip_result.gscip_output);
  return solve_result;
}

GScipSolver::GScipSolver(std::unique_ptr<GScip> gscip)
    : gscip_(std::move(ABSL_DIE_IF_NULL(gscip))) {
  interrupt_event_handler_.Register(gscip_.get());
}

absl::StatusOr<std::unique_ptr<SolverInterface>> GScipSolver::New(
    const ModelProto& model, const InitArgs& init_args) {
  ASSIGN_OR_RETURN(std::unique_ptr<GScip> gscip, GScip::Create(model.name()));
  RETURN_IF_ERROR(gscip->SetMaximize(model.objective().maximize()));
  RETURN_IF_ERROR(gscip->SetObjectiveOffset(model.objective().offset()));
  // TODO(b/204083726): Remove this check if QP support is added
  if (!model.objective().quadratic_coefficients().row_ids().empty()) {
    return absl::InvalidArgumentError(
        "MathOpt does not currently support SCIP models with quadratic "
        "objectives");
  }
  // Can't be const because it had to be moved into the StatusOr and be
  // convereted to std::unique_ptr<SolverInterface>.
  auto solver = absl::WrapUnique(new GScipSolver(std::move(gscip)));

  RETURN_IF_ERROR(solver->AddVariables(
      model.variables(),
      SparseDoubleVectorAsMap(model.objective().linear_coefficients())));
  RETURN_IF_ERROR(solver->AddLinearConstraints(
      model.linear_constraints(), model.linear_constraint_matrix()));

  return solver;
}

absl::StatusOr<SolveResultProto> GScipSolver::Solve(
    const SolveParametersProto& parameters,
    const ModelSolveParametersProto& model_parameters,
    const MessageCallback message_cb,
    const CallbackRegistrationProto& callback_registration, const Callback cb,
    SolveInterrupter* const interrupter) {
  const absl::Time start = absl::Now();

  RETURN_IF_ERROR(CheckRegisteredCallbackEvents(callback_registration,
                                                /*supported_events=*/{}));

  const std::unique_ptr<GScipSolverCallbackHandler> callback_handler =
      GScipSolverCallbackHandler::RegisterIfNeeded(callback_registration, cb,
                                                   start, gscip_->scip());

  std::unique_ptr<GScipSolverMessageCallbackHandler> message_cb_handler;
  if (message_cb != nullptr) {
    message_cb_handler =
        std::make_unique<GScipSolverMessageCallbackHandler>(message_cb);
  }

  ASSIGN_OR_RETURN(auto gscip_parameters, MergeParameters(parameters));

  for (const SolutionHintProto& hint : model_parameters.solution_hints()) {
    absl::flat_hash_map<SCIP_VAR*, double> partial_solution;
    for (const auto [id, val] : MakeView(hint.variable_values())) {
      partial_solution.insert({variables_.at(id), val});
    }
    RETURN_IF_ERROR(gscip_->SuggestHint(partial_solution).status());
  }
  for (const auto [id, value] :
       MakeView(model_parameters.branching_priorities())) {
    RETURN_IF_ERROR(gscip_->SetBranchingPriority(variables_.at(id), value));
  }

  // Before calling solve, set the interrupter on the event handler that calls
  // SCIPinterruptSolve().
  if (interrupter != nullptr) {
    interrupt_event_handler_.interrupter = interrupter;
  }
  const auto interrupter_cleanup = absl::MakeCleanup(
      [&]() { interrupt_event_handler_.interrupter = nullptr; });

  // SCIP returns "infeasible" when the model contain invalid bounds.
  RETURN_IF_ERROR(ListInvertedBounds().ToStatus());

  ASSIGN_OR_RETURN(GScipResult gscip_result,
                   gscip_->Solve(gscip_parameters,
                                 /*legacy_params=*/"",
                                 message_cb_handler != nullptr
                                     ? message_cb_handler->MessageHandler()
                                     : nullptr));

  // Flushes the last unfinished message as early as possible.
  message_cb_handler.reset();

  if (callback_handler) {
    RETURN_IF_ERROR(callback_handler->Flush());
  }

  ASSIGN_OR_RETURN(
      SolveResultProto result,
      CreateSolveResultProto(std::move(gscip_result), model_parameters,
                             parameters.has_cutoff_limit()
                                 ? std::make_optional(parameters.cutoff_limit())
                                 : std::nullopt));
  CHECK_OK(util_time::EncodeGoogleApiProto(
      absl::Now() - start, result.mutable_solve_stats()->mutable_solve_time()));
  return result;
}

absl::flat_hash_set<SCIP_VAR*> GScipSolver::LookupAllVariables(
    absl::Span<const int64_t> variable_ids) {
  absl::flat_hash_set<SCIP_VAR*> result;
  result.reserve(variable_ids.size());
  for (const int64_t var_id : variable_ids) {
    result.insert(variables_.at(var_id));
  }
  return result;
}

// Returns the ids of variables and linear constraints with inverted bounds.
InvertedBounds GScipSolver::ListInvertedBounds() const {
  // Get the SCIP variables/constraints with inverted bounds.
  InvertedBounds inverted_bounds;
  for (const auto& [id, var] : variables_) {
    if (gscip_->Lb(var) > gscip_->Ub(var)) {
      inverted_bounds.variables.push_back(id);
    }
  }
  for (const auto& [id, cstr] : linear_constraints_) {
    if (gscip_->LinearConstraintLb(cstr) > gscip_->LinearConstraintUb(cstr)) {
      inverted_bounds.linear_constraints.push_back(id);
    }
  }

  // Above code have inserted ids in non-stable order.
  std::sort(inverted_bounds.variables.begin(), inverted_bounds.variables.end());
  std::sort(inverted_bounds.linear_constraints.begin(),
            inverted_bounds.linear_constraints.end());
  return inverted_bounds;
}

bool GScipSolver::CanUpdate(const ModelUpdateProto& model_update) {
  return gscip_
             ->CanSafeBulkDelete(
                 LookupAllVariables(model_update.deleted_variable_ids()))
             .ok() &&
         model_update.objective_updates()
             .quadratic_coefficients()
             .row_ids()
             .empty();
}

absl::Status GScipSolver::Update(const ModelUpdateProto& model_update) {
  for (const int64_t constraint_id :
       model_update.deleted_linear_constraint_ids()) {
    SCIP_CONS* const scip_cons = linear_constraints_.at(constraint_id);
    linear_constraints_.erase(constraint_id);
    RETURN_IF_ERROR(gscip_->DeleteConstraint(scip_cons));
  }
  {
    const absl::flat_hash_set<SCIP_VAR*> vars_to_delete =
        LookupAllVariables(model_update.deleted_variable_ids());
    for (const int64_t deleted_variable_id :
         model_update.deleted_variable_ids()) {
      variables_.erase(deleted_variable_id);
    }
    RETURN_IF_ERROR(gscip_->SafeBulkDelete(vars_to_delete));
  }

  const std::optional<int64_t> first_new_var_id =
      FirstVariableId(model_update.new_variables());
  const std::optional<int64_t> first_new_cstr_id =
      FirstLinearConstraintId(model_update.new_linear_constraints());

  if (model_update.objective_updates().has_direction_update()) {
    RETURN_IF_ERROR(gscip_->SetMaximize(
        model_update.objective_updates().direction_update()));
  }
  if (model_update.objective_updates().has_offset_update()) {
    RETURN_IF_ERROR(gscip_->SetObjectiveOffset(
        model_update.objective_updates().offset_update()));
  }
  RETURN_IF_ERROR(UpdateVariables(model_update.variable_updates()));
  const absl::flat_hash_map<int64_t, double> linear_objective_updates =
      SparseDoubleVectorAsMap(
          model_update.objective_updates().linear_coefficients());
  for (const auto& obj_pair : linear_objective_updates) {
    // New variables' coefficient is set when the variables are added below.
    if (!first_new_var_id.has_value() || obj_pair.first < *first_new_var_id) {
      RETURN_IF_ERROR(
          gscip_->SetObjCoef(variables_.at(obj_pair.first), obj_pair.second));
    }
  }

  // Here the model_update.linear_constraint_matrix_updates is split into three
  // sub-matrix:
  //
  //                existing    new
  //                columns   columns
  //              /         |         \
  //    existing  |    1    |    2    |
  //    rows      |         |         |
  //              |---------+---------|
  //    new       |                   |
  //    rows      |         3         |
  //              \                   /
  //
  // The coefficients of sub-matrix 1 are set by UpdateLinearConstraints(), the
  // ones of sub-matrix 2 by AddVariables() and the ones of the sub-matrix 3 by
  // AddLinearConstraints(). The rationale here is that SCIPchgCoefLinear() has
  // a complexity of O(non_zeros). Thus it is inefficient and can lead to O(n^2)
  // behaviors if it was used for new rows or for new columns. For new rows it
  // is more efficient to pass all the variables coefficients at once when
  // building the constraints. For new columns and existing rows, since we can
  // assume there is no existing coefficient, we can use SCIPaddCoefLinear()
  // which is O(1). This leads to only use SCIPchgCoefLinear() for changing the
  // coefficients of existing rows and columns.
  //
  // TODO(b/215722113): maybe we could use SCIPaddCoefLinear() for sub-matrix 1.

  // Add new variables.
  RETURN_IF_ERROR(
      AddVariables(model_update.new_variables(), linear_objective_updates));

  // Update linear constraints properties and sub-matrix 1.
  RETURN_IF_ERROR(
      UpdateLinearConstraints(model_update.linear_constraint_updates(),
                              model_update.linear_constraint_matrix_updates(),
                              /*first_new_var_id=*/first_new_var_id,
                              /*first_new_cstr_id=*/first_new_cstr_id));

  // Update the sub-matrix 2.
  const std::optional first_new_variable_id =
      FirstVariableId(model_update.new_variables());
  if (first_new_variable_id.has_value()) {
    for (const auto& [lin_con_id, var_coeffs] :
         SparseSubmatrixByRows(model_update.linear_constraint_matrix_updates(),
                               /*start_row_id=*/0,
                               /*end_row_id=*/first_new_cstr_id,
                               /*start_col_id=*/*first_new_variable_id,
                               /*end_col_id=*/std::nullopt)) {
      for (const auto& [var_id, value] : var_coeffs) {
        // See above why we use AddLinearConstraintCoef().
        RETURN_IF_ERROR(gscip_->AddLinearConstraintCoef(
            linear_constraints_.at(lin_con_id), variables_.at(var_id), value));
      }
    }
  }

  // Add the new constraints and sets sub-matrix 3.
  RETURN_IF_ERROR(
      AddLinearConstraints(model_update.new_linear_constraints(),
                           model_update.linear_constraint_matrix_updates()));
  return absl::OkStatus();
}

GScipSolver::InterruptEventHandler::InterruptEventHandler()
    : GScipEventHandler(
          {.name = "interrupt event handler",
           .description = "Event handler to call SCIPinterruptSolve() when a "
                          "user SolveInterrupter is triggered."}) {}

SCIP_RETCODE GScipSolver::InterruptEventHandler::Init(GScip* const gscip) {
  // We don't register any event if we don't have an interrupter.
  if (interrupter == nullptr) {
    return SCIP_OKAY;
  }

  // TODO(b/193537362): see if these events are enough or if we should have more
  // of these.
  CatchEvent(SCIP_EVENTTYPE_PRESOLVEROUND);
  CatchEvent(SCIP_EVENTTYPE_NODEEVENT);

  return TryCallInterruptIfNeeded(gscip);
}

SCIP_RETCODE GScipSolver::InterruptEventHandler::Execute(
    const GScipEventHandlerContext context) {
  return TryCallInterruptIfNeeded(context.gscip());
}

SCIP_RETCODE GScipSolver::InterruptEventHandler::TryCallInterruptIfNeeded(
    GScip* const gscip) {
  if (interrupter == nullptr) {
    LOG(WARNING) << "TryCallInterruptIfNeeded() called after interrupter has "
                    "been reset!";
    return SCIP_OKAY;
  }

  if (!interrupter->IsInterrupted()) {
    return SCIP_OKAY;
  }

  const SCIP_STAGE stage = SCIPgetStage(gscip->scip());
  switch (stage) {
    case SCIP_STAGE_INIT:
    case SCIP_STAGE_FREE:
      // This should never happen anyway; but if this happens, we may want to
      // know about it in unit tests.
      LOG(DFATAL) << "TryCallInterruptIfNeeded() called in stage "
                  << (stage == SCIP_STAGE_INIT ? "INIT" : "FREE");
      return SCIP_OKAY;
    case SCIP_STAGE_INITSOLVE:
      LOG(WARNING) << "TryCallInterruptIfNeeded() called in INITSOLVE stage; "
                      "we can't call SCIPinterruptSolve() in this stage.";
      return SCIP_OKAY;
    default:
      return SCIPinterruptSolve(gscip->scip());
  }
}

MATH_OPT_REGISTER_SOLVER(SOLVER_TYPE_GSCIP, GScipSolver::New)

}  // namespace math_opt
}  // namespace operations_research
