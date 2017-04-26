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


#include "ortools/glop/entering_variable.h"

#include <queue>

#include "ortools/base/timer.h"
#include "ortools/base/numbers.h"
#include "ortools/lp_data/lp_utils.h"

namespace operations_research {
namespace glop {

#if defined(ANDROID_JNI) && (defined(__ANDROID__) || defined(__APPLE__))
// Enum -> std::string conversions are not present in MessageLite that is being used
// on Android with ANDROID_JNI build.
std::string GlopParameters_PricingRule_Name(int rule) {
  return SimpleItoa(rule);
}
#endif  // ANDROID_JNI

EnteringVariable::EnteringVariable(const VariablesInfo& variables_info,
                                   RandomBase* random,
                                   ReducedCosts* reduced_costs,
                                   PrimalEdgeNorms* primal_edge_norms)
    : variables_info_(variables_info),
      random_(random),
      reduced_costs_(reduced_costs),
      primal_edge_norms_(primal_edge_norms),
      parameters_(),
      rule_(GlopParameters::DANTZIG),
      unused_columns_() {}

Status EnteringVariable::PrimalChooseEnteringColumn(ColIndex* entering_col) {
  SCOPED_TIME_STAT(&stats_);
  GLOP_RETURN_ERROR_IF_NULL(entering_col);

  // For better redability of the templated function calls below.
  const bool kNormalize = true;
  const bool kNested = true;
  const bool kSteepest = true;

  switch (rule_) {
    case GlopParameters::DANTZIG:
      if (parameters_.use_nested_pricing()) {
        if (unused_columns_.size() != variables_info_.GetNumberOfColumns()) {
          ResetUnusedColumns();
        }
        if (parameters_.normalize_using_column_norm()) {
          DantzigChooseEnteringColumn<kNormalize, kNested>(entering_col);
        } else {
          DantzigChooseEnteringColumn<!kNormalize, kNested>(entering_col);
        }
        if (*entering_col != kInvalidCol) {
          unused_columns_.Clear(*entering_col);
          return Status::OK;
        }
        ResetUnusedColumns();
        if (parameters_.normalize_using_column_norm()) {
          DantzigChooseEnteringColumn<kNormalize, kNested>(entering_col);
        } else {
          DantzigChooseEnteringColumn<!kNormalize, kNested>(entering_col);
        }
      } else {
        if (parameters_.normalize_using_column_norm()) {
          DantzigChooseEnteringColumn<kNormalize, !kNested>(entering_col);
        } else {
          DantzigChooseEnteringColumn<!kNormalize, !kNested>(entering_col);
        }
      }
      return Status::OK;
    case GlopParameters::STEEPEST_EDGE:
      NormalizedChooseEnteringColumn<kSteepest>(entering_col);
      return Status::OK;
    case GlopParameters::DEVEX:
      NormalizedChooseEnteringColumn<!kSteepest>(entering_col);
      return Status::OK;
  }
  LOG(DFATAL) << "Unknown pricing rule: "
              << GlopParameters_PricingRule_Name(rule_)
              << ". Using steepest edge.";
  NormalizedChooseEnteringColumn<kSteepest>(entering_col);
  return Status::OK;
}

namespace {

// Store a column with its update coefficient and ratio.
// This is used during the dual phase I & II ratio tests.
struct ColWithRatio {
  ColWithRatio(ColIndex _col, Fractional reduced_cost, Fractional coeff_m)
      : col(_col), ratio(reduced_cost / coeff_m), coeff_magnitude(coeff_m) {}

  // Returns false if "this" is before "other" in a priority queue.
  bool operator<(const ColWithRatio& other) const {
    if (ratio == other.ratio) {
      if (coeff_magnitude == other.coeff_magnitude) {
        return col > other.col;
      }
      return coeff_magnitude < other.coeff_magnitude;
    }
    return ratio > other.ratio;
  }

