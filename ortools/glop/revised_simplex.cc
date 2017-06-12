// Copyright 2010-2014 Google
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
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/join.h"
#include "ortools/glop/initial_basis.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_print_utils.h"
#include "ortools/lp_data/lp_utils.h"
#include "ortools/lp_data/matrix_utils.h"
#include "ortools/util/fp_utils.h"

DEFINE_bool(simplex_display_numbers_as_fractions, false,
            "Display numbers as fractions.");
DEFINE_bool(simplex_stop_after_first_basis, false,
            "Stop after first basis has been computed.");
DEFINE_bool(simplex_stop_after_feasibility, false,
            "Stop after first phase has been completed.");
DEFINE_bool(simplex_display_stats, false, "Display algorithm statistics.");

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

#define DCHECK_ROW_BOUNDS(row) \
  {                            \
    DCHECK_LE(0, row);         \
    DCHECK_GT(num_rows_, row); \
  }

constexpr const uint64 kDeterministicSeed = 42;

RevisedSimplex::RevisedSimplex()
    : problem_status_(ProblemStatus::INIT),
      num_rows_(0),
      num_cols_(0),
      first_slack_col_(kInvalidCol),
      current_objective_(),
      objective_(),
      lower_bound_(),
      upper_bound_(),
      basis_(),
      variable_name_(),
      direction_(),
      error_(),
      basis_factorization_(matrix_with_slack_, basis_),
      variables_info_(compact_matrix_, lower_bound_, upper_bound_),
      variable_values_(compact_matrix_, basis_, variables_info_,
                       basis_factorization_),
      dual_edge_norms_(basis_factorization_),
      primal_edge_norms_(matrix_with_slack_, compact_matrix_, variables_info_,
                         basis_factorization_),
      update_row_(compact_matrix_, transposed_matrix_, variables_info_, basis_,
                  basis_factorization_),
      reduced_costs_(compact_matrix_, current_objective_, basis_,
                     variables_info_, basis_factorization_, &random_),
      entering_variable_(variables_info_, &random_, &reduced_costs_,
                         &primal_edge_norms_),
      num_iterations_(0),
      num_feasibility_iterations_(0),
      num_optimization_iterations_(0),
      total_time_(0.0),
      feasibility_time_(0.0),
      optimization_time_(0.0),
      last_deterministic_time_update_(0.0),
      iteration_stats_(),
      ratio_test_stats_(),
      function_stats_("SimplexFunctionStats"),
      parameters_(),
      test_lu_(),
      feasibility_phase_(true),
      random_(kDeterministicSeed) {
  SetParameters(parameters_);
}

void RevisedSimplex::ClearStateForNextSolve() {
  SCOPED_TIME_STAT(&function_stats_);
  solution_state_.statuses.clear();
}

void RevisedSimplex::LoadStateForNextSolve(const BasisState& state) {
  SCOPED_TIME_STAT(&function_stats_);
  solution_state_ = state;
  solution_state_has_been_set_externally_ = true;
}

Status RevisedSimplex::Solve(const LinearProgram& lp, TimeLimit* time_limit) {
  SCOPED_TIME_STAT(&function_stats_);
  DCHECK(lp.IsCleanedUp());
  GLOP_RETURN_ERROR_IF_NULL(time_limit);
  if (!lp.IsInEquationForm()) {
    return Status(Status::ERROR_INVALID_PROBLEM,
                  "The problem is not in the equations form.");
  }
  Cleanup update_deterministic_time_on_return(
      [this, time_limit]() { AdvanceDeterministicTime(time_limit); });

  // Initialization. Note That Initialize() must be called first since it
  // analyzes the current solver state.
  const double start_time = time_limit->GetElapsedTime();
  GLOP_RETURN_IF_ERROR(Initialize(lp));
  dual_infeasibility_improvement_direction_.clear();
  update_row_.Invalidate();
  test_lu_.Clear();
  problem_status_ = ProblemStatus::INIT;
  feasibility_phase_ = true;
  num_iterations_ = 0;
  num_feasibility_iterations_ = 0;
  num_optimization_iterations_ = 0;
  feasibility_time_ = 0.0;
  optimization_time_ = 0.0;
  total_time_ = 0.0;

  if (VLOG_IS_ON(1)) {
    ComputeNumberOfEmptyRows();
    ComputeNumberOfEmptyColumns();
    DisplayBasicVariableStatistics();
    DisplayProblem();
  }
  if (FLAGS_simplex_stop_after_first_basis) {
    DisplayAllStats();
    return Status::OK();
  }

  const bool use_dual = parameters_.use_dual_simplex();
  VLOG(1) << "------ " << (use_dual ? "Dual simplex." : "Primal simplex.");
  VLOG(1) << "The matrix has " << matrix_with_slack_.num_rows() << " rows, "
          << matrix_with_slack_.num_cols() << " columns, "
          << matrix_with_slack_.num_entries() << " entries.";

  current_objective_ = objective_;

  // TODO(user): Avoid doing the first phase checks when we know from the
  // incremental solve that the solution is already dual or primal feasible.
  VLOG(1) << "------ First phase: feasibility.";
  entering_variable_.SetPricingRule(parameters_.feasibility_rule());
  if (use_dual) {
    if (parameters_.perturb_costs_in_dual_simplex()) {
      reduced_costs_.PerturbCosts();
    }

    variables_info_.MakeBoxedVariableRelevant(false);
    GLOP_RETURN_IF_ERROR(DualMinimize(time_limit));
    DisplayIterationInfo();
    variables_info_.MakeBoxedVariableRelevant(true);
    reduced_costs_.MakeReducedCostsPrecise();

    // This is needed to display errors properly.
    MakeBoxedVariableDualFeasible(variables_info_.GetNonBasicBoxedVariables(),
                                  /*update_basic_values=*/false);
    variable_values_.RecomputeBasicVariableValues();
    variable_values_.ResetPrimalInfeasibilityInformation();
  } else {
    reduced_costs_.MaintainDualInfeasiblePositions(true);
    GLOP_RETURN_IF_ERROR(Minimize(time_limit));
    DisplayIterationInfo();

    // After the primal phase I, we need to restore the objective.
    current_objective_ = objective_;
    reduced_costs_.ResetForNewObjective();
  }

  // Reduced costs must be explicitly recomputed because DisplayErrors() is
  // const.
  // TODO(user): This API is not really nice.
  reduced_costs_.GetReducedCosts();
  DisplayErrors();

  feasibility_phase_ = false;
  feasibility_time_ = time_limit->GetElapsedTime() - start_time;
  entering_variable_.SetPricingRule(parameters_.optimization_rule());
  num_feasibility_iterations_ = num_iterations_;

  VLOG(1) << "------ Second phase: optimization.";

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
  for (int num_optims = 0;
       // We want to enter the loop when both num_optims and num_iterations_ are
       // *equal* to the corresponding limits (to return a meaningful status
       // when the limits are set to 0).
       num_optims <= parameters_.max_number_of_reoptimizations() &&
       (num_iterations_ == 0 ||
        num_iterations_ < parameters_.max_number_of_iterations()) &&
       !time_limit->LimitReached() && !FLAGS_simplex_stop_after_feasibility &&
       (problem_status_ == ProblemStatus::PRIMAL_FEASIBLE ||
        problem_status_ == ProblemStatus::DUAL_FEASIBLE);
       ++num_optims) {
    if (problem_status_ == ProblemStatus::PRIMAL_FEASIBLE) {
      // Run the primal simplex.
      reduced_costs_.MaintainDualInfeasiblePositions(true);
      GLOP_RETURN_IF_ERROR(Minimize(time_limit));
    } else {
      // Run the dual simplex.
      reduced_costs_.MaintainDualInfeasiblePositions(false);
      GLOP_RETURN_IF_ERROR(DualMinimize(time_limit));
    }

    // Minimize() or DualMinimize() always double check the result with maximum
    // precision by refactoring the basis before exiting (except if an
    // iteration or time limit was reached).
    DCHECK(problem_status_ == ProblemStatus::PRIMAL_FEASIBLE ||
           problem_status_ == ProblemStatus::DUAL_FEASIBLE ||
           basis_factorization_.IsRefactorized());

    // Remove the bound and cost shifts (or perturbations).
    //
    // Note(user): Currently, we never do both at the same time, so we could
    // be a bit faster here, but then this is quick anyway.
    const VariableStatusRow& statuses = variables_info_.GetStatusRow();
    for (ColIndex col(0); col < num_cols_; ++col) {
      if (statuses[col] != VariableStatus::BASIC) {
        SetNonBasicVariableStatusAndDeriveValue(col, statuses[col]);
      }
    }
    GLOP_RETURN_IF_ERROR(basis_factorization_.Refactorize());
    variable_values_.RecomputeBasicVariableValues();
    reduced_costs_.ClearAndRemoveCostShifts();

    // Reduced costs must be explicitly recomputed because DisplayErrors() is
    // const.
    // TODO(user): This API is not really nice.
    reduced_costs_.GetReducedCosts();
    DisplayIterationInfo();
    DisplayErrors();

    // TODO(user): We should also confirm the PRIMAL_UNBOUNDED or DUAL_UNBOUNDED
    // status by checking with the other phase I that the problem is really
    // DUAL_INFEASIBLE or PRIMAL_INFEASIBLE. For instace we currently report
    // PRIMAL_UNBOUNDED with the primal on the problem l30.mps instead of
    // OPTIMAL and the dual does not have issues on this problem.
    if (problem_status_ == ProblemStatus::DUAL_UNBOUNDED) {
      const Fractional tolerance = parameters_.solution_feasibility_tolerance();
      if (reduced_costs_.ComputeMaximumDualResidual() > tolerance ||
          variable_values_.ComputeMaximumPrimalResidual() > tolerance ||
          reduced_costs_.ComputeMaximumDualInfeasibility() > tolerance) {
        LOG(WARNING) << "DUAL_UNBOUNDED was reported, but the residual and/or"
                     << "dual infeasibility is above the tolerance";
      }
      break;
    }

    // Change the status, if after the shift and perturbation removal the
    // problem is not OPTIMAL anymore.
    if (problem_status_ == ProblemStatus::OPTIMAL) {
      const Fractional solution_tolerance =
          parameters_.solution_feasibility_tolerance();
      if (variable_values_.ComputeMaximumPrimalResidual() >
              solution_tolerance ||
          reduced_costs_.ComputeMaximumDualResidual() > solution_tolerance) {
        LOG(WARNING) << "OPTIMAL was reported, yet one of the residuals is "
                        "above the solution feasibility tolerance after the "
                        "shift/perturbation are removed.";
        problem_status_ = ProblemStatus::IMPRECISE;
      } else {
        // We use the "precise" tolerances here to try to report the best
        // possible solution.
        const Fractional primal_tolerance =
            parameters_.primal_feasibility_tolerance();
        const Fractional dual_tolerance =
            parameters_.dual_feasibility_tolerance();
        const Fractional primal_infeasibility =
            variable_values_.ComputeMaximumPrimalInfeasibility();
        const Fractional dual_infeasibility =
            reduced_costs_.ComputeMaximumDualInfeasibility();
        if (primal_infeasibility > primal_tolerance &&
            dual_infeasibility > dual_tolerance) {
          LOG(WARNING) << "OPTIMAL was reported, yet both of the infeasibility "
                          "are above the tolerance after the "
                          "shift/perturbation are removed.";
          problem_status_ = ProblemStatus::IMPRECISE;
        } else if (primal_infeasibility > primal_tolerance) {
          VLOG(1) << "Re-optimizing with dual simplex ... ";
          problem_status_ = ProblemStatus::DUAL_FEASIBLE;
        } else if (dual_infeasibility > dual_tolerance) {
          VLOG(1) << "Re-optimizing with primal simplex ... ";
          problem_status_ = ProblemStatus::PRIMAL_FEASIBLE;
        }
      }
    }
  }

  // Store the result for the solution getters.
  SaveState();
  solution_objective_value_ = ComputeInitialProblemObjectiveValue();
  solution_dual_values_ = reduced_costs_.GetDualValues();
  solution_reduced_costs_ = reduced_costs_.GetReducedCosts();
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

  total_time_ = time_limit->GetElapsedTime() - start_time;
  optimization_time_ = total_time_ - feasibility_time_;
  num_optimization_iterations_ = num_iterations_ - num_feasibility_iterations_;

  DisplayAllStats();
  return Status::OK();
}

