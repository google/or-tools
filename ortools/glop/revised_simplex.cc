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

#include "ortools/glop/revised_simplex.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#include "ortools/glop/initial_basis.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_print_utils.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_utils.h"
#include "ortools/lp_data/matrix_utils.h"
#include "ortools/lp_data/permutation.h"
#include "ortools/util/fp_utils.h"

ABSL_FLAG(bool, simplex_display_numbers_as_fractions, false,
          "Display numbers as fractions.");
ABSL_FLAG(bool, simplex_stop_after_first_basis, false,
          "Stop after first basis has been computed.");
ABSL_FLAG(bool, simplex_stop_after_feasibility, false,
          "Stop after first phase has been completed.");
ABSL_FLAG(bool, simplex_display_stats, false, "Display algorithm statistics.");

namespace operations_research {
namespace glop {
namespace {

// Calls the given closure upon destruction. It can be used to ensure that a
// closure is executed whenever a function returns.
class Cleanup {
 public:
  explicit Cleanup(std::function<void()> closure)
      : closure_(std::move(closure)) {}
  ~Cleanup() { closure_(); }

 private:
  std::function<void()> closure_;
};
}  // namespace

#define DCHECK_COL_BOUNDS(col) \
  {                            \
    DCHECK_LE(0, col);         \
    DCHECK_GT(num_cols_, col); \
  }

// TODO(user): Remove this function.
#define DCHECK_ROW_BOUNDS(row) \
  {                            \
    DCHECK_LE(0, row);         \
    DCHECK_GT(num_rows_, row); \
  }

constexpr const uint64_t kDeterministicSeed = 42;

RevisedSimplex::RevisedSimplex()
    : problem_status_(ProblemStatus::INIT),
      objective_(),
      basis_(),
      variable_name_(),
      direction_(),
      error_(),
      deterministic_random_(kDeterministicSeed),
      random_(deterministic_random_),
      basis_factorization_(&compact_matrix_, &basis_),
      variables_info_(compact_matrix_),
      primal_edge_norms_(compact_matrix_, variables_info_,
                         basis_factorization_),
      dual_edge_norms_(basis_factorization_),
      dual_prices_(random_),
      variable_values_(parameters_, compact_matrix_, basis_, variables_info_,
                       basis_factorization_, &dual_edge_norms_, &dual_prices_),
      update_row_(compact_matrix_, transposed_matrix_, variables_info_, basis_,
                  basis_factorization_),
      reduced_costs_(compact_matrix_, objective_, basis_, variables_info_,
                     basis_factorization_, random_),
      entering_variable_(variables_info_, random_, &reduced_costs_),
      primal_prices_(random_, variables_info_, &primal_edge_norms_,
                     &reduced_costs_),
      iteration_stats_(),
      ratio_test_stats_(),
      function_stats_("SimplexFunctionStats"),
      parameters_(),
      test_lu_() {
  SetParameters(parameters_);
}

void RevisedSimplex::ClearStateForNextSolve() {
  SCOPED_TIME_STAT(&function_stats_);
  solution_state_.statuses.clear();
  variable_starting_values_.clear();
}

void RevisedSimplex::LoadStateForNextSolve(const BasisState& state) {
  SCOPED_TIME_STAT(&function_stats_);
  solution_state_ = state;
  solution_state_has_been_set_externally_ = true;
}

void RevisedSimplex::SetStartingVariableValuesForNextSolve(
    const DenseRow& values) {
  variable_starting_values_ = values;
}

void RevisedSimplex::NotifyThatMatrixIsUnchangedForNextSolve() {
  notify_that_matrix_is_unchanged_ = true;
}

Status RevisedSimplex::Solve(const LinearProgram& lp, TimeLimit* time_limit) {
  SCOPED_TIME_STAT(&function_stats_);
  DCHECK(lp.IsCleanedUp());
  GLOP_RETURN_ERROR_IF_NULL(time_limit);
  Cleanup update_deterministic_time_on_return(
      [this, time_limit]() { AdvanceDeterministicTime(time_limit); });

  default_logger_.EnableLogging(parameters_.log_search_progress());
  default_logger_.SetLogToStdOut(parameters_.log_to_stdout());
  SOLVER_LOG(logger_, "");

  // Initialization. Note That Initialize() must be called first since it
  // analyzes the current solver state.
  const double start_time = time_limit->GetElapsedTime();
  GLOP_RETURN_IF_ERROR(Initialize(lp));
  if (logger_->LoggingIsEnabled()) {
    DisplayBasicVariableStatistics();
  }

  dual_infeasibility_improvement_direction_.clear();
  update_row_.Invalidate();
  test_lu_.Clear();
  problem_status_ = ProblemStatus::INIT;
  phase_ = Phase::FEASIBILITY;
  num_iterations_ = 0;
  num_feasibility_iterations_ = 0;
  num_optimization_iterations_ = 0;
  num_push_iterations_ = 0;
  feasibility_time_ = 0.0;
  optimization_time_ = 0.0;
  push_time_ = 0.0;
  total_time_ = 0.0;

  // In case we abort because of an error, we cannot assume that the current
  // solution state will be in sync with all our internal data structure. In
  // case we abort without resetting it, setting this allow us to still use the
  // previous state info, but we will double-check everything.
  solution_state_has_been_set_externally_ = true;

  if (VLOG_IS_ON(2)) {
    ComputeNumberOfEmptyRows();
    ComputeNumberOfEmptyColumns();
    DisplayProblem();
  }
  if (absl::GetFlag(FLAGS_simplex_stop_after_first_basis)) {
    DisplayAllStats();
    return Status::OK();
  }

  const bool use_dual = parameters_.use_dual_simplex();

  // TODO(user): Avoid doing the first phase checks when we know from the
  // incremental solve that the solution is already dual or primal feasible.
  SOLVER_LOG(logger_, "");
  primal_edge_norms_.SetPricingRule(parameters_.feasibility_rule());
  if (use_dual) {
    if (parameters_.perturb_costs_in_dual_simplex()) {
      reduced_costs_.PerturbCosts();
    }

    if (parameters_.use_dedicated_dual_feasibility_algorithm()) {
      variables_info_.MakeBoxedVariableRelevant(false);
      GLOP_RETURN_IF_ERROR(
          DualMinimize(phase_ == Phase::FEASIBILITY, time_limit));

      if (problem_status_ != ProblemStatus::DUAL_INFEASIBLE) {
        // Note(user): In most cases, the matrix will already be refactorized
        // and both Refactorize() and PermuteBasis() will do nothing. However,
        // if the time limit is reached during the first phase, this might not
        // be the case and RecomputeBasicVariableValues() below DCHECKs that the
        // matrix is refactorized. This is not required, but we currently only
        // want to recompute values from scratch when the matrix was just
        // refactorized to maximize precision.
        GLOP_RETURN_IF_ERROR(basis_factorization_.Refactorize());
        PermuteBasis();

        variables_info_.MakeBoxedVariableRelevant(true);
        reduced_costs_.MakeReducedCostsPrecise();

        // This is needed to display errors properly.
        MakeBoxedVariableDualFeasible(
            variables_info_.GetNonBasicBoxedVariables(),
            /*update_basic_values=*/false);
        variable_values_.RecomputeBasicVariableValues();
      }
    } else {
      // Test initial dual infeasibility, ignoring boxed variables. We currently
      // refactorize/recompute the reduced costs if not already done.
      // TODO(user): Not ideal in an incremental setting.
      reduced_costs_.MakeReducedCostsPrecise();
      bool refactorize = reduced_costs_.NeedsBasisRefactorization();
      GLOP_RETURN_IF_ERROR(RefactorizeBasisIfNeeded(&refactorize));

      const Fractional initial_infeasibility =
          reduced_costs_.ComputeMaximumDualInfeasibilityOnNonBoxedVariables();
      if (initial_infeasibility <
          reduced_costs_.GetDualFeasibilityTolerance()) {
        SOLVER_LOG(logger_, "Initial basis is dual feasible.");
        problem_status_ = ProblemStatus::DUAL_FEASIBLE;
        MakeBoxedVariableDualFeasible(
            variables_info_.GetNonBasicBoxedVariables(),
            /*update_basic_values=*/false);
        variable_values_.RecomputeBasicVariableValues();
      } else {
        // Transform problem and recompute variable values.
        variables_info_.TransformToDualPhaseIProblem(
            reduced_costs_.GetDualFeasibilityTolerance(),
            reduced_costs_.GetReducedCosts());
        DenseRow zero;  // We want the FREE variable at zero here.
        variable_values_.ResetAllNonBasicVariableValues(zero);
        variable_values_.RecomputeBasicVariableValues();

        // Optimize.
        DisplayErrors();
        GLOP_RETURN_IF_ERROR(DualMinimize(false, time_limit));

        // Restore original problem and recompute variable values. Note that we
        // need the reduced cost on the fixed positions here.
        variables_info_.EndDualPhaseI(
            reduced_costs_.GetDualFeasibilityTolerance(),
            reduced_costs_.GetFullReducedCosts());
        variable_values_.ResetAllNonBasicVariableValues(
            variable_starting_values_);
        variable_values_.RecomputeBasicVariableValues();

        // TODO(user): Note that if there was cost shifts, we just keep them
        // until the end of the optim.
        //
        // TODO(user): What if slightly infeasible? we shouldn't really stop.
        // Call primal ? use higher tolerance ? It seems we can always kind of
        // continue and deal with the issue later. Find a way other than this +
        // 1e-6 hack.
        if (problem_status_ == ProblemStatus::OPTIMAL) {
          if (reduced_costs_.ComputeMaximumDualInfeasibility() <
              reduced_costs_.GetDualFeasibilityTolerance() + 1e-6) {
            problem_status_ = ProblemStatus::DUAL_FEASIBLE;
          } else {
            SOLVER_LOG(logger_, "Infeasible after first phase.");
            problem_status_ = ProblemStatus::DUAL_INFEASIBLE;
          }
        }
      }
    }
  } else {
    GLOP_RETURN_IF_ERROR(PrimalMinimize(time_limit));

    // After the primal phase I, we need to restore the objective.
    if (problem_status_ != ProblemStatus::PRIMAL_INFEASIBLE) {
      InitializeObjectiveAndTestIfUnchanged(lp);
      reduced_costs_.ResetForNewObjective();
    }
  }

  DisplayErrors();

  phase_ = Phase::OPTIMIZATION;
  feasibility_time_ = time_limit->GetElapsedTime() - start_time;
  primal_edge_norms_.SetPricingRule(parameters_.optimization_rule());
  num_feasibility_iterations_ = num_iterations_;

  // Because of shifts or perturbations, we may need to re-run a dual simplex
  // after the primal simplex finished, or the opposite.
  //
  // We alter between solving with primal and dual Phase II algorithm as long as
  // time limit permits *and* we did not yet achieve the desired precision.
  // I.e., we run iteration i if the solution from iteration i-1 was not precise
  // after we removed the bound and cost shifts and perturbations.
  //
  // NOTE(user): We may still hit the limit of max_number_of_reoptimizations()
  // which means the status returned can be PRIMAL_FEASIBLE or DUAL_FEASIBLE
  // (i.e., these statuses are not necesserily a consequence of hitting a time
  // limit).
  SOLVER_LOG(logger_, "");
  for (int num_optims = 0;
       // We want to enter the loop when both num_optims and num_iterations_ are
       // *equal* to the corresponding limits (to return a meaningful status
       // when the limits are set to 0).
       num_optims <= parameters_.max_number_of_reoptimizations() &&
       !objective_limit_reached_ &&
       (num_iterations_ == 0 ||
        num_iterations_ < parameters_.max_number_of_iterations()) &&
       !time_limit->LimitReached() &&
       !absl::GetFlag(FLAGS_simplex_stop_after_feasibility) &&
       (problem_status_ == ProblemStatus::PRIMAL_FEASIBLE ||
        problem_status_ == ProblemStatus::DUAL_FEASIBLE);
       ++num_optims) {
    if (problem_status_ == ProblemStatus::PRIMAL_FEASIBLE) {
      // Run the primal simplex.
      GLOP_RETURN_IF_ERROR(PrimalMinimize(time_limit));
    } else {
      // Run the dual simplex.
      GLOP_RETURN_IF_ERROR(
          DualMinimize(phase_ == Phase::FEASIBILITY, time_limit));
    }

    // PrimalMinimize() or DualMinimize() always double check the result with
    // maximum precision by refactoring the basis before exiting (except if an
    // iteration or time limit was reached).
    DCHECK(problem_status_ == ProblemStatus::PRIMAL_FEASIBLE ||
           problem_status_ == ProblemStatus::DUAL_FEASIBLE ||
           basis_factorization_.IsRefactorized());

    // If SetIntegralityScale() was called, we preform a polish operation.
    if (!integrality_scale_.empty() &&
        problem_status_ == ProblemStatus::OPTIMAL) {
      GLOP_RETURN_IF_ERROR(Polish(time_limit));
    }

    // Remove the bound and cost shifts (or perturbations).
    //
    // Note(user): Currently, we never do both at the same time, so we could
    // be a bit faster here, but then this is quick anyway.
    variable_values_.ResetAllNonBasicVariableValues(variable_starting_values_);
    GLOP_RETURN_IF_ERROR(basis_factorization_.Refactorize());
    PermuteBasis();
    variable_values_.RecomputeBasicVariableValues();
    reduced_costs_.ClearAndRemoveCostShifts();

    DisplayErrors();

    // TODO(user): We should also confirm the PRIMAL_UNBOUNDED or DUAL_UNBOUNDED
    // status by checking with the other phase I that the problem is really
    // DUAL_INFEASIBLE or PRIMAL_INFEASIBLE. For instance we currently report
    // PRIMAL_UNBOUNDED with the primal on the problem l30.mps instead of
    // OPTIMAL and the dual does not have issues on this problem.
    //
    // TODO(user): There is another issue on infeas/qual.mps. I think we should
    // just check the dual ray, not really the current solution dual
    // feasibility.
    if (problem_status_ == ProblemStatus::PRIMAL_UNBOUNDED) {
      const Fractional tolerance = parameters_.solution_feasibility_tolerance();
      if (reduced_costs_.ComputeMaximumDualResidual() > tolerance ||
          variable_values_.ComputeMaximumPrimalResidual() > tolerance ||
          variable_values_.ComputeMaximumPrimalInfeasibility() > tolerance) {
        SOLVER_LOG(logger_,
                   "PRIMAL_UNBOUNDED was reported, but the residual and/or "
                   "dual infeasibility is above the tolerance");
        if (parameters_.change_status_to_imprecise()) {
          problem_status_ = ProblemStatus::IMPRECISE;
        }
        break;
      }

      // All of our tolerance are okay, but the dual ray might be fishy. This
      // happens on l30.mps and on L1_sixm250obs.mps.gz. If the ray do not
      // seems good enough, we might actually just be at the optimal and have
      // trouble going down to our relatively low default tolerances.
      //
      // The difference bettween optimal and unbounded can be thin. Say you
      // have a free variable with no constraint and a cost of epsilon,
      // depending on epsilon and your tolerance, this will either cause the
      // problem to be unbounded, or can be ignored.
      //
      // Here, we compute what is the cost gain if we move from the current
      // value with the ray up to the bonds + tolerance. If this gain is < 1,
      // it is hard to claim we are really unbounded. This is a quick
      // heuristic to error on the side of optimality rather than
      // unboundedness.
      double max_magnitude = 0.0;
      double min_distance = kInfinity;
      const DenseRow& lower_bounds = variables_info_.GetVariableLowerBounds();
      const DenseRow& upper_bounds = variables_info_.GetVariableUpperBounds();
      double cost_delta = 0.0;
      for (ColIndex col(0); col < num_cols_; ++col) {
        cost_delta += solution_primal_ray_[col] * objective_[col];
        if (solution_primal_ray_[col] > 0 && upper_bounds[col] != kInfinity) {
          const Fractional value = variable_values_.Get(col);
          const Fractional distance = (upper_bounds[col] - value + tolerance) /
                                      solution_primal_ray_[col];
          min_distance = std::min(distance, min_distance);
          max_magnitude = std::max(solution_primal_ray_[col], max_magnitude);
        }
        if (solution_primal_ray_[col] < 0 && lower_bounds[col] != -kInfinity) {
          const Fractional value = variable_values_.Get(col);
          const Fractional distance = (value - lower_bounds[col] + tolerance) /
                                      -solution_primal_ray_[col];
          min_distance = std::min(distance, min_distance);
          max_magnitude = std::max(-solution_primal_ray_[col], max_magnitude);
        }
      }
      SOLVER_LOG(logger_, "Primal unbounded ray: max blocking magnitude = ",
                 max_magnitude, ", min distance to bound + ", tolerance, " = ",
                 min_distance, ", ray cost delta = ", cost_delta);
      if (min_distance * std::abs(cost_delta) < 1 &&
          reduced_costs_.ComputeMaximumDualInfeasibility() <= tolerance) {
        SOLVER_LOG(logger_,
                   "PRIMAL_UNBOUNDED was reported, but the tolerance are good "
                   "and the unbounded ray is not great.");
        SOLVER_LOG(logger_,
                   "The difference between unbounded and optimal can depends "
                   "on a slight change of tolerance, trying to see if we are "
                   "at OPTIMAL after postsolve.");
        problem_status_ = ProblemStatus::OPTIMAL;
      }
      break;
    }
    if (problem_status_ == ProblemStatus::DUAL_UNBOUNDED) {
      const Fractional tolerance = parameters_.solution_feasibility_tolerance();
      if (reduced_costs_.ComputeMaximumDualResidual() > tolerance ||
          variable_values_.ComputeMaximumPrimalResidual() > tolerance ||
          reduced_costs_.ComputeMaximumDualInfeasibility() > tolerance) {
        SOLVER_LOG(logger_,
                   "DUAL_UNBOUNDED was reported, but the residual and/or "
                   "dual infeasibility is above the tolerance");
        if (parameters_.change_status_to_imprecise()) {
          problem_status_ = ProblemStatus::IMPRECISE;
        }
      }
      break;
    }

    // Change the status, if after the shift and perturbation removal the
    // problem is not OPTIMAL anymore.
    if (problem_status_ == ProblemStatus::OPTIMAL) {
      const Fractional solution_tolerance =
          parameters_.solution_feasibility_tolerance();
      const Fractional primal_residual =
          variable_values_.ComputeMaximumPrimalResidual();
      const Fractional dual_residual =
          reduced_costs_.ComputeMaximumDualResidual();
      if (primal_residual > solution_tolerance ||
          dual_residual > solution_tolerance) {
        SOLVER_LOG(logger_,
                   "OPTIMAL was reported, yet one of the residuals is "
                   "above the solution feasibility tolerance after the "
                   "shift/perturbation are removed.");
        if (parameters_.change_status_to_imprecise()) {
          problem_status_ = ProblemStatus::IMPRECISE;
        }
      } else {
        // We use the "precise" tolerances here to try to report the best
        // possible solution. Note however that we cannot really hope for an
        // infeasibility lower than its corresponding residual error. Note that
        // we already adapt the tolerance like this during the simplex
        // execution.
        const Fractional primal_tolerance = std::max(
            primal_residual, parameters_.primal_feasibility_tolerance());
        const Fractional dual_tolerance =
            std::max(dual_residual, parameters_.dual_feasibility_tolerance());
        const Fractional primal_infeasibility =
            variable_values_.ComputeMaximumPrimalInfeasibility();
        const Fractional dual_infeasibility =
            reduced_costs_.ComputeMaximumDualInfeasibility();
        if (primal_infeasibility > primal_tolerance &&
            dual_infeasibility > dual_tolerance) {
          SOLVER_LOG(logger_,
                     "OPTIMAL was reported, yet both of the infeasibility "
                     "are above the tolerance after the "
                     "shift/perturbation are removed.");
          if (parameters_.change_status_to_imprecise()) {
            problem_status_ = ProblemStatus::IMPRECISE;
          }
        } else if (primal_infeasibility > primal_tolerance) {
          if (num_optims == parameters_.max_number_of_reoptimizations()) {
            SOLVER_LOG(logger_,
                       "The primal infeasibility is still higher than the "
                       "requested internal tolerance, but the maximum "
                       "number of optimization is reached.");
            break;
          }
          SOLVER_LOG(logger_, "");
          SOLVER_LOG(logger_, "Re-optimizing with dual simplex ... ");
          problem_status_ = ProblemStatus::DUAL_FEASIBLE;
        } else if (dual_infeasibility > dual_tolerance) {
          if (num_optims == parameters_.max_number_of_reoptimizations()) {
            SOLVER_LOG(logger_,
                       "The dual infeasibility is still higher than the "
                       "requested internal tolerance, but the maximum "
                       "number of optimization is reached.");
            break;
          }
          SOLVER_LOG(logger_, "");
          SOLVER_LOG(logger_, "Re-optimizing with primal simplex ... ");
          problem_status_ = ProblemStatus::PRIMAL_FEASIBLE;
        }
      }
    }
  }

  // Check that the return status is "precise".
  //
  // TODO(user): we currently skip the DUAL_INFEASIBLE status because the
  // quantities are not up to date in this case.
  if (parameters_.change_status_to_imprecise() &&
      problem_status_ != ProblemStatus::DUAL_INFEASIBLE) {
    const Fractional tolerance = parameters_.solution_feasibility_tolerance();
    if (variable_values_.ComputeMaximumPrimalResidual() > tolerance ||
        reduced_costs_.ComputeMaximumDualResidual() > tolerance) {
      problem_status_ = ProblemStatus::IMPRECISE;
    } else if (problem_status_ == ProblemStatus::DUAL_FEASIBLE ||
               problem_status_ == ProblemStatus::DUAL_UNBOUNDED ||
               problem_status_ == ProblemStatus::PRIMAL_INFEASIBLE) {
      if (reduced_costs_.ComputeMaximumDualInfeasibility() > tolerance) {
        problem_status_ = ProblemStatus::IMPRECISE;
      }
    } else if (problem_status_ == ProblemStatus::PRIMAL_FEASIBLE ||
               problem_status_ == ProblemStatus::PRIMAL_UNBOUNDED ||
               problem_status_ == ProblemStatus::DUAL_INFEASIBLE) {
      if (variable_values_.ComputeMaximumPrimalInfeasibility() > tolerance) {
        problem_status_ = ProblemStatus::IMPRECISE;
      }
    }
  }

  total_time_ = time_limit->GetElapsedTime() - start_time;
  optimization_time_ = total_time_ - feasibility_time_;
  num_optimization_iterations_ = num_iterations_ - num_feasibility_iterations_;

  // If the user didn't provide starting variable values, then there is no need
  // to check for super-basic variables.
  if (!variable_starting_values_.empty()) {
    const int num_super_basic = ComputeNumberOfSuperBasicVariables();
    if (num_super_basic > 0) {
      SOLVER_LOG(logger_,
                 "Num super-basic variables left after optimize phase: ",
                 num_super_basic);
      if (parameters_.push_to_vertex()) {
        if (problem_status_ == ProblemStatus::OPTIMAL) {
          SOLVER_LOG(logger_, "");
          phase_ = Phase::PUSH;
          GLOP_RETURN_IF_ERROR(PrimalPush(time_limit));
          // TODO(user): We should re-check for feasibility at this point and
          // apply clean-up as needed.
        } else {
          SOLVER_LOG(logger_,
                     "Skipping push phase because optimize didn't succeed.");
        }
      }
    }
  }

  total_time_ = time_limit->GetElapsedTime() - start_time;
  push_time_ = total_time_ - feasibility_time_ - optimization_time_;
  num_push_iterations_ = num_iterations_ - num_feasibility_iterations_ -
                         num_optimization_iterations_;

  // Store the result for the solution getters.
  solution_objective_value_ = ComputeInitialProblemObjectiveValue();
  solution_dual_values_ = reduced_costs_.GetDualValues();
  solution_reduced_costs_ = reduced_costs_.GetReducedCosts();
  SaveState();

  if (lp.IsMaximizationProblem()) {
    ChangeSign(&solution_dual_values_);
    ChangeSign(&solution_reduced_costs_);
  }

  // If the problem is unbounded, set the objective value to +/- infinity.
  if (problem_status_ == ProblemStatus::DUAL_UNBOUNDED ||
      problem_status_ == ProblemStatus::PRIMAL_UNBOUNDED) {
    solution_objective_value_ =
        (problem_status_ == ProblemStatus::DUAL_UNBOUNDED) ? kInfinity
                                                           : -kInfinity;
    if (lp.IsMaximizationProblem()) {
      solution_objective_value_ = -solution_objective_value_;
    }
  }

  variable_starting_values_.clear();
  DisplayAllStats();
  return Status::OK();
}

ProblemStatus RevisedSimplex::GetProblemStatus() const {
  return problem_status_;
}

Fractional RevisedSimplex::GetObjectiveValue() const {
  return solution_objective_value_;
}

int64_t RevisedSimplex::GetNumberOfIterations() const {
  return num_iterations_;
}

RowIndex RevisedSimplex::GetProblemNumRows() const { return num_rows_; }

ColIndex RevisedSimplex::GetProblemNumCols() const { return num_cols_; }

Fractional RevisedSimplex::GetVariableValue(ColIndex col) const {
  return variable_values_.Get(col);
}

Fractional RevisedSimplex::GetReducedCost(ColIndex col) const {
  return solution_reduced_costs_[col];
}

const DenseRow& RevisedSimplex::GetReducedCosts() const {
  return solution_reduced_costs_;
}

Fractional RevisedSimplex::GetDualValue(RowIndex row) const {
  return solution_dual_values_[row];
}

VariableStatus RevisedSimplex::GetVariableStatus(ColIndex col) const {
  return variables_info_.GetStatusRow()[col];
}

const BasisState& RevisedSimplex::GetState() const { return solution_state_; }

Fractional RevisedSimplex::GetConstraintActivity(RowIndex row) const {
  // Note the negative sign since the slack variable is such that
  // constraint_activity + slack_value = 0.
  return -variable_values_.Get(SlackColIndex(row));
}

ConstraintStatus RevisedSimplex::GetConstraintStatus(RowIndex row) const {
  // The status of the given constraint is the same as the status of the
  // associated slack variable with a change of sign.
  const VariableStatus s = variables_info_.GetStatusRow()[SlackColIndex(row)];
  if (s == VariableStatus::AT_LOWER_BOUND) {
    return ConstraintStatus::AT_UPPER_BOUND;
  }
  if (s == VariableStatus::AT_UPPER_BOUND) {
    return ConstraintStatus::AT_LOWER_BOUND;
  }
  return VariableToConstraintStatus(s);
}

const DenseRow& RevisedSimplex::GetPrimalRay() const {
  DCHECK_EQ(problem_status_, ProblemStatus::PRIMAL_UNBOUNDED);
  return solution_primal_ray_;
}
const DenseColumn& RevisedSimplex::GetDualRay() const {
  DCHECK_EQ(problem_status_, ProblemStatus::DUAL_UNBOUNDED);
  return solution_dual_ray_;
}

const DenseRow& RevisedSimplex::GetDualRayRowCombination() const {
  DCHECK_EQ(problem_status_, ProblemStatus::DUAL_UNBOUNDED);
  return solution_dual_ray_row_combination_;
}

ColIndex RevisedSimplex::GetBasis(RowIndex row) const { return basis_[row]; }

const BasisFactorization& RevisedSimplex::GetBasisFactorization() const {
  DCHECK(basis_factorization_.GetColumnPermutation().empty());
  return basis_factorization_;
}

std::string RevisedSimplex::GetPrettySolverStats() const {
  return absl::StrFormat(
      "Problem status                               : %s\n"
      "Solving time                                 : %-6.4g\n"
      "Number of iterations                         : %u\n"
      "Time for solvability (first phase)           : %-6.4g\n"
      "Number of iterations for solvability         : %u\n"
      "Time for optimization                        : %-6.4g\n"
      "Number of iterations for optimization        : %u\n"
      "Stop after first basis                       : %d\n",
      GetProblemStatusString(problem_status_), total_time_, num_iterations_,
      feasibility_time_, num_feasibility_iterations_, optimization_time_,
      num_optimization_iterations_,
      absl::GetFlag(FLAGS_simplex_stop_after_first_basis));
}

double RevisedSimplex::DeterministicTime() const {
  // TODO(user): Count what is missing.
  return DeterministicTimeForFpOperations(num_update_price_operations_) +
         basis_factorization_.DeterministicTime() +
         update_row_.DeterministicTime() +
         entering_variable_.DeterministicTime() +
         reduced_costs_.DeterministicTime() +
         primal_edge_norms_.DeterministicTime();
}

void RevisedSimplex::SetVariableNames() {
  variable_name_.resize(num_cols_, "");
  for (ColIndex col(0); col < first_slack_col_; ++col) {
    const ColIndex var_index = col + 1;
    variable_name_[col] = absl::StrFormat("x%d", ColToIntIndex(var_index));
  }
  for (ColIndex col(first_slack_col_); col < num_cols_; ++col) {
    const ColIndex var_index = col - first_slack_col_ + 1;
    variable_name_[col] = absl::StrFormat("s%d", ColToIntIndex(var_index));
  }
}

void RevisedSimplex::SetNonBasicVariableStatusAndDeriveValue(
    ColIndex col, VariableStatus status) {
  variables_info_.UpdateToNonBasicStatus(col, status);
  variable_values_.SetNonBasicVariableValueFromStatus(col);
}

bool RevisedSimplex::BasisIsConsistent() const {
  const DenseBitRow& is_basic = variables_info_.GetIsBasicBitRow();
  const VariableStatusRow& variable_statuses = variables_info_.GetStatusRow();
  for (RowIndex row(0); row < num_rows_; ++row) {
    const ColIndex col = basis_[row];
    if (!is_basic.IsSet(col)) return false;
    if (variable_statuses[col] != VariableStatus::BASIC) return false;
  }
  ColIndex cols_in_basis(0);
  ColIndex cols_not_in_basis(0);
  for (ColIndex col(0); col < num_cols_; ++col) {
    cols_in_basis += is_basic.IsSet(col);
    cols_not_in_basis += !is_basic.IsSet(col);
    if (is_basic.IsSet(col) !=
        (variable_statuses[col] == VariableStatus::BASIC)) {
      return false;
    }
  }
  if (cols_in_basis != RowToColIndex(num_rows_)) return false;
  if (cols_not_in_basis != num_cols_ - RowToColIndex(num_rows_)) return false;
  return true;
}

// Note(user): The basis factorization is not updated by this function but by
// UpdateAndPivot().
void RevisedSimplex::UpdateBasis(ColIndex entering_col, RowIndex basis_row,
                                 VariableStatus leaving_variable_status) {
  SCOPED_TIME_STAT(&function_stats_);
  DCHECK_COL_BOUNDS(entering_col);
  DCHECK_ROW_BOUNDS(basis_row);

  // Check that this is not called with an entering_col already in the basis
  // and that the leaving col is indeed in the basis.
  DCHECK(!variables_info_.GetIsBasicBitRow().IsSet(entering_col));
  DCHECK_NE(basis_[basis_row], entering_col);
  DCHECK_NE(basis_[basis_row], kInvalidCol);

  const ColIndex leaving_col = basis_[basis_row];
  DCHECK(variables_info_.GetIsBasicBitRow().IsSet(leaving_col));

  // Make leaving_col leave the basis and update relevant data.
  // Note thate the leaving variable value is not necessarily at its exact
  // bound, which is like a bound shift.
  variables_info_.UpdateToNonBasicStatus(leaving_col, leaving_variable_status);
  DCHECK(leaving_variable_status == VariableStatus::AT_UPPER_BOUND ||
         leaving_variable_status == VariableStatus::AT_LOWER_BOUND ||
         leaving_variable_status == VariableStatus::FIXED_VALUE);

  basis_[basis_row] = entering_col;
  variables_info_.UpdateToBasicStatus(entering_col);
  update_row_.Invalidate();
}

namespace {

// Comparator used to sort column indices according to a given value vector.
class ColumnComparator {
 public:
  explicit ColumnComparator(const DenseRow& value) : value_(value) {}
  bool operator()(ColIndex col_a, ColIndex col_b) const {
    return value_[col_a] < value_[col_b];
  }

