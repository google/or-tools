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

// IWYU pragma: private, include "ortools/math_opt/cpp/math_opt.h"
// IWYU pragma: friend "ortools/math_opt/cpp/.*"

#ifndef OR_TOOLS_MATH_OPT_CPP_SOLVE_RESULT_H_
#define OR_TOOLS_MATH_OPT_CPP_SOLVE_RESULT_H_

#include <initializer_list>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "ortools/math_opt/cpp/enums.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/solution.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/result.pb.h"  // IWYU pragma: export
#include "ortools/math_opt/solvers/gscip/gscip.pb.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research {
namespace math_opt {

// Problem feasibility status as claimed by the solver (solver is not required
// to return a certificate for the claim).
enum class FeasibilityStatus {
  // Solver does not claim a status.
  kUndetermined = FEASIBILITY_STATUS_UNDETERMINED,

  // Solver claims the problem is feasible.
  kFeasible = FEASIBILITY_STATUS_FEASIBLE,

  // Solver claims the problem is infeasible.
  kInfeasible = FEASIBILITY_STATUS_INFEASIBLE,
};

MATH_OPT_DEFINE_ENUM(FeasibilityStatus, FEASIBILITY_STATUS_UNSPECIFIED);

// Feasibility status of the primal problem and its dual (or the dual of a
// continuous relaxation) as claimed by the solver. The solver is not required
// to return a certificate for the claim (e.g. the solver may claim primal
// feasibility without returning a primal feasible solution). This combined
// status gives a comprehensive description of a solver's claims about
// feasibility and unboundedness of the solved problem. For instance,
//
//   * a feasible status for primal and dual problems indicates the primal is
//     feasible and bounded and likely has an optimal solution (guaranteed for
//     problems without non-linear constraints).
//   * a primal feasible and a dual infeasible status indicates the primal
//     problem is unbounded (i.e. has arbitrarily good solutions).
//
// Note that a dual infeasible status by itself (i.e. accompanied by an
// undetermined primal status) does not imply the primal problem is unbounded as
// we could have both problems be infeasible. Also, while a primal and dual
// feasible status may imply the existence of an optimal solution, it does not
// guarantee the solver has actually found such optimal solution.
struct ProblemStatus {
  // Status for the primal problem.
  FeasibilityStatus primal_status = FeasibilityStatus::kUndetermined;

  // Status for the dual problem (or for the dual of a continuous relaxation).
  FeasibilityStatus dual_status = FeasibilityStatus::kUndetermined;

  // If true, the solver claims the primal or dual problem is infeasible, but
  // it does not know which (or if both are infeasible). Can be true only when
  // primal_problem_status = dual_problem_status = kUndetermined. This extra
  // information is often needed when preprocessing determines there is no
  // optimal solution to the problem (but can't determine if it is due to
  // infeasibility, unboundedness, or both).
  bool primal_or_dual_infeasible = false;

  // Returns an error if the primal_status or dual_status is unspecified.
  static absl::StatusOr<ProblemStatus> FromProto(
      const ProblemStatusProto& problem_status_proto);

  ProblemStatusProto Proto() const;
  std::string ToString() const;
};

std::ostream& operator<<(std::ostream& ostr, const ProblemStatus& status);

struct SolveStats {
  // Elapsed wall clock time as measured by math_opt, roughly the time inside
  // Solver::Solve(). Note: this does not include work done building the model.
  absl::Duration solve_time = absl::ZeroDuration();

  int simplex_iterations = 0;

  int barrier_iterations = 0;

  int first_order_iterations = 0;

  int node_count = 0;

  // Returns an error if converting the problem_status or solve_time fails.
  static absl::StatusOr<SolveStats> FromProto(
      const SolveStatsProto& solve_stats_proto);

  // Will return an error if solve_time is not finite.
  absl::StatusOr<SolveStatsProto> Proto() const;
  std::string ToString() const;
};

std::ostream& operator<<(std::ostream& ostr, const SolveStats& stats);

// The reason a call to Solve() terminates.
enum class TerminationReason {
  // A provably optimal solution (up to numerical tolerances) has been found.
  kOptimal = TERMINATION_REASON_OPTIMAL,

  // The primal problem has no feasible solutions.
  kInfeasible = TERMINATION_REASON_INFEASIBLE,

