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

#include "ortools/math_opt/solvers/gurobi_solver.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "google/protobuf/repeated_ptr_field.h"
#include "ortools/base/linked_hash_map.h"
#include "ortools/base/map_util.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/invalid_indicators.h"
#include "ortools/math_opt/core/inverted_bounds.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/non_streamable_solver_init_arguments.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/core/sorted.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/solvers/gurobi.pb.h"
#include "ortools/math_opt/solvers/gurobi/g_gurobi.h"
#include "ortools/math_opt/solvers/gurobi_callback.h"
#include "ortools/math_opt/solvers/gurobi_init_arguments.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/callback_validator.h"
#include "ortools/port/proto_utils.h"
#include "ortools/util/solve_interrupter.h"
#include "ortools/util/testing_utils.h"

namespace operations_research {
namespace math_opt {
namespace {

constexpr SupportedProblemStructures kGurobiSupportedStructures = {
    .integer_variables = SupportType::kSupported,
    .multi_objectives = SupportType::kSupported,
    .quadratic_objectives = SupportType::kSupported,
    .quadratic_constraints = SupportType::kSupported,
    .second_order_cone_constraints = SupportType::kSupported,
    .sos1_constraints = SupportType::kSupported,
    .sos2_constraints = SupportType::kSupported,
    .indicator_constraints = SupportType::kSupported};

absl::StatusOr<std::unique_ptr<Gurobi>> GurobiFromInitArgs(
    const SolverInterface::InitArgs& init_args) {
  if (kAnyXsanEnabled) {
    return absl::FailedPreconditionError(
        "the Gurobi library is not compatible with any sanitizer (MSAN, ASAN "
        "or TSAN)");
  }

  // We don't test or return an error for incorrect non streamable arguments
  // type since it is already tested by the Solver class.
  const NonStreamableGurobiInitArguments* const non_streamable_args =
      init_args.non_streamable != nullptr
          ? init_args.non_streamable->ToNonStreamableGurobiInitArguments()
          : nullptr;
  std::unique_ptr<Gurobi> gurobi;
  if (non_streamable_args != nullptr &&
      non_streamable_args->primary_env != nullptr) {
    return Gurobi::NewWithSharedPrimaryEnv(non_streamable_args->primary_env);
  } else if (init_args.streamable.has_gurobi() &&
             init_args.streamable.gurobi().has_isv_key()) {
    ASSIGN_OR_RETURN(
        GRBenvUniquePtr env,
        NewPrimaryEnvironment(init_args.streamable.gurobi().isv_key()));
    return Gurobi::New(std::move(env));
  } else {
    return Gurobi::New();
  }
}

inline BasisStatusProto ConvertVariableStatus(const int status) {
  switch (status) {
    case GRB_BASIC:
      return BASIS_STATUS_BASIC;
    case GRB_NONBASIC_LOWER:
      return BASIS_STATUS_AT_LOWER_BOUND;
    case GRB_NONBASIC_UPPER:
      return BASIS_STATUS_AT_UPPER_BOUND;
    case GRB_SUPERBASIC:
      return BASIS_STATUS_FREE;
    default:
      return BASIS_STATUS_UNSPECIFIED;
  }
}

inline int GrbVariableStatus(const BasisStatusProto status) {
  switch (status) {
    case BASIS_STATUS_BASIC:
      return GRB_BASIC;
    case BASIS_STATUS_AT_LOWER_BOUND:
    case BASIS_STATUS_FIXED_VALUE:
      return GRB_NONBASIC_LOWER;
    case BASIS_STATUS_AT_UPPER_BOUND:
      return GRB_NONBASIC_UPPER;
    case BASIS_STATUS_FREE:
      return GRB_SUPERBASIC;
    case BASIS_STATUS_UNSPECIFIED:
    default:
      LOG(FATAL) << "Unexpected invalid initial_basis.";
      return 0;
  }
}

// is_mip indicates if the problem has integer variables or constraints that
// would cause Gurobi to treat the problem as a MIP, e.g. SOS, indicator.
absl::StatusOr<GurobiParametersProto> MergeParameters(
    const SolveParametersProto& solve_parameters,
    const ModelSolveParametersProto& model_parameters, const bool is_mip,
    const bool is_multi_objective_mode) {
  GurobiParametersProto merged_parameters;

  {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_LOGTOCONSOLE);
    parameter->set_value(solve_parameters.enable_output() ? "1" : "0");
  }

  {
    absl::Duration time_limit = absl::InfiniteDuration();
    if (solve_parameters.has_time_limit()) {
      ASSIGN_OR_RETURN(time_limit, util_time::DecodeGoogleApiProto(
                                       solve_parameters.time_limit()));
    }
    // If we do not have a multi-objective model, but the user has specified a
    // time limit on the primary objective, this is functionally a second way to
    // specify the global time limit. So, we should take the min of the two.
    if (!is_multi_objective_mode &&
        model_parameters.primary_objective_parameters().has_time_limit()) {
      ASSIGN_OR_RETURN(
          const absl::Duration primary_objective_time_limit,
          util_time::DecodeGoogleApiProto(
              model_parameters.primary_objective_parameters().time_limit()));
      time_limit = std::min(time_limit, primary_objective_time_limit);
    }
    if (time_limit < absl::InfiniteDuration()) {
      GurobiParametersProto::Parameter* const parameter =
          merged_parameters.add_parameters();
      parameter->set_name(GRB_DBL_PAR_TIMELIMIT);
      parameter->set_value(absl::StrCat(absl::ToDoubleSeconds(time_limit)));
    }
  }

  if (solve_parameters.has_node_limit()) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_DBL_PAR_NODELIMIT);
    parameter->set_value(absl::StrCat(solve_parameters.node_limit()));
  }

  if (solve_parameters.has_threads()) {
    const int threads = solve_parameters.threads();
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_THREADS);
    parameter->set_value(absl::StrCat(threads));
  }

  if (solve_parameters.has_absolute_gap_tolerance()) {
    const double absolute_gap_tolerance =
        solve_parameters.absolute_gap_tolerance();
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_DBL_PAR_MIPGAPABS);
    parameter->set_value(absl::StrCat(absolute_gap_tolerance));
  }

  if (solve_parameters.has_relative_gap_tolerance()) {
    const double relative_gap_tolerance =
        solve_parameters.relative_gap_tolerance();
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_DBL_PAR_MIPGAP);
    parameter->set_value(absl::StrCat(relative_gap_tolerance));
  }

  if (solve_parameters.has_cutoff_limit()) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_DBL_PAR_CUTOFF);
    parameter->set_value(absl::StrCat(solve_parameters.cutoff_limit()));
  }

  if (solve_parameters.has_objective_limit()) {
    if (!is_mip) {
      return absl::InvalidArgumentError(
          "objective_limit is only supported for Gurobi on MIP models");
    }
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_DBL_PAR_BESTOBJSTOP);
    parameter->set_value(absl::StrCat(solve_parameters.objective_limit()));
  }

  if (solve_parameters.has_best_bound_limit()) {
    if (!is_mip) {
      return absl::InvalidArgumentError(
          "best_bound_limit is only supported for Gurobi on MIP models");
    }
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_DBL_PAR_BESTBDSTOP);
    parameter->set_value(absl::StrCat(solve_parameters.best_bound_limit()));
  }

  if (solve_parameters.has_solution_limit()) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_SOLUTIONLIMIT);
    parameter->set_value(absl::StrCat(solve_parameters.solution_limit()));
  }

  if (solve_parameters.has_random_seed()) {
    const int random_seed =
        std::min(GRB_MAXINT, std::max(solve_parameters.random_seed(), 0));
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_SEED);
    parameter->set_value(absl::StrCat(random_seed));
  }

  if (solve_parameters.has_solution_pool_size()) {
    GurobiParametersProto::Parameter* const solution_pool_size =
        merged_parameters.add_parameters();
    solution_pool_size->set_name(GRB_INT_PAR_POOLSOLUTIONS);
    solution_pool_size->set_value(
        absl::StrCat(solve_parameters.solution_pool_size()));
  }

  if (solve_parameters.lp_algorithm() != LP_ALGORITHM_UNSPECIFIED) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_METHOD);
    switch (solve_parameters.lp_algorithm()) {
      case LP_ALGORITHM_PRIMAL_SIMPLEX:
        parameter->set_value(absl::StrCat(GRB_METHOD_PRIMAL));
        break;
      case LP_ALGORITHM_DUAL_SIMPLEX:
        parameter->set_value(absl::StrCat(GRB_METHOD_DUAL));
        break;
      case LP_ALGORITHM_BARRIER:
        parameter->set_value(absl::StrCat(GRB_METHOD_BARRIER));
        break;
      case LP_ALGORITHM_FIRST_ORDER:
        return absl::InvalidArgumentError(
            "lp_algorithm FIRST_ORDER is not supported for gurobi");
      default:
        LOG(FATAL) << "LPAlgorithm: "
                   << ProtoEnumToString(solve_parameters.lp_algorithm())
                   << " unknown, error setting Gurobi parameters";
    }
  }

  if (solve_parameters.scaling() != EMPHASIS_UNSPECIFIED) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_SCALEFLAG);
    switch (solve_parameters.scaling()) {
      case EMPHASIS_OFF:
        parameter->set_value(absl::StrCat(0));
        break;
      case EMPHASIS_LOW:
      case EMPHASIS_MEDIUM:
        parameter->set_value(absl::StrCat(1));
        break;
      case EMPHASIS_HIGH:
        parameter->set_value(absl::StrCat(2));
        break;
      case EMPHASIS_VERY_HIGH:
        parameter->set_value(absl::StrCat(3));
        break;
      default:
        LOG(FATAL) << "Scaling emphasis: "
                   << ProtoEnumToString(solve_parameters.scaling())
                   << " unknown, error setting Gurobi parameters";
    }
  }

  if (solve_parameters.cuts() != EMPHASIS_UNSPECIFIED) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_CUTS);
    switch (solve_parameters.cuts()) {
      case EMPHASIS_OFF:
        parameter->set_value(absl::StrCat(0));
        break;
      case EMPHASIS_LOW:
      case EMPHASIS_MEDIUM:
        parameter->set_value(absl::StrCat(1));
        break;
      case EMPHASIS_HIGH:
        parameter->set_value(absl::StrCat(2));
        break;
      case EMPHASIS_VERY_HIGH:
        parameter->set_value(absl::StrCat(3));
        break;
      default:
        LOG(FATAL) << "Cuts emphasis: "
                   << ProtoEnumToString(solve_parameters.cuts())
                   << " unknown, error setting Gurobi parameters";
    }
  }

  if (solve_parameters.heuristics() != EMPHASIS_UNSPECIFIED) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_DBL_PAR_HEURISTICS);
    switch (solve_parameters.heuristics()) {
      case EMPHASIS_OFF:
        parameter->set_value(absl::StrCat(0.0));
        break;
      case EMPHASIS_LOW:
        parameter->set_value(absl::StrCat(0.025));
        break;
      case EMPHASIS_MEDIUM:
        // As of Gurobi 9.1 this is the default value.
        // https://www.gurobi.com/documentation/9.1/refman/heuristics.html
        parameter->set_value(absl::StrCat(0.05));
        break;
      case EMPHASIS_HIGH:
        parameter->set_value(absl::StrCat(0.1));
        break;
      case EMPHASIS_VERY_HIGH:
        parameter->set_value(absl::StrCat(0.2));
        break;
      default:
        LOG(FATAL) << "Heuristics emphasis: "
                   << ProtoEnumToString(solve_parameters.heuristics())
                   << " unknown, error setting Gurobi parameters";
    }
  }

  if (solve_parameters.presolve() != EMPHASIS_UNSPECIFIED) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_PRESOLVE);
    switch (solve_parameters.presolve()) {
      case EMPHASIS_OFF:
        parameter->set_value(absl::StrCat(0));
        break;
      case EMPHASIS_LOW:
      case EMPHASIS_MEDIUM:
        parameter->set_value(absl::StrCat(1));
        break;
      case EMPHASIS_HIGH:
      case EMPHASIS_VERY_HIGH:
        parameter->set_value(absl::StrCat(2));
        break;
      default:
        LOG(FATAL) << "Presolve emphasis: "
                   << ProtoEnumToString(solve_parameters.presolve())
                   << " unknown, error setting Gurobi parameters";
    }
  }

  if (solve_parameters.has_iteration_limit()) {
    GurobiParametersProto::Parameter* const iterationlimit =
        merged_parameters.add_parameters();
    iterationlimit->set_name(GRB_DBL_PAR_ITERATIONLIMIT);
    iterationlimit->set_value(absl::StrCat(solve_parameters.iteration_limit()));
    GurobiParametersProto::Parameter* const bariterlimit =
        merged_parameters.add_parameters();
    bariterlimit->set_name(GRB_INT_PAR_BARITERLIMIT);
    double val = std::min<double>(std::numeric_limits<int32_t>::max(),
                                  solve_parameters.iteration_limit());
    bariterlimit->set_value(absl::StrCat(val));
  }

  for (const GurobiParametersProto::Parameter& parameter :
       solve_parameters.gurobi().parameters()) {
    *merged_parameters.add_parameters() = parameter;
  }

  return merged_parameters;
}

absl::StatusOr<int64_t> SafeInt64FromDouble(const double d) {
  const int64_t result = static_cast<int64_t>(d);
  if (static_cast<double>(result) != d) {
    return absl::InternalError(
        absl::StrCat("Expected double ", d, " to contain an int64_t."));
  }
  return result;
}

const absl::flat_hash_set<CallbackEventProto>& SupportedMIPEvents() {
  static const auto* const kEvents =
      new absl::flat_hash_set<CallbackEventProto>({
          CALLBACK_EVENT_PRESOLVE,
          CALLBACK_EVENT_SIMPLEX,
          CALLBACK_EVENT_MIP,
          CALLBACK_EVENT_MIP_SOLUTION,
          CALLBACK_EVENT_MIP_NODE,
          // CALLBACK_EVENT_BARRIER is not supported when solving MIPs; it turns
          // out that Gurobi uses a barrier algorithm to solve the root node
          // relaxation (from the traces) but does not call the associated
          // callback.
      });
  return *kEvents;
}

const absl::flat_hash_set<CallbackEventProto>& SupportedLPEvents() {
  static const auto* const kEvents =
      new absl::flat_hash_set<CallbackEventProto>({
          CALLBACK_EVENT_PRESOLVE,
          CALLBACK_EVENT_SIMPLEX,
          CALLBACK_EVENT_BARRIER,
      });
  return *kEvents;
}

// Gurobi names (model, variables and constraints) must be no longer than 255
// characters; or Gurobi fails with an error.
constexpr std::size_t kMaxNameSize = 255;

// Returns a string of at most kMaxNameSize max size.
std::string TruncateName(const std::string_view original_name) {
  return std::string(
      original_name.substr(0, std::min(kMaxNameSize, original_name.size())));
}

// Truncate the names of variables and constraints.
std::vector<std::string> TruncateNames(
    const google::protobuf::RepeatedPtrField<std::string>& original_names) {
  std::vector<std::string> result;
  result.reserve(original_names.size());
  for (const std::string& original_name : original_names) {
    result.push_back(TruncateName(original_name));
  }
  return result;
}

absl::Status SafeGurobiDouble(const double d) {
  if (std::isfinite(d) && std::abs(d) >= GRB_INFINITY) {
    return util::InvalidArgumentErrorBuilder()
           << "finite value: " << d << " will be treated as infinite by Gurobi";
  }
  return absl::OkStatus();
}

std::string EscapedNameForLogging(const absl::string_view name) {
  return absl::StrCat("\"", absl::Utf8SafeCEscape(name), "\"");
}

constexpr int kDeletedIndex = -1;
constexpr int kUnsetIndex = -2;
// Returns a vector of length `size_before_delete` that logically provides a
// mapping from the starting contiguous range [0, ..., size_before_delete) to
// a potentially smaller range [0, ..., num_remaining_elems) after deleting
// each element in `deletes` and shifting the remaining elements such that they
// are contiguous starting at 0. The elements in the output point to the new
// shifted index, or `kDeletedIndex` if the starting index was deleted.
std::vector<int> IndexUpdateMap(const int size_before_delete,
                                absl::Span<const int> deletes) {
  std::vector<int> result(size_before_delete, kUnsetIndex);
  for (const int del : deletes) {
    result[del] = kDeletedIndex;
  }
  int next_free = 0;
  for (int& r : result) {
    if (r != kDeletedIndex) {
      r = next_free;
      ++next_free;
    }
    CHECK_GT(r, kUnsetIndex);
  }
  return result;
}

absl::StatusOr<bool> GetIntAttrElementAsBool(Gurobi& gurobi,
                                             const char* const name,
                                             const int element) {
  ASSIGN_OR_RETURN(const int value, gurobi.GetIntAttrElement(name, element));
  const bool cast_value(value);
  if (static_cast<int>(cast_value) != value) {
    return util::InternalErrorBuilder()
           << "Gurobi unexpectedly returned non-boolean value for " << name
           << ": " << value;
  }
  return cast_value;
}

class OrAccumulator {
 public:
  OrAccumulator() = default;
  // Propagates any error in `update`. Otherwise, ORs the internal state with
  // the value in `update`.
  absl::Status Or(absl::StatusOr<bool> update) {
    RETURN_IF_ERROR(update.status());
    value_ |= *update;
    return absl::OkStatus();
  }
  bool value() const { return value_; }

 private:
  bool value_ = false;
};

// Propagate any error in `maybe_value`. Otherwise, if `maybe_value` is set,
// add (`id`, `maybe_value`) as an entry to `target`.
absl::Status AddMapEntryIfPresent(
    const int64_t map_id,
    absl::StatusOr<std::optional<ModelSubsetProto::Bounds>> maybe_value,
    google::protobuf::Map<int64_t, ModelSubsetProto::Bounds>& target) {
  RETURN_IF_ERROR(maybe_value.status());
  if (maybe_value->has_value()) {
    target[map_id] = **std::move(maybe_value);
  }
  return absl::OkStatus();
}

