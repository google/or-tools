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

#include "ortools/pdlp/primal_dual_hybrid_gradient.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/timer.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/preprocessor.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/proto_utils.h"
#include "ortools/pdlp/iteration_stats.h"
#include "ortools/pdlp/quadratic_program.h"
#include "ortools/pdlp/sharded_optimization_utils.h"
#include "ortools/pdlp/sharded_quadratic_program.h"
#include "ortools/pdlp/sharder.h"
#include "ortools/pdlp/solve_log.pb.h"
#include "ortools/pdlp/solvers.pb.h"
#include "ortools/pdlp/solvers_proto_validation.h"
#include "ortools/pdlp/termination.h"
#include "ortools/pdlp/trust_region.h"

namespace operations_research::pdlp {

namespace {

using ::Eigen::VectorXd;

using IterationStatsCallback =
    std::function<void(const IterationCallbackInfo&)>;

// Computes a `num_threads' that is capped by the problem size and num_shards,
// if specified, to avoid creating unusable threads.
int NumThreads(const int num_threads, const int num_shards,
               const QuadraticProgram& qp) {
  int capped_num_threads = num_threads;
  if (num_shards > 0) {
    capped_num_threads = std::min(capped_num_threads, num_shards);
  }
  const int64_t problem_limit = std::max(qp.variable_lower_bounds.size(),
                                         qp.constraint_lower_bounds.size());
  capped_num_threads =
      static_cast<int>(std::min(int64_t{capped_num_threads}, problem_limit));
  capped_num_threads = std::max(capped_num_threads, 1);
  if (capped_num_threads != num_threads) {
    LOG(WARNING) << "Reducing num_threads from " << num_threads << " to "
                 << capped_num_threads
                 << " because additional threads would be useless.";
  }
  return capped_num_threads;
}

// If `num_shards' is positive, returns it. Otherwise returns a reasonable
// number of shards to use with ShardedQuadraticProgram for the given number of
// threads.
int NumShards(const int num_threads, const int num_shards) {
  if (num_shards > 0) return num_shards;
  return num_threads == 1 ? 1 : 4 * num_threads;
}

std::string ToString(const ConvergenceInformation& convergence_information,
                     const RelativeConvergenceInformation& relative_information,
                     const OptimalityNorm residual_norm) {
  constexpr absl::string_view kFormatStr =
      "%#12.6g %#12.6g %#12.6g | %#12.6g %#12.6g %#12.6g | %#12.6g %#12.6g | "
      "%#12.6g %#12.6g";
  switch (residual_norm) {
    case OPTIMALITY_NORM_L_INF:
      return absl::StrFormat(
          kFormatStr, relative_information.relative_l_inf_primal_residual,
          relative_information.relative_l_inf_dual_residual,
          relative_information.relative_optimality_gap,
          convergence_information.l_inf_primal_residual(),
          convergence_information.l_inf_dual_residual(),
          convergence_information.primal_objective() -
              convergence_information.dual_objective(),
          convergence_information.primal_objective(),
          convergence_information.dual_objective(),
          convergence_information.l2_primal_variable(),
          convergence_information.l2_dual_variable());
    case OPTIMALITY_NORM_L2:
      return absl::StrFormat(kFormatStr,
                             relative_information.relative_l2_primal_residual,
                             relative_information.relative_l2_dual_residual,
                             relative_information.relative_optimality_gap,
                             convergence_information.l2_primal_residual(),
                             convergence_information.l2_dual_residual(),
                             convergence_information.primal_objective() -
                                 convergence_information.dual_objective(),
                             convergence_information.primal_objective(),
                             convergence_information.dual_objective(),
                             convergence_information.l2_primal_variable(),
                             convergence_information.l2_dual_variable());
    case OPTIMALITY_NORM_L_INF_COMPONENTWISE:
      return absl::StrFormat(
          kFormatStr,
          convergence_information.l_inf_componentwise_primal_residual(),
          convergence_information.l_inf_componentwise_dual_residual(),
          relative_information.relative_optimality_gap,
          convergence_information.l_inf_primal_residual(),
          convergence_information.l_inf_dual_residual(),
          convergence_information.primal_objective() -
              convergence_information.dual_objective(),
          convergence_information.primal_objective(),
          convergence_information.dual_objective(),
          convergence_information.l2_primal_variable(),
          convergence_information.l2_dual_variable());
    case OPTIMALITY_NORM_UNSPECIFIED:
      LOG(FATAL) << "Invalid residual norm.";
  }
}

std::string ToShortString(
    const ConvergenceInformation& convergence_information,
    const RelativeConvergenceInformation& relative_information,
    const OptimalityNorm residual_norm) {
  constexpr absl::string_view kFormatStr =
      "%#10.4g %#10.4g %#10.4g | %#10.4g %#10.4g";
  switch (residual_norm) {
    case OPTIMALITY_NORM_L_INF:
      return absl::StrFormat(
          kFormatStr, relative_information.relative_l_inf_primal_residual,
          relative_information.relative_l_inf_dual_residual,
          relative_information.relative_optimality_gap,
          convergence_information.primal_objective(),
          convergence_information.dual_objective());
    case OPTIMALITY_NORM_L2:
      return absl::StrFormat(kFormatStr,
                             relative_information.relative_l2_primal_residual,
                             relative_information.relative_l2_dual_residual,
                             relative_information.relative_optimality_gap,
                             convergence_information.primal_objective(),
                             convergence_information.dual_objective());
    case OPTIMALITY_NORM_L_INF_COMPONENTWISE:
      return absl::StrFormat(
          kFormatStr,
          convergence_information.l_inf_componentwise_primal_residual(),
          convergence_information.l_inf_componentwise_dual_residual(),
          relative_information.relative_optimality_gap,
          convergence_information.primal_objective(),
          convergence_information.dual_objective());
    case OPTIMALITY_NORM_UNSPECIFIED:
      LOG(FATAL) << "Invalid residual norm.";
  }
}

// Returns a string describing iter_stats, based on the ConvergenceInformation
// with candidate_type==preferred_candidate if one exists, otherwise based on
// the first value, if any. termination_criteria.optimality_norm determines the
// norm in which the residuals are displayed.
std::string ToString(const IterationStats& iter_stats,
                     const TerminationCriteria& termination_criteria,
                     const QuadraticProgramBoundNorms& bound_norms,
                     PointType preferred_candidate) {
  std::string iteration_string =
      absl::StrFormat("%6d %8.1f %6.1f", iter_stats.iteration_number(),
                      iter_stats.cumulative_kkt_matrix_passes(),
                      iter_stats.cumulative_time_sec());
  auto convergence_information =
      GetConvergenceInformation(iter_stats, preferred_candidate);
  if (!convergence_information.has_value() &&
      iter_stats.convergence_information_size() > 0) {
    convergence_information = iter_stats.convergence_information(0);
  }
  if (convergence_information.has_value()) {
    const RelativeConvergenceInformation relative_information =
        ComputeRelativeResiduals(
            EffectiveOptimalityCriteria(termination_criteria),
            *convergence_information, bound_norms);
    return absl::StrCat(iteration_string, " | ",
                        ToString(*convergence_information, relative_information,
                                 termination_criteria.optimality_norm()));
  }
  return iteration_string;
}

std::string ToShortString(const IterationStats& iter_stats,
                          const TerminationCriteria& termination_criteria,
                          const QuadraticProgramBoundNorms& bound_norms,
                          PointType preferred_candidate) {
  std::string iteration_string =
      absl::StrFormat("%6d %6.1f", iter_stats.iteration_number(),
                      iter_stats.cumulative_time_sec());
  auto convergence_information =
      GetConvergenceInformation(iter_stats, preferred_candidate);
  if (!convergence_information.has_value() &&
      iter_stats.convergence_information_size() > 0) {
    convergence_information = iter_stats.convergence_information(0);
  }
  if (convergence_information.has_value()) {
    const RelativeConvergenceInformation relative_information =
        ComputeRelativeResiduals(
            EffectiveOptimalityCriteria(termination_criteria),
            *convergence_information, bound_norms);
    return absl::StrCat(
        iteration_string, " | ",
        ToShortString(*convergence_information, relative_information,
                      termination_criteria.optimality_norm()));
  }
  return iteration_string;
}

// Returns a label string corresponding to the format of ToString().
std::string ConvergenceInformationLabelString() {
  return absl::StrFormat(
      "%12s %12s %12s | %12s %12s %12s | %12s %12s | %12s %12s", "rel_prim_res",
      "rel_dual_res", "rel_gap", "prim_resid", "dual_resid", "obj_gap",
      "prim_obj", "dual_obj", "prim_var_l2", "dual_var_l2");
}

std::string ConvergenceInformationLabelShortString() {
  return absl::StrFormat("%10s %10s %10s | %10s %10s", "rel_p_res", "rel_d_res",
                         "rel_gap", "prim_obj", "dual_obj");
}

std::string IterationStatsLabelString() {
  return absl::StrCat(
      absl::StrFormat("%6s %8s %6s", "iter#", "kkt_pass", "time"), " | ",
      ConvergenceInformationLabelString());
}

std::string IterationStatsLabelShortString() {
  return absl::StrCat(absl::StrFormat("%6s %6s", "iter#", "time"), " | ",
                      ConvergenceInformationLabelShortString());
}

enum class InnerStepOutcome {
  kSuccessful,
  kForceNumericalTermination,
};

// Makes the closing changes to the SolveLog and builds a SolverResult.
// NOTE: The primal_solution, dual_solution, and solve_log are passed by value.
// To avoid unnecessary copying, move these objects (i.e. use std::move()).
SolverResult ConstructSolverResult(VectorXd primal_solution,
                                   VectorXd dual_solution,
                                   const IterationStats& stats,
                                   TerminationReason termination_reason,
                                   PointType output_type, SolveLog solve_log) {
  solve_log.set_iteration_count(stats.iteration_number());
  solve_log.set_termination_reason(termination_reason);
  solve_log.set_solution_type(output_type);
  solve_log.set_solve_time_sec(stats.cumulative_time_sec());
  *solve_log.mutable_solution_stats() = stats;
  return SolverResult{.primal_solution = std::move(primal_solution),
                      .dual_solution = std::move(dual_solution),
                      .solve_log = std::move(solve_log)};
}

class PreprocessSolver {
 public:
  // Assumes that the qp and params are valid.
  // Note that the qp is intentionally passed by value.
  // NOTE: Many PreprocessSolver methods accept a params argument. This is
  // passed as an argument instead of stored as a member variable to support
  // using different params in different contexts with the same PreprocessSolver
  // object.
  explicit PreprocessSolver(QuadraticProgram qp,
                            const PrimalDualHybridGradientParams& params);

  // Not copyable or movable (because glop::MainLpPreprocessor isn't).
  PreprocessSolver(const PreprocessSolver&) = delete;
  PreprocessSolver& operator=(const PreprocessSolver&) = delete;
  PreprocessSolver(PreprocessSolver&&) = delete;
  PreprocessSolver& operator=(PreprocessSolver&&) = delete;

  // Zero is used if initial_solution is nullopt. If interrupt_solve is not
  // nullptr, then the solver will periodically check if interrupt_solve->load()
  // is true, in which case the solve will terminate with
  // TERMINATION_REASON_INTERRUPTED_BY_USER. Ownership is not transferred. If
  // iteration_stats_callback is not nullptr, then at each termination step
  // (when iteration stats are logged), iteration_stats_callback will also
  // be called with those iteration stats.
  SolverResult PreprocessAndSolve(
      const PrimalDualHybridGradientParams& params,
      std::optional<PrimalAndDualSolution> initial_solution,
      const std::atomic<bool>* interrupt_solve,
      IterationStatsCallback iteration_stats_callback);

  // Returns a TerminationReasonAndPointType when the termination criteria are
  // satisfied, otherwise returns nothing. The pointers to working_* can be
  // nullptr if an iterate of that type is not available. For the iterate types
  // that are available, uses the primal and dual vectors to compute solution
  // statistics and adds them to the stats proto.
  // NOTE: The primal and dual input pairs should be scaled solutions.
  std::optional<TerminationReasonAndPointType>
  UpdateIterationStatsAndCheckTermination(
      const PrimalDualHybridGradientParams& params,
      bool force_numerical_termination, const VectorXd& working_primal_current,
      const VectorXd& working_dual_current,
      const VectorXd* working_primal_average,
      const VectorXd* working_dual_average,
      const VectorXd* working_primal_delta, const VectorXd* working_dual_delta,
      const VectorXd& last_primal_start_point,
      const VectorXd& last_dual_start_point,
      const std::atomic<bool>* interrupt_solve, IterationStats& stats) const;

  // Returns the solution statistics for the primal and dual input pair, which
  // should be a scaled solution.
  ConvergenceInformation ComputeConvergenceInformationFromWorkingSolution(
      const PrimalDualHybridGradientParams& params,
      const VectorXd& working_primal, const VectorXd& working_dual,
      PointType candidate_type) const;

  // Returns a SolverResult for the original problem, given a SolverResult
  // from the scaled or preprocessed problem. Also computes the reduced costs.
  // NOTE: 'result' is passed by value. To avoid unnecessary copying, move this
  // object (i.e. use std::move()).
  SolverResult ConstructOriginalSolverResult(
      const PrimalDualHybridGradientParams& params, SolverResult result) const;

  const ShardedQuadraticProgram& ShardedWorkingQp() const {
    return sharded_qp_;
  }

  // Returns elapsed time (including preprocessing) in seconds.
  double GetElapsedTime() const { return timer_.Get(); }