  // The primal problem is feasible and arbitrarily good solutions can be
  // found along a primal ray.
  kUnbounded = TERMINATION_REASON_UNBOUNDED,

  // The primal problem is either infeasible or unbounded. More details on the
  // problem status may be available in termination.problem_status. Note that
  // Gurobi's unbounded status may be mapped here.
  kInfeasibleOrUnbounded = TERMINATION_REASON_INFEASIBLE_OR_UNBOUNDED,

  // The problem was solved to one of the criteria above (Optimal, Infeasible,
  // Unbounded, or InfeasibleOrUnbounded), but one or more tolerances was not
  // met. Some primal/dual solutions/rays may be present, but either they will
  // be slightly infeasible, or (if the problem was nearly optimal) their may be
  // a gap between the best solution objective and best objective bound.
  //
  // Users can still query primal/dual solutions/rays and solution stats, but
  // they are responsible for dealing with the numerical imprecision.
  kImprecise = TERMINATION_REASON_IMPRECISE,

  // The optimizer reached some kind of limit and a primal feasible solution
  // is returned. See SolveResultProto.limit_detail for detailed description of
  // the kind of limit that was reached.
  kFeasible = TERMINATION_REASON_FEASIBLE,

  // The optimizer reached some kind of limit and it did not find a primal
  // feasible solution. See SolveResultProto.limit_detail for detailed
  // description of the kind of limit that was reached.
  kNoSolutionFound = TERMINATION_REASON_NO_SOLUTION_FOUND,

  // The algorithm stopped because it encountered unrecoverable numerical
  // error. No solution information is available.
  kNumericalError = TERMINATION_REASON_NUMERICAL_ERROR,

  // The algorithm stopped because of an error not covered by one of the
  // statuses defined above. No solution information is available.
  kOtherError = TERMINATION_REASON_OTHER_ERROR
};

MATH_OPT_DEFINE_ENUM(TerminationReason, TERMINATION_REASON_UNSPECIFIED);

// When a Solve() stops early with TerminationReason kFeasible or
// kNoSolutionFound, the specific limit that was hit.
enum class Limit {
  // Used if the underlying solver cannot determine which limit was reached, or
  // as a null value when we terminated not from a limit (e.g. kOptimal).
  kUndetermined = LIMIT_UNDETERMINED,

  // An iterative algorithm stopped after conducting the maximum number of
  // iterations (e.g. simplex or barrier iterations).
  kIteration = LIMIT_ITERATION,

  // The algorithm stopped after a user-specified computation time.
  kTime = LIMIT_TIME,

  // A branch-and-bound algorithm stopped because it explored a maximum number
  // of nodes in the branch-and-bound tree.
  kNode = LIMIT_NODE,

  // The algorithm stopped because it found the required number of solutions.
  // This is often used in MIPs to get the solver to return the first feasible
  // solution it encounters.
  kSolution = LIMIT_SOLUTION,

  // The algorithm stopped because it ran out of memory.
  kMemory = LIMIT_MEMORY,

  // The solver was run with a cutoff (e.g. SolveParameters.cutoff_limit was
  // set) on the objective, indicating that the user did not want any solution
  // worse than the cutoff, and the solver concluded there were no solutions at
  // least as good as the cutoff. Typically no further solution information is
  // provided.
  kCutoff = LIMIT_CUTOFF,

  // The algorithm stopped because it found a solution better than a minimum
  // limit set by the user.
  kObjective = LIMIT_OBJECTIVE,

  // The algorithm stopped because the norm of an iterate became too large.
  kNorm = LIMIT_NORM,

  // The algorithm stopped because of an interrupt signal or a user interrupt
  // request.
  kInterrupted = LIMIT_INTERRUPTED,

  // The algorithm stopped because it was unable to continue making progress
  // towards the solution.
  kSlowProgress = LIMIT_SLOW_PROGRESS,