// Propagate any error in `should_append`. Otherwise, if `should_append` is
// true, append `id` to `target`.
absl::Status AppendEntryIfTrue(
    const int64_t id, absl::StatusOr<bool> should_append,
    google::protobuf::RepeatedField<int64_t>& target) {
  RETURN_IF_ERROR(should_append.status());
  if (*should_append) {
    target.Add(id);
  }
  return absl::OkStatus();
}

}  // namespace

GurobiSolver::GurobiSolver(std::unique_ptr<Gurobi> g_gurobi)
    : gurobi_(std::move(g_gurobi)) {}

GurobiSolver::GurobiModelElements
GurobiSolver::LinearConstraintData::DependentElements() const {
  GurobiModelElements elements;
  CHECK_NE(constraint_index, kUnspecifiedConstraint);
  elements.linear_constraints.push_back(constraint_index);
  if (slack_index != kUnspecifiedIndex) {
    elements.variables.push_back(slack_index);
  }
  return elements;
}

GurobiSolver::GurobiModelElements
GurobiSolver::SecondOrderConeConstraintData::DependentElements() const {
  const auto index_is_valid = [](const auto index) { return index >= 0; };
  CHECK(absl::c_all_of(slack_variables, index_is_valid));
  CHECK(absl::c_all_of(slack_constraints, index_is_valid));
  CHECK_NE(constraint_index, kUnspecifiedConstraint);
  GurobiModelElements elements{.variables = slack_variables,
                               .linear_constraints = slack_constraints};
  elements.quadratic_constraints.push_back(constraint_index);
  return elements;
}

GurobiSolver::GurobiModelElements
GurobiSolver::SosConstraintData::DependentElements() const {
  const auto index_is_valid = [](const auto index) { return index >= 0; };
  CHECK(absl::c_all_of(slack_variables, index_is_valid));
  CHECK(absl::c_all_of(slack_constraints, index_is_valid));
  CHECK_NE(constraint_index, kUnspecifiedConstraint);
  GurobiModelElements elements{.variables = slack_variables,
                               .linear_constraints = slack_constraints};
  elements.sos_constraints.push_back(constraint_index);
  return elements;
}

absl::StatusOr<TerminationProto> GurobiSolver::ConvertTerminationReason(
    const int gurobi_status, const SolutionClaims solution_claims,
    const double best_primal_bound, const double best_dual_bound) {
  ASSIGN_OR_RETURN(const bool is_maximize, IsMaximize());
  switch (gurobi_status) {
    case GRB_OPTIMAL:
      // TODO(b/290359402): it appears Gurobi could return an infinite
      // best_dual_bound (e.g in Qp/Qc/Socp/multi-obj tests). If so, we could
      // improve the bound by using the target absolute and relative GAPs (and
      // best_primal_bound).
      return OptimalTerminationProto(best_primal_bound, best_dual_bound);
    case GRB_INFEASIBLE:
      return InfeasibleTerminationProto(
          is_maximize, solution_claims.dual_feasible_solution_exists
                           ? FEASIBILITY_STATUS_FEASIBLE
                           : FEASIBILITY_STATUS_UNDETERMINED);
    case GRB_UNBOUNDED:
      // GRB_UNBOUNDED does necessarily imply the primal is feasible
      // https://www.gurobi.com/documentation/9.1/refman/optimization_status_codes.html
      if (solution_claims.primal_feasible_solution_exists) {
        return UnboundedTerminationProto(is_maximize);
      }
      return InfeasibleOrUnboundedTerminationProto(
          is_maximize,
          /*dual_feasibility_status=*/FEASIBILITY_STATUS_INFEASIBLE,
          "Gurobi status GRB_UNBOUNDED");
    case GRB_INF_OR_UNBD:
      return InfeasibleOrUnboundedTerminationProto(
          is_maximize,
          /*dual_feasibility_status=*/FEASIBILITY_STATUS_UNDETERMINED,
          "Gurobi status GRB_UNBOUNDED");
    case GRB_CUTOFF:
      return CutoffTerminationProto(is_maximize, "Gurobi status GRB_CUTOFF");
    case GRB_ITERATION_LIMIT:
      return LimitTerminationProto(
          LIMIT_ITERATION, best_primal_bound, best_dual_bound,
          solution_claims.dual_feasible_solution_exists);
    case GRB_NODE_LIMIT:
      return LimitTerminationProto(
          LIMIT_NODE, best_primal_bound, best_dual_bound,
          solution_claims.dual_feasible_solution_exists);
    case GRB_TIME_LIMIT:
      return LimitTerminationProto(
          LIMIT_TIME, best_primal_bound, best_dual_bound,
          solution_claims.dual_feasible_solution_exists);
    case GRB_SOLUTION_LIMIT:
      return LimitTerminationProto(
          LIMIT_SOLUTION, best_primal_bound, best_dual_bound,
          solution_claims.dual_feasible_solution_exists);
    case GRB_INTERRUPTED:
      return LimitTerminationProto(
          LIMIT_INTERRUPTED, best_primal_bound, best_dual_bound,
          solution_claims.dual_feasible_solution_exists);
    case GRB_NUMERIC:
      return TerminateForReason(is_maximize,
                                TERMINATION_REASON_NUMERICAL_ERROR);
    case GRB_SUBOPTIMAL: {
      if (is_multi_objective_mode()) {
        // Note: We state authoritatively that suboptimal means an objective
        // time out, but we don't really know for sure that there are no other
        // situations in multi-objective mode where this can occur.
        return LimitTerminationProto(
            LIMIT_TIME, best_primal_bound, best_dual_bound,
            solution_claims.dual_feasible_solution_exists,
            "Gurobi returned GRB_SUBOPTIMAL for a multi-objective model, which "
            "indicates that one or more objectives hit their per-objective "
            "time limit");
      }
      return TerminateForReason(is_maximize, TERMINATION_REASON_IMPRECISE);
    }
    case GRB_USER_OBJ_LIMIT:
      // Note: maybe we should override
      // solution_claims.primal_feasible_solution_exists to true or false
      // depending on whether objective_limit and best_bound_limit triggered
      // this. Not sure if it's possible to detect this though.
      return LimitTerminationProto(
          LIMIT_OBJECTIVE, best_primal_bound, best_dual_bound,
          solution_claims.dual_feasible_solution_exists);
    case GRB_LOADED:
      return absl::InternalError(
          "Error creating termination reason, unexpected gurobi status code "
          "GRB_LOADED.");
    case GRB_INPROGRESS:
      return absl::InternalError(
          "Error creating termination reason, unexpected gurobi status code "
          "GRB_INPROGRESS.");
    default:
      return absl::InternalError(absl::StrCat(
          "Missing Gurobi optimization status code case: ", gurobi_status));
  }
}

absl::StatusOr<bool> GurobiSolver::IsMaximize() const {
  ASSIGN_OR_RETURN(const int obj_sense,
                   gurobi_->GetIntAttr(GRB_INT_ATTR_MODELSENSE));
  return obj_sense == GRB_MAXIMIZE;
}

absl::StatusOr<bool> GurobiSolver::IsMIP() const {
  ASSIGN_OR_RETURN(const int is_mip, gurobi_->GetIntAttr(GRB_INT_ATTR_IS_MIP));
  return static_cast<bool>(is_mip);
}

// TODO(b/204595455): Revisit logic when nonconvex QP support is decided upon
absl::StatusOr<bool> GurobiSolver::IsQP() const {
  ASSIGN_OR_RETURN(const int is_qp, gurobi_->GetIntAttr(GRB_INT_ATTR_IS_QP));
  return static_cast<bool>(is_qp);
}

absl::StatusOr<bool> GurobiSolver::IsQCP() const {
  ASSIGN_OR_RETURN(const int is_qcp, gurobi_->GetIntAttr(GRB_INT_ATTR_IS_QCP));
  return static_cast<bool>(is_qcp);
}

// TODO(user): switch the use of this function to something closer to
// GetGurobiDualRay()
template <typename T>
void GurobiSolver::GurobiVectorToSparseDoubleVector(
    const absl::Span<const double> gurobi_values, const T& map,
    SparseDoubleVectorProto& result,
    const SparseVectorFilterProto& filter) const {
  SparseVectorFilterPredicate predicate(filter);
  for (auto [id, gurobi_data] : map) {
    const double value = gurobi_values[get_model_index(gurobi_data)];
    if (predicate.AcceptsAndUpdate(id, value)) {
      result.add_ids(id);
      result.add_values(value);
    }
  }
}

absl::Status GurobiSolver::SetGurobiBasis(const BasisProto& basis) {
  std::vector<int> gurobi_variable_basis_status(num_gurobi_variables_);
  for (const auto [id, value] : MakeView(basis.variable_status())) {
    gurobi_variable_basis_status[variables_map_.at(id)] =
        GrbVariableStatus(static_cast<BasisStatusProto>(value));
  }

  std::vector<int> gurobi_constraint_basis_status;
  gurobi_constraint_basis_status.reserve(num_gurobi_lin_cons_);
  for (const auto [id, value] : MakeView(basis.constraint_status())) {
    const LinearConstraintData& constraint_data =
        linear_constraints_map_.at(id);
    // Non-ranged constraints
    if (constraint_data.slack_index == kUnspecifiedIndex) {
      if (value == BASIS_STATUS_BASIC) {
        gurobi_constraint_basis_status.push_back(kGrbBasicConstraint);
      } else {
        gurobi_constraint_basis_status.push_back(kGrbNonBasicConstraint);
      }
      // Ranged constraints
    } else if (value == BASIS_STATUS_BASIC) {
      // Either constraint or MathOpt slack is basic, but not both (because
      // columns for MathOpt slack and internal Gurobi slack are linearly
      // dependent). We choose the MathOpt slack to be basic.
      gurobi_variable_basis_status[constraint_data.slack_index] = GRB_BASIC;
      gurobi_constraint_basis_status.push_back(kGrbNonBasicConstraint);
    } else {
      gurobi_variable_basis_status[constraint_data.slack_index] =
          GrbVariableStatus(static_cast<BasisStatusProto>(value));
      gurobi_constraint_basis_status.push_back(kGrbNonBasicConstraint);
    }
  }
  RETURN_IF_ERROR(gurobi_->SetIntAttrArray(GRB_INT_ATTR_VBASIS,
                                           gurobi_variable_basis_status));
  RETURN_IF_ERROR(gurobi_->SetIntAttrArray(GRB_INT_ATTR_CBASIS,
                                           gurobi_constraint_basis_status));
  return absl::OkStatus();
}

absl::StatusOr<BasisProto> GurobiSolver::GetGurobiBasis() {
  BasisProto basis;
  ASSIGN_OR_RETURN(
      const std::vector<int> gurobi_variable_basis_status,
      gurobi_->GetIntAttrArray(GRB_INT_ATTR_VBASIS, num_gurobi_variables_));

  for (auto [variable_id, gurobi_variable_index] : variables_map_) {
    basis.mutable_variable_status()->add_ids(variable_id);
    const BasisStatusProto variable_status = ConvertVariableStatus(
        gurobi_variable_basis_status[gurobi_variable_index]);
    if (variable_status == BASIS_STATUS_UNSPECIFIED) {
      return absl::InternalError(
          absl::StrCat("Invalid Gurobi variable basis status: ",
                       gurobi_variable_basis_status[gurobi_variable_index]));
    }
    basis.mutable_variable_status()->add_values(variable_status);
  }

  ASSIGN_OR_RETURN(
      const std::vector<int> gurobi_constraint_basis_status,
      gurobi_->GetIntAttrArray(GRB_INT_ATTR_CBASIS, num_gurobi_lin_cons_));
  for (auto [constraint_id, gurobi_data] : linear_constraints_map_) {
    basis.mutable_constraint_status()->add_ids(constraint_id);
    const int gurobi_constraint_status =
        gurobi_constraint_basis_status[gurobi_data.constraint_index];
    if ((gurobi_constraint_status != kGrbBasicConstraint) &&
        (gurobi_constraint_status != kGrbNonBasicConstraint)) {
      return absl::InternalError(
          absl::StrCat("Invalid Gurobi constraint basis status: ",
                       gurobi_constraint_status));
    }
    // linear_terms <= upper_bound
    if (gurobi_data.lower_bound <= -GRB_INFINITY &&
        gurobi_data.upper_bound < GRB_INFINITY) {
      if (gurobi_constraint_status == kGrbBasicConstraint) {
        basis.mutable_constraint_status()->add_values(BASIS_STATUS_BASIC);
      } else {
        basis.mutable_constraint_status()->add_values(
            BASIS_STATUS_AT_UPPER_BOUND);
      }
      // linear_terms >= lower_bound
    } else if (gurobi_data.lower_bound > -GRB_INFINITY &&
               gurobi_data.upper_bound >= GRB_INFINITY) {
      if (gurobi_constraint_status == kGrbBasicConstraint) {
        basis.mutable_constraint_status()->add_values(BASIS_STATUS_BASIC);
      } else {
        basis.mutable_constraint_status()->add_values(
            BASIS_STATUS_AT_LOWER_BOUND);
      }
      // linear_terms == xxxxx_bound
    } else if (gurobi_data.lower_bound == gurobi_data.upper_bound) {
      if (gurobi_constraint_status == kGrbBasicConstraint) {
        basis.mutable_constraint_status()->add_values(BASIS_STATUS_BASIC);
      } else {
        // TODO(user): consider refining this to
        // AT_LOWER_BOUND/AT_UPPER_BOUND using the sign of the dual variable.
        basis.mutable_constraint_status()->add_values(BASIS_STATUS_FIXED_VALUE);
      }
      //   linear_term - slack == 0 (ranged constraint)
    } else {
      const BasisStatusProto slack_status = ConvertVariableStatus(
          gurobi_variable_basis_status[gurobi_data.slack_index]);
      if (slack_status == BASIS_STATUS_UNSPECIFIED) {
        return absl::InternalError(absl::StrCat(
            "Invalid Gurobi slack variable basis status: ", slack_status));
      }
      if ((gurobi_constraint_status == kGrbBasicConstraint) ||
          (slack_status == BASIS_STATUS_BASIC)) {
        basis.mutable_constraint_status()->add_values(BASIS_STATUS_BASIC);
      } else {
        basis.mutable_constraint_status()->add_values(slack_status);
      }
    }
  }
  return basis;
}

absl::StatusOr<DualRayProto> GurobiSolver::GetGurobiDualRay(
    const SparseVectorFilterProto& linear_constraints_filter,
    const SparseVectorFilterProto& variables_filter, const bool is_maximize) {
  // farkas_dual = lambda
  ASSIGN_OR_RETURN(const std::vector<double> farkas_dual,
                   gurobi_->GetDoubleAttrArray(GRB_DBL_ATTR_FARKASDUAL,
                                               num_gurobi_lin_cons_));

  DualRayProto dual_ray;

  // Compute y = -lambda
  {
    SparseVectorFilterPredicate predicate(linear_constraints_filter);
    for (auto [constraint_id, gurobi_data] : linear_constraints_map_) {
      // constraint_dual_value = y[constraint_id]
      const double value = -farkas_dual[gurobi_data.constraint_index];
      if (predicate.AcceptsAndUpdate(constraint_id, value)) {
        dual_ray.mutable_dual_values()->add_ids(constraint_id);
        if (is_maximize) {
          dual_ray.mutable_dual_values()->add_values(-value);
        } else {
          dual_ray.mutable_dual_values()->add_values(value);
        }
      }
    }
  }

  // Compute r = \bar{a} = A^T lambda
  {
    SparseVectorFilterPredicate predicate(variables_filter);
    for (auto [var_id, gurobi_variable_index] : variables_map_) {
      // reduced_cost_value = r[gurobi_variable_index]
      //                    = \bar{a}[gurobi_variable_index]
      double reduced_cost_value = 0.0;
      ASSIGN_OR_RETURN(Gurobi::SparseMat column,
                       gurobi_->GetVars(gurobi_variable_index, 1));
      for (int i = 0; i < column.inds.size(); ++i) {
        reduced_cost_value += farkas_dual[column.inds[i]] * column.vals[i];
      }
      if (predicate.AcceptsAndUpdate(var_id, reduced_cost_value)) {
        dual_ray.mutable_reduced_costs()->add_ids(var_id);
        if (is_maximize) {
          dual_ray.mutable_reduced_costs()->add_values(-reduced_cost_value);
        } else {
          dual_ray.mutable_reduced_costs()->add_values(reduced_cost_value);
        }
      }
    }
  }
  return dual_ray;
}

absl::StatusOr<SolveResultProto> GurobiSolver::ExtractSolveResultProto(
    const absl::Time start, const ModelSolveParametersProto& model_parameters) {
  SolveResultProto result;
  ASSIGN_OR_RETURN(const int grb_termination,
                   gurobi_->GetIntAttr(GRB_INT_ATTR_STATUS));
  SolutionClaims solution_claims;
  ASSIGN_OR_RETURN(SolutionsAndClaims solution_and_claims,
                   GetSolutions(model_parameters));
  if (grb_termination == GRB_CUTOFF) {
    // Cutoff will not be triggered by bounds e.g. for LP dual feasible
    // solutions. In particular, if the problem is both primal and dual
    // infeasible, we will not get a bound and should not be returning CUTOFF.
    //
    // TODO(b/272268188): test that this has no bad interactions with primal +
    // dual infeasible problems.
    solution_and_claims.solutions.clear();
    solution_and_claims.solution_claims.primal_feasible_solution_exists = false;
    solution_and_claims.solution_claims.dual_feasible_solution_exists = true;
  }
  ASSIGN_OR_RETURN(const double best_primal_bound,
                   GetBestPrimalBound(solution_and_claims.solutions));
  ASSIGN_OR_RETURN(const double best_dual_bound,
                   GetBestDualBound(solution_and_claims.solutions));
  solution_claims = solution_and_claims.solution_claims;

  // TODO(b/195295177): Add tests for rays in unbounded MIPs
  RETURN_IF_ERROR(FillRays(model_parameters, solution_claims, result));

  for (SolutionProto& solution : solution_and_claims.solutions) {
    *result.add_solutions() = std::move(solution);
  }

  ASSIGN_OR_RETURN(
      *result.mutable_termination(),
      ConvertTerminationReason(grb_termination, solution_claims,
                               best_primal_bound, best_dual_bound));

  ASSIGN_OR_RETURN(*result.mutable_solve_stats(), GetSolveStats(start));
  return result;
}