 private:
  struct PresolveInfo {
    explicit PresolveInfo(ShardedQuadraticProgram original_qp,
                          const PrimalDualHybridGradientParams& params)
        : preprocessor_parameters(PreprocessorParameters(params)),
          preprocessor(&preprocessor_parameters),
          sharded_original_qp(std::move(original_qp)),
          trivial_col_scaling_vec(
              OnesVector(sharded_original_qp.PrimalSharder())),
          trivial_row_scaling_vec(
              OnesVector(sharded_original_qp.DualSharder())) {}

    glop::GlopParameters preprocessor_parameters;
    glop::MainLpPreprocessor preprocessor;
    ShardedQuadraticProgram sharded_original_qp;
    bool presolved_problem_was_maximization = false;
    const VectorXd trivial_col_scaling_vec, trivial_row_scaling_vec;
  };

  // TODO(user): experiment with different preprocessor types.
  static glop::GlopParameters PreprocessorParameters(
      const PrimalDualHybridGradientParams& params);

  // If presolve is enabled, moves sharded_qp_ to
  // presolve_info_.sharded_original_qp and computes the presolved linear
  // program and installs it in sharded_qp_. Clears initial_solution if
  // presolve is enabled. If presolve solves the problem completely returns the
  // appropriate TerminationReason. Otherwise returns nullopt. If presolve
  // is disabled or an error occurs modifies nothing and returns nullopt.
  std::optional<TerminationReason> ApplyPresolveIfEnabled(
      const PrimalDualHybridGradientParams& params,
      std::optional<PrimalAndDualSolution>* initial_solution);

  void ComputeAndApplyRescaling(const PrimalDualHybridGradientParams& params,
                                VectorXd& starting_primal_solution,
                                VectorXd& starting_dual_solution);

  void LogQuadraticProgramStats(const QuadraticProgramStats& stats);

  double InitialPrimalWeight(const PrimalDualHybridGradientParams& params,
                             double l2_norm_primal_linear_objective,
                             double l2_norm_constraint_bounds) const;

  PrimalAndDualSolution RecoverOriginalSolution(
      PrimalAndDualSolution working_solution) const;

  // Adds one entry of convergence information and infeasibility information to
  // stats using the input solutions. The primal_solution and dual_solution are
  // solutions for sharded_qp. The col_scaling_vec and row_scaling_vec are used
  // to implicitly unscale sharded_qp when computing the relevant information.
  void AddConvergenceAndInfeasibilityInformation(
      const PrimalDualHybridGradientParams& params,
      const VectorXd& primal_solution, const VectorXd& dual_solution,
      const ShardedQuadraticProgram& sharded_qp,
      const VectorXd& col_scaling_vec, const VectorXd& row_scaling_vec,
      PointType candidate_type, IterationStats& stats) const;

  // Adds one entry of PointMetadata to stats using the input solutions.
  void AddPointMetadata(const PrimalDualHybridGradientParams& params,
                        const VectorXd& primal_solution,
                        const VectorXd& dual_solution, PointType point_type,
                        const VectorXd& last_primal_start_point,
                        const VectorXd& last_dual_start_point,
                        IterationStats& stats) const;

  const QuadraticProgram& Qp() const { return sharded_qp_.Qp(); }

  const int num_threads_;
  const int num_shards_;

  // The bound norms of the original problem.
  QuadraticProgramBoundNorms original_bound_norms_;

  // This is the QP that PDHG is run on. It is modified by presolve and
  // rescaling, if those are enabled, and then serves as the ShardedWorkingQp()
  // when calling Solver::Solve. The original problem is available in
  // presolve_info_->sharded_original_qp if presolve_info_.has_value(), and
  // otherwise can be obtained by undoing the scaling of sharded_qp_ by
  // col_scaling_vec_ and row_scaling_vec_.
  ShardedQuadraticProgram sharded_qp_;

  // Set iff presolve is enabled.
  std::optional<PresolveInfo> presolve_info_;

  // The scaling vectors that map the original (or presolved) quadratic program
  // to the working version. See
  // ShardedQuadraticProgram::RescaleQuadraticProgram() for details.
  VectorXd col_scaling_vec_;
  VectorXd row_scaling_vec_;

  WallTimer timer_;
  IterationStatsCallback iteration_stats_callback_;
};

class Solver {
 public:
  // preprocess_solver should not be nullptr, and the PreprocessSolver object
  // must outlive this Solver object. ownership is not transferred.
  explicit Solver(const PrimalDualHybridGradientParams& params,
                  VectorXd starting_primal_solution,
                  VectorXd starting_dual_solution, double initial_step_size,
                  double initial_primal_weight,
                  const PreprocessSolver* preprocess_solver);

  // Not copyable or movable (because there are const members).
  Solver(const Solver&) = delete;
  Solver& operator=(const Solver&) = delete;
  Solver(Solver&&) = delete;
  Solver& operator=(Solver&&) = delete;

  const QuadraticProgram& WorkingQp() const { return ShardedWorkingQp().Qp(); }

  const ShardedQuadraticProgram& ShardedWorkingQp() const {
    return preprocess_solver_->ShardedWorkingQp();
  }

  // Runs PDHG iterations on the instance that has been initialized in Solver.
  // If interrupt_solve is not nullptr, then the solver will periodically check
  // if interrupt_solve->load() is true, in which case the solve will terminate
  // with TERMINATION_REASON_INTERRUPTED_BY_USER. Ownership is not transferred.
  // solve_log should contain initial problem statistics.
  // On return, SolveResult.reduced_costs will be empty, and the solution will
  // be to the preprocessed/scaled problem.
  SolverResult Solve(const std::atomic<bool>* interrupt_solve,
                     SolveLog solve_log);

 private:
  struct NextSolutionAndDelta {
    VectorXd value;
    // delta is value - current_solution.
    VectorXd delta;
  };

  struct DistanceBasedRestartInfo {
    double distance_moved_last_restart_period;
    int length_of_last_restart_period;
  };

  // Movement terms (weighted squared norms of primal and dual deltas) larger
  // than this cause termination because iterates are diverging, and likely to
  // cause infinite and NaN values.
  constexpr static double kDivergentMovement = 1.0e100;

  NextSolutionAndDelta ComputeNextPrimalSolution(double primal_step_size) const;

  NextSolutionAndDelta ComputeNextDualSolution(
      double dual_step_size, double extrapolation_factor,
      const NextSolutionAndDelta& next_primal) const;

  double ComputeMovement(const VectorXd& delta_primal,
                         const VectorXd& delta_dual) const;

  double ComputeNonlinearity(const VectorXd& delta_primal,
                             const VectorXd& next_dual_product) const;

  // Creates all the simple-to-compute statistics in stats.
  IterationStats CreateSimpleIterationStats(RestartChoice restart_used) const;

  RestartChoice ChooseRestartToApply(bool is_major_iteration);

  VectorXd PrimalAverage() const;

  VectorXd DualAverage() const;

  double ComputeNewPrimalWeight() const;

  // Picks the primal and dual solutions according to output_type, and makes the
  // closing changes to the SolveLog. This function should only be called once
  // the solver is finishing its execution.
  // NOTE: The primal_solution and dual_solution are used as the output except
  // when output_type is POINT_TYPE_CURRENT_ITERATE or
  // POINT_TYPE_ITERATE_DIFFERENCE, in which case the values are computed from
  // Solver data.
  // NOTE: The primal_solution, dual_solution, and solve_log are passed by
  // value. To avoid unnecessary copying, move these objects (i.e. use
  // std::move()).
  SolverResult PickSolutionAndConstructSolverResult(
      VectorXd primal_solution, VectorXd dual_solution,
      const IterationStats& stats, TerminationReason termination_reason,
      PointType output_type, SolveLog solve_log) const;

  double DistanceTraveledFromLastStart(const VectorXd& primal_solution,
                                       const VectorXd& dual_solution) const;

  LocalizedLagrangianBounds ComputeLocalizedBoundsAtCurrent() const;

  LocalizedLagrangianBounds ComputeLocalizedBoundsAtAverage() const;

  // Applies the given RestartChoice. If a restart is chosen, updates the
  // state of the algorithm accordingly and computes a new primal weight.
  void ApplyRestartChoice(RestartChoice restart_to_apply);

  std::optional<SolverResult> MajorIterationAndTerminationCheck(
      bool force_numerical_termination,
      const std::atomic<bool>* interrupt_solve, SolveLog& solve_log);

  bool ShouldDoAdaptiveRestartHeuristic(double candidate_normalized_gap) const;

  RestartChoice DetermineDistanceBasedRestartChoice() const;

  void ResetAverageToCurrent();

  void LogNumericalTermination() const;

  void LogInnerIterationLimitHit() const;

  // Takes a step based on the Malitsky and Pock linesearch algorithm.
  // (https://arxiv.org/pdf/1608.08883.pdf)
  // The current implementation is provably convergent (at an optimal rate)
  // for LP programs (provided we do not change the primal weight at every major
  // iteration). Further, we have observed that this rule is very sensitive to
  // the parameter choice whenever we apply the primal weight recomputation
  // heuristic.
  InnerStepOutcome TakeMalitskyPockStep();

  // Takes a step based on the adaptive heuristic presented in Section 3.1 of
  // https://arxiv.org/pdf/2106.04756.pdf (further generalized to QP).
  InnerStepOutcome TakeAdaptiveStep();

  // Takes a constant-size step.
  InnerStepOutcome TakeConstantSizeStep();

  const PrimalDualHybridGradientParams params_;

  VectorXd current_primal_solution_;
  VectorXd current_dual_solution_;
  VectorXd current_primal_delta_;
  VectorXd current_dual_delta_;

  ShardedWeightedAverage primal_average_;
  ShardedWeightedAverage dual_average_;

  double step_size_;
  double primal_weight_;

  const PreprocessSolver* preprocess_solver_;