  // The algorithm stopped due to a limit not covered by one of the above. Note
  // that kUndetermined is used when the reason cannot be determined, and kOther
  // is used when the reason is known but does not fit into any of the above
  // alternatives.
  kOther = LIMIT_OTHER
};

MATH_OPT_DEFINE_ENUM(Limit, LIMIT_UNSPECIFIED);

// Bounds on the optimal objective value.
struct ObjectiveBounds {
  // Solver claims there exists a primal solution that is numerically feasible
  // (i.e. feasible up to the solvers tolerance), and whose objective value is
  // primal_bound.
  //
  // The optimal value is equal or better (smaller for min objectives and larger
  // for max objectives) than primal_bound, but only up to solver-tolerances.
  double primal_bound = 0.0;

  // Solver claims there exists a dual solution that is numerically feasible
  // (i.e. feasible up to the solvers tolerance), and whose objective value is
  // dual_bound.
  //
  // For MIP solvers, the associated dual problem may be some continuous
  // relaxation (e.g. LP relaxation), but it is often an implicitly defined
  // problem that is a complex consequence of the solvers execution. For both
  // continuous and MIP solvers, the optimal value is equal or worse (larger for
  // min objective and smaller for max objectives) than dual_bound, but only up
  // to solver-tolerances. Some continuous solvers provide a numerically safer
  // dual bound through solver's specific output (e.g. for PDLP,
  // pdlp_output.convergence_information.corrected_dual_objective).
  double dual_bound = 0.0;

  // Returns trivial bounds.
  //
  // Trivial bounds are:
  // * for a maximization:
  //   - primal_bound = -inf
  //   - dual_bound   = +inf
  // * for a minimization:
  //   - primal_bound = +inf
  //   - dual_bound   = -inf
  static ObjectiveBounds MakeTrivial(bool is_maximize);
  static ObjectiveBounds MaximizeMakeTrivial();
  static ObjectiveBounds MinimizeMakeTrivial();

  // Returns unbounded bounds.
  //
  // Unbounded bounds are:
  // * for a maximization:
  //   - primal_bound = dual_bound = +inf
  // * for a minimization:
  //   - primal_bound = dual_bound = -inf
  static ObjectiveBounds MakeUnbounded(bool is_maximize);
  static ObjectiveBounds MinimizeMakeUnbounded();
  static ObjectiveBounds MaximizeMakeUnbounded();

  // Sets both bounds to objective_value.
  static ObjectiveBounds MakeOptimal(double objective_value);

  static ObjectiveBounds FromProto(
      const ObjectiveBoundsProto& objective_bounds_proto);
  ObjectiveBoundsProto Proto() const;
  std::string ToString() const;
};

std::ostream& operator<<(std::ostream& ostr,
                         const ObjectiveBounds& objective_bounds);

// All information regarding why a call to Solve() terminated.
struct Termination {
  // Returns a Termination with the provided reason and details along with
  // trivial bounds and kUndetermined statuses.
  // A variety of static factory functions are provided below for common
  // Termination conditions, generally prefer these if applicable.
  Termination(bool is_maximize, TerminationReason reason,
              std::string detail = {});

  // Additional information in `limit` when value is kFeasible or
  // kNoSolutionFound, see `limit` for details.
  TerminationReason reason;

  // A Termination within a SolveResult returned by math_opt::Solve() satisfies
  // some additional invariants:
  //  * limit is set iff reason is kFeasible or kNoSolutionFound.
  //  * if the limit is kCutoff, the termination reason will be
  //  kNoSolutionFound.
  std::optional<Limit> limit;

  // Additional typically solver specific information about termination.
  // Not all solvers can always determine the limit which caused termination,
  // Limit::kUndetermined is used when the cause cannot be determined.
  std::string detail;

  // Feasibility statuses for primal and dual problems.
  ProblemStatus problem_status;

  // Bounds on the optimal objective value.
  ObjectiveBounds objective_bounds;

  // Returns true if a limit was reached (i.e. if reason is kFeasible or
  // kNoSolutionFound, and limit is not empty).
  bool limit_reached() const;

  // Returns an OkStatus if the reason of this `Termination` is
  // `TerminationReason::kOptimal` or `TerminationReason::kFeasible`, or an
  // `InternalError` otherwise.
  absl::Status EnsureIsOptimalOrFeasible() const;

  // Returns true if the reason of this Termination` is
  // `TerminationReason::kOptimal` or `TerminationReason::kFeasible`, or false
  // otherwise.
  bool IsOptimalOrFeasible() const;