absl::StatusOr<bool> GurobiSolver::AnyElementInIIS(
    const GurobiModelElements& grb_elements) {
  OrAccumulator any_in_iis;
  for (const GurobiVariableIndex grb_index : grb_elements.variables) {
    RETURN_IF_ERROR(any_in_iis.Or(VariableInIIS(grb_index)));
  }
  for (const GurobiLinearConstraintIndex grb_index :
       grb_elements.linear_constraints) {
    RETURN_IF_ERROR(any_in_iis.Or(
        GetIntAttrElementAsBool(*gurobi_, GRB_INT_ATTR_IIS_CONSTR, grb_index)));
  }
  for (const GurobiQuadraticConstraintIndex grb_index :
       grb_elements.quadratic_constraints) {
    RETURN_IF_ERROR(any_in_iis.Or(GetIntAttrElementAsBool(
        *gurobi_, GRB_INT_ATTR_IIS_QCONSTR, grb_index)));
  }
  for (const GurobiSosConstraintIndex grb_index :
       grb_elements.sos_constraints) {
    RETURN_IF_ERROR(any_in_iis.Or(
        GetIntAttrElementAsBool(*gurobi_, GRB_INT_ATTR_IIS_SOS, grb_index)));
  }
  for (const GurobiGeneralConstraintIndex grb_index :
       grb_elements.general_constraints) {
    RETURN_IF_ERROR(any_in_iis.Or(GetIntAttrElementAsBool(
        *gurobi_, GRB_INT_ATTR_IIS_GENCONSTR, grb_index)));
  }
  return any_in_iis.value();
}

// If set, the returned value will have at least one of .lower and/or .upper set
// to true.
absl::StatusOr<std::optional<ModelSubsetProto::Bounds>>
GurobiSolver::VariableBoundsInIIS(const GurobiVariableIndex grb_index) {
  ASSIGN_OR_RETURN(
      const bool var_lb_is_in_iis,
      GetIntAttrElementAsBool(*gurobi_, GRB_INT_ATTR_IIS_LB, grb_index));
  ASSIGN_OR_RETURN(
      const bool var_ub_is_in_iis,
      GetIntAttrElementAsBool(*gurobi_, GRB_INT_ATTR_IIS_UB, grb_index));
  if (!var_lb_is_in_iis && !var_ub_is_in_iis) {
    return std::nullopt;
  }
  ModelSubsetProto::Bounds bounds;
  bounds.set_lower(var_lb_is_in_iis);
  bounds.set_upper(var_ub_is_in_iis);
  return bounds;
}

absl::StatusOr<bool> GurobiSolver::VariableInIIS(
    const GurobiVariableIndex grb_index) {
  ASSIGN_OR_RETURN(const std::optional<ModelSubsetProto::Bounds> bounds,
                   VariableBoundsInIIS(grb_index));
  return bounds.has_value();
}

absl::StatusOr<std::optional<ModelSubsetProto::Bounds>>
GurobiSolver::LinearConstraintInIIS(const LinearConstraintData& grb_data) {
  // Attributing which part of an equality/ranged constraint is part of the
  // IIS can be tricky. So, we take a conservative approach: if anything
  // associated with this linear constraint is part of the IIS (including
  // slack variable/constraints), then we mark any finite bound as being part
  // of the IIS.
  ASSIGN_OR_RETURN(const bool constr_in_iis,
                   AnyElementInIIS(grb_data.DependentElements()));
  if (!constr_in_iis) {
    return std::nullopt;
  }
  ModelSubsetProto::Bounds result;
  result.set_lower(grb_data.lower_bound != -kInf);
  result.set_upper(grb_data.upper_bound != kInf);
  return result;
}

absl::StatusOr<std::optional<ModelSubsetProto::Bounds>>
GurobiSolver::QuadraticConstraintInIIS(
    const GurobiQuadraticConstraintIndex grb_index) {
  ASSIGN_OR_RETURN(
      const bool constr_in_iis,
      GetIntAttrElementAsBool(*gurobi_, GRB_INT_ATTR_IIS_QCONSTR, grb_index));
  if (!constr_in_iis) {
    return std::nullopt;
  }
  ASSIGN_OR_RETURN(
      const char constr_sense,
      gurobi_->GetCharAttrElement(GRB_CHAR_ATTR_QCSENSE, grb_index));
  ModelSubsetProto::Bounds result;
  result.set_lower(constr_sense == GRB_EQUAL ||
                   constr_sense == GRB_GREATER_EQUAL);
  result.set_upper(constr_sense == GRB_EQUAL || constr_sense == GRB_LESS_EQUAL);
  return result;
}

absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
GurobiSolver::ExtractComputeInfeasibleSubsystemResultProto(
    const bool proven_infeasible) {
  ComputeInfeasibleSubsystemResultProto result;
  if (!proven_infeasible) {
    result.set_feasibility(FEASIBILITY_STATUS_UNDETERMINED);
    return result;
  }
  result.set_feasibility(FEASIBILITY_STATUS_INFEASIBLE);
  {
    ASSIGN_OR_RETURN(const bool is_minimal,
                     gurobi_->GetIntAttr(GRB_INT_ATTR_IIS_MINIMAL));
    result.set_is_minimal(is_minimal);
  }
  for (const auto [id, grb_index] : variables_map_) {
    RETURN_IF_ERROR(AddMapEntryIfPresent(
        id, /*maybe_value=*/VariableBoundsInIIS(grb_index),
        *result.mutable_infeasible_subsystem()->mutable_variable_bounds()));
    {
      // Gurobi does not report integrality in its IIS, so we mark any integer
      // variable in the model as being involved.
      ASSIGN_OR_RETURN(
          const char var_type,
          gurobi_->GetCharAttrElement(GRB_CHAR_ATTR_VTYPE, grb_index));
      if (var_type == GRB_BINARY || var_type == GRB_INTEGER) {
        result.mutable_infeasible_subsystem()->add_variable_integrality(
            grb_index);
      }
    }
  }

  for (const auto [id, grb_data] : linear_constraints_map_) {
    RETURN_IF_ERROR(AddMapEntryIfPresent(
        id, /*maybe_value=*/LinearConstraintInIIS(grb_data),
        *result.mutable_infeasible_subsystem()->mutable_linear_constraints()));
  }

  for (const auto [id, grb_index] : quadratic_constraints_map_) {
    RETURN_IF_ERROR(AddMapEntryIfPresent(
        id, /*maybe_value=*/QuadraticConstraintInIIS(grb_index),
        *result.mutable_infeasible_subsystem()
             ->mutable_quadratic_constraints()));
  }

  for (const auto& [id, grb_data] : soc_constraints_map_) {
    RETURN_IF_ERROR(AppendEntryIfTrue(
        id, /*should_append=*/AnyElementInIIS(grb_data.DependentElements()),
        *result.mutable_infeasible_subsystem()
             ->mutable_second_order_cone_constraints()));
  }

  for (const auto& [id, grb_data] : sos1_constraints_map_) {
    RETURN_IF_ERROR(AppendEntryIfTrue(
        id, /*should_append=*/AnyElementInIIS(grb_data.DependentElements()),
        *result.mutable_infeasible_subsystem()->mutable_sos1_constraints()));
  }

  for (const auto& [id, grb_data] : sos2_constraints_map_) {
    RETURN_IF_ERROR(AppendEntryIfTrue(
        id, /*should_append=*/AnyElementInIIS(grb_data.DependentElements()),
        *result.mutable_infeasible_subsystem()->mutable_sos2_constraints()));
  }

  for (const auto& [id, maybe_grb_data] : indicator_constraints_map_) {
    if (!maybe_grb_data.has_value()) {
      continue;
    }
    RETURN_IF_ERROR(AppendEntryIfTrue(
        id,
        /*should_append=*/
        GetIntAttrElementAsBool(*gurobi_, GRB_INT_ATTR_IIS_GENCONSTR,
                                maybe_grb_data->constraint_index),
        *result.mutable_infeasible_subsystem()
             ->mutable_indicator_constraints()));
  }

  // Since each xxx_map_ is in insertion order, we do not need to worry about
  // sorting the repeated fields in `result`.
  return result;
}

absl::StatusOr<GurobiSolver::SolutionsAndClaims> GurobiSolver::GetSolutions(
    const ModelSolveParametersProto& model_parameters) {
  // Note that all multi-objective models will have `IsMip()` return true.
  ASSIGN_OR_RETURN(const bool is_mip, IsMIP());
  ASSIGN_OR_RETURN(const bool is_qp, IsQP());
  ASSIGN_OR_RETURN(const bool is_qcp, IsQCP());

  if (is_mip) {
    return GetMipSolutions(model_parameters);
  } else if (is_qcp) {
    return GetQcpSolution(model_parameters);
  } else if (is_qp) {
    return GetQpSolution(model_parameters);
  } else {
    return GetLpSolution(model_parameters);
  }
}

// TODO: b/365762174 - Remove logging and clamping below when Gurobi fixes its
// behavior upstream (and we migrate onto that version).
absl::StatusOr<SolveStatsProto> GurobiSolver::GetSolveStats(
    const absl::Time start) const {
  SolveStatsProto solve_stats;

  CHECK_OK(util_time::EncodeGoogleApiProto(absl::Now() - start,
                                           solve_stats.mutable_solve_time()));

  if (gurobi_->IsAttrAvailable(GRB_DBL_ATTR_ITERCOUNT)) {
    ASSIGN_OR_RETURN(const double simplex_iters_double,
                     gurobi_->GetDoubleAttr(GRB_DBL_ATTR_ITERCOUNT));
    ASSIGN_OR_RETURN(const int64_t simplex_iters,
                     SafeInt64FromDouble(simplex_iters_double));
    LOG_IF(ERROR, simplex_iters < 0)
        << "Expected GRB_DBL_ATTR_ITERCOUNT to be non-negative, got: "
        << simplex_iters << "; clamping to 0";
    solve_stats.set_simplex_iterations(std::max(int64_t{0}, simplex_iters));
  }

  if (gurobi_->IsAttrAvailable(GRB_INT_ATTR_BARITERCOUNT)) {
    ASSIGN_OR_RETURN(const int barrier_iters,
                     gurobi_->GetIntAttr(GRB_INT_ATTR_BARITERCOUNT));
    LOG_IF(ERROR, barrier_iters < 0)
        << "Expected GRB_INT_ATTR_BARITERCOUNT to be non-negative, got: "
        << barrier_iters << "; clamping to 0";
    solve_stats.set_barrier_iterations(std::max(0, barrier_iters));
  }

  if (gurobi_->IsAttrAvailable(GRB_DBL_ATTR_NODECOUNT)) {
    ASSIGN_OR_RETURN(const double nodes_double,
                     gurobi_->GetDoubleAttr(GRB_DBL_ATTR_NODECOUNT));
    ASSIGN_OR_RETURN(const int64_t nodes, SafeInt64FromDouble(nodes_double));
    LOG_IF(ERROR, nodes < 0)
        << "Expected GRB_DBL_ATTR_NODECOUNT to be non-negative, got: " << nodes
        << "; clamping to 0";
    solve_stats.set_node_count(std::max(int64_t{0}, nodes));
  }
  return solve_stats;
}

absl::StatusOr<GurobiSolver::SolutionsAndClaims> GurobiSolver::GetMipSolutions(
    const ModelSolveParametersProto& model_parameters) {
  int num_solutions = 0;
  if (gurobi_->IsAttrAvailable(GRB_INT_ATTR_SOLCOUNT)) {
    ASSIGN_OR_RETURN(num_solutions, gurobi_->GetIntAttr(GRB_INT_ATTR_SOLCOUNT));
  }
  std::vector<SolutionProto> solutions;
  solutions.reserve(num_solutions);

  for (int i = 0; i < num_solutions; ++i) {
    RETURN_IF_ERROR(gurobi_->SetIntParam(GRB_INT_PAR_SOLUTIONNUMBER, i));

    PrimalSolutionProto primal_solution;
    ASSIGN_OR_RETURN(const double sol_val,
                     gurobi_->GetDoubleAttr(GRB_DBL_ATTR_POOLOBJVAL));
    primal_solution.set_objective_value(sol_val);
    if (is_multi_objective_mode()) {
      for (const auto [id, grb_index] : multi_objectives_map_) {
        RETURN_IF_ERROR(gurobi_->SetIntParam(GRB_INT_PAR_OBJNUMBER, grb_index));
        ASSIGN_OR_RETURN(const double obj_val,
                         gurobi_->GetDoubleAttr(GRB_DBL_ATTR_OBJNVAL));
        // If unset, this is the primary objective. We have already queried its
        // value via PoolObjVal above.
        if (id.has_value()) {
          (*primal_solution.mutable_auxiliary_objective_values())[*id] =
              obj_val;
        }
      }
    }
    // Gurobi v9 provides a feasibility status for the instance as a whole but
    // not for each solution, and pool entries may be infeasible. To be
    // conservative, we only label the first ("best") solution as primal
    // feasible.
    primal_solution.set_feasibility_status(
        i == 0 ? SOLUTION_STATUS_FEASIBLE : SOLUTION_STATUS_UNDETERMINED);
    ASSIGN_OR_RETURN(
        const std::vector<double> grb_var_values,
        gurobi_->GetDoubleAttrArray(GRB_DBL_ATTR_XN, num_gurobi_variables_));
    GurobiVectorToSparseDoubleVector(grb_var_values, variables_map_,
                                     *primal_solution.mutable_variable_values(),
                                     model_parameters.variable_values_filter());
    *solutions.emplace_back(SolutionProto()).mutable_primal_solution() =
        std::move(primal_solution);
  }

  ASSIGN_OR_RETURN(const int grb_termination,
                   gurobi_->GetIntAttr(GRB_INT_ATTR_STATUS));
  // Set solution claims
  ASSIGN_OR_RETURN(const double best_dual_bound, GetGurobiBestDualBound());
  // Note: here the existence of a dual solution refers to a dual solution to
  // some convex relaxation of the MIP. This convex relaxation can likely be
  // interpreted as an LP between the LP relaxation of the MIP and the convex
  // hull of feasible solutions of the MIP. However, here we only use the fact
  // that best_dual_bound being finite implies the existence of the trivial
  // convex relaxation given by (assuming a minimization problem with objective
  // function c^T x): min{c^T x : c^T x >= best_dual_bound}.
  //
  // If this is a multi-objective model, Gurobi v10 does not expose ObjBound.
  // Instead, we fake its existence for optimal solves only.
  // By convention infeasible MIPs are always dual feasible.
  const SolutionClaims solution_claims = {
      .primal_feasible_solution_exists = num_solutions > 0,
      .dual_feasible_solution_exists =
          std::isfinite(best_dual_bound) || grb_termination == GRB_INFEASIBLE ||
          (is_multi_objective_mode() && grb_termination == GRB_OPTIMAL)};

  // Check consistency of solutions, bounds and statuses.
  if (grb_termination == GRB_OPTIMAL && num_solutions == 0) {
    return absl::InternalError(
        "GRB_INT_ATTR_STATUS == GRB_OPTIMAL, but solution pool is empty.");
  }
  // As set above, in multi-objective mode the dual bound is not informative and
  // it will not pass this validation.
  if (!is_multi_objective_mode() && grb_termination == GRB_OPTIMAL &&
      !std::isfinite(best_dual_bound)) {
    return absl::InternalError(
        "GRB_INT_ATTR_STATUS == GRB_OPTIMAL, but GRB_DBL_ATTR_OBJBOUND is "
        "unavailable or infinite.");
  }

  return SolutionsAndClaims{.solutions = std::move(solutions),
                            .solution_claims = solution_claims};
}

absl::StatusOr<GurobiSolver::SolutionAndClaim<PrimalSolutionProto>>
GurobiSolver::GetConvexPrimalSolutionIfAvailable(
    const ModelSolveParametersProto& model_parameters) {
  if (!gurobi_->IsAttrAvailable(GRB_DBL_ATTR_X)) {
    return SolutionAndClaim<PrimalSolutionProto>{
        .solution = std::nullopt, .feasible_solution_exists = false};
  }
  ASSIGN_OR_RETURN(const int grb_termination,
                   gurobi_->GetIntAttr(GRB_INT_ATTR_STATUS));

  // Get primal solutions if available.
  ASSIGN_OR_RETURN(
      const std::vector<double> grb_var_values,
      gurobi_->GetDoubleAttrArray(GRB_DBL_ATTR_X, num_gurobi_variables_));

  PrimalSolutionProto primal_solution;
  // The objective value may be missing for primal feasible solutions for
  // unbounded problems.
  // TODO(b/195295177): for GRB_ITERATION_LIMIT an objective value of 0.0 is
  // returned which breaks LpIncompleteSolveTest.PrimalSimplexAlgorithm. Explore
  // more and make simple example to file a bug.
  if (gurobi_->IsAttrAvailable(GRB_DBL_ATTR_OBJVAL) &&
      grb_termination != GRB_ITERATION_LIMIT) {
    ASSIGN_OR_RETURN(const double sol_val,
                     gurobi_->GetDoubleAttr(GRB_DBL_ATTR_OBJVAL));
    primal_solution.set_objective_value(sol_val);
  } else {
    double objective_value = 0.0;
    ASSIGN_OR_RETURN(
        const std::vector<double> linear_obj_coefs,
        gurobi_->GetDoubleAttrArray(GRB_DBL_ATTR_OBJ, num_gurobi_variables_));
    for (int i = 0; i < num_gurobi_variables_; ++i) {
      objective_value += linear_obj_coefs[i] * grb_var_values[i];
    }
    primal_solution.set_objective_value(objective_value);
  }

  primal_solution.set_feasibility_status(SOLUTION_STATUS_UNDETERMINED);
  if (grb_termination == GRB_OPTIMAL) {
    primal_solution.set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
  } else if (grb_termination == GRB_INFEASIBLE) {
    primal_solution.set_feasibility_status(SOLUTION_STATUS_INFEASIBLE);
  } else if (PrimalSolutionQualityAvailable()) {
    ASSIGN_OR_RETURN(const double solution_quality, GetPrimalSolutionQuality());
    ASSIGN_OR_RETURN(const double tolerance,
                     gurobi_->GetDoubleParam(GRB_DBL_PAR_FEASIBILITYTOL));
    if (solution_quality <= tolerance) {
      primal_solution.set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
    } else {
      primal_solution.set_feasibility_status(SOLUTION_STATUS_INFEASIBLE);
    }
  }

  GurobiVectorToSparseDoubleVector(grb_var_values, variables_map_,
                                   *primal_solution.mutable_variable_values(),
                                   model_parameters.variable_values_filter());
  const bool primal_feasible_solution_exists =
      (primal_solution.feasibility_status() == SOLUTION_STATUS_FEASIBLE);
  return SolutionAndClaim<PrimalSolutionProto>{
      .solution = std::move(primal_solution),
      .feasible_solution_exists = primal_feasible_solution_exists};
}

