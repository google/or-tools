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

#include "ortools/glop/reduced_costs.h"

#include <random>

#ifdef OMP
#include <omp.h>
#endif

#include "ortools/lp_data/lp_utils.h"

namespace operations_research {
namespace glop {

ReducedCosts::ReducedCosts(const CompactSparseMatrix& matrix,
                           const DenseRow& objective,
                           const RowToColMapping& basis,
                           const VariablesInfo& variables_info,
                           const BasisFactorization& basis_factorization,
                           absl::BitGenRef random)
    : matrix_(matrix),
      objective_(objective),
      basis_(basis),
      variables_info_(variables_info),
      basis_factorization_(basis_factorization),
      random_(random),
      parameters_(),
      stats_(),
      must_refactorize_basis_(false),
      recompute_basic_objective_left_inverse_(true),
      recompute_basic_objective_(true),
      recompute_reduced_costs_(true),
      are_reduced_costs_precise_(false),
      are_reduced_costs_recomputed_(false),
      basic_objective_(),
      reduced_costs_(),
      basic_objective_left_inverse_(),
      dual_feasibility_tolerance_() {}

bool ReducedCosts::NeedsBasisRefactorization() const {
  return must_refactorize_basis_;
}

Fractional ReducedCosts::TestEnteringReducedCostPrecision(
    ColIndex entering_col, const ScatteredColumn& direction) {
  SCOPED_TIME_STAT(&stats_);
  if (recompute_basic_objective_) {
    ComputeBasicObjective();
  }
  const Fractional old_reduced_cost = reduced_costs_[entering_col];
  const Fractional precise_reduced_cost =
      objective_[entering_col] + cost_perturbations_[entering_col] -
      PreciseScalarProduct(basic_objective_, direction);

  // Update the reduced cost of the entering variable with the precise version.
  reduced_costs_[entering_col] = precise_reduced_cost;

  // At this point, we have an entering variable that will move the objective in
  // the good direction. We check the precision of the reduced cost and edges
  // norm, but even if they are imprecise, we finish this pivot and will
  // recompute them during the next call to ChooseEnteringColumn().

  // Estimate the accuracy of the reduced costs using the entering variable.
  if (!recompute_reduced_costs_) {
    const Fractional estimated_reduced_costs_accuracy =
        old_reduced_cost - precise_reduced_cost;
    const Fractional scale =
        (std::abs(precise_reduced_cost) <= 1.0) ? 1.0 : precise_reduced_cost;
    stats_.reduced_costs_accuracy.Add(estimated_reduced_costs_accuracy / scale);
    if (std::abs(estimated_reduced_costs_accuracy) / scale >
        parameters_.recompute_reduced_costs_threshold()) {
      VLOG(1) << "Recomputing reduced costs, value = " << precise_reduced_cost
              << " error = "
              << std::abs(precise_reduced_cost - old_reduced_cost);
      MakeReducedCostsPrecise();
    }
  }

  return precise_reduced_cost;
}

Fractional ReducedCosts::ComputeMaximumDualResidual() {
  SCOPED_TIME_STAT(&stats_);
  Fractional dual_residual_error(0.0);
  const RowIndex num_rows = matrix_.num_rows();
  const DenseRow& dual_values = Transpose(GetDualValues());
  for (RowIndex row(0); row < num_rows; ++row) {
    const ColIndex basic_col = basis_[row];
    const Fractional residual =
        objective_[basic_col] + cost_perturbations_[basic_col] -
        matrix_.ColumnScalarProduct(basic_col, dual_values);
    dual_residual_error = std::max(dual_residual_error, std::abs(residual));
  }
  return dual_residual_error;
}

Fractional ReducedCosts::ComputeMaximumDualInfeasibility() {
  SCOPED_TIME_STAT(&stats_);

  // Trigger a recomputation if needed so that reduced_costs_ is valid.
  GetReducedCosts();

  Fractional maximum_dual_infeasibility = 0.0;
  const DenseBitRow& can_decrease = variables_info_.GetCanDecreaseBitRow();
  const DenseBitRow& can_increase = variables_info_.GetCanIncreaseBitRow();
  for (const ColIndex col : variables_info_.GetIsRelevantBitRow()) {
    const Fractional rc = reduced_costs_[col];
    if ((can_increase.IsSet(col) && rc < 0.0) ||
        (can_decrease.IsSet(col) && rc > 0.0)) {
      maximum_dual_infeasibility =
          std::max(maximum_dual_infeasibility, std::abs(rc));
    }
  }
  return maximum_dual_infeasibility;
}

Fractional ReducedCosts::ComputeMaximumDualInfeasibilityOnNonBoxedVariables() {
  SCOPED_TIME_STAT(&stats_);

  // Trigger a recomputation if needed so that reduced_costs_ is valid.
  GetReducedCosts();

  Fractional maximum_dual_infeasibility = 0.0;
  const DenseBitRow& can_decrease = variables_info_.GetCanDecreaseBitRow();
  const DenseBitRow& can_increase = variables_info_.GetCanIncreaseBitRow();
  const DenseBitRow& is_boxed = variables_info_.GetNonBasicBoxedVariables();
  for (const ColIndex col : variables_info_.GetNotBasicBitRow()) {
    if (is_boxed[col]) continue;
    const Fractional rc = reduced_costs_[col];
    if ((can_increase.IsSet(col) && rc < 0.0) ||
        (can_decrease.IsSet(col) && rc > 0.0)) {
      maximum_dual_infeasibility =
          std::max(maximum_dual_infeasibility, std::abs(rc));
    }
  }
  return maximum_dual_infeasibility;
}

Fractional ReducedCosts::ComputeSumOfDualInfeasibilities() {
  SCOPED_TIME_STAT(&stats_);

  // Trigger a recomputation if needed so that reduced_costs_ is valid.
  GetReducedCosts();

  Fractional dual_infeasibility_sum = 0.0;
  const DenseBitRow& can_decrease = variables_info_.GetCanDecreaseBitRow();
  const DenseBitRow& can_increase = variables_info_.GetCanIncreaseBitRow();
  for (const ColIndex col : variables_info_.GetIsRelevantBitRow()) {
    const Fractional rc = reduced_costs_[col];
    if ((can_increase.IsSet(col) && rc < 0.0) ||
        (can_decrease.IsSet(col) && rc > 0.0)) {
      dual_infeasibility_sum += std::abs(std::abs(rc));
    }
  }
  return dual_infeasibility_sum;
}

void ReducedCosts::UpdateBeforeBasisPivot(ColIndex entering_col,
                                          RowIndex leaving_row,
                                          const ScatteredColumn& direction,
                                          UpdateRow* update_row) {
  SCOPED_TIME_STAT(&stats_);
  const ColIndex leaving_col = basis_[leaving_row];
  DCHECK(!variables_info_.GetIsBasicBitRow().IsSet(entering_col));
  DCHECK(variables_info_.GetIsBasicBitRow().IsSet(leaving_col));

  // If we are recomputing everything when requested, no need to update.
  if (!recompute_reduced_costs_) {
    UpdateReducedCosts(entering_col, leaving_col, leaving_row,
                       direction[leaving_row], update_row);
  }

  // Note that it is important to update basic_objective_ AFTER calling
  // UpdateReducedCosts().
  UpdateBasicObjective(entering_col, leaving_row);
}

void ReducedCosts::SetNonBasicVariableCostToZero(ColIndex col,
                                                 Fractional* current_cost) {
  DCHECK_NE(variables_info_.GetStatusRow()[col], VariableStatus::BASIC);
  DCHECK_EQ(current_cost, &objective_[col]);
  reduced_costs_[col] -= objective_[col];
  *current_cost = 0.0;
}

void ReducedCosts::SetParameters(const GlopParameters& parameters) {
  parameters_ = parameters;
}

void ReducedCosts::ResetForNewObjective() {
  SCOPED_TIME_STAT(&stats_);
  recompute_basic_objective_ = true;
  recompute_basic_objective_left_inverse_ = true;
  are_reduced_costs_precise_ = false;
  SetRecomputeReducedCostsAndNotifyWatchers();
}

void ReducedCosts::UpdateDataOnBasisPermutation() {
  SCOPED_TIME_STAT(&stats_);
  recompute_basic_objective_ = true;
  recompute_basic_objective_left_inverse_ = true;
}

void ReducedCosts::MakeReducedCostsPrecise() {
  SCOPED_TIME_STAT(&stats_);
  if (are_reduced_costs_precise_) return;
  must_refactorize_basis_ = true;
  recompute_basic_objective_left_inverse_ = true;
  SetRecomputeReducedCostsAndNotifyWatchers();
}

void ReducedCosts::PerturbCosts() {
  SCOPED_TIME_STAT(&stats_);
  VLOG(1) << "Perturbing the costs ... ";

  Fractional max_cost_magnitude = 0.0;
  const ColIndex structural_size =
      matrix_.num_cols() - RowToColIndex(matrix_.num_rows());
  for (ColIndex col(0); col < structural_size; ++col) {
    max_cost_magnitude =
        std::max(max_cost_magnitude, std::abs(objective_[col]));
  }

  cost_perturbations_.AssignToZero(matrix_.num_cols());
  for (ColIndex col(0); col < structural_size; ++col) {
    const Fractional objective = objective_[col];
    const Fractional magnitude =
        (1.0 + std::uniform_real_distribution<double>()(random_)) *
        (parameters_.relative_cost_perturbation() * std::abs(objective) +
         parameters_.relative_max_cost_perturbation() * max_cost_magnitude);
    DCHECK_GE(magnitude, 0.0);

    // The perturbation direction is such that a dual-feasible solution stays
    // feasible. This is important.
    const VariableType type = variables_info_.GetTypeRow()[col];
    switch (type) {
      case VariableType::UNCONSTRAINED:
        break;
      case VariableType::FIXED_VARIABLE:
        break;
      case VariableType::LOWER_BOUNDED:
        cost_perturbations_[col] = magnitude;
        break;
      case VariableType::UPPER_BOUNDED:
        cost_perturbations_[col] = -magnitude;
        break;
      case VariableType::UPPER_AND_LOWER_BOUNDED:
        // Here we don't necessarily maintain the dual-feasibility of a dual
        // feasible solution, however we can always shift the variable to its
        // other bound (because it is boxed) to restore dual-feasiblity. This is
        // done by MakeBoxedVariableDualFeasible() at the end of the dual
        // phase-I algorithm.
        if (objective > 0.0) {
          cost_perturbations_[col] = magnitude;
        } else if (objective < 0.0) {
          cost_perturbations_[col] = -magnitude;
        }
        break;
    }
  }
}

void ReducedCosts::ShiftCostIfNeeded(bool increasing_rc_is_needed,
                                     ColIndex col) {
  SCOPED_TIME_STAT(&stats_);

  // We always want a minimum step size, so if we have a negative step or
  // a step that is really small, we will shift the cost of the given column.
  const Fractional minimum_delta =
      parameters_.degenerate_ministep_factor() * dual_feasibility_tolerance_;
  if (increasing_rc_is_needed && reduced_costs_[col] <= -minimum_delta) return;
  if (!increasing_rc_is_needed && reduced_costs_[col] >= minimum_delta) return;

  const Fractional delta =
      increasing_rc_is_needed ? minimum_delta : -minimum_delta;
  IF_STATS_ENABLED(stats_.cost_shift.Add(reduced_costs_[col] + delta));
  cost_perturbations_[col] -= reduced_costs_[col] + delta;
  reduced_costs_[col] = -delta;
  has_cost_shift_ = true;
}

bool ReducedCosts::StepIsDualDegenerate(bool increasing_rc_is_needed,
                                        ColIndex col) {
  if (increasing_rc_is_needed && reduced_costs_[col] >= 0.0) return true;
  if (!increasing_rc_is_needed && reduced_costs_[col] <= 0.0) return true;
  return false;
}

void ReducedCosts::ClearAndRemoveCostShifts() {
  SCOPED_TIME_STAT(&stats_);
  has_cost_shift_ = false;
  cost_perturbations_.AssignToZero(matrix_.num_cols());
  recompute_basic_objective_ = true;
  recompute_basic_objective_left_inverse_ = true;
  are_reduced_costs_precise_ = false;
  SetRecomputeReducedCostsAndNotifyWatchers();
}

const DenseRow& ReducedCosts::GetFullReducedCosts() {
  SCOPED_TIME_STAT(&stats_);
  if (!are_reduced_costs_recomputed_) {
    SetRecomputeReducedCostsAndNotifyWatchers();
  }
  return GetReducedCosts();
}

const DenseRow& ReducedCosts::GetReducedCosts() {
  SCOPED_TIME_STAT(&stats_);
  if (basis_factorization_.IsRefactorized()) {
    must_refactorize_basis_ = false;
  }
  if (recompute_reduced_costs_) {
    ComputeReducedCosts();
  }
  return reduced_costs_;
}

const DenseColumn& ReducedCosts::GetDualValues() {
  SCOPED_TIME_STAT(&stats_);
  ComputeBasicObjectiveLeftInverse();
  return Transpose(basic_objective_left_inverse_.values);
}

void ReducedCosts::ComputeBasicObjective() {
  SCOPED_TIME_STAT(&stats_);
  const ColIndex num_cols_in_basis = RowToColIndex(matrix_.num_rows());
  cost_perturbations_.resize(matrix_.num_cols(), 0.0);
  basic_objective_.resize(num_cols_in_basis, 0.0);
  for (ColIndex col(0); col < num_cols_in_basis; ++col) {
    const ColIndex basis_col = basis_[ColToRowIndex(col)];
    basic_objective_[col] =
        objective_[basis_col] + cost_perturbations_[basis_col];
  }
  recompute_basic_objective_ = false;
  recompute_basic_objective_left_inverse_ = true;
}

void ReducedCosts::ComputeReducedCosts() {
  SCOPED_TIME_STAT(&stats_);
  if (recompute_basic_objective_left_inverse_) {
    ComputeBasicObjectiveLeftInverse();
  }
  Fractional dual_residual_error(0.0);
  const ColIndex num_cols = matrix_.num_cols();

  reduced_costs_.resize(num_cols, 0.0);
  const DenseBitRow& is_basic = variables_info_.GetIsBasicBitRow();
#ifdef OMP
  const int num_omp_threads = parameters_.num_omp_threads();
#else
  const int num_omp_threads = 1;
#endif
  if (num_omp_threads == 1) {
    for (ColIndex col(0); col < num_cols; ++col) {
      reduced_costs_[col] = objective_[col] + cost_perturbations_[col] -
                            matrix_.ColumnScalarProduct(
                                col, basic_objective_left_inverse_.values);

      // We also compute the dual residual error y.B - c_B.
      if (is_basic.IsSet(col)) {
        dual_residual_error =
            std::max(dual_residual_error, std::abs(reduced_costs_[col]));
      }
    }
  } else {
#ifdef OMP
    // In the multi-threaded case, perform the same computation as in the
    // single-threaded case above.
    std::vector<Fractional> thread_local_dual_residual_error(num_omp_threads,
                                                             0.0);
    const int parallel_loop_size = num_cols.value();
#pragma omp parallel for num_threads(num_omp_threads)
    for (int i = 0; i < parallel_loop_size; i++) {
      const ColIndex col(i);
      reduced_costs_[col] = objective_[col] + objective_perturbation_[col] -
                            matrix_.ColumnScalarProduct(
                                col, basic_objective_left_inverse_.values);

      if (is_basic.IsSet(col)) {
        thread_local_dual_residual_error[omp_get_thread_num()] =
            std::max(thread_local_dual_residual_error[omp_get_thread_num()],
                     std::abs(reduced_costs_[col]));
      }
    }
    // end of omp parallel for
    for (int i = 0; i < num_omp_threads; i++) {
      dual_residual_error =
          std::max(dual_residual_error, thread_local_dual_residual_error[i]);
    }
#endif  // OMP
  }

  deterministic_time_ +=
      DeterministicTimeForFpOperations(matrix_.num_entries().value());
  recompute_reduced_costs_ = false;
  are_reduced_costs_recomputed_ = true;
  are_reduced_costs_precise_ = basis_factorization_.IsRefactorized();

  // It is not resonable to have a dual tolerance lower than the current
  // dual_residual_error, otherwise we may never terminate (This is happening on
  // dfl001.mps with a low dual_feasibility_tolerance). Note that since we
  // recompute the reduced costs with maximum precision before really exiting,
  // it is fine to do a couple of iterations with a high zero tolerance.
  dual_feasibility_tolerance_ = parameters_.dual_feasibility_tolerance();
  if (dual_residual_error > dual_feasibility_tolerance_) {
    VLOG(2) << "Changing dual_feasibility_tolerance to " << dual_residual_error;
    dual_feasibility_tolerance_ = dual_residual_error;
  }
}

void ReducedCosts::ComputeBasicObjectiveLeftInverse() {
  SCOPED_TIME_STAT(&stats_);
  if (recompute_basic_objective_) {
    ComputeBasicObjective();
  }
  basic_objective_left_inverse_.values = basic_objective_;
  basic_objective_left_inverse_.non_zeros.clear();
  basis_factorization_.LeftSolve(&basic_objective_left_inverse_);
  recompute_basic_objective_left_inverse_ = false;
  IF_STATS_ENABLED(stats_.basic_objective_left_inverse_density.Add(
      Density(basic_objective_left_inverse_.values)));

  // TODO(user): Estimate its accuracy by a few scalar products, and refactorize
  // if it is above a threshold?
}

// Note that the update is such than the entering reduced cost is always set to
// 0.0. In particular, because of this we can step in the wrong direction for
// the dual method if the reduced cost is slightly infeasible.
void ReducedCosts::UpdateReducedCosts(ColIndex entering_col,
                                      ColIndex leaving_col,
                                      RowIndex leaving_row, Fractional pivot,
                                      UpdateRow* update_row) {
  DCHECK_NE(entering_col, leaving_col);
  DCHECK_NE(pivot, 0.0);
  if (recompute_reduced_costs_) return;

  // Note that this is precise because of the CheckPrecision().
  const Fractional entering_reduced_cost = reduced_costs_[entering_col];

  // Nothing to do if the entering reduced cost is 0.0.
  // This correspond to a dual degenerate pivot.
  if (entering_reduced_cost == 0.0) {
    VLOG(2) << "Reduced costs didn't change.";

    // TODO(user): the reduced costs may still be "precise" in this case, but
    // other parts of the code assume that if they are precise then the basis
    // was just refactorized in order to recompute them which is not the case
    // here. Clean this up.
    are_reduced_costs_precise_ = false;
    return;
  }

  are_reduced_costs_recomputed_ = false;
  are_reduced_costs_precise_ = false;
  update_row->ComputeUpdateRow(leaving_row);
  SCOPED_TIME_STAT(&stats_);

  // Update the leaving variable reduced cost.
  // '-pivot' is the value of the entering_edge at 'leaving_row'.
  // The edge of the 'leaving_col' in the new basis is equal to
  // 'entering_edge / -pivot'.
  const Fractional new_leaving_reduced_cost = entering_reduced_cost / -pivot;
  for (const ColIndex col : update_row->GetNonZeroPositions()) {
    const Fractional coeff = update_row->GetCoefficient(col);
    reduced_costs_[col] += new_leaving_reduced_cost * coeff;
  }
  reduced_costs_[leaving_col] = new_leaving_reduced_cost;

  // In the dual, since we compute the update before selecting the entering
  // variable, this cost is still in the update_position_list, so we make sure
  // it is 0 here.
  reduced_costs_[entering_col] = 0.0;
}

bool ReducedCosts::IsValidPrimalEnteringCandidate(ColIndex col) const {
  const Fractional reduced_cost = reduced_costs_[col];
  const DenseBitRow& can_decrease = variables_info_.GetCanDecreaseBitRow();
  const DenseBitRow& can_increase = variables_info_.GetCanIncreaseBitRow();
  const Fractional tolerance = dual_feasibility_tolerance_;
  return (can_increase.IsSet(col) && (reduced_cost < -tolerance)) ||
         (can_decrease.IsSet(col) && (reduced_cost > tolerance));
}

void ReducedCosts::UpdateBasicObjective(ColIndex entering_col,
                                        RowIndex leaving_row) {
  SCOPED_TIME_STAT(&stats_);
  basic_objective_[RowToColIndex(leaving_row)] =
      objective_[entering_col] + cost_perturbations_[entering_col];
  recompute_basic_objective_left_inverse_ = true;
}

void ReducedCosts::SetRecomputeReducedCostsAndNotifyWatchers() {
  recompute_reduced_costs_ = true;
  for (bool* watcher : watchers_) *watcher = true;
}

PrimalPrices::PrimalPrices(absl::BitGenRef random,
                           const VariablesInfo& variables_info,
                           PrimalEdgeNorms* primal_edge_norms,
                           ReducedCosts* reduced_costs)
    : prices_(random),
      variables_info_(variables_info),
      primal_edge_norms_(primal_edge_norms),
      reduced_costs_(reduced_costs) {
  reduced_costs_->AddRecomputationWatcher(&recompute_);
  primal_edge_norms->AddRecomputationWatcher(&recompute_);
}

void PrimalPrices::UpdateBeforeBasisPivot(ColIndex entering_col,
                                          UpdateRow* update_row) {
  // If we are recomputing everything when requested, no need to update.
  if (recompute_) return;

  // Note that the set of positions works because both the reduced costs
  // and the primal edge norms are updated on the same positions which are
  // given by the update_row.
  UpdateEnteringCandidates</*from_clean_state=*/false>(
      update_row->GetNonZeroPositions());
}

void PrimalPrices::RecomputePriceAt(ColIndex col) {
  if (recompute_) return;
  if (reduced_costs_->IsValidPrimalEnteringCandidate(col)) {
    const DenseRow& squared_norms = primal_edge_norms_->GetSquaredNorms();
    const DenseRow& reduced_costs = reduced_costs_->GetReducedCosts();
    DCHECK_NE(0.0, squared_norms[col]);
    prices_.AddOrUpdate(col, Square(reduced_costs[col]) / squared_norms[col]);
  } else {
    prices_.Remove(col);
  }
}

void PrimalPrices::SetAndDebugCheckThatColumnIsDualFeasible(ColIndex col) {
  // If we need a recomputation, we cannot assumes that the reduced costs are
  // valid until we are about to recompute the prices.
  if (recompute_) return;

  DCHECK(!reduced_costs_->IsValidPrimalEnteringCandidate(col));
  prices_.Remove(col);
}

ColIndex PrimalPrices::GetBestEnteringColumn() {
  if (recompute_) {
    const DenseRow& reduced_costs = reduced_costs_->GetReducedCosts();
    prices_.ClearAndResize(reduced_costs.size());
    UpdateEnteringCandidates</*from_clean_state=*/true>(
        variables_info_.GetIsRelevantBitRow());
    recompute_ = false;
  }
  return prices_.GetMaximum();
}

// A variable is an entering candidate if it can move in a direction that
// minimizes the objective. That is, its value needs to increase if its
// reduced cost is negative or it needs to decrease if its reduced cost is
// positive (see the IsValidPrimalEnteringCandidate() function). Note that
// this is the same as a dual-infeasible variable.
template <bool from_clean_state, typename ColumnsToUpdate>
void PrimalPrices::UpdateEnteringCandidates(const ColumnsToUpdate& cols) {
  const Fractional tolerance = reduced_costs_->GetDualFeasibilityTolerance();
  const DenseBitRow& can_decrease = variables_info_.GetCanDecreaseBitRow();
  const DenseBitRow& can_increase = variables_info_.GetCanIncreaseBitRow();
  const DenseRow& squared_norms = primal_edge_norms_->GetSquaredNorms();
  const DenseRow& reduced_costs = reduced_costs_->GetReducedCosts();
  for (const ColIndex col : cols) {
    const Fractional reduced_cost = reduced_costs[col];

    // Optimization for speed (The function is about 30% faster than the code in
    // IsValidPrimalEnteringCandidate() or a switch() on variable_status[col]).
    // This relies on the fact that (double1 > double2) returns a 1 or 0 result
    // when converted to an int. It also uses an XOR (which appears to be
    // faster) since the two conditions on the reduced cost are exclusive.
    const bool is_dual_infeasible = Bitset64<ColIndex>::ConditionalXorOfTwoBits(
        col, reduced_cost > tolerance, can_decrease, reduced_cost < -tolerance,
        can_increase);
    if (is_dual_infeasible) {
      DCHECK(reduced_costs_->IsValidPrimalEnteringCandidate(col));
      const Fractional price = Square(reduced_cost) / squared_norms[col];
      prices_.AddOrUpdate(col, price);
    } else {
      DCHECK(!reduced_costs_->IsValidPrimalEnteringCandidate(col));
      if (!from_clean_state) prices_.Remove(col);
    }
  }
}

}  // namespace glop
}  // namespace operations_research