  // Returns an OkStatus if the reason of this `Termination` is
  // `TerminationReason::kOptimal`, or an `InternalError` otherwise.
  //
  // In most use cases, at least for MIPs, `EnsureIsOptimalOrFeasible` should be
  // used instead.
  absl::Status EnsureIsOptimal() const;

  // Returns true if the reason of this `Termination` is
  // `TerminationReason::kOptimal`, or false otherwise.
  //
  // In most use cases, at least for MIPs, `IsOptimalOrFeasible` should be
  // used instead.
  bool IsOptimal() const;

  // Returns an OkStatus if the reason of this `Termination` is `reason`, or an
  // `InternalError` otherwise.
  absl::Status EnsureReasonIs(TerminationReason reason) const;

  // Returns an OkStatus if the reason of this `Termination` is in `reasons`, or
  // an `InternalError` otherwise.
  absl::Status EnsureReasonIsAnyOf(
      std::initializer_list<TerminationReason> reasons) const;

  // Returns termination with reason kOptimal, the provided objective for both
  // primal and dual bounds, and kFeasible primal and dual statuses.
  static Termination Optimal(double objective_value, std::string detail = {});

  // Returns termination with reason kOptimal, the provided objective bounds and
  // kFeasible primal and dual statuses.
  static Termination Optimal(double primal_objective_value,
                             double dual_objective_value,
                             std::string detail = {});

  // Returns a termination with reason kInfeasible, primal status kInfeasible
  // and the provided dual status.
  //
  // It sets a trivial primal bound and a dual bound based on the provided dual
  // status, which should be kFeasible or kUndetermined. If the dual status is
  // kUndetermined, then the dual bound will be trivial and if the dual status
  // is kFeasible, then the dual bound will be equal to the primal bound.
  //
  // The convention for infeasible MIPs is that dual_feasibility_status is
  // feasible (There always exist a dual feasible convex relaxation of an
  // infeasible MIP).
  static Termination Infeasible(bool is_maximize,
                                FeasibilityStatus dual_feasibility_status =
                                    FeasibilityStatus::kUndetermined,
                                std::string detail = {});

  // Returns a termination with reason kInfeasibleOrUnbounded, primal status
  // kUndetermined, the provided dual status (which should be kUndetermined or
  // kInfeasible) and trivial bounds.
  //
  // primal_or_dual_infeasible is set if dual_feasibility_status is
  // kUndetermined.
  static Termination InfeasibleOrUnbounded(
      bool is_maximize,
      FeasibilityStatus dual_feasibility_status =
          FeasibilityStatus::kUndetermined,
      std::string detail = {});

  // Returns a termination with reason kUnbounded, primal status kFeasible,
  // dual status kInfeasible and unbounded bounds.
  static Termination Unbounded(bool is_maximize, std::string detail = {});

  // Returns a termination with reason kNoSolutionFound and primal status
  // kUndetermined.
  //
  // Assumes dual solution exists iff optional_dual_objective is set even if
  // infinite (some solvers return feasible dual solutions without an objective
  // value). optional_dual_objective should not be set when limit is
  // kCutoff for a valid TerminationProto to be returned (use LimitCutoff()
  // below instead).
  //
  // It sets a trivial primal bound. The dual bound is either set to the
  // optional_dual_objective if set, else to a trivial value.
  //
  // TODO(b/290359402): Consider improving to require a finite dual bound when
  // dual feasible solutions are returned.
  static Termination NoSolutionFound(
      bool is_maximize, Limit limit,
      std::optional<double> optional_dual_objective = std::nullopt,
      std::string detail = {});

  // Returns a termination with reason kFeasible and primal status kFeasible.
  // The dual status depends on optional_dual_objective.
  //
  // finite_primal_objective should be finite and limit should not be
  // kCutoff for a valid TerminationProto to be returned (use LimitCutoff()
  // below instead).
  //
  // Assumes dual solution exists iff optional_dual_objective is set even if
  // infinite (some solvers return feasible dual solutions without an objective
  // value). If set the dual status is set to kFeasible, else
  // it is kUndetermined.
  //
  // It sets the primal bound based on the primal objective. The dual bound is
  // either set to the optional_dual_objective if set, else to a trivial value.
  //
  // TODO(b/290359402): Consider improving to require a finite dual bound when
  // dual feasible solutions are returned.
  static Termination Feasible(
      bool is_maximize, Limit limit, double finite_primal_objective,
      std::optional<double> optional_dual_objective = std::nullopt,
      std::string detail = {});

