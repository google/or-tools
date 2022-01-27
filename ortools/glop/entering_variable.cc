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

#include "ortools/glop/entering_variable.h"

#include <queue>

#include "ortools/base/timer.h"
#include "ortools/lp_data/lp_utils.h"
#include "ortools/port/proto_utils.h"

namespace operations_research {
namespace glop {

EnteringVariable::EnteringVariable(const VariablesInfo& variables_info,
                                   absl::BitGenRef random,
                                   ReducedCosts* reduced_costs)
    : variables_info_(variables_info),
      random_(random),
      reduced_costs_(reduced_costs),
      parameters_() {}

Status EnteringVariable::DualChooseEnteringColumn(
    bool nothing_to_recompute, const UpdateRow& update_row,
    Fractional cost_variation, std::vector<ColIndex>* bound_flip_candidates,
    ColIndex* entering_col) {
  GLOP_RETURN_ERROR_IF_NULL(entering_col);
  const DenseRow& update_coefficient = update_row.GetCoefficients();
  const DenseRow& reduced_costs = reduced_costs_->GetReducedCosts();
  SCOPED_TIME_STAT(&stats_);

  breakpoints_.clear();
  breakpoints_.reserve(update_row.GetNonZeroPositions().size());
  const DenseBitRow& can_decrease = variables_info_.GetCanDecreaseBitRow();
  const DenseBitRow& can_increase = variables_info_.GetCanIncreaseBitRow();
  const DenseBitRow& is_boxed = variables_info_.GetNonBasicBoxedVariables();

  // If everything has the best possible precision currently, we ignore
  // low coefficients. This make sure we will never choose a pivot too small. It
  // however can degrade the dual feasibility of the solution, but we can always
  // fix that later.
  //
  // TODO(user): It is unclear if this is a good idea, but the primal simplex
  // have pretty good/stable behavior with a similar logic. Experiment seems
  // to show that this works well with the dual too.
  const Fractional threshold = nothing_to_recompute
                                   ? parameters_.minimum_acceptable_pivot()
                                   : parameters_.ratio_test_zero_threshold();

  // Harris ratio test. See below for more explanation. Here this is used to
  // prune the first pass by not enqueueing ColWithRatio for columns that have
  // a ratio greater than the current harris_ratio.
  const Fractional harris_tolerance =
      parameters_.harris_tolerance_ratio() *
      reduced_costs_->GetDualFeasibilityTolerance();
  Fractional harris_ratio = std::numeric_limits<Fractional>::max();

  // Like for the primal, we always allow a positive ministep, even if a
  // variable is already infeasible by more than the tolerance.
  const Fractional minimum_delta =
      parameters_.degenerate_ministep_factor() *
      reduced_costs_->GetDualFeasibilityTolerance();

  num_operations_ += 10 * update_row.GetNonZeroPositions().size();
  for (const ColIndex col : update_row.GetNonZeroPositions()) {
    // We will add ratio * coeff to this column with a ratio positive or zero.
    // cost_variation makes sure the leaving variable will be dual-feasible
    // (its update coeff is sign(cost_variation) * 1.0).
    const Fractional coeff = (cost_variation > 0.0) ? update_coefficient[col]
                                                    : -update_coefficient[col];

    // In this case, at some point the reduced cost will be positive if not
    // already, and the column will be dual-infeasible.
    if (can_decrease.IsSet(col) && coeff > threshold) {
      if (!is_boxed[col]) {
        if (-reduced_costs[col] > harris_ratio * coeff) continue;
        harris_ratio = std::min(
            harris_ratio, (-reduced_costs[col] + harris_tolerance) / coeff);
        harris_ratio = std::max(minimum_delta / coeff, harris_ratio);
      }
      breakpoints_.push_back(ColWithRatio(col, -reduced_costs[col], coeff));
      continue;
    }

    // In this case, at some point the reduced cost will be negative if not
    // already, and the column will be dual-infeasible.
    if (can_increase.IsSet(col) && coeff < -threshold) {
      if (!is_boxed[col]) {
        if (reduced_costs[col] > harris_ratio * -coeff) continue;
        harris_ratio = std::min(
            harris_ratio, (reduced_costs[col] + harris_tolerance) / -coeff);
        harris_ratio = std::max(minimum_delta / -coeff, harris_ratio);
      }
      breakpoints_.push_back(ColWithRatio(col, reduced_costs[col], -coeff));
      continue;
    }
  }

  // Process the breakpoints in priority order as suggested by Maros in
  // I. Maros, "A generalized dual phase-2 simplex algorithm", European Journal
  // of Operational Research, 149(1):1-16, 2003.
  // We use directly make_heap() to avoid a copy of breakpoints, benchmark shows
  // that it is slightly faster.
  std::make_heap(breakpoints_.begin(), breakpoints_.end());

  // Harris ratio test. Since we process the breakpoints by increasing ratio, we
  // do not need a two-pass algorithm as described in the literature. Each time
  // we process a new breakpoint, we update the harris_ratio of all the
  // processed breakpoints. For the first new breakpoint with a ratio greater
  // than the current harris_ratio we know that:
  // - All the unprocessed breakpoints will have a ratio greater too, so they
  //   will not contribute to the minimum Harris ratio.
  // - We thus have the actual harris_ratio.
  // - We have processed all breakpoints with a ratio smaller than it.
  harris_ratio = std::numeric_limits<Fractional>::max();

  *entering_col = kInvalidCol;
  bound_flip_candidates->clear();
  Fractional step = 0.0;
  Fractional best_coeff = -1.0;
  Fractional variation_magnitude = std::abs(cost_variation);
  equivalent_entering_choices_.clear();
  while (!breakpoints_.empty()) {
    const ColWithRatio top = breakpoints_.front();
    if (top.ratio > harris_ratio) break;

    // If the column is boxed, we can just switch its bounds and
    // ignore the breakpoint! But we need to see if the entering row still
    // improve the objective. This is called the bound flipping ratio test in
    // the literature. See for instance:
    // http://www.mpi-inf.mpg.de/conferences/adfocs-03/Slides/Bixby_2.pdf
    //
    // For each bound flip, |cost_variation| decreases by
    // |upper_bound - lower_bound| times |coeff|.
    //
    // Note that the actual flipping will be done afterwards by
    // MakeBoxedVariableDualFeasible() in revised_simplex.cc.
    if (variation_magnitude > threshold) {
      if (is_boxed[top.col]) {
        variation_magnitude -=
            variables_info_.GetBoundDifference(top.col) * top.coeff_magnitude;
        if (variation_magnitude > threshold) {
          bound_flip_candidates->push_back(top.col);
          std::pop_heap(breakpoints_.begin(), breakpoints_.end());
          breakpoints_.pop_back();
          continue;
        }
      }
    }

    // TODO(user): We want to maximize both the ratio (objective improvement)
    // and the coeff_magnitude (stable pivot), so we have to make some
    // trade-offs. Investigate alternative strategies.
    if (top.coeff_magnitude >= best_coeff) {
      // Update harris_ratio. Note that because we process ratio in order, the
      // harris ratio can only get smaller if the coeff_magnitude is bigger
      // than the one of the best coefficient.
      harris_ratio = std::min(
          harris_ratio, top.ratio + harris_tolerance / top.coeff_magnitude);

      // If the dual infeasibility is too high, the harris_ratio can be
      // negative. In this case we set it to 0.0, allowing any infeasible
      // position to enter the basis. This is quite important because its
      // helps in the choice of a stable pivot.
      harris_ratio =
          std::max(harris_ratio, minimum_delta / top.coeff_magnitude);

      if (top.coeff_magnitude == best_coeff && top.ratio == step) {
        DCHECK_NE(*entering_col, kInvalidCol);
        equivalent_entering_choices_.push_back(top.col);
      } else {
        equivalent_entering_choices_.clear();
        best_coeff = top.coeff_magnitude;
        *entering_col = top.col;

        // Note that the step is not directly used, so it is okay to leave it
        // negative.
        step = top.ratio;
      }
    }

    // Remove the top breakpoint and maintain the heap structure.
    // This is the same as doing a pop() on a priority_queue.
    std::pop_heap(breakpoints_.begin(), breakpoints_.end());
    breakpoints_.pop_back();
  }

  // Break the ties randomly.
  if (!equivalent_entering_choices_.empty()) {
    equivalent_entering_choices_.push_back(*entering_col);
    *entering_col =
        equivalent_entering_choices_[std::uniform_int_distribution<int>(
            0, equivalent_entering_choices_.size() - 1)(random_)];
    IF_STATS_ENABLED(
        stats_.num_perfect_ties.Add(equivalent_entering_choices_.size()));
  }

  if (*entering_col == kInvalidCol) return Status::OK();

  // If best_coeff is small and they are potential bound flips, we can take a
  // smaller step but use a good pivot.
  const Fractional pivot_limit = parameters_.minimum_acceptable_pivot();
  if (best_coeff < pivot_limit && !bound_flip_candidates->empty()) {
    // Note that it is okay to leave more candidate than necessary in the
    // returned bound_flip_candidates vector.
    for (int i = bound_flip_candidates->size() - 1; i >= 0; --i) {
      const ColIndex col = (*bound_flip_candidates)[i];
      if (std::abs(update_coefficient[col]) < pivot_limit) continue;

      VLOG(1) << "Used bound flip to avoid bad pivot. Before: " << best_coeff
              << " now: " << std::abs(update_coefficient[col]);
      *entering_col = col;
      break;
    }
  }

  return Status::OK();
}

Status EnteringVariable::DualPhaseIChooseEnteringColumn(
    bool nothing_to_recompute, const UpdateRow& update_row,
    Fractional cost_variation, ColIndex* entering_col) {
  GLOP_RETURN_ERROR_IF_NULL(entering_col);
  const DenseRow& update_coefficient = update_row.GetCoefficients();
  const DenseRow& reduced_costs = reduced_costs_->GetReducedCosts();
  SCOPED_TIME_STAT(&stats_);

  // List of breakpoints where a variable change from feasibility to
  // infeasibility or the opposite.
  breakpoints_.clear();
  breakpoints_.reserve(update_row.GetNonZeroPositions().size());

  const Fractional threshold = nothing_to_recompute
                                   ? parameters_.minimum_acceptable_pivot()
                                   : parameters_.ratio_test_zero_threshold();
  const Fractional dual_feasibility_tolerance =
      reduced_costs_->GetDualFeasibilityTolerance();
  const Fractional harris_tolerance =
      parameters_.harris_tolerance_ratio() * dual_feasibility_tolerance;
  const Fractional minimum_delta =
      parameters_.degenerate_ministep_factor() * dual_feasibility_tolerance;

  const DenseBitRow& can_decrease = variables_info_.GetCanDecreaseBitRow();
  const DenseBitRow& can_increase = variables_info_.GetCanIncreaseBitRow();
  const VariableTypeRow& variable_type = variables_info_.GetTypeRow();
  num_operations_ += 10 * update_row.GetNonZeroPositions().size();
  for (const ColIndex col : update_row.GetNonZeroPositions()) {
    // Boxed variables shouldn't be in the update position list because they
    // will be dealt with afterwards by MakeBoxedVariableDualFeasible().
    DCHECK_NE(variable_type[col], VariableType::UPPER_AND_LOWER_BOUNDED);

    // Fixed variable shouldn't be in the update position list.
    DCHECK_NE(variable_type[col], VariableType::FIXED_VARIABLE);

    // Skip if the coeff is too small to be a numerically stable pivot.
    if (std::abs(update_coefficient[col]) < threshold) continue;

    // We will add ratio * coeff to this column. cost_variation makes sure
    // the leaving variable will be dual-feasible (its update coeff is
    // sign(cost_variation) * 1.0).
    //
    // TODO(user): This is the same in DualChooseEnteringColumn(), remove
    // duplication?
    const Fractional coeff = (cost_variation > 0.0) ? update_coefficient[col]
                                                    : -update_coefficient[col];

    // Only proceed if there is a transition, note that if reduced_costs[col]
    // is close to zero, then the variable is counted as dual-feasible.
    if (std::abs(reduced_costs[col]) <= dual_feasibility_tolerance) {
      // Continue if the variation goes in the dual-feasible direction.
      if (coeff > 0 && !can_decrease.IsSet(col)) continue;
      if (coeff < 0 && !can_increase.IsSet(col)) continue;

      // For an already dual-infeasible variable, we allow to push it until
      // the harris_tolerance. But if it is past that or close to it, we also
      // always enforce a minimum push.
      if (coeff * reduced_costs[col] > 0.0) {
        breakpoints_.push_back(ColWithRatio(
            col,
            std::max(minimum_delta,
                     harris_tolerance - std::abs(reduced_costs[col])),
            std::abs(coeff)));
        continue;
      }
    } else {
      // If the two are of the same sign, there is no transition, skip.
      if (coeff * reduced_costs[col] > 0.0) continue;
    }

    // We are sure there is a transition, add it to the set of breakpoints.
    breakpoints_.push_back(ColWithRatio(
        col, std::abs(reduced_costs[col]) + harris_tolerance, std::abs(coeff)));
  }

  // Process the breakpoints in priority order.
  std::make_heap(breakpoints_.begin(), breakpoints_.end());

  // Because of our priority queue, it is easy to choose a sub-optimal step to
  // have a stable pivot. The pivot with the highest magnitude and that reduces
  // the infeasibility the most is chosen.
  Fractional pivot_magnitude = 0.0;

  // Select the last breakpoint that still improves the infeasibility and has a
  // numerically stable pivot.
  *entering_col = kInvalidCol;
  Fractional step = -1.0;
  Fractional improvement = std::abs(cost_variation);
  while (!breakpoints_.empty()) {
    const ColWithRatio top = breakpoints_.front();

    // We keep the greatest coeff_magnitude for the same ratio.
    DCHECK(top.ratio > step ||
           (top.ratio == step && top.coeff_magnitude <= pivot_magnitude));
    if (top.ratio > step && top.coeff_magnitude >= pivot_magnitude) {
      *entering_col = top.col;
      step = top.ratio;
      pivot_magnitude = top.coeff_magnitude;
    }
    improvement -= top.coeff_magnitude;

    // If the variable is free, then not only do we loose the infeasibility
    // improvment, we also render it worse if we keep going in the same
    // direction.
    if (can_decrease.IsSet(top.col) && can_increase.IsSet(top.col) &&
        std::abs(reduced_costs[top.col]) > threshold) {
      improvement -= top.coeff_magnitude;
    }

    if (improvement <= 0.0) break;
    std::pop_heap(breakpoints_.begin(), breakpoints_.end());
    breakpoints_.pop_back();
  }
  return Status::OK();
}

void EnteringVariable::SetParameters(const GlopParameters& parameters) {
  parameters_ = parameters;
}

}  // namespace glop
}  // namespace operations_research