ProblemStatus RevisedSimplex::GetProblemStatus() const {
  return problem_status_;
}

Fractional RevisedSimplex::GetObjectiveValue() const {
  return solution_objective_value_;
}

int64 RevisedSimplex::GetNumberOfIterations() const { return num_iterations_; }

RowIndex RevisedSimplex::GetProblemNumRows() const { return num_rows_; }

ColIndex RevisedSimplex::GetProblemNumCols() const { return num_cols_; }

Fractional RevisedSimplex::GetVariableValue(ColIndex col) const {
  return variable_values_.Get(col);
}

Fractional RevisedSimplex::GetReducedCost(ColIndex col) const {
  return solution_reduced_costs_[col];
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
  return StringPrintf(
      "Problem status                               : %s\n"
      "Solving time                                 : %-6.4g\n"
      "Number of iterations                         : %llu\n"
      "Time for solvability (first phase)           : %-6.4g\n"
      "Number of iterations for solvability         : %llu\n"
      "Time for optimization                        : %-6.4g\n"
      "Number of iterations for optimization        : %llu\n"
      "Stop after first basis                       : %d\n",
      GetProblemStatusString(problem_status_).c_str(), total_time_,
      num_iterations_, feasibility_time_, num_feasibility_iterations_,
      optimization_time_, num_optimization_iterations_,
      FLAGS_simplex_stop_after_first_basis);
}

double RevisedSimplex::DeterministicTime() const {
  // TODO(user): Also take into account the dual edge norms and the reduced cost
  // updates.
  return basis_factorization_.DeterministicTime() +
         update_row_.DeterministicTime() +
         primal_edge_norms_.DeterministicTime();
}

void RevisedSimplex::SetVariableNames() {
  variable_name_.resize(num_cols_, "");
  for (ColIndex col(0); col < first_slack_col_; ++col) {
    const ColIndex var_index = col + 1;
    variable_name_[col] = StringPrintf("x%d", ColToIntIndex(var_index));
  }
  for (ColIndex col(first_slack_col_); col < num_cols_; ++col) {
    const ColIndex var_index = col - first_slack_col_ + 1;
    variable_name_[col] = StringPrintf("s%d", ColToIntIndex(var_index));
  }
}

