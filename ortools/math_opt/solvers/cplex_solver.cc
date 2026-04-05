// Copyright 2010-2026 Google LLC
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

#include "ortools/math_opt/solvers/cplex_solver.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/nullability.h"
#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "google/protobuf/repeated_ptr_field.h"
#include "ortools/base/map_util.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/callback.pb.h"
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
#include "ortools/math_opt/solvers/cplex.pb.h"
#include "ortools/math_opt/solvers/cplex/g_cplex.h"
#include "ortools/math_opt/solvers/message_callback_data.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/callback_validator.h"
#include "ortools/port/proto_utils.h"
#include "ortools/util/solve_interrupter.h"

namespace operations_research {
namespace math_opt {
absl::StatusOr<CplexVersion> ParseCplexVersion(absl::string_view version_str) {
  CplexVersion version;
  std::vector<std::string> parts = absl::StrSplit(version_str, '.');

  if (parts.size() < 3) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid CPLEX version string: ", version_str));
  }

  if (!absl::SimpleAtoi(parts[0], &version.major) ||
      !absl::SimpleAtoi(parts[1], &version.minor) ||
      !absl::SimpleAtoi(parts[2], &version.revision)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid CPLEX version components: ", version_str));
  }

  if (parts.size() > 3) {
    if (!absl::SimpleAtoi(parts[3], &version.subrevision)) {
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid CPLEX subrevision: ", version_str));
    }
  }