  // Calls NoSolutionFound() with LIMIT_CUTOFF LIMIT.
  static Termination Cutoff(bool is_maximize, std::string detail = {});

  // Will return an error if termination_proto.reason is UNSPECIFIED.
  static absl::StatusOr<Termination> FromProto(
      const TerminationProto& termination_proto);
  TerminationProto Proto() const;

  std::string ToString() const;
};

std::ostream& operator<<(std::ostream& ostr, const Termination& termination);

template <typename Sink>
void AbslStringify(Sink& sink, const Termination& termination) {
  sink.Append(termination.ToString());
}

// The result of solving an optimization problem with Solve().
struct SolveResult {
  explicit SolveResult(Termination termination)
      : termination(std::move(termination)) {}

  // The reason the solver stopped.
  Termination termination;

  // Statistics on the solve process, e.g. running time, iterations.
  SolveStats solve_stats;

  //  Basic solutions use, as of Nov 2021:
  //    * All convex optimization solvers (LP, convex QP) return only one
  //      solution as a primal dual pair.
  //    * Only MI(Q)P solvers return more than one solution. MIP solvers do not
  //      return any dual information, or primal infeasible solutions. Solutions
  //      are returned in order of best primal objective first. Gurobi solves
  //      nonconvex QP (integer or continuous) as MIQP.

  // The general contract for the order of solutions that future solvers should
  // implement is to order by:
  //   1. The solutions with a primal feasible solution, ordered by best primal
  //      objective first.
  //   2. The solutions with a dual feasible solution, ordered by best dual
  //      objective (unknown dual objective is worst)
  //   3. All remaining solutions can be returned in any order.
  std::vector<Solution> solutions;

  // Directions of unbounded primal improvement, or equivalently, dual
  // infeasibility certificates. Typically provided for TerminationReasons
  // kUnbounded and kInfeasibleOrUnbounded.
  std::vector<PrimalRay> primal_rays;

  // Directions of unbounded dual improvement, or equivalently, primal
  // infeasibility certificates. Typically provided for TerminationReason
  // kInfeasible.
  std::vector<DualRay> dual_rays;

  // Solver specific output from Gscip. Only populated if Gscip is used.
  GScipOutput gscip_solver_specific_output;
  // Solver specific output from Pdlp. Only populated if Pdlp is used.
  SolveResultProto::PdlpOutput pdlp_solver_specific_output;

  // Returns the SolveResult equivalent of solve_result_proto.
  //
  // Returns an error if:
  //  * Any solution or ray cannot be read from proto (e.g. on a subfield,
  //      ids.size != values.size).
  //  * termination or solve_stats cannot be read from proto.
  // See the FromProto() functions for these types for details.
  //
  // Note: this is (intentionally) a much weaker test than ValidateResult(). The
  // guarantees are just strong enough to ensure that a SolveResult and
  // SolveResultProto can round trip cleanly, e.g. we do not check that a
  // termination reason optimal implies that there is at least one primal
  // feasible solution.
  //
  // While ValidateResult() is called automatically when you are solving
  // locally, users who are reading a solution from disk, solving remotely, or
  // getting their SolveResultProto (or SolveResult) by any other means are
  // encouraged to either call ValidateResult() themselves, do their own
  // validation, or not rely on the strong guarantees of ValidateResult()
  // and just treat SolveResult as a simple struct.
  static absl::StatusOr<SolveResult> FromProto(
      ModelStorageCPtr model, const SolveResultProto& solve_result_proto);

  // Returns the proto equivalent of this.
  //
  // Note that the proto uses a oneof for solver specific output. This method
  // will fail if multiple solver specific outputs are set.
  //
  // TODO(b/231134639): investigate removing the oneof from the proto.
  absl::StatusOr<SolveResultProto> Proto() const;

  absl::Duration solve_time() const { return solve_stats.solve_time; }

  // A primal bound on the optimal objective value as described in
  // ObjectiveBounds. Will return a valid (possibly infinite) bound even if
  // no primal feasible solutions are available.
  double primal_bound() const;