VariableStatus RevisedSimplex::ComputeDefaultVariableStatus(
    ColIndex col) const {
  DCHECK_COL_BOUNDS(col);
  if (lower_bound_[col] == upper_bound_[col]) {
    return VariableStatus::FIXED_VALUE;
  }
  if (lower_bound_[col] == -kInfinity && upper_bound_[col] == kInfinity) {
    return VariableStatus::FREE;
  }

  // Special case for singleton column so UseSingletonColumnInInitialBasis()
  // works better. We set the initial value of a boxed variable to its bound
  // that minimizes the cost.
  if (parameters_.exploit_singleton_column_in_initial_basis() &&
      matrix_with_slack_.column(col).num_entries() == 1) {
    const Fractional objective = objective_[col];
    if (objective > 0 && IsFinite(lower_bound_[col])) {
      return VariableStatus::AT_LOWER_BOUND;
    }
    if (objective < 0 && IsFinite(upper_bound_[col])) {
      return VariableStatus::AT_UPPER_BOUND;
    }
  }

  // Returns the bound with the lowest magnitude. Note that it must be finite
  // because the VariableStatus::FREE case was tested earlier.
  DCHECK(IsFinite(lower_bound_[col]) || IsFinite(upper_bound_[col]));
  return std::abs(lower_bound_[col]) <= std::abs(upper_bound_[col])
             ? VariableStatus::AT_LOWER_BOUND
             : VariableStatus::AT_UPPER_BOUND;
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
  variables_info_.Update(leaving_col, leaving_variable_status);
  DCHECK(leaving_variable_status == VariableStatus::AT_UPPER_BOUND ||
         leaving_variable_status == VariableStatus::AT_LOWER_BOUND ||
         leaving_variable_status == VariableStatus::FIXED_VALUE);

  basis_[basis_row] = entering_col;
  variables_info_.Update(entering_col, VariableStatus::BASIC);
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
// cost, see ComputeDefaultVariableStatus()). This is unit tested.
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
  for (ColIndex col(0); col < num_cols_; ++col) {
    if (matrix_with_slack_.column(col).num_entries() != 1) continue;
    if (lower_bound_[col] == upper_bound_[col]) continue;
    const Fractional slope =
        matrix_with_slack_.column(col).GetFirstCoefficient();
    if (variable_values_.Get(col) == lower_bound_[col]) {
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
    if (new_value >= lower_bound_[col] && new_value <= upper_bound_[col]) {
      error_[row] = 0.0;

      // Use this variable in the initial basis.
      (*basis)[row] = col;
      variables_info_.Update(col, VariableStatus::BASIC);
      variable_values_.Set(col, new_value);
      continue;
    }

    // The idea here is that if the singleton column cannot be used to "absorb"
    // all error_[row], if it is boxed, it can still be used to make the
    // infeasibility smaller (with a bound flip).
    const Fractional box_width = variables_info_.GetBoundDifference(col);
    DCHECK_NE(box_width, 0.0);
    DCHECK_NE(error_[row], 0.0);
    const Fractional error_sign = error_[row] / coeff;
    if (variable_values[col] == lower_bound_[col] && error_sign > 0.0) {
      DCHECK(IsFinite(box_width));
      error_[row] -= coeff * box_width;
      SetNonBasicVariableStatusAndDeriveValue(col,
                                              VariableStatus::AT_UPPER_BOUND);
      continue;
    }
    if (variable_values[col] == upper_bound_[col] && error_sign < 0.0) {
      DCHECK(IsFinite(box_width));
      error_[row] += coeff * box_width;
      SetNonBasicVariableStatusAndDeriveValue(col,
                                              VariableStatus::AT_LOWER_BOUND);
      continue;
    }
  }
}

bool RevisedSimplex::InitializeMatrixAndTestIfUnchanged(
    const LinearProgram& lp, bool* only_change_is_new_rows,
    bool* only_change_is_new_cols, ColIndex* num_new_cols) {
  SCOPED_TIME_STAT(&function_stats_);
  DCHECK(only_change_is_new_rows != nullptr);
  DCHECK(only_change_is_new_cols != nullptr);
  DCHECK(num_new_cols != nullptr);
  DCHECK_NE(kInvalidCol, lp.GetFirstSlackVariable());
  DCHECK_EQ(num_cols_, compact_matrix_.num_cols());
  DCHECK_EQ(num_rows_, compact_matrix_.num_rows());

  DCHECK_EQ(lp.num_variables(),
            lp.GetFirstSlackVariable() + RowToColIndex(lp.num_constraints()));
  DCHECK(IsRightMostSquareMatrixIdentity(lp.GetSparseMatrix()));
  const bool old_part_of_matrix_is_unchanged =
      AreFirstColumnsAndRowsExactlyEquals(
          num_rows_, first_slack_col_, lp.GetSparseMatrix(), compact_matrix_);

  // Test if the matrix is unchanged, and if yes, just returns true. Note that
  // this doesn't check the columns corresponding to the slack variables,
  // because they were checked by lp.IsInEquationForm() when Solve() was called.
  if (old_part_of_matrix_is_unchanged && lp.num_constraints() == num_rows_ &&
      lp.num_variables() == num_cols_) {
    // IMPORTANT: we need to recreate matrix_with_slack_ because this matrix
    // view was refering to a previous lp.GetSparseMatrix(). The matrices are
    // the same, but we do need to update the pointers.
    //
    // TODO(user): use compact_matrix_ everywhere instead.
    matrix_with_slack_.PopulateFromMatrix(lp.GetSparseMatrix());
    return true;
  }

  // Check if the new matrix can be derived from the old one just by adding
  // new rows (i.e new constraints).
  *only_change_is_new_rows = old_part_of_matrix_is_unchanged &&
                             lp.num_constraints() > num_rows_ &&
                             lp.GetFirstSlackVariable() == first_slack_col_;

  // Check if the new matrix can be derived from the old one just by adding
  // new columns (i.e new variables).
  *only_change_is_new_cols = old_part_of_matrix_is_unchanged &&
                             lp.num_constraints() == num_rows_ &&
                             lp.GetFirstSlackVariable() > first_slack_col_;
  *num_new_cols =
      *only_change_is_new_cols ? lp.num_variables() - num_cols_ : ColIndex(0);

  // Initialize matrix_with_slack_.
  matrix_with_slack_.PopulateFromMatrix(lp.GetSparseMatrix());
  first_slack_col_ = lp.GetFirstSlackVariable();

  // Initialize the new dimensions.
  num_rows_ = lp.num_constraints();
  num_cols_ = lp.num_variables();

  // Populate compact_matrix_ and transposed_matrix_ if needed. Note that we
  // already added all the slack variables at this point, so matrix_ will not
  // change anymore.
  compact_matrix_.PopulateFromMatrixView(matrix_with_slack_);
  if (parameters_.use_transposed_matrix()) {
    transposed_matrix_.PopulateFromTranspose(compact_matrix_);
  }
  return false;
}

bool RevisedSimplex::OldBoundsAreUnchangedAndNewVariablesHaveOneBoundAtZero(
    const LinearProgram& lp, ColIndex num_new_cols) {
  SCOPED_TIME_STAT(&function_stats_);
  DCHECK_EQ(lp.num_variables(), num_cols_);
  DCHECK_LE(num_new_cols, first_slack_col_);
  const ColIndex first_new_col(first_slack_col_ - num_new_cols);

  // Check the original variable bounds.
  for (ColIndex col(0); col < first_new_col; ++col) {
    if (lower_bound_[col] != lp.variable_lower_bounds()[col] ||
        upper_bound_[col] != lp.variable_upper_bounds()[col]) {
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
  for (ColIndex col(first_slack_col_); col < num_cols_; ++col) {
    if (lower_bound_[col - num_new_cols] != lp.variable_lower_bounds()[col] ||
        upper_bound_[col - num_new_cols] != lp.variable_upper_bounds()[col]) {
      return false;
    }
  }
  return true;
}

bool RevisedSimplex::InitializeBoundsAndTestIfUnchanged(
    const LinearProgram& lp) {
  SCOPED_TIME_STAT(&function_stats_);
  lower_bound_.resize(num_cols_, 0.0);
  upper_bound_.resize(num_cols_, 0.0);
  bound_perturbation_.assign(num_cols_, 0.0);

  // Variable bounds, for both non-slack and slack variables.
  DCHECK_EQ(lp.num_variables(), num_cols_);
  bool bounds_are_unchanged = true;
  for (ColIndex col(0); col < lp.num_variables(); ++col) {
    if (lower_bound_[col] != lp.variable_lower_bounds()[col] ||
        upper_bound_[col] != lp.variable_upper_bounds()[col]) {
      bounds_are_unchanged = false;
    }
    lower_bound_[col] = lp.variable_lower_bounds()[col];
    upper_bound_[col] = lp.variable_upper_bounds()[col];
  }

  return bounds_are_unchanged;
}

bool RevisedSimplex::InitializeObjectiveAndTestIfUnchanged(
    const LinearProgram& lp) {
  DCHECK_EQ(num_cols_, lp.num_variables());
  SCOPED_TIME_STAT(&function_stats_);
  current_objective_.assign(num_cols_, 0.0);

  // Note that we use the minimization version of the objective.
  bool objective_is_unchanged = true;
  objective_.resize(num_cols_, 0.0);
  for (ColIndex col(0); col < lp.num_variables(); ++col) {
    if (objective_[col] !=
        lp.GetObjectiveCoefficientForMinimizationVersion(col)) {
      objective_is_unchanged = false;
    }
    objective_[col] = lp.GetObjectiveCoefficientForMinimizationVersion(col);
  }
  for (ColIndex col(lp.num_variables()); col < num_cols_; ++col) {
    if (objective_[col] != 0.0) {
      objective_is_unchanged = false;
    }
    objective_[col] = 0.0;
  }

  // Sets the members needed to display the objective correctly.
  objective_offset_ = lp.objective_offset();
  objective_scaling_factor_ = lp.objective_scaling_factor();
  if (lp.IsMaximizationProblem()) {
    objective_offset_ = -objective_offset_;
    objective_scaling_factor_ = -objective_scaling_factor_;
  }
  return objective_is_unchanged;
}

void RevisedSimplex::InitializeObjectiveLimit(const LinearProgram& lp) {
  objective_limit_reached_ = false;
  DCHECK(std::isfinite(objective_offset_));
  DCHECK(std::isfinite(objective_scaling_factor_));
  DCHECK_NE(0.0, objective_scaling_factor_);

  // This sets dual_objective_limit_ and then primal_objective_limit_.
  const Fractional tolerance = parameters_.solution_feasibility_tolerance();
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

    // The std::isfinite() test is there to avoid generating NaNs with clang in
    // fast-math mode on iOS 9.3.i.
    if (set_dual) {
      dual_objective_limit_ = std::isfinite(shifted_limit)
                                  ? shifted_limit * (1.0 + tolerance)
                                  : shifted_limit;
    } else {
      primal_objective_limit_ = std::isfinite(shifted_limit)
                                    ? shifted_limit * (1.0 - tolerance)
                                    : shifted_limit;
    }
  }
}

void RevisedSimplex::InitializeVariableStatusesForWarmStart(
    const BasisState& state, ColIndex num_new_cols) {
  variables_info_.Initialize();
  RowIndex num_basic_variables(0);
  DCHECK_LE(num_new_cols, first_slack_col_);
  const ColIndex first_new_col(first_slack_col_ - num_new_cols);
  // Compute the status for all the columns (note that the slack variables are
  // already added at the end of the matrix at this stage).
  for (ColIndex col(0); col < num_cols_; ++col) {
    const VariableStatus default_status = ComputeDefaultVariableStatus(col);

    // Start with the given "warm" status from the BasisState if it exists.
    VariableStatus status = default_status;
    if (col < first_new_col && col < state.statuses.size()) {
      status = state.statuses[col];
    } else if (col >= first_slack_col_ &&
               col - num_new_cols < state.statuses.size()) {
      status = state.statuses[col - num_new_cols];
    }

    if (status == VariableStatus::BASIC) {
      // Do not allow more than num_rows_ VariableStatus::BASIC variables.
      if (num_basic_variables == num_rows_) {
        VLOG(1) << "Too many basic variables in the warm-start basis."
                << "Only keeping the first ones as VariableStatus::BASIC.";
        SetNonBasicVariableStatusAndDeriveValue(col, default_status);
      } else {
        ++num_basic_variables;
        variables_info_.UpdateToBasicStatus(col);
      }
    } else {
      // Remove incompatibilities between the warm status and the variable
      // bounds. We use the default status as an indication of the bounds
      // type.
      if ((status != default_status) &&
          ((default_status == VariableStatus::FIXED_VALUE) ||
           (status == VariableStatus::FREE) ||
           (status == VariableStatus::FIXED_VALUE) ||
           (status == VariableStatus::AT_LOWER_BOUND &&
            lower_bound_[col] == -kInfinity) ||
           (status == VariableStatus::AT_UPPER_BOUND &&
            upper_bound_[col] == kInfinity))) {
        status = default_status;
      }
      SetNonBasicVariableStatusAndDeriveValue(col, status);
    }
  }
}

// This implementation starts with an initial matrix B equal to the the identity
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
  int num_free_variables = 0;
  variables_info_.Initialize();
  for (ColIndex col(0); col < num_cols_; ++col) {
    const VariableStatus status = ComputeDefaultVariableStatus(col);
    SetNonBasicVariableStatusAndDeriveValue(col, status);
    if (status == VariableStatus::FREE) ++num_free_variables;
  }
  VLOG(1) << "Number of free variables in the problem: " << num_free_variables;

  // Start by using an all-slack basis.
  RowToColMapping basis(num_rows_, kInvalidCol);
  for (RowIndex row(0); row < num_rows_; ++row) {
    basis[row] = SlackColIndex(row);
  }

  // If possible, for the primal simplex we replace some slack variables with
  // some singleton columns present in the problem.
  if (!parameters_.use_dual_simplex()) {
    // Compute the primal infeasibility of the initial variable values in
    // error_.
    ComputeVariableValuesError();

    // TODO(user): A better but slightly more complex algorithm would be to:
    // - Ignore all singleton columns except the slacks during phase I.
    // - For this, change the slack variable bounds accordingly.
    // - At the end of phase I, restore the slack variable bounds and perform
    //   the same algorithm to start with feasible and "optimal" values of the
    //   singleton columns.
    if (parameters_.exploit_singleton_column_in_initial_basis()) {
      basis.assign(num_rows_, kInvalidCol);
      UseSingletonColumnInInitialBasis(&basis);

      // Eventually complete the basis with fixed slack columns.
      for (RowIndex row(0); row < num_rows_; ++row) {
        if (basis[row] == kInvalidCol) {
          basis[row] = SlackColIndex(row);
        }
      }
    }
  }

  // Use an advanced initial basis to remove the fixed variables from the basis.
  if (parameters_.initial_basis() != GlopParameters::NONE) {
    // First unassign the fixed variables from basis.
    int num_fixed_variables = 0;
    for (RowIndex row(0); row < basis.size(); ++row) {
      const ColIndex col = basis[row];
      if (lower_bound_[col] == upper_bound_[col]) {
        basis[row] = kInvalidCol;
        ++num_fixed_variables;
      }
    }

    if (num_fixed_variables > 0) {
      // Then complete the basis with an advanced initial basis algorithm.
      VLOG(1) << "Trying to remove " << num_fixed_variables
              << " fixed variables from the initial basis.";
      InitialBasis initial_basis(matrix_with_slack_, objective_, lower_bound_,
                                 upper_bound_, variables_info_.GetTypeRow());

      if (parameters_.use_dual_simplex()) {
        // This dual version only uses zero-cost columns to complete the basis.
        initial_basis.CompleteTriangularDualBasis(num_cols_, &basis);
      } else if (parameters_.initial_basis() == GlopParameters::BIXBY) {
        if (parameters_.use_scaling()) {
          initial_basis.CompleteBixbyBasis(first_slack_col_, &basis);
        } else {
          LOG(WARNING) << "Bixby initial basis algorithm requires the problem "
                       << "to be scaled. Skipping Bixby's algorithm.";
        }
      } else if (parameters_.initial_basis() == GlopParameters::TRIANGULAR) {
        // Note the use of num_cols_ here because this algorithm
        // benefits from treating fixed slack columns like any other column.
        RowToColMapping basis_copy = basis;
        if (!initial_basis.CompleteTriangularPrimalBasis(num_cols_, &basis)) {
          LOG(WARNING) << "Reverting to Bixby's initial basis algorithm.";
          basis = basis_copy;
          if (parameters_.use_scaling()) {
            initial_basis.CompleteBixbyBasis(first_slack_col_, &basis);
          }
        }
      } else {
        LOG(WARNING) << "Unsupported initial_basis parameters: "
                     << parameters_.initial_basis();
      }
    }
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
    variables_info_.Update(basis_[row], VariableStatus::BASIC);
  }
  GLOP_RETURN_IF_ERROR(basis_factorization_.Initialize());
  PermuteBasis();
  DCHECK(BasisIsConsistent());

  variable_values_.RecomputeBasicVariableValues();
  const Fractional tolerance = parameters_.primal_feasibility_tolerance();
  DCHECK_LE(variable_values_.ComputeMaximumPrimalResidual(), tolerance);
  return Status::OK();
}

Status RevisedSimplex::Initialize(const LinearProgram& lp) {
  parameters_ = initial_parameters_;
  PropagateParameters();

  // Calling InitializeMatrixAndTestIfUnchanged() first is important because
  // this is where num_rows_ and num_cols_ are computed.
  //
  // Note that these functions can't depend on use_dual_simplex() since we may
  // change it below.
  bool only_change_is_new_rows = false;
  bool only_change_is_new_cols = false;
  ColIndex num_new_cols(0);
  const bool is_matrix_unchanged = InitializeMatrixAndTestIfUnchanged(
      lp, &only_change_is_new_rows, &only_change_is_new_cols, &num_new_cols);
  const bool only_new_bounds =
      only_change_is_new_cols && num_new_cols > 0 &&
      OldBoundsAreUnchangedAndNewVariablesHaveOneBoundAtZero(lp, num_new_cols);
  const bool objective_is_unchanged = InitializeObjectiveAndTestIfUnchanged(lp);
  const bool bounds_are_unchanged = InitializeBoundsAndTestIfUnchanged(lp);

  // If parameters_.allow_simplex_algorithm_change() is true and we already have
  // a primal (resp. dual) feasible solution, then we use the primal (resp.
  // dual) algorithm since there is a good chance that it will be faster.
  if (is_matrix_unchanged && parameters_.allow_simplex_algorithm_change()) {
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
  if (VLOG_IS_ON(1)) {
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
  if (!solution_state_.IsEmpty()) {
    if (solution_state_has_been_set_externally_) {
      // If an external basis has been provided we need to perform more work,
      // e.g., factorize and validate it.
      InitializeVariableStatusesForWarmStart(solution_state_, ColIndex(0));
      basis_.assign(num_rows_, kInvalidCol);
      RowIndex row(0);
      for (ColIndex col : variables_info_.GetIsBasicBitRow()) {
        basis_[row] = col;
        ++row;
      }
      // TODO(user): If the basis is incomplete, we could complete it with
      // better slack variables than is done by InitializeFirstBasis() by
      // using a partial LU decomposition (see markowitz.h).
      if (InitializeFirstBasis(basis_).ok()) {
        primal_edge_norms_.Clear();
        dual_edge_norms_.Clear();
        dual_pricing_vector_.clear();
        reduced_costs_.ClearAndRemoveCostShifts();
        solve_from_scratch = false;
      } else {
        LOG(WARNING) << "RevisedSimplex is not using the externally provided "
                        "basis because it is not factorizable.";
      }
    } else if (!parameters_.use_dual_simplex()) {
      // With primal simplex, always clear dual norms and dual pricing.
      // Incrementality is supported only if only change to the matrix and
      // bounds is adding new columns (objective may change), and that all
      // new columns have a bound equal to zero.
      dual_edge_norms_.Clear();
      dual_pricing_vector_.clear();
      if (is_matrix_unchanged && bounds_are_unchanged) {
        // TODO(user): Do not do that if objective_is_unchanged. Currently
        // this seems to break something. Investigate.
        reduced_costs_.ClearAndRemoveCostShifts();
        solve_from_scratch = false;
      } else if (only_change_is_new_cols && only_new_bounds) {
        InitializeVariableStatusesForWarmStart(solution_state_, num_new_cols);
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
        if (is_matrix_unchanged) {
          if (!bounds_are_unchanged) {
            InitializeVariableStatusesForWarmStart(solution_state_,
                                                   ColIndex(0));
            variable_values_.RecomputeBasicVariableValues();
          }
          solve_from_scratch = false;
        } else if (only_change_is_new_rows) {
          // For the dual-simplex, we also perform a warm start if a couple of
          // new rows where added.
          InitializeVariableStatusesForWarmStart(solution_state_, ColIndex(0));

          // TODO(user): Both the edge norms and the reduced costs do not really
          // need to be recomputed. We just need to initialize the ones of the
          // new slack variables to 1.0 for the norms and 0.0 for the reduced
          // costs.
          dual_edge_norms_.Clear();
          reduced_costs_.ClearAndRemoveCostShifts();
          dual_pricing_vector_.clear();

          // Note that this needs to be done after the Clear() calls above.
          GLOP_RETURN_IF_ERROR(InitializeFirstBasis(basis_));
          solve_from_scratch = false;
        }
      }
    }
  }

  if (solve_from_scratch) {
    VLOG(1) << "Solve from scratch.";
    basis_factorization_.Clear();
    reduced_costs_.ClearAndRemoveCostShifts();
    primal_edge_norms_.Clear();
    dual_edge_norms_.Clear();
    dual_pricing_vector_.clear();
    GLOP_RETURN_IF_ERROR(CreateInitialBasis());
  } else {
    VLOG(1) << "Incremental solve.";
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
  const Fractional tolerance = parameters_.primal_feasibility_tolerance();
  for (RowIndex row(0); row < num_rows_; ++row) {
    const ColIndex col = basis_[row];
    const Fractional value = variable_values[col];
    if (variable_types[col] == VariableType::UNCONSTRAINED) {
      ++num_free_variables;
    }
    if (value > upper_bound_[col] + tolerance ||
        value < lower_bound_[col] - tolerance) {
      ++num_infeasible_variables;
    }
    if (col >= first_slack_col_) {
      ++num_slack_variables;
    }
    if (lower_bound_[col] == upper_bound_[col]) {
      ++num_fixed_variables;
    } else if (variable_values[col] == lower_bound_[col] ||
               variable_values[col] == upper_bound_[col]) {
      ++num_variables_at_bound;
    }
  }

  VLOG(1) << "Basis size: " << num_rows_;
  VLOG(1) << "Number of basic infeasible variables: "
          << num_infeasible_variables;
  VLOG(1) << "Number of basic slack variables: " << num_slack_variables;
  VLOG(1) << "Number of basic variables at bound: " << num_variables_at_bound;
  VLOG(1) << "Number of basic fixed variables: " << num_fixed_variables;
  VLOG(1) << "Number of basic free variables: " << num_free_variables;
}

void RevisedSimplex::SaveState() {
  DCHECK_EQ(num_cols_, variables_info_.GetStatusRow().size());
  solution_state_.statuses = variables_info_.GetStatusRow();
  solution_state_has_been_set_externally_ = false;
}

RowIndex RevisedSimplex::ComputeNumberOfEmptyRows() {
  DenseBooleanColumn contains_data(num_rows_, false);
  for (ColIndex col(0); col < num_cols_; ++col) {
    for (const SparseColumn::Entry e : matrix_with_slack_.column(col)) {
      contains_data[e.row()] = true;
    }
  }
  RowIndex num_empty_rows(0);
  for (RowIndex row(0); row < num_rows_; ++row) {
    if (!contains_data[row]) {
      ++num_empty_rows;
      VLOG(1) << "Row " << row << " is empty.";
    }
  }
  return num_empty_rows;
}

ColIndex RevisedSimplex::ComputeNumberOfEmptyColumns() {
  ColIndex num_empty_cols(0);
  for (ColIndex col(0); col < num_cols_; ++col) {
    if (matrix_with_slack_.column(col).IsEmpty()) {
      ++num_empty_cols;
      VLOG(1) << "Column " << col << " is empty.";
    }
  }
  return num_empty_cols;
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

  // If we are doing too many degenerate iterations, we try to perturb the
  // problem by extending each basic variable bound with a random value. See how
  // bound_perturbation_ is used in ComputeHarrisRatioAndLeavingCandidates().
  //
  // Note that the perturbation is currenlty only reset to zero at the end of
  // the algorithm.
  //
  // TODO(user): This is currently disabled because the improvement is unclear.
  if (/* DISABLES CODE */ false &&
      (!feasibility_phase_ && num_consecutive_degenerate_iterations_ >= 100)) {
    VLOG(1) << "Perturbing the problem.";
    const Fractional tolerance = parameters_.harris_tolerance_ratio() *
                                 parameters_.primal_feasibility_tolerance();
    std::uniform_real_distribution<double> dist(0, tolerance);
    for (ColIndex col(0); col < num_cols_; ++col) {
      bound_perturbation_[col] += dist(random_);
    }
  }
}

void RevisedSimplex::ComputeVariableValuesError() {
  SCOPED_TIME_STAT(&function_stats_);
  error_.assign(num_rows_, 0.0);
  const DenseRow& variable_values = variable_values_.GetDenseRow();
  for (ColIndex col(0); col < num_cols_; ++col) {
    const Fractional value = variable_values[col];
    compact_matrix_.ColumnAddMultipleToDenseColumn(col, -value, &error_);
  }
}

void RevisedSimplex::ComputeDirection(ColIndex col) {
  SCOPED_TIME_STAT(&function_stats_);
  DCHECK_COL_BOUNDS(col);
  basis_factorization_.RightSolveForProblemColumn(col, &direction_,
                                                  &direction_non_zero_);
  direction_infinity_norm_ = 0.0;
  for (const RowIndex row : direction_non_zero_) {
    direction_infinity_norm_ =
        std::max(direction_infinity_norm_, std::abs(direction_[row]));
  }
  IF_STATS_ENABLED(ratio_test_stats_.direction_density.Add(
      num_rows_ == 0 ? 0.0
                     : static_cast<double>(direction_non_zero_.size()) /
                           static_cast<double>(num_rows_.value())));
}

Fractional RevisedSimplex::ComputeDirectionError(ColIndex col) {
  SCOPED_TIME_STAT(&function_stats_);
  compact_matrix_.ColumnCopyToDenseColumn(col, &error_);
  for (const RowIndex row : direction_non_zero_) {
    compact_matrix_.ColumnAddMultipleToDenseColumn(col, -direction_[row],
                                                   &error_);
  }
  return InfinityNorm(error_);
}

void RevisedSimplex::SkipVariableForRatioTest(RowIndex row) {
  // Setting direction_[row] to 0.0 is an effective way to ignore the row
  // during the ratio test. The direction vector will be restored later from
  // the information in direction_ignored_position_.
  IF_STATS_ENABLED(
      ratio_test_stats_.abs_skipped_pivot.Add(std::abs(direction_[row])));
  VLOG(1) << "Skipping leaving variable with coefficient " << direction_[row];
  direction_ignored_position_.SetCoefficient(row, direction_[row]);
  direction_[row] = 0.0;
}

template <bool is_entering_reduced_cost_positive>
Fractional RevisedSimplex::GetRatio(RowIndex row) const {
  const ColIndex col = basis_[row];
  const Fractional direction = direction_[row];
  const Fractional value = variable_values_.Get(col);
  DCHECK(variables_info_.GetIsBasicBitRow().IsSet(col));
  DCHECK_NE(direction, 0.0);
  if (is_entering_reduced_cost_positive) {
    if (direction > 0.0) {
      return (upper_bound_[col] - value) / direction;
    } else {
      return (lower_bound_[col] - value) / direction;
    }
  } else {
    if (direction > 0.0) {
      return (value - lower_bound_[col]) / direction;
    } else {
      return (value - upper_bound_[col]) / direction;
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

  // Initialy, we can skip any variable with a ratio greater than
  // bound_flip_ratio since it seems to be always better to choose the
  // bound-flip over such leaving variable.
  Fractional harris_ratio = bound_flip_ratio;
  leaving_candidates->Clear();
  const Fractional threshold = parameters_.ratio_test_zero_threshold();
  for (const RowIndex row : direction_non_zero_) {
    const Fractional magnitude = std::abs(direction_[row]);
    if (magnitude < threshold) continue;
    Fractional ratio = GetRatio<is_entering_reduced_cost_positive>(row);
    // TODO(user): The perturbation is currently disabled, so no need to test
    // anything here.
    if (false && ratio < 0.0) {
      // If the variable is already pass its bound, we use the perturbed version
      // of the bound (if bound_perturbation_[basis_[row]] is not zero).
      ratio += std::abs(bound_perturbation_[basis_[row]] / direction_[row]);
    }
    if (ratio <= harris_ratio) {
      leaving_candidates->SetCoefficient(row, ratio);

      // The second std::max() makes sure harris_ratio is lower bounded by a small
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
  direction_ignored_position_.Clear();
  int stats_num_leaving_choices = 0;
  equivalent_leaving_choices_.clear();
  while (true) {
    stats_num_leaving_choices = 0;

    // We initialize current_ratio with the maximum step the entering variable
    // can take (bound-flip). Note that we do not use tolerance here.
    const Fractional entering_value = variable_values_.Get(entering_col);
    Fractional current_ratio =
        (reduced_cost > 0.0) ? entering_value - lower_bound_[entering_col]
                             : upper_bound_[entering_col] - entering_value;
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
    //
    // Note(user): As of May 2013, just checking the pivot size is not
    // preventing the algorithm to run into a singular basis in some rare cases.
    // One way to be more precise is to also take into account the norm of the
    // direction.
    if (pivot_magnitude <
        parameters_.small_pivot_threshold() * direction_infinity_norm_) {
      VLOG(1) << "Trying not to pivot by " << direction_[*leaving_row]
              << " direction_infinity_norm_ = " << direction_infinity_norm_;

      // The first countermeasure is to recompute everything to the best
      // precision we can in the hope of avoiding such a choice. Note that this
      // helps a lot on the Netlib problems.
      if (!basis_factorization_.IsRefactorized()) {
        *refactorize = true;
        return Status::OK();
      }

      // Note(user): This reduces quite a bit the number of iterations.
      // What is its impact? Is it dangerous?
      if (std::abs(direction_[*leaving_row]) <
          parameters_.minimum_acceptable_pivot()) {
        SkipVariableForRatioTest(*leaving_row);
        continue;
      }

      // TODO(user): in almost all cases, testing the pivot is not useful
      // because the two countermeasures above will be enough. Investigate
      // more. The false makes sure that this will just be compiled away.
      if (/* DISABLES CODE */ (false) &&
          /* DISABLES CODE */ (!TestPivot(entering_col, *leaving_row))) {
        SkipVariableForRatioTest(*leaving_row);
        continue;
      }

      IF_STATS_ENABLED(ratio_test_stats_.abs_tested_pivot.Add(pivot_magnitude));
    }
    break;
  }

  // Update the target bound.
  if (*leaving_row != kInvalidRow) {
    const bool is_reduced_cost_positive = (reduced_cost > 0.0);
    const bool is_leaving_coeff_positive = (direction_[*leaving_row] > 0.0);
    *target_bound = (is_reduced_cost_positive == is_leaving_coeff_positive)
                        ? upper_bound_[basis_[*leaving_row]]
                        : lower_bound_[basis_[*leaving_row]];
  }

  // Revert the temporary modification to direction_.
  // This is important! Otherwise we would propagate some artificial errors.
  for (const SparseColumn::Entry e : direction_ignored_position_) {
    direction_[e.row()] = e.coefficient();
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

template <typename Rows>
void RevisedSimplex::UpdatePrimalPhaseICosts(const Rows& rows) {
  SCOPED_TIME_STAT(&function_stats_);
  bool objective_changed = false;
  const Fractional tolerance = parameters_.primal_feasibility_tolerance();
  for (RowIndex row : rows) {
    const ColIndex col = basis_[row];
    const Fractional value = variable_values_.Get(col);

    // The primal simplex will try to minimize the cost (hence the primal
    // infeasibility).
    Fractional cost = 0.0;
    if (value > upper_bound_[col] + tolerance) {
      cost = 1.0;
    } else if (value < lower_bound_[col] - tolerance) {
      cost = -1.0;
    }
    if (current_objective_[col] != cost) {
      objective_changed = true;
    }
    current_objective_[col] = cost;
  }
  // If the objective changed, the reduced costs need to be recomputed.
  if (objective_changed) {
    reduced_costs_.ResetForNewObjective();
  }
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

  // We initialize current_ratio with the maximum step the entering variable
  // can take (bound-flip). Note that we do not use tolerance here.
  const Fractional entering_value = variable_values_.Get(entering_col);
  Fractional current_ratio = (reduced_cost > 0.0)
                                 ? entering_value - lower_bound_[entering_col]
                                 : upper_bound_[entering_col] - entering_value;
  DCHECK_GT(current_ratio, 0.0);

  std::vector<BreakPoint> breakpoints;
  const Fractional tolerance = parameters_.primal_feasibility_tolerance();
  for (RowIndex row : direction_non_zero_) {
    const Fractional direction =
        reduced_cost > 0.0 ? direction_[row] : -direction_[row];
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
    const ColIndex col = basis_[row];
    DCHECK(variables_info_.GetIsBasicBitRow().IsSet(col));

    const Fractional value = variable_values_.Get(col);
    const Fractional lower_bound = lower_bound_[col];
    const Fractional upper_bound = upper_bound_[col];
    const Fractional to_lower = (lower_bound - tolerance - value) / direction;
    const Fractional to_upper = (upper_bound + tolerance - value) / direction;

    // Enqueue the possible transitions. Note that the second tests exclude the
    // case where to_lower or to_upper are infinite.
    if (to_lower >= 0.0 && to_lower < current_ratio) {
      breakpoints.push_back(BreakPoint(row, to_lower, magnitude, lower_bound));
    }
    if (to_upper >= 0.0 && to_upper < current_ratio) {
      breakpoints.push_back(BreakPoint(row, to_upper, magnitude, upper_bound));
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
      DCHECK_GT(current_ratio, 0.0);
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
  GLOP_RETURN_ERROR_IF_NULL(leaving_row);
  GLOP_RETURN_ERROR_IF_NULL(cost_variation);

  // TODO(user): Reuse parameters_.optimization_rule() to decide if we use
  // steepest edge or the normal Dantzig pricing.
  const DenseColumn& squared_norm = dual_edge_norms_.GetEdgeSquaredNorms();
  SCOPED_TIME_STAT(&function_stats_);

  *leaving_row = kInvalidRow;
  Fractional best_price(0.0);
  const DenseColumn& squared_infeasibilities =
      variable_values_.GetPrimalSquaredInfeasibilities();
  equivalent_leaving_choices_.clear();
  for (const RowIndex row : variable_values_.GetPrimalInfeasiblePositions()) {
    const Fractional scaled_best_price = best_price * squared_norm[row];
    if (squared_infeasibilities[row] >= scaled_best_price) {
      if (squared_infeasibilities[row] == scaled_best_price) {
        DCHECK_NE(*leaving_row, kInvalidRow);
        equivalent_leaving_choices_.push_back(row);
        continue;
      }
      equivalent_leaving_choices_.clear();
      best_price = squared_infeasibilities[row] / squared_norm[row];
      *leaving_row = row;
    }
  }

  // Break the ties randomly.
  if (!equivalent_leaving_choices_.empty()) {
    equivalent_leaving_choices_.push_back(*leaving_row);
    *leaving_row =
        equivalent_leaving_choices_[std::uniform_int_distribution<int>(
            0, equivalent_leaving_choices_.size() - 1)(random_)];
  }

  // Return right away if there is no leaving variable.
  // Fill cost_variation and target_bound otherwise.
  if (*leaving_row == kInvalidRow) return Status::OK();
  const ColIndex leaving_col = basis_[*leaving_row];
  const Fractional value = variable_values_.Get(leaving_col);
  if (value < lower_bound_[leaving_col]) {
    *cost_variation = lower_bound_[leaving_col] - value;
    *target_bound = lower_bound_[leaving_col];
    DCHECK_GT(*cost_variation, 0.0);
  } else {
    *cost_variation = upper_bound_[leaving_col] - value;
    *target_bound = upper_bound_[leaving_col];
    DCHECK_LT(*cost_variation, 0.0);
  }
  return Status::OK();
}

namespace {

// Returns true if a basic variable with given cost and type is to be considered
// as a leaving candidate for the dual phase I. This utility function is used
// to keep is_dual_entering_candidate_ up to date.
bool IsDualPhaseILeavingCandidate(Fractional cost, VariableType type,
                                  Fractional threshold) {
  if (cost == 0.0) return false;
  return type == VariableType::UPPER_AND_LOWER_BOUNDED ||
         type == VariableType::FIXED_VARIABLE ||
         (type == VariableType::UPPER_BOUNDED && cost < -threshold) ||
         (type == VariableType::LOWER_BOUNDED && cost > threshold);
}

}  // namespace

void RevisedSimplex::DualPhaseIUpdatePrice(RowIndex leaving_row,
                                           ColIndex entering_col) {
  SCOPED_TIME_STAT(&function_stats_);
  const VariableTypeRow& variable_type = variables_info_.GetTypeRow();
  const Fractional threshold = parameters_.ratio_test_zero_threshold();

  // Convert the dual_pricing_vector_ from the old basis into the new one (which
  // is the same as multiplying it by an Eta matrix corresponding to the
  // direction).
  const Fractional step =
      dual_pricing_vector_[leaving_row] / direction_[leaving_row];
  for (const RowIndex row : direction_non_zero_) {
    dual_pricing_vector_[row] -= direction_[row] * step;
    is_dual_entering_candidate_.Set(
        row,
        IsDualPhaseILeavingCandidate(dual_pricing_vector_[row],
                                     variable_type[basis_[row]], threshold));
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
  is_dual_entering_candidate_.Set(
      leaving_row,
      IsDualPhaseILeavingCandidate(dual_pricing_vector_[leaving_row],
                                   variable_type[entering_col], threshold));
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
        (can_increase.IsSet(col) && reduced_cost < -tolerance)
            ? 1.0
            : (can_decrease.IsSet(col) && reduced_cost > tolerance) ? -1.0
                                                                    : 0.0;
    if (sign != dual_infeasibility_improvement_direction_[col]) {
      if (sign == 0.0) {
        --num_dual_infeasible_positions_;
      } else if (dual_infeasibility_improvement_direction_[col] == 0.0) {
        ++num_dual_infeasible_positions_;
      }
      if (!something_to_do) {
        initially_all_zero_scratchpad_.resize(num_rows_, 0.0);
        something_to_do = true;
      }
      compact_matrix_.ColumnAddMultipleToDenseColumn(
          col, sign - dual_infeasibility_improvement_direction_[col],
          &initially_all_zero_scratchpad_);
      dual_infeasibility_improvement_direction_[col] = sign;
    }
  }
  if (something_to_do) {
    const VariableTypeRow& variable_type = variables_info_.GetTypeRow();
    const Fractional threshold = parameters_.ratio_test_zero_threshold();
    basis_factorization_.RightSolveWithNonZeros(&initially_all_zero_scratchpad_,
                                                &row_index_vector_scratchpad_);
    for (const RowIndex row : row_index_vector_scratchpad_) {
      dual_pricing_vector_[row] += initially_all_zero_scratchpad_[row];
      initially_all_zero_scratchpad_[row] = 0.0;
      is_dual_entering_candidate_.Set(
          row,
          IsDualPhaseILeavingCandidate(dual_pricing_vector_[row],
                                       variable_type[basis_[row]], threshold));
    }
  }
}

Status RevisedSimplex::DualPhaseIChooseLeavingVariableRow(
    RowIndex* leaving_row, Fractional* cost_variation,
    Fractional* target_bound) {
  SCOPED_TIME_STAT(&function_stats_);
  GLOP_RETURN_ERROR_IF_NULL(leaving_row);
  GLOP_RETURN_ERROR_IF_NULL(cost_variation);

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
      dual_pricing_vector_.empty()) {
    // Recompute everything from scratch.
    num_dual_infeasible_positions_ = 0;
    dual_pricing_vector_.assign(num_rows_, 0.0);
    is_dual_entering_candidate_.ClearAndResize(num_rows_);
    dual_infeasibility_improvement_direction_.assign(num_cols_, 0.0);
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

  // TODO(user): Reuse parameters_.optimization_rule() to decide if we use
  // steepest edge or the normal Dantzig pricing.
  const DenseColumn& squared_norm = dual_edge_norms_.GetEdgeSquaredNorms();

  // Now take a leaving variable that maximizes the infeasibility variation and
  // can leave the basis while being dual-feasible.
  Fractional best_price(0.0);
  equivalent_leaving_choices_.clear();
  for (const RowIndex row : is_dual_entering_candidate_) {
    const Fractional squared_cost = Square(dual_pricing_vector_[row]);
    const Fractional scaled_best_price = best_price * squared_norm[row];
    if (squared_cost >= scaled_best_price) {
      if (squared_cost == scaled_best_price) {
        DCHECK_NE(*leaving_row, kInvalidRow);
        equivalent_leaving_choices_.push_back(row);
        continue;
      }
      equivalent_leaving_choices_.clear();
      best_price = squared_cost / squared_norm[row];
      *leaving_row = row;
    }
  }

  // Break the ties randomly.
  if (!equivalent_leaving_choices_.empty()) {
    equivalent_leaving_choices_.push_back(*leaving_row);
    *leaving_row =
        equivalent_leaving_choices_[std::uniform_int_distribution<int>(
            0, equivalent_leaving_choices_.size() - 1)(random_)];
  }

  // Returns right away if there is no leaving variable or fill the other
  // return values otherwise.
  if (*leaving_row == kInvalidRow) return Status::OK();
  *cost_variation = dual_pricing_vector_[*leaving_row];
  const ColIndex leaving_col = basis_[*leaving_row];
  if (*cost_variation < 0.0) {
    *target_bound = upper_bound_[leaving_col];
  } else {
    *target_bound = lower_bound_[leaving_col];
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
  const Fractional dual_feasibility_tolerance =
      reduced_costs_.GetDualFeasibilityTolerance();
  const VariableStatusRow& variable_status = variables_info_.GetStatusRow();
  for (const ColIndex col : cols) {
    const Fractional reduced_cost = reduced_costs[col];
    const VariableStatus status = variable_status[col];
    DCHECK(variables_info_.GetTypeRow()[col] ==
           VariableType::UPPER_AND_LOWER_BOUNDED);
    // TODO(user): refactor this as DCHECK(IsVariableBasicOrExactlyAtBound())?
    DCHECK(variable_values[col] == lower_bound_[col] ||
           variable_values[col] == upper_bound_[col] ||
           status == VariableStatus::BASIC);
    if (reduced_cost > dual_feasibility_tolerance &&
        status == VariableStatus::AT_UPPER_BOUND) {
      variables_info_.Update(col, VariableStatus::AT_LOWER_BOUND);
      changed_cols.push_back(col);
    } else if (reduced_cost < -dual_feasibility_tolerance &&
               status == VariableStatus::AT_LOWER_BOUND) {
      variables_info_.Update(col, VariableStatus::AT_UPPER_BOUND);
      changed_cols.push_back(col);
    }
  }

  if (!changed_cols.empty()) {
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
  MatrixView basis_matrix;
  basis_matrix.PopulateFromBasis(matrix_with_slack_, basis_);
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
    // TODO(user): We need to permute is_dual_entering_candidate_ too. Right
    // now, we recompute both the dual_pricing_vector_ and
    // is_dual_entering_candidate_ on each refactorization, so this don't
    // matter.
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
  const ColIndex leaving_col = basis_[leaving_row];
  const VariableStatus leaving_variable_status =
      lower_bound_[leaving_col] == upper_bound_[leaving_col]
          ? VariableStatus::FIXED_VALUE
          : target_bound == lower_bound_[leaving_col]
                ? VariableStatus::AT_LOWER_BOUND
                : VariableStatus::AT_UPPER_BOUND;
  if (variable_values_.Get(leaving_col) != target_bound) {
    ratio_test_stats_.bound_shift.Add(variable_values_.Get(leaving_col) -
                                      target_bound);
  }
  UpdateBasis(entering_col, leaving_row, leaving_variable_status);
  GLOP_RETURN_IF_ERROR(basis_factorization_.Update(
      entering_col, leaving_row, direction_non_zero_, &direction_));
  if (basis_factorization_.IsRefactorized()) {
    PermuteBasis();
  }
  return Status::OK();
}

bool RevisedSimplex::NeedsBasisRefactorization(bool refactorize) {
  if (basis_factorization_.IsRefactorized()) return false;
  if (reduced_costs_.NeedsBasisRefactorization()) return true;
  const GlopParameters::PricingRule pricing_rule =
      feasibility_phase_ ? parameters_.feasibility_rule()
                         : parameters_.optimization_rule();
  if (parameters_.use_dual_simplex()) {
    // TODO(user): Currently the dual is always using STEEPEST_EDGE.
    DCHECK_EQ(pricing_rule, GlopParameters::STEEPEST_EDGE);
    if (dual_edge_norms_.NeedsBasisRefactorization()) return true;
  } else {
    if (pricing_rule == GlopParameters::STEEPEST_EDGE &&
        primal_edge_norms_.NeedsBasisRefactorization()) {
      return true;
    }
  }
  return refactorize;
}

Status RevisedSimplex::RefactorizeBasisIfNeeded(bool* refactorize) {
  SCOPED_TIME_STAT(&function_stats_);
  if (NeedsBasisRefactorization(*refactorize)) {
    GLOP_RETURN_IF_ERROR(basis_factorization_.Refactorize());
    update_row_.Invalidate();
    PermuteBasis();
  }
  *refactorize = false;
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
Status RevisedSimplex::Minimize(TimeLimit* time_limit) {
  GLOP_RETURN_ERROR_IF_NULL(time_limit);
  Cleanup update_deterministic_time_on_return(
      [this, time_limit]() { AdvanceDeterministicTime(time_limit); });
  num_consecutive_degenerate_iterations_ = 0;
  DisplayIterationInfo();
  bool refactorize = false;

  if (feasibility_phase_) {
    // Initialize the primal phase-I objective.
    current_objective_.assign(num_cols_, 0.0);
    UpdatePrimalPhaseICosts(IntegerRange<RowIndex>(RowIndex(0), num_rows_));
  }

  while (true) {
    // TODO(user): we may loop a bit more than the actual number of iteration.
    // fix.
    IF_STATS_ENABLED(
        ScopedTimeDistributionUpdater timer(&iteration_stats_.total));
    GLOP_RETURN_IF_ERROR(RefactorizeBasisIfNeeded(&refactorize));
    if (basis_factorization_.IsRefactorized()) {
      CorrectErrorsOnVariableValues();
      DisplayIterationInfo();

      if (feasibility_phase_) {
        // Since the variable values may have been recomputed, we need to
        // recompute the primal infeasible variables and update their costs.
        UpdatePrimalPhaseICosts(IntegerRange<RowIndex>(RowIndex(0), num_rows_));
      }

      // Computing the objective at each iteration takes time, so we just
      // check the limit when the basis is refactorized.
      if (!feasibility_phase_ &&
          ComputeObjectiveValue() < primal_objective_limit_) {
        VLOG(1) << "Stopping the primal simplex because"
                << " the objective limit " << primal_objective_limit_
                << " has been reached.";
        problem_status_ = ProblemStatus::PRIMAL_FEASIBLE;
        objective_limit_reached_ = true;
        return Status::OK();
      }
    } else if (feasibility_phase_) {
      // Note that direction_non_zero_ contains the positions of the basic
      // variables whose values were updated during the last iteration.
      UpdatePrimalPhaseICosts(direction_non_zero_);
    }

    Fractional reduced_cost = 0.0;
    ColIndex entering_col = kInvalidCol;
    GLOP_RETURN_IF_ERROR(
        entering_variable_.PrimalChooseEnteringColumn(&entering_col));
    if (entering_col == kInvalidCol) {
      if (reduced_costs_.AreReducedCostsPrecise() &&
          basis_factorization_.IsRefactorized()) {
        if (feasibility_phase_) {
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
      } else {
        VLOG(1) << "Optimal reached, double checking...";
        reduced_costs_.MakeReducedCostsPrecise();
        refactorize = true;
        continue;
      }
    } else {
      reduced_cost = reduced_costs_.GetReducedCosts()[entering_col];
      DCHECK(reduced_costs_.IsValidPrimalEnteringCandidate(entering_col));

      // Solve the system B.d = a with a the entering column.
      ComputeDirection(entering_col);
      primal_edge_norms_.TestEnteringEdgeNormPrecision(
          entering_col,
          ScatteredColumnReference(direction_, direction_non_zero_));
      if (!reduced_costs_.TestEnteringReducedCostPrecision(
              entering_col,
              ScatteredColumnReference(direction_, direction_non_zero_),
              &reduced_cost)) {
        VLOG(1) << "Skipping col #" << entering_col << " whose reduced cost is "
                << reduced_cost;
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
      break;
    }

    Fractional step_length;
    RowIndex leaving_row;
    Fractional target_bound;
    if (feasibility_phase_) {
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
      if (feasibility_phase_) {
        // This shouldn't happen by construction.
        VLOG(1) << "Unbounded feasibility problem !?";
        problem_status_ = ProblemStatus::ABNORMAL;
      } else {
        VLOG(1) << "Unbounded problem.";
        problem_status_ = ProblemStatus::PRIMAL_UNBOUNDED;
        solution_primal_ray_.assign(num_cols_, 0.0);
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
    if (feasibility_phase_ && leaving_row != kInvalidRow) {
      // For phase-I we currently always set the leaving variable to its exact
      // bound even if by doing so we may take a small step in the wrong
      // direction and may increase the overall infeasibility.
      //
      // TODO(user): Investigate alternatives even if this seems to work well in
      // practice. Note that the final returned solution will have the property
      // that all non-basic variables are at their exact bound, so it is nice
      // that we do not report ProblemStatus::PRIMAL_FEASIBLE if a solution with
      // this property
      // cannot be found.
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

    variable_values_.UpdateOnPivoting(
        ScatteredColumnReference(direction_, direction_non_zero_), entering_col,
        step);
    if (leaving_row != kInvalidRow) {
      primal_edge_norms_.UpdateBeforeBasisPivot(
          entering_col, basis_[leaving_row], leaving_row,
          ScatteredColumnReference(direction_, direction_non_zero_),
          &update_row_);
      reduced_costs_.UpdateBeforeBasisPivot(entering_col, leaving_row,
                                            direction_, &update_row_);
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
      reduced_costs_.SetAndDebugCheckThatColumnIsDualFeasible(entering_col);
      IF_STATS_ENABLED(timer.AlsoUpdate(&iteration_stats_.bound_flip));
    }

    if (feasibility_phase_ && leaving_row != kInvalidRow) {
      // Set the leaving variable to its exact bound.
      variable_values_.SetNonBasicVariableValueFromStatus(leaving_col);
      reduced_costs_.SetNonBasicVariableCostToZero(
          leaving_col, &current_objective_[leaving_col]);
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
//   has all its variables boxed.
// - Pan's method, which is really fast but have no theoretical guarantee of
//   terminating and thus needs to use one of the other methods as a fallback if
//   it fails to make progress.
//
// Note that the returned status applies to the primal problem!
Status RevisedSimplex::DualMinimize(TimeLimit* time_limit) {
  Cleanup update_deterministic_time_on_return(
      [this, time_limit]() { AdvanceDeterministicTime(time_limit); });
  num_consecutive_degenerate_iterations_ = 0;
  bool refactorize = false;
  std::vector<ColIndex> bound_flip_candidates;

  // Leaving variable.
  RowIndex leaving_row;
  Fractional cost_variation;
  Fractional target_bound;

  // Entering variable.
  ColIndex entering_col;
  Fractional entering_coeff;
  Fractional ratio;

  while (true) {
    // TODO(user): we may loop a bit more than the actual number of iteration.
    // fix.
    IF_STATS_ENABLED(
        ScopedTimeDistributionUpdater timer(&iteration_stats_.total));

    const bool old_refactorize_value = refactorize;
    GLOP_RETURN_IF_ERROR(RefactorizeBasisIfNeeded(&refactorize));

    // If the basis is refactorized, we recompute all the values in order to
    // have a good precision.
    if (basis_factorization_.IsRefactorized()) {
      // We do not want to recompute the reduced costs too often, this is
      // because that may break the overall direction taken by the last steps
      // and may lead to less improvement on degenerate problems.
      //
      // During phase-I, we do want the reduced costs to be as precise as
      // possible. TODO(user): Investigate why and fix the TODO in
      // PermuteBasis().
      //
      // Reduced costs are needed by MakeBoxedVariableDualFeasible(), so if we
      // do recompute them, it is better to do that first.
      if (!feasibility_phase_ && !reduced_costs_.AreReducedCostsRecomputed() &&
          !old_refactorize_value) {
        const Fractional dual_residual_error =
            reduced_costs_.ComputeMaximumDualResidual();
        if (dual_residual_error >
            reduced_costs_.GetDualFeasibilityTolerance()) {
          VLOG(1) << "Recomputing reduced costs. Dual residual = "
                  << dual_residual_error;
          reduced_costs_.MakeReducedCostsPrecise();
        }
      } else {
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
      if (!feasibility_phase_) {
        MakeBoxedVariableDualFeasible(
            variables_info_.GetNonBasicBoxedVariables(),
            /*update_basic_values=*/false);
        variable_values_.RecomputeBasicVariableValues();
        variable_values_.ResetPrimalInfeasibilityInformation();

        // Computing the objective at each iteration takes time, so we just
        // check the limit when the basis is refactorized.
        if (ComputeObjectiveValue() > dual_objective_limit_) {
          VLOG(1) << "Stopping the dual simplex because"
                  << " the objective limit " << dual_objective_limit_
                  << " has been reached.";
          problem_status_ = ProblemStatus::DUAL_FEASIBLE;
          objective_limit_reached_ = true;
          return Status::OK();
        }
      }

      reduced_costs_.GetReducedCosts();
      DisplayIterationInfo();
    } else {
      // Updates from the previous iteration that can be skipped if we
      // recompute everything (see other case above).
      if (!feasibility_phase_) {
        // Make sure the boxed variables are dual-feasible before choosing the
        // leaving variable row.
        MakeBoxedVariableDualFeasible(bound_flip_candidates,
                                      /*update_basic_values=*/true);
        bound_flip_candidates.clear();

        // The direction_non_zero_ contains the positions for which the basic
        // variable value was changed during the previous iterations.
        variable_values_.UpdatePrimalInfeasibilityInformation(
            direction_non_zero_);
      }
    }

    if (feasibility_phase_) {
      GLOP_RETURN_IF_ERROR(DualPhaseIChooseLeavingVariableRow(
          &leaving_row, &cost_variation, &target_bound));
    } else {
      GLOP_RETURN_IF_ERROR(DualChooseLeavingVariableRow(
          &leaving_row, &cost_variation, &target_bound));
    }
    if (leaving_row == kInvalidRow) {
      if (!basis_factorization_.IsRefactorized()) {
        VLOG(1) << "Optimal reached, double checking.";
        refactorize = true;
        continue;
      }
      if (feasibility_phase_) {
        // Note that since the basis is refactorized, the variable values
        // will be recomputed at the beginning of the second phase. The boxed
        // variable values will also be corrected by
        // MakeBoxedVariableDualFeasible().
        if (num_dual_infeasible_positions_ == 0) {
          problem_status_ = ProblemStatus::DUAL_FEASIBLE;
        } else {
          problem_status_ = ProblemStatus::DUAL_INFEASIBLE;
        }
      } else {
        problem_status_ = ProblemStatus::OPTIMAL;
      }
      return Status::OK();
    }

    update_row_.ComputeUpdateRow(leaving_row);
    if (feasibility_phase_) {
      GLOP_RETURN_IF_ERROR(entering_variable_.DualPhaseIChooseEnteringColumn(
          update_row_, cost_variation, &entering_col, &entering_coeff, &ratio));
    } else {
      GLOP_RETURN_IF_ERROR(entering_variable_.DualChooseEnteringColumn(
          update_row_, cost_variation, &bound_flip_candidates, &entering_col,
          &entering_coeff, &ratio));
    }

    // No entering_col: Unbounded problem / Infeasible problem.
    if (entering_col == kInvalidCol) {
      if (!reduced_costs_.AreReducedCostsPrecise()) {
        VLOG(1) << "No entering column. Double checking...";
        refactorize = true;
        continue;
      }
      DCHECK(basis_factorization_.IsRefactorized());
      if (feasibility_phase_) {
        // This shouldn't happen by construction.
        VLOG(1) << "Unbounded dual feasibility problem !?";
        problem_status_ = ProblemStatus::ABNORMAL;
      } else {
        problem_status_ = ProblemStatus::DUAL_UNBOUNDED;
        solution_dual_ray_ = update_row_.GetUnitRowLeftInverse().dense_column;
        update_row_.RecomputeFullUpdateRow(leaving_row);
        solution_dual_ray_row_combination_.assign(num_cols_, 0.0);
        for (const ColIndex col : update_row_.GetNonZeroPositions()) {
          solution_dual_ray_row_combination_[col] =
              update_row_.GetCoefficient(col);
        }
        if (cost_variation < 0) {
          ChangeSign(&solution_dual_ray_);
          ChangeSign(&solution_dual_ray_row_combination_);
        }
      }
      return Status::OK();
    }

    // If the coefficient is too small, we recompute the reduced costs.
    if (std::abs(entering_coeff) < parameters_.dual_small_pivot_threshold() &&
        !reduced_costs_.AreReducedCostsPrecise()) {
      VLOG(1) << "Trying not to pivot by " << entering_coeff;
      refactorize = true;
      continue;
    }

    // If the reduced cost is already precise, we check with the direction_.
    // This is at least needed to avoid corner cases where
    // direction_[leaving_row] is actually 0 which causes a floating
    // point exception below.
    ComputeDirection(entering_col);
    if (std::abs(direction_[leaving_row]) <
        parameters_.minimum_acceptable_pivot()) {
      VLOG(1) << "Do not pivot by " << entering_coeff
              << " because the direction is " << direction_[leaving_row];
      refactorize = true;
      update_row_.IgnoreUpdatePosition(entering_col);
      continue;
    }

    // This test takes place after the check for optimality/feasibility because
    // when running with 0 iterations, we still want to report
    // ProblemStatus::OPTIMAL or ProblemStatus::PRIMAL_FEASIBLE if it is the
    // case at the beginning of the algorithm.
    AdvanceDeterministicTime(time_limit);
    if (num_iterations_ == parameters_.max_number_of_iterations() ||
        time_limit->LimitReached()) {
      return Status::OK();
    }

    IF_STATS_ENABLED({
      if (ratio == 0.0) {
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
    //
    // During phase I, we do not need the basic variable values at all.
    Fractional primal_step = 0.0;
    if (feasibility_phase_) {
      DualPhaseIUpdatePrice(leaving_row, entering_col);
    } else {
      primal_step =
          ComputeStepToMoveBasicVariableToBound(leaving_row, target_bound);
      variable_values_.UpdateOnPivoting(
          ScatteredColumnReference(direction_, direction_non_zero_),
          entering_col, primal_step);
    }

    reduced_costs_.UpdateBeforeBasisPivot(entering_col, leaving_row, direction_,
                                          &update_row_);
    dual_edge_norms_.UpdateBeforeBasisPivot(
        entering_col, leaving_row,
        ScatteredColumnReference(direction_, direction_non_zero_),
        update_row_.GetUnitRowLeftInverse());

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

ColIndex RevisedSimplex::SlackColIndex(RowIndex row) const {
  // TODO(user): Remove this function.
  DCHECK_ROW_BOUNDS(row);
  return first_slack_col_ + RowToColIndex(row);
}

std::string RevisedSimplex::StatString() {
  std::string result;
  result.append(iteration_stats_.StatString());
  result.append(ratio_test_stats_.StatString());
  result.append(entering_variable_.StatString());
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
  if (FLAGS_simplex_display_stats) {
    fprintf(stderr, "%s", StatString().c_str());
    fprintf(stderr, "%s", GetPrettySolverStats().c_str());
  }
}

Fractional RevisedSimplex::ComputeObjectiveValue() const {
  SCOPED_TIME_STAT(&function_stats_);
  return PreciseScalarProduct(current_objective_,
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
  random_.seed(parameters_.random_seed());
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
  variable_values_.SetBoundTolerance(
      parameters_.primal_feasibility_tolerance());
}

void RevisedSimplex::DisplayIterationInfo() const {
  if (VLOG_IS_ON(1)) {
    const int iter = feasibility_phase_
                         ? num_iterations_
                         : num_iterations_ - num_feasibility_iterations_;
    // Note that in the dual phase II, ComputeObjectiveValue() is also computing
    // the dual objective even if it uses the variable values. This is because
    // if we modify the bounds to make the problem primal-feasible, we are at
    // the optimal and hence the two objectives are the same.
    const Fractional objective =
        !feasibility_phase_
            ? ComputeInitialProblemObjectiveValue()
            : (parameters_.use_dual_simplex()
                   ? reduced_costs_.ComputeSumOfDualInfeasibilities()
                   : variable_values_.ComputeSumOfPrimalInfeasibilities());
    VLOG(1) << (feasibility_phase_ ? "Feasibility" : "Optimization")
            << " phase, iteration # " << iter
            << ", objective = " << StringPrintf("%.15E", objective);
  }
}

void RevisedSimplex::DisplayErrors() const {
  if (VLOG_IS_ON(1)) {
    VLOG(1) << "Primal infeasibility (bounds) = "
            << variable_values_.ComputeMaximumPrimalInfeasibility();
    VLOG(1) << "Primal residual |A.x - b| = "
            << variable_values_.ComputeMaximumPrimalResidual();
    VLOG(1) << "Dual infeasibility (reduced costs) = "
            << reduced_costs_.ComputeMaximumDualInfeasibility();
    VLOG(1) << "Dual residual |c_B - y.B| = "
            << reduced_costs_.ComputeMaximumDualResidual();
  }
}

namespace {

std::string StringifyMonomialWithFlags(const Fractional a, const std::string& x) {
  return StringifyMonomial(a, x, FLAGS_simplex_display_numbers_as_fractions);
}

// Returns a std::string representing the rational approximation of x or a decimal
// approximation of x according to FLAGS_simplex_display_numbers_as_fractions.
std::string StringifyWithFlags(const Fractional x) {
  return Stringify(x, FLAGS_simplex_display_numbers_as_fractions);
}

}  // namespace

std::string RevisedSimplex::SimpleVariableInfo(ColIndex col) const {
  std::string output;
  VariableType variable_type = variables_info_.GetTypeRow()[col];
  VariableStatus variable_status = variables_info_.GetStatusRow()[col];
  StringAppendF(&output, "%d (%s) = %s, %s, %s, [%s,%s]", col.value(),
                variable_name_[col].c_str(),
                StringifyWithFlags(variable_values_.Get(col)).c_str(),
                GetVariableStatusString(variable_status).c_str(),
                GetVariableTypeString(variable_type).c_str(),
                StringifyWithFlags(lower_bound_[col]).c_str(),
                StringifyWithFlags(upper_bound_[col]).c_str());
  return output;
}

void RevisedSimplex::DisplayInfoOnVariables() const {
  if (VLOG_IS_ON(3)) {
    for (ColIndex col(0); col < num_cols_; ++col) {
      const Fractional variable_value = variable_values_.Get(col);
      const Fractional objective_coefficient = current_objective_[col];
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
    for (ColIndex col(0); col < num_cols_; ++col) {
      switch (variable_type[col]) {
        case VariableType::UNCONSTRAINED:
          break;
        case VariableType::LOWER_BOUNDED:
          VLOG(3) << variable_name_[col]
                  << " >= " << StringifyWithFlags(lower_bound_[col]) << ";";
          break;
        case VariableType::UPPER_BOUNDED:
          VLOG(3) << variable_name_[col]
                  << " <= " << StringifyWithFlags(upper_bound_[col]) << ";";
          break;
        case VariableType::UPPER_AND_LOWER_BOUNDED:
          VLOG(3) << StringifyWithFlags(lower_bound_[col])
                  << " <= " << variable_name_[col]
                  << " <= " << StringifyWithFlags(upper_bound_[col]) << ";";
          break;
        case VariableType::FIXED_VARIABLE:
          VLOG(3) << variable_name_[col] << " = "
                  << StringifyWithFlags(lower_bound_[col]) << ";";
          break;
        default:                           // This should never happen.
          LOG(DFATAL) << "Column " << col
                      << " has no meaningful status.";
          break;
      }
    }
  }
}

ITIVector<RowIndex, SparseRow> RevisedSimplex::ComputeDictionary() {
  ITIVector<RowIndex, SparseRow> dictionary(num_rows_.value());
  for (ColIndex col(0); col < num_cols_; ++col) {
    ComputeDirection(col);
    for (const RowIndex row : direction_non_zero_) {
      dictionary[row].SetCoefficient(col, direction_[row]);
    }
  }
  return dictionary;
}

void RevisedSimplex::DisplayRevisedSimplexDebugInfo() {
  if (VLOG_IS_ON(3)) {
    // This function has a complexity in O(num_non_zeros_in_matrix).
    DisplayInfoOnVariables();

    std::string output = "z = " + StringifyWithFlags(ComputeObjectiveValue());
    const DenseRow& reduced_costs = reduced_costs_.GetReducedCosts();
    for (const ColIndex col : variables_info_.GetNotBasicBitRow()) {
      StrAppend(&output, StringifyMonomialWithFlags(reduced_costs[col],
                                                          variable_name_[col]));
    }
    VLOG(3) << output << ";";

    const RevisedSimplexDictionary dictionary(this);
    RowIndex r(0);
    for (const SparseRow& row : dictionary) {
      output.clear();
      ColIndex basic_col = basis_[r];
      StrAppend(&output, variable_name_[basic_col], " = ",
                      StringifyWithFlags(variable_values_.Get(basic_col)));
      for (const SparseRowEntry e : row) {
        if (e.col() != basic_col) {
          StrAppend(&output,
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
      const Fractional coeff = current_objective_[col];
      has_objective |= (coeff != 0.0);
      StrAppend(&output,
                      StringifyMonomialWithFlags(coeff, variable_name_[col]));
    }
    if (!has_objective) {
      StrAppend(&output, " 0");
    }
    VLOG(3) << output << ";";
    for (RowIndex row(0); row < num_rows_; ++row) {
      output = "";
      for (ColIndex col(0); col < num_cols_; ++col) {
        StrAppend(
            &output, StringifyMonomialWithFlags(
                         matrix_with_slack_.column(col).LookUpCoefficient(row),
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