bool GurobiSolver::PrimalSolutionQualityAvailable() const {
  return gurobi_->IsAttrAvailable(GRB_DBL_ATTR_CONSTR_RESIDUAL) &&
         gurobi_->IsAttrAvailable(GRB_DBL_ATTR_CONSTR_VIO) &&
         gurobi_->IsAttrAvailable(GRB_DBL_ATTR_BOUND_VIO) &&
         gurobi_->IsAttrAvailable(GRB_DBL_ATTR_CONSTR_SRESIDUAL) &&
         gurobi_->IsAttrAvailable(GRB_DBL_ATTR_CONSTR_SVIO) &&
         gurobi_->IsAttrAvailable(GRB_DBL_ATTR_BOUND_SVIO);
}

absl::StatusOr<double> GurobiSolver::GetPrimalSolutionQuality() const {
  ASSIGN_OR_RETURN(const double constraint_residual,
                   gurobi_->GetDoubleAttr(GRB_DBL_ATTR_CONSTR_RESIDUAL));
  ASSIGN_OR_RETURN(const double constraint_violation,
                   gurobi_->GetDoubleAttr(GRB_DBL_ATTR_CONSTR_VIO));
  ASSIGN_OR_RETURN(const double bound_violation,
                   gurobi_->GetDoubleAttr(GRB_DBL_ATTR_BOUND_VIO));
  ASSIGN_OR_RETURN(const double constraint_scaled_residual,
                   gurobi_->GetDoubleAttr(GRB_DBL_ATTR_CONSTR_SRESIDUAL));
  ASSIGN_OR_RETURN(const double constraint_scaled_violation,
                   gurobi_->GetDoubleAttr(GRB_DBL_ATTR_CONSTR_SVIO));
  ASSIGN_OR_RETURN(const double bound_scaled_violation,
                   gurobi_->GetDoubleAttr(GRB_DBL_ATTR_BOUND_SVIO));
  return std::max({constraint_residual, constraint_violation, bound_violation,
                   constraint_scaled_residual, constraint_scaled_violation,
                   bound_scaled_violation});
}

absl::StatusOr<double> GurobiSolver::GetBestPrimalBound(
    absl::Span<const SolutionProto> solutions) const {
  // We avoid using GRB_DBL_ATTR_OBJVAL because it may be incorrect on early
  // termination and for infeasible solutions (as of Gurobi 9.0.1).
  // Note that for (primal) unbounded problems the primal_bound is correctly
  // filled by ConvertTerminationReason() possibly overriding the value
  // returned by GetBestPrimalBound().
  ASSIGN_OR_RETURN(const bool is_maximize, IsMaximize());
  double best_objective_value = is_maximize ? -kInf : kInf;
  for (const SolutionProto& solution : solutions) {
    if (solution.has_primal_solution() &&
        solution.primal_solution().feasibility_status() ==
            SOLUTION_STATUS_FEASIBLE) {
      const double sol_val = solution.primal_solution().objective_value();
      best_objective_value = is_maximize
                                 ? std::max(best_objective_value, sol_val)
                                 : std::min(best_objective_value, sol_val);
    }
  }
  return best_objective_value;
}

absl::StatusOr<double> GurobiSolver::GetBestDualBound(
    absl::Span<const SolutionProto> solutions) const {
  ASSIGN_OR_RETURN(const bool is_maximize, IsMaximize());
  // GetGurobiBestDualBound() returns the correct bound for problems without
  // dual solutions (e.g. MIP).
  ASSIGN_OR_RETURN(double best_dual_bound, GetGurobiBestDualBound());
  // However, GetGurobiBestDualBound() may be incorrect for QP problems.
  for (const SolutionProto& solution : solutions) {
    if (solution.has_dual_solution() &&
        solution.dual_solution().feasibility_status() ==
            SOLUTION_STATUS_FEASIBLE &&
        solution.dual_solution().has_objective_value()) {
      const double sol_val = solution.dual_solution().objective_value();
      best_dual_bound = is_maximize ? std::min(best_dual_bound, sol_val)
                                    : std::max(best_dual_bound, sol_val);
    }
  }
  return best_dual_bound;
}

absl::StatusOr<double> GurobiSolver::GetGurobiBestDualBound() const {
  // As of v9.0.2, on multi objective models Gurobi incorrectly reports that
  // ObjBound is available. We work around this by adding a check if we are in
  // multi objective mode.
  if (gurobi_->IsAttrAvailable(GRB_DBL_ATTR_OBJBOUND) &&
      !is_multi_objective_mode()) {
    ASSIGN_OR_RETURN(const double obj_bound,
                     gurobi_->GetDoubleAttr(GRB_DBL_ATTR_OBJBOUND));
    // Note: Unbounded models return GRB_DBL_ATTR_OBJBOUND = GRB_INFINITY so
    // the conversion of +/-GRB_INFINITY to +/-kInf is needed and consistent.
    if (std::abs(obj_bound) < GRB_INFINITY) {
      return obj_bound;
    }
  }
  ASSIGN_OR_RETURN(const bool is_maximize, IsMaximize());
  return is_maximize ? kInf : -kInf;
}

absl::StatusOr<std::optional<BasisProto>> GurobiSolver::GetBasisIfAvailable() {
  if (gurobi_->IsAttrAvailable(GRB_INT_ATTR_VBASIS) &&
      gurobi_->IsAttrAvailable(GRB_INT_ATTR_CBASIS)) {
    ASSIGN_OR_RETURN(BasisProto basis, GetGurobiBasis());
    ASSIGN_OR_RETURN(const int grb_termination,
                     gurobi_->GetIntAttr(GRB_INT_ATTR_STATUS));
    basis.set_basic_dual_feasibility(SOLUTION_STATUS_UNDETERMINED);
    if (grb_termination == GRB_OPTIMAL) {
      basis.set_basic_dual_feasibility(SOLUTION_STATUS_FEASIBLE);
    } else if (grb_termination == GRB_UNBOUNDED) {
      basis.set_basic_dual_feasibility(SOLUTION_STATUS_INFEASIBLE);
    }
    // TODO(b/195295177): double check if the move is needed
    return std::move(basis);
  }
  return std::nullopt;
}

absl::StatusOr<GurobiSolver::SolutionsAndClaims> GurobiSolver::GetLpSolution(
    const ModelSolveParametersProto& model_parameters) {
  ASSIGN_OR_RETURN(auto primal_solution_and_claim,
                   GetConvexPrimalSolutionIfAvailable(model_parameters));
  ASSIGN_OR_RETURN(auto dual_solution_and_claim,
                   GetConvexDualSolutionIfAvailable(model_parameters));
  ASSIGN_OR_RETURN(auto basis, GetBasisIfAvailable());
  const SolutionClaims solution_claims = {
      .primal_feasible_solution_exists =
          primal_solution_and_claim.feasible_solution_exists,
      .dual_feasible_solution_exists =
          dual_solution_and_claim.feasible_solution_exists};

  if (!primal_solution_and_claim.solution.has_value() &&
      !dual_solution_and_claim.solution.has_value() && !basis.has_value()) {
    return SolutionsAndClaims{.solution_claims = solution_claims};
  }
  SolutionsAndClaims solution_and_claims{.solution_claims = solution_claims};
  SolutionProto& solution =
      solution_and_claims.solutions.emplace_back(SolutionProto());
  if (primal_solution_and_claim.solution.has_value()) {
    *solution.mutable_primal_solution() =
        std::move(*primal_solution_and_claim.solution);
  }
  if (dual_solution_and_claim.solution.has_value()) {
    *solution.mutable_dual_solution() =
        std::move(*dual_solution_and_claim.solution);
  }
  if (basis.has_value()) {
    *solution.mutable_basis() = std::move(*basis);
  }
  return solution_and_claims;
}

absl::StatusOr<GurobiSolver::SolutionAndClaim<DualSolutionProto>>
GurobiSolver::GetConvexDualSolutionIfAvailable(
    const ModelSolveParametersProto& model_parameters) {
  if (!gurobi_->IsAttrAvailable(GRB_DBL_ATTR_PI) ||
      !gurobi_->IsAttrAvailable(GRB_DBL_ATTR_RC)) {
    return SolutionAndClaim<DualSolutionProto>{
        .solution = std::nullopt, .feasible_solution_exists = false};
  }

  // Note that we can ignore the reduced costs of the slack variables for
  // ranged constraints.
  DualSolutionProto dual_solution;
  bool dual_feasible_solution_exists = false;
  ASSIGN_OR_RETURN(
      const std::vector<double> grb_constraint_duals,
      gurobi_->GetDoubleAttrArray(GRB_DBL_ATTR_PI, num_gurobi_lin_cons_));
  GurobiVectorToSparseDoubleVector(grb_constraint_duals,
                                   linear_constraints_map_,
                                   *dual_solution.mutable_dual_values(),
                                   model_parameters.dual_values_filter());

  ASSIGN_OR_RETURN(
      const std::vector<double> grb_reduced_cost_values,
      gurobi_->GetDoubleAttrArray(GRB_DBL_ATTR_RC, num_gurobi_variables_));
  GurobiVectorToSparseDoubleVector(grb_reduced_cost_values, variables_map_,
                                   *dual_solution.mutable_reduced_costs(),
                                   model_parameters.reduced_costs_filter());

  if (!quadratic_constraints_map_.empty() &&
      gurobi_->IsAttrAvailable(GRB_DBL_ATTR_QCPI)) {
    ASSIGN_OR_RETURN(
        const std::vector<double> grb_quad_constraint_duals,
        gurobi_->GetDoubleAttrArray(GRB_DBL_ATTR_QCPI, num_gurobi_quad_cons_));
    GurobiVectorToSparseDoubleVector(
        grb_quad_constraint_duals, quadratic_constraints_map_,
        *dual_solution.mutable_quadratic_dual_values(),
        model_parameters.quadratic_dual_values_filter());
  }

  ASSIGN_OR_RETURN(const int grb_termination,
                   gurobi_->GetIntAttr(GRB_INT_ATTR_STATUS));
  if (grb_termination == GRB_OPTIMAL &&
      gurobi_->IsAttrAvailable(GRB_DBL_ATTR_OBJVAL)) {
    ASSIGN_OR_RETURN(const double obj_val,
                     gurobi_->GetDoubleAttr(GRB_DBL_ATTR_OBJVAL));
    dual_solution.set_objective_value(obj_val);
  }

  dual_solution.set_feasibility_status(SOLUTION_STATUS_UNDETERMINED);
  if (grb_termination == GRB_OPTIMAL) {
    dual_solution.set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
    dual_feasible_solution_exists = true;
  } else if (grb_termination == GRB_UNBOUNDED) {
    dual_solution.set_feasibility_status(SOLUTION_STATUS_INFEASIBLE);
  }
  // TODO(b/290359402): We could use gurobi's dual solution quality measures
  // for further upgrade the dual feasibility but it likely is only useful
  // for phase II of dual simplex because:
  //   * the quality measures seem to evaluate if the basis is dual feasible
  //     so for primal simplex we would not improve over checking
  //     GRB_OPTIMAL.
  //   * for phase I dual simplex we cannot rely on the quality measures.
  // We could also use finiteness of GRB_DBL_ATTR_OBJBOUND to deduce dual
  // feasibility.

  // Note: GRB_DBL_ATTR_OBJBOUND can sometimes provide the objective value of a
  // sub-optimal dual feasible solution.
  // Here we only use it to possibly update dual_feasible_solution_exists.
  ASSIGN_OR_RETURN(const double best_dual_bound, GetGurobiBestDualBound());
  if (dual_feasible_solution_exists || std::isfinite(best_dual_bound)) {
    dual_feasible_solution_exists = true;
  } else if (grb_termination == GRB_OPTIMAL) {
    return absl::InternalError(
        "GRB_INT_ATTR_STATUS == GRB_OPTIMAL, but GRB_DBL_ATTR_OBJBOUND is "
        "unavailable or infinite, and no dual feasible solution is returned");
  }
  return SolutionAndClaim<DualSolutionProto>{
      .solution = std::move(dual_solution),
      .feasible_solution_exists = dual_feasible_solution_exists};
}

absl::Status GurobiSolver::FillRays(
    const ModelSolveParametersProto& model_parameters,
    const SolutionClaims solution_claims, SolveResultProto& result) {
  ASSIGN_OR_RETURN(const bool is_maximize, IsMaximize());
  // GRB_DBL_ATTR_UNBDRAY is sometimes incorrectly available for problems
  // without variables. We also give priority to the conclusions obtained from
  // dual solutions or bounds.
  if (!solution_claims.dual_feasible_solution_exists &&
      num_gurobi_variables_ > 0 &&
      gurobi_->IsAttrAvailable(GRB_DBL_ATTR_UNBDRAY)) {
    ASSIGN_OR_RETURN(const std::vector<double> grb_ray_var_values,
                     gurobi_->GetDoubleAttrArray(GRB_DBL_ATTR_UNBDRAY,
                                                 num_gurobi_variables_));
    PrimalRayProto* const primal_ray = result.add_primal_rays();
    GurobiVectorToSparseDoubleVector(grb_ray_var_values, variables_map_,
                                     *primal_ray->mutable_variable_values(),
                                     model_parameters.variable_values_filter());
  }
  // GRB_DBL_ATTR_FARKASDUAL is sometimes incorrectly available for problems
  // without constraints. We also give priority to the conclusions obtained from
  // primal solutions.
  if (!solution_claims.primal_feasible_solution_exists &&
      num_gurobi_lin_cons_ > 0 &&
      gurobi_->IsAttrAvailable(GRB_DBL_ATTR_FARKASDUAL)) {
    ASSIGN_OR_RETURN(
        DualRayProto dual_ray,
        GetGurobiDualRay(model_parameters.dual_values_filter(),
                         model_parameters.reduced_costs_filter(), is_maximize));
    result.mutable_dual_rays()->Add(std::move(dual_ray));
  }
  return absl::OkStatus();
}

absl::StatusOr<GurobiSolver::SolutionsAndClaims> GurobiSolver::GetQpSolution(
    const ModelSolveParametersProto& model_parameters) {
  ASSIGN_OR_RETURN((auto [primal_solution, found_primal_feasible_solution]),
                   GetConvexPrimalSolutionIfAvailable(model_parameters));
  // TODO(b/225189115): Expand QpDualsTest to check maximization problems and
  // other edge cases.
  ASSIGN_OR_RETURN((auto [dual_solution, found_dual_feasible_solution]),
                   GetConvexDualSolutionIfAvailable(model_parameters));
  // TODO(b/280353996): we want to extract the basis here (when we solve via
  // simplex), but the existing code extracts a basis which fails our validator.

  const SolutionClaims solution_claims = {
      .primal_feasible_solution_exists = found_primal_feasible_solution,
      .dual_feasible_solution_exists = found_dual_feasible_solution};

  if (!primal_solution.has_value()) {
    return GurobiSolver::SolutionsAndClaims{.solution_claims = solution_claims};
  }
  SolutionsAndClaims solution_and_claims{.solution_claims = solution_claims};
  SolutionProto& solution =
      solution_and_claims.solutions.emplace_back(SolutionProto());
  if (primal_solution.has_value()) {
    *solution.mutable_primal_solution() = *std::move(primal_solution);
  }
  if (dual_solution.has_value()) {
    *solution.mutable_dual_solution() = *std::move(dual_solution);
  }
  return solution_and_claims;
}

absl::StatusOr<GurobiSolver::SolutionsAndClaims> GurobiSolver::GetQcpSolution(
    const ModelSolveParametersProto& model_parameters) {
  ASSIGN_OR_RETURN((auto [primal_solution, found_primal_feasible_solution]),
                   GetConvexPrimalSolutionIfAvailable(model_parameters));
  ASSIGN_OR_RETURN((auto [dual_solution, found_dual_feasible_solution]),
                   GetConvexDualSolutionIfAvailable(model_parameters));
  ASSIGN_OR_RETURN(const int grb_termination,
                   gurobi_->GetIntAttr(GRB_INT_ATTR_STATUS));
  // By default, Gurobi will not return duals for optimally solved QCPs.
  const bool proven_feasible = grb_termination == GRB_OPTIMAL;
  const SolutionClaims solution_claims = {
      .primal_feasible_solution_exists = found_primal_feasible_solution,
      .dual_feasible_solution_exists =
          found_dual_feasible_solution || proven_feasible};

  SolutionsAndClaims solution_and_claims{.solution_claims = solution_claims};
  if (primal_solution.has_value() || dual_solution.has_value()) {
    SolutionProto& solution =
        solution_and_claims.solutions.emplace_back(SolutionProto());
    if (primal_solution.has_value()) {
      *solution.mutable_primal_solution() = *std::move(primal_solution);
    }
    if (dual_solution.has_value()) {
      *solution.mutable_dual_solution() = *std::move(dual_solution);
    }
  }
  return solution_and_claims;
}