 private:
  const DenseRow& value_;
};

}  // namespace

// To understand better what is going on in this function, let us say that this
// algorithm will produce the optimal solution to a problem containing only
// singleton columns (provided that the variables start at the minimum possible
// cost, see DefaultVariableStatus()). This is unit tested.
//
// The error_ must be equal to the constraint activity for the current variable
// values before this function is called. If error_[row] is 0.0, that mean this
// constraint is currently feasible.
void RevisedSimplex::UseSingletonColumnInInitialBasis(RowToColMapping* basis) {
  SCOPED_TIME_STAT(&function_stats_);
  // Computes the singleton columns and the cost variation of the corresponding
  // variables (in the only possible direction, i.e away from its current bound)
  // for a unit change in the infeasibility of the corresponding row.
  //
  // Note that the slack columns will be treated as normal singleton columns.
  std::vector<ColIndex> singleton_column;
  DenseRow cost_variation(num_cols_, 0.0);
  const DenseRow& lower_bounds = variables_info_.GetVariableLowerBounds();
  const DenseRow& upper_bounds = variables_info_.GetVariableUpperBounds();
  for (ColIndex col(0); col < num_cols_; ++col) {
    if (compact_matrix_.column(col).num_entries() != 1) continue;
    if (lower_bounds[col] == upper_bounds[col]) continue;
    const Fractional slope = compact_matrix_.column(col).GetFirstCoefficient();
    if (variable_values_.Get(col) == lower_bounds[col]) {
      cost_variation[col] = objective_[col] / std::abs(slope);
    } else {
      cost_variation[col] = -objective_[col] / std::abs(slope);
    }
    singleton_column.push_back(col);
  }
  if (singleton_column.empty()) return;

  // Sort the singleton columns for the case where many of them correspond to
  // the same row (equivalent to a piecewise-linear objective on this variable).
  // Negative cost_variation first since moving the singleton variable away from
  // its current bound means the least decrease in the objective function for
  // the same "error" variation.
  ColumnComparator comparator(cost_variation);
  std::sort(singleton_column.begin(), singleton_column.end(), comparator);
  DCHECK_LE(cost_variation[singleton_column.front()],
            cost_variation[singleton_column.back()]);

  // Use a singleton column to "absorb" the error when possible to avoid
  // introducing unneeded artificial variables. Note that with scaling on, the
  // only possible coefficient values are 1.0 or -1.0 (or maybe epsilon close to
  // them) and that the SingletonColumnSignPreprocessor makes them all positive.
  // However, this code works for any coefficient value.
  const DenseRow& variable_values = variable_values_.GetDenseRow();
  for (const ColIndex col : singleton_column) {
    const RowIndex row = compact_matrix_.column(col).EntryRow(EntryIndex(0));

    // If no singleton columns have entered the basis for this row, choose the
    // first one. It will be the one with the least decrease in the objective
    // function when it leaves the basis.
    if ((*basis)[row] == kInvalidCol) {
      (*basis)[row] = col;
    }

    // If there is already no error in this row (i.e. it is primal-feasible),
    // there is nothing to do.
    if (error_[row] == 0.0) continue;

    // In this case, all the infeasibility can be "absorbed" and this variable
    // may not be at one of its bound anymore, so we have to use it in the
    // basis.
    const Fractional coeff =
        compact_matrix_.column(col).EntryCoefficient(EntryIndex(0));
    const Fractional new_value = variable_values[col] + error_[row] / coeff;
    if (new_value >= lower_bounds[col] && new_value <= upper_bounds[col]) {
      error_[row] = 0.0;

      // Use this variable in the initial basis.
      (*basis)[row] = col;
      continue;
    }

    // The idea here is that if the singleton column cannot be used to "absorb"
    // all error_[row], if it is boxed, it can still be used to make the
    // infeasibility smaller (with a bound flip).
    const Fractional box_width = variables_info_.GetBoundDifference(col);
    DCHECK_NE(box_width, 0.0);
    DCHECK_NE(error_[row], 0.0);
    const Fractional error_sign = error_[row] / coeff;
    if (variable_values[col] == lower_bounds[col] && error_sign > 0.0) {
      DCHECK(IsFinite(box_width));
      error_[row] -= coeff * box_width;
      SetNonBasicVariableStatusAndDeriveValue(col,
                                              VariableStatus::AT_UPPER_BOUND);
      continue;
    }
    if (variable_values[col] == upper_bounds[col] && error_sign < 0.0) {
      DCHECK(IsFinite(box_width));
      error_[row] += coeff * box_width;
      SetNonBasicVariableStatusAndDeriveValue(col,
                                              VariableStatus::AT_LOWER_BOUND);
      continue;
    }
  }
}

bool RevisedSimplex::InitializeMatrixAndTestIfUnchanged(
    const LinearProgram& lp, bool lp_is_in_equation_form,
    bool* only_change_is_new_rows, bool* only_change_is_new_cols,
    ColIndex* num_new_cols) {
  SCOPED_TIME_STAT(&function_stats_);
  DCHECK(only_change_is_new_rows != nullptr);
  DCHECK(only_change_is_new_cols != nullptr);
  DCHECK(num_new_cols != nullptr);
  DCHECK_EQ(num_cols_, compact_matrix_.num_cols());
  DCHECK_EQ(num_rows_, compact_matrix_.num_rows());

  // This works whether the lp is in equation form (with slack) or not.
  const bool old_part_of_matrix_is_unchanged =
      AreFirstColumnsAndRowsExactlyEquals(
          num_rows_, first_slack_col_, lp.GetSparseMatrix(), compact_matrix_);

  // This is the only adaptation we need for the test below.
  const ColIndex lp_first_slack =
      lp_is_in_equation_form ? lp.GetFirstSlackVariable() : lp.num_variables();

  // Test if the matrix is unchanged, and if yes, just returns true. Note that
  // this doesn't check the columns corresponding to the slack variables,
  // because they were checked by lp.IsInEquationForm() when Solve() was called.
  if (old_part_of_matrix_is_unchanged && lp.num_constraints() == num_rows_ &&
      lp_first_slack == first_slack_col_) {
    return true;
  }

  // Check if the new matrix can be derived from the old one just by adding
  // new rows (i.e new constraints).
  *only_change_is_new_rows = old_part_of_matrix_is_unchanged &&
                             lp.num_constraints() > num_rows_ &&
                             lp_first_slack == first_slack_col_;

  // Check if the new matrix can be derived from the old one just by adding
  // new columns (i.e new variables).
  *only_change_is_new_cols = old_part_of_matrix_is_unchanged &&
                             lp.num_constraints() == num_rows_ &&
                             lp_first_slack > first_slack_col_;
  *num_new_cols = *only_change_is_new_cols ? lp_first_slack - first_slack_col_
                                           : ColIndex(0);

  // Initialize first_slack_.
  first_slack_col_ = lp_first_slack;

  // Initialize the new dimensions.
  num_rows_ = lp.num_constraints();
  num_cols_ = lp_first_slack + RowToColIndex(lp.num_constraints());

  // Populate compact_matrix_ and transposed_matrix_ if needed. Note that we
  // already added all the slack variables at this point, so matrix_ will not
  // change anymore.
  if (lp_is_in_equation_form) {
    // TODO(user): This can be sped up by removing the MatrixView, but then
    // this path will likely go away.
    compact_matrix_.PopulateFromMatrixView(MatrixView(lp.GetSparseMatrix()));
  } else {
    compact_matrix_.PopulateFromSparseMatrixAndAddSlacks(lp.GetSparseMatrix());
  }
  if (parameters_.use_transposed_matrix()) {
    transposed_matrix_.PopulateFromTranspose(compact_matrix_);
  }
  return false;
}

// Preconditions: This should only be called if there are only new variable
// in the lp.
bool RevisedSimplex::OldBoundsAreUnchangedAndNewVariablesHaveOneBoundAtZero(
    const LinearProgram& lp, bool lp_is_in_equation_form,
    ColIndex num_new_cols) {
  SCOPED_TIME_STAT(&function_stats_);
  DCHECK_LE(num_new_cols, first_slack_col_);
  const ColIndex first_new_col(first_slack_col_ - num_new_cols);

  // Check the original variable bounds.
  const DenseRow& lower_bounds = variables_info_.GetVariableLowerBounds();
  const DenseRow& upper_bounds = variables_info_.GetVariableUpperBounds();
  for (ColIndex col(0); col < first_new_col; ++col) {
    if (lower_bounds[col] != lp.variable_lower_bounds()[col] ||
        upper_bounds[col] != lp.variable_upper_bounds()[col]) {
      return false;
    }
  }

  // Check that each new variable has a bound of zero.
  for (ColIndex col(first_new_col); col < first_slack_col_; ++col) {
    if (lp.variable_lower_bounds()[col] != 0.0 &&
        lp.variable_upper_bounds()[col] != 0.0) {
      return false;
    }
  }

  // Check that the slack bounds are unchanged.
  if (lp_is_in_equation_form) {
    for (ColIndex col(first_slack_col_); col < num_cols_; ++col) {
      if (lower_bounds[col - num_new_cols] != lp.variable_lower_bounds()[col] ||
          upper_bounds[col - num_new_cols] != lp.variable_upper_bounds()[col]) {
        return false;
      }
    }
  } else {
    DCHECK_EQ(num_rows_, lp.num_constraints());
    for (RowIndex row(0); row < num_rows_; ++row) {
      const ColIndex col = first_slack_col_ + RowToColIndex(row);
      if (lower_bounds[col - num_new_cols] !=
              -lp.constraint_upper_bounds()[row] ||
          upper_bounds[col - num_new_cols] !=
              -lp.constraint_lower_bounds()[row]) {
        return false;
      }
    }
  }
  return true;
}

bool RevisedSimplex::InitializeObjectiveAndTestIfUnchanged(
    const LinearProgram& lp) {
  SCOPED_TIME_STAT(&function_stats_);

  bool objective_is_unchanged = true;
  objective_.resize(num_cols_, 0.0);

  // This function work whether the lp is in equation form (with slack) or
  // without, since the objective of the slacks are always zero.
  DCHECK_GE(num_cols_, lp.num_variables());
  for (ColIndex col(lp.num_variables()); col < num_cols_; ++col) {
    if (objective_[col] != 0.0) {
      objective_is_unchanged = false;
      objective_[col] = 0.0;
    }
  }

  if (lp.IsMaximizationProblem()) {
    // Note that we use the minimization version of the objective internally.
    for (ColIndex col(0); col < lp.num_variables(); ++col) {
      const Fractional coeff = -lp.objective_coefficients()[col];
      if (objective_[col] != coeff) {
        objective_is_unchanged = false;
        objective_[col] = coeff;
      }
    }
    objective_offset_ = -lp.objective_offset();
    objective_scaling_factor_ = -lp.objective_scaling_factor();
  } else {
    for (ColIndex col(0); col < lp.num_variables(); ++col) {
      const Fractional coeff = lp.objective_coefficients()[col];
      if (objective_[col] != coeff) {
        objective_is_unchanged = false;
        objective_[col] = coeff;
      }
    }
    objective_offset_ = lp.objective_offset();
    objective_scaling_factor_ = lp.objective_scaling_factor();
  }

  return objective_is_unchanged;
}

void RevisedSimplex::InitializeObjectiveLimit(const LinearProgram& lp) {
  objective_limit_reached_ = false;
  DCHECK(std::isfinite(objective_offset_));
  DCHECK(std::isfinite(objective_scaling_factor_));
  DCHECK_NE(0.0, objective_scaling_factor_);

  // This sets dual_objective_limit_ and then primal_objective_limit_.
  for (const bool set_dual : {true, false}) {
    // NOTE(user): If objective_scaling_factor_ is negative, the optimization
    // direction was reversed (during preprocessing or inside revised simplex),
    // i.e., the original problem is maximization. In such case the _meaning_ of
    // the lower and upper limits is swapped. To this end we must change the
    // signs of limits, which happens automatically when calculating shifted
    // limits. We must also use upper (resp. lower) limit in place of lower
    // (resp. upper) limit when calculating the final objective_limit_.
    //
    // Choose lower limit if using the dual simplex and scaling factor is
    // negative or if using the primal simplex and scaling is nonnegative, upper
    // limit otherwise.
    const Fractional limit = (objective_scaling_factor_ >= 0.0) != set_dual
                                 ? parameters_.objective_lower_limit()
                                 : parameters_.objective_upper_limit();
    const Fractional shifted_limit =
        limit / objective_scaling_factor_ - objective_offset_;
    if (set_dual) {
      dual_objective_limit_ = shifted_limit;
    } else {
      primal_objective_limit_ = shifted_limit;
    }
  }
}

// This implementation starts with an initial matrix B equal to the identity
// matrix (modulo a column permutation). For that it uses either the slack
// variables or the singleton columns present in the problem. Afterwards, the
// fixed slacks in the basis are exchanged with normal columns of A if possible
// by the InitialBasis class.
Status RevisedSimplex::CreateInitialBasis() {
  SCOPED_TIME_STAT(&function_stats_);

  // Initialize the variable values and statuses.
  // Note that for the dual algorithm, boxed variables will be made
  // dual-feasible later by MakeBoxedVariableDualFeasible(), so it doesn't
  // really matter at which of their two finite bounds they start.
  variables_info_.InitializeToDefaultStatus();
  variable_values_.ResetAllNonBasicVariableValues(variable_starting_values_);

  // Start by using an all-slack basis.
  RowToColMapping basis(num_rows_, kInvalidCol);
  for (RowIndex row(0); row < num_rows_; ++row) {
    basis[row] = SlackColIndex(row);
  }

  // If possible, for the primal simplex we replace some slack variables with
  // some singleton columns present in the problem.
  const DenseRow& lower_bounds = variables_info_.GetVariableLowerBounds();
  const DenseRow& upper_bounds = variables_info_.GetVariableUpperBounds();
  if (!parameters_.use_dual_simplex() &&
      parameters_.initial_basis() != GlopParameters::MAROS &&
      parameters_.exploit_singleton_column_in_initial_basis()) {
    // For UseSingletonColumnInInitialBasis() to work better, we change
    // the value of the boxed singleton column with a non-zero cost to the best
    // of their two bounds.
    for (ColIndex col(0); col < num_cols_; ++col) {
      if (compact_matrix_.column(col).num_entries() != 1) continue;
      const VariableStatus status = variables_info_.GetStatusRow()[col];
      const Fractional objective = objective_[col];
      if (objective > 0 && IsFinite(lower_bounds[col]) &&
          status == VariableStatus::AT_UPPER_BOUND) {
        SetNonBasicVariableStatusAndDeriveValue(col,
                                                VariableStatus::AT_LOWER_BOUND);
      } else if (objective < 0 && IsFinite(upper_bounds[col]) &&
                 status == VariableStatus::AT_LOWER_BOUND) {
        SetNonBasicVariableStatusAndDeriveValue(col,
                                                VariableStatus::AT_UPPER_BOUND);
      }
    }

    // Compute the primal infeasibility of the initial variable values in
    // error_.
    ComputeVariableValuesError();

    // TODO(user): A better but slightly more complex algorithm would be to:
    // - Ignore all singleton columns except the slacks during phase I.
    // - For this, change the slack variable bounds accordingly.
    // - At the end of phase I, restore the slack variable bounds and perform
    //   the same algorithm to start with feasible and "optimal" values of the
    //   singleton columns.
    basis.assign(num_rows_, kInvalidCol);
    UseSingletonColumnInInitialBasis(&basis);

    // Eventually complete the basis with fixed slack columns.
    for (RowIndex row(0); row < num_rows_; ++row) {
      if (basis[row] == kInvalidCol) {
        basis[row] = SlackColIndex(row);
      }
    }
  }

  // Use an advanced initial basis to remove the fixed variables from the basis.
  if (parameters_.initial_basis() == GlopParameters::NONE) {
    return InitializeFirstBasis(basis);
  }
  if (parameters_.initial_basis() == GlopParameters::MAROS) {
    InitialBasis initial_basis(compact_matrix_, objective_, lower_bounds,
                               upper_bounds, variables_info_.GetTypeRow());
    if (parameters_.use_dual_simplex()) {
      // This dual version only uses zero-cost columns to complete the
      // basis.
      initial_basis.GetDualMarosBasis(num_cols_, &basis);
    } else {
      initial_basis.GetPrimalMarosBasis(num_cols_, &basis);
    }
    int number_changed = 0;
    for (RowIndex row(0); row < num_rows_; ++row) {
      if (basis[row] != SlackColIndex(row)) {
        number_changed++;
      }
    }
    VLOG(1) << "Number of Maros basis changes: " << number_changed;
  } else if (parameters_.initial_basis() == GlopParameters::BIXBY ||
             parameters_.initial_basis() == GlopParameters::TRIANGULAR) {
    // First unassign the fixed variables from basis.
    int num_fixed_variables = 0;
    for (RowIndex row(0); row < basis.size(); ++row) {
      const ColIndex col = basis[row];
      if (lower_bounds[col] == upper_bounds[col]) {
        basis[row] = kInvalidCol;
        ++num_fixed_variables;
      }
    }

    if (num_fixed_variables == 0) {
      SOLVER_LOG(logger_, "Crash is set to ", parameters_.initial_basis(),
                 " but there is no equality rows to remove from initial all "
                 "slack basis. Starting from there.");
    } else {
      // Then complete the basis with an advanced initial basis algorithm.
      SOLVER_LOG(logger_, "Trying to remove ", num_fixed_variables,
                 " fixed variables from the initial basis.");
      InitialBasis initial_basis(compact_matrix_, objective_, lower_bounds,
                                 upper_bounds, variables_info_.GetTypeRow());

      if (parameters_.initial_basis() == GlopParameters::BIXBY) {
        if (parameters_.use_scaling()) {
          initial_basis.CompleteBixbyBasis(first_slack_col_, &basis);
        } else {
          VLOG(1) << "Bixby initial basis algorithm requires the problem "
                  << "to be scaled. Skipping Bixby's algorithm.";
        }
      } else if (parameters_.initial_basis() == GlopParameters::TRIANGULAR) {
        // Note the use of num_cols_ here because this algorithm
        // benefits from treating fixed slack columns like any other column.
        if (parameters_.use_dual_simplex()) {
          // This dual version only uses zero-cost columns to complete the
          // basis.
          initial_basis.CompleteTriangularDualBasis(num_cols_, &basis);
        } else {
          initial_basis.CompleteTriangularPrimalBasis(num_cols_, &basis);
        }

        const Status status = InitializeFirstBasis(basis);
        if (status.ok()) {
          return status;
        } else {
          SOLVER_LOG(
              logger_,
              "Advanced basis algo failed, Reverting to all slack basis.");

          for (RowIndex row(0); row < num_rows_; ++row) {
            basis[row] = SlackColIndex(row);
          }
        }
      }
    }
  } else {
    LOG(WARNING) << "Unsupported initial_basis parameters: "
                 << parameters_.initial_basis();
  }

  return InitializeFirstBasis(basis);
}

Status RevisedSimplex::InitializeFirstBasis(const RowToColMapping& basis) {
  basis_ = basis;

  // For each row which does not have a basic column, assign it to the
  // corresponding slack column.
  basis_.resize(num_rows_, kInvalidCol);
  for (RowIndex row(0); row < num_rows_; ++row) {
    if (basis_[row] == kInvalidCol) {
      basis_[row] = SlackColIndex(row);
    }
  }

  GLOP_RETURN_IF_ERROR(basis_factorization_.Initialize());
  PermuteBasis();

  // Test that the upper bound on the condition number of basis is not too high.
  // The number was not computed by any rigorous analysis, we just prefer to
  // revert to the all slack basis if the condition number of our heuristic
  // first basis seems bad. See for instance on cond11.mps, where we get an
  // infinity upper bound.
  const Fractional condition_number_ub =
      basis_factorization_.ComputeInfinityNormConditionNumberUpperBound();
  if (condition_number_ub > parameters_.initial_condition_number_threshold()) {
    const std::string error_message =
        absl::StrCat("The matrix condition number upper bound is too high: ",
                     condition_number_ub);
    SOLVER_LOG(logger_, error_message);
    return Status(Status::ERROR_LU, error_message);
  }

  // Everything is okay, finish the initialization.
  for (RowIndex row(0); row < num_rows_; ++row) {
    variables_info_.UpdateToBasicStatus(basis_[row]);
  }
  DCHECK(BasisIsConsistent());

  variable_values_.ResetAllNonBasicVariableValues(variable_starting_values_);
  variable_values_.RecomputeBasicVariableValues();

  if (logger_->LoggingIsEnabled()) {
    // TODO(user): Maybe return an error status if this is too high. Note
    // however that if we want to do that, we need to reset variables_info_ to a
    // consistent state.
    const Fractional tolerance = parameters_.primal_feasibility_tolerance();
    if (variable_values_.ComputeMaximumPrimalResidual() > tolerance) {
      SOLVER_LOG(
          logger_,
          "The primal residual of the initial basis is above the tolerance, ",
          variable_values_.ComputeMaximumPrimalResidual(), " vs. ", tolerance);
    }
  }
  return Status::OK();
}

Status RevisedSimplex::Initialize(const LinearProgram& lp) {
  parameters_ = initial_parameters_;
  PropagateParameters();

  // We accept both kind of input.
  //
  // TODO(user): Ideally there should be no need to ever put the slack in the
  // LinearProgram. That take extra memory (one big SparseColumn per slack) and
  // just add visible overhead in incremental solve when one wants to add/remove
  // constraints. But for historical reason, we handle both for now.
  const bool lp_is_in_equation_form = lp.IsInEquationForm();

  // Calling InitializeMatrixAndTestIfUnchanged() first is important because
  // this is where num_rows_ and num_cols_ are computed.
  //
  // Note that these functions can't depend on use_dual_simplex() since we may
  // change it below.
  ColIndex num_new_cols(0);
  bool only_change_is_new_rows = false;
  bool only_change_is_new_cols = false;
  bool matrix_is_unchanged = true;
  bool only_new_bounds = false;
  if (solution_state_.IsEmpty() || !notify_that_matrix_is_unchanged_) {
    matrix_is_unchanged = InitializeMatrixAndTestIfUnchanged(
        lp, lp_is_in_equation_form, &only_change_is_new_rows,
        &only_change_is_new_cols, &num_new_cols);
    only_new_bounds = only_change_is_new_cols && num_new_cols > 0 &&
                      OldBoundsAreUnchangedAndNewVariablesHaveOneBoundAtZero(
                          lp, lp_is_in_equation_form, num_new_cols);
  } else if (DEBUG_MODE) {
    CHECK(InitializeMatrixAndTestIfUnchanged(
        lp, lp_is_in_equation_form, &only_change_is_new_rows,
        &only_change_is_new_cols, &num_new_cols));
  }
  notify_that_matrix_is_unchanged_ = false;

  // TODO(user): move objective with ReducedCosts class.
  const bool objective_is_unchanged = InitializeObjectiveAndTestIfUnchanged(lp);

  const bool bounds_are_unchanged =
      lp_is_in_equation_form
          ? variables_info_.LoadBoundsAndReturnTrueIfUnchanged(
                lp.variable_lower_bounds(), lp.variable_upper_bounds())
          : variables_info_.LoadBoundsAndReturnTrueIfUnchanged(
                lp.variable_lower_bounds(), lp.variable_upper_bounds(),
                lp.constraint_lower_bounds(), lp.constraint_upper_bounds());

  // If parameters_.allow_simplex_algorithm_change() is true and we already have
  // a primal (resp. dual) feasible solution, then we use the primal (resp.
  // dual) algorithm since there is a good chance that it will be faster.
  if (matrix_is_unchanged && parameters_.allow_simplex_algorithm_change()) {
    if (objective_is_unchanged && !bounds_are_unchanged) {
      parameters_.set_use_dual_simplex(true);
      PropagateParameters();
    }
    if (bounds_are_unchanged && !objective_is_unchanged) {
      parameters_.set_use_dual_simplex(false);
      PropagateParameters();
    }
  }

  InitializeObjectiveLimit(lp);

  // Computes the variable name as soon as possible for logging.
  // TODO(user): do we really need to store them? we could just compute them
  // on the fly since we do not need the speed.
  if (VLOG_IS_ON(2)) {
    SetVariableNames();
  }

  // Warm-start? This is supported only if the solution_state_ is non empty,
  // i.e., this revised simplex i) was already used to solve a problem, or
  // ii) the solution state was provided externally. Note that the
  // solution_state_ may have nothing to do with the current problem, e.g.,
  // objective, matrix, and/or bounds had changed. So we support several
  // scenarios of warm-start depending on how did the problem change and which
  // simplex algorithm is used (primal or dual).
  bool solve_from_scratch = true;

  // Try to perform a "quick" warm-start with no matrix factorization involved.
  if (!solution_state_.IsEmpty() && !solution_state_has_been_set_externally_) {
    if (!parameters_.use_dual_simplex()) {
      // With primal simplex, always clear dual norms and dual pricing.
      // Incrementality is supported only if only change to the matrix and
      // bounds is adding new columns (objective may change), and that all
      // new columns have a bound equal to zero.
      dual_edge_norms_.Clear();
      dual_pricing_vector_.clear();
      if (matrix_is_unchanged && bounds_are_unchanged) {
        // TODO(user): Do not do that if objective_is_unchanged. Currently
        // this seems to break something. Investigate.
        reduced_costs_.ClearAndRemoveCostShifts();
        solve_from_scratch = false;
      } else if (only_change_is_new_cols && only_new_bounds) {
        variables_info_.InitializeFromBasisState(first_slack_col_, num_new_cols,
                                                 solution_state_);
        variable_values_.ResetAllNonBasicVariableValues(
            variable_starting_values_);

        const ColIndex first_new_col(first_slack_col_ - num_new_cols);
        for (ColIndex& col_ref : basis_) {
          if (col_ref >= first_new_col) {
            col_ref += num_new_cols;
          }
        }

        // Make sure the primal edge norm are recomputed from scratch.
        // TODO(user): only the norms of the new columns actually need to be
        // computed.
        primal_edge_norms_.Clear();
        reduced_costs_.ClearAndRemoveCostShifts();
        solve_from_scratch = false;
      }
    } else {
      // With dual simplex, always clear primal norms. Incrementality is
      // supported only if the objective remains the same (the matrix may
      // contain new rows and the bounds may change).
      primal_edge_norms_.Clear();
      if (objective_is_unchanged) {
        if (matrix_is_unchanged) {
          if (!bounds_are_unchanged) {
            variables_info_.InitializeFromBasisState(
                first_slack_col_, ColIndex(0), solution_state_);
            variable_values_.ResetAllNonBasicVariableValues(
                variable_starting_values_);
            variable_values_.RecomputeBasicVariableValues();
          }
          solve_from_scratch = false;
        } else if (only_change_is_new_rows) {
          // For the dual-simplex, we also perform a warm start if a couple of
          // new rows where added.
          variables_info_.InitializeFromBasisState(
              first_slack_col_, ColIndex(0), solution_state_);
          dual_edge_norms_.ResizeOnNewRows(num_rows_);

          // TODO(user): The reduced costs do not really need to be recomputed.
          // We just need to initialize the ones of the new slack variables to
          // 0.
          reduced_costs_.ClearAndRemoveCostShifts();
          dual_pricing_vector_.clear();

          // Note that this needs to be done after the Clear() calls above.
          if (InitializeFirstBasis(basis_).ok()) {
            solve_from_scratch = false;
          }
        }
      }
    }
  }

  // If we couldn't perform a "quick" warm start above, we can at least try to
  // reuse the variable statuses.
  if (solve_from_scratch && !solution_state_.IsEmpty()) {
    basis_factorization_.Clear();
    reduced_costs_.ClearAndRemoveCostShifts();
    primal_edge_norms_.Clear();
    dual_edge_norms_.Clear();
    dual_pricing_vector_.clear();

    // If an external basis has been provided or if the matrix changed, we need
    // to perform more work, e.g., factorize the proposed basis and validate it.
    variables_info_.InitializeFromBasisState(first_slack_col_, ColIndex(0),
                                             solution_state_);

    // Use the set of basic columns as a "hint" to construct the first basis.
    std::vector<ColIndex> candidates;
    for (const ColIndex col : variables_info_.GetIsBasicBitRow()) {
      candidates.push_back(col);
    }
    SOLVER_LOG(logger_, "The warm-start state contains ", candidates.size(),
               " candidates for the basis (num_rows = ", num_rows_.value(),
               ").");

    // Optimization: Try to factorize it right away if we have the correct
    // number of element. Ideally the other path below would no require a
    // "double" factorization effort, so this would not be needed.
    if (candidates.size() == num_rows_) {
      basis_.clear();
      for (const ColIndex col : candidates) {
        basis_.push_back(col);
      }

      // TODO(user): Depending on the error here, there is no point doing extra
      // work below. This is the case when we fail because of a bad initial
      // condition number for instance.
      if (InitializeFirstBasis(basis_).ok()) {
        solve_from_scratch = false;
      }
    }

    if (solve_from_scratch) {
      basis_ = basis_factorization_.ComputeInitialBasis(candidates);
      const int num_super_basic =
          variables_info_.ChangeUnusedBasicVariablesToFree(basis_);
      const int num_snapped = variables_info_.SnapFreeVariablesToBound(
          parameters_.crossover_bound_snapping_distance(),
          variable_starting_values_);
      if (logger_->LoggingIsEnabled()) {
        SOLVER_LOG(logger_, "The initial basis did not use ",
                   " BASIC columns from the initial state and used ",
                   (num_rows_ - (candidates.size() - num_super_basic)).value(),
                   " slack variables that were not marked BASIC.");
        if (num_snapped > 0) {
          SOLVER_LOG(logger_, num_snapped,
                     " of the FREE variables where moved to their bound.");
        }
      }

      if (InitializeFirstBasis(basis_).ok()) {
        solve_from_scratch = false;
      } else {
        SOLVER_LOG(logger_,
                   "RevisedSimplex is not using the warm start "
                   "basis because it is not factorizable.");
      }
    }
  }

  if (solve_from_scratch) {
    SOLVER_LOG(logger_, "Starting basis: create from scratch.");
    basis_factorization_.Clear();
    reduced_costs_.ClearAndRemoveCostShifts();
    primal_edge_norms_.Clear();
    dual_edge_norms_.Clear();
    dual_pricing_vector_.clear();
    GLOP_RETURN_IF_ERROR(CreateInitialBasis());
  } else {
    SOLVER_LOG(logger_, "Starting basis: incremental solve.");
  }
  DCHECK(BasisIsConsistent());
  return Status::OK();
}

void RevisedSimplex::DisplayBasicVariableStatistics() {
  SCOPED_TIME_STAT(&function_stats_);

  int num_fixed_variables = 0;
  int num_free_variables = 0;
  int num_variables_at_bound = 0;
  int num_slack_variables = 0;
  int num_infeasible_variables = 0;

  const DenseRow& variable_values = variable_values_.GetDenseRow();
  const VariableTypeRow& variable_types = variables_info_.GetTypeRow();
  const DenseRow& lower_bounds = variables_info_.GetVariableLowerBounds();
  const DenseRow& upper_bounds = variables_info_.GetVariableUpperBounds();
  const Fractional tolerance = parameters_.primal_feasibility_tolerance();
  for (RowIndex row(0); row < num_rows_; ++row) {
    const ColIndex col = basis_[row];
    const Fractional value = variable_values[col];
    if (variable_types[col] == VariableType::UNCONSTRAINED) {
      ++num_free_variables;
    }
    if (value > upper_bounds[col] + tolerance ||
        value < lower_bounds[col] - tolerance) {
      ++num_infeasible_variables;
    }
    if (col >= first_slack_col_) {
      ++num_slack_variables;
    }
    if (lower_bounds[col] == upper_bounds[col]) {
      ++num_fixed_variables;
    } else if (variable_values[col] == lower_bounds[col] ||
               variable_values[col] == upper_bounds[col]) {
      ++num_variables_at_bound;
    }
  }

  SOLVER_LOG(logger_, "The matrix with slacks has ",
             compact_matrix_.num_rows().value(), " rows, ",
             compact_matrix_.num_cols().value(), " columns, ",
             compact_matrix_.num_entries().value(), " entries.");
  SOLVER_LOG(logger_, "Number of basic infeasible variables: ",
             num_infeasible_variables);
  SOLVER_LOG(logger_, "Number of basic slack variables: ", num_slack_variables);
  SOLVER_LOG(logger_,
             "Number of basic variables at bound: ", num_variables_at_bound);
  SOLVER_LOG(logger_, "Number of basic fixed variables: ", num_fixed_variables);
  SOLVER_LOG(logger_, "Number of basic free variables: ", num_free_variables);
  SOLVER_LOG(logger_, "Number of super-basic variables: ",
             ComputeNumberOfSuperBasicVariables());
}

void RevisedSimplex::SaveState() {
  DCHECK_EQ(num_cols_, variables_info_.GetStatusRow().size());
  solution_state_.statuses = variables_info_.GetStatusRow();
  solution_state_has_been_set_externally_ = false;
}

RowIndex RevisedSimplex::ComputeNumberOfEmptyRows() {
  DenseBooleanColumn contains_data(num_rows_, false);
  for (ColIndex col(0); col < num_cols_; ++col) {
    for (const SparseColumn::Entry e : compact_matrix_.column(col)) {
      contains_data[e.row()] = true;
    }
  }
  RowIndex num_empty_rows(0);
  for (RowIndex row(0); row < num_rows_; ++row) {
    if (!contains_data[row]) {
      ++num_empty_rows;
      VLOG(2) << "Row " << row << " is empty.";
    }
  }
  return num_empty_rows;
}

ColIndex RevisedSimplex::ComputeNumberOfEmptyColumns() {
  ColIndex num_empty_cols(0);
  for (ColIndex col(0); col < num_cols_; ++col) {
    if (compact_matrix_.column(col).IsEmpty()) {
      ++num_empty_cols;
      VLOG(2) << "Column " << col << " is empty.";
    }
  }
  return num_empty_cols;
}

int RevisedSimplex::ComputeNumberOfSuperBasicVariables() const {
  const VariableStatusRow& variable_statuses = variables_info_.GetStatusRow();
  int num_super_basic = 0;
  for (ColIndex col(0); col < num_cols_; ++col) {
    if (variable_statuses[col] == VariableStatus::FREE &&
        variable_values_.Get(col) != 0.0) {
      ++num_super_basic;
    }
  }
  return num_super_basic;
}

void RevisedSimplex::CorrectErrorsOnVariableValues() {
  SCOPED_TIME_STAT(&function_stats_);
  DCHECK(basis_factorization_.IsRefactorized());

  // TODO(user): The primal residual error does not change if we take degenerate
  // steps or if we do not change the variable values. No need to recompute it
  // in this case.
  const Fractional primal_residual =
      variable_values_.ComputeMaximumPrimalResidual();

  // If the primal_residual is within the tolerance, no need to recompute
  // the basic variable values with a better precision.
  if (primal_residual >= parameters_.harris_tolerance_ratio() *
                             parameters_.primal_feasibility_tolerance()) {
    variable_values_.RecomputeBasicVariableValues();
    VLOG(1) << "Primal infeasibility (bounds error) = "
            << variable_values_.ComputeMaximumPrimalInfeasibility()
            << ", Primal residual |A.x - b| = "
            << variable_values_.ComputeMaximumPrimalResidual();
  }
}

void RevisedSimplex::ComputeVariableValuesError() {
  SCOPED_TIME_STAT(&function_stats_);
  error_.AssignToZero(num_rows_);
  const DenseRow& variable_values = variable_values_.GetDenseRow();
  for (ColIndex col(0); col < num_cols_; ++col) {
    const Fractional value = variable_values[col];
    compact_matrix_.ColumnAddMultipleToDenseColumn(col, -value, &error_);
  }
}

void RevisedSimplex::ComputeDirection(ColIndex col) {
  SCOPED_TIME_STAT(&function_stats_);
  DCHECK_COL_BOUNDS(col);
  basis_factorization_.RightSolveForProblemColumn(col, &direction_);
  direction_infinity_norm_ = 0.0;
  if (direction_.non_zeros.empty()) {
    // We still compute the direction non-zeros because our code relies on it.
    for (RowIndex row(0); row < num_rows_; ++row) {
      const Fractional value = direction_[row];
      if (value != 0.0) {
        direction_.non_zeros.push_back(row);
        direction_infinity_norm_ =
            std::max(direction_infinity_norm_, std::abs(value));
      }
    }
  } else {
    for (const auto e : direction_) {
      direction_infinity_norm_ =
          std::max(direction_infinity_norm_, std::abs(e.coefficient()));
    }
  }
  IF_STATS_ENABLED(ratio_test_stats_.direction_density.Add(
      num_rows_ == 0 ? 0.0
                     : static_cast<double>(direction_.non_zeros.size()) /
                           static_cast<double>(num_rows_.value())));
}

Fractional RevisedSimplex::ComputeDirectionError(ColIndex col) {
  SCOPED_TIME_STAT(&function_stats_);
  compact_matrix_.ColumnCopyToDenseColumn(col, &error_);
  for (const auto e : direction_) {
    compact_matrix_.ColumnAddMultipleToDenseColumn(col, -e.coefficient(),
                                                   &error_);
  }
  return InfinityNorm(error_);
}

template <bool is_entering_reduced_cost_positive>
Fractional RevisedSimplex::GetRatio(const DenseRow& lower_bounds,
                                    const DenseRow& upper_bounds,
                                    RowIndex row) const {
  const ColIndex col = basis_[row];
  const Fractional direction = direction_[row];
  const Fractional value = variable_values_.Get(col);
  DCHECK(variables_info_.GetIsBasicBitRow().IsSet(col));
  DCHECK_NE(direction, 0.0);
  if (is_entering_reduced_cost_positive) {
    if (direction > 0.0) {
      return (upper_bounds[col] - value) / direction;
    } else {
      return (lower_bounds[col] - value) / direction;
    }
  } else {
    if (direction > 0.0) {
      return (value - lower_bounds[col]) / direction;
    } else {
      return (value - upper_bounds[col]) / direction;
    }
  }
}

template <bool is_entering_reduced_cost_positive>
Fractional RevisedSimplex::ComputeHarrisRatioAndLeavingCandidates(
    Fractional bound_flip_ratio, SparseColumn* leaving_candidates) const {
  SCOPED_TIME_STAT(&function_stats_);
  const Fractional harris_tolerance =
      parameters_.harris_tolerance_ratio() *
      parameters_.primal_feasibility_tolerance();
  const Fractional minimum_delta = parameters_.degenerate_ministep_factor() *
                                   parameters_.primal_feasibility_tolerance();

  // Initially, we can skip any variable with a ratio greater than
  // bound_flip_ratio since it seems to be always better to choose the
  // bound-flip over such leaving variable.
  Fractional harris_ratio = bound_flip_ratio;
  leaving_candidates->Clear();

  // If the basis is refactorized, then we should have everything with a good
  // precision, so we only consider "acceptable" pivots. Otherwise we consider
  // all the entries, and if the algorithm return a pivot that is too small, we
  // will refactorize and recompute the relevant quantities.
  const Fractional threshold = basis_factorization_.IsRefactorized()
                                   ? parameters_.minimum_acceptable_pivot()
                                   : parameters_.ratio_test_zero_threshold();

  const DenseRow& lower_bounds = variables_info_.GetVariableLowerBounds();
  const DenseRow& upper_bounds = variables_info_.GetVariableUpperBounds();
  for (const auto e : direction_) {
    const Fractional magnitude = std::abs(e.coefficient());
    if (magnitude <= threshold) continue;
    const Fractional ratio = GetRatio<is_entering_reduced_cost_positive>(
        lower_bounds, upper_bounds, e.row());
    if (ratio <= harris_ratio) {
      leaving_candidates->SetCoefficient(e.row(), ratio);

      // The second max() makes sure harris_ratio is lower bounded by a small
      // positive value. The more classical approach is to bound it by 0.0 but
      // since we will always perform a small positive step, we allow any
      // variable to go a bit more out of bound (even if it is past the harris
      // tolerance). This increase the number of candidates and allows us to
      // choose a more numerically stable pivot.
      //
      // Note that at least lower bounding it by 0.0 is really important on
      // numerically difficult problems because its helps in the choice of a
      // stable pivot.
      harris_ratio = std::min(harris_ratio,
                              std::max(minimum_delta / magnitude,
                                       ratio + harris_tolerance / magnitude));
    }
  }
  return harris_ratio;
}

namespace {

// Returns true if the candidate ratio is supposed to be more stable than the
// current ratio (or if the two are equal).
// The idea here is to take, by order of preference:
// - the minimum positive ratio in order to intoduce a primal infeasibility
//   which is as small as possible.
// - or the least negative one in order to have the smallest bound shift
//   possible on the leaving variable.
bool IsRatioMoreOrEquallyStable(Fractional candidate, Fractional current) {
  if (current >= 0.0) {
    return candidate >= 0.0 && candidate <= current;
  } else {
    return candidate >= current;
  }
}

}  // namespace

// Ratio-test or Quotient-test. Choose the row of the leaving variable.
// Known as CHUZR or CHUZRO in FORTRAN codes.
Status RevisedSimplex::ChooseLeavingVariableRow(
    ColIndex entering_col, Fractional reduced_cost, bool* refactorize,
    RowIndex* leaving_row, Fractional* step_length, Fractional* target_bound) {
  SCOPED_TIME_STAT(&function_stats_);
  GLOP_RETURN_ERROR_IF_NULL(refactorize);
  GLOP_RETURN_ERROR_IF_NULL(leaving_row);
  GLOP_RETURN_ERROR_IF_NULL(step_length);
  DCHECK_COL_BOUNDS(entering_col);
  DCHECK_NE(0.0, reduced_cost);

  // A few cases will cause the test to be recomputed from the beginning.
  int stats_num_leaving_choices = 0;
  equivalent_leaving_choices_.clear();
  const DenseRow& lower_bounds = variables_info_.GetVariableLowerBounds();
  const DenseRow& upper_bounds = variables_info_.GetVariableUpperBounds();
  while (true) {
    stats_num_leaving_choices = 0;

    // We initialize current_ratio with the maximum step the entering variable
    // can take (bound-flip). Note that we do not use tolerance here.
    const Fractional entering_value = variable_values_.Get(entering_col);
    Fractional current_ratio =
        (reduced_cost > 0.0) ? entering_value - lower_bounds[entering_col]
                             : upper_bounds[entering_col] - entering_value;
    DCHECK_GT(current_ratio, 0.0);

    // First pass of the Harris ratio test. If 'harris_tolerance' is zero, this
    // actually computes the minimum leaving ratio of all the variables. This is
    // the same as the 'classic' ratio test.
    const Fractional harris_ratio =
        (reduced_cost > 0.0) ? ComputeHarrisRatioAndLeavingCandidates<true>(
                                   current_ratio, &leaving_candidates_)
                             : ComputeHarrisRatioAndLeavingCandidates<false>(
                                   current_ratio, &leaving_candidates_);

    // If the bound-flip is a viable solution (i.e. it doesn't move the basic
    // variable too much out of bounds), we take it as it is always stable and
    // fast.
    if (current_ratio <= harris_ratio) {
      *leaving_row = kInvalidRow;
      *step_length = current_ratio;
      break;
    }

    // Second pass of the Harris ratio test. Amongst the variables with 'ratio
    // <= harris_ratio', we choose the leaving row with the largest coefficient.
    //
    // This has a big impact, because picking a leaving variable with a small
    // direction_[row] is the main source of Abnormal LU errors.
    Fractional pivot_magnitude = 0.0;
    stats_num_leaving_choices = 0;
    *leaving_row = kInvalidRow;
    equivalent_leaving_choices_.clear();
    for (const SparseColumn::Entry e : leaving_candidates_) {
      const Fractional ratio = e.coefficient();
      if (ratio > harris_ratio) continue;
      ++stats_num_leaving_choices;
      const RowIndex row = e.row();

      // If the magnitudes are the same, we choose the leaving variable with
      // what is probably the more stable ratio, see
      // IsRatioMoreOrEquallyStable().
      const Fractional candidate_magnitude = std::abs(direction_[row]);
      if (candidate_magnitude < pivot_magnitude) continue;
      if (candidate_magnitude == pivot_magnitude) {
        if (!IsRatioMoreOrEquallyStable(ratio, current_ratio)) continue;
        if (ratio == current_ratio) {
          DCHECK_NE(kInvalidRow, *leaving_row);
          equivalent_leaving_choices_.push_back(row);
          continue;
        }
      }
      equivalent_leaving_choices_.clear();
      current_ratio = ratio;
      pivot_magnitude = candidate_magnitude;
      *leaving_row = row;
    }

    // Break the ties randomly.
    if (!equivalent_leaving_choices_.empty()) {
      equivalent_leaving_choices_.push_back(*leaving_row);
      *leaving_row =
          equivalent_leaving_choices_[std::uniform_int_distribution<int>(
              0, equivalent_leaving_choices_.size() - 1)(random_)];
    }

    // Since we took care of the bound-flip at the beginning, at this point
    // we have a valid leaving row.
    DCHECK_NE(kInvalidRow, *leaving_row);

    // A variable already outside one of its bounds +/- tolerance is considered
    // at its bound and its ratio is zero. Not doing this may lead to a step
    // that moves the objective in the wrong direction. We may want to allow
    // such steps, but then we will need to check that it doesn't break the
    // bounds of the other variables.
    if (current_ratio <= 0.0) {
      // Instead of doing a zero step, we do a small positive step. This
      // helps on degenerate problems.
      const Fractional minimum_delta =
          parameters_.degenerate_ministep_factor() *
          parameters_.primal_feasibility_tolerance();
      *step_length = minimum_delta / pivot_magnitude;
    } else {
      *step_length = current_ratio;
    }

    // Note(user): Testing the pivot at each iteration is useful for debugging
    // an LU factorization problem. Remove the false if you need to investigate
    // this, it makes sure that this will be compiled away.
    if (/* DISABLES CODE */ (false)) {
      TestPivot(entering_col, *leaving_row);
    }

    // We try various "heuristics" to avoid a small pivot.
    //
    // The smaller 'direction_[*leaving_row]', the less precise
    // it is. So we want to avoid pivoting by such a row. Small pivots lead to
    // ill-conditioned bases or even to matrices that are not a basis at all if
    // the actual (infinite-precision) coefficient is zero.
    //
    // TODO(user): We may have to choose another entering column if
    // we cannot prevent pivoting by a small pivot.
    // (Chvatal, p.115, about epsilon2.)
    if (pivot_magnitude <
        parameters_.small_pivot_threshold() * direction_infinity_norm_) {
      // The first countermeasure is to recompute everything to the best
      // precision we can in the hope of avoiding such a choice. Note that this
      // helps a lot on the Netlib problems.
      if (!basis_factorization_.IsRefactorized()) {
        VLOG(1) << "Refactorizing to avoid pivoting by "
                << direction_[*leaving_row]
                << " direction_infinity_norm_ = " << direction_infinity_norm_
                << " reduced cost = " << reduced_cost;
        *refactorize = true;
        return Status::OK();
      }

      // Because of the "threshold" in ComputeHarrisRatioAndLeavingCandidates()
      // we kwnow that this pivot will still have an acceptable magnitude.
      //
      // TODO(user): An issue left to fix is that if there is no such pivot at
      // all, then we will report unbounded even if this is not really the case.
      // As of 2018/07/18, this happens on l30.mps.
      VLOG(1) << "Couldn't avoid pivoting by " << direction_[*leaving_row]
              << " direction_infinity_norm_ = " << direction_infinity_norm_
              << " reduced cost = " << reduced_cost;
      DCHECK_GE(std::abs(direction_[*leaving_row]),
                parameters_.minimum_acceptable_pivot());
      IF_STATS_ENABLED(ratio_test_stats_.abs_tested_pivot.Add(pivot_magnitude));
    }
    break;
  }

  // Update the target bound.
  if (*leaving_row != kInvalidRow) {
    const bool is_reduced_cost_positive = (reduced_cost > 0.0);
    const bool is_leaving_coeff_positive = (direction_[*leaving_row] > 0.0);
    *target_bound = (is_reduced_cost_positive == is_leaving_coeff_positive)
                        ? upper_bounds[basis_[*leaving_row]]
                        : lower_bounds[basis_[*leaving_row]];
  }

  // Stats.
  IF_STATS_ENABLED({
    ratio_test_stats_.leaving_choices.Add(stats_num_leaving_choices);
    if (!equivalent_leaving_choices_.empty()) {
      ratio_test_stats_.num_perfect_ties.Add(
          equivalent_leaving_choices_.size());
    }
    if (*leaving_row != kInvalidRow) {
      ratio_test_stats_.abs_used_pivot.Add(std::abs(direction_[*leaving_row]));
    }
  });
  return Status::OK();
}

namespace {

// Store a row with its ratio, coefficient magnitude and target bound. This is
// used by PrimalPhaseIChooseLeavingVariableRow(), see this function for more
// details.
struct BreakPoint {
  BreakPoint(RowIndex _row, Fractional _ratio, Fractional _coeff_magnitude,
             Fractional _target_bound)
      : row(_row),
        ratio(_ratio),
        coeff_magnitude(_coeff_magnitude),
        target_bound(_target_bound) {}