  // A dual bound on the optimal objective value as described in
  // ObjectiveBounds. Will return a valid (possibly infinite) bound even if
  // no dual feasible solutions are available.
  double dual_bound() const;

  // Indicates if at least one primal feasible solution is available.
  //
  // For SolveResults generated by calling Solver::Solve(), when
  // termination.reason is TerminationReason::kOptimal or
  // TerminationReason::kFeasible, this is guaranteed to be true and need not be
  // checked. SolveResult objects generated directed from a proto need not have
  // this property.
  bool has_primal_feasible_solution() const;

  // Returns the best primal feasible solution. CHECK fails if no such solution
  // is available; check this using `has_primal_feasible_solution()`.
  const PrimalSolution& best_primal_solution() const;

  // The objective value of the best primal feasible solution. Will CHECK fail
  // if there are no primal feasible solutions.
  //
  // primal_bound() above is guaranteed to be at least as good (larger or equal
  // for max problems and smaller or equal for min problems) as
  // objective_value() and will never CHECK fail, so it may be preferable in
  // some cases. Note that primal_bound() could be better than objective_value()
  // even for optimal terminations, but on such optimal termination, both should
  // satisfy the optimality tolerances.
  double objective_value() const;
  double objective_value(Objective objective) const;

  // A bound on the best possible objective value.
  //
  // best_objective_bound() is always equal to dual_bound(), so they can be
  // used interchangeably.
  double best_objective_bound() const;

  // The variable values from the best primal feasible solution. Will CHECK fail
  // if there are no primal feasible solutions.
  const VariableMap<double>& variable_values() const;

  // Returns true only if the problem has been shown to be feasible and bounded.
  bool bounded() const;

  // Indicates if at least one primal ray is available.
  //
  // This is NOT guaranteed to be true when termination.reason is
  // TerminationReason::kUnbounded or TerminationReason::kInfeasibleOrUnbounded.
  bool has_ray() const { return !primal_rays.empty(); }

  // The variable values from the first primal ray. Will CHECK fail if there
  // are no primal rays.
  const VariableMap<double>& ray_variable_values() const;

  // Indicates if the best solution has an associated dual feasible solution.
  //
  // This is NOT guaranteed to be true when termination.reason is
  // TerminationReason::kOptimal. It also may be true even when the best
  // solution does not have an associated primal feasible solution.
  bool has_dual_feasible_solution() const;

  // The dual values associated to the best solution.
  //
  // If there is at least one primal feasible solution, this corresponds to the
  // dual values associated to the best primal feasible solution. Will CHECK
  // fail if the best solution does not have an associated dual feasible
  // solution.
  const LinearConstraintMap<double>& dual_values() const;

  // The reduced costs associated to the best solution.
  //
  // If there is at least one primal feasible solution, this corresponds to the
  // reduced costs associated to the best primal feasible solution. Will CHECK
  // fail if the best solution does not have an associated dual feasible
  // solution.
  const VariableMap<double>& reduced_costs() const;

  // Indicates if at least one dual ray is available.
  //
  // This is NOT guaranteed to be true when termination.reason is
  // TerminationReason::kInfeasible.
  bool has_dual_ray() const { return !dual_rays.empty(); }

  // The dual values from the first dual ray. Will CHECK fail if there
  // are no dual rays.
  const LinearConstraintMap<double>& ray_dual_values() const;

  // The reduced from the first dual ray. Will CHECK fail if there
  // are no dual rays.
  const VariableMap<double>& ray_reduced_costs() const;

  // Indicates if the best solution has an associated basis.
  bool has_basis() const;

  // The constraint basis status for the best solution. Will CHECK fail if the
  // best solution does not have an associated basis.
  const LinearConstraintMap<BasisStatus>& constraint_status() const;

  // The variable basis status for the best solution. Will CHECK fail if the
  // best solution does not have an associated basis.
  const VariableMap<BasisStatus>& variable_status() const;
};

// Prints a summary of the solve result on a single line.
//
// This prints the number of available solutions and rays instead of their
// values.
//
// Printing the whole solution could be problematic for huge models.
std::ostream& operator<<(std::ostream& out, const SolveResult& result);

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_SOLVE_RESULT_H_