  ColIndex col;
  Fractional ratio;
  Fractional coeff_magnitude;
};

}  // namespace

Status EnteringVariable::DualChooseEnteringColumn(
    const UpdateRow& update_row, Fractional cost_variation,
    std::vector<ColIndex>* bound_flip_candidates, ColIndex* entering_col,
    Fractional* pivot, Fractional* step) {
  GLOP_RETURN_ERROR_IF_NULL(entering_col);
  GLOP_RETURN_ERROR_IF_NULL(pivot);
  const DenseRow& update_coefficient = update_row.GetCoefficients();
  const DenseRow& reduced_costs = reduced_costs_->GetReducedCosts();
  SCOPED_TIME_STAT(&stats_);

  std::vector<ColWithRatio> breakpoints;
  breakpoints.reserve(update_row.GetNonZeroPositions().size());
  const Fractional threshold = parameters_.ratio_test_zero_threshold();
  const DenseBitRow& can_decrease = variables_info_.GetCanDecreaseBitRow();
  const DenseBitRow& can_increase = variables_info_.GetCanIncreaseBitRow();

  // Harris ratio test. See below for more explanation. Here this is used to
  // prune the first pass by not enqueueing ColWithRatio for columns that have
  // a ratio greater than the current harris_ratio.
  const VariableTypeRow& variable_type = variables_info_.GetTypeRow();
  const Fractional harris_tolerance =
      parameters_.harris_tolerance_ratio() *
      reduced_costs_->GetDualFeasibilityTolerance();
  Fractional harris_ratio = std::numeric_limits<Fractional>::max();

  for (const ColIndex col : update_row.GetNonZeroPositions()) {
    // We will add ratio * coeff to this column with a ratio positive or zero.
    // cost_variation makes sure the leaving variable will be dual-feasible
    // (its update coeff is sign(cost_variation) * 1.0).
    const Fractional coeff = (cost_variation > 0.0) ? update_coefficient[col]
                                                    : -update_coefficient[col];

    // In this case, at some point the reduced cost will be positive if not
    // already, and the column will be dual-infeasible.
    if (can_decrease.IsSet(col) && coeff > threshold) {
      if (variable_type[col] != VariableType::UPPER_AND_LOWER_BOUNDED) {
        if (-reduced_costs[col] > harris_ratio * coeff) continue;
        harris_ratio = std::min(
            harris_ratio, (-reduced_costs[col] + harris_tolerance) / coeff);
        harris_ratio = std::max(0.0, harris_ratio);
      }
      breakpoints.push_back(ColWithRatio(col, -reduced_costs[col], coeff));
      continue;
    }

    // In this case, at some point the reduced cost will be negative if not
    // already, and the column will be dual-infeasible.
    if (can_increase.IsSet(col) && coeff < -threshold) {
      if (variable_type[col] != VariableType::UPPER_AND_LOWER_BOUNDED) {
        if (reduced_costs[col] > harris_ratio * -coeff) continue;
        harris_ratio = std::min(
            harris_ratio, (reduced_costs[col] + harris_tolerance) / -coeff);
        harris_ratio = std::max(0.0, harris_ratio);
      }
      breakpoints.push_back(ColWithRatio(col, reduced_costs[col], -coeff));
      continue;
    }
  }

  // Process the breakpoints in priority order as suggested by Maros in
  // I. Maros, "A generalized dual phase-2 simplex algorithm", European Journal
  // of Operational Research, 149(1):1-16, 2003.
  // We use directly make_heap() to avoid a copy of breakpoints, benchmark shows
  // that it is slightly faster.
  std::make_heap(breakpoints.begin(), breakpoints.end());

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
  Fractional best_coeff = -1.0;
  Fractional variation_magnitude = std::abs(cost_variation);
  equivalent_entering_choices_.clear();
  while (!breakpoints.empty()) {
    const ColWithRatio top = breakpoints.front();
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
    bool variable_can_flip = false;
    if (variable_type[top.col] == VariableType::UPPER_AND_LOWER_BOUNDED) {
      variation_magnitude -=
          variables_info_.GetBoundDifference(top.col) * top.coeff_magnitude;
      if (variation_magnitude > threshold) {
        variable_can_flip = true;
        bound_flip_candidates->push_back(top.col);
      }
    }

    // Update harris_ratio (only if the variable cannot flip).
    if (!variable_can_flip) {
      harris_ratio = std::min(
          harris_ratio, top.ratio + harris_tolerance / top.coeff_magnitude);

      // If the dual infeasibility is too high, the harris_ratio can be
      // negative. In this case we set it to 0.0, allowing any infeasible
      // position to enter the basis. This is quite important because its helps
      // in the choice of a stable pivot.
      harris_ratio = std::max(harris_ratio, 0.0);
    }

    // TODO(user): We want to maximize both the ratio (objective improvement)
    // and the coeff_magnitude (stable pivot), so we have to make some
    // trade-offs.
    if (top.coeff_magnitude >= best_coeff) {
      if (top.coeff_magnitude == best_coeff && top.ratio == *step) {
        DCHECK_NE(*entering_col, kInvalidCol);
        equivalent_entering_choices_.push_back(top.col);
      } else {
        equivalent_entering_choices_.clear();
        best_coeff = top.coeff_magnitude;
        *entering_col = top.col;

        // Note that the step is not directly used, so it is okay to leave it
        // negative.
        *step = top.ratio;
      }
    }

    // Remove the top breakpoint and maintain the heap structure.
    // This is the same as doing a pop() on a priority_queue.
    std::pop_heap(breakpoints.begin(), breakpoints.end());
    breakpoints.pop_back();
  }

  // Break the ties randomly.
  if (!equivalent_entering_choices_.empty()) {
    equivalent_entering_choices_.push_back(*entering_col);
    *entering_col = equivalent_entering_choices_[random_->Uniform(
        equivalent_entering_choices_.size())];
    IF_STATS_ENABLED(
        stats_.num_perfect_ties.Add(equivalent_entering_choices_.size()));
  }

  if (*entering_col == kInvalidCol) return Status::OK;
  *pivot = update_coefficient[*entering_col];

  // If the step is 0.0, we make sure the reduced cost is 0.0 so
  // UpdateReducedCosts() will not take a step that goes in the wrong way (a few
  // experiments seems to indicate that this is not a good idea). See comment
  // at the top of UpdateReducedCosts().
  //
  // Note that ShiftCost() actually shifts the cost a bit more in order to do a
  // non-zero step. This helps on degenerate problems. See the comment of
  // ShiftCost() for more detail.
  //
  // TODO(user): Do not do that if we do not end up using this pivot?
  if (*step <= 0.0) {
    // In order to be mathematically consistent, we shift the cost of the
    // entering column in such a way that its reduced cost is indeed zero. This
    // is called cost-shifting or perturbation in the literature and it does
    // really help on degenerate problems. The pertubation will be removed once
    // the pertubed problem is solved to the optimal.
    reduced_costs_->ShiftCost(*entering_col);
  }
  return Status::OK;
}

Status EnteringVariable::DualPhaseIChooseEnteringColumn(
    const UpdateRow& update_row, Fractional cost_variation,
    ColIndex* entering_col, Fractional* pivot, Fractional* step) {
  GLOP_RETURN_ERROR_IF_NULL(entering_col);
  GLOP_RETURN_ERROR_IF_NULL(pivot);
  const DenseRow& update_coefficient = update_row.GetCoefficients();
  const DenseRow& reduced_costs = reduced_costs_->GetReducedCosts();
  SCOPED_TIME_STAT(&stats_);

  // List of breakpoints where a variable change from feasibility to
  // infeasibility or the opposite.
  std::vector<ColWithRatio> breakpoints;
  breakpoints.reserve(update_row.GetNonZeroPositions().size());

  // Ratio test.
  const Fractional threshold = parameters_.ratio_test_zero_threshold();
  const Fractional dual_feasibility_tolerance =
      reduced_costs_->GetDualFeasibilityTolerance();
  const DenseBitRow& can_decrease = variables_info_.GetCanDecreaseBitRow();
  const DenseBitRow& can_increase = variables_info_.GetCanIncreaseBitRow();
  const VariableTypeRow& variable_type = variables_info_.GetTypeRow();
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
    // is close to zero, then the variable is supposed to be dual-feasible.
    if (std::abs(reduced_costs[col]) <= dual_feasibility_tolerance) {
      // Continue if the variation goes in the dual-feasible direction.
      if (coeff > 0 && !can_decrease.IsSet(col)) continue;
      if (coeff < 0 && !can_increase.IsSet(col)) continue;

      // Note that here, a variable which is already dual-infeasible will still
      // have a positive ratio. This may sounds weird, but the idea is to put
      // first in the sorted breakpoint list a variable which has a reduced
      // costs close to zero in order to minimize the magnitude of a step in the
      // wrong direction.
    } else {
      // If the two are of the same sign, there is no transition, skip.
      if (coeff * reduced_costs[col] > 0) continue;
    }

    // We are sure there is a transition, add it to the set of breakpoints.
    breakpoints.push_back(
        ColWithRatio(col, std::abs(reduced_costs[col]), std::abs(coeff)));
  }