  // We want to process the breakpoints by increasing ratio and decreasing
  // coefficient magnitude (if the ratios are the same). Returns false if "this"
  // is before "other" in a priority queue.
  bool operator<(const BreakPoint& other) const {
    if (ratio == other.ratio) {
      if (coeff_magnitude == other.coeff_magnitude) {
        return row > other.row;
      }
      return coeff_magnitude < other.coeff_magnitude;
    }
    return ratio > other.ratio;
  }

  RowIndex row;
  Fractional ratio;
  Fractional coeff_magnitude;
  Fractional target_bound;
};

}  // namespace

void RevisedSimplex::PrimalPhaseIChooseLeavingVariableRow(
    ColIndex entering_col, Fractional reduced_cost, bool* refactorize,
    RowIndex* leaving_row, Fractional* step_length,
    Fractional* target_bound) const {
  SCOPED_TIME_STAT(&function_stats_);
  RETURN_IF_NULL(refactorize);
  RETURN_IF_NULL(leaving_row);
  RETURN_IF_NULL(step_length);
  DCHECK_COL_BOUNDS(entering_col);
  DCHECK_NE(0.0, reduced_cost);
  const DenseRow& lower_bounds = variables_info_.GetVariableLowerBounds();
  const DenseRow& upper_bounds = variables_info_.GetVariableUpperBounds();

  // We initialize current_ratio with the maximum step the entering variable
  // can take (bound-flip). Note that we do not use tolerance here.
  const Fractional entering_value = variable_values_.Get(entering_col);
  Fractional current_ratio = (reduced_cost > 0.0)
                                 ? entering_value - lower_bounds[entering_col]
                                 : upper_bounds[entering_col] - entering_value;
  DCHECK_GT(current_ratio, 0.0);

  std::vector<BreakPoint> breakpoints;
  const Fractional tolerance = parameters_.primal_feasibility_tolerance();
  for (const auto e : direction_) {
    const Fractional direction =
        reduced_cost > 0.0 ? e.coefficient() : -e.coefficient();
    const Fractional magnitude = std::abs(direction);
    if (magnitude < tolerance) continue;

    // Computes by how much we can add 'direction' to the basic variable value
    // with index 'row' until it changes of primal feasibility status. That is
    // from infeasible to feasible or from feasible to infeasible. Note that the
    // transition infeasible->feasible->infeasible is possible. We use
    // tolerances here, but when the step will be performed, it will move the
    // variable to the target bound (possibly taking a small negative step).
    //
    // Note(user): The negative step will only happen when the leaving variable
    // was slightly infeasible (less than tolerance). Moreover, the overall
    // infeasibility will not necessarily increase since it doesn't take into
    // account all the variables with an infeasibility smaller than the
    // tolerance, and here we will at least improve the one of the leaving
    // variable.
    const ColIndex col = basis_[e.row()];
    DCHECK(variables_info_.GetIsBasicBitRow().IsSet(col));

    const Fractional value = variable_values_.Get(col);
    const Fractional lower_bound = lower_bounds[col];
    const Fractional upper_bound = upper_bounds[col];
    const Fractional to_lower = (lower_bound - tolerance - value) / direction;
    const Fractional to_upper = (upper_bound + tolerance - value) / direction;

    // Enqueue the possible transitions. Note that the second tests exclude the
    // case where to_lower or to_upper are infinite.
    if (to_lower >= 0.0 && to_lower < current_ratio) {
      breakpoints.push_back(
          BreakPoint(e.row(), to_lower, magnitude, lower_bound));
    }
    if (to_upper >= 0.0 && to_upper < current_ratio) {
      breakpoints.push_back(
          BreakPoint(e.row(), to_upper, magnitude, upper_bound));
    }
  }

  // Order the breakpoints by increasing ratio and decreasing coefficient
  // magnitude (if the ratios are the same).
  std::make_heap(breakpoints.begin(), breakpoints.end());

  // Select the last breakpoint that still improves the infeasibility and has
  // the largest coefficient magnitude.
  Fractional improvement = std::abs(reduced_cost);
  Fractional best_magnitude = 0.0;
  *leaving_row = kInvalidRow;
  while (!breakpoints.empty()) {
    const BreakPoint top = breakpoints.front();
    // TODO(user): consider using >= here. That will lead to bigger ratio and
    // hence a better impact on the infeasibility. The drawback is that more
    // effort may be needed to update the reduced costs.
    //
    // TODO(user): Use a random tie breaking strategy for BreakPoint with
    // same ratio and same coefficient magnitude? Koberstein explains in his PhD
    // that it helped on the dual-simplex.
    if (top.coeff_magnitude > best_magnitude) {
      *leaving_row = top.row;
      current_ratio = top.ratio;
      best_magnitude = top.coeff_magnitude;
      *target_bound = top.target_bound;
    }

    // As long as the sum of primal infeasibilities is decreasing, we look for
    // pivots that are numerically more stable.
    improvement -= top.coeff_magnitude;
    if (improvement <= 0.0) break;
    std::pop_heap(breakpoints.begin(), breakpoints.end());
    breakpoints.pop_back();
  }

  // Try to avoid a small pivot by refactorizing.
  if (*leaving_row != kInvalidRow) {
    const Fractional threshold =
        parameters_.small_pivot_threshold() * direction_infinity_norm_;
    if (best_magnitude < threshold && !basis_factorization_.IsRefactorized()) {
      *refactorize = true;
      return;
    }
  }
  *step_length = current_ratio;
}

// This implements the pricing step for the dual simplex.
Status RevisedSimplex::DualChooseLeavingVariableRow(RowIndex* leaving_row,
                                                    Fractional* cost_variation,
                                                    Fractional* target_bound) {
  SCOPED_TIME_STAT(&function_stats_);
  GLOP_RETURN_ERROR_IF_NULL(leaving_row);
  GLOP_RETURN_ERROR_IF_NULL(cost_variation);
  GLOP_RETURN_ERROR_IF_NULL(target_bound);

  // This is not supposed to happen, but better be safe.
  if (dual_prices_.Size() == 0) {
    variable_values_.RecomputeDualPrices();
  }

  // Return right away if there is no leaving variable.
  // Fill cost_variation and target_bound otherwise.
  *leaving_row = dual_prices_.GetMaximum();
  if (*leaving_row == kInvalidRow) return Status::OK();

  const DenseRow& lower_bounds = variables_info_.GetVariableLowerBounds();
  const DenseRow& upper_bounds = variables_info_.GetVariableUpperBounds();
  const ColIndex leaving_col = basis_[*leaving_row];
  const Fractional value = variable_values_.Get(leaving_col);
  if (value < lower_bounds[leaving_col]) {
    *cost_variation = lower_bounds[leaving_col] - value;
    *target_bound = lower_bounds[leaving_col];
    DCHECK_GT(*cost_variation, 0.0);
  } else {
    *cost_variation = upper_bounds[leaving_col] - value;
    *target_bound = upper_bounds[leaving_col];
    DCHECK_LT(*cost_variation, 0.0);
  }
  return Status::OK();
}

namespace {

// Returns true if a basic variable with given cost and type is to be considered
// as a leaving candidate for the dual phase I.
bool IsDualPhaseILeavingCandidate(Fractional cost, VariableType type,
                                  Fractional threshold) {
  if (cost == 0.0) return false;
  return type == VariableType::UPPER_AND_LOWER_BOUNDED ||
         type == VariableType::FIXED_VARIABLE ||
         (type == VariableType::UPPER_BOUNDED && cost < -threshold) ||
         (type == VariableType::LOWER_BOUNDED && cost > threshold);
}

}  // namespace

// Important: The norm should be updated before this is called.
template <bool use_dense_update>
void RevisedSimplex::OnDualPriceChange(const DenseColumn& squared_norm,
                                       RowIndex row, VariableType type,
                                       Fractional threshold) {
  const Fractional price = dual_pricing_vector_[row];
  const bool is_candidate =
      IsDualPhaseILeavingCandidate(price, type, threshold);
  if (is_candidate) {
    if (use_dense_update) {
      dual_prices_.DenseAddOrUpdate(row, Square(price) / squared_norm[row]);
    } else {
      dual_prices_.AddOrUpdate(row, Square(price) / squared_norm[row]);
    }
  } else {
    dual_prices_.Remove(row);
  }
}

void RevisedSimplex::DualPhaseIUpdatePrice(RowIndex leaving_row,
                                           ColIndex entering_col) {
  SCOPED_TIME_STAT(&function_stats_);

  // If the prices are going to be recomputed, there is nothing to do. See the
  // logic at the beginning of DualPhaseIChooseLeavingVariableRow() which must
  // be in sync with this one.
  //
  // TODO(user): Move the logic in a single class, so it is easier to enforce
  // invariant.
  if (reduced_costs_.AreReducedCostsRecomputed() ||
      dual_edge_norms_.NeedsBasisRefactorization() ||
      dual_pricing_vector_.empty()) {
    return;
  }

  const VariableTypeRow& variable_type = variables_info_.GetTypeRow();
  const Fractional threshold = parameters_.ratio_test_zero_threshold();

  // Note that because the norm are also updated only on the position of the
  // direction, scaled_dual_pricing_vector_ will be up to date.
  const DenseColumn& squared_norms = dual_edge_norms_.GetEdgeSquaredNorms();

  // Convert the dual_pricing_vector_ from the old basis into the new one (which
  // is the same as multiplying it by an Eta matrix corresponding to the
  // direction).
  const Fractional step =
      dual_pricing_vector_[leaving_row] / direction_[leaving_row];
  for (const auto e : direction_) {
    dual_pricing_vector_[e.row()] -= e.coefficient() * step;
    OnDualPriceChange(squared_norms, e.row(), variable_type[basis_[e.row()]],
                      threshold);
  }
  dual_pricing_vector_[leaving_row] = step;

  // The entering_col which was dual-infeasible is now dual-feasible, so we
  // have to remove it from the infeasibility sum.
  dual_pricing_vector_[leaving_row] -=
      dual_infeasibility_improvement_direction_[entering_col];
  if (dual_infeasibility_improvement_direction_[entering_col] != 0.0) {
    --num_dual_infeasible_positions_;
  }
  dual_infeasibility_improvement_direction_[entering_col] = 0.0;

  // The leaving variable will also be dual-feasible.
  dual_infeasibility_improvement_direction_[basis_[leaving_row]] = 0.0;

  // Update the leaving row entering candidate status.
  OnDualPriceChange(squared_norms, leaving_row, variable_type[entering_col],
                    threshold);
}

template <typename Cols>
void RevisedSimplex::DualPhaseIUpdatePriceOnReducedCostChange(
    const Cols& cols) {
  SCOPED_TIME_STAT(&function_stats_);
  bool something_to_do = false;
  const DenseBitRow& can_decrease = variables_info_.GetCanDecreaseBitRow();
  const DenseBitRow& can_increase = variables_info_.GetCanIncreaseBitRow();
  const DenseRow& reduced_costs = reduced_costs_.GetReducedCosts();
  const Fractional tolerance = reduced_costs_.GetDualFeasibilityTolerance();
  for (ColIndex col : cols) {
    const Fractional reduced_cost = reduced_costs[col];
    const Fractional sign =
        (can_increase.IsSet(col) && reduced_cost < -tolerance)  ? 1.0
        : (can_decrease.IsSet(col) && reduced_cost > tolerance) ? -1.0
                                                                : 0.0;
    if (sign != dual_infeasibility_improvement_direction_[col]) {
      if (sign == 0.0) {
        --num_dual_infeasible_positions_;
      } else if (dual_infeasibility_improvement_direction_[col] == 0.0) {
        ++num_dual_infeasible_positions_;
      }
      if (!something_to_do) {
        initially_all_zero_scratchpad_.values.resize(num_rows_, 0.0);
        initially_all_zero_scratchpad_.ClearSparseMask();
        initially_all_zero_scratchpad_.non_zeros.clear();
        something_to_do = true;
      }

      // We add a factor 10 because of the scattered access.
      num_update_price_operations_ +=
          10 * compact_matrix_.column(col).num_entries().value();
      compact_matrix_.ColumnAddMultipleToSparseScatteredColumn(
          col, sign - dual_infeasibility_improvement_direction_[col],
          &initially_all_zero_scratchpad_);
      dual_infeasibility_improvement_direction_[col] = sign;
    }
  }
  if (something_to_do) {
    initially_all_zero_scratchpad_.ClearNonZerosIfTooDense();
    initially_all_zero_scratchpad_.ClearSparseMask();
    const DenseColumn& squared_norms = dual_edge_norms_.GetEdgeSquaredNorms();

    const VariableTypeRow& variable_type = variables_info_.GetTypeRow();
    const Fractional threshold = parameters_.ratio_test_zero_threshold();
    basis_factorization_.RightSolve(&initially_all_zero_scratchpad_);
    if (initially_all_zero_scratchpad_.non_zeros.empty()) {
      dual_prices_.StartDenseUpdates();
      for (RowIndex row(0); row < num_rows_; ++row) {
        if (initially_all_zero_scratchpad_[row] == 0.0) continue;
        dual_pricing_vector_[row] += initially_all_zero_scratchpad_[row];
        OnDualPriceChange</*use_dense_update=*/true>(
            squared_norms, row, variable_type[basis_[row]], threshold);
      }
      initially_all_zero_scratchpad_.values.AssignToZero(num_rows_);
    } else {
      for (const auto e : initially_all_zero_scratchpad_) {
        dual_pricing_vector_[e.row()] += e.coefficient();
        OnDualPriceChange(squared_norms, e.row(),
                          variable_type[basis_[e.row()]], threshold);
        initially_all_zero_scratchpad_[e.row()] = 0.0;
      }
    }
    initially_all_zero_scratchpad_.non_zeros.clear();
  }
}

Status RevisedSimplex::DualPhaseIChooseLeavingVariableRow(
    RowIndex* leaving_row, Fractional* cost_variation,
    Fractional* target_bound) {
  SCOPED_TIME_STAT(&function_stats_);
  GLOP_RETURN_ERROR_IF_NULL(leaving_row);
  GLOP_RETURN_ERROR_IF_NULL(cost_variation);
  const DenseRow& lower_bounds = variables_info_.GetVariableLowerBounds();
  const DenseRow& upper_bounds = variables_info_.GetVariableUpperBounds();

  // dual_infeasibility_improvement_direction_ is zero for dual-feasible
  // positions and contains the sign in which the reduced cost of this column
  // needs to move to improve the feasibility otherwise (+1 or -1).
  //
  // Its current value was the one used to compute dual_pricing_vector_ and
  // was updated accordingly by DualPhaseIUpdatePrice().
  //
  // If more variables changed of dual-feasibility status during the last
  // iteration, we need to call DualPhaseIUpdatePriceOnReducedCostChange() to
  // take them into account.
  if (reduced_costs_.AreReducedCostsRecomputed() ||
      dual_edge_norms_.NeedsBasisRefactorization() ||
      dual_pricing_vector_.empty()) {
    // Recompute everything from scratch.
    num_dual_infeasible_positions_ = 0;
    dual_pricing_vector_.AssignToZero(num_rows_);
    dual_prices_.ClearAndResize(num_rows_);
    dual_infeasibility_improvement_direction_.AssignToZero(num_cols_);
    DualPhaseIUpdatePriceOnReducedCostChange(
        variables_info_.GetIsRelevantBitRow());
  } else {
    // Update row is still equal to the row used during the last iteration
    // to update the reduced costs.
    DualPhaseIUpdatePriceOnReducedCostChange(update_row_.GetNonZeroPositions());
  }

  // If there is no dual-infeasible position, we are done.
  *leaving_row = kInvalidRow;
  if (num_dual_infeasible_positions_ == 0) return Status::OK();

  *leaving_row = dual_prices_.GetMaximum();

  // Returns right away if there is no leaving variable or fill the other
  // return values otherwise.
  if (*leaving_row == kInvalidRow) return Status::OK();
  *cost_variation = dual_pricing_vector_[*leaving_row];
  const ColIndex leaving_col = basis_[*leaving_row];
  if (*cost_variation < 0.0) {
    *target_bound = upper_bounds[leaving_col];
  } else {
    *target_bound = lower_bounds[leaving_col];
  }
  DCHECK(IsFinite(*target_bound));
  return Status::OK();
}

template <typename BoxedVariableCols>
void RevisedSimplex::MakeBoxedVariableDualFeasible(
    const BoxedVariableCols& cols, bool update_basic_values) {
  SCOPED_TIME_STAT(&function_stats_);
  std::vector<ColIndex> changed_cols;

  // It is important to flip bounds within a tolerance because of precision
  // errors. Otherwise, this leads to cycling on many of the Netlib problems
  // since this is called at each iteration (because of the bound-flipping ratio
  // test).
  const DenseRow& variable_values = variable_values_.GetDenseRow();
  const DenseRow& reduced_costs = reduced_costs_.GetReducedCosts();
  const DenseRow& lower_bounds = variables_info_.GetVariableLowerBounds();
  const DenseRow& upper_bounds = variables_info_.GetVariableUpperBounds();
  const Fractional dual_feasibility_tolerance =
      reduced_costs_.GetDualFeasibilityTolerance();
  const VariableStatusRow& variable_status = variables_info_.GetStatusRow();
  for (const ColIndex col : cols) {
    const Fractional reduced_cost = reduced_costs[col];
    const VariableStatus status = variable_status[col];
    DCHECK(variables_info_.GetTypeRow()[col] ==
           VariableType::UPPER_AND_LOWER_BOUNDED);
    // TODO(user): refactor this as DCHECK(IsVariableBasicOrExactlyAtBound())?
    DCHECK(variable_values[col] == lower_bounds[col] ||
           variable_values[col] == upper_bounds[col] ||
           status == VariableStatus::BASIC);
    if (reduced_cost > dual_feasibility_tolerance &&
        status == VariableStatus::AT_UPPER_BOUND) {
      variables_info_.UpdateToNonBasicStatus(col,
                                             VariableStatus::AT_LOWER_BOUND);
      changed_cols.push_back(col);
    } else if (reduced_cost < -dual_feasibility_tolerance &&
               status == VariableStatus::AT_LOWER_BOUND) {
      variables_info_.UpdateToNonBasicStatus(col,
                                             VariableStatus::AT_UPPER_BOUND);
      changed_cols.push_back(col);
    }
  }

  if (!changed_cols.empty()) {
    iteration_stats_.num_dual_flips.Add(changed_cols.size());
    variable_values_.UpdateGivenNonBasicVariables(changed_cols,
                                                  update_basic_values);
  }
}

Fractional RevisedSimplex::ComputeStepToMoveBasicVariableToBound(
    RowIndex leaving_row, Fractional target_bound) {
  SCOPED_TIME_STAT(&function_stats_);

  // We just want the leaving variable to go to its target_bound.
  const ColIndex leaving_col = basis_[leaving_row];
  const Fractional leaving_variable_value = variable_values_.Get(leaving_col);
  Fractional unscaled_step = leaving_variable_value - target_bound;

  // In Chvatal p 157 update_[entering_col] is used instead of
  // direction_[leaving_row], but the two quantities are actually the
  // same. This is because update_[col] is the value at leaving_row of
  // the right inverse of col and direction_ is the right inverse of the
  // entering_col. Note that direction_[leaving_row] is probably more
  // precise.
  // TODO(user): use this to check precision and trigger recomputation.
  return unscaled_step / direction_[leaving_row];
}

bool RevisedSimplex::TestPivot(ColIndex entering_col, RowIndex leaving_row) {
  VLOG(1) << "Test pivot.";
  SCOPED_TIME_STAT(&function_stats_);
  const ColIndex leaving_col = basis_[leaving_row];
  basis_[leaving_row] = entering_col;

  // TODO(user): If 'is_ok' is true, we could use the computed lu in
  // basis_factorization_ rather than recompute it during UpdateAndPivot().
  CompactSparseMatrixView basis_matrix(&compact_matrix_, &basis_);
  const bool is_ok = test_lu_.ComputeFactorization(basis_matrix).ok();
  basis_[leaving_row] = leaving_col;
  return is_ok;
}

// Note that this function is an optimization and that if it was doing nothing
// the algorithm will still be correct and work. Using it does change the pivot
// taken during the simplex method though.
void RevisedSimplex::PermuteBasis() {
  SCOPED_TIME_STAT(&function_stats_);

  // Fetch the current basis column permutation and return if it is empty which
  // means the permutation is the identity.
  const ColumnPermutation& col_perm =
      basis_factorization_.GetColumnPermutation();
  if (col_perm.empty()) return;

  // Permute basis_.
  ApplyColumnPermutationToRowIndexedVector(col_perm, &basis_);

  // Permute dual_pricing_vector_ if needed.
  if (!dual_pricing_vector_.empty()) {
    // TODO(user): We need to permute dual_prices_ too now, we recompute
    // everything one each basis factorization, so this don't matter.
    ApplyColumnPermutationToRowIndexedVector(col_perm, &dual_pricing_vector_);
  }

  // Notify the other classes.
  reduced_costs_.UpdateDataOnBasisPermutation();
  dual_edge_norms_.UpdateDataOnBasisPermutation(col_perm);

  // Finally, remove the column permutation from all subsequent solves since
  // it has been taken into account in basis_.
  basis_factorization_.SetColumnPermutationToIdentity();
}

Status RevisedSimplex::UpdateAndPivot(ColIndex entering_col,
                                      RowIndex leaving_row,
                                      Fractional target_bound) {
  SCOPED_TIME_STAT(&function_stats_);

  // Tricky and a bit hacky.
  //
  // The basis update code assumes that we already computed the left inverse of
  // the leaving row, otherwise it will just refactorize the basis. This left
  // inverse is needed by update_row_.ComputeUpdateRow(), so in most case it
  // will already be computed. However, in some situation we don't need the
  // full update row, so just the left inverse can be computed.
  //
  // TODO(user): Ideally this shouldn't be needed if we are going to refactorize
  // the basis anyway. So we should know that before hand which is currently
  // hard to do.
  Fractional pivot_from_update_row;
  if (update_row_.IsComputed()) {
    pivot_from_update_row = update_row_.GetCoefficient(entering_col);
  } else {
    // We only need the left inverse and the update row position at the
    // entering_col to check precision.
    update_row_.ComputeUnitRowLeftInverse(leaving_row);
    pivot_from_update_row = compact_matrix_.ColumnScalarProduct(
        entering_col, update_row_.GetUnitRowLeftInverse().values);
  }

  const DenseRow& lower_bounds = variables_info_.GetVariableLowerBounds();
  const DenseRow& upper_bounds = variables_info_.GetVariableUpperBounds();
  const ColIndex leaving_col = basis_[leaving_row];
  const VariableStatus leaving_variable_status =
      lower_bounds[leaving_col] == upper_bounds[leaving_col]
          ? VariableStatus::FIXED_VALUE
      : target_bound == lower_bounds[leaving_col]
          ? VariableStatus::AT_LOWER_BOUND
          : VariableStatus::AT_UPPER_BOUND;
  if (variable_values_.Get(leaving_col) != target_bound) {
    ratio_test_stats_.bound_shift.Add(variable_values_.Get(leaving_col) -
                                      target_bound);
  }
  UpdateBasis(entering_col, leaving_row, leaving_variable_status);

  // Test precision by comparing two ways to get the "pivot".
  const Fractional pivot_from_direction = direction_[leaving_row];
  const Fractional diff =
      std::abs(pivot_from_update_row - pivot_from_direction);
  if (diff > parameters_.refactorization_threshold() *
                 (1 + std::abs(pivot_from_direction))) {
    VLOG(1) << "Refactorizing: imprecise pivot " << pivot_from_direction
            << " diff = " << diff;
    GLOP_RETURN_IF_ERROR(basis_factorization_.ForceRefactorization());
  } else {
    GLOP_RETURN_IF_ERROR(
        basis_factorization_.Update(entering_col, leaving_row, direction_));
  }
  if (basis_factorization_.IsRefactorized()) {
    PermuteBasis();
  }
  return Status::OK();
}

Status RevisedSimplex::RefactorizeBasisIfNeeded(bool* refactorize) {
  SCOPED_TIME_STAT(&function_stats_);
  if (*refactorize && !basis_factorization_.IsRefactorized()) {
    GLOP_RETURN_IF_ERROR(basis_factorization_.Refactorize());
    update_row_.Invalidate();
    PermuteBasis();
  }
  *refactorize = false;
  return Status::OK();
}

void RevisedSimplex::SetIntegralityScale(ColIndex col, Fractional scale) {
  if (col >= integrality_scale_.size()) {
    integrality_scale_.resize(col + 1, 0.0);
  }
  integrality_scale_[col] = scale;
}

Status RevisedSimplex::Polish(TimeLimit* time_limit) {
  GLOP_RETURN_ERROR_IF_NULL(time_limit);
  Cleanup update_deterministic_time_on_return(
      [this, time_limit]() { AdvanceDeterministicTime(time_limit); });

  // Get all non-basic variables with a reduced costs close to zero.
  // Note that because we only choose entering candidate with a cost of zero,
  // this set will not change (modulo epsilons).
  const DenseRow& rc = reduced_costs_.GetReducedCosts();
  std::vector<ColIndex> candidates;
  for (const ColIndex col : variables_info_.GetNotBasicBitRow()) {
    if (!variables_info_.GetIsRelevantBitRow()[col]) continue;
    if (std::abs(rc[col]) < 1e-9) candidates.push_back(col);
  }

  bool refactorize = false;
  int num_pivots = 0;
  Fractional total_gain = 0.0;
  for (int i = 0; i < 10; ++i) {
    AdvanceDeterministicTime(time_limit);
    if (time_limit->LimitReached()) break;
    if (num_pivots >= 5) break;
    if (candidates.empty()) break;

    // Pick a random one and remove it from the list.
    const int index =
        std::uniform_int_distribution<int>(0, candidates.size() - 1)(random_);
    const ColIndex entering_col = candidates[index];
    std::swap(candidates[index], candidates.back());
    candidates.pop_back();

    // We need the entering variable to move in the correct direction.
    Fractional fake_rc = 1.0;
    if (!variables_info_.GetCanDecreaseBitRow()[entering_col]) {
      CHECK(variables_info_.GetCanIncreaseBitRow()[entering_col]);
      fake_rc = -1.0;
    }

    // Refactorize if needed.
    if (reduced_costs_.NeedsBasisRefactorization()) refactorize = true;
    GLOP_RETURN_IF_ERROR(RefactorizeBasisIfNeeded(&refactorize));

    // Compute the direction and by how much we can move along it.
    ComputeDirection(entering_col);
    Fractional step_length;
    RowIndex leaving_row;
    Fractional target_bound;
    bool local_refactorize = false;
    GLOP_RETURN_IF_ERROR(
        ChooseLeavingVariableRow(entering_col, fake_rc, &local_refactorize,
                                 &leaving_row, &step_length, &target_bound));

    if (local_refactorize) continue;
    if (step_length == kInfinity || step_length == -kInfinity) continue;
    if (std::abs(step_length) <= 1e-6) continue;
    if (leaving_row != kInvalidRow && std::abs(direction_[leaving_row]) < 0.1) {
      continue;
    }
    const Fractional step = (fake_rc > 0.0) ? -step_length : step_length;

    // Evaluate if pivot reduce the fractionality of the basis.
    //
    // TODO(user): Count with more weight variable with a small domain, i.e.
    // binary variable, compared to a variable in [0, 1k] ?
    const auto get_diff = [this](ColIndex col, Fractional old_value,
                                 Fractional new_value) {
      if (col >= integrality_scale_.size() || integrality_scale_[col] == 0.0) {
        return 0.0;
      }
      const Fractional s = integrality_scale_[col];
      return (std::abs(new_value * s - std::round(new_value * s)) -
              std::abs(old_value * s - std::round(old_value * s)));
    };
    Fractional diff = get_diff(entering_col, variable_values_.Get(entering_col),
                               variable_values_.Get(entering_col) + step);
    for (const auto e : direction_) {
      const ColIndex col = basis_[e.row()];
      const Fractional old_value = variable_values_.Get(col);
      const Fractional new_value = old_value - e.coefficient() * step;
      diff += get_diff(col, old_value, new_value);
    }

    // Ignore low decrease in integrality.
    if (diff > -1e-2) continue;
    total_gain -= diff;

    // We perform the change.
    num_pivots++;
    variable_values_.UpdateOnPivoting(direction_, entering_col, step);

    // This is a bound flip of the entering column.
    if (leaving_row == kInvalidRow) {
      if (step > 0.0) {
        SetNonBasicVariableStatusAndDeriveValue(entering_col,
                                                VariableStatus::AT_UPPER_BOUND);
      } else if (step < 0.0) {
        SetNonBasicVariableStatusAndDeriveValue(entering_col,
                                                VariableStatus::AT_LOWER_BOUND);
      }
      continue;
    }

    // Perform the pivot.
    const ColIndex leaving_col = basis_[leaving_row];
    update_row_.ComputeUpdateRow(leaving_row);

    // Note that this will only do work if the norms are computed.
    //
    // TODO(user): We should probably move all the "update" in a function so
    // that all "iterations" function can just reuse the same code. Everything
    // that is currently not "cleared" should be updated. If one does not want
    // that, then it is easy to call Clear() on the quantities that do not needs
    // to be kept in sync with the current basis.
    primal_edge_norms_.UpdateBeforeBasisPivot(
        entering_col, leaving_col, leaving_row, direction_, &update_row_);
    dual_edge_norms_.UpdateBeforeBasisPivot(
        entering_col, leaving_row, direction_,
        update_row_.GetUnitRowLeftInverse());

    // TODO(user): Rather than maintaining this, it is probably better to
    // recompute it in one go after Polish() is done. We don't use the reduced
    // costs here as we just assume that the set of candidates does not change.
    reduced_costs_.UpdateBeforeBasisPivot(entering_col, leaving_row, direction_,
                                          &update_row_);

    const Fractional dir = -direction_[leaving_row] * step;
    const bool is_degenerate =
        (dir == 0.0) ||
        (dir > 0.0 && variable_values_.Get(leaving_col) >= target_bound) ||
        (dir < 0.0 && variable_values_.Get(leaving_col) <= target_bound);
    if (!is_degenerate) {
      variable_values_.Set(leaving_col, target_bound);
    }
    GLOP_RETURN_IF_ERROR(
        UpdateAndPivot(entering_col, leaving_row, target_bound));
  }

  VLOG(1) << "Polish num_pivots: " << num_pivots << " gain:" << total_gain;
  return Status::OK();
}

// Minimizes c.x subject to A.x = 0 where A is an mxn-matrix, c an n-vector, and
// x an n-vector.
//
// x is split in two parts x_B and x_N (B standing for basis).
// In the same way, A is split in A_B (also known as B) and A_N, and
// c is split into c_B and c_N.
//
// The goal is to minimize    c_B.x_B + c_N.x_N
//                subject to    B.x_B + A_N.x_N = 0
//                       and  x_lower <= x <= x_upper.
//
// To minimize c.x, at each iteration a variable from x_N is selected to
// enter the basis, and a variable from x_B is selected to leave the basis.
// To avoid explicit inversion of B, the algorithm solves two sub-systems:
// y.B = c_B and B.d = a (a being the entering column).
Status RevisedSimplex::PrimalMinimize(TimeLimit* time_limit) {
  GLOP_RETURN_ERROR_IF_NULL(time_limit);
  Cleanup update_deterministic_time_on_return(
      [this, time_limit]() { AdvanceDeterministicTime(time_limit); });
  num_consecutive_degenerate_iterations_ = 0;
  bool refactorize = false;

  // At this point, we are not sure the prices are always up to date, so
  // lets always reset them for the first iteration below.
  primal_prices_.ForceRecomputation();

  if (phase_ == Phase::FEASIBILITY) {
    // Initialize the primal phase-I objective.
    // Note that this temporarily erases the problem objective.
    objective_.AssignToZero(num_cols_);
    variable_values_.UpdatePrimalPhaseICosts(
        util::IntegerRange<RowIndex>(RowIndex(0), num_rows_), &objective_);
    reduced_costs_.ResetForNewObjective();
  }

  while (true) {
    // TODO(user): we may loop a bit more than the actual number of iteration.
    // fix.
    IF_STATS_ENABLED(
        ScopedTimeDistributionUpdater timer(&iteration_stats_.total));

    // Trigger a refactorization if one of the class we use request it.
    if (reduced_costs_.NeedsBasisRefactorization()) refactorize = true;
    if (primal_edge_norms_.NeedsBasisRefactorization()) refactorize = true;
    GLOP_RETURN_IF_ERROR(RefactorizeBasisIfNeeded(&refactorize));

    if (basis_factorization_.IsRefactorized()) {
      CorrectErrorsOnVariableValues();
      DisplayIterationInfo(/*primal=*/true);

      if (phase_ == Phase::FEASIBILITY) {
        // Since the variable values may have been recomputed, we need to
        // recompute the primal infeasible variables and update their costs.
        if (variable_values_.UpdatePrimalPhaseICosts(
                util::IntegerRange<RowIndex>(RowIndex(0), num_rows_),
                &objective_)) {
          reduced_costs_.ResetForNewObjective();
        }
      }

      // Computing the objective at each iteration takes time, so we just
      // check the limit when the basis is refactorized.
      if (phase_ == Phase::OPTIMIZATION &&
          ComputeObjectiveValue() < primal_objective_limit_) {
        VLOG(1) << "Stopping the primal simplex because"
                << " the objective limit " << primal_objective_limit_
                << " has been reached.";
        problem_status_ = ProblemStatus::PRIMAL_FEASIBLE;
        objective_limit_reached_ = true;
        return Status::OK();
      }
    } else if (phase_ == Phase::FEASIBILITY) {
      // Note that direction_.non_zeros contains the positions of the basic
      // variables whose values were updated during the last iteration.
      if (variable_values_.UpdatePrimalPhaseICosts(direction_.non_zeros,
                                                   &objective_)) {
        reduced_costs_.ResetForNewObjective();
      }
    }

    const ColIndex entering_col = primal_prices_.GetBestEnteringColumn();
    if (entering_col == kInvalidCol) {
      if (reduced_costs_.AreReducedCostsPrecise() &&
          basis_factorization_.IsRefactorized()) {
        if (phase_ == Phase::FEASIBILITY) {
          const Fractional primal_infeasibility =
              variable_values_.ComputeMaximumPrimalInfeasibility();
          if (primal_infeasibility <
              parameters_.primal_feasibility_tolerance()) {
            problem_status_ = ProblemStatus::PRIMAL_FEASIBLE;
          } else {
            VLOG(1) << "Infeasible problem! infeasibility = "
                    << primal_infeasibility;
            problem_status_ = ProblemStatus::PRIMAL_INFEASIBLE;
          }
        } else {
          problem_status_ = ProblemStatus::OPTIMAL;
        }
        break;
      }

      VLOG(1) << "Optimal reached, double checking...";
      reduced_costs_.MakeReducedCostsPrecise();
      refactorize = true;
      continue;
    }

    DCHECK(reduced_costs_.IsValidPrimalEnteringCandidate(entering_col));

    // Solve the system B.d = a with a the entering column.
    ComputeDirection(entering_col);

    // This might trigger a recomputation on the next iteration, but we
    // finish this one even if the price is imprecise.
    primal_edge_norms_.TestEnteringEdgeNormPrecision(entering_col, direction_);
    const Fractional reduced_cost =
        reduced_costs_.TestEnteringReducedCostPrecision(entering_col,
                                                        direction_);

    // The test might have changed the reduced cost of the entering_col.
    // If it is no longer a valid entering candidate, we loop.
    primal_prices_.RecomputePriceAt(entering_col);
    if (!reduced_costs_.IsValidPrimalEnteringCandidate(entering_col)) {
      reduced_costs_.MakeReducedCostsPrecise();
      VLOG(1) << "Skipping col #" << entering_col
              << " whose reduced cost is no longer valid under precise reduced "
                 "cost: "
              << reduced_cost;
      continue;
    }

    // This test takes place after the check for optimality/feasibility because
    // when running with 0 iterations, we still want to report
    // ProblemStatus::OPTIMAL or ProblemStatus::PRIMAL_FEASIBLE if it is the
    // case at the beginning of the algorithm.
    AdvanceDeterministicTime(time_limit);
    if (num_iterations_ == parameters_.max_number_of_iterations() ||
        time_limit->LimitReached()) {
      break;
    }

    Fractional step_length;
    RowIndex leaving_row;
    Fractional target_bound;
    if (phase_ == Phase::FEASIBILITY) {
      PrimalPhaseIChooseLeavingVariableRow(entering_col, reduced_cost,
                                           &refactorize, &leaving_row,
                                           &step_length, &target_bound);
    } else {
      GLOP_RETURN_IF_ERROR(
          ChooseLeavingVariableRow(entering_col, reduced_cost, &refactorize,
                                   &leaving_row, &step_length, &target_bound));
    }
    if (refactorize) continue;

    if (step_length == kInfinity || step_length == -kInfinity) {
      if (!basis_factorization_.IsRefactorized() ||
          !reduced_costs_.AreReducedCostsPrecise()) {
        VLOG(1) << "Infinite step length, double checking...";
        reduced_costs_.MakeReducedCostsPrecise();
        continue;
      }
      if (phase_ == Phase::FEASIBILITY) {
        // This shouldn't happen by construction.
        VLOG(1) << "Unbounded feasibility problem !?";
        problem_status_ = ProblemStatus::ABNORMAL;
      } else {
        VLOG(1) << "Unbounded problem.";
        problem_status_ = ProblemStatus::PRIMAL_UNBOUNDED;
        solution_primal_ray_.AssignToZero(num_cols_);
        for (RowIndex row(0); row < num_rows_; ++row) {
          const ColIndex col = basis_[row];
          solution_primal_ray_[col] = -direction_[row];
        }
        solution_primal_ray_[entering_col] = 1.0;
        if (step_length == -kInfinity) {
          ChangeSign(&solution_primal_ray_);
        }
      }
      break;
    }

    Fractional step = (reduced_cost > 0.0) ? -step_length : step_length;
    if (phase_ == Phase::FEASIBILITY && leaving_row != kInvalidRow) {
      // For phase-I we currently always set the leaving variable to its exact
      // bound even if by doing so we may take a small step in the wrong
      // direction and may increase the overall infeasibility.
      //
      // TODO(user): Investigate alternatives even if this seems to work well in
      // practice. Note that the final returned solution will have the property
      // that all non-basic variables are at their exact bound, so it is nice
      // that we do not report ProblemStatus::PRIMAL_FEASIBLE if a solution with
      // this property cannot be found.
      step = ComputeStepToMoveBasicVariableToBound(leaving_row, target_bound);
    }

    // Store the leaving_col before basis_ change.
    const ColIndex leaving_col =
        (leaving_row == kInvalidRow) ? kInvalidCol : basis_[leaving_row];

    // An iteration is called 'degenerate' if the leaving variable is already
    // primal-infeasible and we make it even more infeasible or if we do a zero
    // step.
    bool is_degenerate = false;
    if (leaving_row != kInvalidRow) {
      Fractional dir = -direction_[leaving_row] * step;
      is_degenerate =
          (dir == 0.0) ||
          (dir > 0.0 && variable_values_.Get(leaving_col) >= target_bound) ||
          (dir < 0.0 && variable_values_.Get(leaving_col) <= target_bound);

      // If the iteration is not degenerate, the leaving variable should go to
      // its exact target bound (it is how the step is computed).
      if (!is_degenerate) {
        DCHECK_EQ(step, ComputeStepToMoveBasicVariableToBound(leaving_row,
                                                              target_bound));
      }
    }

    variable_values_.UpdateOnPivoting(direction_, entering_col, step);
    if (leaving_row != kInvalidRow) {
      // Important: the norm must be updated before the reduced_cost.
      primal_edge_norms_.UpdateBeforeBasisPivot(
          entering_col, basis_[leaving_row], leaving_row, direction_,
          &update_row_);
      reduced_costs_.UpdateBeforeBasisPivot(entering_col, leaving_row,
                                            direction_, &update_row_);
      primal_prices_.UpdateBeforeBasisPivot(entering_col, &update_row_);
      if (!is_degenerate) {
        // On a non-degenerate iteration, the leaving variable should be at its
        // exact bound. This corrects an eventual small numerical error since
        // 'value + direction * step' where step is
        // '(target_bound - value) / direction'
        // may be slighlty different from target_bound.
        variable_values_.Set(leaving_col, target_bound);
      }
      GLOP_RETURN_IF_ERROR(
          UpdateAndPivot(entering_col, leaving_row, target_bound));
      IF_STATS_ENABLED({
        if (is_degenerate) {
          timer.AlsoUpdate(&iteration_stats_.degenerate);
        } else {
          timer.AlsoUpdate(&iteration_stats_.normal);
        }
      });
    } else {
      // Bound flip. This makes sure that the flipping variable is at its bound
      // and has the correct status.
      DCHECK_EQ(VariableType::UPPER_AND_LOWER_BOUNDED,
                variables_info_.GetTypeRow()[entering_col]);
      if (step > 0.0) {
        SetNonBasicVariableStatusAndDeriveValue(entering_col,
                                                VariableStatus::AT_UPPER_BOUND);
      } else if (step < 0.0) {
        SetNonBasicVariableStatusAndDeriveValue(entering_col,
                                                VariableStatus::AT_LOWER_BOUND);
      }
      primal_prices_.SetAndDebugCheckThatColumnIsDualFeasible(entering_col);
      IF_STATS_ENABLED(timer.AlsoUpdate(&iteration_stats_.bound_flip));
    }

    if (phase_ == Phase::FEASIBILITY && leaving_row != kInvalidRow) {
      // Set the leaving variable to its exact bound.
      variable_values_.SetNonBasicVariableValueFromStatus(leaving_col);

      // Change the objective value of the leaving variable to zero.
      reduced_costs_.SetNonBasicVariableCostToZero(leaving_col,
                                                   &objective_[leaving_col]);
      primal_prices_.RecomputePriceAt(leaving_col);
    }

    // Stats about consecutive degenerate iterations.
    if (step_length == 0.0) {
      num_consecutive_degenerate_iterations_++;
    } else {
      if (num_consecutive_degenerate_iterations_ > 0) {
        iteration_stats_.degenerate_run_size.Add(
            num_consecutive_degenerate_iterations_);
        num_consecutive_degenerate_iterations_ = 0;
      }
    }
    ++num_iterations_;
  }
  if (num_consecutive_degenerate_iterations_ > 0) {
    iteration_stats_.degenerate_run_size.Add(
        num_consecutive_degenerate_iterations_);
  }
  return Status::OK();
}

// TODO(user): Two other approaches for the phase I described in Koberstein's
// PhD thesis seem worth trying at some point:
// - The subproblem approach, which enables one to use a normal phase II dual,
//   but requires an efficient bound-flipping ratio test since the new problem
//   has all its variables boxed. This one is implemented now, but require
//   a bit more tunning.
// - Pan's method, which is really fast but have no theoretical guarantee of
//   terminating and thus needs to use one of the other methods as a fallback if
//   it fails to make progress.
//
// Note that the returned status applies to the primal problem!
Status RevisedSimplex::DualMinimize(bool feasibility_phase,
                                    TimeLimit* time_limit) {
  Cleanup update_deterministic_time_on_return(
      [this, time_limit]() { AdvanceDeterministicTime(time_limit); });
  num_consecutive_degenerate_iterations_ = 0;
  bool refactorize = false;

  bound_flip_candidates_.clear();

  // Leaving variable.
  RowIndex leaving_row;
  Fractional cost_variation;
  Fractional target_bound;

  // Entering variable.
  ColIndex entering_col;

  while (true) {
    // TODO(user): we may loop a bit more than the actual number of iteration.
    // fix.
    IF_STATS_ENABLED(
        ScopedTimeDistributionUpdater timer(&iteration_stats_.total));

    // Trigger a refactorization if one of the class we use request it.
    const bool old_refactorize_value = refactorize;
    if (reduced_costs_.NeedsBasisRefactorization()) refactorize = true;
    if (dual_edge_norms_.NeedsBasisRefactorization()) refactorize = true;
    GLOP_RETURN_IF_ERROR(RefactorizeBasisIfNeeded(&refactorize));

    // If the basis is refactorized, we recompute all the values in order to
    // have a good precision.
    if (basis_factorization_.IsRefactorized()) {
      // We do not want to recompute the reduced costs too often, this is
      // because that may break the overall direction taken by the last steps
      // and may lead to less improvement on degenerate problems.
      //
      // For now, we just recompute them if refactorize was set during the
      // loop and not because of normal refactorization.
      //
      // During phase-I, we do want the reduced costs to be as precise as
      // possible. TODO(user): Investigate why and fix the TODO in
      // PermuteBasis().
      //
      // Reduced costs are needed by MakeBoxedVariableDualFeasible(), so if we
      // do recompute them, it is better to do that first.
      if (feasibility_phase || old_refactorize_value) {
        reduced_costs_.MakeReducedCostsPrecise();
      }

      // TODO(user): Make RecomputeBasicVariableValues() do nothing
      // if it was already recomputed on a refactorized basis. This is the
      // same behavior as MakeReducedCostsPrecise().
      //
      // TODO(user): Do not recompute the variable values each time we
      // refactorize the matrix, like for the reduced costs? That may lead to
      // a worse behavior than keeping the "imprecise" version and only
      // recomputing it when its precision is above a threshold.
      if (!feasibility_phase) {
        MakeBoxedVariableDualFeasible(
            variables_info_.GetNonBasicBoxedVariables(),
            /*update_basic_values=*/false);
        variable_values_.RecomputeBasicVariableValues();
        variable_values_.RecomputeDualPrices();

        // Computing the objective at each iteration takes time, so we just
        // check the limit when the basis is refactorized.
        //
        // Hack: We need phase_ here and not the local feasibility_phase
        // variable because this must not be checked for the dual phase I algo
        // that use the same code as the dual phase II (i.e. the local
        // feasibility_phase will be false).
        if (phase_ == Phase::OPTIMIZATION &&
            dual_objective_limit_ != kInfinity &&
            ComputeObjectiveValue() > dual_objective_limit_) {
          SOLVER_LOG(logger_,
                     "Stopping the dual simplex because"
                     " the objective limit ",
                     dual_objective_limit_, " has been reached.");
          problem_status_ = ProblemStatus::DUAL_FEASIBLE;
          objective_limit_reached_ = true;
          return Status::OK();
        }
      }

      DisplayIterationInfo(/*primal=*/false);
    } else {
      // Updates from the previous iteration that can be skipped if we
      // recompute everything (see other case above).
      if (!feasibility_phase) {
        // Make sure the boxed variables are dual-feasible before choosing the
        // leaving variable row.
        MakeBoxedVariableDualFeasible(bound_flip_candidates_,
                                      /*update_basic_values=*/true);
        bound_flip_candidates_.clear();

        // The direction_.non_zeros contains the positions for which the basic
        // variable value was changed during the previous iterations.
        variable_values_.UpdateDualPrices(direction_.non_zeros);
      }
    }

    if (feasibility_phase) {
      GLOP_RETURN_IF_ERROR(DualPhaseIChooseLeavingVariableRow(
          &leaving_row, &cost_variation, &target_bound));
    } else {
      GLOP_RETURN_IF_ERROR(DualChooseLeavingVariableRow(
          &leaving_row, &cost_variation, &target_bound));
    }
    if (leaving_row == kInvalidRow) {
      // TODO(user): integrate this with the main "re-optimization" loop.
      // Also distinguish cost perturbation and shifts?
      if (!basis_factorization_.IsRefactorized() ||
          reduced_costs_.HasCostShift()) {
        VLOG(1) << "Optimal reached, double checking.";
        reduced_costs_.ClearAndRemoveCostShifts();
        IF_STATS_ENABLED(timer.AlsoUpdate(&iteration_stats_.refactorize));
        refactorize = true;
        continue;
      }
      if (feasibility_phase) {
        // Note that since the basis is refactorized, the variable values
        // will be recomputed at the beginning of the second phase. The boxed
        // variable values will also be corrected by
        // MakeBoxedVariableDualFeasible().
        if (num_dual_infeasible_positions_ == 0) {
          problem_status_ = ProblemStatus::DUAL_FEASIBLE;
        } else {
          VLOG(1) << "DUAL infeasible in dual phase I.";
          problem_status_ = ProblemStatus::DUAL_INFEASIBLE;
        }
      } else {
        problem_status_ = ProblemStatus::OPTIMAL;
      }
      IF_STATS_ENABLED(timer.AlsoUpdate(&iteration_stats_.normal));
      return Status::OK();
    }

    update_row_.ComputeUpdateRow(leaving_row);
    if (feasibility_phase) {
      GLOP_RETURN_IF_ERROR(entering_variable_.DualPhaseIChooseEnteringColumn(
          reduced_costs_.AreReducedCostsPrecise(), update_row_, cost_variation,
          &entering_col));
    } else {
      GLOP_RETURN_IF_ERROR(entering_variable_.DualChooseEnteringColumn(
          reduced_costs_.AreReducedCostsPrecise(), update_row_, cost_variation,
          &bound_flip_candidates_, &entering_col));
    }

    // No entering_col: dual unbounded (i.e. primal infeasible).
    if (entering_col == kInvalidCol) {
      if (!reduced_costs_.AreReducedCostsPrecise()) {
        VLOG(1) << "No entering column. Double checking...";
        IF_STATS_ENABLED(timer.AlsoUpdate(&iteration_stats_.refactorize));
        refactorize = true;
        continue;
      }
      DCHECK(basis_factorization_.IsRefactorized());
      if (feasibility_phase) {
        // This shouldn't happen by construction.
        VLOG(1) << "Unbounded dual feasibility problem !?";
        problem_status_ = ProblemStatus::ABNORMAL;
      } else {
        problem_status_ = ProblemStatus::DUAL_UNBOUNDED;
        solution_dual_ray_ =
            Transpose(update_row_.GetUnitRowLeftInverse().values);
        update_row_.RecomputeFullUpdateRow(leaving_row);
        solution_dual_ray_row_combination_.AssignToZero(num_cols_);
        for (const ColIndex col : update_row_.GetNonZeroPositions()) {
          solution_dual_ray_row_combination_[col] =
              update_row_.GetCoefficient(col);
        }
        if (cost_variation < 0) {
          ChangeSign(&solution_dual_ray_);
          ChangeSign(&solution_dual_ray_row_combination_);
        }
      }
      IF_STATS_ENABLED(timer.AlsoUpdate(&iteration_stats_.normal));
      return Status::OK();
    }

    // If the coefficient is too small, we recompute the reduced costs if not
    // already done. This is an extra heuristic to avoid computing the direction
    // If the pivot is small. But the real recomputation step is just below.
    const Fractional entering_coeff = update_row_.GetCoefficient(entering_col);
    if (std::abs(entering_coeff) < parameters_.dual_small_pivot_threshold() &&
        !reduced_costs_.AreReducedCostsPrecise()) {
      VLOG(1) << "Trying not to pivot by " << entering_coeff;
      IF_STATS_ENABLED(timer.AlsoUpdate(&iteration_stats_.refactorize));
      refactorize = true;
      continue;
    }

    ComputeDirection(entering_col);

    // If the pivot is small compared to others in the direction_ vector we try
    // to recompute everything. If we cannot, then note that
    // DualChooseEnteringColumn() should guaranteed that the pivot is not too
    // small when everything has already been recomputed.
    if (std::abs(direction_[leaving_row]) <
        parameters_.small_pivot_threshold() * direction_infinity_norm_) {
      if (!reduced_costs_.AreReducedCostsPrecise()) {
        VLOG(1) << "Trying not pivot by " << entering_coeff << " ("
                << direction_[leaving_row]
                << ") because the direction has a norm of "
                << direction_infinity_norm_;
        IF_STATS_ENABLED(timer.AlsoUpdate(&iteration_stats_.refactorize));
        refactorize = true;
        continue;
      }
    }

    // This test takes place after the check for optimality/feasibility because
    // when running with 0 iterations, we still want to report
    // ProblemStatus::OPTIMAL or ProblemStatus::PRIMAL_FEASIBLE if it is the
    // case at the beginning of the algorithm.
    AdvanceDeterministicTime(time_limit);
    if (num_iterations_ == parameters_.max_number_of_iterations() ||
        time_limit->LimitReached()) {
      IF_STATS_ENABLED(timer.AlsoUpdate(&iteration_stats_.normal));
      return Status::OK();
    }

    // Before we update the reduced costs, if its sign is already dual
    // infeasible and the update direction will make it worse we make sure the
    // reduced cost is 0.0 so UpdateReducedCosts() will not take a step that
    // goes in the wrong direction (a few experiments seems to indicate that
    // this is not a good idea). See comment at the top of UpdateReducedCosts().
    //
    // Note that ShiftCostIfNeeded() actually shifts the cost a bit more in
    // order to do a non-zero step. This helps on degenerate problems. Like the
    // pertubation, we will remove all these shifts at the end.
    const bool increasing_rc_is_needed =
        (cost_variation > 0.0) == (entering_coeff > 0.0);
    reduced_costs_.ShiftCostIfNeeded(increasing_rc_is_needed, entering_col);

    IF_STATS_ENABLED({
      if (reduced_costs_.StepIsDualDegenerate(increasing_rc_is_needed,
                                              entering_col)) {
        timer.AlsoUpdate(&iteration_stats_.degenerate);
      } else {
        timer.AlsoUpdate(&iteration_stats_.normal);
      }
    });

    // Update basis. Note that direction_ is already computed.
    //
    // TODO(user): this is pretty much the same in the primal or dual code.
    // We just need to know to what bound the leaving variable will be set to.
    // Factorize more common code?
    reduced_costs_.UpdateBeforeBasisPivot(entering_col, leaving_row, direction_,
                                          &update_row_);
    dual_edge_norms_.UpdateBeforeBasisPivot(
        entering_col, leaving_row, direction_,
        update_row_.GetUnitRowLeftInverse());

    // During phase I, we do not need the basic variable values at all.
    // Important: The norm should be updated before that.
    Fractional primal_step = 0.0;
    if (feasibility_phase) {
      DualPhaseIUpdatePrice(leaving_row, entering_col);
    } else {
      primal_step =
          ComputeStepToMoveBasicVariableToBound(leaving_row, target_bound);
      variable_values_.UpdateOnPivoting(direction_, entering_col, primal_step);
    }

    // It is important to do the actual pivot after the update above!
    const ColIndex leaving_col = basis_[leaving_row];
    GLOP_RETURN_IF_ERROR(
        UpdateAndPivot(entering_col, leaving_row, target_bound));

    // This makes sure the leaving variable is at its exact bound. Tests
    // indicate that this makes everything more stable. Note also that during
    // the feasibility phase, the variable values are not used, but that the
    // correct non-basic variable value are needed at the end.
    variable_values_.SetNonBasicVariableValueFromStatus(leaving_col);

    // This is slow, but otherwise we have a really bad precision on the
    // variable values ...
    if (std::abs(primal_step) * parameters_.primal_feasibility_tolerance() >
        1.0) {
      refactorize = true;
    }
    ++num_iterations_;
  }
  return Status::OK();
}

Status RevisedSimplex::PrimalPush(TimeLimit* time_limit) {
  GLOP_RETURN_ERROR_IF_NULL(time_limit);
  Cleanup update_deterministic_time_on_return(
      [this, time_limit]() { AdvanceDeterministicTime(time_limit); });
  bool refactorize = false;

  // We clear all the quantities that we don't update so they will be recomputed
  // later if needed.
  primal_edge_norms_.Clear();
  dual_edge_norms_.Clear();
  update_row_.Invalidate();
  reduced_costs_.ClearAndRemoveCostShifts();

  std::vector<ColIndex> super_basic_cols;
  for (const ColIndex col : variables_info_.GetNotBasicBitRow()) {
    if (variables_info_.GetStatusRow()[col] == VariableStatus::FREE &&
        variable_values_.Get(col) != 0) {
      super_basic_cols.push_back(col);
    }
  }

  while (!super_basic_cols.empty()) {
    AdvanceDeterministicTime(time_limit);
    if (time_limit->LimitReached()) break;

    IF_STATS_ENABLED(
        ScopedTimeDistributionUpdater timer(&iteration_stats_.total));
    GLOP_RETURN_IF_ERROR(RefactorizeBasisIfNeeded(&refactorize));
    if (basis_factorization_.IsRefactorized()) {
      CorrectErrorsOnVariableValues();
      DisplayIterationInfo(/*primal=*/true);
    }

    // TODO(user): Select at random like in Polish().
    ColIndex entering_col = super_basic_cols.back();

    DCHECK(variables_info_.GetCanDecreaseBitRow()[entering_col]);
    DCHECK(variables_info_.GetCanIncreaseBitRow()[entering_col]);

    // Decide which direction to send the entering column.
    // UNCONSTRAINED variables go towards zero. Other variables go towards their
    // closest bound. We assume that we're at an optimal solution, so all FREE
    // variables have approximately zero reduced cost, which means that the
    // objective value won't change from moving this column into the basis.
    // TODO(user): As an improvement for variables with two bounds, try both
    // and pick one that doesn't require a basis change (if possible), otherwise
    // pick the closer bound.
    Fractional fake_rc;
    const Fractional entering_value = variable_values_.Get(entering_col);
    if (variables_info_.GetTypeRow()[entering_col] ==
        VariableType::UNCONSTRAINED) {
      if (entering_value > 0) {
        fake_rc = 1.0;
      } else {
        fake_rc = -1.0;
      }
    } else {
      const Fractional diff_ub =
          variables_info_.GetVariableUpperBounds()[entering_col] -
          entering_value;
      const Fractional diff_lb =
          entering_value -
          variables_info_.GetVariableLowerBounds()[entering_col];
      if (diff_lb <= diff_ub) {
        fake_rc = 1.0;
      } else {
        fake_rc = -1.0;
      }
    }

    // Solve the system B.d = a with a the entering column.
    ComputeDirection(entering_col);

    Fractional step_length;
    RowIndex leaving_row;
    Fractional target_bound;

    GLOP_RETURN_IF_ERROR(ChooseLeavingVariableRow(entering_col, fake_rc,
                                                  &refactorize, &leaving_row,
                                                  &step_length, &target_bound));

    if (refactorize) continue;

    // At this point, we know the iteration will finish or stop with an error.
    super_basic_cols.pop_back();

    if (step_length == kInfinity || step_length == -kInfinity) {
      if (variables_info_.GetTypeRow()[entering_col] ==
          VariableType::UNCONSTRAINED) {
        step_length = std::fabs(entering_value);
      } else {
        VLOG(1) << "Infinite step for bounded variable ?!";
        problem_status_ = ProblemStatus::ABNORMAL;
        break;
      }
    }

    const Fractional step = (fake_rc > 0.0) ? -step_length : step_length;

    // Store the leaving_col before basis_ change.
    const ColIndex leaving_col =
        (leaving_row == kInvalidRow) ? kInvalidCol : basis_[leaving_row];

    // An iteration is called 'degenerate' if the leaving variable is already
    // primal-infeasible and we make it even more infeasible or if we do a zero
    // step.
    // TODO(user): Test setting the step size to zero for degenerate steps.
    // We don't need to force a positive step because each super-basic variable
    // is pivoted in exactly once.
    bool is_degenerate = false;
    if (leaving_row != kInvalidRow) {
      Fractional dir = -direction_[leaving_row] * step;
      is_degenerate =
          (dir == 0.0) ||
          (dir > 0.0 && variable_values_.Get(leaving_col) >= target_bound) ||
          (dir < 0.0 && variable_values_.Get(leaving_col) <= target_bound);

      // If the iteration is not degenerate, the leaving variable should go to
      // its exact target bound (it is how the step is computed).
      if (!is_degenerate) {
        DCHECK_EQ(step, ComputeStepToMoveBasicVariableToBound(leaving_row,
                                                              target_bound));
      }
    }

    variable_values_.UpdateOnPivoting(direction_, entering_col, step);
    if (leaving_row != kInvalidRow) {
      if (!is_degenerate) {
        // On a non-degenerate iteration, the leaving variable should be at its
        // exact bound. This corrects an eventual small numerical error since
        // 'value + direction * step' where step is
        // '(target_bound - value) / direction'
        // may be slighlty different from target_bound.
        variable_values_.Set(leaving_col, target_bound);
      }
      GLOP_RETURN_IF_ERROR(
          UpdateAndPivot(entering_col, leaving_row, target_bound));
      IF_STATS_ENABLED({
        if (is_degenerate) {
          timer.AlsoUpdate(&iteration_stats_.degenerate);
        } else {
          timer.AlsoUpdate(&iteration_stats_.normal);
        }
      });
    } else {
      // Snap the super-basic variable to its bound. Note that
      // variable_values_.UpdateOnPivoting() should already be close to that but
      // here we make sure it is exact and remove any small numerical errors.
      if (variables_info_.GetTypeRow()[entering_col] ==
          VariableType::UNCONSTRAINED) {
        variable_values_.Set(entering_col, 0.0);
      } else if (step > 0.0) {
        SetNonBasicVariableStatusAndDeriveValue(entering_col,
                                                VariableStatus::AT_UPPER_BOUND);
      } else if (step < 0.0) {
        SetNonBasicVariableStatusAndDeriveValue(entering_col,
                                                VariableStatus::AT_LOWER_BOUND);
      }
      IF_STATS_ENABLED(timer.AlsoUpdate(&iteration_stats_.bound_flip));
    }

    ++num_iterations_;
  }

  if (!super_basic_cols.empty() > 0) {
    SOLVER_LOG(logger_, "Push terminated early with ", super_basic_cols.size(),
               " super-basic variables remaining.");
  }

  // TODO(user): What status should be returned if the time limit is hit?
  // If the optimization phase finished, then OPTIMAL is technically correct
  // but also misleading.

  return Status::OK();
}

ColIndex RevisedSimplex::SlackColIndex(RowIndex row) const {
  DCHECK_ROW_BOUNDS(row);
  return first_slack_col_ + RowToColIndex(row);
}

std::string RevisedSimplex::StatString() {
  std::string result;
  result.append(iteration_stats_.StatString());
  result.append(ratio_test_stats_.StatString());
  result.append(entering_variable_.StatString());
  result.append(dual_prices_.StatString());
  result.append(reduced_costs_.StatString());
  result.append(variable_values_.StatString());
  result.append(primal_edge_norms_.StatString());
  result.append(dual_edge_norms_.StatString());
  result.append(update_row_.StatString());
  result.append(basis_factorization_.StatString());
  result.append(function_stats_.StatString());
  return result;
}

void RevisedSimplex::DisplayAllStats() {
  if (absl::GetFlag(FLAGS_simplex_display_stats)) {
    absl::FPrintF(stderr, "%s", StatString());
    absl::FPrintF(stderr, "%s", GetPrettySolverStats());
  }
}

Fractional RevisedSimplex::ComputeObjectiveValue() const {
  SCOPED_TIME_STAT(&function_stats_);
  return PreciseScalarProduct(objective_,
                              Transpose(variable_values_.GetDenseRow()));
}

Fractional RevisedSimplex::ComputeInitialProblemObjectiveValue() const {
  SCOPED_TIME_STAT(&function_stats_);
  const Fractional sum = PreciseScalarProduct(
      objective_, Transpose(variable_values_.GetDenseRow()));
  return objective_scaling_factor_ * (sum + objective_offset_);
}

void RevisedSimplex::SetParameters(const GlopParameters& parameters) {
  SCOPED_TIME_STAT(&function_stats_);
  deterministic_random_.seed(parameters.random_seed());

  initial_parameters_ = parameters;
  parameters_ = parameters;
  PropagateParameters();
}

void RevisedSimplex::PropagateParameters() {
  SCOPED_TIME_STAT(&function_stats_);
  basis_factorization_.SetParameters(parameters_);
  entering_variable_.SetParameters(parameters_);
  reduced_costs_.SetParameters(parameters_);
  dual_edge_norms_.SetParameters(parameters_);
  primal_edge_norms_.SetParameters(parameters_);
  update_row_.SetParameters(parameters_);
}

void RevisedSimplex::DisplayIterationInfo(bool primal) {
  if (!logger_->LoggingIsEnabled()) return;
  const std::string first_word = primal ? "Primal " : "Dual ";

  switch (phase_) {
    case Phase::FEASIBILITY: {
      const int64_t iter = num_iterations_;
      std::string name;
      Fractional objective;
      if (parameters_.use_dual_simplex()) {
        if (parameters_.use_dedicated_dual_feasibility_algorithm()) {
          objective = reduced_costs_.ComputeSumOfDualInfeasibilities();
        } else {
          // The internal objective of the transformed problem is the negation
          // of the sum of the dual infeasibility of the original problem.
          objective = -PreciseScalarProduct(
              objective_, Transpose(variable_values_.GetDenseRow()));
        }
        name = "sum_dual_infeasibilities";
      } else {
        objective = variable_values_.ComputeSumOfPrimalInfeasibilities();
        name = "sum_primal_infeasibilities";
      }

      SOLVER_LOG(logger_, first_word, "feasibility phase, iteration # ", iter,
                 ", ", name, " = ", absl::StrFormat("%.15E", objective));
      break;
    }
    case Phase::OPTIMIZATION: {
      const int64_t iter = num_iterations_ - num_feasibility_iterations_;
      // Note that in the dual phase II, ComputeObjectiveValue() is also
      // computing the dual objective even if it uses the variable values.
      // This is because if we modify the bounds to make the problem
      // primal-feasible, we are at the optimal and hence the two objectives
      // are the same.
      const Fractional objective = ComputeInitialProblemObjectiveValue();
      SOLVER_LOG(logger_, first_word, "optimization phase, iteration # ", iter,
                 ", objective = ", absl::StrFormat("%.15E", objective));
      break;
    }
    case Phase::PUSH: {
      const int64_t iter = num_iterations_ - num_feasibility_iterations_ -
                           num_optimization_iterations_;
      SOLVER_LOG(logger_, first_word, "push phase, iteration # ", iter,
                 ", remaining_variables_to_push = ",
                 ComputeNumberOfSuperBasicVariables());
    }
  }
}

void RevisedSimplex::DisplayErrors() {
  if (!logger_->LoggingIsEnabled()) return;
  SOLVER_LOG(logger_,
             "Current status: ", GetProblemStatusString(problem_status_));
  SOLVER_LOG(logger_, "Primal infeasibility (bounds) = ",
             variable_values_.ComputeMaximumPrimalInfeasibility());
  SOLVER_LOG(logger_, "Primal residual |A.x - b| = ",
             variable_values_.ComputeMaximumPrimalResidual());
  SOLVER_LOG(logger_, "Dual infeasibility (reduced costs) = ",
             reduced_costs_.ComputeMaximumDualInfeasibility());
  SOLVER_LOG(logger_, "Dual residual |c_B - y.B| = ",
             reduced_costs_.ComputeMaximumDualResidual());
}

namespace {

std::string StringifyMonomialWithFlags(const Fractional a,
                                       const std::string& x) {
  return StringifyMonomial(
      a, x, absl::GetFlag(FLAGS_simplex_display_numbers_as_fractions));
}

// Returns a string representing the rational approximation of x or a decimal
// approximation of x according to
// absl::GetFlag(FLAGS_simplex_display_numbers_as_fractions).
std::string StringifyWithFlags(const Fractional x) {
  return Stringify(x,
                   absl::GetFlag(FLAGS_simplex_display_numbers_as_fractions));
}

}  // namespace

std::string RevisedSimplex::SimpleVariableInfo(ColIndex col) const {
  std::string output;
  VariableType variable_type = variables_info_.GetTypeRow()[col];
  VariableStatus variable_status = variables_info_.GetStatusRow()[col];
  const DenseRow& lower_bounds = variables_info_.GetVariableLowerBounds();
  const DenseRow& upper_bounds = variables_info_.GetVariableUpperBounds();
  absl::StrAppendFormat(&output, "%d (%s) = %s, %s, %s, [%s,%s]", col.value(),
                        variable_name_[col],
                        StringifyWithFlags(variable_values_.Get(col)),
                        GetVariableStatusString(variable_status),
                        GetVariableTypeString(variable_type),
                        StringifyWithFlags(lower_bounds[col]),
                        StringifyWithFlags(upper_bounds[col]));
  return output;
}

void RevisedSimplex::DisplayInfoOnVariables() const {
  if (VLOG_IS_ON(3)) {
    for (ColIndex col(0); col < num_cols_; ++col) {
      const Fractional variable_value = variable_values_.Get(col);
      const Fractional objective_coefficient = objective_[col];
      const Fractional objective_contribution =
          objective_coefficient * variable_value;
      VLOG(3) << SimpleVariableInfo(col) << ". " << variable_name_[col] << " = "
              << StringifyWithFlags(variable_value) << " * "
              << StringifyWithFlags(objective_coefficient)
              << "(obj) = " << StringifyWithFlags(objective_contribution);
    }
    VLOG(3) << "------";
  }
}

void RevisedSimplex::DisplayVariableBounds() {
  if (VLOG_IS_ON(3)) {
    const VariableTypeRow& variable_type = variables_info_.GetTypeRow();
    const DenseRow& lower_bounds = variables_info_.GetVariableLowerBounds();
    const DenseRow& upper_bounds = variables_info_.GetVariableUpperBounds();
    for (ColIndex col(0); col < num_cols_; ++col) {
      switch (variable_type[col]) {
        case VariableType::UNCONSTRAINED:
          break;
        case VariableType::LOWER_BOUNDED:
          VLOG(3) << variable_name_[col]
                  << " >= " << StringifyWithFlags(lower_bounds[col]) << ";";
          break;
        case VariableType::UPPER_BOUNDED:
          VLOG(3) << variable_name_[col]
                  << " <= " << StringifyWithFlags(upper_bounds[col]) << ";";
          break;
        case VariableType::UPPER_AND_LOWER_BOUNDED:
          VLOG(3) << StringifyWithFlags(lower_bounds[col])
                  << " <= " << variable_name_[col]
                  << " <= " << StringifyWithFlags(upper_bounds[col]) << ";";
          break;
        case VariableType::FIXED_VARIABLE:
          VLOG(3) << variable_name_[col] << " = "
                  << StringifyWithFlags(lower_bounds[col]) << ";";
          break;
        default:  // This should never happen.
          LOG(DFATAL) << "Column " << col << " has no meaningful status.";
          break;
      }
    }
  }
}

absl::StrongVector<RowIndex, SparseRow> RevisedSimplex::ComputeDictionary(
    const DenseRow* column_scales) {
  absl::StrongVector<RowIndex, SparseRow> dictionary(num_rows_.value());
  for (ColIndex col(0); col < num_cols_; ++col) {
    ComputeDirection(col);
    for (const auto e : direction_) {
      if (column_scales == nullptr) {
        dictionary[e.row()].SetCoefficient(col, e.coefficient());
        continue;
      }
      const Fractional numerator =
          col < column_scales->size() ? (*column_scales)[col] : 1.0;
      const Fractional denominator = GetBasis(e.row()) < column_scales->size()
                                         ? (*column_scales)[GetBasis(e.row())]
                                         : 1.0;
      dictionary[e.row()].SetCoefficient(
          col, direction_[e.row()] * (numerator / denominator));
    }
  }
  return dictionary;
}

void RevisedSimplex::ComputeBasicVariablesForState(
    const LinearProgram& linear_program, const BasisState& state) {
  LoadStateForNextSolve(state);
  Status status = Initialize(linear_program);
  if (status.ok()) {
    variable_values_.RecomputeBasicVariableValues();
    solution_objective_value_ = ComputeInitialProblemObjectiveValue();
  }
}

void RevisedSimplex::DisplayRevisedSimplexDebugInfo() {
  if (VLOG_IS_ON(3)) {
    // This function has a complexity in O(num_non_zeros_in_matrix).
    DisplayInfoOnVariables();

    std::string output = "z = " + StringifyWithFlags(ComputeObjectiveValue());
    const DenseRow& reduced_costs = reduced_costs_.GetReducedCosts();
    for (const ColIndex col : variables_info_.GetNotBasicBitRow()) {
      absl::StrAppend(&output, StringifyMonomialWithFlags(reduced_costs[col],
                                                          variable_name_[col]));
    }
    VLOG(3) << output << ";";

    const RevisedSimplexDictionary dictionary(nullptr, this);
    RowIndex r(0);
    for (const SparseRow& row : dictionary) {
      output.clear();
      ColIndex basic_col = basis_[r];
      absl::StrAppend(&output, variable_name_[basic_col], " = ",
                      StringifyWithFlags(variable_values_.Get(basic_col)));
      for (const SparseRowEntry e : row) {
        if (e.col() != basic_col) {
          absl::StrAppend(&output,
                          StringifyMonomialWithFlags(e.coefficient(),
                                                     variable_name_[e.col()]));
        }
      }
      VLOG(3) << output << ";";
    }
    VLOG(3) << "------";
    DisplayVariableBounds();
    ++r;
  }
}

void RevisedSimplex::DisplayProblem() const {
  // This function has a complexity in O(num_rows * num_cols *
  // num_non_zeros_in_row).
  if (VLOG_IS_ON(3)) {
    DisplayInfoOnVariables();
    std::string output = "min: ";
    bool has_objective = false;
    for (ColIndex col(0); col < num_cols_; ++col) {
      const Fractional coeff = objective_[col];
      has_objective |= (coeff != 0.0);
      absl::StrAppend(&output,
                      StringifyMonomialWithFlags(coeff, variable_name_[col]));
    }
    if (!has_objective) {
      absl::StrAppend(&output, " 0");
    }
    VLOG(3) << output << ";";
    for (RowIndex row(0); row < num_rows_; ++row) {
      output = "";
      for (ColIndex col(0); col < num_cols_; ++col) {
        absl::StrAppend(&output,
                        StringifyMonomialWithFlags(
                            compact_matrix_.column(col).LookUpCoefficient(row),
                            variable_name_[col]));
      }
      VLOG(3) << output << " = 0;";
    }
    VLOG(3) << "------";
  }
}

void RevisedSimplex::AdvanceDeterministicTime(TimeLimit* time_limit) {
  DCHECK(time_limit != nullptr);
  const double current_deterministic_time = DeterministicTime();
  const double deterministic_time_delta =
      current_deterministic_time - last_deterministic_time_update_;
  time_limit->AdvanceDeterministicTime(deterministic_time_delta);
  last_deterministic_time_update_ = current_deterministic_time;
}

#undef DCHECK_COL_BOUNDS
#undef DCHECK_ROW_BOUNDS

}  // namespace glop
}  // namespace operations_research