absl::Status GurobiSolver::SetParameters(
    const SolveParametersProto& parameters,
    const ModelSolveParametersProto& model_parameters) {
  ASSIGN_OR_RETURN(const bool is_mip, IsMIP());
  ASSIGN_OR_RETURN(const GurobiParametersProto gurobi_parameters,
                   MergeParameters(parameters, model_parameters, is_mip,
                                   is_multi_objective_mode()));
  std::vector<std::string> parameter_errors;
  for (const GurobiParametersProto::Parameter& parameter :
       gurobi_parameters.parameters()) {
    absl::Status param_status =
        gurobi_->SetParam(parameter.name().c_str(), parameter.value());
    if (!param_status.ok()) {
      parameter_errors.emplace_back(std::move(param_status).message());
    }
  }
  if (!parameter_errors.empty()) {
    return absl::InvalidArgumentError(absl::StrJoin(parameter_errors, "; "));
  }
  return absl::OkStatus();
}

absl::Status GurobiSolver::AddNewVariables(
    const VariablesProto& new_variables) {
  const int num_new_variables = new_variables.lower_bounds().size();
  std::vector<char> variable_type(num_new_variables);
  for (int j = 0; j < num_new_variables; ++j) {
    const VariableId id = new_variables.ids(j);
    gtl::InsertOrDie(&variables_map_, id, j + num_gurobi_variables_);
    variable_type[j] = new_variables.integers(j) ? GRB_INTEGER : GRB_CONTINUOUS;
  }
  // We need to copy the names, RepeatedPtrField cannot be converted to
  // absl::Span<std::string>.
  const std::vector<std::string> variable_names =
      TruncateNames(new_variables.names());
  RETURN_IF_ERROR(gurobi_->AddVars(
      /*obj=*/{},
      /*lb=*/new_variables.lower_bounds(),
      /*ub=*/new_variables.upper_bounds(),
      /*vtype=*/variable_type, variable_names));
  num_gurobi_variables_ += num_new_variables;

  return absl::OkStatus();
}

absl::Status GurobiSolver::AddSingleObjective(const ObjectiveProto& objective) {
  const int model_sense = objective.maximize() ? GRB_MAXIMIZE : GRB_MINIMIZE;
  RETURN_IF_ERROR(gurobi_->SetIntAttr(GRB_INT_ATTR_MODELSENSE, model_sense));
  RETURN_IF_ERROR(
      gurobi_->SetDoubleAttr(GRB_DBL_ATTR_OBJCON, objective.offset()));
  RETURN_IF_ERROR(UpdateDoubleListAttribute(objective.linear_coefficients(),
                                            GRB_DBL_ATTR_OBJ, variables_map_));
  RETURN_IF_ERROR(
      ResetQuadraticObjectiveTerms(objective.quadratic_coefficients()));
  return absl::OkStatus();
}

absl::Status GurobiSolver::AddMultiObjectives(
    const ObjectiveProto& primary_objective,
    const google::protobuf::Map<int64_t, ObjectiveProto>&
        auxiliary_objectives) {
  absl::flat_hash_set<int64_t> priorities = {primary_objective.priority()};
  for (const auto& [id, objective] : auxiliary_objectives) {
    const int64_t priority = objective.priority();
    if (!priorities.insert(priority).second) {
      return util::InvalidArgumentErrorBuilder()
             << "repeated objective priority: " << priority;
    }
  }
  const bool is_maximize = primary_objective.maximize();
  RETURN_IF_ERROR(gurobi_->SetIntAttr(
      GRB_INT_ATTR_MODELSENSE, is_maximize ? GRB_MAXIMIZE : GRB_MINIMIZE));
  RETURN_IF_ERROR(AddNewMultiObjective(
      primary_objective, /*objective_id=*/std::nullopt, is_maximize));
  for (const int64_t id : SortedMapKeys(auxiliary_objectives)) {
    RETURN_IF_ERROR(
        AddNewMultiObjective(auxiliary_objectives.at(id), id, is_maximize));
  }
  return absl::OkStatus();
}

absl::Status GurobiSolver::AddNewMultiObjective(
    const ObjectiveProto& objective,
    const std::optional<AuxiliaryObjectiveId> objective_id,
    const bool is_maximize) {
  std::vector<GurobiVariableIndex> var_indices;
  var_indices.reserve(objective.linear_coefficients().ids_size());
  for (const int64_t var_id : objective.linear_coefficients().ids()) {
    var_indices.push_back(variables_map_.at(var_id));
  }
  const GurobiMultiObjectiveIndex grb_index =
      static_cast<int>(multi_objectives_map_.size());
  // * MathOpt and Gurobi have different priority orderings (lower and higher
  //   are more important, respectively). Therefore, we negate priorities from
  //   MathOpt (which is OK as they are restricted to be nonnegative in MathOpt,
  //   but not in Gurobi).
  // * Tolerances are set to default values, as of Gurobi v9.5.
  // * Gurobi exposes only a single objective sense for the entire model. We use
  //   the objective weight to handle mixing senses across objectives (weight of
  //   1 if objective sense agrees with model sense, -1 otherwise).
  RETURN_IF_ERROR(gurobi_->SetNthObjective(
      /*index=*/grb_index, /*priority=*/static_cast<int>(-objective.priority()),
      /*weight=*/objective.maximize() == is_maximize ? +1.0 : -1.0,
      /*abs_tol=*/1.0e-6,
      /*rel_tol=*/0.0, /*name=*/objective.name(),
      /*constant=*/objective.offset(), /*lind=*/var_indices,
      /*lval=*/objective.linear_coefficients().values()));
  multi_objectives_map_.insert({objective_id, grb_index});
  return absl::OkStatus();
}

// Given a vector of pairs<LinearConstraintId, LinearConstraintData&> add a
// slack variable for each of the constraints in the underlying `gurobi_` using
// the referenced bounds.
absl::Status GurobiSolver::AddNewSlacks(
    const std::vector<LinearConstraintData*>& new_slacks) {
  // Note that we are really adding the sub-matrix
  //    D * slack
  // to the set of linear constraints, and the D matrix is stored in compressed
  // sparse column (CSC) format. In our particular case, D is a diagonal matrix
  // with -1.0 coefficients for each new slack in the row indicated in the
  // row_indices vector.
  const int num_slacks = new_slacks.size();
  if (num_slacks == 0) {
    return absl::OkStatus();
  }
  // Build the D matrix in CSC format.
  const std::vector<double> column_non_zeros(num_slacks, -1.0);
  std::vector<double> lower_bounds;
  std::vector<double> upper_bounds;
  const std::vector<char> vtypes(num_slacks, GRB_CONTINUOUS);
  std::vector<GurobiLinearConstraintIndex> row_indices;
  std::vector<int> column_non_zero_begin;
  column_non_zero_begin.reserve(num_slacks);
  row_indices.reserve(num_slacks);
  lower_bounds.reserve(num_slacks);
  upper_bounds.reserve(num_slacks);
  for (int k = 0; k < num_slacks; ++k) {
    CHECK_NE(new_slacks[k], nullptr);
    const LinearConstraintData& constraint_data = *new_slacks[k];
    row_indices.push_back(constraint_data.constraint_index);
    lower_bounds.push_back(constraint_data.lower_bound);
    upper_bounds.push_back(constraint_data.upper_bound);
    column_non_zero_begin.push_back(k);
  }
  // Add variables to the underlying model.
  RETURN_IF_ERROR(gurobi_->AddVars(/*vbegin=*/column_non_zero_begin,
                                   /*vind=*/row_indices,
                                   /*vval=*/column_non_zeros, /*obj=*/{},
                                   /*lb=*/lower_bounds, /*ub=*/upper_bounds,
                                   /*vtype=*/vtypes, /*names=*/{}));
  num_gurobi_variables_ += num_slacks;
  return absl::OkStatus();
}

absl::Status GurobiSolver::AddNewLinearConstraints(
    const LinearConstraintsProto& constraints) {
  const int num_new_constraints = constraints.lower_bounds().size();

  // We need to copy the names, RepeatedPtrField cannot be converted to
  // absl::Span<std::string>.
  const std::vector<std::string> constraint_names =
      TruncateNames(constraints.names());
  // Constraints are translated into:
  // 1.  ax <= upper_bound (if lower bound <= -GRB_INFINITY, and upper_bound
  //                        is finite and less than GRB_INFINITY)
  // 2.  ax >= lower_bound (if upper bound >= GRB_INFINITY, and lower_bound is
  //                        finite and greater than -GRB_INFINITY)
  // 3.  ax == xxxxx_bound (if both bounds are finite, equal, and their
  //                        absolute values less than GRB_INFINITY)
  // 4.  ax - slack = 0.0  (otherwise,
  //                        slack bounds == [lower_bound, upper_bound])
  std::vector<double> constraint_rhs;
  std::vector<char> constraint_sense;
  std::vector<LinearConstraintData*> new_slacks;
  constraint_rhs.reserve(num_new_constraints);
  constraint_sense.reserve(num_new_constraints);
  new_slacks.reserve(num_new_constraints);
  for (int i = 0; i < num_new_constraints; ++i) {
    const int64_t id = constraints.ids(i);
    LinearConstraintData& constraint_data =
        gtl::InsertKeyOrDie(&linear_constraints_map_, id);
    const double lb = constraints.lower_bounds(i);
    const double ub = constraints.upper_bounds(i);
    RETURN_IF_ERROR(SafeGurobiDouble(lb))
        << "lower bound for linear constraint " << id << ": "
        << EscapedNameForLogging(
               constraints.names().empty() ? "" : constraints.names(i));
    RETURN_IF_ERROR(SafeGurobiDouble(ub))
        << "upper bound for linear constraint " << id << ": "
        << EscapedNameForLogging(
               constraints.names().empty() ? "" : constraints.names(i));
    constraint_data.lower_bound = lb;
    constraint_data.upper_bound = ub;
    constraint_data.constraint_index = i + num_gurobi_lin_cons_;
    char sense = GRB_EQUAL;
    double rhs = 0.0;
    const bool lb_is_grb_neg_inf = lb <= -GRB_INFINITY;
    const bool ub_is_grb_pos_inf = ub >= GRB_INFINITY;
    if (lb_is_grb_neg_inf && !ub_is_grb_pos_inf) {
      sense = GRB_LESS_EQUAL;
      rhs = ub;
    } else if (!lb_is_grb_neg_inf && ub_is_grb_pos_inf) {
      sense = GRB_GREATER_EQUAL;
      rhs = lb;
    } else if (lb == ub) {
      sense = GRB_EQUAL;
      rhs = lb;
    } else {
      // Note that constraints where the lower bound and the upper bound are
      // -+infinity translate into a range constraint with an unbounded slack.
      constraint_data.slack_index = new_slacks.size() + num_gurobi_variables_;
      new_slacks.push_back(&constraint_data);
    }
    constraint_rhs.emplace_back(rhs);
    constraint_sense.emplace_back(sense);
  }
  // Add all constraints in one call.
  RETURN_IF_ERROR(
      gurobi_->AddConstrs(constraint_sense, constraint_rhs, constraint_names));
  num_gurobi_lin_cons_ += num_new_constraints;
  // Add slacks for true ranged constraints (if needed)
  if (!new_slacks.empty()) {
    RETURN_IF_ERROR(AddNewSlacks(new_slacks));
  }
  return absl::OkStatus();
}

absl::Status GurobiSolver::AddNewQuadraticConstraints(
    const google::protobuf::Map<QuadraticConstraintId,
                                QuadraticConstraintProto>& constraints) {
  // Constraints are translated into:
  // 1.  ax <= upper_bound (if lower bound <= -GRB_INFINITY, and upper_bound
  //                        is finite and less than GRB_INFINITY)
  // 2.  ax >= lower_bound (if upper bound >= GRB_INFINITY, and lower_bound is
  //                        finite and greater than -GRB_INFINITY)
  // 3.  ax == xxxxx_bound (if both bounds are finite, equal, and their
  //                        absolute values less than GRB_INFINITY)
  // 4.  Return an error otherwise, we do not currently support ranged quadratic
  //     constraints.
  for (const auto& [id, constraint] : constraints) {
    char sense = GRB_EQUAL;
    double rhs = 0.0;
    const double lb = constraint.lower_bound();
    const double ub = constraint.upper_bound();
    RETURN_IF_ERROR(SafeGurobiDouble(lb))
        << "lower bound for quadratic constraint " << id << ": "
        << EscapedNameForLogging(constraint.name());
    RETURN_IF_ERROR(SafeGurobiDouble(ub))
        << "upper bound for quadratic constraint " << id << ": "
        << EscapedNameForLogging(constraint.name());
    const bool lb_is_grb_neg_inf = lb <= -GRB_INFINITY;
    const bool ub_is_grb_pos_inf = ub >= GRB_INFINITY;
    if (lb_is_grb_neg_inf && ub_is_grb_pos_inf) {
      // The constraint is vacuous, so we just skip it.
      // TODO(b/227217735): Ensure duals properly account for this constraint.
      continue;
    } else if (lb_is_grb_neg_inf && !ub_is_grb_pos_inf) {
      sense = GRB_LESS_EQUAL;
      rhs = ub;
    } else if (!lb_is_grb_neg_inf && ub_is_grb_pos_inf) {
      sense = GRB_GREATER_EQUAL;
      rhs = lb;
    } else if (lb == ub) {
      sense = GRB_EQUAL;
      rhs = lb;
    } else {
      // We do not currently support ranged quadratic constraints, though it is
      // possible to support this if there is a need.
      return absl::UnimplementedError(
          "ranged quadratic constraints are not currently supported in Gurobi "
          "interface");
    }
    const SparseDoubleVectorProto& linear_coeffs = constraint.linear_terms();
    const int num_linear_coeffs = linear_coeffs.ids_size();
    std::vector<GurobiVariableIndex> linear_col_index(num_linear_coeffs);
    for (int k = 0; k < num_linear_coeffs; ++k) {
      linear_col_index[k] = variables_map_.at(linear_coeffs.ids(k));
    }
    const SparseDoubleMatrixProto& quad_coeffs = constraint.quadratic_terms();
    const int num_quad_coeffs = quad_coeffs.row_ids_size();
    std::vector<GurobiVariableIndex> quad_row_index(num_quad_coeffs);
    std::vector<GurobiVariableIndex> quad_col_index(num_quad_coeffs);
    for (int k = 0; k < num_quad_coeffs; ++k) {
      quad_row_index[k] = variables_map_.at(quad_coeffs.row_ids(k));
      quad_col_index[k] = variables_map_.at(quad_coeffs.column_ids(k));
    }
    RETURN_IF_ERROR(gurobi_->AddQConstr(
        linear_col_index, linear_coeffs.values(), quad_row_index,
        quad_col_index, quad_coeffs.coefficients(), sense, rhs,
        TruncateName(constraint.name())));
    gtl::InsertOrDie(&quadratic_constraints_map_, id, num_gurobi_quad_cons_);
    ++num_gurobi_quad_cons_;
  }
  return absl::OkStatus();
}

std::optional<GurobiSolver::VariableId> GurobiSolver::TryExtractVariable(
    const LinearExpressionProto& expression) {
  if (expression.offset() == 0 && expression.ids_size() == 1 &&
      expression.coefficients(0) == 1) {
    return expression.ids(0);
  }
  return std::nullopt;
}

absl::StatusOr<GurobiSolver::VariableEqualToExpression>
GurobiSolver::CreateSlackVariableEqualToExpression(
    const LinearExpressionProto& expression) {
  const GurobiVariableIndex slack_variable = num_gurobi_variables_;
  std::vector<GurobiVariableIndex> slack_col_indices = {slack_variable};
  std::vector<double> slack_coeffs = {-1.0};
  for (int j = 0; j < expression.ids_size(); ++j) {
    slack_col_indices.push_back(variables_map_.at(expression.ids(j)));
    slack_coeffs.push_back(expression.coefficients(j));
  }
  RETURN_IF_ERROR(gurobi_->AddVar(0, -kInf, kInf, GRB_CONTINUOUS, ""));
  RETURN_IF_ERROR(gurobi_->AddConstr(slack_col_indices, slack_coeffs, GRB_EQUAL,
                                     -expression.offset(), ""));
  return VariableEqualToExpression{.variable_index = num_gurobi_variables_++,
                                   .constraint_index = num_gurobi_lin_cons_++};
}

absl::Status GurobiSolver::AddNewSecondOrderConeConstraints(
    const google::protobuf::Map<SecondOrderConeConstraintId,
                                SecondOrderConeConstraintProto>& constraints) {
  for (const auto& [id, constraint] : constraints) {
    // The MathOpt proto representation for a second-order cone constraint is:
    //       ||`constraint`.arguments_to_norm||_2 <= `constraint`.upper_bound.
    // Gurobi requires second-order cone constraints to be passed via the
    // quadratic constraint API as:
    //       arg_var[0]^2 + ... + arg_var[d]^2 <= ub_var^2
    //       ub_var >= 0,
    // for variables arg_var[0], ..., arg_var[d], ub_var. To get to this form,
    // we add slack variables:
    //       ub_var     = `constraint`.upper_bound
    //       arg_var[i] = `constraint`.arguments_to_norm[i] for each i
    // Note that we elide adding a slack variable/constraint if the expression
    // we are setting it equal to is just a variable already in the model.
    SecondOrderConeConstraintData& constraint_data =
        gtl::InsertKeyOrDie(&soc_constraints_map_, id);
    constraint_data.constraint_index = num_gurobi_quad_cons_;
    // We force a new variable to be added so that we can add a lower bound on
    // it. Otherwise, we must update the model to flush bounds, or risk either
    // a Gurobi error, or stomping on a potentially stronger bound.
    ASSIGN_OR_RETURN(
        (const auto [ub_var, ub_cons]),
        CreateSlackVariableEqualToExpression(constraint.upper_bound()));
    RETURN_IF_ERROR(
        gurobi_->SetDoubleAttrElement(GRB_DBL_ATTR_LB, ub_var, 0.0));
    constraint_data.slack_variables.push_back(ub_var);
    constraint_data.slack_constraints.push_back(ub_cons);
    std::vector<GurobiVariableIndex> quad_var_indices = {ub_var};
    std::vector<double> quad_coeffs = {-1.0};
    for (const LinearExpressionProto& expression :
         constraint.arguments_to_norm()) {
      quad_coeffs.push_back(1.0);
      if (const std::optional<VariableId> maybe_variable =
              TryExtractVariable(expression);
          maybe_variable.has_value()) {
        quad_var_indices.push_back(variables_map_.at(*maybe_variable));
        continue;
      }
      ASSIGN_OR_RETURN((const auto [arg_var, arg_cons]),
                       CreateSlackVariableEqualToExpression(expression));
      quad_var_indices.push_back(arg_var);
      constraint_data.slack_variables.push_back(arg_var);
      constraint_data.slack_constraints.push_back(arg_cons);
    }
    RETURN_IF_ERROR(gurobi_->AddQConstr({}, {}, quad_var_indices,
                                        quad_var_indices, quad_coeffs,
                                        GRB_LESS_EQUAL, 0.0, ""));
    ++num_gurobi_quad_cons_;
  }
  return absl::OkStatus();
}

