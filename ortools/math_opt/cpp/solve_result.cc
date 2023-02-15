// Copyright 2010-2022 Google LLC
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
#include <ostream>
#include <sstream>
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
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/port/proto_utils.h"
#include "ortools/util/fp_roundtrip_conv.h"
#include "ortools/util/status_macros.h"

namespace operations_research {
namespace math_opt {

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

TerminationProto Termination::Proto() const {
  TerminationProto proto;
  proto.set_reason(EnumToProto(reason));
  proto.set_limit(EnumToProto(limit));
  proto.set_detail(detail);
  return proto;
}

bool Termination::limit_reached() const {
  return reason == TerminationReason::kFeasible ||
         reason == TerminationReason::kNoSolutionFound;
}

absl::StatusOr<Termination> Termination::FromProto(
    const TerminationProto& termination_proto) {
  const std::optional<TerminationReason> reason =
      EnumFromProto(termination_proto.reason());
  if (!reason.has_value()) {
    return absl::InvalidArgumentError("reason must be specified");
  }
  Termination result(*reason, termination_proto.detail());
  result.limit = EnumFromProto(termination_proto.limit());
  return result;
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

ProblemStatusProto ProblemStatus::Proto() const {
  ProblemStatusProto proto;
  proto.set_primal_status(EnumToProto(primal_status));
  proto.set_dual_status(EnumToProto(dual_status));
  proto.set_primal_or_dual_infeasible(primal_or_dual_infeasible);
  return proto;
}

absl::StatusOr<ProblemStatus> ProblemStatus::FromProto(
    const ProblemStatusProto& problem_status_proto) {
  const std::optional<FeasibilityStatus> primal_status =
      EnumFromProto(problem_status_proto.primal_status());
  if (!primal_status.has_value()) {
    return absl::InvalidArgumentError("primal_status must be specified");
  }
  const std::optional<FeasibilityStatus> dual_status =
      EnumFromProto(problem_status_proto.dual_status());
  if (!dual_status.has_value()) {
    return absl::InvalidArgumentError("dual_status must be specified");
  }
  return ProblemStatus{.primal_status = *primal_status,
                       .dual_status = *dual_status,
                       .primal_or_dual_infeasible =
                           problem_status_proto.primal_or_dual_infeasible()};
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

absl::StatusOr<SolveStatsProto> SolveStats::Proto() const {
  SolveStatsProto proto;
  RETURN_IF_ERROR(
      util_time::EncodeGoogleApiProto(solve_time, proto.mutable_solve_time()))
      << "invalid solve_time (value must be finite)";
  proto.set_best_primal_bound(best_primal_bound);
  proto.set_best_dual_bound(best_dual_bound);
  *proto.mutable_problem_status() = problem_status.Proto();
  proto.set_simplex_iterations(simplex_iterations);
  proto.set_barrier_iterations(barrier_iterations);
  proto.set_first_order_iterations(first_order_iterations);
  proto.set_node_count(node_count);
  return proto;
}

absl::StatusOr<SolveStats> SolveStats::FromProto(
    const SolveStatsProto& solve_stats_proto) {
  SolveStats result;
  OR_ASSIGN_OR_RETURN3(
      result.solve_time,
      util_time::DecodeGoogleApiProto(solve_stats_proto.solve_time()),
      _ << "invalid solve_time");
  result.best_primal_bound = solve_stats_proto.best_primal_bound();
  result.best_dual_bound = solve_stats_proto.best_dual_bound();
  OR_ASSIGN_OR_RETURN3(
      result.problem_status,
      ProblemStatus::FromProto(solve_stats_proto.problem_status()),
      _ << "invalid problem_status");
  result.simplex_iterations = solve_stats_proto.simplex_iterations();
  result.barrier_iterations = solve_stats_proto.barrier_iterations();
  result.first_order_iterations = solve_stats_proto.first_order_iterations();
  result.node_count = solve_stats_proto.node_count();
  return result;
}

std::ostream& operator<<(std::ostream& ostr, const SolveStats& solve_stats) {
  ostr << "{solve_time: " << solve_stats.solve_time;
  ostr << ", best_primal_bound: "
       << RoundTripDoubleFormat(solve_stats.best_primal_bound);
  ostr << ", best_dual_bound: "
       << RoundTripDoubleFormat(solve_stats.best_dual_bound);
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

absl::Status CheckSolverSpecificOutputEmpty(const SolveResultProto& result) {
  if (result.solver_specific_output_case() ==
      SolveResultProto::SOLVER_SPECIFIC_OUTPUT_NOT_SET) {
    return absl::OkStatus();
  }
  return util::InvalidArgumentErrorBuilder()
         << "cannot set solver specific output twice, was already "
         << static_cast<int>(result.solver_specific_output_case());
}

absl::StatusOr<SolveResultProto> SolveResult::Proto() const {
  SolveResultProto result;
  *result.mutable_termination() = termination.Proto();
  OR_ASSIGN_OR_RETURN3(*result.mutable_solve_stats(), solve_stats.Proto(),
                       _ << "invalid solve_stats");
  for (const Solution& solution : solutions) {
    *result.add_solutions() = solution.Proto();
  }
  for (const PrimalRay& primal_ray : primal_rays) {
    *result.add_primal_rays() = primal_ray.Proto();
  }
  for (const DualRay& dual_ray : dual_rays) {
    *result.add_dual_rays() = dual_ray.Proto();
  }
  // See yaqs/5107601535926272 on checking if a proto is empty.
  if (gscip_solver_specific_output.ByteSizeLong() > 0) {
    *result.mutable_gscip_output() = gscip_solver_specific_output;
  }
  return result;
}

absl::StatusOr<SolveResult> SolveResult::FromProto(
    const ModelStorage* model, const SolveResultProto& solve_result_proto) {
  OR_ASSIGN_OR_RETURN3(auto termination,
                       Termination::FromProto(solve_result_proto.termination()),
                       _ << "invalid termination");
  SolveResult result(std::move(termination));
  OR_ASSIGN_OR_RETURN3(result.solve_stats,
                       SolveStats::FromProto(solve_result_proto.solve_stats()),
                       _ << "invalid solve_stats");

  for (int i = 0; i < solve_result_proto.solutions_size(); ++i) {
    OR_ASSIGN_OR_RETURN3(
        auto solution,
        Solution::FromProto(model, solve_result_proto.solutions(i)),
        _ << "invalid solution at index " << i);
    result.solutions.push_back(std::move(solution));
  }
  for (int i = 0; i < solve_result_proto.primal_rays_size(); ++i) {
    OR_ASSIGN_OR_RETURN3(
        auto primal_ray,
        PrimalRay::FromProto(model, solve_result_proto.primal_rays(i)),
        _ << "invalid primal ray at index " << i);
    result.primal_rays.push_back(std::move(primal_ray));
  }
  for (int i = 0; i < solve_result_proto.dual_rays_size(); ++i) {
    OR_ASSIGN_OR_RETURN3(
        auto dual_ray,
        DualRay::FromProto(model, solve_result_proto.dual_rays(i)),
        _ << "invalid dual ray at index " << i);
    result.dual_rays.push_back(std::move(dual_ray));
  }
  switch (solve_result_proto.solver_specific_output_case()) {
    case SolveResultProto::kGscipOutput:
      result.gscip_solver_specific_output = solve_result_proto.gscip_output();
      return result;
    case SolveResultProto::SOLVER_SPECIFIC_OUTPUT_NOT_SET:
      return result;
  }
  return util::InvalidArgumentErrorBuilder()
         << "unexpected value of solver_specific_output_case "
         << solve_result_proto.solver_specific_output_case();
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

namespace {
// Prints only the vector size, not its content.
template <typename T>
void PrintVectorSize(std::ostream& out, const std::vector<T>& v) {
  out << '[';
  if (!v.empty()) {
    out << v.size() << " available";
  }
  out << ']';
}
}  // namespace

std::ostream& operator<<(std::ostream& out, const SolveResult& result) {
  out << "{termination: " << result.termination
      << ", solve_stats: " << result.solve_stats << ", solutions: ";
  PrintVectorSize(out, result.solutions);
  out << ", primal_rays: ";
  PrintVectorSize(out, result.primal_rays);
  out << ", dual_rays: ";
  PrintVectorSize(out, result.dual_rays);
  {
    const std::string gscip_specific_output =
        ProtobufShortDebugString(result.gscip_solver_specific_output);
    if (!gscip_specific_output.empty()) {
      out << ", gscip_solver_specific_output: " << gscip_specific_output;
    }
  }
  out << '}';

  return out;
}

}  // namespace math_opt
}  // namespace operations_research