  return version;
}

bool CplexSupportsObjectiveLimit() {
  auto cplex_or = Cplex::New("check_version");
  if (!cplex_or.ok()) {
    // If we can't load CPLEX, we probably can't run tests either, but maybe we
    // shouldn't crash here. However, if the test suite runs, it expects CPLEX
    // to work.
    LOG(WARNING) << "Failed to load CPLEX to check version: "
                 << cplex_or.status();
    return false;
  }
  auto version_str_or = cplex_or.value()->Version();
  if (!version_str_or.ok()) return false;

  auto version_or = ParseCplexVersion(*version_str_or);
  if (!version_or.ok()) return false;

  return *version_or >= CplexVersion{21, 1, 0};
}

namespace {

constexpr SupportedProblemStructures kCplexSupportedStructures = {
    .integer_variables = SupportType::kSupported,
    .multi_objectives = SupportType::kNotImplemented,
    .quadratic_objectives = SupportType::kNotImplemented,
    .quadratic_constraints = SupportType::kNotImplemented,
    .second_order_cone_constraints = SupportType::kNotImplemented,
    .sos1_constraints = SupportType::kNotImplemented,
    .sos2_constraints = SupportType::kNotImplemented,
    .indicator_constraints = SupportType::kNotImplemented};

template <typename TOutput, typename TSpanValue, typename TCheckFn>
static absl::StatusOr<TOutput> CheckCopySpan(
    const absl::Span<const TSpanValue> in, TCheckFn check_fn) {
  TOutput out;
  out.reserve(in.size());

  for (const auto& item : in) {
    RETURN_IF_ERROR(check_fn(item));
    out.push_back(item);
  }

  return out;
}

template <typename T>
static void AddCplexParameterProto(CplexParametersProto& parameters,
                                   absl::string_view name, T value) {
  CplexParametersProto::Parameter* const parameter =
      parameters.add_parameters();

  auto set_fields = [&](auto* typed_param) {
    typed_param->set_name(name);
    typed_param->set_value(value);
  };

  if constexpr (std::is_same_v<T, double>)
    set_fields(parameter->mutable_parameter_double());
  else if constexpr (std::is_same_v<T, bool>)
    set_fields(parameter->mutable_parameter_bool());
  else if constexpr (std::is_same_v<T, int32_t>)
    set_fields(parameter->mutable_parameter_int32());
  else if constexpr (std::is_same_v<T, int64_t>)
    set_fields(parameter->mutable_parameter_int64());
  else if constexpr (std::is_same_v<T, absl::string_view>)
    set_fields(parameter->mutable_parameter_string());
}

// is_mip indicates if the problem has integer variables or constraints that
// would cause Cplex to treat the problem as a MIP, e.g. SOS, indicator.
absl::StatusOr<CplexParametersProto> MergeParameters(
    const SolveParametersProto& solve_parameters,
    const ModelSolveParametersProto& model_parameters, const bool is_mip,
    const bool is_minimization, const bool supports_objective_limit) {
  CplexParametersProto merged_parameters;

  {
    AddCplexParameterProto<bool>(
        merged_parameters, "CPXPARAM_ScreenOutput",
        solve_parameters.enable_output() ? true : false);
  }

  {
    // Default: show parameter-change messages.
    // Overridden to false in Solve() when a message callback is registered.
    // Including it here ensures it is restored on each Solve() call, preventing
    // the override from leaking across calls.
    AddCplexParameterProto<bool>(
        merged_parameters, "CPXPARAM_ParamDisplay", true);
  }

  {
    absl::Duration time_limit = absl::InfiniteDuration();
    if (solve_parameters.has_time_limit()) {
      ASSIGN_OR_RETURN(time_limit, util_time::DecodeGoogleApiProto(
                                       solve_parameters.time_limit()));
    }
    if (time_limit < absl::InfiniteDuration()) {
      AddCplexParameterProto<double>(merged_parameters, "CPXPARAM_TimeLimit",
                                     absl::ToDoubleSeconds(time_limit));
    }
  }

  if (solve_parameters.has_node_limit()) {
    AddCplexParameterProto<int64_t>(merged_parameters,
                                    "CPXPARAM_MIP_Limits_Nodes",
                                    solve_parameters.node_limit());
  }

  if (solve_parameters.has_threads()) {
    AddCplexParameterProto<int32_t>(merged_parameters, "CPXPARAM_Threads",
                                    solve_parameters.threads());
  }

  if (solve_parameters.has_absolute_gap_tolerance()) {
    AddCplexParameterProto<double>(merged_parameters,
                                   "CPXPARAM_MIP_Tolerances_AbsMIPGap",
                                   solve_parameters.absolute_gap_tolerance());
  }

  if (solve_parameters.has_relative_gap_tolerance()) {
    // CPLEX's MIPGap parameter range is [0, 1]. Values > 1.0 are semantically
    // equivalent to 1.0 (accept any feasible solution), so we clamp to stay
    // within CPLEX's accepted range. Returning invalid argument would be
    // preferred but test-suite explicitly uses values > 1.0.
    const double gap = std::min(solve_parameters.relative_gap_tolerance(), 1.0);
    AddCplexParameterProto<double>(merged_parameters,
                                   "CPXPARAM_MIP_Tolerances_MIPGap", gap);
  }

  if (solve_parameters.has_cutoff_limit()) {
    if (!is_mip) {
      return absl::InvalidArgumentError(
          "cutoff_limit is only supported for Cplex on MIP models");
    }

    if (is_minimization) {
      AddCplexParameterProto<double>(merged_parameters,
                                     "CPXPARAM_MIP_Tolerances_UpperCutoff",
                                     solve_parameters.cutoff_limit());
    } else {
      AddCplexParameterProto<double>(merged_parameters,
                                     "CPXPARAM_MIP_Tolerances_LowerCutoff",
                                     solve_parameters.cutoff_limit());
    }
  }

  if (solve_parameters.has_objective_limit()) {
    if (!supports_objective_limit) {
      return absl::InvalidArgumentError(
          "objective_limit is not supported for CPLEX");
    }

    const double limit = solve_parameters.objective_limit();
    if (is_mip) {
      if (is_minimization) {
        AddCplexParameterProto<double>(
            merged_parameters, "CPXPARAM_MIP_Limits_LowerObjStop", limit);
      } else {
        AddCplexParameterProto<double>(
            merged_parameters, "CPXPARAM_MIP_Limits_UpperObjStop", limit);
      }
    } else {
      if (is_minimization) {
        AddCplexParameterProto<double>(
            merged_parameters, "CPXPARAM_Simplex_Limits_LowerObj", limit);
      } else {
        AddCplexParameterProto<double>(
            merged_parameters, "CPXPARAM_Simplex_Limits_UpperObj", limit);
      }
    }
  }

  if (solve_parameters.has_best_bound_limit()) {
    return absl::InvalidArgumentError(
        "best_bound_limit is currently not supported for CPLEX");
  }

  if (solve_parameters.has_solution_limit()) {
    AddCplexParameterProto<int64_t>(merged_parameters,
                                    "CPXPARAM_MIP_Limits_Solutions",
                                    solve_parameters.solution_limit());
  }

  if (solve_parameters.has_random_seed()) {
    const int random_seed =
        std::min(CPX_BIGINT, std::max(solve_parameters.random_seed(), 0));
    AddCplexParameterProto<int32_t>(merged_parameters, "CPXPARAM_RandomSeed",
                                    random_seed);
  }

  if (solve_parameters.has_solution_pool_size()) {
    AddCplexParameterProto<int32_t>(merged_parameters,
                                    "CPXPARAM_MIP_Pool_Capacity",
                                    solve_parameters.solution_pool_size());
  }

  if (solve_parameters.lp_algorithm() != LP_ALGORITHM_UNSPECIFIED) {
    int cplex_lp_method = 0;
    switch (solve_parameters.lp_algorithm()) {
      case LP_ALGORITHM_PRIMAL_SIMPLEX:
        cplex_lp_method = CPX_ALG_PRIMAL;
        break;
      case LP_ALGORITHM_DUAL_SIMPLEX:
        cplex_lp_method = CPX_ALG_DUAL;
        break;
      case LP_ALGORITHM_BARRIER:
        cplex_lp_method = CPX_ALG_BARRIER;
        break;
      case LP_ALGORITHM_FIRST_ORDER:
        return absl::InvalidArgumentError(
            "lp_algorithm FIRST_ORDER is not supported for cplex");
      default:
        LOG(FATAL) << "LPAlgorithm: "
                   << ProtoEnumToString(solve_parameters.lp_algorithm())
                   << " unknown, error setting Cplex parameters";
    }

    AddCplexParameterProto<int32_t>(merged_parameters, "CPXPARAM_LPMethod",
                                    cplex_lp_method);

    if (is_mip) {
      AddCplexParameterProto<int32_t>(merged_parameters,
                                      "CPXPARAM_MIP_Strategy_StartAlgorithm",
                                      cplex_lp_method);
      AddCplexParameterProto<int32_t>(merged_parameters,
                                      "CPXPARAM_MIP_Strategy_SubAlgorithm",
                                      cplex_lp_method);
    }
  }

  if (solve_parameters.scaling() != EMPHASIS_UNSPECIFIED) {
    switch (solve_parameters.scaling()) {
      case EMPHASIS_OFF:
        AddCplexParameterProto<int32_t>(merged_parameters,
                                        "CPXPARAM_Read_Scale", -1);
        break;
      case EMPHASIS_LOW:
      case EMPHASIS_MEDIUM:
        AddCplexParameterProto<int32_t>(merged_parameters,
                                        "CPXPARAM_Read_Scale", 0);
        break;
      case EMPHASIS_HIGH:
      case EMPHASIS_VERY_HIGH:
        AddCplexParameterProto<int32_t>(merged_parameters,
                                        "CPXPARAM_Read_Scale", 1);
        break;
      default:
        LOG(FATAL) << "Scaling emphasis: "
                   << ProtoEnumToString(solve_parameters.scaling())
                   << " unknown, error setting Cplex parameters";
    }
  }

  // CPLEX does not offer a global parameter for all cuts in the C API
  if (solve_parameters.cuts() != EMPHASIS_UNSPECIFIED) {
    int cplex_cut_level = 0;  // Default to Auto (0)
    switch (solve_parameters.cuts()) {
      case EMPHASIS_OFF:
        cplex_cut_level = -1;  // CPLEX Off
        break;
      case EMPHASIS_LOW:
      case EMPHASIS_MEDIUM:
        cplex_cut_level = 1;  // CPLEX Moderate
        break;
      case EMPHASIS_HIGH:
        cplex_cut_level = 2;  // CPLEX Aggressive
        break;
      case EMPHASIS_VERY_HIGH:
        cplex_cut_level = 3;  // CPLEX Very Aggressive
        break;
      default:
        LOG(FATAL) << "Cuts emphasis: "
                   << ProtoEnumToString(solve_parameters.cuts())
                   << " unknown, error setting Cplex parameters";
    }

    // List of all CPLEX cut parameters to update
    const std::vector<absl::string_view> cut_params = {
        "CPXPARAM_MIP_Cuts_BQP",        "CPXPARAM_MIP_Cuts_Cliques",
        "CPXPARAM_MIP_Cuts_Covers",     "CPXPARAM_MIP_Cuts_Disjunctive",
        "CPXPARAM_MIP_Cuts_FlowCovers", "CPXPARAM_MIP_Cuts_PathCut",
        "CPXPARAM_MIP_Cuts_Gomory",     "CPXPARAM_MIP_Cuts_GUBCovers",
        "CPXPARAM_MIP_Cuts_Implied",    "CPXPARAM_MIP_Cuts_LocalImplied",
        "CPXPARAM_MIP_Cuts_LiftProj",   "CPXPARAM_MIP_Cuts_MIRCut",
        "CPXPARAM_MIP_Cuts_MCFCut",     "CPXPARAM_MIP_Cuts_Nodecuts",
        "CPXPARAM_MIP_Cuts_RLT",        "CPXPARAM_MIP_Cuts_ZeroHalfCut"};

    for (absl::string_view param_name : cut_params) {
      AddCplexParameterProto<int32_t>(merged_parameters, param_name,
                                      cplex_cut_level);
    }
  }

  // CPLEX uses an effort multiplier
  // Important: "The behavior of CPLEX is undefined if both heuristic effort and
  // heuristic frequency are set to non-default values."
  if (solve_parameters.heuristics() != EMPHASIS_UNSPECIFIED) {
    switch (solve_parameters.heuristics()) {
      case EMPHASIS_OFF:
        AddCplexParameterProto<double>(
            merged_parameters, "CPXPARAM_MIP_Strategy_HeuristicEffort", 0.0);
        break;
      case EMPHASIS_LOW:
        AddCplexParameterProto<double>(
            merged_parameters, "CPXPARAM_MIP_Strategy_HeuristicEffort", 0.5);
        break;
      case EMPHASIS_MEDIUM:
        AddCplexParameterProto<double>(merged_parameters,
                                       "CPXPARAM_MIP_Strategy_HeuristicEffort",
                                       1.0);  // default
        break;
      case EMPHASIS_HIGH:
        AddCplexParameterProto<double>(
            merged_parameters, "CPXPARAM_MIP_Strategy_HeuristicEffort", 2.0);
        break;
      case EMPHASIS_VERY_HIGH:
        AddCplexParameterProto<double>(
            merged_parameters, "CPXPARAM_MIP_Strategy_HeuristicEffort", 4.0);
        break;
      default:
        LOG(FATAL) << "Heuristics emphasis: "
                   << ProtoEnumToString(solve_parameters.heuristics())
                   << " unknown, error setting CPLEX parameters";
    }
  }

  // CPLEX does not offer a global parameter in the C API
  if (solve_parameters.presolve() != EMPHASIS_UNSPECIFIED) {
    int cplex_presolve_level = 0;
    switch (solve_parameters.presolve()) {
      case EMPHASIS_OFF:
        cplex_presolve_level = -1;
        break;
      case EMPHASIS_LOW:
        cplex_presolve_level = 0;
        break;
      case EMPHASIS_MEDIUM:
        cplex_presolve_level = 1;
        break;
      case EMPHASIS_HIGH:
        cplex_presolve_level = 2;
        break;
      case EMPHASIS_VERY_HIGH:
        cplex_presolve_level = 3;
        break;
      default:
        LOG(FATAL) << "Presolve emphasis: "
                   << ProtoEnumToString(solve_parameters.presolve())
                   << " unknown, error setting CPLEX parameters";
    }

    switch (cplex_presolve_level) {
      case -1:
        AddCplexParameterProto<bool>(merged_parameters,
                                     "CPXPARAM_Preprocessing_Presolve",
                                     false);  // off
        AddCplexParameterProto<int32_t>(merged_parameters,
                                        "CPXPARAM_Preprocessing_Aggregator",
                                        0);  // off
        AddCplexParameterProto<int32_t>(merged_parameters,
                                        "CPXPARAM_MIP_Strategy_Probe",
                                        -1);  // off
        AddCplexParameterProto<int32_t>(merged_parameters,
                                        "CPXPARAM_Preprocessing_RepeatPresolve",
                                        0);  // off
        break;
      case 0:
        AddCplexParameterProto<bool>(merged_parameters,
                                     "CPXPARAM_Preprocessing_Presolve",
                                     true);  // on (default)
        AddCplexParameterProto<int32_t>(merged_parameters,
                                        "CPXPARAM_MIP_Strategy_Probe",
                                        -1);  // off
        AddCplexParameterProto<int32_t>(merged_parameters,
                                        "CPXPARAM_Preprocessing_RepeatPresolve",
                                        0);  // off
        break;
      case 1:
        AddCplexParameterProto<bool>(merged_parameters,
                                     "CPXPARAM_Preprocessing_Presolve",
                                     true);  // on (default)
        AddCplexParameterProto<int32_t>(merged_parameters,
                                        "CPXPARAM_MIP_Strategy_Probe",
                                        1);  // moderate
        AddCplexParameterProto<int32_t>(merged_parameters,
                                        "CPXPARAM_Preprocessing_RepeatPresolve",
                                        1);  // represolve wo. cuts
        break;
      case 2:
        AddCplexParameterProto<bool>(merged_parameters,
                                     "CPXPARAM_Preprocessing_Presolve",
                                     true);  // on (default)
        AddCplexParameterProto<int32_t>(merged_parameters,
                                        "CPXPARAM_MIP_Strategy_Probe",
                                        2);  // aggressive
        AddCplexParameterProto<int32_t>(merged_parameters,
                                        "CPXPARAM_Preprocessing_RepeatPresolve",
                                        2);  // represolve w. cuts
        break;
      case 3:
        AddCplexParameterProto<bool>(merged_parameters,
                                     "CPXPARAM_Preprocessing_Presolve",
                                     true);  // on (default)
        AddCplexParameterProto<int32_t>(merged_parameters,
                                        "CPXPARAM_MIP_Strategy_Probe",
                                        3);  // very aggressive
        AddCplexParameterProto<int32_t>(
            merged_parameters, "CPXPARAM_Preprocessing_RepeatPresolve",
            3);  // represolve w. cuts and allow new root cuts
        break;
      default:
        LOG(FATAL) << "Presolve emphasis: "
                   << ProtoEnumToString(solve_parameters.presolve())
                   << " unknown, error setting CPLEX parameters";
    }
  }

  if (solve_parameters.has_iteration_limit()) {
    AddCplexParameterProto<int64_t>(merged_parameters,
                                    "CPXPARAM_Simplex_Limits_Iterations",
                                    solve_parameters.iteration_limit());

    AddCplexParameterProto<int64_t>(merged_parameters,
                                    "CPXPARAM_Barrier_Limits_Iteration",
                                    solve_parameters.iteration_limit());
  }

  for (const CplexParametersProto::Parameter& parameter :
       solve_parameters.cplex().parameters()) {
    *merged_parameters.add_parameters() = parameter;
  }

  return merged_parameters;
}

// Be safe and introduce a char limit.
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

absl::Status SafeCplexDouble(const double d) {
  if (std::isfinite(d) && std::abs(d) >= CPX_INFBOUND) {
    return util::InvalidArgumentErrorBuilder()
           << "finite value: " << d << " will be treated as infinite by CPLEX";
  }
  return absl::OkStatus();
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

absl::StatusOr<std::unique_ptr<Cplex>> CplexFromInitArgs(
    const SolverInterface::InitArgs& init_args) {
  if (init_args.non_streamable != nullptr) {
    return absl::InvalidArgumentError(
        "CPLEX support in MathOpt does not currently accept non-streamable "
        "init arguments (e.g. pre-existing CPXENVptr).");
  }
  return Cplex::New("cplex_model");
}

void CPXPUBLIC MessageCallbackImpl(void* handle, const char* message) {
  auto* const buffered_message_cb =
      static_cast<BufferedMessageCallback*>(handle);
  if (message != nullptr && buffered_message_cb != nullptr) {
    buffered_message_cb->OnMessage(message);
  }
}

}  // namespace

CplexSolver::CplexSolver(std::unique_ptr<Cplex> g_cplex)
    : cplex_(std::move(g_cplex)) {}

CplexSolver::CplexModelElements
CplexSolver::LinearConstraintData::DependentElements() const {
  CplexModelElements elements;
  CHECK_NE(constraint_index, kUnspecifiedConstraint);
  elements.linear_constraints.push_back(constraint_index);
  return elements;
}

absl::StatusOr<TerminationProto> CplexSolver::ConvertTerminationReason(
    const int cplex_status, const bool had_cutoff,
    const bool had_iteration_limit, const bool had_objective_limit,
    const SolutionClaims solution_claims, const double best_primal_bound,
    const double best_dual_bound) {
  ASSIGN_OR_RETURN(const bool is_maximize, IsMaximize());

  switch (cplex_status) {
    case 1:  // CPX_STAT_OPTIMAL
      return OptimalTerminationProto(best_primal_bound, best_dual_bound);
    case 2:  // CPX_STAT_UNBOUNDED
      if (solution_claims.primal_feasible_solution_exists) {
        return UnboundedTerminationProto(is_maximize);
      }
      return InfeasibleOrUnboundedTerminationProto(
          is_maximize,
          /*dual_feasibility_status=*/FEASIBILITY_STATUS_INFEASIBLE,
          "Cplex status CPX_STAT_UNBOUNDED");
    case 3:  // CPX_STAT_INFEASIBLE
      if (had_cutoff) {
        // CPLEX has no dedicated cutoff status code (unlike Gurobi's
        // GRB_CUTOFF). When a cutoff is active and CPLEX returns infeasible,
        // we cannot distinguish "truly infeasible" from "no solution better
        // than cutoff." We assume cutoff termination, matching the gSCIP
        // approach for API conformance.
        return CutoffTerminationProto(is_maximize,
                                      "Cplex status CPX_STAT_INFEASIBLE");
      }
      return InfeasibleTerminationProto(
          is_maximize, solution_claims.dual_feasible_solution_exists
                           ? FEASIBILITY_STATUS_FEASIBLE
                           : FEASIBILITY_STATUS_UNDETERMINED);
    case 4:  // CPX_STAT_INForUNBD
      return InfeasibleOrUnboundedTerminationProto(
          is_maximize,
          /*dual_feasibility_status=*/FEASIBILITY_STATUS_UNDETERMINED,
          "Cplex status CPX_STAT_INForUNBD");
    case 5:  // CPX_STAT_OPTIMAL_INFEAS
      return TerminateForReason(is_maximize, TERMINATION_REASON_IMPRECISE);
    case 6:  // CPX_STAT_NUM_BEST
      return TerminateForReason(is_maximize, TERMINATION_REASON_IMPRECISE);
    case 10:  // CPX_STAT_ABORT_IT_LIM
      return LimitTerminationProto(
          LIMIT_ITERATION, best_primal_bound, best_dual_bound,
          solution_claims.dual_feasible_solution_exists);
    case 11:  // CPX_STAT_ABORT_TIME_LIM
      return LimitTerminationProto(
          LIMIT_TIME, best_primal_bound, best_dual_bound,
          solution_claims.dual_feasible_solution_exists);
    case 12:  // CPX_STAT_ABORT_OBJ_LIM
      return LimitTerminationProto(
          LIMIT_OBJECTIVE, best_primal_bound, best_dual_bound,
          solution_claims.dual_feasible_solution_exists);
    case 13:  // CPX_STAT_ABORT_USER
      return LimitTerminationProto(
          LIMIT_INTERRUPTED, best_primal_bound, best_dual_bound,
          solution_claims.dual_feasible_solution_exists);
    // IMPORTANT: Assumption -> never happening as we don't use it's source
    // "CPXfeasopt"
    case 14:  // CPX_STAT_FEASIBLE_RELAXED_SUM
    case 15:  // CPX_STAT_OPTIMAL_RELAXED_SUM
    case 16:  // CPX_STAT_FEASIBLE_RELAXED_INF
    case 17:  // CPX_STAT_OPTIMAL_RELAXED_INF
    case 18:  // CPX_STAT_FEASIBLE_RELAXED_QUAD
    case 19:  // CPX_STAT_OPTIMAL_RELAXED_QUAD
      return TerminateForReason(is_maximize, TERMINATION_REASON_OTHER_ERROR);
    case 20:  // CPX_STAT_OPTIMAL_FACE_UNBOUNDED
      // The optimal objective is attained but the solution set is unbounded
      // (multiple optimal solutions exist along a ray). This IS optimal.
      return OptimalTerminationProto(best_primal_bound, best_dual_bound);
    case 21:  // CPX_STAT_ABORT_PRIM_OBJ_LIM
      return LimitTerminationProto(
          LIMIT_OBJECTIVE, best_primal_bound, best_dual_bound,
          solution_claims.dual_feasible_solution_exists);
    case 22:  // CPX_STAT_ABORT_DUAL_OBJ_LIM
      return LimitTerminationProto(
          LIMIT_OBJECTIVE, best_primal_bound, best_dual_bound,
          solution_claims.dual_feasible_solution_exists);
    case 23:  // CPX_STAT_FEASIBLE
      // A feasible solution was found but optimality was not proven (e.g.,
      // deterministic time limit hit after Phase I). Not an error.
      return FeasibleTerminationProto(is_maximize, LIMIT_UNDETERMINED,
                                      best_primal_bound, best_dual_bound);
    // IMPORTANT: Assumption -> never happening as we don't use it's source
    // "CPXfeasopt"
    case 24:  // CPX_STAT_FIRSTORDER
      return TerminateForReason(is_maximize, TERMINATION_REASON_OTHER_ERROR);
    case 25:  // CPX_STAT_ABORT_DETTIME_LIM
      return LimitTerminationProto(
          LIMIT_TIME, best_primal_bound, best_dual_bound,
          solution_claims.dual_feasible_solution_exists);
    // IMPORTANT: Assumption -> never happening as we don't use it's source
    // "conflict refiner"
    case 30:  // CPX_STAT_CONFLICT_FEASIBLE
    case 31:  // CPX_STAT_CONFLICT_MINIMAL
    case 32:  // CPX_STAT_CONFLICT_ABORT_CONTRADICTION
    case 33:  // CPX_STAT_CONFLICT_ABORT_TIME_LIM
    case 34:  // CPX_STAT_CONFLICT_ABORT_IT_LIM
    case 35:  // CPX_STAT_CONFLICT_ABORT_NODE_LIM
    case 36:  // CPX_STAT_CONFLICT_ABORT_OBJ_LIM
    case 37:  // CPX_STAT_CONFLICT_ABORT_MEM_LIM
    case 38:  // CPX_STAT_CONFLICT_ABORT_USER
    case 39:  // CPX_STAT_CONFLICT_ABORT_DETTIME_LIM
    // IMPORTANT: Assumption -> never happening as we don't use it's source
    // "Benders decomposition"
    case 40:  // CPX_STAT_BENDERS_MASTER_UNBOUNDED
    case 41:  // CPX_STAT_BENDERS_NUM_BEST
    // IMPORTANT: Assumption -> never happening as we don't use it's source
    // "Multi-objective optimization"
    case 301:  // CPX_STAT_MULTIOBJ_OPTIMAL
    case 302:  // CPX_STAT_MULTIOBJ_INFEASIBLE
    case 303:  // CPX_STAT_MULTIOBJ_INForUNBD
    case 304:  // CPX_STAT_MULTIOBJ_UNBOUNDED
    case 305:  // CPX_STAT_MULTIOBJ_NON_OPTIMAL
    case 306:  // CPX_STAT_MULTIOBJ_STOPPED
      return TerminateForReason(is_maximize, TERMINATION_REASON_OTHER_ERROR);

    // MIP Status Codes
    case 101:  // CPXMIP_OPTIMAL
    case 102:  // CPXMIP_OPTIMAL_TOL
      return OptimalTerminationProto(best_primal_bound, best_dual_bound);
    case 103:  // CPXMIP_INFEASIBLE
      if (had_cutoff) {
        // Same cutoff ambiguity as CPX_STAT_INFEASIBLE (case 3) — see comment
        // there. CPLEX doesn't distinguish "truly infeasible" from "no solution
        // better than cutoff."
        return CutoffTerminationProto(is_maximize,
                                      "Cplex status CPXMIP_INFEASIBLE");
      }
      return InfeasibleTerminationProto(
          is_maximize, solution_claims.dual_feasible_solution_exists
                           ? FEASIBILITY_STATUS_FEASIBLE
                           : FEASIBILITY_STATUS_UNDETERMINED);
    case 104:  // CPXMIP_SOL_LIM
      return LimitTerminationProto(
          LIMIT_SOLUTION, best_primal_bound, best_dual_bound,
          solution_claims.dual_feasible_solution_exists);
    case 105:  // CPXMIP_NODE_LIM_FEAS
    case 106:  // CPXMIP_NODE_LIM_INFEAS
      return LimitTerminationProto(
          LIMIT_NODE, best_primal_bound, best_dual_bound,
          solution_claims.dual_feasible_solution_exists);
    case 107:  // CPXMIP_TIME_LIM_FEAS
    case 108:  // CPXMIP_TIME_LIM_INFEAS
      return LimitTerminationProto(
          LIMIT_TIME, best_primal_bound, best_dual_bound,
          solution_claims.dual_feasible_solution_exists);
    case 110:  // CPXMIP_FAIL_INFEAS
      if (had_iteration_limit) {
        // same as case 3: user-limit? high chance (but no guarantee) this
        // status is a result of that (e.g. limit hit in root relax)
        return LimitTerminationProto(
            LIMIT_ITERATION, best_primal_bound, best_dual_bound,
            solution_claims.dual_feasible_solution_exists,
            "Terminated because of an error; no integer solution. As "
            "iteration_limit was set, it's probably the reason");
      }
      // Numerical error with no feasible solution found.  This is NOT a proof
      // of infeasibility — the solver failed, it did not conclude infeasible.
      return TerminateForReason(is_maximize,
                                TERMINATION_REASON_NUMERICAL_ERROR);
    case 131:  // CPXMIP_DETTIME_LIM_FEAS
    case 132:  // CPXMIP_DETTIME_LIM_INFEAS
      return LimitTerminationProto(
          LIMIT_TIME, best_primal_bound, best_dual_bound,
          solution_claims.dual_feasible_solution_exists);
    case 111:  // CPXMIP_MEM_LIM_FEAS
    case 112:  // CPXMIP_MEM_LIM_INFEAS
    case 116:  // CPXMIP_FAIL_FEAS_NO_TREE
    case 117:  // CPXMIP_FAIL_INFEAS_NO_TREE
      return LimitTerminationProto(
          LIMIT_MEMORY, best_primal_bound, best_dual_bound,
          solution_claims.dual_feasible_solution_exists);
    case 113:  // CPXMIP_ABORT_FEAS
    case 114:  // CPXMIP_ABORT_INFEAS
      // CPLEX has no dedicated status code for objective limit termination in
      // MIP. When the objective limit triggers, CPLEX returns ABORT_FEAS/INFEAS
      // — the same codes used for user interrupts. We disambiguate by checking
      // whether an objective limit was set.
      return LimitTerminationProto(
          had_objective_limit ? LIMIT_OBJECTIVE : LIMIT_INTERRUPTED,
          best_primal_bound, best_dual_bound,
          solution_claims.dual_feasible_solution_exists);
    case 115:  // CPXMIP_OPTIMAL_INFEAS
      return TerminateForReason(is_maximize, TERMINATION_REASON_IMPRECISE);
    case 118:  // CPXMIP_UNBOUNDED
      if (solution_claims.primal_feasible_solution_exists) {
        return UnboundedTerminationProto(is_maximize);
      }
      return InfeasibleOrUnboundedTerminationProto(
          is_maximize,
          /*dual_feasibility_status=*/FEASIBILITY_STATUS_INFEASIBLE,
          "Cplex status CPXMIP_UNBOUNDED");
    case 119:  // CPXMIP_INForUNBD
      return InfeasibleOrUnboundedTerminationProto(
          is_maximize,
          /*dual_feasibility_status=*/FEASIBILITY_STATUS_UNDETERMINED,
          "Cplex status CPXMIP_INForUNBD");
    case 109:  // CPXMIP_FAIL_FEAS
      // Numerical error but a feasible incumbent exists. Analogous to case 110
      // but with a solution — preserve it via IMPRECISE rather than INFEASIBLE.
      return TerminateForReason(is_maximize, TERMINATION_REASON_IMPRECISE);
    case 120:  // CPXMIP_FEASIBLE_RELAXED_SUM
    case 121:  // CPXMIP_OPTIMAL_RELAXED_SUM
    case 122:  // CPXMIP_FEASIBLE_RELAXED_INF
    case 123:  // CPXMIP_OPTIMAL_RELAXED_INF
    case 124:  // CPXMIP_FEASIBLE_RELAXED_QUAD
    case 125:  // CPXMIP_OPTIMAL_RELAXED_QUAD
    case 126:  // CPXMIP_ABORT_RELAXED
      // FeasOpt results — the solver relaxed the model to find feasibility.
      // MathOpt does not invoke FeasOpt, so these should not arise in normal
      // use. Map to OTHER_ERROR to surface the unexpected state.
      return TerminateForReason(is_maximize, TERMINATION_REASON_OTHER_ERROR);
    case 127:  // CPXMIP_FEASIBLE
      // CPLEX found a feasible solution but did not prove optimality.
      return FeasibleTerminationProto(is_maximize, LIMIT_UNDETERMINED,
                                      best_primal_bound, best_dual_bound);
    case 128:  // CPXMIP_POPULATESOL_LIM
      return LimitTerminationProto(
          LIMIT_SOLUTION, best_primal_bound, best_dual_bound,
          solution_claims.dual_feasible_solution_exists);
    case 129:  // CPXMIP_OPTIMAL_POPULATED
    case 130:  // CPXMIP_OPTIMAL_POPULATED_TOL
      return OptimalTerminationProto(best_primal_bound, best_dual_bound);
    case 133:  // CPXMIP_ABORT_RELAXATION_UNBOUNDED
      return InfeasibleOrUnboundedTerminationProto(
          is_maximize,
          /*dual_feasibility_status=*/FEASIBILITY_STATUS_INFEASIBLE,
          "Cplex status CPXMIP_ABORT_RELAXATION_UNBOUNDED");

    default:
      return absl::InternalError(absl::StrCat(
          "Missing Cplex optimization status code case: ", cplex_status));
  }
}

absl::StatusOr<bool> CplexSolver::IsMaximize() const {
  CHECK(cplex_ != nullptr);

  ASSIGN_OR_RETURN(const int obj_sense, cplex_->GetObjSen());
  return obj_sense == CPX_MAX;
}

absl::StatusOr<bool> CplexSolver::IsMIP() const {
  CHECK(cplex_ != nullptr);

  ASSIGN_OR_RETURN(auto problem_type, cplex_->GetProbType());
  return problem_type == CPXPROB_MILP || problem_type == CPXPROB_FIXEDMILP;
}

absl::StatusOr<std::string> CplexSolver::Version() const {
  CHECK(cplex_ != nullptr);

  return cplex_->Version();
}

template <typename T>
void CplexSolver::CplexVectorToSparseDoubleVector(
    const absl::Span<const double> cplex_values, const T& map,
    SparseDoubleVectorProto& result,
    const SparseVectorFilterProto& filter) const {
  SparseVectorFilterPredicate predicate(filter);
  for (const auto& [id, cplex_data] : map) {
    const double value = cplex_values[get_model_index(cplex_data)];
    if (predicate.AcceptsAndUpdate(id, value)) {
      result.add_ids(id);
      result.add_values(value);
    }
  }
}

absl::StatusOr<SolveResultProto> CplexSolver::ExtractSolveResultProto(
    const absl::Time start, const ModelSolveParametersProto& model_parameters,
    const bool had_cutoff, const bool had_iteration_limit,
    const bool had_objective_limit) {
  SolveResultProto result;

  ASSIGN_OR_RETURN(auto cplex_stat, cplex_->GetStat());

  SolutionClaims solution_claims;
  ASSIGN_OR_RETURN(SolutionsAndClaims solution_and_claims,
                   GetSolutions(model_parameters));

  ASSIGN_OR_RETURN(const double best_primal_bound,
                   GetBestPrimalBound(solution_and_claims.solutions));
  ASSIGN_OR_RETURN(const double best_dual_bound,
                   GetBestDualBound(solution_and_claims.solutions));
  solution_claims = solution_and_claims.solution_claims;

  for (SolutionProto& solution : solution_and_claims.solutions) {
    *result.add_solutions() = std::move(solution);
  }

  ASSIGN_OR_RETURN(
      *result.mutable_termination(),
      ConvertTerminationReason(cplex_stat, had_cutoff, had_iteration_limit,
                               had_objective_limit, solution_claims,
                               best_primal_bound, best_dual_bound));

  ASSIGN_OR_RETURN(*result.mutable_solve_stats(), GetSolveStats(start));
  return result;
}

absl::StatusOr<CplexSolver::SolutionsAndClaims> CplexSolver::GetSolutions(
    const ModelSolveParametersProto& model_parameters) {
  ASSIGN_OR_RETURN(const bool is_mip, IsMIP());

  if (is_mip) {
    return GetMipSolutions(model_parameters);
  } else {
    return GetLpSolution(model_parameters);
  }
}

absl::StatusOr<SolveStatsProto> CplexSolver::GetSolveStats(
    const absl::Time start) const {
  SolveStatsProto solve_stats;

  CHECK_OK(util_time::EncodeGoogleApiProto(absl::Now() - start,
                                           solve_stats.mutable_solve_time()));

  ASSIGN_OR_RETURN(auto problem_type, cplex_->GetProbType());

  int simplex_iters = 0;
  if (problem_type == CPXPROB_MILP) {
    ASSIGN_OR_RETURN(simplex_iters, cplex_->GetMipItCnt());
  } else {
    ASSIGN_OR_RETURN(simplex_iters, cplex_->GetItCnt());
  }
  solve_stats.set_simplex_iterations(simplex_iters);

  ASSIGN_OR_RETURN(int barrier_iters, cplex_->GetBarItCnt());
  solve_stats.set_barrier_iterations(barrier_iters);

  ASSIGN_OR_RETURN(int node_count, cplex_->GetNodeCnt());
  solve_stats.set_node_count(node_count);

  return solve_stats;
}

absl::StatusOr<CplexSolver::SolutionsAndClaims> CplexSolver::GetMipSolutions(
    const ModelSolveParametersProto& model_parameters) {
  // Assumption: we did not touch CPX_PARAM_SOLNPOOLCAPACITY
  ASSIGN_OR_RETURN(int num_solutions, cplex_->GetSolNPoolNumSolns());
  std::vector<SolutionProto> solutions;
  solutions.reserve(num_solutions);

  for (int i = 0; i < num_solutions; ++i) {
    PrimalSolutionProto primal_solution;
    ASSIGN_OR_RETURN(const double sol_val, cplex_->GetSolnPoolObjVal(i));
    primal_solution.set_objective_value(sol_val);

    // Only the incumbent (solution 0) is vouched for by the termination
    // status. Pool solutions passed feasibility checks when found, but no
    // solver-level claim covers them — mark as UNDETERMINED (matches Gurobi).
    primal_solution.set_feasibility_status(
        i == 0 ? SOLUTION_STATUS_FEASIBLE : SOLUTION_STATUS_UNDETERMINED);

    ASSIGN_OR_RETURN(const std::vector<double> cpx_var_values,
                     cplex_->GetSolnPoolX(i));
    CplexVectorToSparseDoubleVector(cpx_var_values, variables_map_,
                                    *primal_solution.mutable_variable_values(),
                                    model_parameters.variable_values_filter());

    *solutions.emplace_back(SolutionProto()).mutable_primal_solution() =
        std::move(primal_solution);
  }

  ASSIGN_OR_RETURN(const int cpx_stat, cplex_->GetStat());

  // Set solution claims
  ASSIGN_OR_RETURN(const double best_dual_bound, GetCplexBestDualBound());

  const SolutionClaims solution_claims = {
      .primal_feasible_solution_exists = num_solutions > 0,
      .dual_feasible_solution_exists = std::isfinite(best_dual_bound) ||
                                       cpx_stat == CPX_STAT_INFEASIBLE ||
                                       cpx_stat == CPXMIP_INFEASIBLE};

  // Check consistency of solutions, bounds and statuses.
  if ((cpx_stat == CPXMIP_OPTIMAL || cpx_stat == CPXMIP_OPTIMAL_TOL) &&
      num_solutions == 0) {
    return absl::InternalError(
        "CPX MIP Status is optimal, but solution pool is empty.");
  }

  return SolutionsAndClaims{.solutions = std::move(solutions),
                            .solution_claims = solution_claims};
}

absl::StatusOr<CplexSolver::SolutionAndClaim<PrimalSolutionProto>>
CplexSolver::GetConvexPrimalSolutionIfAvailable(
    const ModelSolveParametersProto& model_parameters) {
  ASSIGN_OR_RETURN(auto soln_info, cplex_->SolnInfo());

  // Check solntype (index 1) to see if any solution exists (even infeasible)
  if (std::get<1>(soln_info) == CPX_NO_SOLN) {
    return SolutionAndClaim<PrimalSolutionProto>{
        .solution = std::nullopt, .feasible_solution_exists = false};
  }

  ASSIGN_OR_RETURN(double sol_val, cplex_->GetObjVal());

  PrimalSolutionProto primal_solution;
  primal_solution.set_objective_value(sol_val);

  // pfeasind (index 2): CPLEX's primal feasibility indicator from CPXsolninfo.
  // When a solution exists (solntype != CPX_NO_SOLN, checked above), pfeasind
  // is always computed: 1 = primal feasible, 0 = not primal feasible.
  if (std::get<2>(soln_info)) {
    primal_solution.set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
  } else {
    primal_solution.set_feasibility_status(SOLUTION_STATUS_INFEASIBLE);
  }

  ASSIGN_OR_RETURN(auto sol_x, cplex_->GetX());
  CplexVectorToSparseDoubleVector(sol_x, variables_map_,
                                  *primal_solution.mutable_variable_values(),
                                  model_parameters.variable_values_filter());

  const bool primal_feasible_solution_exists =
      (primal_solution.feasibility_status() == SOLUTION_STATUS_FEASIBLE);
  return SolutionAndClaim<PrimalSolutionProto>{
      .solution = std::move(primal_solution),
      .feasible_solution_exists = primal_feasible_solution_exists};
}

// We follow the gurobi wrapper here instead of trusting
// CPXgetobjval/CPXgetbestobjval
absl::StatusOr<double> CplexSolver::GetBestPrimalBound(
    absl::Span<const SolutionProto> solutions) const {
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

absl::StatusOr<double> CplexSolver::GetBestDualBound(
    absl::Span<const SolutionProto> solutions) const {
  ASSIGN_OR_RETURN(const bool is_maximize, IsMaximize());
  ASSIGN_OR_RETURN(double best_dual_bound, GetCplexBestDualBound());
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

absl::StatusOr<double> CplexSolver::GetCplexBestDualBound() const {
  ASSIGN_OR_RETURN(const bool is_mip, IsMIP());
  if (is_mip) {
    ASSIGN_OR_RETURN(auto best_obj_val, cplex_->GetBestObjVal());
    if (std::abs(best_obj_val) < CPX_INFBOUND) return best_obj_val;
  }

  ASSIGN_OR_RETURN(const bool is_maximize, IsMaximize());
  return is_maximize ? kInf : -kInf;
}

absl::StatusOr<CplexSolver::SolutionsAndClaims> CplexSolver::GetLpSolution(
    const ModelSolveParametersProto& model_parameters) {
  ASSIGN_OR_RETURN(auto primal_solution_and_claim,
                   GetConvexPrimalSolutionIfAvailable(model_parameters));
  ASSIGN_OR_RETURN(auto dual_solution_and_claim,
                   GetConvexDualSolutionIfAvailable(model_parameters));

  const SolutionClaims solution_claims = {
      .primal_feasible_solution_exists =
          primal_solution_and_claim.feasible_solution_exists,
      .dual_feasible_solution_exists =
          dual_solution_and_claim.feasible_solution_exists};

  if (!primal_solution_and_claim.solution.has_value() &&
      !dual_solution_and_claim.solution.has_value()) {
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
  return solution_and_claims;
}

absl::StatusOr<CplexSolver::SolutionAndClaim<DualSolutionProto>>
CplexSolver::GetConvexDualSolutionIfAvailable(
    const ModelSolveParametersProto& model_parameters) {
  ASSIGN_OR_RETURN(auto soln_info, cplex_->SolnInfo());

  // Check solntype (index 1) to see if any solution exists
  // CPX_PRIMAL_SOLN (3) does not contain duals
  if (std::get<1>(soln_info) == CPX_NO_SOLN ||
      std::get<1>(soln_info) == CPX_PRIMAL_SOLN) {
    return SolutionAndClaim<DualSolutionProto>{
        .solution = std::nullopt, .feasible_solution_exists = false};
  }

  ASSIGN_OR_RETURN(const int cpx_stat, cplex_->GetStat());

  DualSolutionProto dual_solution;
  bool dual_feasible_solution_exists = false;

  // Only set dual objective at optimality (strong duality: primal = dual).
  // At non-optimal terminations (e.g., iteration limit), the primal objective
  // from CPXgetobjval is not a valid dual objective value.
  if (cpx_stat == CPX_STAT_OPTIMAL || cpx_stat == CPX_STAT_OPTIMAL_INFEAS) {
    ASSIGN_OR_RETURN(double sol_val, cplex_->GetObjVal());
    dual_solution.set_objective_value(sol_val);
  }

  ASSIGN_OR_RETURN(std::vector<double> sol_dual, cplex_->GetPi());
  CplexVectorToSparseDoubleVector(sol_dual, linear_constraints_map_,
                                  *dual_solution.mutable_dual_values(),
                                  model_parameters.dual_values_filter());

  ASSIGN_OR_RETURN(std::vector<double> sol_reduced, cplex_->GetDj());
  CplexVectorToSparseDoubleVector(sol_reduced, variables_map_,
                                  *dual_solution.mutable_reduced_costs(),
                                  model_parameters.reduced_costs_filter());

  // Check dfeasind (index 3) for feasibility
  if (std::get<3>(soln_info)) {
    dual_solution.set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
    dual_feasible_solution_exists = true;
  } else {
    dual_solution.set_feasibility_status(SOLUTION_STATUS_INFEASIBLE);
  }

  return SolutionAndClaim<DualSolutionProto>{
      .solution = std::move(dual_solution),
      .feasible_solution_exists = dual_feasible_solution_exists};
}

absl::Status CplexSolver::SetParameters(
    const SolveParametersProto& parameters,
    const ModelSolveParametersProto& model_parameters) {
  ASSIGN_OR_RETURN(const bool is_mip, IsMIP());
  ASSIGN_OR_RETURN(const bool is_maximization, IsMaximize());

  // Parameters vs. cplex versions
  ASSIGN_OR_RETURN(const std::string cplex_version, Version());
  ASSIGN_OR_RETURN(const CplexVersion version_loaded,
                   ParseCplexVersion(cplex_version));
  const bool supports_objective_limit =
      version_loaded >= CplexVersion{21, 1, 0};  // modern versions only!

  ASSIGN_OR_RETURN(const CplexParametersProto cplex_parameters,
                   MergeParameters(parameters, model_parameters, is_mip,
                                   !is_maximization, supports_objective_limit));

  std::vector<std::string> parameter_errors;
  for (const CplexParametersProto::Parameter& parameter :
       cplex_parameters.parameters()) {
    absl::Status param_status;

    auto FnGetParamNameOkayOrAddToErrors =
        [&cplex_ = cplex_,
         &parameter_errors](const auto& name) -> absl::StatusOr<int> {
      const auto param_id_or_error = cplex_->GetParamNum(name);
      if (!param_id_or_error.ok()) {
        parameter_errors.emplace_back(param_id_or_error.status().message());
        return param_id_or_error.status();
      }
      return param_id_or_error;
    };

    auto FnSetParamOkayOrAddToErrors = [&cplex_ = cplex_, &parameter_errors](
                                           int id, const auto& value,
                                           auto&& setter) -> void {
      const auto status = ((*cplex_).*setter)(id, value);
      if (!status.ok()) parameter_errors.emplace_back(status.message());
    };

    // ---

    if (parameter.has_parameter_double()) {
      const auto maybe_param_id =
          FnGetParamNameOkayOrAddToErrors(parameter.parameter_double().name());
      if (maybe_param_id.ok())
        FnSetParamOkayOrAddToErrors(maybe_param_id.value(),
                                    parameter.parameter_double().value(),
                                    &Cplex::SetParamDouble);
    } else if (parameter.has_parameter_bool()) {
      const auto maybe_param_id =
          FnGetParamNameOkayOrAddToErrors(parameter.parameter_bool().name());
      if (maybe_param_id.ok())
        FnSetParamOkayOrAddToErrors(maybe_param_id.value(),
                                    parameter.parameter_bool().value(),
                                    &Cplex::SetParamBool);
    } else if (parameter.has_parameter_int32()) {
      const auto maybe_param_id =
          FnGetParamNameOkayOrAddToErrors(parameter.parameter_int32().name());
      if (maybe_param_id.ok())
        FnSetParamOkayOrAddToErrors(maybe_param_id.value(),
                                    parameter.parameter_int32().value(),
                                    &Cplex::SetParamInt32);
    } else if (parameter.has_parameter_int64()) {
      const auto maybe_param_id =
          FnGetParamNameOkayOrAddToErrors(parameter.parameter_int64().name());
      if (maybe_param_id.ok())
        FnSetParamOkayOrAddToErrors(maybe_param_id.value(),
                                    parameter.parameter_int64().value(),
                                    &Cplex::SetParamInt64);
    } else if (parameter.has_parameter_string()) {
      const auto maybe_param_id =
          FnGetParamNameOkayOrAddToErrors(parameter.parameter_string().name());
      if (maybe_param_id.ok())
        FnSetParamOkayOrAddToErrors(maybe_param_id.value(),
                                    parameter.parameter_string().value(),
                                    &Cplex::SetParamString);
    }
  }
  if (!parameter_errors.empty()) {
    return absl::InvalidArgumentError(absl::StrJoin(parameter_errors, "; "));
  }
  return absl::OkStatus();
}

absl::Status CplexSolver::AddNewVariables(const VariablesProto& new_variables) {
  VLOG(2) << "CplexSolver::AddNewVariables";

  const int num_new_variables = new_variables.lower_bounds().size();

  std::vector<char> variable_types(num_new_variables, 'C');
  bool is_mip = false;
  for (int i = 0; i < num_new_variables; ++i) {
    const VariableId id = new_variables.ids(i);
    gtl::InsertOrDie(&variables_map_, id, i + num_cplex_variables_);

    const bool is_integer = new_variables.integers(i);
    if (is_integer) {
      variable_types[i] = 'I';
      is_mip = true;
    }
  }

  ASSIGN_OR_RETURN(
      auto checked_lbs,
      CheckCopySpan<std::vector<double>>(
          absl::MakeConstSpan(new_variables.lower_bounds()), SafeCplexDouble));
  ASSIGN_OR_RETURN(
      auto checked_ubs,
      CheckCopySpan<std::vector<double>>(
          absl::MakeConstSpan(new_variables.upper_bounds()), SafeCplexDouble));

  const std::vector<std::string> names_maybe_truncated =
      TruncateNames(new_variables.names());

  // Cplex interprets anything other than nullptr as MIP
  absl::Span<const char> variable_types_span_to_pass_on = variable_types;
  if (!is_mip) variable_types_span_to_pass_on = absl::Span<const char>{};

  RETURN_IF_ERROR(cplex_->NewCols(checked_lbs, checked_ubs,
                                  variable_types_span_to_pass_on,
                                  names_maybe_truncated));

  num_cplex_variables_ += num_new_variables;

  return absl::OkStatus();
}

absl::Status CplexSolver::AddSingleObjective(const ObjectiveProto& objective) {
  VLOG(2) << "CplexSolver::AddSingleObjective";

  bool is_maximize = objective.maximize();

  RETURN_IF_ERROR(cplex_->ChgObjSen(is_maximize ? CPX_MAX : CPX_MIN));

  RETURN_IF_ERROR(cplex_->ChgObjOffset(objective.offset()));

  ASSIGN_OR_RETURN(
      (auto [indices, values]),
      PrepareLinearObjectiveNonzeros(objective.linear_coefficients().ids(),
                                     objective.linear_coefficients().values()));
  RETURN_IF_ERROR(cplex_->ChgObj(indices, values));

  return absl::OkStatus();
}

absl::Status CplexSolver::AddNewLinearConstraints(
    const LinearConstraintsProto& new_constraints) {
  VLOG(2) << "CplexSolver::AddNewLinearConstraints";

  const int num_new_constraints = new_constraints.lower_bounds().size();

  // (parallel) dense vectors of size |num_new_constraints|
  std::vector<double> rhs;
  std::vector<char> sense;
  rhs.reserve(num_new_constraints);
  sense.reserve(num_new_constraints);

  // (single) sparse vector of size <= |num_new_constraints|
  std::vector<int> range_cons_index;
  std::vector<double> range_cons_diff;

  for (int i = 0; i < num_new_constraints; ++i) {
    const int64_t id = new_constraints.ids(i);
    LinearConstraintData& constraint_data =
        gtl::InsertKeyOrDie(&linear_constraints_map_, id);

    const double lb = new_constraints.lower_bounds(i);
    RETURN_IF_ERROR(SafeCplexDouble(lb));
    const double ub = new_constraints.upper_bounds(i);
    RETURN_IF_ERROR(SafeCplexDouble(ub));

    constraint_data.lower_bound = lb;
    constraint_data.upper_bound = ub;
    constraint_data.constraint_index = i + num_cplex_lin_cons_;

    if (IsFinite(lb) && IsFinite(ub)) {
      if (lb == ub) {
        // eq constraint
        rhs.push_back(lb);
        sense.push_back('E');
      } else {
        // range-constraint
        rhs.push_back(lb);
        sense.push_back('R');

        range_cons_index.push_back(i + num_cplex_lin_cons_);
        range_cons_diff.push_back(ub - lb);
      }
    } else if (IsFinite(lb)) {
      // greater equal constraint
      rhs.push_back(lb);
      sense.push_back('G');
    } else if (IsFinite(ub)) {
      // less equal constraint
      rhs.push_back(ub);
      sense.push_back('L');
    } else {
      // Free constraint: -∞ ≤ a·x ≤ +∞.  Must still create a CPLEX row so
      // that constraint indices stay synchronized.
      rhs.push_back(-CPX_INFBOUND);
      sense.push_back('G');
    }
  }

  const std::vector<std::string> names_maybe_truncated =
      TruncateNames(new_constraints.names());

  RETURN_IF_ERROR(cplex_->NewRows(rhs, sense,
                                  absl::Span<const double>(),  // rngval
                                  names_maybe_truncated));

  RETURN_IF_ERROR(cplex_->ChgRngVal(range_cons_index, range_cons_diff));

  num_cplex_lin_cons_ += num_new_constraints;

  return absl::OkStatus();
}

absl::Status CplexSolver::ChangeCoefficients(
    const SparseDoubleMatrixProto& matrix) {
  for (const double coefficient : matrix.coefficients()) {
    RETURN_IF_ERROR(SafeCplexDouble(coefficient));
  }
  const int num_coefficients = matrix.row_ids().size();
  std::vector<CplexLinearConstraintIndex> row_index(num_coefficients);
  std::vector<CplexVariableIndex> col_index(num_coefficients);
  for (int k = 0; k < num_coefficients; ++k) {
    row_index[k] =
        linear_constraints_map_.at(matrix.row_ids(k)).constraint_index;
    col_index[k] = variables_map_.at(matrix.column_ids(k));
  }
  return cplex_->ChgCoefList(row_index, col_index, matrix.coefficients());
}

absl::Status CplexSolver::LoadModel(const ModelProto& input_model) {
  CHECK(cplex_ != nullptr);

  RETURN_IF_ERROR(cplex_->ChgProbName(TruncateName(input_model.name())));

  RETURN_IF_ERROR(AddNewVariables(input_model.variables()));

  RETURN_IF_ERROR(AddNewLinearConstraints(input_model.linear_constraints()));

  RETURN_IF_ERROR(ChangeCoefficients(input_model.linear_constraint_matrix()));

  if (input_model.auxiliary_objectives().empty()) {
    RETURN_IF_ERROR(AddSingleObjective(input_model.objective()));
  } else {
    return absl::UnimplementedError(
        "Multiple objectives are currently not supported in CPLEX interface");
  }
  return absl::OkStatus();
}

absl::Status CplexSolver::UpdateLinearConstraints(
    const LinearConstraintUpdatesProto& constraints_update,
    std::vector<CplexVariableIndex>& deleted_variables_index) {
  const SparseDoubleVectorProto& constraint_lower_bounds =
      constraints_update.lower_bounds();
  const SparseDoubleVectorProto& constraint_upper_bounds =
      constraints_update.upper_bounds();

  // If no update, just return.
  if (constraint_lower_bounds.ids().empty() &&
      constraint_upper_bounds.ids().empty()) {
    return absl::OkStatus();
  }

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

  std::vector<char> sense_data;
  std::vector<double> rhs_data;
  std::vector<CplexLinearConstraintIndex> rhs_index;

  std::vector<int> range_cons_index;
  std::vector<double> range_cons_diff;

  // Iterate on the changes, and populate the three possible changes.
  for (UpdateConstraintData& update_data : update_vector) {
    const bool same_lower_bound =
        (update_data.source.lower_bound == update_data.new_lower_bound) ||
        ((update_data.source.lower_bound <= -CPX_INFBOUND) &&
         (update_data.new_lower_bound <= -CPX_INFBOUND));
    const bool same_upper_bound =
        (update_data.source.upper_bound == update_data.new_upper_bound) ||
        ((update_data.source.upper_bound >= CPX_INFBOUND) &&
         (update_data.new_upper_bound >= CPX_INFBOUND));
    if (same_upper_bound && same_lower_bound) continue;

    // Validate the new bounds before mutating cached state, matching the
    // initial-load validation in AddNewLinearConstraints().
    RETURN_IF_ERROR(SafeCplexDouble(update_data.new_lower_bound));
    RETURN_IF_ERROR(SafeCplexDouble(update_data.new_upper_bound));

    // Save into linear_constraints_map_[id] the new bounds for the linear
    // constraint.
    update_data.source.lower_bound = update_data.new_lower_bound;
    update_data.source.upper_bound = update_data.new_upper_bound;
    // Detect the type of constraint to add and store RHS and bounds.
    if (update_data.new_lower_bound <= -CPX_INFBOUND &&
        update_data.new_upper_bound < CPX_INFBOUND) {
      rhs_index.emplace_back(update_data.source.constraint_index);
      rhs_data.emplace_back(update_data.new_upper_bound);
      sense_data.emplace_back('L');
    } else if (update_data.new_lower_bound > -CPX_INFBOUND &&
               update_data.new_upper_bound >= CPX_INFBOUND) {
      rhs_index.emplace_back(update_data.source.constraint_index);
      rhs_data.emplace_back(update_data.new_lower_bound);
      sense_data.emplace_back('G');
    } else if (update_data.new_lower_bound == update_data.new_upper_bound) {
      rhs_index.emplace_back(update_data.source.constraint_index);
      rhs_data.emplace_back(update_data.new_lower_bound);
      sense_data.emplace_back('E');
    } else if (update_data.new_lower_bound <= -CPX_INFBOUND &&
               update_data.new_upper_bound >= CPX_INFBOUND) {
      // Free constraint: -∞ ≤ a·x ≤ +∞.
      rhs_index.emplace_back(update_data.source.constraint_index);
      rhs_data.emplace_back(-CPX_INFBOUND);
      sense_data.emplace_back('G');
    } else {
      // Range constraint (both bounds finite, lb != ub).
      range_cons_index.emplace_back(update_data.source.constraint_index);
      range_cons_diff.emplace_back(update_data.new_upper_bound -
                                   update_data.new_lower_bound);

      rhs_index.emplace_back(update_data.source.constraint_index);
      rhs_data.emplace_back(update_data.new_lower_bound);
      sense_data.emplace_back('R');
    }
  }

  // Pass down changes to Cplex.
  if (!rhs_index.empty()) {
    RETURN_IF_ERROR(cplex_->ChgSense(rhs_index, sense_data));
    RETURN_IF_ERROR(cplex_->ChgRhs(rhs_index, rhs_data));
    RETURN_IF_ERROR(cplex_->ChgRngVal(range_cons_index, range_cons_diff));
  }

  return absl::OkStatus();
}

void CplexSolver::UpdateCplexIndices(const DeletedIndices& deleted_indices) {
  // Recover the updated indices of variables.
  if (!deleted_indices.variables.empty()) {
    const std::vector<CplexVariableIndex> old_to_new =
        IndexUpdateMap(num_cplex_variables_, deleted_indices.variables);
    for (auto& [_, cpx_index] : variables_map_) {
      cpx_index = old_to_new[cpx_index];
      CHECK_NE(cpx_index, kDeletedIndex);
    }
  }
  // Recover the updated indices of linear constraints.
  if (!deleted_indices.linear_constraints.empty()) {
    const std::vector<CplexLinearConstraintIndex> old_to_new =
        IndexUpdateMap(num_cplex_lin_cons_, deleted_indices.linear_constraints);
    for (auto& [_, lin_con_data] : linear_constraints_map_) {
      lin_con_data.constraint_index = old_to_new[lin_con_data.constraint_index];
      CHECK_NE(lin_con_data.constraint_index, kDeletedIndex);
    }
  }
}

absl::StatusOr<bool> CplexSolver::Update(const ModelUpdateProto& model_update) {
  if (!undeletable_variables_.empty()) {
    for (const VariableId id : model_update.deleted_variable_ids()) {
      if (undeletable_variables_.contains(id)) {
        return false;
      }
    }
  }
  if (!UpdateIsSupported(model_update, kCplexSupportedStructures)) {
    return false;
  }

  RETURN_IF_ERROR(AddNewVariables(model_update.new_variables()));

  RETURN_IF_ERROR(
      AddNewLinearConstraints(model_update.new_linear_constraints()));

  RETURN_IF_ERROR(
      ChangeCoefficients(model_update.linear_constraint_matrix_updates()));

  if (model_update.objective_updates().has_direction_update()) {
    RETURN_IF_ERROR(cplex_->ChgObjSen(
        model_update.objective_updates().direction_update() ? CPX_MAX
                                                            : CPX_MIN));
  }

  if (model_update.objective_updates().has_offset_update()) {
    RETURN_IF_ERROR(
        cplex_->ChgObjOffset(model_update.objective_updates().offset_update()));
  }

  if (!model_update.objective_updates().linear_coefficients().ids().empty()) {
    ASSIGN_OR_RETURN(
        (auto [obj_indices, obj_values]),
        PrepareLinearObjectiveNonzeros(
            model_update.objective_updates().linear_coefficients().ids(),
            model_update.objective_updates().linear_coefficients().values()));
    RETURN_IF_ERROR(cplex_->ChgObj(obj_indices, obj_values));
  }

  // Update bounds
  const auto& var_updates = model_update.variable_updates();

  // Validate all new variable bounds before passing them to CPLEX, matching
  // the initial-load validation in AddNewVariables().
  for (int i = 0; i < var_updates.lower_bounds().ids_size(); ++i) {
    RETURN_IF_ERROR(SafeCplexDouble(var_updates.lower_bounds().values(i)));
  }
  for (int i = 0; i < var_updates.upper_bounds().ids_size(); ++i) {
    RETURN_IF_ERROR(SafeCplexDouble(var_updates.upper_bounds().values(i)));
  }

  if (!var_updates.lower_bounds().ids().empty()) {
    std::vector<int> indices;
    std::vector<double> values;
    std::vector<char> lu;
    for (int i = 0; i < var_updates.lower_bounds().ids_size(); ++i) {
      indices.push_back(variables_map_.at(var_updates.lower_bounds().ids(i)));
      values.push_back(var_updates.lower_bounds().values(i));
      lu.push_back('L');
    }
    RETURN_IF_ERROR(cplex_->Chgbds(indices, lu, values));
  }
  if (!var_updates.upper_bounds().ids().empty()) {
    std::vector<int> indices;
    std::vector<double> values;
    std::vector<char> lu;
    for (int i = 0; i < var_updates.upper_bounds().ids_size(); ++i) {
      indices.push_back(variables_map_.at(var_updates.upper_bounds().ids(i)));
      values.push_back(var_updates.upper_bounds().values(i));
      lu.push_back('U');
    }
    RETURN_IF_ERROR(cplex_->Chgbds(indices, lu, values));
  }

  // Integers
  if (model_update.variable_updates().has_integers()) {
    const SparseBoolVectorProto& update =
        model_update.variable_updates().integers();
    std::vector<CplexVariableIndex> index;
    index.reserve(update.ids_size());
    std::vector<char> value;
    value.reserve(update.values_size());
    for (int i = 0; i < update.ids_size(); ++i) {
      index.push_back(variables_map_.at(update.ids(i)));
      value.push_back(update.values(i) ? CPX_INTEGER : CPX_CONTINUOUS);
    }
    RETURN_IF_ERROR(cplex_->Chgctype(index, value));
  }

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
    linear_constraints_map_.erase(id);
  }

  UpdateCplexIndices(deleted_indices);

  if (!deleted_indices.linear_constraints.empty()) {
    RETURN_IF_ERROR(
        cplex_->DelSetRows(deleted_indices.linear_constraints));
    num_cplex_lin_cons_ -= deleted_indices.linear_constraints.size();
  }

  if (!deleted_indices.variables.empty()) {
    RETURN_IF_ERROR(cplex_->DelSetCols(deleted_indices.variables));
    num_cplex_variables_ -= deleted_indices.variables.size();
  }

  return true;
}

absl::StatusOr<std::unique_ptr<CplexSolver>> CplexSolver::New(
    const ModelProto& input_model, const SolverInterface::InitArgs& init_args) {
  RETURN_IF_ERROR(
      ModelIsSupported(input_model, kCplexSupportedStructures, "Cplex"));

  ASSIGN_OR_RETURN(std::unique_ptr<Cplex> cplex, CplexFromInitArgs(init_args));
  auto cplex_solver = absl::WrapUnique(new CplexSolver(std::move(cplex)));
  RETURN_IF_ERROR(cplex_solver->LoadModel(input_model));
  return cplex_solver;
}

absl::StatusOr<InvertedBounds> CplexSolver::ListInvertedBounds() const {
  InvertedBounds inverted_bounds;
  ASSIGN_OR_RETURN(const int n_vars, cplex_->GetNumCols());

  if (n_vars > 0) {
    ASSIGN_OR_RETURN(const std::vector<double> var_lbs,
                     cplex_->GetLb(0, n_vars - 1));
    ASSIGN_OR_RETURN(const std::vector<double> var_ubs,
                     cplex_->GetUb(0, n_vars - 1));

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

  std::sort(inverted_bounds.variables.begin(), inverted_bounds.variables.end());
  std::sort(inverted_bounds.linear_constraints.begin(),
            inverted_bounds.linear_constraints.end());
  return inverted_bounds;
}

absl::Status CplexSolver::ResetModelParameters(
    const ModelSolveParametersProto& model_parameters) {
  // No model-level attributes to reset. CPLEX does not yet implement
  // branching priorities, lazy constraints, or other per-solve model
  // modifications that Gurobi resets here. Solve parameters (threads,
  // tolerances, etc.) are re-applied by SetParameters() on each Solve() call,
  // so resetting them here is unnecessary.
  return absl::OkStatus();
}

absl::StatusOr<SolveResultProto> CplexSolver::Solve(
    const SolveParametersProto& parameters,
    const ModelSolveParametersProto& model_parameters,
    const MessageCallback message_cb,
    const CallbackRegistrationProto& callback_registration, const Callback cb,
    const SolveInterrupter* absl_nullable const interrupter) {
  RETURN_IF_ERROR(ModelSolveParametersAreSupported(
      model_parameters, kCplexSupportedStructures, "Cplex"));

  // Solution hints are silently ignored — CPLEX's C API supports MIP starts
  // (CPXaddmipstarts) but this integration does not implement them yet.
  // Silently ignoring matches the behavior of Glop and GLPK.
  if (model_parameters.has_initial_basis()) {
    return absl::UnimplementedError(
        "Initial basis is not currently supported for CPLEX.");
  }
  if (!model_parameters.branching_priorities().ids().empty()) {
    return absl::UnimplementedError(
        "Branching priorities are not currently supported for CPLEX.");
  }
  if (!model_parameters.lazy_linear_constraint_ids().empty()) {
    return absl::UnimplementedError(
        "Lazy linear constraints are not currently supported for CPLEX.");
  }

  const absl::Time start = absl::Now();

  ASSIGN_OR_RETURN(const InvertedBounds inverted_bounds, ListInvertedBounds());
  RETURN_IF_ERROR(inverted_bounds.ToStatus());

  // Set parameters
  RETURN_IF_ERROR(SetParameters(parameters, model_parameters));

  // Per the MathOpt spec (parameters.proto, enable_output):
  //   "if the solver supports message callback and the user registers a
  //    callback for it, then this parameter value is ignored and no traces
  //    are printed."
  // Suppress console output and parameter-change diagnostics when a message
  // callback is registered, matching the behavior of Gurobi and Xpress.
  if (message_cb != nullptr) {
    RETURN_IF_ERROR(cplex_->SetParamBool(CPXPARAM_ScreenOutput, false));
    RETURN_IF_ERROR(cplex_->SetParamBool(CPXPARAM_ParamDisplay, false));
  }

  // We use a volatile int as the terminate flag for CPLEX.
  volatile int terminate_flag = 0;

  // Register the callback to update the terminate_flag when interruption is
  // requested. The ScopedSolveInterrupterCallback ensures that the callback is
  // unregistered when this scope exits.
  const ScopedSolveInterrupterCallback scoped_interrupt_cb(
      interrupter, [&terminate_flag]() { terminate_flag = 1; });

  // Pass the address of the terminate flag to CPLEX.
  RETURN_IF_ERROR(cplex_->SetTerminate(&terminate_flag));

  // Ensure we clear the terminate pointer in CPLEX when we exit this function
  // to avoid CPLEX accessing a dangling pointer (local variable on stack).
  absl::Cleanup clear_terminate = [this]() {
    const absl::Status status = cplex_->SetTerminate(nullptr);
    if (!status.ok()) {
      LOG(ERROR) << "Failed to clear CPLEX terminate flag: " << status;
    }
  };

  // Channel state — declared before the cleanup so they are destroyed after it.
  CPXCHANNELptr result_channel = nullptr;
  CPXCHANNELptr warning_channel = nullptr;
  CPXCHANNELptr error_channel = nullptr;
  CPXCHANNELptr log_channel = nullptr;
  std::unique_ptr<BufferedMessageCallback> buffered_message_callback;

  bool result_attached = false;
  bool warning_attached = false;
  bool error_attached = false;
  bool log_attached = false;

  // IMPORTANT: The cleanup MUST be constructed BEFORE any AddFuncDest call.
  // If an AddFuncDest call fails mid-way (via RETURN_IF_ERROR), the cleanup
  // detaches every channel that was successfully attached, preventing a
  // use-after-free when buffered_message_callback is destroyed on stack
  // unwind.
  absl::Cleanup clear_message_cb = [&]() {
    if (buffered_message_callback == nullptr) return;
    void* const handle = buffered_message_callback.get();
    auto detach = [&](const bool attached, CPXCHANNELptr channel,
                      const char* label) {
      if (!attached) return;
      const absl::Status status =
          cplex_->DelFuncDest(channel, handle, MessageCallbackImpl);
      if (!status.ok()) {
        LOG(ERROR) << "Failed to remove CPLEX " << label
                   << " message callback: " << status;
      }
    };
    detach(result_attached, result_channel, "result");
    detach(warning_attached, warning_channel, "warning");
    detach(error_attached, error_channel, "error");
    detach(log_attached, log_channel, "log");
  };

  // Register message callback destinations on all four CPLEX channels.
  if (message_cb != nullptr) {
    buffered_message_callback =
        std::make_unique<BufferedMessageCallback>(message_cb);
    ASSIGN_OR_RETURN(
        (std::tie(result_channel, warning_channel, error_channel, log_channel)),
        cplex_->GetChannels());

    void* const handle = buffered_message_callback.get();

    auto attach = [&](CPXCHANNELptr channel,
                      bool& attached) -> absl::Status {
      if (channel == nullptr) return absl::OkStatus();
      RETURN_IF_ERROR(
          cplex_->AddFuncDest(channel, handle, MessageCallbackImpl));
      attached = true;
      return absl::OkStatus();
    };

    RETURN_IF_ERROR(attach(result_channel, result_attached));
    RETURN_IF_ERROR(attach(warning_channel, warning_attached));
    RETURN_IF_ERROR(attach(error_channel, error_attached));
    RETURN_IF_ERROR(attach(log_channel, log_attached));
  }

  if (callback_registration.request_registration_size() > 0 || cb != nullptr) {
    return absl::UnimplementedError(
        "Callbacks are not currently supported for CPLEX.");
  }

  ASSIGN_OR_RETURN(const bool is_mip, IsMIP());
  if (is_mip) {
    RETURN_IF_ERROR(cplex_->MipOpt());
  } else {
    RETURN_IF_ERROR(cplex_->LpOpt());
  }

  if (buffered_message_callback != nullptr) {
    buffered_message_callback->Flush();
  }

  // Detach message callbacks before result extraction. CPLEX emits internal
  // diagnostics (e.g., Error 1217) during stat queries that are harmless but
  // would be forwarded to the user's message callback if still attached.
  std::move(clear_message_cb).Invoke();

  // Suppress CPLEX's default stderr output during result extraction. CPLEX
  // prints error messages (e.g., "CPLEX Error 1217: No solution exists.") to
  // stderr when querying stats that are inapplicable to the current solve state,
  // even though the return code is handled correctly. Disabling screen output
  // after the solve phase ensures all solve-time messages are visible while
  // extraction noise is suppressed.
  //
  // This is unconditional: when message_cb was set, ScreenOutput is already
  // false (set above); this handles the case where message_cb is null but
  // enable_output was true.
  RETURN_IF_ERROR(
      cplex_->SetParamBool(CPXPARAM_ScreenOutput, false));

  const bool had_cutoff = parameters.has_cutoff_limit();
  const bool had_iteration_limit = parameters.has_iteration_limit();
  const bool had_objective_limit = parameters.has_objective_limit();

  ASSIGN_OR_RETURN(
      SolveResultProto solve_result,
      ExtractSolveResultProto(start, model_parameters, had_cutoff,
                              had_iteration_limit, had_objective_limit));

  // Reset CPLEX parameters so that settings from this Solve() call do not
  // leak into subsequent Solve() calls (mirrors Gurobi's ResetParameters).
  RETURN_IF_ERROR(cplex_->SetDefaults());
  RETURN_IF_ERROR(ResetModelParameters(model_parameters));

  return solve_result;
}

absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
CplexSolver::ComputeInfeasibleSubsystem(
    const SolveParametersProto& parameters, MessageCallback message_cb,
    const SolveInterrupter* absl_nullable interrupter) {
  return absl::UnimplementedError(
      "ComputeInfeasibleSubsystem not implemented for CPLEX");
}

absl::StatusOr<std::tuple<std::vector<int>, std::vector<double>>>
CplexSolver::PrepareLinearObjectiveNonzeros(
    const absl::Span<const int64_t> indices,
    const absl::Span<const double> values) {
  VLOG(2) << "CplexSolver::PrepareLinearObjectiveNonzeros";

  if (indices.size() != values.size())
    return absl::InvalidArgumentError(
        "CplexSolver::PrepareLinearObjectiveNonzeros: sizes of arguments don't "
        "match");

  std::vector<int> res_indices;
  std::vector<double> res_values;

  for (size_t i = 0; i < indices.size(); ++i) {
    RETURN_IF_ERROR(SafeCplexDouble(values[i]));
    res_indices.push_back(variables_map_.at(indices[i]));
    res_values.push_back(values[i]);
  }

  return {{res_indices, res_values}};
}

bool CplexSolver::IsFinite(double value) {
  return value < CPX_INFBOUND && value > -CPX_INFBOUND;
}

MATH_OPT_REGISTER_SOLVER(SOLVER_TYPE_CPLEX, CplexSolver::New)

}  // namespace math_opt
}  // namespace operations_research