absl::Status GurobiSolver::AddNewSosConstraints(
    const google::protobuf::Map<AnyConstraintId, SosConstraintProto>&
        constraints,
    const int sos_type,
    absl::flat_hash_map<int64_t, SosConstraintData>& constraints_map) {
  for (const auto& [id, constraint] : constraints) {
    SosConstraintData& constraint_data =
        gtl::InsertKeyOrDie(&constraints_map, id);
    constraint_data.constraint_index = num_gurobi_sos_cons_;
    std::vector<GurobiVariableIndex> sos_var_indices;
    std::vector<double> weights;
    // As of v9.0.2, Gurobi does not allow SOS constraints to contain repeated
    // variables. So, we track the variables we "reuse" (i.e., were already
    // present in the model). Slack variables introduced in
    // `ExtractVariableEqualToExpression()` will not be present in the proto
    // inputs, so we can safely ignore tracking them.
    absl::flat_hash_set<VariableId> reused_variables;
    for (int i = 0; i < constraint.expressions_size(); ++i) {
      weights.push_back(constraint.weights().empty() ? i + 1
                                                     : constraint.weights(i));
      if (const std::optional<VariableId> maybe_variable =
              TryExtractVariable(constraint.expressions(i));
          maybe_variable.has_value() &&
          !reused_variables.contains(*maybe_variable)) {
        sos_var_indices.push_back(variables_map_.at(*maybe_variable));
        reused_variables.insert(*maybe_variable);
        if (sos_type == 2) {
          // If this variable is deleted, Gurobi will drop the corresponding
          // term from the SOS constraint, potentially changing the meaning of
          // the constraint.
          undeletable_variables_.insert(*maybe_variable);
        }
        continue;
      }
      ASSIGN_OR_RETURN(
          (const auto [var_index, cons_index]),
          CreateSlackVariableEqualToExpression(constraint.expressions(i)));
      sos_var_indices.push_back(var_index);
      constraint_data.slack_variables.push_back(var_index);
      constraint_data.slack_constraints.push_back(cons_index);
    }
    RETURN_IF_ERROR(gurobi_->AddSos({sos_type}, {0}, sos_var_indices, weights));
    ++num_gurobi_sos_cons_;
  }
  return absl::OkStatus();
}

absl::Status GurobiSolver::AddNewIndicatorConstraints(
    const google::protobuf::Map<IndicatorConstraintId,
                                IndicatorConstraintProto>& constraints) {
  for (const auto& [id, constraint] : constraints) {
    if (!constraint.has_indicator_id()) {
      if (constraint.activate_on_zero()) {
        return util::UnimplementedErrorBuilder()
               << "MathOpt does not currently support Gurobi models with "
                  "indicator constraints that activate on zero with unset "
                  "indicator variables; encountered one at id: "
               << id;
      }
      gtl::InsertOrDie(&indicator_constraints_map_, id, std::nullopt);
      continue;
    }
    const int num_terms = constraint.expression().ids_size();
    std::vector<GurobiVariableIndex> grb_ids(num_terms);
    for (int k = 0; k < num_terms; ++k) {
      grb_ids[k] = variables_map_.at(constraint.expression().ids(k));
    }
    char sense = GRB_EQUAL;
    double rhs = 0.0;
    const double lb = constraint.lower_bound();
    const double ub = constraint.upper_bound();
    RETURN_IF_ERROR(SafeGurobiDouble(lb))
        << "lower bound for indicator constraint " << id << ": "
        << EscapedNameForLogging(constraint.name());
    RETURN_IF_ERROR(SafeGurobiDouble(ub))
        << "upper bound for indicator constraint " << id << ": "
        << EscapedNameForLogging(constraint.name());
    const bool lb_is_grb_neg_inf = lb <= -GRB_INFINITY;
    const bool ub_is_grb_pos_inf = ub >= GRB_INFINITY;
    if (lb_is_grb_neg_inf && ub_is_grb_pos_inf) {
      // The constraint is vacuous, so we just skip it.
      continue;
    } else if (lb_is_grb_neg_inf && !ub_is_grb_pos_inf) {
      sense = GRB_LESS_EQUAL;
      rhs = ub;
    } else if (!lb_is_grb_neg_inf && ub_is_grb_pos_inf) {
      sense = GRB_GREATER_EQUAL;
      rhs = lb;
    } else if (lb == ub) {
      sense = GRB_EQUAL;
      rhs = lb;
    } else {
      // We do not currently support ranged indicator constraints, though it
      // is possible to support this if there is a need.
      return absl::UnimplementedError(
          "ranged indicator constraints are not currently supported in "
          "Gurobi "
          "interface");
    }
    RETURN_IF_ERROR(gurobi_->AddIndicator(
        /*name=*/constraint.name(),
        /*binvar=*/variables_map_.at(constraint.indicator_id()),
        /*binval=*/constraint.activate_on_zero() ? 0 : 1,
        /*ind=*/grb_ids, /*val=*/constraint.expression().values(),
        /*sense=*/sense, /*rhs=*/rhs));
    gtl::InsertOrDie(&indicator_constraints_map_, id,
                     IndicatorConstraintData{
                         .constraint_index = num_gurobi_gen_cons_,
                         .indicator_variable_id = constraint.indicator_id()});
    ++num_gurobi_gen_cons_;
    // Deleting the indicator variable, but not the associated indicator
    // constraint, will lead to a Gurobi error.
    undeletable_variables_.insert(constraint.indicator_id());
  }
  return absl::OkStatus();
}

absl::Status GurobiSolver::ChangeCoefficients(
    const SparseDoubleMatrixProto& matrix) {
  const int num_coefficients = matrix.row_ids().size();
  std::vector<GurobiLinearConstraintIndex> row_index(num_coefficients);
  std::vector<GurobiVariableIndex> col_index(num_coefficients);
  for (int k = 0; k < num_coefficients; ++k) {
    row_index[k] =
        linear_constraints_map_.at(matrix.row_ids(k)).constraint_index;
    col_index[k] = variables_map_.at(matrix.column_ids(k));
  }
  return gurobi_->ChgCoeffs(row_index, col_index, matrix.coefficients());
}

absl::Status GurobiSolver::UpdateDoubleListAttribute(
    const SparseDoubleVectorProto& update,
    const char* absl_nonnull attribute_name, const IdHashMap& id_hash_map) {
  if (update.ids_size() == 0) {
    return absl::OkStatus();
  }
  std::vector<int> index;
  index.reserve(update.ids_size());
  for (const int64_t id : update.ids()) {
    index.push_back(id_hash_map.at(id));
  }
  return gurobi_->SetDoubleAttrList(attribute_name, index, update.values());
}

absl::Status GurobiSolver::UpdateInt32ListAttribute(
    const SparseInt32VectorProto& update,
    const char* absl_nonnull attribute_name, const IdHashMap& id_hash_map) {
  if (update.ids_size() == 0) {
    return absl::OkStatus();
  }
  std::vector<int> index;
  index.reserve(update.ids_size());
  for (const int64_t id : update.ids()) {
    index.push_back(id_hash_map.at(id));
  }
  return gurobi_->SetIntAttrList(attribute_name, index, update.values());
}

absl::Status GurobiSolver::LoadModel(const ModelProto& input_model) {
  CHECK(gurobi_ != nullptr);
  RETURN_IF_ERROR(gurobi_->SetStringAttr(GRB_STR_ATTR_MODELNAME,
                                         TruncateName(input_model.name())));
  RETURN_IF_ERROR(AddNewVariables(input_model.variables()));

  RETURN_IF_ERROR(AddNewLinearConstraints(input_model.linear_constraints()));
  RETURN_IF_ERROR(
      AddNewQuadraticConstraints(input_model.quadratic_constraints()));
  RETURN_IF_ERROR(AddNewSecondOrderConeConstraints(
      input_model.second_order_cone_constraints()));
  RETURN_IF_ERROR(AddNewSosConstraints(input_model.sos1_constraints(),
                                       GRB_SOS_TYPE1, sos1_constraints_map_));
  RETURN_IF_ERROR(AddNewSosConstraints(input_model.sos2_constraints(),
                                       GRB_SOS_TYPE2, sos2_constraints_map_));
  RETURN_IF_ERROR(
      AddNewIndicatorConstraints(input_model.indicator_constraints()));

  RETURN_IF_ERROR(ChangeCoefficients(input_model.linear_constraint_matrix()));

  if (input_model.auxiliary_objectives().empty()) {
    RETURN_IF_ERROR(AddSingleObjective(input_model.objective()));
  } else {
    RETURN_IF_ERROR(AddMultiObjectives(input_model.objective(),
                                       input_model.auxiliary_objectives()));
  }
  return absl::OkStatus();
}

absl::Status GurobiSolver::ResetQuadraticObjectiveTerms(
    const SparseDoubleMatrixProto& terms) {
  quadratic_objective_coefficients_.clear();
  RETURN_IF_ERROR(gurobi_->DelQ());
  const int num_terms = terms.row_ids().size();
  if (num_terms > 0) {
    std::vector<GurobiVariableIndex> first_var_index(num_terms);
    std::vector<GurobiVariableIndex> second_var_index(num_terms);
    for (int k = 0; k < num_terms; ++k) {
      const VariableId row_id = terms.row_ids(k);
      const VariableId column_id = terms.column_ids(k);
      first_var_index[k] = variables_map_.at(row_id);
      second_var_index[k] = variables_map_.at(column_id);
      quadratic_objective_coefficients_[{row_id, column_id}] =
          terms.coefficients(k);
    }
    RETURN_IF_ERROR(gurobi_->AddQpTerms(first_var_index, second_var_index,
                                        terms.coefficients()));
  }
  return absl::OkStatus();
}

absl::Status GurobiSolver::UpdateQuadraticObjectiveTerms(
    const SparseDoubleMatrixProto& terms) {
  CHECK(gurobi_ != nullptr);
  const int num_terms = terms.row_ids().size();
  if (num_terms > 0) {
    std::vector<GurobiVariableIndex> first_var_index(num_terms);
    std::vector<GurobiVariableIndex> second_var_index(num_terms);
    std::vector<double> coefficient_updates(num_terms);
    for (int k = 0; k < num_terms; ++k) {
      const VariableId row_id = terms.row_ids(k);
      const VariableId column_id = terms.column_ids(k);
      first_var_index[k] = variables_map_.at(row_id);
      second_var_index[k] = variables_map_.at(column_id);
      const std::pair<VariableId, VariableId> qp_term_key(row_id, column_id);
      const double new_coefficient = terms.coefficients(k);
      // Gurobi will maintain any existing quadratic coefficients unless we
      // call GRBdelq (which we don't). So, since stored entries in terms
      // specify the target coefficients, we need to compute the difference
      // from the existing coefficient with Gurobi, if any.
      coefficient_updates[k] =
          new_coefficient - quadratic_objective_coefficients_[qp_term_key];
      quadratic_objective_coefficients_[qp_term_key] = new_coefficient;
    }
    RETURN_IF_ERROR(gurobi_->AddQpTerms(first_var_index, second_var_index,
                                        coefficient_updates));
  }
  return absl::OkStatus();
}

// Bound changes in constraints can induce new variables, and also remove
// some slacks. We first add all new variables, and queue all deletions to be
// dealt with later on.
absl::Status GurobiSolver::UpdateLinearConstraints(
    const LinearConstraintUpdatesProto& constraints_update,
    std::vector<GurobiVariableIndex>& deleted_variables_index) {
  const SparseDoubleVectorProto& constraint_lower_bounds =
      constraints_update.lower_bounds();
  const SparseDoubleVectorProto& constraint_upper_bounds =
      constraints_update.upper_bounds();

  // If no update, just return.
  if (constraint_lower_bounds.ids().empty() &&
      constraint_upper_bounds.ids().empty()) {
    return absl::OkStatus();
  }

  // We want to avoid changing the right-hand-side, sense, or slacks of each
  // constraint more than once. Since we can refer to the same constraint ID
  // both in the `constraint_upper_bounds` and `constraint_lower_bounds`
  // sparse vectors, we collect all changes into a single structure:
  struct UpdateConstraintData {
    LinearConstraintId constraint_id;
    LinearConstraintData& source;
    double new_lower_bound;
    double new_upper_bound;
    UpdateConstraintData(const LinearConstraintId id,
                         LinearConstraintData& reference)
        : constraint_id(id),
          source(reference),
          new_lower_bound(reference.lower_bound),
          new_upper_bound(reference.upper_bound) {}
  };
  const int upper_bounds_size = constraint_upper_bounds.ids().size();
  const int lower_bounds_size = constraint_lower_bounds.ids().size();
  std::vector<UpdateConstraintData> update_vector;
  update_vector.reserve(upper_bounds_size + lower_bounds_size);
  // We exploit the fact that IDs are sorted in increasing order to merge
  // changes into a vector of aggregated changes.
  for (int lower_index = 0, upper_index = 0;
       lower_index < lower_bounds_size || upper_index < upper_bounds_size;) {
    VariableId lower_id = std::numeric_limits<int64_t>::max();
    if (lower_index < lower_bounds_size) {
      lower_id = constraint_lower_bounds.ids(lower_index);
    }
    VariableId upper_id = std::numeric_limits<int64_t>::max();
    if (upper_index < upper_bounds_size) {
      upper_id = constraint_upper_bounds.ids(upper_index);
    }
    const VariableId id = std::min(lower_id, upper_id);
    DCHECK(id < std::numeric_limits<int64_t>::max());
    UpdateConstraintData update(id, linear_constraints_map_.at(id));
    if (lower_id == upper_id) {
      update.new_lower_bound = constraint_lower_bounds.values(lower_index++);
      update.new_upper_bound = constraint_upper_bounds.values(upper_index++);
    } else if (lower_id < upper_id) {
      update.new_lower_bound = constraint_lower_bounds.values(lower_index++);
    } else { /* upper_id < lower_id */
      update.new_upper_bound = constraint_upper_bounds.values(upper_index++);
    }
    update_vector.emplace_back(update);
  }

  // We have grouped all changes in update_vector, now generate changes in
  // slack bounds, rhs, senses, new slacks, and deleted_slacks (to be dealt
  // with later, outside this function).
  // These three vectors keep changes to right-hand-side and senses.
  std::vector<char> sense_data;
  std::vector<double> rhs_data;
  std::vector<GurobiLinearConstraintIndex> rhs_index;
  // These three vectors keep changes to bounds on existing slack.
  std::vector<double> lower_bound_data;
  std::vector<double> upper_bound_data;
  std::vector<GurobiVariableIndex> bound_index;
  // This vector keep newly introduced slacks.
  std::vector<LinearConstraintData*> new_slacks;
  // Iterate on the changes, and populate the three possible changes.
  for (UpdateConstraintData& update_data : update_vector) {
    const bool same_lower_bound =
        (update_data.source.lower_bound == update_data.new_lower_bound) ||
        ((update_data.source.lower_bound <= -GRB_INFINITY) &&
         (update_data.new_lower_bound <= -GRB_INFINITY));
    const bool same_upper_bound =
        (update_data.source.upper_bound == update_data.new_upper_bound) ||
        ((update_data.source.upper_bound >= GRB_INFINITY) &&
         (update_data.new_upper_bound >= GRB_INFINITY));
    if (same_upper_bound && same_lower_bound) continue;
    // Save into linear_constraints_map_[id] the new bounds for the linear
    // constraint.
    update_data.source.lower_bound = update_data.new_lower_bound;
    update_data.source.upper_bound = update_data.new_upper_bound;
    bool delete_slack = false;
    // Detect the type of constraint to add and store RHS and bounds.
    if (update_data.new_lower_bound <= -GRB_INFINITY &&
        update_data.new_upper_bound < GRB_INFINITY) {
      delete_slack = true;
      rhs_index.emplace_back(update_data.source.constraint_index);
      rhs_data.emplace_back(update_data.new_upper_bound);
      sense_data.emplace_back(GRB_LESS_EQUAL);
    } else if (update_data.new_lower_bound > -GRB_INFINITY &&
               update_data.new_upper_bound >= GRB_INFINITY) {
      delete_slack = true;
      rhs_index.emplace_back(update_data.source.constraint_index);
      rhs_data.emplace_back(update_data.new_lower_bound);
      sense_data.emplace_back(GRB_GREATER_EQUAL);
    } else if (update_data.new_lower_bound == update_data.new_upper_bound) {
      delete_slack = true;
      rhs_index.emplace_back(update_data.source.constraint_index);
      rhs_data.emplace_back(update_data.new_lower_bound);
      sense_data.emplace_back(GRB_EQUAL);
    } else {
      // Note that constraints where the lower bound and the upper bound are
      // -+infinity translated into a range constraint with an unbounded
      // slack.
      if (update_data.source.slack_index != kUnspecifiedIndex) {
        bound_index.emplace_back(update_data.source.slack_index);
        lower_bound_data.emplace_back(update_data.new_lower_bound);
        upper_bound_data.emplace_back(update_data.new_upper_bound);
      } else {
        // Note that if we add a new slack, we must both reset the sense and
        // right hand side for the inequality.
        rhs_index.emplace_back(update_data.source.constraint_index);
        rhs_data.emplace_back(0.0);
        sense_data.emplace_back(GRB_EQUAL);
        // Update the slack_index in the linear_constraints_map_[id]
        update_data.source.slack_index =
            new_slacks.size() + num_gurobi_variables_;
        // Save the data needed to add the new slack.
        new_slacks.push_back(&update_data.source);
      }
    }
    // If the constraint had a slack, and now is marked for deletion, we reset
    // the stored slack_index in linear_constraints_map_[id], save the index
    // in the list of variables to be deleted later on and remove the
    // constraint from slack_map_.
    if (delete_slack && update_data.source.slack_index != kUnspecifiedIndex) {
      deleted_variables_index.emplace_back(update_data.source.slack_index);
      update_data.source.slack_index = kUnspecifiedIndex;
    }
  }

  // Pass down changes to Gurobi.
  if (!rhs_index.empty()) {
    RETURN_IF_ERROR(
        gurobi_->SetDoubleAttrList(GRB_DBL_ATTR_RHS, rhs_index, rhs_data));
    RETURN_IF_ERROR(
        gurobi_->SetCharAttrList(GRB_CHAR_ATTR_SENSE, rhs_index, sense_data));
  }  // rhs changes
  if (!bound_index.empty()) {
    RETURN_IF_ERROR(gurobi_->SetDoubleAttrList(GRB_DBL_ATTR_LB, bound_index,
                                               lower_bound_data));
    RETURN_IF_ERROR(gurobi_->SetDoubleAttrList(GRB_DBL_ATTR_UB, bound_index,
                                               upper_bound_data));
  }  // Slack bound changes.

  if (!new_slacks.empty()) {
    RETURN_IF_ERROR(AddNewSlacks(new_slacks));
  }
  return absl::OkStatus();
}

