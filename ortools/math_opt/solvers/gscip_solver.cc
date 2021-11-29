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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "scip/type_cons.h"
#include "scip/type_var.h"
#include "ortools/base/map_util.h"
#include "ortools/gscip/gscip.h"
#include "ortools/gscip/gscip.pb.h"
#include "ortools/gscip/gscip_parameters.h"
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
#include "ortools/math_opt/solvers/gscip_solver_callback.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/port/proto_utils.h"
#include "absl/status/status.h"
#include "ortools/base/status_macros.h"
#include "ortools/base/protoutil.h"

namespace operations_research {
namespace math_opt {

namespace {

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
inline int FindRowStart(const SparseDoubleMatrixProto& matrix, const int row_id,
                        const int scan_start) {
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

    const auto vars_begin = linear_constraint_matrix_->column_ids().begin();
    result.variable_ids = absl::MakeConstSpan(vars_begin + matrix_start_,
                                              vars_begin + matrix_end_);
    const auto coefficients_begins =
        linear_constraint_matrix_->coefficients().begin();
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
  absl::optional<T> value_;
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
    CHECK_GE(id, next_unused_variable_id_);
    ASSIGN_OR_RETURN(
        SCIP_VAR* const v,
        gscip_->AddVariable(
            variables.lower_bounds(i), variables.upper_bounds(i),
            gtl::FindWithDefault(linear_objective_coefficients, id),
            GScipVarTypeFromIsInteger(variables.integers(i)),
            SafeName(variables, i)));
    gtl::InsertOrDie(&variables_, id, v);
    next_unused_variable_id_ = id + 1;
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
    CHECK_GE(current.linear_constraint_id, next_unused_linear_constraint_id_);

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
    next_unused_linear_constraint_id_ = current.linear_constraint_id + 1;
  }
  return absl::OkStatus();
}

absl::Status GScipSolver::UpdateLinearConstraints(
    const LinearConstraintUpdatesProto linear_constraint_updates,
    const SparseDoubleMatrixProto& linear_constraint_matrix) {
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
  for (int i = 0; i < linear_constraint_matrix.row_ids_size(); ++i) {
    const int64_t lin_con_id = linear_constraint_matrix.row_ids(i);
    if (lin_con_id >= next_unused_linear_constraint_id_) {
      break;
    }
    const int64_t var_id = linear_constraint_matrix.column_ids(i);
    const double value = linear_constraint_matrix.coefficients(i);
    RETURN_IF_ERROR(gscip_->SetLinearConstraintCoef(
        linear_constraints_.at(lin_con_id), variables_.at(var_id), value));
  }
  return absl::OkStatus();
}

GScipParameters::MetaParamValue ConvertMathOptEmphasis(Emphasis emphasis) {
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

GScipParameters GScipSolver::MergeCommonParameters(
    const CommonSolveParametersProto& common_solver_parameters,
    const GScipParameters& gscip_parameters) {
  // First build the result by translating common parameters to a
  // GScipParameters, and then merging with user provided gscip_parameters.
  // This results in user provided solver specific parameters overwriting
  // common parameters should there be any conflict.
  GScipParameters result;
  if (common_solver_parameters.has_time_limit()) {
    GScipSetTimeLimit(
        util_time::DecodeGoogleApiProto(common_solver_parameters.time_limit())
            .value(),
        &result);
  }
  if (common_solver_parameters.has_threads()) {
    GScipSetMaxNumThreads(common_solver_parameters.threads(), &result);
  }
  if (common_solver_parameters.has_enable_output()) {
    // GScip has also GScipSetOutputEnabled() but this changes the log
    // level. Setting `silence_output` sets the `quiet` field on the default
    // message handler of SCIP which removes the output. Here it is important to
    // use this rather than changing the log level so that if the user registers
    // for CALLBACK_EVENT_MESSAGE they do get some messages even when
    // `enable_output` is false.
    result.set_silence_output(!common_solver_parameters.enable_output());
  }
  if (common_solver_parameters.has_random_seed()) {
    GScipSetRandomSeed(&result, common_solver_parameters.random_seed());
  }
  if (common_solver_parameters.lp_algorithm() != LP_ALGORITHM_UNSPECIFIED) {
    char alg;
    switch (common_solver_parameters.lp_algorithm()) {
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
                   << ProtoEnumToString(common_solver_parameters.lp_algorithm())
                   << " unknown, error setting gSCIP parameters";
    }
    (*result.mutable_char_params())["lp/initalgorithm"] = alg;
  }
  if (common_solver_parameters.cuts() != EMPHASIS_UNSPECIFIED) {
    result.set_separating(
        ConvertMathOptEmphasis(common_solver_parameters.cuts()));
  }
  if (common_solver_parameters.heuristics() != EMPHASIS_UNSPECIFIED) {
    result.set_heuristics(
        ConvertMathOptEmphasis(common_solver_parameters.heuristics()));
  }
  if (common_solver_parameters.presolve() != EMPHASIS_UNSPECIFIED) {
    result.set_presolve(
        ConvertMathOptEmphasis(common_solver_parameters.presolve()));
  }
  if (common_solver_parameters.scaling() != EMPHASIS_UNSPECIFIED) {
    int scaling_value;
    switch (common_solver_parameters.scaling()) {
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
                   << ProtoEnumToString(common_solver_parameters.scaling())
                   << " unknown, error setting gSCIP parameters";
    }
    (*result.mutable_int_params())["lp/scaling"] = scaling_value;
  }
  result.MergeFrom(gscip_parameters);
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

}  // namespace

absl::StatusOr<std::pair<SolveResultProto::TerminationReason, std::string>>
GScipSolver::ConvertTerminationReason(const GScipOutput::Status gscip_status,
                                      const std::string& gscip_status_detail,
                                      const bool has_feasible_solution) {
  switch (gscip_status) {
    case GScipOutput::UNKNOWN:
      return std::make_pair(SolveResultProto::TERMINATION_REASON_UNSPECIFIED,
                            gscip_status_detail);
    case GScipOutput::USER_INTERRUPT:
      return std::make_pair(SolveResultProto::INTERRUPTED, gscip_status_detail);
    case GScipOutput::NODE_LIMIT:
      return std::make_pair(
          SolveResultProto::NODE_LIMIT,
          JoinDetails(gscip_status_detail,
                      "Underlying gSCIP status: NODE_LIMIT."));
    case GScipOutput::TOTAL_NODE_LIMIT:
      return std::make_pair(
          SolveResultProto::NODE_LIMIT,
          JoinDetails(gscip_status_detail,
                      "Underlying gSCIP status: TOTAL_NODE_LIMIT."));
    case GScipOutput::STALL_NODE_LIMIT:
      return std::make_pair(SolveResultProto::SLOW_PROGRESS,
                            gscip_status_detail);
    case GScipOutput::TIME_LIMIT:
      return std::make_pair(SolveResultProto::TIME_LIMIT, gscip_status_detail);
    case GScipOutput::MEM_LIMIT:
      return std::make_pair(SolveResultProto::MEMORY_LIMIT,
                            gscip_status_detail);

    case GScipOutput::SOL_LIMIT:
      return std::make_pair(SolveResultProto::SOLUTION_LIMIT,
                            JoinDetails(gscip_status_detail,
                                        "Underlying gSCIP status: SOL_LIMIT."));
    case GScipOutput::BEST_SOL_LIMIT:
      return std::make_pair(
          SolveResultProto::SOLUTION_LIMIT,
          JoinDetails(gscip_status_detail,
                      "Underlying gSCIP status: BEST_SOL_LIMIT."));

    case GScipOutput::RESTART_LIMIT:
      return std::make_pair(
          SolveResultProto::OTHER_LIMIT,
          JoinDetails(gscip_status_detail,
                      "Underlying gSCIP status: RESTART_LIMIT."));
    case GScipOutput::OPTIMAL:
      return std::make_pair(SolveResultProto::OPTIMAL,
                            JoinDetails(gscip_status_detail,
                                        "Underlying gSCIP status: OPTIMAL."));
    case GScipOutput::GAP_LIMIT:
      return std::make_pair(SolveResultProto::OPTIMAL,
                            JoinDetails(gscip_status_detail,
                                        "Underlying gSCIP status: GAP_LIMIT."));
    case GScipOutput::INFEASIBLE:
      return std::make_pair(SolveResultProto::INFEASIBLE, gscip_status_detail);
    case GScipOutput::UNBOUNDED: {
      if (has_feasible_solution) {
        return std::make_pair(
            SolveResultProto::UNBOUNDED,
            JoinDetails(gscip_status_detail,
                        "Underlying gSCIP status was UNBOUNDED, both primal "
                        "ray and feasible solution are present."));
      } else {
        return std::make_pair(
            SolveResultProto::DUAL_INFEASIBLE,
            JoinDetails(
                gscip_status_detail,
                "Underlying gSCIP status was UNBOUNDED, but only primal ray "
                "was given, no feasible solution was found."));
      }
    }

    case GScipOutput::INF_OR_UNBD:
      return std::make_pair(
          SolveResultProto::DUAL_INFEASIBLE,
          JoinDetails(gscip_status_detail,
                      "Underlying gSCIP status: INF_OR_UNBD."));
    case GScipOutput::TERMINATE:
      return std::make_pair(
          SolveResultProto::OTHER_ERROR,
          JoinDetails(gscip_status_detail,
                      "Underlying gSCIP status: OTHER_ERROR."));
    case GScipOutput::INVALID_SOLVER_PARAMETERS:
      return absl::InvalidArgumentError(gscip_status_detail);
    default:
      return absl::InternalError(JoinDetails(
          gscip_status_detail, absl::StrCat("Missing GScipOutput.status case: ",
                                            ProtoEnumToString(gscip_status))));
  }
}

absl::StatusOr<SolveResultProto> GScipSolver::CreateSolveResultProto(
    GScipResult gscip_result,
    const ModelSolveParametersProto& model_parameters) {
  SolveResultProto solve_result;
  ASSIGN_OR_RETURN(
      const auto reason_and_detail,
      ConvertTerminationReason(gscip_result.gscip_output.status(),
                               gscip_result.gscip_output.status_detail(),
                               !gscip_result.solutions.empty()));
  solve_result.set_termination_reason(reason_and_detail.first);
  solve_result.set_termination_detail(reason_and_detail.second);

  const int num_solutions = gscip_result.solutions.size();
  CHECK_EQ(num_solutions, gscip_result.objective_values.size());

  LazyInitialized<std::vector<int64_t>> sorted_variables([&]() {
    std::vector<int64_t> sorted;
    sorted.reserve(variables_.size());
    for (const auto& entry : variables_) {
      sorted.emplace_back(entry.first);
    }
    std::sort(sorted.begin(), sorted.end());
    return sorted;
  });
  for (int i = 0; i < gscip_result.solutions.size(); ++i) {
    PrimalSolutionProto* const primal_solution =
        solve_result.add_primal_solutions();
    primal_solution->set_objective_value(gscip_result.objective_values[i]);
    *primal_solution->mutable_variable_values() = FillSparseDoubleVector(
        sorted_variables.GetOrCreate(), variables_, gscip_result.solutions[i],
        model_parameters.primal_variables_filter());
  }
  if (!gscip_result.primal_ray.empty()) {
    *solve_result.add_primal_rays()->mutable_variable_values() =
        FillSparseDoubleVector(sorted_variables.GetOrCreate(), variables_,
                               gscip_result.primal_ray,
                               model_parameters.primal_variables_filter());
  }
  // TODO(user): add support for the basis and dual solutions in gscip, then
  //  populate them here.
  SolveStatsProto* const common_stats = solve_result.mutable_solve_stats();
  const GScipSolvingStats& gscip_stats = gscip_result.gscip_output.stats();
  common_stats->set_best_dual_bound(gscip_stats.best_bound());
  common_stats->set_best_primal_bound(gscip_stats.best_objective());
  common_stats->set_node_count(gscip_stats.node_count());
  common_stats->set_simplex_iterations(gscip_stats.primal_simplex_iterations() +
                                       gscip_stats.dual_simplex_iterations());
  common_stats->set_barrier_iterations(gscip_stats.total_lp_iterations() -
                                       common_stats->simplex_iterations());
  *solve_result.mutable_gscip_output() = std::move(gscip_result.gscip_output);
  return solve_result;
}

absl::StatusOr<std::unique_ptr<SolverInterface>> GScipSolver::New(
    const ModelProto& model, const SolverInitializerProto& initializer) {
  auto solver = absl::WrapUnique(new GScipSolver);
  ASSIGN_OR_RETURN(solver->gscip_, GScip::Create(model.name()));
  RETURN_IF_ERROR(solver->gscip_->SetMaximize(model.objective().maximize()));
  RETURN_IF_ERROR(
      solver->gscip_->SetObjectiveOffset(model.objective().offset()));
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
    const CallbackRegistrationProto& callback_registration, const Callback cb) {
  const absl::Time start = absl::Now();

  const std::unique_ptr<GScipSolverCallbackHandler> callback_handler =
      GScipSolverCallbackHandler::RegisterIfNeeded(callback_registration, cb,
                                                   start, gscip_->scip());

  const GScipParameters gscip_parameters = MergeCommonParameters(
      parameters.common_parameters(), parameters.gscip_parameters());
  // TODO(user): reorganize gscip to respect warning is error argument on bad
  //  parameters.

  ASSIGN_OR_RETURN(
      GScipResult gscip_result,
      gscip_->Solve(
          gscip_parameters,
          /*legacy_params=*/"",
          callback_handler ? callback_handler->MessageHandler() : nullptr));
  if (callback_handler) {
    RETURN_IF_ERROR(callback_handler->Flush());
  }
  ASSIGN_OR_RETURN(
      SolveResultProto result,
      CreateSolveResultProto(std::move(gscip_result), model_parameters));
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

bool GScipSolver::CanUpdate(const ModelUpdateProto& model_update) {
  return gscip_
      ->CanSafeBulkDelete(
          LookupAllVariables(model_update.deleted_variable_ids()))
      .ok();
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
    if (obj_pair.first < next_unused_variable_id_) {
      RETURN_IF_ERROR(
          gscip_->SetObjCoef(variables_.at(obj_pair.first), obj_pair.second));
    }
  }
  RETURN_IF_ERROR(
      AddVariables(model_update.new_variables(), linear_objective_updates));
  RETURN_IF_ERROR(
      UpdateLinearConstraints(model_update.linear_constraint_updates(),
                              model_update.linear_constraint_matrix_updates()));
  RETURN_IF_ERROR(
      AddLinearConstraints(model_update.new_linear_constraints(),
                           model_update.linear_constraint_matrix_updates()));
  return absl::OkStatus();
}

MATH_OPT_REGISTER_SOLVER(SOLVER_TYPE_GSCIP, GScipSolver::New)

}  // namespace math_opt
}  // namespace operations_research
