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

#include "ortools/math_opt/cpp/solve_result.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/protoutil.h"
#include "ortools/math_opt/core/model_storage.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/port/proto_utils.h"

namespace operations_research {
namespace math_opt {
namespace {

// Converts a map with BasisStatusProto values to a map with BasisStatus values
// CHECKing that no values are BASIS_STATUS_UNSPECIFIED (the validation code
// should have tested that).
//
// TODO(b/201344491): use FromProto() factory methods on solution members and
// remove the need for this conversion from `IndexedSolutions`.
template <typename TypedIndex>
absl::flat_hash_map<TypedIndex, BasisStatus> BasisStatusMapFromProto(
    const absl::flat_hash_map<TypedIndex, BasisStatusProto>& proto_map) {
  absl::flat_hash_map<TypedIndex, BasisStatus> cpp_map;
  for (const auto& [id, proto_value] : proto_map) {
    const std::optional<BasisStatus> opt_status = EnumFromProto(proto_value);
    CHECK(opt_status.has_value());
    cpp_map.emplace(id, *opt_status);
  }
  return cpp_map;
}

}  // namespace

std::optional<absl::string_view> Enum<FeasibilityStatus>::ToOptString(
    FeasibilityStatus value) {
  switch (value) {
    case FeasibilityStatus::kUndetermined:
      return "undetermined";
    case FeasibilityStatus::kFeasible:
      return "feasible";
    case FeasibilityStatus::kInfeasible:
      return "infeasible";
  }
  return std::nullopt;
}

absl::Span<const FeasibilityStatus> Enum<FeasibilityStatus>::AllValues() {
  static constexpr FeasibilityStatus kFeasibilityStatus[] = {
      FeasibilityStatus::kUndetermined,
      FeasibilityStatus::kFeasible,
      FeasibilityStatus::kInfeasible,
  };
  return absl::MakeConstSpan(kFeasibilityStatus);
}

std::optional<absl::string_view> Enum<TerminationReason>::ToOptString(
    TerminationReason value) {
  switch (value) {
    case TerminationReason::kOptimal:
      return "optimal";
    case TerminationReason::kInfeasible:
      return "infeasible";
    case TerminationReason::kUnbounded:
      return "unbounded";
    case TerminationReason::kInfeasibleOrUnbounded:
      return "infeasible_or_unbounded";
    case TerminationReason::kImprecise:
      return "imprecise";
    case TerminationReason::kFeasible:
      return "feasible";
    case TerminationReason::kNoSolutionFound:
      return "no_solution_found";
    case TerminationReason::kNumericalError:
      return "numerical_error";
    case TerminationReason::kOtherError:
      return "other_error";
  }
  return std::nullopt;
}

absl::Span<const TerminationReason> Enum<TerminationReason>::AllValues() {
  static constexpr TerminationReason kTerminationReasonValues[] = {
      TerminationReason::kOptimal,
      TerminationReason::kInfeasible,
      TerminationReason::kUnbounded,
      TerminationReason::kInfeasibleOrUnbounded,
      TerminationReason::kImprecise,
      TerminationReason::kFeasible,
      TerminationReason::kNoSolutionFound,
      TerminationReason::kNumericalError,
      TerminationReason::kOtherError,
  };
  return absl::MakeConstSpan(kTerminationReasonValues);
}

std::optional<absl::string_view> Enum<Limit>::ToOptString(Limit value) {
  switch (value) {
    case Limit::kUndetermined:
      return "undetermined";
    case Limit::kIteration:
      return "iteration";
    case Limit::kTime:
      return "time";
    case Limit::kNode:
      return "node";
    case Limit::kSolution:
      return "solution";
    case Limit::kMemory:
      return "memory";
    case Limit::kCutoff:
      return "cutoff";
    case Limit::kObjective:
      return "objective";
    case Limit::kNorm:
      return "norm";
    case Limit::kInterrupted:
      return "interrupted";
    case Limit::kSlowProgress:
      return "slow_progress";
    case Limit::kOther:
      return "other";
  }
  return std::nullopt;
}

absl::Span<const Limit> Enum<Limit>::AllValues() {
  static constexpr Limit kLimitValues[] = {
      Limit::kUndetermined, Limit::kIteration,    Limit::kTime,
      Limit::kNode,         Limit::kSolution,     Limit::kMemory,
      Limit::kCutoff,       Limit::kObjective,    Limit::kNorm,
      Limit::kInterrupted,  Limit::kSlowProgress, Limit::kOther};
  return absl::MakeConstSpan(kLimitValues);
}

Termination::Termination(const TerminationReason reason, std::string detail)
    : reason(reason), detail(std::move(detail)) {}

Termination Termination::Feasible(const Limit limit, const std::string detail) {
  Termination termination(TerminationReason::kFeasible, detail);
  termination.limit = limit;
  return termination;
}

Termination Termination::NoSolutionFound(const Limit limit,
                                         const std::string detail) {
  Termination termination(TerminationReason::kNoSolutionFound, detail);
  termination.limit = limit;
  return termination;
}

TerminationProto Termination::ToProto() const {
  TerminationProto proto;
  proto.set_reason(EnumToProto(reason));
  if (limit.has_value()) {
    proto.set_limit(EnumToProto(*limit));
  }
  proto.set_detail(detail);
  return proto;
}

bool Termination::limit_reached() const {
  return reason == TerminationReason::kFeasible ||
         reason == TerminationReason::kNoSolutionFound;
}

Termination Termination::FromProto(const TerminationProto& termination_proto) {
  const bool limit_reached =
      termination_proto.reason() == TERMINATION_REASON_FEASIBLE ||
      termination_proto.reason() == TERMINATION_REASON_NO_SOLUTION_FOUND;
  const bool has_limit = termination_proto.limit() != LIMIT_UNSPECIFIED;
  CHECK_EQ(limit_reached, has_limit)
      << "Termination reason should be TERMINATION_REASON_FEASIBLE or "
         "TERMINATION_REASON_NO_SOLUTION_FOUND if and only if limit is "
         "specified, but found reason="
      << ProtoEnumToString(termination_proto.reason())
      << " and limit=" << ProtoEnumToString(termination_proto.limit());

  if (has_limit) {
    const std::optional<Limit> opt_limit =
        EnumFromProto(termination_proto.limit());
    CHECK(opt_limit.has_value());
    if (termination_proto.reason() == TERMINATION_REASON_FEASIBLE) {
      return Feasible(*opt_limit, termination_proto.detail());
    }
    return NoSolutionFound(*opt_limit, termination_proto.detail());
  }

  const std::optional<TerminationReason> opt_reason =
      EnumFromProto(termination_proto.reason());
  CHECK(opt_reason.has_value());
  return Termination(*opt_reason, termination_proto.detail());
}

std::ostream& operator<<(std::ostream& ostr, const Termination& termination) {
  ostr << "{reason: " << termination.reason;
  if (termination.limit.has_value()) {
    ostr << ", limit: " << *termination.limit;
  }
  if (!termination.detail.empty()) {
    // TODO(b/200835670): quote detail and escape it properly.
    ostr << ", detail: " << termination.detail;
  }
  ostr << "}";
  return ostr;
}

std::string Termination::ToString() const {
  std::ostringstream stream;
  stream << *this;
  return stream.str();
}

ProblemStatusProto ProblemStatus::ToProto() const {
  ProblemStatusProto proto;
  proto.set_primal_status(EnumToProto(primal_status));
  proto.set_dual_status(EnumToProto(dual_status));
  proto.set_primal_or_dual_infeasible(primal_or_dual_infeasible);
  return proto;
}

ProblemStatus ProblemStatus::FromProto(
    const ProblemStatusProto& problem_status_proto) {
  ProblemStatus result;
  // TODO(b/209014770): consider adding a function to simplify this pattern.
  const std::optional<FeasibilityStatus> opt_primal_status =
      EnumFromProto(problem_status_proto.primal_status());
  const std::optional<FeasibilityStatus> opt_dual_status =
      EnumFromProto(problem_status_proto.dual_status());
  CHECK(opt_primal_status.has_value());
  CHECK(opt_dual_status.has_value());
  result.primal_status = *opt_primal_status;
  result.dual_status = *opt_dual_status;
  result.primal_or_dual_infeasible =
      problem_status_proto.primal_or_dual_infeasible();
  return result;
}

std::ostream& operator<<(std::ostream& ostr,
                         const ProblemStatus& problem_status) {
  ostr << "{primal_status: " << problem_status.primal_status;
  ostr << ", dual_status: " << problem_status.dual_status;
  ostr << ", primal_or_dual_infeasible: "
       << (problem_status.primal_or_dual_infeasible ? "true" : "false");
  ostr << "}";
  return ostr;
}

std::string ProblemStatus::ToString() const {
  std::ostringstream stream;
  stream << *this;
  return stream.str();
}

SolveStatsProto SolveStats::ToProto() const {
  SolveStatsProto proto;
  CHECK_OK(
      util_time::EncodeGoogleApiProto(solve_time, proto.mutable_solve_time()));
  proto.set_best_primal_bound(best_primal_bound);
  proto.set_best_dual_bound(best_dual_bound);
  *proto.mutable_problem_status() = problem_status.ToProto();
  proto.set_simplex_iterations(simplex_iterations);
  proto.set_barrier_iterations(barrier_iterations);
  proto.set_first_order_iterations(first_order_iterations);
  proto.set_node_count(node_count);
  return proto;
}

SolveStats SolveStats::FromProto(const SolveStatsProto& solve_stats_proto) {
  SolveStats result;
  result.solve_time =
      util_time::DecodeGoogleApiProto(solve_stats_proto.solve_time()).value();
  result.best_primal_bound = solve_stats_proto.best_primal_bound();
  result.best_dual_bound = solve_stats_proto.best_dual_bound();
  result.problem_status =
      ProblemStatus::FromProto(solve_stats_proto.problem_status());
  result.simplex_iterations = solve_stats_proto.simplex_iterations();
  result.barrier_iterations = solve_stats_proto.barrier_iterations();
  result.first_order_iterations = solve_stats_proto.first_order_iterations();
  result.node_count = solve_stats_proto.node_count();
  return result;
}

std::ostream& operator<<(std::ostream& ostr, const SolveStats& solve_stats) {
  ostr << "{solve_time: " << solve_stats.solve_time;
  ostr << ", best_primal_bound: " << solve_stats.best_primal_bound;
  ostr << ", best_dual_bound: " << solve_stats.best_dual_bound;
  ostr << ", problem_status: " << solve_stats.problem_status;
  ostr << ", simplex_iterations: " << solve_stats.simplex_iterations;
  ostr << ", barrier_iterations: " << solve_stats.barrier_iterations;
  ostr << ", first_order_iterations: " << solve_stats.first_order_iterations;
  ostr << ", node_count: " << solve_stats.node_count;
  ostr << "}";
  return ostr;
}

std::string SolveStats::ToString() const {
  std::ostringstream stream;
  stream << *this;
  return stream.str();
}

SolveResult SolveResult::FromProto(const ModelStorage* model,
                                   const SolveResultProto& solve_result_proto) {
  SolveResult result(Termination::FromProto(solve_result_proto.termination()));
  result.solve_stats = SolveStats::FromProto(solve_result_proto.solve_stats());

  for (const SolutionProto& solution : solve_result_proto.solutions()) {
    result.solutions.push_back(Solution::FromProto(model, solution));
  }
  for (const PrimalRayProto& primal_ray : solve_result_proto.primal_rays()) {
    result.primal_rays.push_back(PrimalRay::FromProto(model, primal_ray));
  }
  for (const DualRayProto& dual_ray : solve_result_proto.dual_rays()) {
    result.dual_rays.push_back(DualRay::FromProto(model, dual_ray));
  }
  if (solve_result_proto.has_gscip_output()) {
    result.gscip_solver_specific_output =
        std::move(solve_result_proto.gscip_output());
  }
  return result;
}

bool SolveResult::has_primal_feasible_solution() const {
  return !solutions.empty() && solutions[0].primal_solution.has_value() &&
         (solutions[0].primal_solution->feasibility_status ==
          SolutionStatus::kFeasible);
}

double SolveResult::best_objective_bound() const {
  return solve_stats.best_dual_bound;
}

double SolveResult::objective_value() const {
  CHECK(has_primal_feasible_solution());
  return solutions[0].primal_solution->objective_value;
}

bool SolveResult::bounded() const {
  return solve_stats.problem_status.primal_status ==
             FeasibilityStatus::kFeasible &&
         solve_stats.problem_status.dual_status == FeasibilityStatus::kFeasible;
}

const VariableMap<double>& SolveResult::variable_values() const {
  CHECK(has_primal_feasible_solution());
  return solutions[0].primal_solution->variable_values;
}

const VariableMap<double>& SolveResult::ray_variable_values() const {
  CHECK(has_ray());
  return primal_rays[0].variable_values;
}

bool SolveResult::has_dual_feasible_solution() const {
  return !solutions.empty() && solutions[0].dual_solution.has_value() &&
         (solutions[0].dual_solution->feasibility_status ==
          SolutionStatus::kFeasible);
}

const LinearConstraintMap<double>& SolveResult::dual_values() const {
  CHECK(has_dual_feasible_solution());
  return solutions[0].dual_solution->dual_values;
}

const VariableMap<double>& SolveResult::reduced_costs() const {
  CHECK(has_dual_feasible_solution());
  return solutions[0].dual_solution->reduced_costs;
}
const LinearConstraintMap<double>& SolveResult::ray_dual_values() const {
  CHECK(has_dual_ray());
  return dual_rays[0].dual_values;
}

const VariableMap<double>& SolveResult::ray_reduced_costs() const {
  CHECK(has_dual_ray());
  return dual_rays[0].reduced_costs;
}

bool SolveResult::has_basis() const {
  return !solutions.empty() && solutions[0].basis.has_value();
}

const LinearConstraintMap<BasisStatus>& SolveResult::constraint_status() const {
  CHECK(has_basis());
  return solutions[0].basis->constraint_status;
}

const VariableMap<BasisStatus>& SolveResult::variable_status() const {
  CHECK(has_basis());
  return solutions[0].basis->variable_status;
}

}  // namespace math_opt
}  // namespace operations_research