// This function re-assign indices for variables and constraints after
// deletion. The updated indices are computed from the previous indices,
// sorted in incremental form, but re-assigned so that all indices are
// contiguous between [0, num_variables-1], [0, num_linear_constraints-1], and
// [0, num_quad_constraints-1], etc.
void GurobiSolver::UpdateGurobiIndices(const DeletedIndices& deleted_indices) {
  // Recover the updated indices of variables.
  if (!deleted_indices.variables.empty()) {
    const std::vector<GurobiVariableIndex> old_to_new =
        IndexUpdateMap(num_gurobi_variables_, deleted_indices.variables);
    for (auto& [_, grb_index] : variables_map_) {
      grb_index = old_to_new[grb_index];
      CHECK_NE(grb_index, kDeletedIndex);
    }
    for (auto& [_, lin_con_data] : linear_constraints_map_) {
      if (lin_con_data.slack_index != kUnspecifiedIndex) {
        lin_con_data.slack_index = old_to_new[lin_con_data.slack_index];
        CHECK_NE(lin_con_data.slack_index, kDeletedIndex);
      }
    }
    for (auto& [_, soc_con_data] : soc_constraints_map_) {
      for (GurobiVariableIndex& index : soc_con_data.slack_variables) {
        index = old_to_new[index];
        CHECK_NE(index, kDeletedIndex);
      }
    }
    for (auto& [_, sos1_con_data] : sos1_constraints_map_) {
      for (GurobiVariableIndex& index : sos1_con_data.slack_variables) {
        index = old_to_new[index];
        CHECK_NE(index, kDeletedIndex);
      }
    }
    for (auto& [_, sos2_con_data] : sos2_constraints_map_) {
      for (GurobiVariableIndex& index : sos2_con_data.slack_variables) {
        index = old_to_new[index];
        CHECK_NE(index, kDeletedIndex);
      }
    }
  }
  // Recover the updated indices of linear constraints.
  if (!deleted_indices.linear_constraints.empty()) {
    const std::vector<GurobiLinearConstraintIndex> old_to_new = IndexUpdateMap(
        num_gurobi_lin_cons_, deleted_indices.linear_constraints);
    for (auto& [_, lin_con_data] : linear_constraints_map_) {
      lin_con_data.constraint_index = old_to_new[lin_con_data.constraint_index];
      CHECK_NE(lin_con_data.constraint_index, kDeletedIndex);
    }
    for (auto& [_, soc_con_data] : soc_constraints_map_) {
      for (GurobiLinearConstraintIndex& index :
           soc_con_data.slack_constraints) {
        index = old_to_new[index];
        CHECK_NE(index, kDeletedIndex);
      }
    }
    for (auto& [_, sos1_con_data] : sos1_constraints_map_) {
      for (GurobiLinearConstraintIndex& index :
           sos1_con_data.slack_constraints) {
        index = old_to_new[index];
        CHECK_NE(index, kDeletedIndex);
      }
    }
    for (auto& [_, sos2_con_data] : sos2_constraints_map_) {
      for (GurobiLinearConstraintIndex& index :
           sos2_con_data.slack_constraints) {
        index = old_to_new[index];
        CHECK_NE(index, kDeletedIndex);
      }
    }
  }
  // Recover the updated indices of quadratic constraints.
  if (!deleted_indices.quadratic_constraints.empty()) {
    const std::vector<GurobiQuadraticConstraintIndex> old_to_new =
        IndexUpdateMap(num_gurobi_quad_cons_,
                       deleted_indices.quadratic_constraints);
    for (auto& [_, grb_index] : quadratic_constraints_map_) {
      grb_index = old_to_new[grb_index];
      CHECK_NE(grb_index, kDeletedIndex);
    }
    for (auto& [_, soc_con_data] : soc_constraints_map_) {
      GurobiQuadraticConstraintIndex& grb_index = soc_con_data.constraint_index;
      grb_index = old_to_new[soc_con_data.constraint_index];
      CHECK_NE(grb_index, kDeletedIndex);
    }
  }
  // Recover the updated indices of SOS constraints.
  if (!deleted_indices.sos_constraints.empty()) {
    const std::vector<GurobiSosConstraintIndex> old_to_new =
        IndexUpdateMap(num_gurobi_sos_cons_, deleted_indices.sos_constraints);
    for (auto& [_, sos1_data] : sos1_constraints_map_) {
      GurobiSosConstraintIndex& grb_index = sos1_data.constraint_index;
      grb_index = old_to_new[grb_index];
      CHECK_NE(grb_index, kDeletedIndex);
    }
    for (auto& [_, sos2_data] : sos2_constraints_map_) {
      GurobiSosConstraintIndex& grb_index = sos2_data.constraint_index;
      grb_index = old_to_new[grb_index];
      CHECK_NE(grb_index, kDeletedIndex);
    }
  }
  // Recover the updated indices of general constraints.
  if (!deleted_indices.general_constraints.empty()) {
    const std::vector<GurobiGeneralConstraintIndex> old_to_new = IndexUpdateMap(
        num_gurobi_gen_cons_, deleted_indices.general_constraints);
    for (auto& [_, indicator_data] : indicator_constraints_map_) {
      if (!indicator_data.has_value()) {
        continue;
      }
      GurobiGeneralConstraintIndex& grb_index =
          indicator_data->constraint_index;
      grb_index = old_to_new[grb_index];
      CHECK_NE(grb_index, kDeletedIndex);
    }
  }
}

absl::StatusOr<bool> GurobiSolver::Update(
    const ModelUpdateProto& model_update) {
  if (!undeletable_variables_.empty()) {
    for (const VariableId id : model_update.deleted_variable_ids()) {
      if (undeletable_variables_.contains(id)) {
        return false;
      }
    }
  }
  if (!UpdateIsSupported(model_update, kGurobiSupportedStructures)) {
    return false;
  }
  // As of 2022-12-06 we do not support incrementalism for multi-objective
  // models: not adding/deleting/modifying the auxiliary objectives...
  if (const AuxiliaryObjectivesUpdatesProto& objs_update =
          model_update.auxiliary_objectives_updates();
      !objs_update.deleted_objective_ids().empty() ||
      !objs_update.new_objectives().empty() ||
      !objs_update.objective_updates().empty()) {
    return false;
  }
  // ...or modifying the primary objective of a multi-objective model.
  if (is_multi_objective_mode() && model_update.has_objective_updates()) {
    return false;
  }

  RETURN_IF_ERROR(AddNewVariables(model_update.new_variables()));

  RETURN_IF_ERROR(
      AddNewLinearConstraints(model_update.new_linear_constraints()));

  RETURN_IF_ERROR(AddNewQuadraticConstraints(
      model_update.quadratic_constraint_updates().new_constraints()));
  RETURN_IF_ERROR(AddNewSecondOrderConeConstraints(
      model_update.second_order_cone_constraint_updates().new_constraints()));
  RETURN_IF_ERROR(AddNewSosConstraints(
      model_update.sos1_constraint_updates().new_constraints(), GRB_SOS_TYPE1,
      sos1_constraints_map_));
  RETURN_IF_ERROR(AddNewSosConstraints(
      model_update.sos2_constraint_updates().new_constraints(), GRB_SOS_TYPE2,
      sos2_constraints_map_));
  RETURN_IF_ERROR(AddNewIndicatorConstraints(
      model_update.indicator_constraint_updates().new_constraints()));

  RETURN_IF_ERROR(
      ChangeCoefficients(model_update.linear_constraint_matrix_updates()));

  if (model_update.objective_updates().has_direction_update()) {
    const int model_sense = model_update.objective_updates().direction_update()
                                ? GRB_MAXIMIZE
                                : GRB_MINIMIZE;
    RETURN_IF_ERROR(gurobi_->SetIntAttr(GRB_INT_ATTR_MODELSENSE, model_sense));
  }

  if (model_update.objective_updates().has_offset_update()) {
    RETURN_IF_ERROR(gurobi_->SetDoubleAttr(
        GRB_DBL_ATTR_OBJCON, model_update.objective_updates().offset_update()));
  }

  RETURN_IF_ERROR(UpdateDoubleListAttribute(
      model_update.objective_updates().linear_coefficients(), GRB_DBL_ATTR_OBJ,
      variables_map_));

  RETURN_IF_ERROR(UpdateQuadraticObjectiveTerms(
      model_update.objective_updates().quadratic_coefficients()));

  RETURN_IF_ERROR(
      UpdateDoubleListAttribute(model_update.variable_updates().lower_bounds(),
                                GRB_DBL_ATTR_LB, variables_map_));

  RETURN_IF_ERROR(
      UpdateDoubleListAttribute(model_update.variable_updates().upper_bounds(),
                                GRB_DBL_ATTR_UB, variables_map_));

  if (model_update.variable_updates().has_integers()) {
    const SparseBoolVectorProto& update =
        model_update.variable_updates().integers();
    std::vector<GurobiVariableIndex> index;
    index.reserve(update.ids_size());
    for (const int64_t id : update.ids()) {
      index.push_back(variables_map_.at(id));
    }
    std::vector<char> value;
    value.reserve(update.values_size());
    for (const bool val : update.values()) {
      value.push_back(val ? GRB_INTEGER : GRB_CONTINUOUS);
    }
    RETURN_IF_ERROR(
        gurobi_->SetCharAttrList(GRB_CHAR_ATTR_VTYPE, index, value));
  }

  // Now we update quadratic_objective_coefficients_, removing any terms where
  // either one or both of the involved variables are about to be deleted.
  const absl::flat_hash_set<VariableId> variable_ids_to_be_deleted(
      model_update.deleted_variable_ids().begin(),
      model_update.deleted_variable_ids().end());
  // NOTE: Introducing more state and complexity should speed this up, but we
  // opt for the simpler approach for now.
  for (auto it = quadratic_objective_coefficients_.cbegin();
       it != quadratic_objective_coefficients_.cend();
       /*incremented in loop*/) {
    if (variable_ids_to_be_deleted.contains(it->first.first) ||
        variable_ids_to_be_deleted.contains(it->first.second)) {
      quadratic_objective_coefficients_.erase(it++);
    } else {
      ++it;
    }
  }
  // We cache all Gurobi variables and constraint indices that must be
  // deleted, and perform deletions at the end of the update call.
  DeletedIndices deleted_indices;

  RETURN_IF_ERROR(UpdateLinearConstraints(
      model_update.linear_constraint_updates(), deleted_indices.variables));

  for (const VariableId id : model_update.deleted_variable_ids()) {
    deleted_indices.variables.emplace_back(variables_map_.at(id));
    variables_map_.erase(id);
  }

  for (const LinearConstraintId id :
       model_update.deleted_linear_constraint_ids()) {
    LinearConstraintData& constraint_data = linear_constraints_map_.at(id);
    deleted_indices.linear_constraints.push_back(
        constraint_data.constraint_index);
    if (constraint_data.slack_index != kUnspecifiedIndex) {
      deleted_indices.variables.push_back(constraint_data.slack_index);
      constraint_data.slack_index = kUnspecifiedIndex;
    }
    linear_constraints_map_.erase(id);
  }

  for (const QuadraticConstraintId id :
       model_update.quadratic_constraint_updates().deleted_constraint_ids()) {
    const GurobiQuadraticConstraintIndex grb_index =
        quadratic_constraints_map_.at(id);
    deleted_indices.quadratic_constraints.push_back(grb_index);
    quadratic_constraints_map_.erase(id);
  }

  for (const SecondOrderConeConstraintId id :
       model_update.second_order_cone_constraint_updates()
           .deleted_constraint_ids()) {
    deleted_indices.quadratic_constraints.push_back(
        soc_constraints_map_.at(id).constraint_index);
    for (const GurobiVariableIndex index :
         soc_constraints_map_.at(id).slack_variables) {
      deleted_indices.variables.push_back(index);
    }
    for (const GurobiLinearConstraintIndex index :
         soc_constraints_map_.at(id).slack_constraints) {
      deleted_indices.linear_constraints.push_back(index);
    }
    soc_constraints_map_.erase(id);
  }

  const auto sos_updater = [&](const SosConstraintData& sos_constraint) {
    deleted_indices.sos_constraints.push_back(sos_constraint.constraint_index);
    for (const GurobiVariableIndex index : sos_constraint.slack_variables) {
      deleted_indices.variables.push_back(index);
    }
    for (const GurobiLinearConstraintIndex index :
         sos_constraint.slack_constraints) {
      deleted_indices.linear_constraints.push_back(index);
    }
  };
  for (const Sos1ConstraintId id :
       model_update.sos1_constraint_updates().deleted_constraint_ids()) {
    sos_updater(sos1_constraints_map_.at(id));
    sos1_constraints_map_.erase(id);
  }

  for (const Sos2ConstraintId id :
       model_update.sos2_constraint_updates().deleted_constraint_ids()) {
    sos_updater(sos2_constraints_map_.at(id));
    sos2_constraints_map_.erase(id);
  }

  for (const IndicatorConstraintId id :
       model_update.indicator_constraint_updates().deleted_constraint_ids()) {
    // Otherwise the constraint is not actually registered with Gurobi.
    const auto it = indicator_constraints_map_.find(id);
    CHECK(it != indicator_constraints_map_.end()) << "id: " << id;
    if (it->second.has_value()) {
      deleted_indices.general_constraints.push_back(
          it->second->constraint_index);
    }
    indicator_constraints_map_.erase(it);
  }

  UpdateGurobiIndices(deleted_indices);

  // If we are removing variables or constraints we remove them after adding
  // any variable or constraint. This is to avoid problems with
  // the numbering of possibly new variables and constraints.
  // After that we must update the model so that sequence of updates don't
  // interfere with one-another.
  if (!deleted_indices.linear_constraints.empty()) {
    RETURN_IF_ERROR(gurobi_->DelConstrs(deleted_indices.linear_constraints));
    num_gurobi_lin_cons_ -= deleted_indices.linear_constraints.size();
  }

  if (!deleted_indices.quadratic_constraints.empty()) {
    RETURN_IF_ERROR(
        gurobi_->DelQConstrs(deleted_indices.quadratic_constraints));
    num_gurobi_quad_cons_ -= deleted_indices.quadratic_constraints.size();
  }

  if (!deleted_indices.sos_constraints.empty()) {
    RETURN_IF_ERROR(gurobi_->DelSos(deleted_indices.sos_constraints));
  }

  if (!deleted_indices.general_constraints.empty()) {
    RETURN_IF_ERROR(
        gurobi_->DelGenConstrs(deleted_indices.general_constraints));
  }

  if (!deleted_indices.variables.empty()) {
    RETURN_IF_ERROR(gurobi_->DelVars(deleted_indices.variables));
    num_gurobi_variables_ -= deleted_indices.variables.size();
  }

  // Synchronize all pending changes.
  RETURN_IF_ERROR(gurobi_->UpdateModel());

  return true;
}

absl::StatusOr<std::unique_ptr<GurobiSolver>> GurobiSolver::New(
    const ModelProto& input_model, const SolverInterface::InitArgs& init_args) {
  // TODO(user): Correctly load the gurobi library in open source.
  RETURN_IF_ERROR(
      ModelIsSupported(input_model, kGurobiSupportedStructures, "Gurobi"));
  if (!input_model.auxiliary_objectives().empty() &&
      !input_model.objective().quadratic_coefficients().row_ids().empty()) {
    return util::InvalidArgumentErrorBuilder()
           << "Gurobi does not support multiple objective models with "
              "quadratic objectives";
  }
  for (const auto& [id, obj] : input_model.auxiliary_objectives()) {
    if (!obj.quadratic_coefficients().row_ids().empty()) {
      return util::InvalidArgumentErrorBuilder()
             << "Gurobi does not support multiple objective models with "
                "quadratic objectives";
    }
  }
  ASSIGN_OR_RETURN(std::unique_ptr<Gurobi> gurobi,
                   GurobiFromInitArgs(init_args));
  auto gurobi_solver = absl::WrapUnique(new GurobiSolver(std::move(gurobi)));
  RETURN_IF_ERROR(gurobi_solver->LoadModel(input_model));
  return gurobi_solver;
}