  // For Malitsky-Pock linesearch only: step_size_ / previous_step_size
  double ratio_last_two_step_sizes_;
  // For adaptive restarts only.
  double normalized_gap_at_last_trial_ =
      std::numeric_limits<double>::infinity();
  // For adaptive restarts only.
  double normalized_gap_at_last_restart_ =
      std::numeric_limits<double>::infinity();
  int iterations_completed_;
  int num_rejected_steps_;
  // A cache of constraint_matrix.transpose() * current_dual_solution.
  VectorXd current_dual_product_;
  // The primal point at which the algorithm was last restarted from, or
  // the initial primal starting point if no restart has occurred.
  VectorXd last_primal_start_point_;
  // The dual point at which the algorithm was last restarted from, or
  // the initial dual starting point if no restart has occurred.
  VectorXd last_dual_start_point_;
  // Information for deciding whether to trigger a distance-based restart.
  // The distances are initialized to +inf to force a restart during the first
  // major iteration check.
  DistanceBasedRestartInfo distance_based_restart_info_ = {
      .distance_moved_last_restart_period =
          std::numeric_limits<double>::infinity(),
      .length_of_last_restart_period = 1,
  };
};

PreprocessSolver::PreprocessSolver(QuadraticProgram qp,
                                   const PrimalDualHybridGradientParams& params)
    : num_threads_(NumThreads(params.num_threads(), params.num_shards(), qp)),
      num_shards_(NumShards(num_threads_, params.num_shards())),
      sharded_qp_(std::move(qp), num_threads_, num_shards_) {}

SolverResult ErrorSolverResult(const TerminationReason reason,
                               const std::string& message) {
  SolveLog error_log;
  error_log.set_termination_reason(reason);
  error_log.set_termination_string(message);
  LOG(WARNING) << "The solver did not run because of invalid input: "
               << message;
  return SolverResult{.solve_log = error_log};
}

std::optional<SolverResult> CheckProblemStats(
    const QuadraticProgramStats& problem_stats) {
  const double kExcessiveInputValue = 1e50;
  const double kExcessivelySmallInputValue = 1e-50;
  const double kMaxDynamicRange = 1e20;
  if (std::isnan(problem_stats.constraint_matrix_l2_norm())) {
    return ErrorSolverResult(TERMINATION_REASON_INVALID_PROBLEM,
                             "Constraint matrix has a NAN.");
  }
  if (problem_stats.constraint_matrix_abs_max() > kExcessiveInputValue) {
    return ErrorSolverResult(
        TERMINATION_REASON_INVALID_PROBLEM,
        absl::StrCat("Constraint matrix has a non-zero with absolute value ",
                     problem_stats.constraint_matrix_abs_max(),
                     " which exceeds limit of ", kExcessiveInputValue, "."));
  }
  if (problem_stats.constraint_matrix_abs_max() >
      kMaxDynamicRange * problem_stats.constraint_matrix_abs_min()) {
    LOG(WARNING) << "Constraint matrix has largest absolute value "
                 << problem_stats.constraint_matrix_abs_max()
                 << " and smallest non-zero absolute value "
                 << problem_stats.constraint_matrix_abs_min()
                 << " performance may suffer.";
  }
  if (problem_stats.constraint_matrix_col_min_l_inf_norm() > 0 &&
      problem_stats.constraint_matrix_col_min_l_inf_norm() <
          kExcessivelySmallInputValue) {
    return ErrorSolverResult(
        TERMINATION_REASON_INVALID_PROBLEM,
        absl::StrCat("Constraint matrix has a column with Linf norm ",
                     problem_stats.constraint_matrix_col_min_l_inf_norm(),
                     " which is less than limit of ",
                     kExcessivelySmallInputValue, "."));
  }
  if (problem_stats.constraint_matrix_row_min_l_inf_norm() > 0 &&
      problem_stats.constraint_matrix_row_min_l_inf_norm() <
          kExcessivelySmallInputValue) {
    return ErrorSolverResult(
        TERMINATION_REASON_INVALID_PROBLEM,
        absl::StrCat("Constraint matrix has a row with Linf norm ",
                     problem_stats.constraint_matrix_row_min_l_inf_norm(),
                     " which is less than limit of ",
                     kExcessivelySmallInputValue, "."));
  }
  if (std::isnan(problem_stats.combined_bounds_l2_norm())) {
    return ErrorSolverResult(TERMINATION_REASON_INVALID_PROBLEM,
                             "Constraint bounds vector has a NAN.");
  }
  if (problem_stats.combined_bounds_max() > kExcessiveInputValue) {
    return ErrorSolverResult(
        TERMINATION_REASON_INVALID_PROBLEM,
        absl::StrCat("Combined constraint bounds vector has a non-zero with "
                     "absolute value ",
                     problem_stats.combined_bounds_max(),
                     " which exceeds limit of ", kExcessiveInputValue, "."));
  }
  if (problem_stats.combined_bounds_max() >
      kMaxDynamicRange * problem_stats.combined_bounds_min()) {
    LOG(WARNING)
        << "Combined constraint bounds vector has largest absolute value "
        << problem_stats.combined_bounds_max()
        << " and smallest non-zero absolute value "
        << problem_stats.combined_bounds_min() << "; performance may suffer.";
  }
  if (std::isnan(problem_stats.variable_bound_gaps_l2_norm())) {
    return ErrorSolverResult(TERMINATION_REASON_INVALID_PROBLEM,
                             "Variable bounds vector has a NAN.");
  }
  if (problem_stats.variable_bound_gaps_max() > kExcessiveInputValue) {
    return ErrorSolverResult(
        TERMINATION_REASON_INVALID_PROBLEM,
        absl::StrCat("Variable bound gaps vector has a finite non-zero with "
                     "absolute value ",
                     problem_stats.variable_bound_gaps_max(),
                     " which exceeds limit of ", kExcessiveInputValue, "."));
  }
  if (problem_stats.variable_bound_gaps_max() >
      kMaxDynamicRange * problem_stats.variable_bound_gaps_min()) {
    LOG(WARNING) << "Variable bound gap vector has largest absolute value "
                 << problem_stats.variable_bound_gaps_max()
                 << " and smallest non-zero absolute value "
                 << problem_stats.variable_bound_gaps_min()
                 << "; performance may suffer.";
  }
  if (std::isnan(problem_stats.objective_vector_l2_norm())) {
    return ErrorSolverResult(TERMINATION_REASON_INVALID_PROBLEM,
                             "Objective vector has a NAN.");
  }
  if (problem_stats.objective_vector_abs_max() > kExcessiveInputValue) {
    return ErrorSolverResult(
        TERMINATION_REASON_INVALID_PROBLEM,
        absl::StrCat("Objective vector has a non-zero with absolute value ",
                     problem_stats.objective_vector_abs_max(),
                     " which exceeds limit of ", kExcessiveInputValue, "."));
  }
  if (problem_stats.objective_vector_abs_max() >
      kMaxDynamicRange * problem_stats.objective_vector_abs_min()) {
    LOG(WARNING) << "Objective vector has largest absolute value "
                 << problem_stats.objective_vector_abs_max()
                 << " and smallest non-zero absolute value "
                 << problem_stats.objective_vector_abs_min()
                 << "; performance may suffer.";
  }
  if (std::isnan(problem_stats.objective_matrix_l2_norm())) {
    return ErrorSolverResult(TERMINATION_REASON_INVALID_PROBLEM,
                             "Objective matrix has a NAN.");
  }
  if (problem_stats.objective_matrix_abs_max() > kExcessiveInputValue) {
    return ErrorSolverResult(
        TERMINATION_REASON_INVALID_PROBLEM,
        absl::StrCat("Objective matrix has a non-zero with absolute value ",
                     problem_stats.objective_matrix_abs_max(),
                     " which exceeds limit of ", kExcessiveInputValue, "."));
  }
  if (problem_stats.objective_matrix_abs_max() >
      kMaxDynamicRange * problem_stats.objective_matrix_abs_min()) {
    LOG(WARNING) << "Objective matrix has largest absolute value "
                 << problem_stats.objective_matrix_abs_max()
                 << " and smallest non-zero absolute value "
                 << problem_stats.objective_matrix_abs_min()
                 << "; performance may suffer.";
  }
  return std::nullopt;
}

std::optional<SolverResult> CheckInitialSolution(
    const ShardedQuadraticProgram& sharded_qp,
    const PrimalAndDualSolution& initial_solution) {
  const double kExcessiveInputValue = 1e50;
  if (initial_solution.primal_solution.size() != sharded_qp.PrimalSize()) {
    return ErrorSolverResult(
        TERMINATION_REASON_INVALID_INITIAL_SOLUTION,
        absl::StrCat("Initial primal solution has size ",
                     initial_solution.primal_solution.size(),
                     " which differs from problem primal size ",
                     sharded_qp.PrimalSize()));
  }
  if (std::isnan(
          Norm(initial_solution.primal_solution, sharded_qp.PrimalSharder()))) {
    return ErrorSolverResult(TERMINATION_REASON_INVALID_INITIAL_SOLUTION,
                             "Initial primal solution has a NAN.");
  }
  if (const double norm = LInfNorm(initial_solution.primal_solution,
                                   sharded_qp.PrimalSharder());
      norm > kExcessiveInputValue) {
    return ErrorSolverResult(
        TERMINATION_REASON_INVALID_INITIAL_SOLUTION,
        absl::StrCat(
            "Initial primal solution has an entry with absolute value ", norm,
            " which exceeds limit of ", kExcessiveInputValue));
  }
  if (initial_solution.dual_solution.size() != sharded_qp.DualSize()) {
    return ErrorSolverResult(
        TERMINATION_REASON_INVALID_INITIAL_SOLUTION,
        absl::StrCat("Initial dual solution has size ",
                     initial_solution.dual_solution.size(),
                     " which differs from problem dual size ",
                     sharded_qp.DualSize()));
  }
  if (std::isnan(
          Norm(initial_solution.dual_solution, sharded_qp.DualSharder()))) {
    return ErrorSolverResult(TERMINATION_REASON_INVALID_INITIAL_SOLUTION,
                             "Initial dual solution has a NAN.");
  }
  if (const double norm =
          LInfNorm(initial_solution.dual_solution, sharded_qp.DualSharder());
      norm > kExcessiveInputValue) {
    return ErrorSolverResult(
        TERMINATION_REASON_INVALID_INITIAL_SOLUTION,
        absl::StrCat("Initial dual solution has an entry with absolute value ",
                     norm, " which exceeds limit of ", kExcessiveInputValue));
  }
  return std::nullopt;
}

SolverResult PreprocessSolver::PreprocessAndSolve(
    const PrimalDualHybridGradientParams& params,
    std::optional<PrimalAndDualSolution> initial_solution,
    const std::atomic<bool>* interrupt_solve,
    IterationStatsCallback iteration_stats_callback) {
  SolveLog solve_log;
  if (Qp().problem_name.has_value()) {
    solve_log.set_instance_name(*Qp().problem_name);
  }
  *solve_log.mutable_params() = params;
  *solve_log.mutable_original_problem_stats() =
      ComputeStats(sharded_qp_, params.infinite_constraint_bound_threshold());
  const QuadraticProgramStats& original_problem_stats =
      solve_log.original_problem_stats();
  if (auto maybe_result = CheckProblemStats(original_problem_stats);
      maybe_result.has_value()) {
    return *maybe_result;
  }
  if (initial_solution.has_value()) {
    if (auto maybe_result =
            CheckInitialSolution(sharded_qp_, *initial_solution);
        maybe_result.has_value()) {
      return *maybe_result;
    }
  }
  original_bound_norms_ = BoundNormsFromProblemStats(original_problem_stats);
  const std::string preprocessing_string = absl::StrCat(
      params.presolve_options().use_glop() ? "presolving and " : "",
      "rescaling:");
  if (params.verbosity_level() >= 1) {
    LOG(INFO) << "Problem stats before " << preprocessing_string;
    LogQuadraticProgramStats(solve_log.original_problem_stats());
  }
  timer_.Start();
  iteration_stats_callback_ = std::move(iteration_stats_callback);
  std::optional<TerminationReason> maybe_terminate =
      ApplyPresolveIfEnabled(params, &initial_solution);
  if (maybe_terminate.has_value()) {
    // Glop also feeds zero primal and dual solutions when the preprocessor
    // has a non-INIT status. When the preprocessor status is optimal the
    // vectors have length 0. When the status is something else the lengths
    // may be non-zero, but that's OK since we don't promise to produce a
    // meaningful solution in that case.
    IterationStats iteration_stats;
    iteration_stats.set_cumulative_time_sec(GetElapsedTime());
    solve_log.set_preprocessing_time_sec(iteration_stats.cumulative_time_sec());
    VectorXd working_primal = ZeroVector(sharded_qp_.PrimalSharder());
    VectorXd working_dual = ZeroVector(sharded_qp_.DualSharder());
    PrimalAndDualSolution original = RecoverOriginalSolution(
        {.primal_solution = working_primal, .dual_solution = working_dual});
    AddConvergenceAndInfeasibilityInformation(
        params, original.primal_solution, original.dual_solution,
        presolve_info_->sharded_original_qp,
        presolve_info_->trivial_col_scaling_vec,
        presolve_info_->trivial_row_scaling_vec, POINT_TYPE_PRESOLVER_SOLUTION,
        iteration_stats);
    std::optional<TerminationReasonAndPointType> earned_termination =
        CheckIterateTerminationCriteria(params.termination_criteria(),
                                        iteration_stats, original_bound_norms_,
                                        /*force_numerical_termination=*/false);
    if (!earned_termination.has_value()) {
      earned_termination = CheckSimpleTerminationCriteria(
          params.termination_criteria(), iteration_stats, interrupt_solve);
    }
    TerminationReason final_termination_reason;
    if (earned_termination.has_value() &&
        (earned_termination->reason == TERMINATION_REASON_OPTIMAL ||
         earned_termination->reason == TERMINATION_REASON_PRIMAL_INFEASIBLE ||
         earned_termination->reason == TERMINATION_REASON_DUAL_INFEASIBLE)) {
      final_termination_reason = earned_termination->reason;
    } else {
      if (*maybe_terminate == TERMINATION_REASON_OPTIMAL) {
        final_termination_reason = TERMINATION_REASON_NUMERICAL_ERROR;
        LOG(WARNING) << "Presolve claimed to solve the LP optimally but the "
                        "solution doesn't satisfy the optimality criteria.";
      } else {
        final_termination_reason = *maybe_terminate;
      }
    }
    return ConstructOriginalSolverResult(
        params, ConstructSolverResult(
                    std::move(working_primal), std::move(working_dual),
                    std::move(iteration_stats), final_termination_reason,
                    POINT_TYPE_PRESOLVER_SOLUTION, std::move(solve_log)));
  }

  VectorXd starting_primal_solution;
  VectorXd starting_dual_solution;
  // The current solution is updated by ComputeAndApplyRescaling.
  if (initial_solution.has_value()) {
    starting_primal_solution = std::move(initial_solution->primal_solution);
    starting_dual_solution = std::move(initial_solution->dual_solution);
  } else {
    SetZero(sharded_qp_.PrimalSharder(), starting_primal_solution);
    SetZero(sharded_qp_.DualSharder(), starting_dual_solution);
  }
  // The following projections are necessary since all our checks assume that
  // the primal and dual variable bounds are satisfied.
  ProjectToPrimalVariableBounds(sharded_qp_, starting_primal_solution);
  ProjectToDualVariableBounds(sharded_qp_, starting_dual_solution);

  ComputeAndApplyRescaling(params, starting_primal_solution,
                           starting_dual_solution);
  *solve_log.mutable_preprocessed_problem_stats() =
      ComputeStats(sharded_qp_, params.infinite_constraint_bound_threshold());
  if (params.verbosity_level() >= 1) {
    LOG(INFO) << "Problem stats after " << preprocessing_string;
    LogQuadraticProgramStats(solve_log.preprocessed_problem_stats());
  }

  double step_size = 0.0;
  if (params.linesearch_rule() ==
      PrimalDualHybridGradientParams::CONSTANT_STEP_SIZE_RULE) {
    std::mt19937 random(1);
    double inverse_step_size;
    const auto lipschitz_result =
        EstimateMaximumSingularValueOfConstraintMatrix(
            sharded_qp_, std::nullopt, std::nullopt,
            /*desired_relative_error=*/0.2, /*failure_probability=*/0.0005,
            random);
    // With high probability, the estimate of the lipschitz term is within
    // +/- estimated_relative_error * lipschitz_term.
    const double lipschitz_term_upper_bound =
        lipschitz_result.singular_value /
        (1.0 - lipschitz_result.estimated_relative_error);
    inverse_step_size = lipschitz_term_upper_bound;
    step_size = inverse_step_size > 0.0 ? 1.0 / inverse_step_size : 1.0;
  } else {
    // This initial step size is designed to err on the side of being too big.
    // This is because
    //  (i) too-big steps are rejected and hence don't hurt us other than
    //  wasting
    //      an iteration and
    // (ii) the step size adjustment algorithm shrinks the step size as far as
    //      needed in a single iteration but raises it slowly.
    // The tiny constant is there to keep the step size finite in the case of a
    // trivial LP with no constraints.
    step_size =
        1.0 /
        std::max(
            1.0e-20,
            solve_log.preprocessed_problem_stats().constraint_matrix_abs_max());
  }
  step_size *= params.initial_step_size_scaling();

  const double primal_weight = InitialPrimalWeight(
      params, solve_log.preprocessed_problem_stats().objective_vector_l2_norm(),
      solve_log.preprocessed_problem_stats().combined_bounds_l2_norm());
  solve_log.set_preprocessing_time_sec(GetElapsedTime());

  Solver solver(params, starting_primal_solution, starting_dual_solution,
                step_size, primal_weight, this);
  SolverResult result = solver.Solve(interrupt_solve, std::move(solve_log));
  return ConstructOriginalSolverResult(params, std::move(result));
}

void LogInfoWithoutPrefix(absl::string_view message) {
  LOG(INFO).NoPrefix() << message;
}

glop::GlopParameters PreprocessSolver::PreprocessorParameters(
    const PrimalDualHybridGradientParams& params) {
  glop::GlopParameters glop_params;
  // TODO(user): Test if dualization helps or hurts performance.
  glop_params.set_solve_dual_problem(glop::GlopParameters::NEVER_DO);
  // Experiments show that this preprocessing step can hurt because it relaxes
  // variable bounds.
  glop_params.set_use_implied_free_preprocessor(false);
  // We do our own scaling.
  glop_params.set_use_scaling(false);
  if (params.presolve_options().has_glop_parameters()) {
    glop_params.MergeFrom(params.presolve_options().glop_parameters());
  }
  return glop_params;
}

TerminationReason GlopStatusToTerminationReason(
    const glop::ProblemStatus glop_status) {
  switch (glop_status) {
    case glop::ProblemStatus::OPTIMAL:
      return TERMINATION_REASON_OPTIMAL;
    case glop::ProblemStatus::INVALID_PROBLEM:
      return TERMINATION_REASON_INVALID_PROBLEM;
    case glop::ProblemStatus::ABNORMAL:
    case glop::ProblemStatus::IMPRECISE:
      return TERMINATION_REASON_NUMERICAL_ERROR;
    case glop::ProblemStatus::PRIMAL_INFEASIBLE:
    case glop::ProblemStatus::DUAL_INFEASIBLE:
    case glop::ProblemStatus::INFEASIBLE_OR_UNBOUNDED:
    case glop::ProblemStatus::DUAL_UNBOUNDED:
    case glop::ProblemStatus::PRIMAL_UNBOUNDED:
      return TERMINATION_REASON_PRIMAL_OR_DUAL_INFEASIBLE;
    default:
      LOG(WARNING) << "Unexpected preprocessor status " << glop_status;
      return TERMINATION_REASON_OTHER;
  }
}

std::optional<TerminationReason> PreprocessSolver::ApplyPresolveIfEnabled(
    const PrimalDualHybridGradientParams& params,
    std::optional<PrimalAndDualSolution>* const initial_solution) {
  const bool presolve_enabled = params.presolve_options().use_glop();
  if (!presolve_enabled) {
    return std::nullopt;
  }
  if (!IsLinearProgram(Qp())) {
    LOG(WARNING)
        << "Skipping presolve, which is only supported for linear programs";
    return std::nullopt;
  }
  absl::StatusOr<MPModelProto> model = QpToMpModelProto(Qp());
  if (!model.ok()) {
    LOG(WARNING)
        << "Skipping presolve because of error converting to MPModelProto: "
        << model.status();
    return std::nullopt;
  }
  if (initial_solution->has_value()) {
    LOG(WARNING) << "Ignoring initial solution. Initial solutions "
                    "are ignored when presolve is on.";
    initial_solution->reset();
  }
  glop::LinearProgram glop_lp;
  glop::MPModelProtoToLinearProgram(*model, &glop_lp);
  // Save RAM
  model->Clear();
  presolve_info_.emplace(std::move(sharded_qp_), params);
  // To simplify our code we ignore the return value indicating whether
  // postprocessing is required. We simply call RecoverSolution()
  // unconditionally, which may do nothing.
  presolve_info_->preprocessor.Run(&glop_lp);
  presolve_info_->presolved_problem_was_maximization =
      glop_lp.IsMaximizationProblem();
  MPModelProto output;
  glop::LinearProgramToMPModelProto(glop_lp, &output);
  // This will only fail if given an invalid LP, which shouldn't happen.
  absl::StatusOr<QuadraticProgram> presolved_qp =
      QpFromMpModelProto(output, /*relax_integer_variables=*/false);
  CHECK_OK(presolved_qp.status());
  // MPModelProto doesn't support scaling factors, so if glop_lp has an
  // objective_scaling_factor it won't set in output and presolved_qp. The
  // scaling factor of presolved_qp isn't actually used anywhere, but we set it
  // for completeness.
  presolved_qp->objective_scaling_factor = glop_lp.objective_scaling_factor();
  sharded_qp_ = ShardedQuadraticProgram(std::move(*presolved_qp), num_threads_,
                                        num_shards_);
  // A status of INIT means the preprocessor created a (usually) smaller
  // problem that needs solving. Other statuses mean the preprocessor solved
  // the problem completely.
  if (presolve_info_->preprocessor.status() != glop::ProblemStatus::INIT) {
    col_scaling_vec_ = OnesVector(sharded_qp_.PrimalSharder());
    row_scaling_vec_ = OnesVector(sharded_qp_.DualSharder());
    return GlopStatusToTerminationReason(presolve_info_->preprocessor.status());
  }
  return std::nullopt;
}

void PreprocessSolver::ComputeAndApplyRescaling(
    const PrimalDualHybridGradientParams& params,
    VectorXd& starting_primal_solution, VectorXd& starting_dual_solution) {
  ScalingVectors scaling = ApplyRescaling(
      RescalingOptions{.l_inf_ruiz_iterations = params.l_inf_ruiz_iterations(),
                       .l2_norm_rescaling = params.l2_norm_rescaling()},
      sharded_qp_);
  row_scaling_vec_ = std::move(scaling.row_scaling_vec);
  col_scaling_vec_ = std::move(scaling.col_scaling_vec);

  CoefficientWiseQuotientInPlace(col_scaling_vec_, sharded_qp_.PrimalSharder(),
                                 starting_primal_solution);
  CoefficientWiseQuotientInPlace(row_scaling_vec_, sharded_qp_.DualSharder(),
                                 starting_dual_solution);
}

void PreprocessSolver::LogQuadraticProgramStats(
    const QuadraticProgramStats& stats) {
  LOG(INFO) << absl::StrFormat(
                   "There are %i variables, %i constraints, and %i ",
                   stats.num_variables(), stats.num_constraints(),
                   stats.constraint_matrix_num_nonzeros())
            << "constraint matrix nonzeros.";
  if (Qp().constraint_matrix.nonZeros() > 0) {
    LOG(INFO) << "Absolute values of nonzero constraint matrix elements: "
              << absl::StrFormat("largest=%f, smallest=%f, avg=%f",
                                 stats.constraint_matrix_abs_max(),
                                 stats.constraint_matrix_abs_min(),
                                 stats.constraint_matrix_abs_avg());
    LOG(INFO) << "Constraint matrix, infinity norm: "
              << absl::StrFormat("max(row & col)=%f, min_col=%f, min_row=%f",
                                 stats.constraint_matrix_abs_max(),
                                 stats.constraint_matrix_col_min_l_inf_norm(),
                                 stats.constraint_matrix_row_min_l_inf_norm());
    LOG(INFO) << "Constraint bounds statistics (max absolute value per row): "
              << absl::StrFormat("largest=%f, smallest=%f, avg=%f, l2_norm=%f",
                                 stats.combined_bounds_max(),
                                 stats.combined_bounds_min(),
                                 stats.combined_bounds_avg(),
                                 stats.combined_bounds_l2_norm());
  }
  if (!IsLinearProgram(Qp())) {
    LOG(INFO) << absl::StrFormat(
        "There are %i nonzero diagonal coefficients in the objective matrix.",
        stats.objective_matrix_num_nonzeros());
    LOG(INFO) << "Absolute values of nonzero objective matrix elements: "
              << absl::StrFormat("largest=%f, smallest=%f, avg=%f",
                                 stats.objective_matrix_abs_max(),
                                 stats.objective_matrix_abs_min(),
                                 stats.objective_matrix_abs_avg());
  }
  LOG(INFO) << "Absolute values of objective vector elements: "
            << absl::StrFormat("largest=%f, smallest=%f, avg=%f, l2_norm=%f",
                               stats.objective_vector_abs_max(),
                               stats.objective_vector_abs_min(),
                               stats.objective_vector_abs_avg(),
                               stats.objective_vector_l2_norm());

  LOG(INFO) << "Gaps between variable upper and lower bounds: "
            << absl::StrFormat(
                   "#finite=%i of %i, largest=%f, smallest=%f, avg=%f",
                   stats.variable_bound_gaps_num_finite(),
                   stats.num_variables(), stats.variable_bound_gaps_max(),
                   stats.variable_bound_gaps_min(),
                   stats.variable_bound_gaps_avg());
}

double PreprocessSolver::InitialPrimalWeight(
    const PrimalDualHybridGradientParams& params,
    const double l2_norm_primal_linear_objective,
    const double l2_norm_constraint_bounds) const {
  if (params.has_initial_primal_weight()) {
    return params.initial_primal_weight();
  }
  if (l2_norm_primal_linear_objective > 0.0 &&
      l2_norm_constraint_bounds > 0.0) {
    // The hand-wavy motivation for this choice is that the objective vector
    // has units of (objective units)/(primal units) and the constraint
    // bounds vector has units of (objective units)/(dual units),
    // therefore this ratio has units (dual units)/(primal units). By
    // dimensional analysis, these are the same units as the primal weight.
    return l2_norm_primal_linear_objective / l2_norm_constraint_bounds;
  } else {
    return 1.0;
  }
}

PrimalAndDualSolution PreprocessSolver::RecoverOriginalSolution(
    PrimalAndDualSolution working_solution) const {
  glop::ProblemSolution glop_solution(glop::RowIndex{0}, glop::ColIndex{0});
  if (presolve_info_.has_value()) {
    // We compute statuses relative to the working problem so we can detect when
    // variables are at their bounds without floating-point roundoff induced by
    // scaling.
    glop_solution = internal::ComputeStatuses(Qp(), working_solution);
  }
  CoefficientWiseProductInPlace(col_scaling_vec_, sharded_qp_.PrimalSharder(),
                                working_solution.primal_solution);
  CoefficientWiseProductInPlace(row_scaling_vec_, sharded_qp_.DualSharder(),
                                working_solution.dual_solution);
  if (presolve_info_.has_value()) {
    glop_solution.primal_values =
        glop::DenseRow(working_solution.primal_solution.begin(),
                       working_solution.primal_solution.end());
    glop_solution.dual_values =
        glop::DenseColumn(working_solution.dual_solution.begin(),
                          working_solution.dual_solution.end());
    // We got the working QP by calling LinearProgramToMPModelProto() and
    // QpFromMpModelProto(). We need to negate the duals if the LP resulting
    // from presolve was a max problem.
    if (presolve_info_->presolved_problem_was_maximization) {
      for (glop::RowIndex i{0}; i < glop_solution.dual_values.size(); ++i) {
        glop_solution.dual_values[i] *= -1;
      }
    }
    presolve_info_->preprocessor.RecoverSolution(&glop_solution);
    PrimalAndDualSolution solution;
    solution.primal_solution =
        Eigen::Map<Eigen::VectorXd>(glop_solution.primal_values.data(),
                                    glop_solution.primal_values.size().value());
    solution.dual_solution =
        Eigen::Map<Eigen::VectorXd>(glop_solution.dual_values.data(),
                                    glop_solution.dual_values.size().value());
    // We called QpToMpModelProto() and MPModelProtoToLinearProgram() to convert
    // our original QP into input for glop's preprocessor. The former multiplies
    // the objective vector by the objective_scaling_factor, which multiplies
    // the duals by that factor as well. To undo this we divide by the
    // objective_scaling_factor.
    solution.dual_solution /=
        presolve_info_->sharded_original_qp.Qp().objective_scaling_factor;
    // Glop's preprocessor sometimes violates the primal bounds constraints. To
    // be safe we project both primal and dual.
    ProjectToPrimalVariableBounds(presolve_info_->sharded_original_qp,
                                  solution.primal_solution);
    ProjectToDualVariableBounds(presolve_info_->sharded_original_qp,
                                solution.dual_solution);
    return solution;
  } else {
    return working_solution;
  }
}

void PreprocessSolver::AddConvergenceAndInfeasibilityInformation(
    const PrimalDualHybridGradientParams& params,
    const VectorXd& primal_solution, const VectorXd& dual_solution,
    const ShardedQuadraticProgram& sharded_qp, const VectorXd& col_scaling_vec,
    const VectorXd& row_scaling_vec, PointType candidate_type,
    IterationStats& stats) const {
  const TerminationCriteria::DetailedOptimalityCriteria criteria =
      EffectiveOptimalityCriteria(params.termination_criteria());
  *stats.add_convergence_information() = ComputeConvergenceInformation(
      params, sharded_qp, col_scaling_vec, row_scaling_vec, primal_solution,
      dual_solution,
      EpsilonRatio(criteria.eps_optimal_primal_residual_absolute(),
                   criteria.eps_optimal_primal_residual_relative()),
      EpsilonRatio(criteria.eps_optimal_dual_residual_absolute(),
                   criteria.eps_optimal_dual_residual_relative()),
      candidate_type);
  *stats.add_infeasibility_information() = ComputeInfeasibilityInformation(
      params, sharded_qp, col_scaling_vec, row_scaling_vec, primal_solution,
      dual_solution, candidate_type);
}

void SetActiveSetInformation(const ShardedQuadraticProgram& sharded_qp,
                             const VectorXd& primal_solution,
                             const VectorXd& dual_solution,
                             const VectorXd& primal_start_point,
                             const VectorXd& dual_start_point,
                             PointMetadata& metadata) {
  CHECK_EQ(primal_solution.size(), sharded_qp.PrimalSize());
  CHECK_EQ(dual_solution.size(), sharded_qp.DualSize());
  CHECK_EQ(primal_start_point.size(), sharded_qp.PrimalSize());
  CHECK_EQ(dual_start_point.size(), sharded_qp.DualSize());

  const QuadraticProgram& qp = sharded_qp.Qp();
  metadata.set_active_primal_variable_count(
      static_cast<int64_t>(sharded_qp.PrimalSharder().ParallelSumOverShards(
          [&](const Sharder::Shard& shard) {
            const auto primal_shard = shard(primal_solution);
            const auto lower_bound_shard = shard(qp.variable_lower_bounds);
            const auto upper_bound_shard = shard(qp.variable_upper_bounds);
            return (primal_shard.array() > lower_bound_shard.array() &&
                    primal_shard.array() < upper_bound_shard.array())
                .count();
          })));

  // Most of the computation from the previous ParallelSumOverShards is
  // duplicated here. However the overhead shouldn't be too large, and using
  // ParallelSumOverShards is simpler than just using ParallelForEachShard.
  metadata.set_active_primal_variable_change(
      static_cast<int64_t>(sharded_qp.PrimalSharder().ParallelSumOverShards(
          [&](const Sharder::Shard& shard) {
            const auto primal_shard = shard(primal_solution);
            const auto primal_start_shard = shard(primal_start_point);
            const auto lower_bound_shard = shard(qp.variable_lower_bounds);
            const auto upper_bound_shard = shard(qp.variable_upper_bounds);
            return ((primal_shard.array() > lower_bound_shard.array() &&
                     primal_shard.array() < upper_bound_shard.array()) !=
                    (primal_start_shard.array() > lower_bound_shard.array() &&
                     primal_start_shard.array() < upper_bound_shard.array()))
                .count();
          })));

  metadata.set_active_dual_variable_count(
      static_cast<int64_t>(sharded_qp.DualSharder().ParallelSumOverShards(
          [&](const Sharder::Shard& shard) {
            const auto dual_shard = shard(dual_solution);
            const auto lower_bound_shard = shard(qp.constraint_lower_bounds);
            const auto upper_bound_shard = shard(qp.constraint_upper_bounds);
            const double kInfinity = std::numeric_limits<double>::infinity();
            return (dual_shard.array() != 0.0 ||
                    (lower_bound_shard.array() == -kInfinity &&
                     upper_bound_shard.array() == kInfinity))
                .count();
          })));

  metadata.set_active_dual_variable_change(
      static_cast<int64_t>(sharded_qp.DualSharder().ParallelSumOverShards(
          [&](const Sharder::Shard& shard) {
            const auto dual_shard = shard(dual_solution);
            const auto dual_start_shard = shard(dual_start_point);
            const auto lower_bound_shard = shard(qp.constraint_lower_bounds);
            const auto upper_bound_shard = shard(qp.constraint_upper_bounds);
            const double kInfinity = std::numeric_limits<double>::infinity();
            return ((dual_shard.array() != 0.0 ||
                     (lower_bound_shard.array() == -kInfinity &&
                      upper_bound_shard.array() == kInfinity)) !=
                    (dual_start_shard.array() != 0.0 ||
                     (lower_bound_shard.array() == -kInfinity &&
                      upper_bound_shard.array() == kInfinity)))
                .count();
          })));
}

void PreprocessSolver::AddPointMetadata(
    const PrimalDualHybridGradientParams& params,
    const VectorXd& primal_solution, const VectorXd& dual_solution,
    PointType point_type, const VectorXd& last_primal_start_point,
    const VectorXd& last_dual_start_point, IterationStats& stats) const {
  PointMetadata metadata;
  metadata.set_point_type(point_type);
  std::vector<int> random_projection_seeds(
      params.random_projection_seeds().begin(),
      params.random_projection_seeds().end());
  SetRandomProjections(sharded_qp_, primal_solution, dual_solution,
                       random_projection_seeds, metadata);
  if (point_type != POINT_TYPE_ITERATE_DIFFERENCE) {
    SetActiveSetInformation(sharded_qp_, primal_solution, dual_solution,
                            last_primal_start_point, last_dual_start_point,
                            metadata);
  }
  *stats.add_point_metadata() = metadata;
}

std::optional<TerminationReasonAndPointType>
PreprocessSolver::UpdateIterationStatsAndCheckTermination(
    const PrimalDualHybridGradientParams& params,
    bool force_numerical_termination, const VectorXd& working_primal_current,
    const VectorXd& working_dual_current,
    const VectorXd* working_primal_average,
    const VectorXd* working_dual_average, const VectorXd* working_primal_delta,
    const VectorXd* working_dual_delta, const VectorXd& last_primal_start_point,
    const VectorXd& last_dual_start_point,
    const std::atomic<bool>* interrupt_solve, IterationStats& stats) const {
  if (presolve_info_.has_value()) {
    {  // This block exists to destroy `original_current` to save RAM.
      PrimalAndDualSolution original_current =
          RecoverOriginalSolution({.primal_solution = working_primal_current,
                                   .dual_solution = working_dual_current});
      AddConvergenceAndInfeasibilityInformation(
          params, original_current.primal_solution,
          original_current.dual_solution, presolve_info_->sharded_original_qp,
          presolve_info_->trivial_col_scaling_vec,
          presolve_info_->trivial_row_scaling_vec, POINT_TYPE_CURRENT_ITERATE,
          stats);
    }
    if (working_primal_average != nullptr && working_dual_average != nullptr) {
      PrimalAndDualSolution original_average =
          RecoverOriginalSolution({.primal_solution = *working_primal_average,
                                   .dual_solution = *working_dual_average});
      AddConvergenceAndInfeasibilityInformation(
          params, original_average.primal_solution,
          original_average.dual_solution, presolve_info_->sharded_original_qp,
          presolve_info_->trivial_col_scaling_vec,
          presolve_info_->trivial_row_scaling_vec, POINT_TYPE_AVERAGE_ITERATE,
          stats);
    }
  } else {
    AddConvergenceAndInfeasibilityInformation(
        params, working_primal_current, working_dual_current, sharded_qp_,
        col_scaling_vec_, row_scaling_vec_, POINT_TYPE_CURRENT_ITERATE, stats);
    if (working_primal_average != nullptr && working_dual_average != nullptr) {
      AddConvergenceAndInfeasibilityInformation(
          params, *working_primal_average, *working_dual_average, sharded_qp_,
          col_scaling_vec_, row_scaling_vec_, POINT_TYPE_AVERAGE_ITERATE,
          stats);
    }
  }
  AddPointMetadata(params, working_primal_current, working_dual_current,
                   POINT_TYPE_CURRENT_ITERATE, last_primal_start_point,
                   last_dual_start_point, stats);
  if (working_primal_average != nullptr && working_dual_average != nullptr) {
    AddPointMetadata(params, *working_primal_average, *working_dual_average,
                     POINT_TYPE_AVERAGE_ITERATE, last_primal_start_point,
                     last_dual_start_point, stats);
  }
  if (working_primal_delta != nullptr && working_dual_delta != nullptr) {
    if (presolve_info_.has_value()) {
      PrimalAndDualSolution original_delta =
          RecoverOriginalSolution({.primal_solution = *working_primal_delta,
                                   .dual_solution = *working_dual_delta});
      *stats.add_infeasibility_information() = ComputeInfeasibilityInformation(
          params, presolve_info_->sharded_original_qp,
          presolve_info_->trivial_col_scaling_vec,
          presolve_info_->trivial_row_scaling_vec,
          original_delta.primal_solution, original_delta.dual_solution,
          POINT_TYPE_ITERATE_DIFFERENCE);
    } else {
      *stats.add_infeasibility_information() = ComputeInfeasibilityInformation(
          params, sharded_qp_, col_scaling_vec_, row_scaling_vec_,
          *working_primal_delta, *working_dual_delta,
          POINT_TYPE_ITERATE_DIFFERENCE);
    }
    AddPointMetadata(params, *working_primal_delta, *working_dual_delta,
                     POINT_TYPE_ITERATE_DIFFERENCE, last_primal_start_point,
                     last_dual_start_point, stats);
  }
  constexpr int kLogEvery = 15;
  static std::atomic_int log_counter{0};
  if (params.verbosity_level() >= 4) {
    if (log_counter == 0) {
      LogInfoWithoutPrefix(absl::StrCat("I ", IterationStatsLabelString()));
    }
    LogInfoWithoutPrefix(absl::StrCat(
        "A ", ToString(stats, params.termination_criteria(),
                       original_bound_norms_, POINT_TYPE_AVERAGE_ITERATE)));
    LogInfoWithoutPrefix(absl::StrCat(
        "C ", ToString(stats, params.termination_criteria(),
                       original_bound_norms_, POINT_TYPE_CURRENT_ITERATE)));
  } else if (params.verbosity_level() >= 3) {
    if (log_counter == 0) {
      LogInfoWithoutPrefix(IterationStatsLabelString());
    }
    LogInfoWithoutPrefix(ToString(stats, params.termination_criteria(),
                                  original_bound_norms_,
                                  POINT_TYPE_AVERAGE_ITERATE));
  } else if (params.verbosity_level() >= 2) {
    if (log_counter == 0) {
      LogInfoWithoutPrefix(IterationStatsLabelShortString());
    }
    LogInfoWithoutPrefix(ToShortString(stats, params.termination_criteria(),
                                       original_bound_norms_,
                                       POINT_TYPE_AVERAGE_ITERATE));
  }
  if (++log_counter >= kLogEvery) {
    log_counter = 0;
  }
  if (iteration_stats_callback_ != nullptr) {
    iteration_stats_callback_(
        {.termination_criteria = params.termination_criteria(),
         .iteration_stats = stats,
         .bound_norms = original_bound_norms_});
  }

  if (const auto termination = CheckIterateTerminationCriteria(
          params.termination_criteria(), stats, original_bound_norms_,
          force_numerical_termination);
      termination.has_value()) {
    return termination;
  }
  return CheckSimpleTerminationCriteria(params.termination_criteria(), stats,
                                        interrupt_solve);
}

ConvergenceInformation
PreprocessSolver::ComputeConvergenceInformationFromWorkingSolution(
    const PrimalDualHybridGradientParams& params,
    const VectorXd& working_primal, const VectorXd& working_dual,
    PointType candidate_type) const {
  const TerminationCriteria::DetailedOptimalityCriteria criteria =
      EffectiveOptimalityCriteria(params.termination_criteria());
  const double primal_epsilon_ratio =
      EpsilonRatio(criteria.eps_optimal_primal_residual_absolute(),
                   criteria.eps_optimal_primal_residual_relative());
  const double dual_epsilon_ratio =
      EpsilonRatio(criteria.eps_optimal_dual_residual_absolute(),
                   criteria.eps_optimal_dual_residual_relative());
  if (presolve_info_.has_value()) {
    PrimalAndDualSolution original = RecoverOriginalSolution(
        {.primal_solution = working_primal, .dual_solution = working_dual});
    return ComputeConvergenceInformation(
        params, presolve_info_->sharded_original_qp,
        presolve_info_->trivial_col_scaling_vec,
        presolve_info_->trivial_row_scaling_vec, original.primal_solution,
        original.dual_solution, primal_epsilon_ratio, dual_epsilon_ratio,
        candidate_type);
  } else {
    return ComputeConvergenceInformation(
        params, sharded_qp_, col_scaling_vec_, row_scaling_vec_, working_primal,
        working_dual, primal_epsilon_ratio, dual_epsilon_ratio, candidate_type);
  }
}

// 'result' is used both as the input and as the temporary that will be
// returned.
SolverResult PreprocessSolver::ConstructOriginalSolverResult(
    const PrimalDualHybridGradientParams& params, SolverResult result) const {
  const bool use_zero_primal_objective =
      result.solve_log.termination_reason() ==
      TERMINATION_REASON_PRIMAL_INFEASIBLE;
  if (presolve_info_.has_value()) {
    // Transform the solutions so they match the original unscaled problem.
    PrimalAndDualSolution original_solution = RecoverOriginalSolution(
        {.primal_solution = std::move(result.primal_solution),
         .dual_solution = std::move(result.dual_solution)});
    result.primal_solution = std::move(original_solution.primal_solution);
    result.dual_solution = std::move(original_solution.dual_solution);
    // RecoverOriginalSolution doesn't recover reduced costs so we need to
    // compute them with respect to the original problem.
    result.reduced_costs = ReducedCosts(
        params, presolve_info_->sharded_original_qp, result.primal_solution,
        result.dual_solution, use_zero_primal_objective);
  } else {
    result.reduced_costs =
        ReducedCosts(params, sharded_qp_, result.primal_solution,
                     result.dual_solution, use_zero_primal_objective);
    // Transform the solutions so they match the original unscaled problem.
    CoefficientWiseProductInPlace(col_scaling_vec_, sharded_qp_.PrimalSharder(),
                                  result.primal_solution);
    CoefficientWiseProductInPlace(row_scaling_vec_, sharded_qp_.DualSharder(),
                                  result.dual_solution);
    CoefficientWiseQuotientInPlace(
        col_scaling_vec_, sharded_qp_.PrimalSharder(), result.reduced_costs);
  }
  if (iteration_stats_callback_ != nullptr) {
    iteration_stats_callback_(
        {.termination_criteria = params.termination_criteria(),
         .iteration_stats = result.solve_log.solution_stats(),
         .bound_norms = original_bound_norms_});
  }

  if (params.verbosity_level() >= 1) {
    LOG(INFO) << "Termination reason: "
              << TerminationReason_Name(result.solve_log.termination_reason());
    LOG(INFO) << "Solution point type: "
              << PointType_Name(result.solve_log.solution_type());
    LOG(INFO) << "Final solution stats:";
    LOG(INFO) << IterationStatsLabelString();
    LOG(INFO) << ToString(result.solve_log.solution_stats(),
                          params.termination_criteria(), original_bound_norms_,
                          result.solve_log.solution_type());
    const auto& convergence_info = GetConvergenceInformation(
        result.solve_log.solution_stats(), result.solve_log.solution_type());
    if (convergence_info.has_value()) {
      if (std::isfinite(convergence_info->corrected_dual_objective())) {
        LOG(INFO) << "Dual objective after infeasibility correction: "
                  << convergence_info->corrected_dual_objective();
      }
    }
  }
  return result;
}

Solver::Solver(const PrimalDualHybridGradientParams& params,
               VectorXd starting_primal_solution,
               VectorXd starting_dual_solution, const double initial_step_size,
               const double initial_primal_weight,
               const PreprocessSolver* preprocess_solver)
    : params_(params),
      current_primal_solution_(std::move(starting_primal_solution)),
      current_dual_solution_(std::move(starting_dual_solution)),
      primal_average_(&preprocess_solver->ShardedWorkingQp().PrimalSharder()),
      dual_average_(&preprocess_solver->ShardedWorkingQp().DualSharder()),
      step_size_(initial_step_size),
      primal_weight_(initial_primal_weight),
      preprocess_solver_(preprocess_solver) {}

Solver::NextSolutionAndDelta Solver::ComputeNextPrimalSolution(
    double primal_step_size) const {
  const int64_t primal_size = ShardedWorkingQp().PrimalSize();
  NextSolutionAndDelta result = {
      .value = VectorXd(primal_size),
      .delta = VectorXd(primal_size),
  };
  const QuadraticProgram& qp = WorkingQp();
  // This computes the primal portion of the PDHG algorithm:
  // argmin_x[gradient(f)(current_primal_solution)'x + g(x)
  //   + current_dual_solution' K x
  //   + (0.5 / primal_step_size) * norm(x - current_primal_solution) ^ 2]
  // See Sections 2 - 3 of Chambolle and Pock and the comment in the header.
  // We omitted the constant terms from Chambolle and Pock's (7).
  // This minimization is easy to do in closed form since it can be separated
  // into independent problems for each of the primal variables.
  ShardedWorkingQp().PrimalSharder().ParallelForEachShard(
      [&](const Sharder::Shard& shard) {
        if (!IsLinearProgram(qp)) {
          // TODO(user): Does changing this to auto (so it becomes an
          // Eigen deferred result), or inlining it below, change performance?
          const VectorXd diagonal_scaling =
              primal_step_size *
                  shard(qp.objective_matrix->diagonal()).array() +
              1.0;
          shard(result.value) =
              (shard(current_primal_solution_) -
               primal_step_size *
                   (shard(qp.objective_vector) - shard(current_dual_product_)))
                  // Scale i-th element by 1 / (1 + primal_step_size * Q_{ii})
                  .cwiseQuotient(diagonal_scaling)
                  .cwiseMin(shard(qp.variable_upper_bounds))
                  .cwiseMax(shard(qp.variable_lower_bounds));
        } else {
          // The formula in the LP case is simplified for better performance.
          shard(result.value) =
              (shard(current_primal_solution_) -
               primal_step_size *
                   (shard(qp.objective_vector) - shard(current_dual_product_)))
                  .cwiseMin(shard(qp.variable_upper_bounds))
                  .cwiseMax(shard(qp.variable_lower_bounds));
        }
        shard(result.delta) =
            shard(result.value) - shard(current_primal_solution_);
      });
  return result;
}

Solver::NextSolutionAndDelta Solver::ComputeNextDualSolution(
    double dual_step_size, double extrapolation_factor,
    const NextSolutionAndDelta& next_primal_solution) const {
  const int64_t dual_size = ShardedWorkingQp().DualSize();
  NextSolutionAndDelta result = {
      .value = VectorXd(dual_size),
      .delta = VectorXd(dual_size),
  };
  const QuadraticProgram& qp = WorkingQp();
  VectorXd extrapolated_primal(ShardedWorkingQp().PrimalSize());
  ShardedWorkingQp().PrimalSharder().ParallelForEachShard(
      [&](const Sharder::Shard& shard) {
        shard(extrapolated_primal) =
            (shard(next_primal_solution.value) +
             extrapolation_factor * shard(next_primal_solution.delta));
      });
  // TODO(user): Refactor this multiplication so that we only do one matrix
  // vector mutiply for the primal variable. This only applies to Malitsky and
  // Pock and not to the adaptive step size rule.
  ShardedWorkingQp().TransposedConstraintMatrixSharder().ParallelForEachShard(
      [&](const Sharder::Shard& shard) {
        VectorXd temp =
            shard(current_dual_solution_) -
            dual_step_size *
                shard(ShardedWorkingQp().TransposedConstraintMatrix())
                    .transpose() *
                extrapolated_primal;
        // Each element of the argument of the cwiseMin is the critical point
        // of the respective 1D minimization problem if it's negative.
        // Likewise the argument to the cwiseMax is the critical point if
        // positive.
        shard(result.value) =
            VectorXd::Zero(temp.size())
                .cwiseMin(temp +
                          dual_step_size * shard(qp.constraint_upper_bounds))
                .cwiseMax(temp +
                          dual_step_size * shard(qp.constraint_lower_bounds));
        shard(result.delta) =
            (shard(result.value) - shard(current_dual_solution_));
      });
  return result;
}

double Solver::ComputeMovement(const VectorXd& delta_primal,
                               const VectorXd& delta_dual) const {
  const double primal_movement =
      (0.5 * primal_weight_) *
      SquaredNorm(delta_primal, ShardedWorkingQp().PrimalSharder());
  const double dual_movement =
      (0.5 / primal_weight_) *
      SquaredNorm(delta_dual, ShardedWorkingQp().DualSharder());
  return primal_movement + dual_movement;
}

double Solver::ComputeNonlinearity(const VectorXd& delta_primal,
                                   const VectorXd& next_dual_product) const {
  // Lemma 1 in Chambolle and Pock includes a term with L_f, the Lipshitz
  // constant of f. This is zero in our formulation.
  return ShardedWorkingQp().PrimalSharder().ParallelSumOverShards(
      [&](const Sharder::Shard& shard) {
        return -shard(delta_primal)
                    .dot(shard(next_dual_product) -
                         shard(current_dual_product_));
      });
}

IterationStats Solver::CreateSimpleIterationStats(
    RestartChoice restart_used) const {
  IterationStats stats;
  double num_kkt_passes_per_rejected_step = 1.0;
  if (params_.linesearch_rule() ==
      PrimalDualHybridGradientParams::MALITSKY_POCK_LINESEARCH_RULE) {
    num_kkt_passes_per_rejected_step = 0.5;
  }
  stats.set_iteration_number(iterations_completed_);
  stats.set_cumulative_rejected_steps(num_rejected_steps_);
  // TODO(user): This formula doesn't account for kkt passes in major
  // iterations.
  stats.set_cumulative_kkt_matrix_passes(iterations_completed_ +
                                         num_kkt_passes_per_rejected_step *
                                             num_rejected_steps_);
  stats.set_cumulative_time_sec(preprocess_solver_->GetElapsedTime());
  stats.set_restart_used(restart_used);
  stats.set_step_size(step_size_);
  stats.set_primal_weight(primal_weight_);
  return stats;
}

double Solver::DistanceTraveledFromLastStart(
    const VectorXd& primal_solution, const VectorXd& dual_solution) const {
  return std::sqrt((0.5 * primal_weight_) *
                       SquaredDistance(primal_solution,
                                       last_primal_start_point_,
                                       ShardedWorkingQp().PrimalSharder()) +
                   (0.5 / primal_weight_) *
                       SquaredDistance(dual_solution, last_dual_start_point_,
                                       ShardedWorkingQp().DualSharder()));
}

LocalizedLagrangianBounds Solver::ComputeLocalizedBoundsAtCurrent() const {
  const double distance_traveled_by_current = DistanceTraveledFromLastStart(
      current_primal_solution_, current_dual_solution_);
  return ComputeLocalizedLagrangianBounds(
      ShardedWorkingQp(), current_primal_solution_, current_dual_solution_,
      PrimalDualNorm::kEuclideanNorm, primal_weight_,
      distance_traveled_by_current,
      /*primal_product=*/nullptr, &current_dual_product_,
      params_.use_diagonal_qp_trust_region_solver(),
      params_.diagonal_qp_trust_region_solver_tolerance());
}

LocalizedLagrangianBounds Solver::ComputeLocalizedBoundsAtAverage() const {
  // TODO(user): These vectors are recomputed again for termination checks
  // and again if we eventually restart to the average.
  VectorXd average_primal = PrimalAverage();
  VectorXd average_dual = DualAverage();

  const double distance_traveled_by_average =
      DistanceTraveledFromLastStart(average_primal, average_dual);

  return ComputeLocalizedLagrangianBounds(
      ShardedWorkingQp(), average_primal, average_dual,
      PrimalDualNorm::kEuclideanNorm, primal_weight_,
      distance_traveled_by_average,
      /*primal_product=*/nullptr, /*dual_product=*/nullptr,
      params_.use_diagonal_qp_trust_region_solver(),
      params_.diagonal_qp_trust_region_solver_tolerance());
}

bool AverageHasBetterPotential(
    const LocalizedLagrangianBounds& local_bounds_at_average,
    const LocalizedLagrangianBounds& local_bounds_at_current) {
  return BoundGap(local_bounds_at_average) /
             MathUtil::Square(local_bounds_at_average.radius) <
         BoundGap(local_bounds_at_current) /
             MathUtil::Square(local_bounds_at_current.radius);
}

double NormalizedGap(
    const LocalizedLagrangianBounds& local_bounds_at_candidate) {
  const double distance_traveled_by_candidate =
      local_bounds_at_candidate.radius;
  return BoundGap(local_bounds_at_candidate) / distance_traveled_by_candidate;
}

// TODO(user): Review / cleanup adaptive heuristic.
bool Solver::ShouldDoAdaptiveRestartHeuristic(
    double candidate_normalized_gap) const {
  const double gap_reduction_ratio =
      candidate_normalized_gap / normalized_gap_at_last_restart_;
  if (gap_reduction_ratio < params_.sufficient_reduction_for_restart()) {
    return true;
  }
  if (gap_reduction_ratio < params_.necessary_reduction_for_restart() &&
      candidate_normalized_gap > normalized_gap_at_last_trial_) {
    // We've made the "necessary" amount of progress, and iterates appear to
    // be getting worse, so restart.
    return true;
  }
  return false;
}

RestartChoice Solver::DetermineDistanceBasedRestartChoice() const {
  // The following checks are safeguards that normally should not be triggered.
  if (primal_average_.NumTerms() == 0) {
    return RESTART_CHOICE_NO_RESTART;
  } else if (distance_based_restart_info_.length_of_last_restart_period == 0) {
    return RESTART_CHOICE_RESTART_TO_AVERAGE;
  }
  const int restart_period_length = primal_average_.NumTerms();
  const double distance_moved_this_restart_period_by_average =
      DistanceTraveledFromLastStart(primal_average_.ComputeAverage(),
                                    dual_average_.ComputeAverage());
  const double distance_moved_last_restart_period =
      distance_based_restart_info_.distance_moved_last_restart_period;

  // A restart should be triggered when the normalized distance traveled by
  // the average is at least a constant factor smaller than the last.
  // TODO(user): Experiment with using .necessary_reduction_for_restart()
  // as a heuristic when deciding if a restart should be triggered.
  if ((distance_moved_this_restart_period_by_average / restart_period_length) <
      params_.sufficient_reduction_for_restart() *
          (distance_moved_last_restart_period /
           distance_based_restart_info_.length_of_last_restart_period)) {
    // Restart at current solution when it yields a smaller normalized potential
    // function value than the average (heuristic suggested by ohinder@).
    if (AverageHasBetterPotential(ComputeLocalizedBoundsAtAverage(),
                                  ComputeLocalizedBoundsAtCurrent())) {
      return RESTART_CHOICE_RESTART_TO_AVERAGE;
    } else {
      return RESTART_CHOICE_WEIGHTED_AVERAGE_RESET;
    }
  } else {
    return RESTART_CHOICE_NO_RESTART;
  }
}

RestartChoice Solver::ChooseRestartToApply(const bool is_major_iteration) {
  if (!primal_average_.HasNonzeroWeight() &&
      !dual_average_.HasNonzeroWeight()) {
    return RESTART_CHOICE_NO_RESTART;
  }
  // TODO(user): This forced restart is very important for the performance of
  // ADAPTIVE_HEURISTIC. Test if the impact comes primarily from the first
  // forced restart (which would unseat a good initial starting point that could
  // prevent restarts early in the solve) or if it's really needed for the full
  // duration of the solve. If it is really needed, should we then trigger major
  // iterations on powers of two?
  const int restart_length = primal_average_.NumTerms();
  if (restart_length >= iterations_completed_ / 2 &&
      params_.restart_strategy() ==
          PrimalDualHybridGradientParams::ADAPTIVE_HEURISTIC) {
    if (AverageHasBetterPotential(ComputeLocalizedBoundsAtAverage(),
                                  ComputeLocalizedBoundsAtCurrent())) {
      return RESTART_CHOICE_RESTART_TO_AVERAGE;
    } else {
      return RESTART_CHOICE_WEIGHTED_AVERAGE_RESET;
    }
  }
  if (is_major_iteration) {
    switch (params_.restart_strategy()) {
      case PrimalDualHybridGradientParams::NO_RESTARTS:
        return RESTART_CHOICE_WEIGHTED_AVERAGE_RESET;
      case PrimalDualHybridGradientParams::EVERY_MAJOR_ITERATION:
        return RESTART_CHOICE_RESTART_TO_AVERAGE;
      case PrimalDualHybridGradientParams::ADAPTIVE_HEURISTIC: {
        const LocalizedLagrangianBounds local_bounds_at_average =
            ComputeLocalizedBoundsAtAverage();
        const LocalizedLagrangianBounds local_bounds_at_current =
            ComputeLocalizedBoundsAtCurrent();
        double normalized_gap;
        RestartChoice choice;
        if (AverageHasBetterPotential(local_bounds_at_average,
                                      local_bounds_at_current)) {
          normalized_gap = NormalizedGap(local_bounds_at_average);
          choice = RESTART_CHOICE_RESTART_TO_AVERAGE;
        } else {
          normalized_gap = NormalizedGap(local_bounds_at_current);
          choice = RESTART_CHOICE_WEIGHTED_AVERAGE_RESET;
        }
        if (ShouldDoAdaptiveRestartHeuristic(normalized_gap)) {
          return choice;
        } else {
          normalized_gap_at_last_trial_ = normalized_gap;
          return RESTART_CHOICE_NO_RESTART;
        }
      }
      case PrimalDualHybridGradientParams::ADAPTIVE_DISTANCE_BASED: {
        return DetermineDistanceBasedRestartChoice();
      }
      default:
        LOG(FATAL) << "Unrecognized restart_strategy "
                   << params_.restart_strategy();
        return RESTART_CHOICE_UNSPECIFIED;
    }
  } else {
    return RESTART_CHOICE_NO_RESTART;
  }
}

VectorXd Solver::PrimalAverage() const {
  if (primal_average_.HasNonzeroWeight()) {
    return primal_average_.ComputeAverage();
  } else {
    return current_primal_solution_;
  }
}

VectorXd Solver::DualAverage() const {
  if (dual_average_.HasNonzeroWeight()) {
    return dual_average_.ComputeAverage();
  } else {
    return current_dual_solution_;
  }
}

double Solver::ComputeNewPrimalWeight() const {
  const double primal_distance =
      Distance(current_primal_solution_, last_primal_start_point_,
               ShardedWorkingQp().PrimalSharder());
  const double dual_distance =
      Distance(current_dual_solution_, last_dual_start_point_,
               ShardedWorkingQp().DualSharder());
  // This choice of a nonzero tolerance balances performance and numerical
  // issues caused by very huge or very tiny weights. It was picked as the best
  // among {0.0, 1.0e-20, 2.0e-16, 1.0e-10, 1.0e-5} on the preprocessed MIPLIB
  // dataset. The effect of changing this value is relatively minor overall.
  constexpr double kNonzeroTol = 1.0e-10;
  if (primal_distance <= kNonzeroTol || primal_distance >= 1.0 / kNonzeroTol ||
      dual_distance <= kNonzeroTol || dual_distance >= 1.0 / kNonzeroTol) {
    return primal_weight_;
  }
  const double smoothing_param = params_.primal_weight_update_smoothing();
  const double unsmoothed_new_primal_weight = dual_distance / primal_distance;
  const double new_primal_weight =
      std::exp(smoothing_param * std::log(unsmoothed_new_primal_weight) +
               (1.0 - smoothing_param) * std::log(primal_weight_));
  LOG_IF(INFO, params_.verbosity_level() >= 4)
      << "New computed primal weight is " << new_primal_weight
      << " at iteration " << iterations_completed_;
  return new_primal_weight;
}

SolverResult Solver::PickSolutionAndConstructSolverResult(
    VectorXd primal_solution, VectorXd dual_solution,
    const IterationStats& stats, TerminationReason termination_reason,
    PointType output_type, SolveLog solve_log) const {
  switch (output_type) {
    case POINT_TYPE_CURRENT_ITERATE:
      AssignVector(current_primal_solution_, ShardedWorkingQp().PrimalSharder(),
                   primal_solution);
      AssignVector(current_dual_solution_, ShardedWorkingQp().DualSharder(),
                   dual_solution);
      break;
    case POINT_TYPE_ITERATE_DIFFERENCE:
      AssignVector(current_primal_delta_, ShardedWorkingQp().PrimalSharder(),
                   primal_solution);
      AssignVector(current_dual_delta_, ShardedWorkingQp().DualSharder(),
                   dual_solution);
      break;
    case POINT_TYPE_AVERAGE_ITERATE:
    case POINT_TYPE_PRESOLVER_SOLUTION:
      break;
    default:
      // Default to average whenever the type is POINT_TYPE_NONE.
      output_type = POINT_TYPE_AVERAGE_ITERATE;
      break;
  }
  return ConstructSolverResult(
      std::move(primal_solution), std::move(dual_solution), stats,
      termination_reason, output_type, std::move(solve_log));
}

void Solver::ApplyRestartChoice(const RestartChoice restart_to_apply) {
  switch (restart_to_apply) {
    case RESTART_CHOICE_UNSPECIFIED:
    case RESTART_CHOICE_NO_RESTART:
      return;
    case RESTART_CHOICE_WEIGHTED_AVERAGE_RESET:
      LOG_IF(INFO, params_.verbosity_level() >= 4)
          << "Restarted to current on iteration " << iterations_completed_
          << " after " << primal_average_.NumTerms() << " iterations";
      break;
    case RESTART_CHOICE_RESTART_TO_AVERAGE:
      LOG_IF(INFO, params_.verbosity_level() >= 4)
          << "Restarted to average on iteration " << iterations_completed_
          << " after " << primal_average_.NumTerms() << " iterations";
      current_primal_solution_ = primal_average_.ComputeAverage();
      current_dual_solution_ = dual_average_.ComputeAverage();
      current_dual_product_ = TransposedMatrixVectorProduct(
          WorkingQp().constraint_matrix, current_dual_solution_,
          ShardedWorkingQp().ConstraintMatrixSharder());
      break;
  }
  primal_weight_ = ComputeNewPrimalWeight();
  ratio_last_two_step_sizes_ = 1;
  if (params_.restart_strategy() ==
      PrimalDualHybridGradientParams::ADAPTIVE_HEURISTIC) {
    // It's important for the theory that the distances here are calculated
    // given the new primal weight.
    const LocalizedLagrangianBounds local_bounds_at_last_restart =
        ComputeLocalizedBoundsAtCurrent();
    const double distance_traveled_since_last_restart =
        local_bounds_at_last_restart.radius;
    normalized_gap_at_last_restart_ = BoundGap(local_bounds_at_last_restart) /
                                      distance_traveled_since_last_restart;
    normalized_gap_at_last_trial_ = std::numeric_limits<double>::infinity();
  } else if (params_.restart_strategy() ==
             PrimalDualHybridGradientParams::ADAPTIVE_DISTANCE_BASED) {
    // Update parameters for distance-based restarts.
    distance_based_restart_info_ = {
        .distance_moved_last_restart_period = DistanceTraveledFromLastStart(
            current_primal_solution_, current_dual_solution_),
        .length_of_last_restart_period = primal_average_.NumTerms()};
  }
  primal_average_.Clear();
  dual_average_.Clear();
  AssignVector(current_primal_solution_, ShardedWorkingQp().PrimalSharder(),
               /*dest=*/last_primal_start_point_);
  AssignVector(current_dual_solution_, ShardedWorkingQp().DualSharder(),
               /*dest=*/last_dual_start_point_);
}

std::optional<SolverResult> Solver::MajorIterationAndTerminationCheck(
    bool force_numerical_termination, const std::atomic<bool>* interrupt_solve,
    SolveLog& solve_log) {
  const int major_iteration_cycle =
      iterations_completed_ % params_.major_iteration_frequency();
  const bool is_major_iteration =
      major_iteration_cycle == 0 && iterations_completed_ > 0;
  // Just decide what to do for now. The actual restart, if any, is
  // performed after the termination check.
  const RestartChoice restart = force_numerical_termination
                                    ? RESTART_CHOICE_NO_RESTART
                                    : ChooseRestartToApply(is_major_iteration);
  IterationStats stats = CreateSimpleIterationStats(restart);
  const bool check_termination =
      major_iteration_cycle % params_.termination_check_frequency() == 0 ||
      CheckSimpleTerminationCriteria(params_.termination_criteria(), stats,
                                     interrupt_solve)
          .has_value() ||
      force_numerical_termination;
  // We check termination on every major iteration.
  DCHECK(!is_major_iteration || check_termination);
  if (check_termination) {
    // Check for termination and update iteration stats with both simple and
    // solution statistics. The later are computationally harder to compute and
    // hence only computed here.
    VectorXd primal_average = PrimalAverage();
    VectorXd dual_average = DualAverage();

    const std::optional<TerminationReasonAndPointType>
        maybe_termination_reason =
            preprocess_solver_->UpdateIterationStatsAndCheckTermination(
                params_, force_numerical_termination, current_primal_solution_,
                current_dual_solution_,
                primal_average_.HasNonzeroWeight() ? &primal_average : nullptr,
                dual_average_.HasNonzeroWeight() ? &dual_average : nullptr,
                current_primal_delta_.size() > 0 ? &current_primal_delta_
                                                 : nullptr,
                current_dual_delta_.size() > 0 ? &current_dual_delta_ : nullptr,
                last_primal_start_point_, last_dual_start_point_,
                interrupt_solve, stats);
    if (params_.record_iteration_stats()) {
      *solve_log.add_iteration_stats() = stats;
    }
    // We've terminated.
    if (maybe_termination_reason.has_value()) {
      return PickSolutionAndConstructSolverResult(
          std::move(primal_average), std::move(dual_average), stats,
          maybe_termination_reason->reason, maybe_termination_reason->type,
          std::move(solve_log));
    }
  } else if (params_.record_iteration_stats()) {
    // Record simple iteration stats only.
    *solve_log.add_iteration_stats() = stats;
  }
  ApplyRestartChoice(restart);
  return std::nullopt;
}

void Solver::ResetAverageToCurrent() {
  primal_average_.Clear();
  dual_average_.Clear();
  primal_average_.Add(current_primal_solution_, /*weight=*/1.0);
  dual_average_.Add(current_dual_solution_, /*weight=*/1.0);
}

void Solver::LogNumericalTermination() const {
  LOG(WARNING) << "Forced numerical termination at iteration "
               << iterations_completed_;
}

void Solver::LogInnerIterationLimitHit() const {
  LOG(WARNING) << "Inner iteration limit reached at iteration "
               << iterations_completed_;
}

InnerStepOutcome Solver::TakeMalitskyPockStep() {
  InnerStepOutcome outcome = InnerStepOutcome::kSuccessful;
  const double primal_step_size = step_size_ / primal_weight_;
  NextSolutionAndDelta next_primal_solution =
      ComputeNextPrimalSolution(primal_step_size);
  // The theory by Malitsky and Pock holds for any new_step_size in the interval
  // [step_size, step_size * sqrt(1 + theta)]. The dilating coefficient
  // determines where in this interval the new step size lands.
  double dilating_coeff =
      1 + (params_.malitsky_pock_parameters().step_size_interpolation() *
           (sqrt(1 + ratio_last_two_step_sizes_) - 1));
  double new_primal_step_size = primal_step_size * dilating_coeff;
  double step_size_downscaling =
      params_.malitsky_pock_parameters().step_size_downscaling_factor();
  double contraction_factor =
      params_.malitsky_pock_parameters().linesearch_contraction_factor();
  const double dual_weight = primal_weight_ * primal_weight_;
  int inner_iterations = 0;
  for (bool accepted_step = false; !accepted_step; ++inner_iterations) {
    if (inner_iterations >= 60) {
      LogInnerIterationLimitHit();
      ResetAverageToCurrent();
      outcome = InnerStepOutcome::kForceNumericalTermination;
      break;
    }
    const double new_last_two_step_sizes_ratio =
        new_primal_step_size / primal_step_size;
    NextSolutionAndDelta next_dual_solution = ComputeNextDualSolution(
        dual_weight * new_primal_step_size, new_last_two_step_sizes_ratio,
        next_primal_solution);

    VectorXd next_dual_product = TransposedMatrixVectorProduct(
        WorkingQp().constraint_matrix, next_dual_solution.value,
        ShardedWorkingQp().ConstraintMatrixSharder());
    double delta_dual_norm =
        Norm(next_dual_solution.delta, ShardedWorkingQp().DualSharder());
    double delta_dual_prod_norm =
        Distance(current_dual_product_, next_dual_product,
                 ShardedWorkingQp().PrimalSharder());
    if (primal_weight_ * new_primal_step_size * delta_dual_prod_norm <=
        contraction_factor * delta_dual_norm) {
      // Accept new_step_size as a good step.
      step_size_ = new_primal_step_size * primal_weight_;
      ratio_last_two_step_sizes_ = new_last_two_step_sizes_ratio;
      // Malitsky and Pock guarantee uses a nonsymmetric weighted average,
      // the primal variable average involves the initial point, while the dual
      // doesn't. See Theorem 2 in https://arxiv.org/pdf/1608.08883.pdf for
      // details.
      if (!primal_average_.HasNonzeroWeight()) {
        primal_average_.Add(
            current_primal_solution_,
            /*weight=*/new_primal_step_size * new_last_two_step_sizes_ratio);
      }

      current_primal_solution_ = std::move(next_primal_solution.value);
      current_dual_solution_ = std::move(next_dual_solution.value);
      current_dual_product_ = std::move(next_dual_product);
      primal_average_.Add(current_primal_solution_,
                          /*weight=*/new_primal_step_size);
      dual_average_.Add(current_dual_solution_,
                        /*weight=*/new_primal_step_size);
      const double movement =
          ComputeMovement(next_primal_solution.delta, next_dual_solution.delta);
      if (movement == 0.0) {
        LogNumericalTermination();
        ResetAverageToCurrent();
        outcome = InnerStepOutcome::kForceNumericalTermination;
      } else if (movement > kDivergentMovement) {
        LogNumericalTermination();
        outcome = InnerStepOutcome::kForceNumericalTermination;
      }
      current_primal_delta_ = std::move(next_primal_solution.delta);
      current_dual_delta_ = std::move(next_dual_solution.delta);
      break;
    } else {
      new_primal_step_size = step_size_downscaling * new_primal_step_size;
    }
  }
  // inner_iterations isn't incremented for the accepted step.
  num_rejected_steps_ += inner_iterations;
  return outcome;
}

InnerStepOutcome Solver::TakeAdaptiveStep() {
  bool force_numerical_termination = false;
  for (bool accepted_step = false; !accepted_step;) {
    const double primal_step_size = step_size_ / primal_weight_;
    const double dual_step_size = step_size_ * primal_weight_;
    NextSolutionAndDelta next_primal_solution =
        ComputeNextPrimalSolution(primal_step_size);
    NextSolutionAndDelta next_dual_solution = ComputeNextDualSolution(
        dual_step_size, /*extrapolation_factor=*/1.0, next_primal_solution);
    const double movement =
        ComputeMovement(next_primal_solution.delta, next_dual_solution.delta);
    if (movement == 0.0) {
      LogNumericalTermination();
      ResetAverageToCurrent();
      force_numerical_termination = true;
      break;
    } else if (movement > kDivergentMovement) {
      LogNumericalTermination();
      force_numerical_termination = true;
      break;
    }
    VectorXd next_dual_product = TransposedMatrixVectorProduct(
        WorkingQp().constraint_matrix, next_dual_solution.value,
        ShardedWorkingQp().ConstraintMatrixSharder());
    const double nonlinearity =
        ComputeNonlinearity(next_primal_solution.delta, next_dual_product);

    // See equation (5) in https://arxiv.org/pdf/2106.04756.pdf.
    const double step_size_limit =
        nonlinearity > 0 ? movement / nonlinearity
                         : std::numeric_limits<double>::infinity();

    if (step_size_ <= step_size_limit) {
      current_primal_solution_ = std::move(next_primal_solution.value);
      current_dual_solution_ = std::move(next_dual_solution.value);
      current_dual_product_ = std::move(next_dual_product);
      current_primal_delta_ = std::move(next_primal_solution.delta);
      current_dual_delta_ = std::move(next_dual_solution.delta);
      primal_average_.Add(current_primal_solution_, /*weight=*/step_size_);
      dual_average_.Add(current_dual_solution_, /*weight=*/step_size_);
      accepted_step = true;
    }
    const double total_steps_attempted =
        num_rejected_steps_ + iterations_completed_ + 1;
    // Our step sizes are a factor 1 - (total_steps_attempted + 1)^(-
    // step_size_reduction_exponent) smaller than they could be as a margin to
    // reduce rejected steps.
    const double first_term =
        (1 - std::pow(total_steps_attempted + 1.0,
                      -params_.adaptive_linesearch_parameters()
                           .step_size_reduction_exponent())) *
        step_size_limit;
    const double second_term =
        (1 + std::pow(total_steps_attempted + 1.0,
                      -params_.adaptive_linesearch_parameters()
                           .step_size_growth_exponent())) *
        step_size_;
    // From the first term when we have to reject a step, the step_size
    // decreases by a factor of at least 1 - (total_steps_attempted + 1)^(-
    // step_size_reduction_exponent). From the second term we increase the
    // step_size by a factor of at most 1 + (total_steps_attempted +
    // 1)^(-step_size_growth_exponent) Therefore if more than order
    // (total_steps_attempted + 1)^(step_size_reduction_exponent
    // - step_size_growth_exponent) fraction of the time we have a rejected
    // step, we overall decrease the step_size. When the step_size is
    // sufficiently small we stop having rejected steps.
    step_size_ = std::min(first_term, second_term);
    if (!accepted_step) {
      ++num_rejected_steps_;
    }
  }
  if (force_numerical_termination) {
    return InnerStepOutcome::kForceNumericalTermination;
  }
  return InnerStepOutcome::kSuccessful;
}

InnerStepOutcome Solver::TakeConstantSizeStep() {
  const double primal_step_size = step_size_ / primal_weight_;
  const double dual_step_size = step_size_ * primal_weight_;
  NextSolutionAndDelta next_primal_solution =
      ComputeNextPrimalSolution(primal_step_size);
  NextSolutionAndDelta next_dual_solution = ComputeNextDualSolution(
      dual_step_size, /*extrapolation_factor=*/1.0, next_primal_solution);
  const double movement =
      ComputeMovement(next_primal_solution.delta, next_dual_solution.delta);
  if (movement == 0.0) {
    LogNumericalTermination();
    ResetAverageToCurrent();
    return InnerStepOutcome::kForceNumericalTermination;
  } else if (movement > kDivergentMovement) {
    LogNumericalTermination();
    return InnerStepOutcome::kForceNumericalTermination;
  }
  VectorXd next_dual_product = TransposedMatrixVectorProduct(
      WorkingQp().constraint_matrix, next_dual_solution.value,
      ShardedWorkingQp().ConstraintMatrixSharder());
  current_primal_solution_ = std::move(next_primal_solution.value);
  current_dual_solution_ = std::move(next_dual_solution.value);
  current_dual_product_ = std::move(next_dual_product);
  current_primal_delta_ = std::move(next_primal_solution.delta);
  current_dual_delta_ = std::move(next_dual_solution.delta);
  primal_average_.Add(current_primal_solution_, /*weight=*/step_size_);
  dual_average_.Add(current_dual_solution_, /*weight=*/step_size_);
  return InnerStepOutcome::kSuccessful;
}

SolverResult Solver::Solve(const std::atomic<bool>* interrupt_solve,
                           SolveLog solve_log) {
  last_primal_start_point_ =
      CloneVector(current_primal_solution_, ShardedWorkingQp().PrimalSharder());
  last_dual_start_point_ =
      CloneVector(current_dual_solution_, ShardedWorkingQp().DualSharder());
  // Note: Any cached values computed here also need to be recomputed after a
  // restart.

  ratio_last_two_step_sizes_ = 1;
  current_dual_product_ = TransposedMatrixVectorProduct(
      WorkingQp().constraint_matrix, current_dual_solution_,
      ShardedWorkingQp().ConstraintMatrixSharder());

  // This is set to true if we can't proceed any more because of numerical
  // issues. We may or may not have found the optimal solution.
  bool force_numerical_termination = false;

  num_rejected_steps_ = 0;

  for (iterations_completed_ = 0;; ++iterations_completed_) {
    // This code performs the logic of the major iterations and termination
    // checks. It may modify the current solution and primal weight (e.g., when
    // performing a restart).
    const std::optional<SolverResult> maybe_result =
        MajorIterationAndTerminationCheck(force_numerical_termination,
                                          interrupt_solve, solve_log);
    if (maybe_result.has_value()) {
      return maybe_result.value();
    }

    // TODO(user): If we use a step rule that could reject many steps in a
    // row, we should add a termination check within this loop also. For the
    // Malitsky and Pock rule, we perform a termination check and declare
    // NUMERICAL_ERROR whenever we hit 60 inner iterations.
    InnerStepOutcome outcome;
    switch (params_.linesearch_rule()) {
      case PrimalDualHybridGradientParams::MALITSKY_POCK_LINESEARCH_RULE:
        outcome = TakeMalitskyPockStep();
        break;
      case PrimalDualHybridGradientParams::ADAPTIVE_LINESEARCH_RULE:
        outcome = TakeAdaptiveStep();
        break;
      case PrimalDualHybridGradientParams::CONSTANT_STEP_SIZE_RULE:
        outcome = TakeConstantSizeStep();
        break;
      default:
        LOG(FATAL) << "Unrecognized linesearch rule "
                   << params_.linesearch_rule();
    }
    if (outcome == InnerStepOutcome::kForceNumericalTermination) {
      force_numerical_termination = true;
    }
  }  // loop over iterations
}

}  // namespace

SolverResult PrimalDualHybridGradient(
    QuadraticProgram qp, const PrimalDualHybridGradientParams& params,
    const std::atomic<bool>* interrupt_solve,
    IterationStatsCallback iteration_stats_callback) {
  return PrimalDualHybridGradient(std::move(qp), params, std::nullopt,
                                  interrupt_solve,
                                  std::move(iteration_stats_callback));
}

SolverResult PrimalDualHybridGradient(
    QuadraticProgram qp, const PrimalDualHybridGradientParams& params,
    std::optional<PrimalAndDualSolution> initial_solution,
    const std::atomic<bool>* interrupt_solve,
    IterationStatsCallback iteration_stats_callback) {
  const absl::Status params_status =
      ValidatePrimalDualHybridGradientParams(params);
  if (!params_status.ok()) {
    return ErrorSolverResult(TERMINATION_REASON_INVALID_PARAMETER,
                             params_status.ToString());
  }
  if (!qp.constraint_matrix.isCompressed()) {
    return ErrorSolverResult(TERMINATION_REASON_INVALID_PROBLEM,
                             "constraint_matrix must be in compressed format. "
                             "Call constraint_matrix.makeCompressed()");
  }
  const absl::Status dimensions_status = ValidateQuadraticProgramDimensions(qp);
  if (!dimensions_status.ok()) {
    return ErrorSolverResult(TERMINATION_REASON_INVALID_PROBLEM,
                             dimensions_status.ToString());
  }
  if (!HasValidBounds(qp)) {
    return ErrorSolverResult(TERMINATION_REASON_INVALID_PROBLEM,
                             "The input problem has inconsistent bounds.");
  }
  if (qp.objective_scaling_factor == 0) {
    return ErrorSolverResult(TERMINATION_REASON_INVALID_PROBLEM,
                             "The objective scaling factor cannot be zero.");
  }
  PreprocessSolver solver(std::move(qp), params);
  return solver.PreprocessAndSolve(params, std::move(initial_solution),
                                   interrupt_solve,
                                   std::move(iteration_stats_callback));
}

namespace internal {

glop::ProblemSolution ComputeStatuses(const QuadraticProgram& qp,
                                      const PrimalAndDualSolution& solution) {
  glop::ProblemSolution glop_solution(
      glop::RowIndex(solution.dual_solution.size()),
      glop::ColIndex(solution.primal_solution.size()));
  // This doesn't matter much as glop's preprocessor doesn't use this much.
  // We pick IMPRECISE since we are often calling this code early in the solve.
  glop_solution.status = glop::ProblemStatus::IMPRECISE;
  for (glop::RowIndex i{0}; i.value() < solution.dual_solution.size(); ++i) {
    if (qp.constraint_lower_bounds[i.value()] ==
        qp.constraint_upper_bounds[i.value()]) {
      glop_solution.constraint_statuses[i] =
          glop::ConstraintStatus::FIXED_VALUE;
    } else if (solution.dual_solution[i.value()] > 0) {
      glop_solution.constraint_statuses[i] =
          glop::ConstraintStatus::AT_LOWER_BOUND;
    } else if (solution.dual_solution[i.value()] < 0) {
      glop_solution.constraint_statuses[i] =
          glop::ConstraintStatus::AT_UPPER_BOUND;
    } else {
      glop_solution.constraint_statuses[i] = glop::ConstraintStatus::BASIC;
    }
  }

  for (glop::ColIndex i{0}; i.value() < solution.primal_solution.size(); ++i) {
    const bool at_lb = solution.primal_solution[i.value()] <=
                       qp.variable_lower_bounds[i.value()];
    const bool at_ub = solution.primal_solution[i.value()] >=
                       qp.variable_upper_bounds[i.value()];
    // Note that ShardedWeightedAverage is designed so that variables at their
    // bounds will be exactly at their bounds even with floating-point roundoff.
    if (at_lb) {
      if (at_ub) {
        glop_solution.variable_statuses[i] = glop::VariableStatus::FIXED_VALUE;
      } else {
        glop_solution.variable_statuses[i] =
            glop::VariableStatus::AT_LOWER_BOUND;
      }
    } else {
      if (at_ub) {
        glop_solution.variable_statuses[i] =
            glop::VariableStatus::AT_UPPER_BOUND;
      } else {
        glop_solution.variable_statuses[i] = glop::VariableStatus::BASIC;
      }
    }
  }
  return glop_solution;
}

}  // namespace internal

}  // namespace operations_research::pdlp