  // Process the breakpoints in priority order.
  std::make_heap(breakpoints.begin(), breakpoints.end());

  // Because of our priority queue, it is easy to choose a sub-optimal step to
  // have a stable pivot. The pivot with the highest magnitude and that reduces
  // the infeasibility the most is chosen.
  Fractional pivot_magnitude = 0.0;

  // Select the last breakpoint that still improves the infeasibility and has a
  // numerically stable pivot.
  *entering_col = kInvalidCol;
  *step = -1.0;
  Fractional improvement = std::abs(cost_variation);
  while (!breakpoints.empty()) {
    const ColWithRatio top = breakpoints.front();

    // We keep the greatest coeff_magnitude for the same ratio.
    DCHECK(top.ratio > *step ||
           (top.ratio == *step && top.coeff_magnitude <= pivot_magnitude));
    if (top.ratio > *step && top.coeff_magnitude >= pivot_magnitude) {
      *entering_col = top.col;
      *step = top.ratio;
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
    std::pop_heap(breakpoints.begin(), breakpoints.end());
    breakpoints.pop_back();
  }
  *pivot =
      (*entering_col == kInvalidCol) ? 0.0 : update_coefficient[*entering_col];
  return Status::OK;
}

void EnteringVariable::SetParameters(const GlopParameters& parameters) {
  parameters_ = parameters;
}

void EnteringVariable::SetPricingRule(GlopParameters::PricingRule rule) {
  rule_ = rule;
}

DenseBitRow* EnteringVariable::ResetUnusedColumns() {
  SCOPED_TIME_STAT(&stats_);
  const ColIndex num_cols = variables_info_.GetNumberOfColumns();
  if (unused_columns_.size() != num_cols) {
    unused_columns_.ClearAndResize(num_cols);
  }

  // Invert the set of unused columns, minus the basis.
  const DenseBitRow& is_basic = variables_info_.GetIsBasicBitRow();
  for (ColIndex col(0); col < num_cols; ++col) {
    if (unused_columns_.IsSet(col)) {
      unused_columns_.Clear(col);
    } else {
      if (!is_basic.IsSet(col)) {
        unused_columns_.Set(col);
      }
    }
  }
  return &unused_columns_;
}

template <bool normalize, bool nested_pricing>
void EnteringVariable::DantzigChooseEnteringColumn(ColIndex* entering_col) {
  DenseRow dummy;
  const DenseRow& matrix_column_norms =
      normalize ? primal_edge_norms_->GetMatrixColumnNorms() : dummy;
  const DenseRow& reduced_costs = reduced_costs_->GetReducedCosts();
  SCOPED_TIME_STAT(&stats_);

  Fractional best_price(0.0);
  *entering_col = kInvalidCol;
  for (const ColIndex col : reduced_costs_->GetDualInfeasiblePositions()) {
    if (nested_pricing && !unused_columns_.IsSet(col)) continue;
    const Fractional unormalized_price = std::abs(reduced_costs[col]);
    if (normalize) {
      if (unormalized_price > best_price * matrix_column_norms[col]) {
        best_price = unormalized_price / matrix_column_norms[col];
        *entering_col = col;
      }
    } else {
      if (unormalized_price > best_price) {
        best_price = unormalized_price;
        *entering_col = col;
      }
    }
  }
}

// TODO(user): Here we could fill a priority queue with the normalized
// reduced cost of the top n candidate columns. This makes it possible
// - To respond right away after each bound flip iteration.
// - To return the top-n choices if we want to consider multiple candidates in
//   the other parts of the simplex algorithm.
template <bool use_steepest_edge>
void EnteringVariable::NormalizedChooseEnteringColumn(ColIndex* entering_col) {
  const DenseRow& weights = use_steepest_edge
                                ? primal_edge_norms_->GetEdgeSquaredNorms()
                                : primal_edge_norms_->GetDevexWeights();
  const DenseRow& reduced_costs = reduced_costs_->GetReducedCosts();
  SCOPED_TIME_STAT(&stats_);

  Fractional best_price(0.0);
  *entering_col = kInvalidCol;
  equivalent_entering_choices_.clear();
  for (const ColIndex col : reduced_costs_->GetDualInfeasiblePositions()) {
    if (use_steepest_edge) {
      // Note that here the weights are squared.
      const Fractional squared_reduced_cost = Square(reduced_costs[col]);
      if (squared_reduced_cost >= best_price * weights[col]) {
        if (squared_reduced_cost == best_price * weights[col]) {
          equivalent_entering_choices_.push_back(col);
          continue;
        }
        equivalent_entering_choices_.clear();
        best_price = squared_reduced_cost / weights[col];
        *entering_col = col;
      }
    } else {
      const Fractional positive_reduced_cost = std::abs(reduced_costs[col]);
      if (positive_reduced_cost >= best_price * weights[col]) {
        if (positive_reduced_cost == best_price * weights[col]) {
          equivalent_entering_choices_.push_back(col);
          continue;
        }
        equivalent_entering_choices_.clear();
        best_price = positive_reduced_cost / weights[col];
        *entering_col = col;
      }
    }
  }
  // Break the ties randomly.
  if (!equivalent_entering_choices_.empty()) {
    equivalent_entering_choices_.push_back(*entering_col);
    *entering_col = equivalent_entering_choices_[random_->Uniform(
        equivalent_entering_choices_.size())];
    IF_STATS_ENABLED(
        stats_.num_perfect_ties.Add(equivalent_entering_choices_.size()));
  }
}

}  // namespace glop
}  // namespace operations_research