absl::StatusOr<std::unique_ptr<GurobiSolver::GurobiCallbackData>>
GurobiSolver::RegisterCallback(
    const CallbackRegistrationProto& registration, const Callback cb,
    const MessageCallback message_cb, const absl::Time start,
    SolveInterrupter* absl_nullable const local_interrupter) {
  const absl::flat_hash_set<CallbackEventProto> events = EventSet(registration);

  // Note that IS_MIP does not necessarily mean the problem has integer
  // variables. Please refer to Gurobi's doc for details:
  // https://www.gurobi.com/documentation/9.1/refman/ismip.html.
  //
  // Here we assume that we get MIP related events and use a MIP solving
  // strategy when IS_MIP is true.
  ASSIGN_OR_RETURN(const int is_mip, gurobi_->GetIntAttr(GRB_INT_ATTR_IS_MIP));

  RETURN_IF_ERROR(CheckRegisteredCallbackEvents(
      registration, is_mip ? SupportedMIPEvents() : SupportedLPEvents()))
      << "for a " << (is_mip ? "MIP" : "LP") << " model";

  // Set Gurobi parameters.
  if (message_cb != nullptr) {
    // Disable logging messages to the console the user wants to handle
    // messages.
    RETURN_IF_ERROR(gurobi_->SetIntParam(GRB_INT_PAR_LOGTOCONSOLE, 0));
  }
  if (registration.add_cuts() || registration.add_lazy_constraints()) {
    // This is to signal the solver presolve to limit primal transformations
    // that precludes crushing cuts to the presolved model.
    RETURN_IF_ERROR(gurobi_->SetIntParam(GRB_INT_PAR_PRECRUSH, 1));
  }
  if (registration.add_lazy_constraints()) {
    // This is needed so that the solver knows that some presolve reductions
    // can not be performed safely.
    RETURN_IF_ERROR(gurobi_->SetIntParam(GRB_INT_PAR_LAZYCONSTRAINTS, 1));
  }
  return std::make_unique<GurobiCallbackData>(
      GurobiCallbackInput{
          .user_cb = cb,
          .message_cb = message_cb,
          .variable_ids = variables_map_,
          .num_gurobi_vars = num_gurobi_variables_,
          .events = EventToGurobiWhere(events),
          .mip_solution_filter = registration.mip_solution_filter(),
          .mip_node_filter = registration.mip_node_filter(),
          .start = start},
      local_interrupter);
}

absl::StatusOr<InvertedBounds> GurobiSolver::ListInvertedBounds() const {
  InvertedBounds inverted_bounds;
  {
    ASSIGN_OR_RETURN(
        const std::vector<double> var_lbs,
        gurobi_->GetDoubleAttrArray(GRB_DBL_ATTR_LB, num_gurobi_variables_));
    ASSIGN_OR_RETURN(
        const std::vector<double> var_ubs,
        gurobi_->GetDoubleAttrArray(GRB_DBL_ATTR_UB, num_gurobi_variables_));
    for (const auto& [id, index] : variables_map_) {
      if (var_lbs[index] > var_ubs[index]) {
        inverted_bounds.variables.push_back(id);
      }
    }
  }
  for (const auto& [id, cstr_data] : linear_constraints_map_) {
    if (cstr_data.lower_bound > cstr_data.upper_bound) {
      inverted_bounds.linear_constraints.push_back(id);
    }
  }

  // Above code have inserted ids in non-stable order.
  std::sort(inverted_bounds.variables.begin(), inverted_bounds.variables.end());
  std::sort(inverted_bounds.linear_constraints.begin(),
            inverted_bounds.linear_constraints.end());
  return inverted_bounds;
}

absl::StatusOr<InvalidIndicators> GurobiSolver::ListInvalidIndicators() const {
  InvalidIndicators invalid_indicators;
  for (const auto& [constraint_id, indicator_data] :
       indicator_constraints_map_) {
    if (!indicator_data.has_value()) {
      continue;
    }
    const int64_t indicator_id = indicator_data->indicator_variable_id;
    const GurobiVariableIndex variable_index = variables_map_.at(indicator_id);
    ASSIGN_OR_RETURN(const double var_lb, gurobi_->GetDoubleAttrElement(
                                              GRB_DBL_ATTR_LB, variable_index));
    ASSIGN_OR_RETURN(const double var_ub, gurobi_->GetDoubleAttrElement(
                                              GRB_DBL_ATTR_UB, variable_index));
    ASSIGN_OR_RETURN(
        const char var_type,
        gurobi_->GetCharAttrElement(GRB_CHAR_ATTR_VTYPE, variable_index));
    if (!(var_type == GRB_BINARY ||
          (var_type == GRB_INTEGER && var_lb >= 0.0 && var_ub <= 1.0))) {
      invalid_indicators.invalid_indicators.push_back(
          {.variable = indicator_id, .constraint = constraint_id});
    }
  }
  // Above code may have inserted ids in non-stable order.
  invalid_indicators.Sort();
  return invalid_indicators;
}

bool GurobiSolver::is_multi_objective_mode() const {
  return !multi_objectives_map_.empty();
}

absl::Status GurobiSolver::SetMultiObjectiveParameters(
    const ModelSolveParametersProto& model_parameters) {
  const auto set_tolerances =
      [&](const GurobiMultiObjectiveIndex index,
          const ObjectiveParametersProto& objective_parameters)
      -> absl::Status {
    RETURN_IF_ERROR(gurobi_->SetIntParam("ObjNumber", index));
    if (objective_parameters.has_objective_degradation_absolute_tolerance()) {
      RETURN_IF_ERROR(gurobi_->SetDoubleAttr(
          "ObjNAbsTol",
          objective_parameters.objective_degradation_absolute_tolerance()));
    }
    if (objective_parameters.has_objective_degradation_relative_tolerance()) {
      RETURN_IF_ERROR(gurobi_->SetDoubleAttr(
          "ObjNRelTol",
          objective_parameters.objective_degradation_relative_tolerance()));
    }
    return absl::OkStatus();
  };
  const auto set_time_limit =
      [&](const GurobiMultiObjectiveIndex index,
          const ObjectiveParametersProto& objective_parameters)
      -> absl::Status {
    if (!objective_parameters.has_time_limit()) {
      // Unset time_limit defaults to infinite, so we don't need to do anything.
      return absl::OkStatus();
    }
    ASSIGN_OR_RETURN(
        const absl::Duration time_limit,
        util_time::DecodeGoogleApiProto(objective_parameters.time_limit()));
    return gurobi_->SetMultiObjectiveDoubleParam(
        GRB_DBL_PAR_TIMELIMIT, index, absl::ToDoubleSeconds(time_limit));
  };
  if (model_parameters.has_primary_objective_parameters()) {
    const GurobiMultiObjectiveIndex obj_index =
        multi_objectives_map_.at(std::nullopt);
    RETURN_IF_ERROR(set_tolerances(
        obj_index, model_parameters.primary_objective_parameters()))
        << " for primary objective";
    RETURN_IF_ERROR(set_time_limit(
        obj_index, model_parameters.primary_objective_parameters()))
        << " for primary objective";
  }
  for (const auto& [id, objective_parameters] :
       model_parameters.auxiliary_objective_parameters()) {
    const GurobiMultiObjectiveIndex obj_index = multi_objectives_map_.at(id);
    RETURN_IF_ERROR(set_tolerances(obj_index, objective_parameters))
        << " for auxiliary objective " << id;
    RETURN_IF_ERROR(set_time_limit(obj_index, objective_parameters))
        << " for auxiliary objective " << id;
  }
  return absl::OkStatus();
}

absl::Status GurobiSolver::ResetModelParameters(
    const ModelSolveParametersProto& model_parameters) {
  for (int i = 0; i < model_parameters.branching_priorities().ids_size(); ++i) {
    const int64_t var_id = model_parameters.branching_priorities().ids(i);
    const GurobiVariableIndex grb_index = variables_map_.at(var_id);
    RETURN_IF_ERROR(
        gurobi_->SetIntAttrElement(GRB_INT_ATTR_BRANCHPRIORITY, grb_index, 0))
        << "failed to reset branching priority for variable ID " << var_id
        << " (Gurobi index = " << grb_index << ")";
  }
  for (const int64_t lazy_constraint_id :
       model_parameters.lazy_linear_constraint_ids()) {
    const GurobiLinearConstraintIndex lazy_constraint_index =
        linear_constraints_map_.at(lazy_constraint_id).constraint_index;
    RETURN_IF_ERROR(
        gurobi_->SetIntAttrElement(GRB_INT_ATTR_LAZY, lazy_constraint_index, 0))
        << "failed to reset lazy constraint for lazy constraint ID "
        << lazy_constraint_id << " (Gurobi index = " << lazy_constraint_index
        << ")";
  }
  return absl::OkStatus();
}

absl::StatusOr<SolveResultProto> GurobiSolver::Solve(
    const SolveParametersProto& parameters,
    const ModelSolveParametersProto& model_parameters,
    const MessageCallback message_cb,
    const CallbackRegistrationProto& callback_registration, const Callback cb,
    const SolveInterrupter* absl_nullable const interrupter) {
  RETURN_IF_ERROR(ModelSolveParametersAreSupported(
      model_parameters, kGurobiSupportedStructures, "Gurobi"));
  const absl::Time start = absl::Now();

  // Need to run GRBupdatemodel before:
  //  1. setting parameters (to test if the problem is a MIP)
  //  2. registering callbacks (to test if the problem is a MIP),
  //  3. setting basis and getting the obj sense.
  // We just run it first.
  RETURN_IF_ERROR(gurobi_->UpdateModel());

  // Gurobi returns "infeasible" when bounds are inverted.
  {
    ASSIGN_OR_RETURN(const InvertedBounds inverted_bounds,
                     ListInvertedBounds());
    RETURN_IF_ERROR(inverted_bounds.ToStatus());
  }

  // Gurobi will silently impose that indicator variables are binary even if
  // not so specified by the user in the model. We return an error here if
  // this is the case to be consistent across solvers.
  {
    ASSIGN_OR_RETURN(const InvalidIndicators invalid_indicators,
                     ListInvalidIndicators());
    RETURN_IF_ERROR(invalid_indicators.ToStatus());
  }

  // We must set the parameters before calling RegisterCallback since it
  // changes some parameters depending on the callback registration.
  RETURN_IF_ERROR(SetParameters(parameters, model_parameters));

  // We use a local interrupter that will triggers the calls to GRBterminate()
  // when either the user interrupter is triggered or when a callback returns
  // a true `terminate`.
  std::unique_ptr<SolveInterrupter> local_interrupter;
  if (cb != nullptr || interrupter != nullptr) {
    local_interrupter = std::make_unique<SolveInterrupter>();
  }
  const ScopedSolveInterrupterCallback scoped_terminate_callback(
      local_interrupter.get(), [&]() {
        // Make an immediate call to GRBterminate() as soon as this
        // interrupter is triggered (which may immediately happen in the code
        // below when it is chained with the optional user interrupter).
        //
        // This call may happen too early. This is not an issue since we will
        // repeat this call at each call of the Gurobi callback. See the
        // comment in GurobiCallbackImpl() for details.
        gurobi_->Terminate();
      });

  // Chain the user interrupter to the local interrupter. If/when the user
  // interrupter is triggered, this triggers the local interrupter. This may
  // happen immediately if the user interrupter is already triggered.
  //
  // The local interrupter can also be triggered by a callback returning a
  // true `terminate`.
  const ScopedSolveInterrupterCallback scoped_chaining_callback(
      interrupter, [&]() { local_interrupter->Interrupt(); });

  if (model_parameters.has_initial_basis()) {
    RETURN_IF_ERROR(SetGurobiBasis(model_parameters.initial_basis()));
  }
  RETURN_IF_ERROR(gurobi_->SetIntAttr(GRB_INT_ATTR_NUMSTART,
                                      model_parameters.solution_hints_size()));
  for (int i = 0; i < model_parameters.solution_hints_size(); ++i) {
    RETURN_IF_ERROR(gurobi_->SetIntParam(GRB_INT_PAR_STARTNUMBER, i));
    RETURN_IF_ERROR(UpdateDoubleListAttribute(
        model_parameters.solution_hints(i).variable_values(),
        GRB_DBL_ATTR_START, variables_map_));
  }
  RETURN_IF_ERROR(
      UpdateInt32ListAttribute(model_parameters.branching_priorities(),
                               GRB_INT_ATTR_BRANCHPRIORITY, variables_map_));
  if (is_multi_objective_mode()) {
    RETURN_IF_ERROR(SetMultiObjectiveParameters(model_parameters));
  }
  for (const int64_t lazy_constraint_id :
       model_parameters.lazy_linear_constraint_ids()) {
    const GurobiLinearConstraintIndex lazy_constraint_index =
        linear_constraints_map_.at(lazy_constraint_id).constraint_index;
    // We select a value of "1" here, which means that the lazy constraints will
    // be separated at feasible solutions, and that Gurobi has latitude to
    // select which violated constraints to add to the model if multiple are
    // violated. This seems like a reasonable default choice for us, but we may
    // want to revisit later (or expose this choice to the user).
    RETURN_IF_ERROR(gurobi_->SetIntAttrElement(GRB_INT_ATTR_LAZY,
                                               lazy_constraint_index, 1));
  }

  // Here we register the callback when we either have a user callback or a
  // local interrupter. The rationale for doing so when we have only an
  // interrupter is explained in GurobiCallbackImpl().
  Gurobi::Callback grb_cb = nullptr;
  std::unique_ptr<GurobiCallbackData> gurobi_cb_data;
  if (cb != nullptr || local_interrupter != nullptr || message_cb != nullptr) {
    ASSIGN_OR_RETURN(gurobi_cb_data,
                     RegisterCallback(callback_registration, cb, message_cb,
                                      start, local_interrupter.get()));
    grb_cb = [&gurobi_cb_data](
                 const Gurobi::CallbackContext& cb_context) -> absl::Status {
      return GurobiCallbackImpl(cb_context, gurobi_cb_data->callback_input,
                                gurobi_cb_data->message_callback_data,
                                gurobi_cb_data->local_interrupter);
    };
  }

  RETURN_IF_ERROR(gurobi_->Optimize(grb_cb));

  // We flush message callbacks before testing for Gurobi error in case where
  // the unfinished line of message would help with the error.
  if (gurobi_cb_data != nullptr) {
    GurobiCallbackImplFlush(gurobi_cb_data->callback_input,
                            gurobi_cb_data->message_callback_data);
  }

  ASSIGN_OR_RETURN(SolveResultProto solve_result,
                   ExtractSolveResultProto(start, model_parameters));
  // Reset Gurobi parameters.
  // TODO(b/277246682): ensure that resetting parameters does not degrade
  // incrementalism performance.
  RETURN_IF_ERROR(gurobi_->ResetParameters());
  RETURN_IF_ERROR(ResetModelParameters(model_parameters));

  return solve_result;
}

// TODO(b/277339044): Remove code duplication with GurobiSolver::Solve().
absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
GurobiSolver::ComputeInfeasibleSubsystem(
    const SolveParametersProto& parameters, MessageCallback message_cb,
    const SolveInterrupter* absl_nullable const interrupter) {
  const absl::Time start = absl::Now();

  // Need to run GRBupdatemodel before:
  //  1. setting parameters (to test if the problem is a MIP)
  //  2. registering callbacks (to test if the problem is a MIP),
  RETURN_IF_ERROR(gurobi_->UpdateModel());

  // Gurobi will silently impose that indicator variables are binary even if
  // not so specified by the user in the model. We return an error here if
  // this is the case to be consistent across solvers.
  {
    ASSIGN_OR_RETURN(const InvalidIndicators invalid_indicators,
                     ListInvalidIndicators());
    RETURN_IF_ERROR(invalid_indicators.ToStatus());
  }

  // We must set the parameters before calling RegisterCallback since it
  // changes some parameters depending on the callback registration.
  RETURN_IF_ERROR(SetParameters(parameters));

  // We use a local interrupter that will triggers the calls to
  // GRBterminate() when either the user interrupter is triggered or when a
  // callback returns a true `terminate`.
  std::unique_ptr<SolveInterrupter> local_interrupter;
  if (interrupter != nullptr) {
    local_interrupter = std::make_unique<SolveInterrupter>();
  }
  const ScopedSolveInterrupterCallback scoped_terminate_callback(
      local_interrupter.get(), [&]() {
        // Make an immediate call to GRBterminate() as soon as this
        // interrupter is triggered (which may immediately happen in the
        // code below when it is chained with the optional user
        // interrupter).
        //
        // This call may happen too early. This is not an issue since we
        // will repeat this call at each call of the Gurobi callback. See
        // the comment in GurobiCallbackImpl() for details.
        gurobi_->Terminate();
      });

  // Chain the user interrupter to the local interrupter. If/when the user
  // interrupter is triggered, this triggers the local interrupter. This may
  // happen immediately if the user interrupter is already triggered.
  //
  // The local interrupter can also be triggered by a callback returning a
  // true `terminate`.
  const ScopedSolveInterrupterCallback scoped_chaining_callback(
      interrupter, [&]() { local_interrupter->Interrupt(); });

  // Here we register the callback when we either have a message callback or a
  // local interrupter. The rationale for doing so when we have only an
  // interrupter is explained in GurobiCallbackImpl().
  Gurobi::Callback grb_cb = nullptr;
  std::unique_ptr<GurobiCallbackData> gurobi_cb_data;
  if (local_interrupter != nullptr || message_cb != nullptr) {
    ASSIGN_OR_RETURN(gurobi_cb_data,
                     RegisterCallback({}, nullptr, message_cb, start,
                                      local_interrupter.get()));
    grb_cb = [&gurobi_cb_data](
                 const Gurobi::CallbackContext& cb_context) -> absl::Status {
      return GurobiCallbackImpl(cb_context, gurobi_cb_data->callback_input,
                                gurobi_cb_data->message_callback_data,
                                gurobi_cb_data->local_interrupter);
    };
  }

  ASSIGN_OR_RETURN(const bool proven_infeasible, gurobi_->ComputeIIS(grb_cb));

  // We flush message callbacks before testing for Gurobi error in case
  // where the unfinished line of message would help with the error.
  if (gurobi_cb_data != nullptr) {
    GurobiCallbackImplFlush(gurobi_cb_data->callback_input,
                            gurobi_cb_data->message_callback_data);
  }

  ASSIGN_OR_RETURN(
      ComputeInfeasibleSubsystemResultProto iis_result,
      ExtractComputeInfeasibleSubsystemResultProto(proven_infeasible));
  // Reset Gurobi parameters.
  // TODO(b/277246682): ensure that resetting parameters does not degrade
  // incrementalism performance.
  RETURN_IF_ERROR(gurobi_->ResetParameters());

  return iis_result;
}

MATH_OPT_REGISTER_SOLVER(SOLVER_TYPE_GUROBI, GurobiSolver::New)

}  // namespace math_opt
}  // namespace operations_research
