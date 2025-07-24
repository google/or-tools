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

// Unimplemented features:
//  * Quadratic objective
//  * TODO(b/272767311): initial basis, more precise returned basis.
//  * TODO(b/271104776): Returning rays

#include "ortools/math_opt/solvers/highs_solver.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Highs.h"
#include "absl/algorithm/container.h"
#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "io/HighsIO.h"
#include "lp_data/HConst.h"
#include "lp_data/HStruct.h"
#include "lp_data/HighsInfo.h"
#include "lp_data/HighsLp.h"
#include "lp_data/HighsModelUtils.h"
#include "lp_data/HighsOptions.h"
#include "lp_data/HighsStatus.h"
#include "model/HighsModel.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/empty_bounds.h"
#include "ortools/math_opt/core/inverted_bounds.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/core/sorted.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/solvers/highs.pb.h"
#include "ortools/math_opt/solvers/message_callback_data.h"
#include "ortools/util/solve_interrupter.h"
#include "ortools/util/status_macros.h"
#include "simplex/SimplexConst.h"
#include "util/HighsInt.h"

namespace operations_research::math_opt {
namespace {

constexpr absl::string_view kOutputFlag = "output_flag";
constexpr absl::string_view kLogToConsole = "log_to_console";

constexpr SupportedProblemStructures kHighsSupportedStructures = {
    .integer_variables = SupportType::kSupported,
    .quadratic_objectives = SupportType::kNotImplemented};

absl::Status ToStatus(const HighsStatus status) {
  switch (status) {
    case HighsStatus::kOk:
      return absl::OkStatus();
    case HighsStatus::kWarning:
      // There doesn't seem to be much we can do with this beyond ignoring it,
      // which does not seem best. Highs returns a warning when you solve and
      // don't get a primal feasible solution, but MathOpt does not consider
      // this to be warning worthy.
      return absl::OkStatus();
    case HighsStatus::kError:
      return util::InternalErrorBuilder() << "HighsStatus: kError";
    default:
      return util::InternalErrorBuilder()
             << "unexpected HighsStatus: " << static_cast<int>(status);
  }
}

absl::Status ToStatus(const OptionStatus option_status) {
  switch (option_status) {
    case OptionStatus::kOk:
      return absl::OkStatus();
    case OptionStatus::kUnknownOption:
      return absl::InvalidArgumentError("option name was unknown");
    case OptionStatus::kIllegalValue:
      // NOTE: highs returns this if the option type is wrong or if the value
      // is out of bounds for the option.
      return absl::InvalidArgumentError("option value not valid for name");
  }
  return util::InternalErrorBuilder()
         << "unexpected option_status: " << static_cast<int>(option_status);
}

absl::StatusOr<int> SafeIntCast(const int64_t i, const absl::string_view name) {
  if constexpr (sizeof(int) >= sizeof(int64_t)) {
    return static_cast<int>(i);
  } else {
    const int64_t kMin = static_cast<int64_t>(std::numeric_limits<int>::min());
    const int64_t kMax = static_cast<int64_t>(std::numeric_limits<int>::max());
    if (i < kMin || i > kMax) {
      return util::InvalidArgumentErrorBuilder()
             << name << " has value " << i
             << " not representable as an int (the range [" << kMin << ", "
             << kMax << "]) and thus is not supported for HiGHS";
    }
    return static_cast<int>(i);
  }
}

template <typename T>
int64_t CastInt64StaticAssert(const T value) {
  static_assert(std::is_integral_v<T>);
  static_assert(sizeof(T) <= sizeof(int64_t));
  return static_cast<int64_t>(value);
}

// Note: the highs solver has very little documentation, but you can find some
// here https://www.gams.com/latest/docs/S_HIGHS.html.
absl::StatusOr<std::unique_ptr<HighsOptions>> MakeOptions(
    const SolveParametersProto& parameters, const bool has_log_callback,
    const bool is_integer) {
  // Copy/move seem to be broken for HighsOptions, asan errors.
  auto result = std::make_unique<HighsOptions>();

  if (parameters.highs().bool_options().contains(kOutputFlag)) {
    result->output_flag = parameters.highs().bool_options().at(kOutputFlag);
  } else {
    result->output_flag = parameters.enable_output() || has_log_callback;
  }
  // This feature of highs is pretty confusing/surprising. To use a callback,
  // you need log_to_console to be true. From this line:
  //   https://github.com/ERGO-Code/HiGHS/blob/master/src/io/HighsIO.cpp#L101
  // we see that if log_to_console is false and log_file_stream are null, we get
  // no logging at all.
  //
  // Further, when the callback is set, we won't log to console anyway. But from
  // the names it seems like it should be
  // result.log_to_console = parameters.enable_output() && !has_log_callback;
  if (parameters.highs().bool_options().contains(kLogToConsole)) {
    result->log_to_console =
        parameters.highs().bool_options().at(kLogToConsole);
  } else {
    result->log_to_console = result->output_flag;
  }
  if (parameters.has_time_limit()) {
    OR_ASSIGN_OR_RETURN3(
        const absl::Duration time_limit,
        util_time::DecodeGoogleApiProto(parameters.time_limit()),
        _ << "invalid time_limit value for HiGHS.");
    result->time_limit = absl::ToDoubleSeconds(time_limit);
  }
  if (parameters.has_iteration_limit()) {
    if (is_integer) {
      return util::InvalidArgumentErrorBuilder()
             << "iteration_limit not supported for HiGHS on problems with "
                "integer variables";
    }
    ASSIGN_OR_RETURN(
        const int iter_limit,
        SafeIntCast(parameters.iteration_limit(), "iteration_limit"));

    result->simplex_iteration_limit = iter_limit;
    result->ipm_iteration_limit = iter_limit;
  }
  if (parameters.has_node_limit()) {
    ASSIGN_OR_RETURN(result->mip_max_nodes,
                     SafeIntCast(parameters.node_limit(), "node_limit"));
  }
  if (parameters.has_cutoff_limit()) {
    // TODO(b/271606858) : It may be possible to get this working for IPs via
    //  objective_bound. For LPs this approach will not work.
    return absl::InvalidArgumentError("cutoff_limit not supported for HiGHS");
  }
  if (parameters.has_objective_limit()) {
    if (is_integer) {
      return util::InvalidArgumentErrorBuilder()
             << "objective_limit not supported for HiGHS solver on integer "
                "problems.";
    } else {
      // TODO(b/271616762): it appears that HiGHS intended to support this case
      // but that it is just broken, we should set result.objective_target.
      return absl::InvalidArgumentError(
          "objective_limit for LP appears to have a missing/broken HiGHS "
          "implementation, see b/271616762");
    }
  }
  if (parameters.has_best_bound_limit()) {
    if (is_integer) {
      return util::InvalidArgumentErrorBuilder()
             << "best_bound_limit not supported for HiGHS solver on integer "
                "problems.";
    } else {
      result->objective_bound = parameters.best_bound_limit();
    }
  }
  if (parameters.has_solution_limit()) {
    result->mip_max_improving_sols = parameters.solution_limit();
  }
  if (parameters.has_threads()) {
    // Do not assign result.threads = parameters.threads() here, this is
    // requires global synchronization. See
    // cs/highs/src/lp_data/Highs.cpp:607
    return util::InvalidArgumentErrorBuilder()
           << "threads not supported for HiGHS solver, this must be set using "
              "globals, see HiGHS documentation";
  }
  if (parameters.has_random_seed()) {
    result->random_seed = parameters.random_seed();
  }
  if (parameters.has_absolute_gap_tolerance()) {
    result->mip_abs_gap = parameters.absolute_gap_tolerance();
  }
  if (parameters.has_relative_gap_tolerance()) {
    result->mip_rel_gap = parameters.relative_gap_tolerance();
  }
  if (parameters.has_solution_pool_size()) {
    return util::InvalidArgumentErrorBuilder()
           << "solution_pool_size not supported for HiGHS";
  }
  if (parameters.lp_algorithm() != LP_ALGORITHM_UNSPECIFIED) {
    if (is_integer) {
      return util::InvalidArgumentErrorBuilder()
             << "lp_algorithm is not supported for HiGHS on problems with "
                "integer variables";
    }
    switch (parameters.lp_algorithm()) {
      case LP_ALGORITHM_PRIMAL_SIMPLEX:
        result->solver = ::kSimplexString;
        result->simplex_strategy = ::kSimplexStrategyPrimal;
        break;
      case LP_ALGORITHM_DUAL_SIMPLEX:
        result->solver = ::kSimplexString;
        result->simplex_strategy = ::kSimplexStrategyDual;
        break;
      case LP_ALGORITHM_BARRIER:
        result->solver = ::kIpmString;
        break;
      default:
        return util::InvalidArgumentErrorBuilder()
               << "unsupported lp_algorithm: "
               << LPAlgorithmProto_Name(parameters.lp_algorithm());
    }
  }
  if (parameters.presolve() != EMPHASIS_UNSPECIFIED) {
    if (parameters.presolve() == EMPHASIS_OFF) {
      result->presolve = ::kHighsOffString;
    } else {
      result->presolve = ::kHighsOnString;
    }
  }
  if (parameters.cuts() != EMPHASIS_UNSPECIFIED) {
    return util::InvalidArgumentErrorBuilder()
           << "cuts solve parameter unsupported for HiGHS";
  }
  if (parameters.heuristics() != EMPHASIS_UNSPECIFIED) {
    switch (parameters.heuristics()) {
      case EMPHASIS_OFF:
        result->mip_heuristic_effort = 0.0;
        break;
      case EMPHASIS_LOW:
        result->mip_heuristic_effort = 0.025;
        break;
      case EMPHASIS_MEDIUM:
        result->mip_heuristic_effort = 0.05;
        break;
      case EMPHASIS_HIGH:
        result->mip_heuristic_effort = 0.1;
        break;
      case EMPHASIS_VERY_HIGH:
        result->mip_heuristic_effort = 0.2;
        break;
      default:
        return util::InvalidArgumentErrorBuilder()
               << "unexpected value for solve_parameters.heuristics of: "
               << parameters.heuristics();
    }
  }
  if (parameters.scaling() != EMPHASIS_UNSPECIFIED) {
    // Maybe we can do better here? Not clear how highs scaling works
    if (parameters.scaling() == EMPHASIS_OFF) {
      result->simplex_scale_strategy = ::kSimplexScaleStrategyOff;
    }
  }
  for (const auto& [name, value] : parameters.highs().string_options()) {
    if (name == kOutputFlag || name == kLogToConsole) {
      // This case was handled specially above. We need to do the output
      // parameters first, as we don't want extra logging while setting options.
      continue;
    }
    RETURN_IF_ERROR(ToStatus(setLocalOptionValue(result->log_options, name,
                                                 result->log_options,
                                                 result->records, value)))
        << "error setting string option name: " << name
        << " to value:" << value;
  }
  for (const auto& [name, value] : parameters.highs().double_options()) {
    RETURN_IF_ERROR(ToStatus(
        setLocalOptionValue(result->log_options, name, result->records, value)))
        << "error setting double option name: " << name
        << " to value:" << value;
  }
  for (const auto& [name, value] : parameters.highs().int_options()) {
    RETURN_IF_ERROR(ToStatus(
        setLocalOptionValue(result->log_options, name, result->records, value)))
        << "error setting int option name: " << name << " to value:" << value;
  }
  for (const auto& [name, value] : parameters.highs().bool_options()) {
    RETURN_IF_ERROR(ToStatus(
        setLocalOptionValue(result->log_options, name, result->records, value)))
        << "error setting bool option name: " << name << " to value:" << value;
  }
  return result;
}

double DualObjective(const HighsInfo& highs_info, const bool is_integer) {
  // TODO(b/290359402): for is_integer = false, consider computing the objective
  // of a returned dual feasible solution instead.
  return is_integer ? highs_info.mip_dual_bound
                    : highs_info.objective_function_value;
}
// Note that this is the expected/required function signature for highs logging
// callbacks as set with Highs::setLogCallback().
void HighsLogCallback(HighsLogType, const char* const message,
                      void* const log_callback_data) {
  BufferedMessageCallback& buffered_callback =
      *static_cast<BufferedMessageCallback*>(log_callback_data);
  buffered_callback.OnMessage(message);
}

// highs_info must be valid. Does not fill in solve time.
absl::StatusOr<SolveStatsProto> ToSolveStats(const HighsInfo& highs_info) {
  SolveStatsProto result;
  // HiGHS does to not report simplex and barrier count for mip. There is no
  // way to extract it, as it is held in
  // HighsMipSolver.mipdata_.total_lp_iterations, but the HighsMipSolver
  // object is created and destroyed within a single call to Highs.run() here:
  // https://github.com/ERGO-Code/HiGHS/blob/master/src/lp_data/Highs.cpp#L2976
  result.set_simplex_iterations(std::max(
      int64_t{0}, CastInt64StaticAssert(highs_info.simplex_iteration_count)));
  result.set_barrier_iterations(std::max(
      int64_t{0}, CastInt64StaticAssert(highs_info.ipm_iteration_count)));
  result.set_node_count(std::max(int64_t{0}, highs_info.mip_node_count));
  return result;
}

// Returns nullopt for nonbasic variables when the upper/lower status is not
// known.
absl::StatusOr<std::optional<BasisStatusProto>> ToBasisStatus(
    const HighsBasisStatus highs_basis, const double lb, const double ub,
    const std::optional<double> value) {
  switch (highs_basis) {
    case HighsBasisStatus::kBasic:
      return BASIS_STATUS_BASIC;
    case HighsBasisStatus::kUpper:
      return BASIS_STATUS_AT_UPPER_BOUND;
    case HighsBasisStatus::kLower:
      // Note: highs returns lower for fixed.
      // https://github.com/ERGO-Code/HiGHS/blob/master/src/lp_data/HConst.h#L192
      // TODO(b/272767311): investigate returning fixed instead.
      return BASIS_STATUS_AT_LOWER_BOUND;
    case HighsBasisStatus::kZero:
      return BASIS_STATUS_FREE;
    // TODO(b/272767311): this can potentially be simplified/deleted, we need
    // to see if HiGHS will ever return kNonbasic/decide if we want to support
    // kNonbasic as part of the mathopt starting basis API.
    case HighsBasisStatus::kNonbasic: {
      const bool lb_finite = std::isfinite(lb);
      const bool ub_finite = std::isfinite(ub);
      // TODO(b/272767311): it would be better if this was configurable, use a
      // small/conservative value for now (if it fails, we fail to return a
      // basis).
      constexpr double kAtBoundTolerance = 1.0e-10;
      if (lb_finite && ub_finite) {
        if (lb == ub) {
          return BASIS_STATUS_FIXED_VALUE;
        } else if (value.has_value() &&
                   std::abs(lb - *value) < kAtBoundTolerance) {
          return BASIS_STATUS_AT_LOWER_BOUND;
        } else if (value.has_value() &&
                   std::abs(ub - *value) < kAtBoundTolerance) {
          return BASIS_STATUS_AT_UPPER_BOUND;
        }
        // We cannot infer if we are at upper or at lower. Mathopt does not
        // an encoding for nonbasic but unknown upper/lower, see b/272767311.
        return std::nullopt;
      } else if (lb_finite) {
        return BASIS_STATUS_AT_LOWER_BOUND;
      } else if (ub_finite) {
        return BASIS_STATUS_AT_LOWER_BOUND;
      } else {
        return BASIS_STATUS_FREE;
      }
    }
  }
  return util::InternalErrorBuilder()
         << "unexpected highs basis: " << static_cast<int>(highs_basis);
}

absl::StatusOr<SolutionStatusProto> ToSolutionStatus(
    const HighsInt highs_solution_status) {
  switch (highs_solution_status) {
    case ::kSolutionStatusInfeasible:
      return SOLUTION_STATUS_INFEASIBLE;
    case ::kSolutionStatusFeasible:
      return SOLUTION_STATUS_FEASIBLE;
    case ::kSolutionStatusNone:
      return SOLUTION_STATUS_UNDETERMINED;
  }
  return util::InternalErrorBuilder()
         << "unimplemented highs SolutionStatus: " << highs_solution_status;
}

}  // namespace

absl::StatusOr<FeasibilityStatusProto> HighsSolver::DualFeasibilityStatus(
    const HighsInfo& highs_info, const bool is_integer,
    const SolutionClaims solution_claims) {
  const bool dual_feasible_solution_exists =
      solution_claims.highs_returned_dual_feasible_solution ||
      (is_integer && std::isfinite(highs_info.mip_dual_bound));
  if (dual_feasible_solution_exists &&
      solution_claims.highs_returned_primal_ray) {
    return util::InternalErrorBuilder()
           << "Found dual feasible solution and primal ray";
  }
  if (dual_feasible_solution_exists) {
    return FEASIBILITY_STATUS_FEASIBLE;
  }
  if (solution_claims.highs_returned_primal_ray) {
    return FEASIBILITY_STATUS_INFEASIBLE;
  }
  return FEASIBILITY_STATUS_UNDETERMINED;
}

absl::StatusOr<FeasibilityStatusProto> HighsSolver::PrimalFeasibilityStatus(
    const SolutionClaims solution_claims) {
  if (solution_claims.highs_returned_primal_feasible_solution &&
      solution_claims.highs_returned_dual_ray) {
    return util::InternalErrorBuilder()
           << "Found primal feasible solution and dual ray";
  }
  if (solution_claims.highs_returned_primal_feasible_solution) {
    return FEASIBILITY_STATUS_FEASIBLE;
  }
  if (solution_claims.highs_returned_dual_ray) {
    return FEASIBILITY_STATUS_INFEASIBLE;
  }
  return FEASIBILITY_STATUS_UNDETERMINED;
}

absl::StatusOr<TerminationProto> HighsSolver::MakeTermination(
    const HighsModelStatus highs_model_status, const HighsInfo& highs_info,
    const bool is_integer, const bool had_node_limit,
    const bool had_solution_limit, const bool is_maximize,
    const SolutionClaims solution_claims) {
  ASSIGN_OR_RETURN(
      const FeasibilityStatusProto dual_feasibility_status,
      DualFeasibilityStatus(highs_info, is_integer, solution_claims));
  ASSIGN_OR_RETURN(const FeasibilityStatusProto primal_feasibility_status,
                   PrimalFeasibilityStatus(solution_claims));

  const std::optional<double> optional_finite_primal_objective =
      (primal_feasibility_status == FEASIBILITY_STATUS_FEASIBLE)
          ? std::make_optional(highs_info.objective_function_value)
          : std::nullopt;
  const std::optional<double> optional_dual_objective =
      (dual_feasibility_status == FEASIBILITY_STATUS_FEASIBLE)
          ? std::make_optional(DualObjective(highs_info, is_integer))
          : std::nullopt;
  switch (highs_model_status) {
    case HighsModelStatus::kNotset:
    case HighsModelStatus::kLoadError:
    case HighsModelStatus::kModelError:
    case HighsModelStatus::kPresolveError:
    case HighsModelStatus::kSolveError:
    case HighsModelStatus::kPostsolveError:
    case HighsModelStatus::kUnknown:
    // Note: we actually deal with kModelEmpty separately in Solve(), this
    // case should not be hit.
    case HighsModelStatus::kModelEmpty:
      return util::InternalErrorBuilder()
             << "HighsModelStatus was "
             << utilModelStatusToString(highs_model_status);
    case HighsModelStatus::kOptimal: {
      return OptimalTerminationProto(highs_info.objective_function_value,
                                     DualObjective(highs_info, is_integer),
                                     "HighsModelStatus is kOptimal");
    }
    case HighsModelStatus::kInfeasible:
      // By convention infeasible MIPs are always dual feasible.
      return InfeasibleTerminationProto(is_maximize,
                                        /*dual_feasibility_status=*/is_integer
                                            ? FEASIBILITY_STATUS_FEASIBLE
                                            : dual_feasibility_status);
    case HighsModelStatus::kUnboundedOrInfeasible:
      return InfeasibleOrUnboundedTerminationProto(
          is_maximize, dual_feasibility_status,
          "HighsModelStatus is kUnboundedOrInfeasible");
    case HighsModelStatus::kUnbounded: {
      // TODO(b/271104776): we should potentially always return
      // TERMINATION_REASON_UNBOUNDED instead, we need to determine if
      // HighsModelStatus::kUnbounded implies the problem is known to be primal
      // feasible (for LP and MIP).
      if (highs_info.primal_solution_status == ::kSolutionStatusFeasible) {
        return UnboundedTerminationProto(is_maximize);
      } else {
        return InfeasibleOrUnboundedTerminationProto(
            is_maximize,
            /*dual_feasibility_status=*/FEASIBILITY_STATUS_INFEASIBLE,
            "HighsModelStatus is kUnbounded");
      }
    }
    case HighsModelStatus::kObjectiveBound:
      return LimitTerminationProto(
          is_maximize, LIMIT_OBJECTIVE, optional_finite_primal_objective,
          optional_dual_objective, "HighsModelStatus is kObjectiveBound");
    case HighsModelStatus::kObjectiveTarget:
      return LimitTerminationProto(
          is_maximize, LIMIT_OBJECTIVE, optional_finite_primal_objective,
          optional_dual_objective, "HighsModelStatus is kObjectiveTarget");
    case HighsModelStatus::kTimeLimit:
      return LimitTerminationProto(is_maximize, LIMIT_TIME,
                                   optional_finite_primal_objective,
                                   optional_dual_objective);
    case HighsModelStatus::kIterationLimit: {
      return LimitTerminationProto(is_maximize, LIMIT_ITERATION,
                                   optional_finite_primal_objective,
                                   optional_dual_objective);
    }
    case HighsModelStatus::kSolutionLimit: {
      if (had_node_limit && !had_solution_limit) {
        return LimitTerminationProto(is_maximize, LIMIT_NODE,
                                     optional_finite_primal_objective,
                                     optional_dual_objective);
      } else if (had_solution_limit && !had_node_limit) {
        return LimitTerminationProto(is_maximize, LIMIT_SOLUTION,
                                     optional_finite_primal_objective,
                                     optional_dual_objective);
      } else {
        return LimitTerminationProto(
            is_maximize, LIMIT_UNDETERMINED, optional_finite_primal_objective,
            optional_dual_objective,
            "HighsModelStatus was kSolutionLimit but cannot infer a MathOpt "
            "Limit, could be NODE_LIMIT or SOLUTION_LIMIT");
      }
    }
    case HighsModelStatus::kInterrupt:
      return LimitTerminationProto(is_maximize, LIMIT_INTERRUPTED,
                                   optional_finite_primal_objective,
                                   optional_dual_objective);
    case HighsModelStatus::kMemoryLimit:
      return LimitTerminationProto(
          is_maximize, LIMIT_OTHER, optional_finite_primal_objective,
          optional_dual_objective, "Highs hit kMemoryLimit");
  }
  return util::InternalErrorBuilder() << "HighsModelStatus unimplemented: "
                                      << static_cast<int>(highs_model_status);
}

SolveResultProto HighsSolver::ResultForHighsModelStatusModelEmpty(
    const bool is_maximize, const double objective_offset,
    const absl::flat_hash_map<int64_t, IndexAndBound>& lin_con_data) {
  SolveResultProto result;
  bool feasible = true;
  for (const auto& [unused, lin_con_bounds] : lin_con_data) {
    if (lin_con_bounds.lb > 0 || lin_con_bounds.ub < 0) {
      feasible = false;
      break;
    }
  }
  result.mutable_termination()->set_reason(
      feasible ? TERMINATION_REASON_OPTIMAL : TERMINATION_REASON_INFEASIBLE);
  result.mutable_termination()->set_detail("HighsModelStatus was kEmptyModel");
  if (feasible) {
    auto solution = result.add_solutions()->mutable_primal_solution();
    solution->set_objective_value(objective_offset);
    solution->set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
    *result.mutable_termination() =
        OptimalTerminationProto(objective_offset, objective_offset);
  } else {
    // If the primal problem has no variables, the dual problem is unconstrained
    // and thus always feasible.
    *result.mutable_termination() =
        InfeasibleTerminationProto(is_maximize, /*dual_feasibility_status=*/
                                   FEASIBILITY_STATUS_FEASIBLE);
    // It is probably possible to return a ray here as well.
  }
  return result;
}

InvertedBounds HighsSolver::ListInvertedBounds() {
  const auto find_crossed =
      [](const absl::flat_hash_map<int64_t, IndexAndBound>& id_to_bound_data) {
        std::vector<int64_t> result;
        for (const auto& [id, bound_data] : id_to_bound_data) {
          if (bound_data.bounds_cross()) {
            result.push_back(id);
          }
        }
        absl::c_sort(result);
        return result;
      };
  return {.variables = find_crossed(variable_data_),
          .linear_constraints = find_crossed(lin_con_data_)};
}

absl::StatusOr<std::optional<BasisProto>> HighsSolver::ExtractBasis() {
  const HighsInfo& highs_info = highs_->getInfo();
  const HighsBasis& highs_basis = highs_->getBasis();
  const HighsSolution& highs_solution = highs_->getSolution();
  if (highs_info.basis_validity != ::kBasisValidityValid) {
    return std::nullopt;
  }
  // We need the primal/dual solution to try and infer a more precise status
  // for varaiables and constraints listed as kNonBasic.
  if (!highs_solution.value_valid || !highs_solution.dual_valid) {
    return std::nullopt;
  }
  // Make sure the solution is the right size
  RETURN_IF_ERROR(EnsureOneEntryPerVariable(highs_solution.col_value))
      << "invalid highs_solution.col_value";
  RETURN_IF_ERROR(EnsureOneEntryPerVariable(highs_solution.col_dual))
      << "invalid highs_solution.col_dual";
  // Make sure the basis is the right size
  RETURN_IF_ERROR(EnsureOneEntryPerVariable(highs_basis.col_status))
      << "invalid highs_basis.col_status";
  RETURN_IF_ERROR(EnsureOneEntryPerLinearConstraint(highs_basis.row_status))
      << "invalid highs_basis.row_status";
  BasisProto basis;

  if (highs_->getModelStatus() == HighsModelStatus::kOptimal) {
    basis.set_basic_dual_feasibility(SOLUTION_STATUS_FEASIBLE);
  } else if (highs_info.dual_solution_status == kSolutionStatusInfeasible) {
    basis.set_basic_dual_feasibility(SOLUTION_STATUS_INFEASIBLE);
  } else {
    // TODO(b/272767311): we need to do more to fill this in properly.
    basis.set_basic_dual_feasibility(SOLUTION_STATUS_UNDETERMINED);
  }
  for (const int64_t var_id : SortedMapKeys(variable_data_)) {
    const IndexAndBound& index_and_bounds = variable_data_.at(var_id);
    const double var_value = highs_solution.col_value[index_and_bounds.index];
    OR_ASSIGN_OR_RETURN3(
        const std::optional<BasisStatusProto> status,
        ToBasisStatus(highs_basis.col_status[variable_data_.at(var_id).index],
                      index_and_bounds.lb, index_and_bounds.ub, var_value),
        _ << "invalid highs_basis.col_status for variable with id: " << var_id);
    if (!status.has_value()) {
      return std::nullopt;
    }
    basis.mutable_variable_status()->add_ids(var_id);
    basis.mutable_variable_status()->add_values(*status);
  }
  for (const int64_t lin_con_id : SortedMapKeys(lin_con_data_)) {
    const IndexAndBound& index_and_bounds = lin_con_data_.at(lin_con_id);
    const double dual_value = highs_solution.row_dual[index_and_bounds.index];
    OR_ASSIGN_OR_RETURN3(
        const std::optional<BasisStatusProto> status,
        ToBasisStatus(highs_basis.row_status[index_and_bounds.index],
                      index_and_bounds.lb, index_and_bounds.ub, dual_value),
        _ << "invalid highs_basis.row_status for linear constraint with id: "
          << lin_con_id);
    if (!status.has_value()) {
      return std::nullopt;
    }
    basis.mutable_constraint_status()->add_ids(lin_con_id);
    basis.mutable_constraint_status()->add_values(*status);
  }
  return basis;
}

absl::StatusOr<bool> HighsSolver::PrimalRayReturned() const {
  if (!highs_->hasInvert()) {
    return false;
  }
  bool has_primal_ray = false;
  // Note getPrimalRay may return without modifying has_primal_ray, in which
  // case it will remain at its default false value.
  RETURN_IF_ERROR(ToStatus(highs_->getPrimalRay(has_primal_ray,
                                                /*primal_ray_value=*/nullptr)));
  return has_primal_ray;
}

absl::StatusOr<bool> HighsSolver::DualRayReturned() const {
  if (!highs_->hasInvert()) {
    return false;
  }
  bool has_dual_ray = false;
  // Note getPrimalRay may return without modifying has_dual_ray, in which
  // case it will remain at its default false value.
  RETURN_IF_ERROR(ToStatus(highs_->getDualRay(has_dual_ray,
                                              /*dual_ray_value=*/nullptr)));
  return has_dual_ray;
}

absl::StatusOr<HighsSolver::SolutionsAndClaims>
HighsSolver::ExtractSolutionAndRays(
    const ModelSolveParametersProto& model_params) {
  const HighsInfo& highs_info = highs_->getInfo();
  const HighsSolution& highs_solution = highs_->getSolution();
  SolutionsAndClaims solution_and_claims;
  if (highs_info.primal_solution_status == ::kSolutionStatusFeasible &&
      !highs_solution.value_valid) {
    return absl::InternalError(
        "highs_info.primal_solution_status==::kSolutionStatusFeasible, but no "
        "valid primal solution returned");
  }
  if (highs_solution.value_valid || highs_solution.dual_valid) {
    SolutionProto& solution =
        solution_and_claims.solutions.emplace_back(SolutionProto());
    if (highs_solution.value_valid) {
      RETURN_IF_ERROR(EnsureOneEntryPerVariable(highs_solution.col_value))
          << "invalid highs_solution.col_value";
      PrimalSolutionProto& primal_solution =
          *solution.mutable_primal_solution();
      primal_solution.set_objective_value(highs_info.objective_function_value);
      OR_ASSIGN_OR_RETURN3(const SolutionStatusProto primal_solution_status,
                           ToSolutionStatus(highs_info.primal_solution_status),
                           _ << "invalid highs_info.primal_solution_status");
      primal_solution.set_feasibility_status(primal_solution_status);
      solution_and_claims.solution_claims
          .highs_returned_primal_feasible_solution =
          primal_solution.feasibility_status() == SOLUTION_STATUS_FEASIBLE;
      for (const int64_t var_id : SortedMapKeys(variable_data_)) {
        primal_solution.mutable_variable_values()->add_ids(var_id);
        primal_solution.mutable_variable_values()->add_values(
            highs_solution.col_value[variable_data_.at(var_id).index]);
      }
    }
    if (highs_solution.dual_valid) {
      RETURN_IF_ERROR(EnsureOneEntryPerVariable(highs_solution.col_dual))
          << "invalid highs_solution.col_dual";
      RETURN_IF_ERROR(
          EnsureOneEntryPerLinearConstraint(highs_solution.row_dual))
          << "invalid highs_solution.row_dual";
      DualSolutionProto& dual_solution = *solution.mutable_dual_solution();
      dual_solution.set_objective_value(highs_info.objective_function_value);
      OR_ASSIGN_OR_RETURN3(const SolutionStatusProto dual_solution_status,
                           ToSolutionStatus(highs_info.dual_solution_status),
                           _ << "invalid highs_info.dual_solution_status");
      dual_solution.set_feasibility_status(dual_solution_status);
      solution_and_claims.solution_claims
          .highs_returned_dual_feasible_solution =
          dual_solution.feasibility_status() == SOLUTION_STATUS_FEASIBLE;
      for (const int64_t var_id : SortedMapKeys(variable_data_)) {
        dual_solution.mutable_reduced_costs()->add_ids(var_id);
        dual_solution.mutable_reduced_costs()->add_values(
            highs_solution.col_dual[variable_data_.at(var_id).index]);
      }
      for (const int64_t lin_con_id : SortedMapKeys(lin_con_data_)) {
        dual_solution.mutable_dual_values()->add_ids(lin_con_id);
        dual_solution.mutable_dual_values()->add_values(
            highs_solution.row_dual[lin_con_data_.at(lin_con_id).index]);
      }
    }
    ASSIGN_OR_RETURN(std::optional<BasisProto> basis_proto,
                     HighsSolver::ExtractBasis());
    if (basis_proto.has_value()) {
      *solution.mutable_basis() = *std::move(basis_proto);
    }
    ApplyAllFilters(model_params, solution);
  }

  ASSIGN_OR_RETURN(
      solution_and_claims.solution_claims.highs_returned_primal_ray,
      PrimalRayReturned());
  ASSIGN_OR_RETURN(solution_and_claims.solution_claims.highs_returned_dual_ray,
                   DualRayReturned());

  return solution_and_claims;
}

absl::StatusOr<std::unique_ptr<SolverInterface>> HighsSolver::New(
    const ModelProto& model, const InitArgs&) {
  RETURN_IF_ERROR(ModelIsSupported(model, kHighsSupportedStructures, "Highs"));
  HighsModel highs_model;
  HighsLp& lp = highs_model.lp_;
  lp.model_name_ = model.name();
  lp.objective_name_ = model.objective().name();
  const int num_vars = model.variables().ids_size();
  lp.num_col_ = num_vars;
  // NOTE: HiGHS issues a warning if lp.integrality_ is nonempty but all
  // variables are continuous. It would be nice to disable this warning, as we
  // should always just set this, otherwise incrementalism is just more
  // complicated.
  //
  // See
  // https://github.com/ERGO-Code/HiGHS/blob/master/src/lp_data/HighsLpUtils.cpp#L535
  bool has_integer_var = false;
  for (const bool is_integer : model.variables().integers()) {
    if (is_integer) {
      has_integer_var = true;
      break;
    }
  }

  absl::flat_hash_map<int64_t, IndexAndBound> variable_data;
  for (int i = 0; i < num_vars; ++i) {
    const double raw_lb = model.variables().lower_bounds(i);
    const double raw_ub = model.variables().upper_bounds(i);
    const IndexAndBound index_and_bound(/*index=*/i, /*lb=*/raw_lb,
                                        /*ub=*/raw_ub,
                                        model.variables().integers(i));
    variable_data.try_emplace(model.variables().ids(i), index_and_bound);
    lp.col_names_.push_back(
        model.variables().names_size() > 0 ? model.variables().names(i) : "");

    // If the bounds are crossed, we give an error at solve time (unless they
    // are uncrossed before the solve begins). Passing crossed bounds to HiGHS
    // here causes Highs:passModel() below to fail, but we don't want to fail
    // in New(). So we pass dummy values instead temporarily.
    // TODO(b/271595607): once HiGHS is updated, check if the unrounded bounds
    // cross instead.
    if (index_and_bound.rounded_bounds_cross()) {
      lp.col_lower_.push_back(0.0);
      lp.col_upper_.push_back(0.0);
    } else {
      // TODO(b/271595607): once HiGHS is updated, pass the original bound, not
      // the rounded bound.
      lp.col_lower_.push_back(index_and_bound.rounded_lb());
      lp.col_upper_.push_back(index_and_bound.rounded_ub());
    }
    if (has_integer_var) {
      lp.integrality_.push_back(model.variables().integers(i)
                                    ? HighsVarType::kInteger
                                    : HighsVarType::kContinuous);
    }
  }
  lp.offset_ = model.objective().offset();
  lp.sense_ =
      model.objective().maximize() ? ObjSense::kMaximize : ObjSense::kMinimize;
  lp.col_cost_.resize(num_vars);
  for (const auto [var_id, lin_obj] :
       MakeView(model.objective().linear_coefficients())) {
    lp.col_cost_[variable_data.at(var_id).index] = lin_obj;
  }

  const int num_lin_cons = model.linear_constraints().ids_size();
  lp.num_row_ = num_lin_cons;
  absl::flat_hash_map<int64_t, IndexAndBound> lin_con_data;
  for (int i = 0; i < num_lin_cons; ++i) {
    const double lb = model.linear_constraints().lower_bounds(i);
    const double ub = model.linear_constraints().upper_bounds(i);
    lin_con_data.try_emplace(model.linear_constraints().ids(i),
                             IndexAndBound(/*index=*/i, /*lb=*/lb, /*ub=*/ub,
                                           /*is_integer=*/false));
    lp.row_names_.push_back(model.linear_constraints().names_size() > 0
                                ? model.linear_constraints().names(i)
                                : "");
    // See comment above for the case when a variable lb > ub, we need to avoid
    // an immediate error in New().
    if (lb > ub) {
      lp.row_lower_.push_back(0.0);
      lp.row_upper_.push_back(0.0);
    } else {
      lp.row_lower_.push_back(lb);
      lp.row_upper_.push_back(ub);
    }
  }
  lp.a_matrix_.format_ = MatrixFormat::kRowwise;
  lp.a_matrix_.num_col_ = num_vars;
  lp.a_matrix_.num_row_ = num_lin_cons;
  lp.a_matrix_.start_.clear();  // This starts out as {0} by default.
  const SparseDoubleMatrixProto& lin_con_mat = model.linear_constraint_matrix();
  int mat_index = 0;
  for (int highs_con = 0; highs_con < lin_con_data.size(); highs_con++) {
    lp.a_matrix_.start_.push_back(mat_index);
    while (mat_index < lin_con_mat.row_ids_size() &&
           lin_con_data.at(lin_con_mat.row_ids(mat_index)).index <= highs_con) {
      mat_index++;
    }
  }
  lp.a_matrix_.start_.push_back(lin_con_mat.row_ids_size());
  for (int i = 0; i < lin_con_mat.row_ids_size(); ++i) {
    const int var = variable_data.at(lin_con_mat.column_ids(i)).index;
    const double coef = lin_con_mat.coefficients(i);
    lp.a_matrix_.index_.push_back(var);
    lp.a_matrix_.value_.push_back(coef);
  }
  auto highs = std::make_unique<Highs>();
  // Disable output immediately, calling passModel() below will generate output
  // otherwise.
  HighsOptions disable_output;
  disable_output.output_flag = false;
  disable_output.log_to_console = false;
  RETURN_IF_ERROR(ToStatus(highs->passOptions(disable_output)));
  RETURN_IF_ERROR(ToStatus(highs->passModel(std::move(highs_model))));
  return absl::WrapUnique(new HighsSolver(
      std::move(highs), std::move(variable_data), std::move(lin_con_data)));
}

absl::StatusOr<SolveResultProto> HighsSolver::Solve(
    const SolveParametersProto& parameters,
    const ModelSolveParametersProto& model_parameters,
    MessageCallback message_cb, const CallbackRegistrationProto&, Callback,
    const SolveInterrupter* const) {
  RETURN_IF_ERROR(ModelSolveParametersAreSupported(
      model_parameters, kHighsSupportedStructures, "Highs"));
  const absl::Time start = absl::Now();
  auto set_solve_time = [&start](SolveResultProto& result) -> absl::Status {
    const absl::Duration solve_time = absl::Now() - start;
    OR_ASSIGN_OR_RETURN3(*result.mutable_solve_stats()->mutable_solve_time(),
                         util_time::EncodeGoogleApiProto(solve_time),
                         _ << "error encoding solve_stats.solve_time");
    return absl::OkStatus();
  };

  if (model_parameters.solution_hints_size() > 0) {
    // Take the first solution hint and set the solution.
    const SolutionHintProto& hint = model_parameters.solution_hints(0);
    HighsInt num_entries = hint.variable_values().ids_size();
    std::vector<HighsInt> index(num_entries);
    std::vector<double> value(num_entries);
    size_t i = 0;
    for (const auto [id, val] : MakeView(hint.variable_values())) {
      index[i] = variable_data_.at(id).index;
      value[i] = val;
      ++i;
    }
    RETURN_IF_ERROR(ToStatus(highs_->setSolution(num_entries, index.data(), value.data())));
  }

  RETURN_IF_ERROR(ListInvertedBounds().ToStatus());
  // TODO(b/271595607): delete this code once we upgrade HiGHS, if HiGHS does
  // return a proper infeasibility status for models with empty integer bounds.
  const bool is_maximize = highs_->getModel().lp_.sense_ == ObjSense::kMaximize;
  for (const auto& [var_id, bounds] : variable_data_) {
    if (bounds.rounded_bounds_cross()) {
      SolveResultProto result =
          ResultForIntegerInfeasible(is_maximize, var_id, bounds.lb, bounds.ub);
      RETURN_IF_ERROR(set_solve_time(result));
      return result;
    }
  }

  BufferedMessageCallback buffered_message_callback(std::move(message_cb));
  if (buffered_message_callback.has_user_message_callback()) {
    RETURN_IF_ERROR(ToStatus(
        highs_->setLogCallback(&HighsLogCallback, &buffered_message_callback)))
        << "failed to register logging callback";
  }
  auto message_cb_cleanup =
      absl::MakeCleanup([this, &buffered_message_callback]() {
        if (buffered_message_callback.has_user_message_callback()) {
          // As of March 6th, 2023, this code never returns an error (see the
          // HiGHS source). If we really want to be able to recover from errors,
          // more care is needed, as we need to prevent HiGHS from invoking the
          // user callback after this function, since it will not be alive (e.g.
          // wrap the user callback in a new callback that is guarded by an
          // atomic bool that we disable here). Further, to propagate this
          // error, we need a class instead of absl::Cleanup.
          CHECK_OK(ToStatus(highs_->setLogCallback(nullptr, nullptr)));
          buffered_message_callback.Flush();
        }
      });

  bool is_integer = false;
  // NOTE: lp_.integrality_ may be empty if the problem is an LP.
  for (const HighsVarType var_type : highs_->getModel().lp_.integrality_) {
    if (var_type == HighsVarType::kInteger) {
      is_integer = true;
      break;
    }
  }
  auto it = parameters.highs().bool_options().find("solve_relaxation");
  if (it != parameters.highs().bool_options().end() && it->second) {
    is_integer = false;
  }
  ASSIGN_OR_RETURN(
      const std::unique_ptr<HighsOptions> options,
      MakeOptions(parameters,
                  buffered_message_callback.has_user_message_callback(),
                  is_integer));
  RETURN_IF_ERROR(ToStatus(highs_->passOptions(*options)));
  RETURN_IF_ERROR(ToStatus(highs_->run()));
  std::move(message_cb_cleanup).Invoke();
  // When the model is empty, highs_->getInfo() is invalid, so we bail out.
  if (highs_->getModelStatus() == HighsModelStatus::kModelEmpty) {
    SolveResultProto result = ResultForHighsModelStatusModelEmpty(
        is_maximize, highs_->getModel().lp_.offset_, lin_con_data_);
    RETURN_IF_ERROR(set_solve_time(result));
    return result;
  }
  const HighsInfo& info = highs_->getInfo();
  if (!info.valid) {
    return absl::InternalError("HighsInfo not valid");
  }

  SolveResultProto result;
  ASSIGN_OR_RETURN(SolutionsAndClaims solutions_and_claims,
                   ExtractSolutionAndRays(model_parameters));
  for (SolutionProto& solution : solutions_and_claims.solutions) {
    *result.add_solutions() = std::move(solution);
  }
  ASSIGN_OR_RETURN(*result.mutable_termination(),
                   MakeTermination(highs_->getModelStatus(), info, is_integer,
                                   parameters.has_node_limit(),
                                   parameters.has_solution_limit(), is_maximize,
                                   solutions_and_claims.solution_claims));

  ASSIGN_OR_RETURN(*result.mutable_solve_stats(), ToSolveStats(info));

  RETURN_IF_ERROR(set_solve_time(result));
  return result;
}

absl::StatusOr<bool> HighsSolver::Update(const ModelUpdateProto&) {
  return false;
}

absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
HighsSolver::ComputeInfeasibleSubsystem(const SolveParametersProto&,
                                        MessageCallback,
                                        const SolveInterrupter*) {
  return absl::UnimplementedError(
      "HiGHS does not provide a method to compute an infeasible subsystem");
}

MATH_OPT_REGISTER_SOLVER(SOLVER_TYPE_HIGHS, HighsSolver::New);

}  // namespace operations_research::math_opt
