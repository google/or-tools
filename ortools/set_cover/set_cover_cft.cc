// Copyright 2025 Francesco Cavaliere
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

#include "ortools/set_cover/set_cover_cft.h"

#include <absl/algorithm/container.h>
#include <absl/random/random.h>
#include <absl/status/status.h>
#include <absl/strings/str_join.h>
#include <absl/types/span.h>

#include <limits>
#include <random>

#include "ortools/base/status_macros.h"
#include "ortools/base/stl_util.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_views.h"

namespace operations_research::scp {

////////////////////////////////////////////////////////////////////////
////////////////////////// COMMON DEFINITIONS //////////////////////////
////////////////////////////////////////////////////////////////////////
namespace {
class CoverCounters {
 public:
  CoverCounters(BaseInt nelems = 0) : cov_counters(nelems, 0) {}
  void Reset(BaseInt nelems) { cov_counters.assign(nelems, 0); }
  BaseInt Size() const { return cov_counters.size(); }
  BaseInt operator[](ElementIndex i) const {
    assert(i < ElementIndex(cov_counters.size()));
    return cov_counters[i];
  }

  template <typename IterableT>
  BaseInt Cover(const IterableT& subset) {
    BaseInt covered = 0;
    for (ElementIndex i : subset) {
      covered += cov_counters[i] == 0 ? 1ULL : 0ULL;
      cov_counters[i]++;
    }
    return covered;
  }

  template <typename IterableT>
  BaseInt Uncover(const IterableT& subset) {
    BaseInt uncovered = 0;
    for (ElementIndex i : subset) {
      --cov_counters[i];
      uncovered += cov_counters[i] == 0 ? 1ULL : 0ULL;
    }
    return uncovered;
  }

  // Check if all the elements of a subset are already covered.
  template <typename IterableT>
  bool IsRedundantCover(IterableT const& subset) const {
    return absl::c_all_of(subset,
                          [&](ElementIndex i) { return cov_counters[i] > 0; });
  }

  // Check if all the elements would still be covered if the subset was removed.
  template <typename IterableT>
  bool IsRedundantUncover(IterableT const& subset) const {
    return absl::c_all_of(subset,
                          [&](ElementIndex i) { return cov_counters[i] > 1; });
  }

 private:
  ElementToIntVector cov_counters;
};
}  // namespace

Solution::Solution(const SubModel& model,
                   const std::vector<SubsetIndex>& core_subsets)
    : cost_(), subsets_() {
  subsets_.reserve(core_subsets.size() + model.fixed_columns().size());
  cost_ = model.fixed_cost();
  for (FullSubsetIndex full_j : model.fixed_columns()) {
    subsets_.push_back(full_j);
  }
  for (SubsetIndex core_j : core_subsets) {
    FullSubsetIndex full_j = model.MapCoreToFullSubsetIndex(core_j);
    AddSubset(full_j, model.subset_costs()[core_j]);
  }
}

///////////////////////////////////////////////////////////////////////
///////////////////////////// SUBGRADIENT /////////////////////////////
///////////////////////////////////////////////////////////////////////

BoundCBs::BoundCBs(const SubModel& model)
    : squared_norm_(static_cast<Cost>(model.num_elements())),
      direction_(ElementCostVector(model.num_elements(), .0)),
      prev_best_lb_(std::numeric_limits<Cost>::lowest()),
      max_iter_countdown_(10 *
                          model.num_focus_elements()),  // Arbitrary from [1]
      exit_test_countdown_(300),                        // Arbitrrary from [1]
      exit_test_period_(300),                           // Arbitrary from [1]
      step_size_(0.1),                                  // Arbitrary from [1]
      last_min_lb_seen_(std::numeric_limits<Cost>::max()),
      last_max_lb_seen_(.0),
      step_size_update_countdown_(20),  // Arbitrary from [1]
      step_size_update_period_(20)      // Arbitrary from [1]
{}

bool BoundCBs::ExitCondition(const SubgradientContext& context) {
  if (last_core_update_countdown_ > 0) {
    --last_core_update_countdown_;
    return false;
  }
  if (--max_iter_countdown_ <= 0 || squared_norm_ <= kTol ||
      // Note: assumes integral costs
      context.best_dual_state.lower_bound() >=
          context.best_solution.cost() - context.model.fixed_cost() - .999) {
    return true;
  }
  if (--exit_test_countdown_ > 0) {
    return false;
  }
  exit_test_countdown_ = exit_test_period_;
  Cost best_lower_bound = context.best_dual_state.lower_bound();
  Cost abs_improvement = best_lower_bound - prev_best_lb_;
  Cost rel_improvement = DivideIfGE0(abs_improvement, best_lower_bound);
  prev_best_lb_ = best_lower_bound;
  return abs_improvement < 1.0 && rel_improvement < .001;
}

void BoundCBs::ComputeMultipliersDelta(const SubgradientContext& context,
                                       ElementCostVector& delta_mults) {
  Cost lower_bound = context.current_dual_state.lower_bound();
  UpdateStepSize(lower_bound);

  direction_ = context.subgradient;
  MakeMinimalCoverageSubgradient(context, direction_);

  if (prev_direction_.empty()) {
    prev_direction_ = direction_;
  }

  squared_norm_ = .0;
  // Stabilize current direction and compute squared norm
  for (ElementIndex i : context.model.ElementRange()) {
    direction_[i] = stabilization_coeff * direction_[i] +
                    (1.0 - stabilization_coeff) * prev_direction_[i];
    if ((context.current_dual_state.multipliers()[i] <= .0) &&
        (direction_[i] < .0)) {
      direction_[i] = 0;
    }
    squared_norm_ += direction_[i] * direction_[i];
    prev_direction_[i] = direction_[i];
  }

  Cost upper_bound = context.best_solution.cost() - context.model.fixed_cost();
  Cost delta = upper_bound - lower_bound;
  Cost step_constant = step_size_ * delta / squared_norm_;
  for (ElementIndex i : context.model.ElementRange()) {
    delta_mults[i] = step_constant * direction_[i];
  }
}

void BoundCBs::UpdateStepSize(Cost lower_bound) {
  last_min_lb_seen_ = std::min(last_min_lb_seen_, lower_bound);
  last_max_lb_seen_ = std::max(last_max_lb_seen_, lower_bound);

  if (--step_size_update_countdown_ <= 0) {
    step_size_update_countdown_ = step_size_update_period_;

    Cost delta = last_max_lb_seen_ - last_min_lb_seen_;
    Cost gap = DivideIfGE0(delta, last_max_lb_seen_);
    if (gap <= .001) {    // Arbitray from [1]
      step_size_ *= 1.5;  // Arbitray from [1]

      // Arbitrary from c4v4
      stabilization_coeff = (1.0 + stabilization_coeff) / 2.0;

    } else if (gap > .01) {  // Arbitray from [1]
      step_size_ /= 2.0;     // Arbitray from [1]
    }
    last_min_lb_seen_ = std::numeric_limits<Cost>::max();
    last_max_lb_seen_ = .0;
    // Not described in the paper, but in rare cases the subgradient diverges
    step_size_ = std::clamp(step_size_, 1e-6, 10.0);  // Arbitrary from c4v4
  }
}

void BoundCBs::MakeMinimalCoverageSubgradient(const SubgradientContext& context,
                                              ElementCostVector& subgradient) {
  lagrangian_solution_.clear();
  const auto& reduced_costs = context.current_dual_state.reduced_costs();
  for (SubsetIndex j : context.model.SubsetRange()) {
    if (reduced_costs[j] < .0) {
      lagrangian_solution_.push_back(j);
    }
  }

  absl::c_sort(lagrangian_solution_, [&](SubsetIndex j1, SubsetIndex j2) {
    return reduced_costs[j1] > reduced_costs[j2];
  });

  const auto& cols = context.model.columns();
  for (SubsetIndex j : lagrangian_solution_) {
    if (absl::c_all_of(cols[j], [&](auto i) { return subgradient[i] < 0; })) {
      absl::c_for_each(cols[j], [&](auto i) { subgradient[i] += 1.0; });
    }
  }
}

bool BoundCBs::UpdateCoreModel(SubModel& core_model,
                               PrimalDualState& best_state) {
  if (core_model.UpdateCore(best_state)) {
    // grant at least 10 iterations before the next exit test
    prev_best_lb_ =
        std::min(prev_best_lb_, best_state.dual_state.lower_bound());
    exit_test_countdown_ = std::max<BaseInt>(exit_test_countdown_, 20);
    max_iter_countdown_ = std::max<BaseInt>(max_iter_countdown_, 20);
    last_core_update_countdown_ = 20;
    return true;
  }
  return false;
}

void SubgradientOptimization(SubModel& model, SubgradientCBs& cbs,
                             PrimalDualState& best_state) {
  DCHECK(ValidateSubModel(model));

  ElementCostVector subgradient = ElementCostVector(model.num_elements(), .0);
  DualState dual_state = best_state.dual_state;
  Solution solution;

  ElementCostVector multipliers_delta(model.num_elements());  // to avoid allocs
  SubgradientContext context = {.model = model,
                                .current_dual_state = dual_state,
                                .best_dual_state = best_state.dual_state,
                                .best_solution = best_state.solution,
                                .subgradient = subgradient};
  size_t iter = 0;
  while (!cbs.ExitCondition(context)) {
    // Compute subgradient (O(nnz))
    subgradient.assign(model.num_elements(), 1.0);
    for (SubsetIndex j : model.SubsetRange()) {
      if (dual_state.reduced_costs()[j] < .0) {
        for (ElementIndex i : model.columns()[j]) {
          subgradient[i] -= 1.0;
        }
      }
    }

    cbs.ComputeMultipliersDelta(context, multipliers_delta);
    dual_state.DualUpdate(model, [&](ElementIndex i, Cost& i_mult) {
      i_mult = std::max<Cost>(.0, i_mult + multipliers_delta[i]);
    });
    if (dual_state.lower_bound() > best_state.dual_state.lower_bound()) {
      best_state.dual_state = dual_state;
    }

    cbs.RunHeuristic(context, solution);
    if (!solution.subsets().empty() &&
        solution.cost() < best_state.solution.cost()) {
      best_state.solution = solution;
    }

    if (++iter % 50 == 0)
      std::cout << "Subgradient " << iter << " -- Bounds: Lower "
                << dual_state.lower_bound() << ", best "
                << best_state.dual_state.lower_bound() << " - Upper "
                << best_state.solution.cost() - model.fixed_cost()
                << ", global " << best_state.solution.cost() << "\n";

    if (cbs.UpdateCoreModel(model, best_state)) {
      std::cout << "Core model has been updated.\n";
      dual_state = best_state.dual_state;
    }
  }
  std::cout << "Subgradient End" << " -- Bounds: Lower "
            << dual_state.lower_bound() << ", best "
            << best_state.dual_state.lower_bound() << " - Upper "
            << best_state.solution.cost() - model.fixed_cost() << ", global "
            << best_state.solution.cost() << "\n";
}

////////////////////////////////////////////////////////////////////////
/////////////////////// MULTIPLIERS BASED GREEDY ///////////////////////
////////////////////////////////////////////////////////////////////////
namespace {
struct Score {
  Cost score;
  SubsetIndex idx;
};

class GreedyScores {
 public:
  static constexpr BaseInt removed_idx = -1;
  static constexpr Cost max_score = std::numeric_limits<Cost>::max();

  GreedyScores(const SubModel& model, const DualState& dual_state)
      : bad_size_(),
        worst_good_score_(std::numeric_limits<Cost>::lowest()),
        scores_(),
        reduced_costs_(dual_state.reduced_costs()),
        covering_counts_(model.num_subsets()),
        score_map_(model.num_subsets()) {
    BaseInt s = 0;
    for (SubsetIndex j : model.SubsetRange()) {
      DCHECK(model.column_size(j) > 0);
      covering_counts_[j] = model.column_size(j);
      Cost j_score = ComputeScore(reduced_costs_[j], covering_counts_[j]);
      scores_.push_back({j_score, j});
      score_map_[j] = s++;
      DCHECK(std::isfinite(reduced_costs_[j]));
      DCHECK(std::isfinite(j_score));
    }
    bad_size_ = scores_.size();
  }

  SubsetIndex FindMinScoreColumn(const SubModel& model,
                                 const DualState& dual_state) {
    // Check if the bad/good partition should be updated
    if (bad_size_ == scores_.size()) {
      if (bad_size_ > model.num_focus_elements()) {
        bad_size_ = bad_size_ - model.num_focus_elements();
        absl::c_nth_element(scores_, scores_.begin() + bad_size_,
                            [](Score a, Score b) { return a.score > b.score; });
        worst_good_score_ = scores_[bad_size_].score;
        for (BaseInt s = 0; s < scores_.size(); ++s) {
          score_map_[scores_[s].idx] = s;
        }
      } else {
        bad_size_ = 0;
        worst_good_score_ = max_score;
      }
      DCHECK(bad_size_ > 0 || worst_good_score_ == max_score);
    }

    Score min_score =
        *std::min_element(scores_.begin() + bad_size_, scores_.end(),
                          [](Score a, Score b) { return a.score < b.score; });
    SubsetIndex j_star = min_score.idx;
    DCHECK_LT(min_score.score, max_score);
    return j_star;
  }

  // For each row in the given set, if `cond` returns true, the row is
  // considered newly covered. The function then iterates over the columns of
  // that row, updating the scores of the columns accordingly.
  template <typename RowT, typename ElementSpanT, typename CondT>
  BaseInt UpdateColumnsScoreOfRowsIf(const RowT& rows,
                                     const ElementCostVector& multipliers,
                                     const ElementSpanT& row_idxs, CondT cond) {
    BaseInt processed_rows_count = 0;
    for (ElementIndex i : row_idxs) {
      if (!cond(i)) {
        continue;
      }

      ++processed_rows_count;
      for (SubsetIndex j : rows[i]) {
        covering_counts_[j] -= 1;
        reduced_costs_[j] += multipliers[i];

        BaseInt s = score_map_[j];
        DCHECK_NE(s, removed_idx) << "Column is not in the score map";
        scores_[s].score = ComputeScore(reduced_costs_[j], covering_counts_[j]);

        if (covering_counts_[j] == 0) {
          // Column is redundant: its score can be removed
          if (s < bad_size_) {
            // Column is bad: promote to good partition before removal
            SwapScores(s, bad_size_ - 1);
            s = --bad_size_;
          }
          SwapScores(s, scores_.size() - 1);
          scores_.pop_back();
        } else if (s >= bad_size_ && scores_[s].score > worst_good_score_) {
          // Column not good anymore: move it into bad partition
          SwapScores(s, bad_size_++);
        }
      }
    }
    return processed_rows_count;
  }

 private:
  void SwapScores(BaseInt s1, BaseInt s2) {
    SubsetIndex j1 = scores_[s1].idx, j2 = scores_[s2].idx;
    std::swap(scores_[s1], scores_[s2]);
    std::swap(score_map_[j1], score_map_[j2]);
  }

  // Score computed as described in [1]
  static Cost ComputeScore(Cost adjusted_reduced_cost,
                           BaseInt num_rows_covered) {
    DCHECK(std::isfinite(adjusted_reduced_cost)) << "Gamma is not finite";
    return num_rows_covered == 0
               ? max_score
               : (adjusted_reduced_cost > .0
                      ? adjusted_reduced_cost / num_rows_covered
                      : adjusted_reduced_cost * num_rows_covered);
  }

 private:
  // scores_ is partitioned into bad-scores / good-scores
  BaseInt bad_size_;

  // sentinel level to trigger a partition update of the scores
  Cost worst_good_score_;

  // column scores kept updated
  std::vector<Score> scores_;

  // reduced costs adjusted to currently uncovered rows (size=n)
  SubsetCostVector reduced_costs_;

  // number of uncovered rows covered by each column (size=n)
  SubsetToIntVector covering_counts_;

  // position of each column score into the scores_
  SubsetToIntVector score_map_;
};

// Stores the redundancy set and related information
class RedundancyRemover {
 public:
  RedundancyRemover(const SubModel& model, CoverCounters& total_coverage)
      : redund_set_(),
        total_coverage_(total_coverage),
        partial_coverage_(model.num_elements()),
        partial_cost_(.0),
        partial_size_(0),
        partial_cov_count_(0),
        cols_to_remove_() {}

  Cost TryRemoveRedundantCols(const SubModel& model, Cost cost_cutoff,
                              std::vector<SubsetIndex>& sol_subsets) {
    for (SubsetIndex j : sol_subsets) {
      if (total_coverage_.IsRedundantUncover(model.columns()[j]))
        redund_set_.push_back({model.subset_costs()[j], j});
      else {
        ++partial_size_;
        partial_cost_ += model.subset_costs()[j];
        partial_cov_count_ += partial_coverage_.Cover(model.columns()[j]);
      }
      if (partial_cost_ >= cost_cutoff) {
        return partial_cost_;
      }
    }
    if (redund_set_.empty()) {
      return partial_cost_;
    }
    absl::c_sort(redund_set_,
                 [](Score a, Score b) { return a.score < b.score; });

    if (partial_cov_count_ < model.num_focus_elements()) {
      // Complete partial solution heuristically
      HeuristicRedundancyRemoval(model, cost_cutoff);
    } else {
      // All redundant columns can be removed
      for (Score redund_col : redund_set_) {
        cols_to_remove_.push_back(redund_col.idx);
      }
    }

    // Note: In [1], an enumeration to selected the best redundant columns to
    // remove is performed when the number of redundant columns is <= 10.
    // However, based on experiments with github.com/c4v4/cft/, it appears
    // that this enumeration does not provide significant benefits to justify
    // the added complexity.

    if (partial_cost_ < cost_cutoff) {
      gtl::STLEraseAllFromSequenceIf(&sol_subsets, [&](auto j) {
        return absl::c_any_of(cols_to_remove_,
                              [j](auto j_r) { return j_r == j; });
      });
    }
    return partial_cost_;
  }

 private:
  // Remove redundant columns from the redundancy set using a heuristic
  void HeuristicRedundancyRemoval(const SubModel& model, Cost cost_cutoff) {
    while (!redund_set_.empty()) {
      if (partial_cov_count_ == model.num_focus_elements()) {
        return;
      }
      SubsetIndex j = redund_set_.back().idx;
      const auto& j_col = model.columns()[j];
      redund_set_.pop_back();

      if (total_coverage_.IsRedundantUncover(j_col)) {
        total_coverage_.Uncover(j_col);
        cols_to_remove_.push_back(j);
      } else {
        partial_cost_ += model.subset_costs()[j];
        partial_cov_count_ += partial_coverage_.Cover(j_col);
      }
    }
  }

 private:
  // redundant columns + their cost
  std::vector<Score> redund_set_;

  // row-cov if all the remaining columns are selected
  CoverCounters total_coverage_;

  // row-cov if we selected the current column
  CoverCounters partial_coverage_;

  // current partial solution cost
  Cost partial_cost_;

  // current partial solution size
  BaseInt partial_size_;

  // number of covered rows
  Cost partial_cov_count_;

  // list of columns to remove
  std::vector<SubsetIndex> cols_to_remove_;
};

}  // namespace

Solution RunMultiplierBasedGreedy(const SubModel& model,
                                  const DualState& dual_state,
                                  Cost cost_cutoff) {
  std::vector<SubsetIndex> sol_subsets;
  CoverGreedly(model, dual_state, cost_cutoff,
               std::numeric_limits<BaseInt>::max(), sol_subsets);
  return Solution(model, sol_subsets);
}

Cost CoverGreedly(const SubModel& model, const DualState& dual_state,
                  Cost cost_cutoff, BaseInt stop_size,
                  std::vector<SubsetIndex>& sol_subsets) {
  Cost sol_cost = .0;
  for (SubsetIndex j : sol_subsets) {
    sol_cost += model.subset_costs()[j];
  }
  if (sol_cost >= cost_cutoff) {
    sol_subsets.clear();
    return std::numeric_limits<Cost>::max();
  }
  if (sol_subsets.size() >= stop_size) {
    // Solution already has required size -> early exit
    return sol_cost;
  }

  // Process input solution (if not empty)
  BaseInt num_rows_to_cover = model.num_focus_elements();
  CoverCounters covered_rows(model.num_elements());
  for (SubsetIndex j : sol_subsets) {
    num_rows_to_cover -= covered_rows.Cover(model.columns()[j]);
    if (num_rows_to_cover == 0) {
      return sol_cost;
    }
  }

  // Initialize column scores taking into account rows already covered
  GreedyScores scores(model, dual_state);  // TODO(?): cache it!
  if (!sol_subsets.empty()) {
    scores.UpdateColumnsScoreOfRowsIf(
        model.rows(), dual_state.multipliers(), model.ElementRange(),
        [&](auto i) { return covered_rows[i] > 0; });
  }

  // Fill up partial solution
  while (num_rows_to_cover > 0 && sol_subsets.size() < stop_size) {
    SubsetIndex j_star = scores.FindMinScoreColumn(model, dual_state);
    const auto& column = model.columns()[j_star];
    num_rows_to_cover -= scores.UpdateColumnsScoreOfRowsIf(
        model.rows(), dual_state.multipliers(), column,
        [&](auto i) { return covered_rows[i] == 0; });
    sol_subsets.push_back(j_star);
    covered_rows.Cover(column);
  }

  // Either remove redundant columns or discard solution
  RedundancyRemover remover(model, covered_rows);  // TODO(?): cache it!
  return remover.TryRemoveRedundantCols(model, cost_cutoff, sol_subsets);
}

///////////////////////////////////////////////////////////////////////
//////////////////////// THREE PHASE ALGORITHM ////////////////////////
///////////////////////////////////////////////////////////////////////
namespace {

DualState MakeTentativeDualState(const SubModel& model) {
  DualState tentative_dual_state(model);
  tentative_dual_state.DualUpdate(
      model, [&](ElementIndex i, Cost& i_multiplier) {
        i_multiplier = std::numeric_limits<Cost>::max();
        for (SubsetIndex j : model.rows()[i]) {
          Cost candidate = model.subset_costs()[j] / model.column_size(j);
          i_multiplier = std::min(i_multiplier, candidate);
        }
      });
  return tentative_dual_state;
}

void FixBestColumns(SubModel& model, PrimalDualState& state) {
  auto& [best_sol, dual_state] = state;

  // This approach is not the most efficient but prioritizes clarity and the
  // current abstraction system. We save the current core multipliers, mapped to
  // the full model's element indices. By organizing the multipliers using the
  // full model indices, we can easily map them to the new core model indices
  // after fixing columns. Note: This mapping isn't strictly necessary because
  // fixing columns effectively removes rows, and the remaining multipliers
  // naturally shift to earlier positions. A simple iteration would suffice to
  // discard multipliers for rows no longer in the new core model.
  FullElementCostVector full_multipliers(model.num_elements(), .0);
  for (ElementIndex core_i : model.ElementRange()) {
    FullElementIndex full_i = model.MapCoreToFullElementIndex(core_i);
    full_multipliers[full_i] = dual_state.multipliers()[core_i];
  }

  std::vector<SubsetIndex> cols_to_fix;
  CoverCounters row_coverage(model.num_elements());
  for (SubsetIndex j : model.SubsetRange()) {
    if (dual_state.reduced_costs()[j] < -0.001) {
      cols_to_fix.push_back(j);
      row_coverage.Cover(model.columns()[j]);
    }
  }

  // Remove columns that overlap between each other
  gtl::STLEraseAllFromSequenceIf(&cols_to_fix, [&](SubsetIndex j) {
    return absl::c_any_of(model.columns()[j],
                          [&](ElementIndex i) { return row_coverage[i] > 1; });
  });

  // Ensure at least a minimum number of columns are fixed
  BaseInt fix_at_least =
      cols_to_fix.size() + std::max<BaseInt>(1, model.num_elements() / 200);
  CoverGreedly(model, state.dual_state, std::numeric_limits<Cost>::max(),
               fix_at_least, cols_to_fix);

  // Fix columns and update the model
  Cost fixed_cost_delta = model.FixColumns(cols_to_fix);

  std::cout << "Fixed " << cols_to_fix.size()
            << " new columns with cost: " << fixed_cost_delta << '\n';
  std::cout << "Globally fixed " << model.fixed_columns().size()
            << " columns, with cost " << model.fixed_cost() << '\n';

  // Update multipliers for the reduced model
  dual_state.DualUpdate(model, [&](ElementIndex core_i, Cost& multiplier) {
    // Note: if SubModelView is used as CoreModel, then this mapping is always
    // the identity mapping and can be removed
    multiplier = full_multipliers[model.MapCoreToFullElementIndex(core_i)];
  });
}

void RandomizeDualState(const SubModel& model, DualState& dual_state,
                        std::mt19937& rnd) {
  dual_state.DualUpdate(model, [&](ElementIndex, Cost& i_multiplier) {
    i_multiplier *= absl::Uniform(rnd, 0.9, 1.1);
  });
}
}  // namespace

void HeuristicCBs::RunHeuristic(const SubgradientContext& context,
                                Solution& solution) {
  solution = RunMultiplierBasedGreedy(
      context.model, context.current_dual_state,
      context.best_solution.cost() - context.model.fixed_cost());
}

void HeuristicCBs::ComputeMultipliersDelta(const SubgradientContext& context,
                                           ElementCostVector& delta_mults) {
  Cost squared_norm = .0;
  for (ElementIndex i : context.model.ElementRange()) {
    squared_norm += context.subgradient[i] * context.subgradient[i];
  }

  Cost lower_bound = context.current_dual_state.lower_bound();
  Cost upper_bound = context.best_solution.cost() - context.model.fixed_cost();
  DCHECK_GE(upper_bound, lower_bound);
  Cost delta = upper_bound - lower_bound;
  Cost step_constant = step_size_ * delta / squared_norm;
  for (ElementIndex i : context.model.ElementRange()) {
    delta_mults[i] = step_constant * context.subgradient[i];
  }
}

PrimalDualState RunThreePhase(SubModel& model, const Solution& init_solution) {
  DCHECK(ValidateSubModel(model));

  PrimalDualState best_state = {.solution = init_solution,
                                .dual_state = MakeTentativeDualState(model)};
  if (best_state.solution.Empty()) {
    best_state.solution =
        RunMultiplierBasedGreedy(model, best_state.dual_state);
  }
  std::cout << "Initial lower bound: " << best_state.dual_state.lower_bound()
            << "\nInitial solution cost: " << best_state.solution.cost()
            << "\nStarting 3-phase algorithm\n";

  PrimalDualState curr_state = best_state;
  BaseInt iter_count = 0;
  std::mt19937 rnd(0xcf7);
  while (model.num_focus_elements() > 0 &&
         // note: assumes integral costs
         curr_state.dual_state.lower_bound() <
             best_state.solution.cost() - model.fixed_cost() - .999) {
    ++iter_count;
    std::cout << "\n\n3Phase iteration: " << iter_count << '\n';
    std::cout << " Active size: rows " << model.num_focus_elements() << "/"
              << model.num_elements() << ", columns "
              << model.num_focus_subsets() << "/" << model.num_subsets()
              << '\n';

    // Phase 1: refine the current dual_state and model
    BoundCBs dual_bound_cbs(model);
    std::cout << "\nSubgradient Phase:\n";
    SubgradientOptimization(model, dual_bound_cbs, curr_state);
    if (iter_count == 1) {
      best_state.dual_state = curr_state.dual_state;
    }
    // Phase 2: search for good solutions
    HeuristicCBs heuristic_cbs;
    heuristic_cbs.set_step_size(dual_bound_cbs.step_size());
    std::cout << "\nHeuristic Phase:\n";
    SubgradientOptimization(model, heuristic_cbs, curr_state);
    if (curr_state.solution.cost() < best_state.solution.cost()) {
      best_state.solution = curr_state.solution;
    }

    std::cout << "\n3Phase Bounds: Lower "
              << best_state.dual_state.lower_bound() << ", Upper "
              << best_state.solution.cost() << '\n';

    // Phase 3: Fix the best columns (diving)
    FixBestColumns(model, curr_state);
    RandomizeDualState(model, curr_state.dual_state, rnd);
  }

  std::cout << "\n\n3Phase End\n";
  std::cout << "Final Bounds: Lower " << best_state.dual_state.lower_bound()
            << ", Upper " << best_state.solution.cost() << '\n';

  return best_state;
}

///////////////////////////////////////////////////////////////////////
//////////////////////// FULL TO CORE PRICING /////////////////////////
///////////////////////////////////////////////////////////////////////

namespace {
std::vector<FullSubsetIndex> ComputeTentativeFocus(StrongModelView full_model) {
  FullSubsetBoolVector selected(full_model.num_subsets(), false);
  std::vector<FullSubsetIndex> columns_focus;
  columns_focus.reserve(full_model.num_elements() * kMinCov);

  // Select the first min_row_coverage columns for each row
  for (const auto& row : full_model.rows()) {
    BaseInt countdown = kMinCov;
    for (FullSubsetIndex j : row) {
      if (--countdown <= 0) {
        break;
      }
      if (!selected[j]) {
        selected[j] = true;
        columns_focus.push_back(j);
      }
    }
  }
  // NOTE: unnecessary, but it keeps equivalence between SubModelView/SubModel
  absl::c_sort(columns_focus);
  return columns_focus;
}

void SelecteMinRedCostColumns(FilterModelView full_model,
                              const SubsetCostVector& reduced_costs,
                              std::vector<FullSubsetIndex>& new_core_columns,
                              FullSubsetBoolVector& selected) {
  DCHECK_EQ(reduced_costs.size(), full_model.num_subsets());
  DCHECK_EQ(selected.size(), full_model.num_subsets());

  std::vector<SubsetIndex> candidates;
  for (SubsetIndex j : full_model.SubsetRange())
    if (reduced_costs[j] < 0.1) {
      candidates.push_back(j);
    }

  BaseInt max_size = 5 * full_model.num_focus_elements();
  if (candidates.size() > max_size) {
    absl::c_nth_element(candidates, candidates.begin() + max_size - 1,
                        [&](SubsetIndex j1, SubsetIndex j2) {
                          return reduced_costs[j1] < reduced_costs[j2];
                        });
    candidates.resize(max_size);
  }
  for (SubsetIndex j : candidates) {
    FullSubsetIndex j_full = static_cast<FullSubsetIndex>(j);
    if (!selected[j_full]) {
      selected[j_full] = true;
      new_core_columns.push_back(j_full);
    }
  }
}

static void SelectMinRedCostByRow(FilterModelView full_model,
                                  const SubsetCostVector& reduced_costs,
                                  std::vector<FullSubsetIndex>& columns_map,
                                  FullSubsetBoolVector& selected) {
  DCHECK_EQ(reduced_costs.size(), full_model.num_subsets());
  DCHECK_EQ(selected.size(), full_model.num_subsets());

  for (const auto& row : full_model.rows()) {
    // Collect best `kMinCov` columns covering row `i`
    SubsetIndex best_cols[kMinCov];
    BaseInt best_size = 0;
    for (SubsetIndex j : row) {
      if (best_size < kMinCov) {
        best_cols[best_size++] = j;
        continue;
      }
      if (reduced_costs[j] < reduced_costs[best_cols[kMinCov - 1]]) {
        BaseInt n = kMinCov - 1;
        while (n > 0 && reduced_costs[j] < reduced_costs[best_cols[n - 1]]) {
          best_cols[n] = best_cols[n - 1];
          --n;
        }
        best_cols[n] = j;
      }
    }

    DCHECK(best_size > 0);
    for (BaseInt s = 0; s < best_size; ++s) {
      FullSubsetIndex j = static_cast<FullSubsetIndex>(best_cols[s]);
      if (!selected[j]) {
        selected[j] = true;
        columns_map.push_back(j);
      }
    }
  }
}
}  // namespace

FullToCoreModel::FullToCoreModel(const Model* full_model)
    : SubModel(full_model, ComputeTentativeFocus(StrongModelView(full_model))),
      full_model_(full_model),
      is_focus_col_(full_model->num_subsets(), true),
      is_focus_row_(full_model->num_elements(), true),
      num_subsets_(full_model->num_subsets()),
      num_elements_(full_model->num_elements()),
      full_dual_state_(*full_model),
      best_dual_state_(full_dual_state_) {
  ResetPricingPeriod();
  DCHECK(ValidateSubModel(*this));
  DCHECK(FullToSubModelInvariantCheck());
}

void FullToCoreModel::ResetPricingPeriod() {
  update_countdown_ = 10;
  update_period_ = 10;
  update_max_period_ = std::min<BaseInt>(1000, full_model_->num_elements() / 3);
}

Cost FullToCoreModel::FixColumns(
    const std::vector<SubsetIndex>& columns_to_fix) {
  StrongModelView typed_full_model = StrongTypedFullModelView();
  for (SubsetIndex core_j : columns_to_fix) {
    FullSubsetIndex full_j = SubModel::MapCoreToFullSubsetIndex(core_j);
    IsFocusCol(full_j) = false;
    for (FullElementIndex full_i : typed_full_model.columns()[full_j]) {
      IsFocusRow(full_i) = false;
    }
  }
  for (FullSubsetIndex full_j : typed_full_model.SubsetRange()) {
    if (!IsFocusCol(full_j)) {
      continue;
    }
    IsFocusCol(full_j) = false;
    for (FullElementIndex full_i : typed_full_model.columns()[full_j]) {
      if (IsFocusRow(full_i)) {
        IsFocusCol(full_j) = true;
        break;
      }
    }
  }
  ResetPricingPeriod();
  Cost fixed_cost = base::FixColumns(columns_to_fix);
  DCHECK(FullToSubModelInvariantCheck());
  return fixed_cost;
}

bool FullToCoreModel::UpdateCore(PrimalDualState& core_state) {
  if (--update_countdown_ > 0) {
    return false;
  }

  UpdateDualState(core_state.dual_state, full_dual_state_, best_dual_state_);
  UpdatePricingPeriod(full_dual_state_, core_state);
  std::cout << "Lower bounds: Real " << full_dual_state_.lower_bound()
            << ", Core " << core_state.dual_state.lower_bound() << '\n';

  auto fixing_full_model = FixingFullModelView();
  FullSubsetBoolVector selected_columns(fixing_full_model.num_subsets(), false);
  std::vector<FullSubsetIndex> new_core_columns;

  // Always retain best solution in the core model
  for (FullSubsetIndex full_j : core_state.solution.subsets()) {
    if (IsFocusCol(full_j)) {
      new_core_columns.push_back(full_j);
      selected_columns[full_j] = true;
    }
  }

  SelecteMinRedCostColumns(fixing_full_model, full_dual_state_.reduced_costs(),
                           new_core_columns, selected_columns);
  SelectMinRedCostByRow(fixing_full_model, full_dual_state_.reduced_costs(),
                        new_core_columns, selected_columns);

  // NOTE: unnecessary, but it keeps equivalence between SubModelView/SubModel
  absl::c_sort(new_core_columns);
  SetFocus(new_core_columns);
  core_state.dual_state.DualUpdate(*this, [](ElementIndex i, Cost& i_mult) {
    // multipliers didn't cange, but reduced cost must be recomputed
  });

  DCHECK(FullToSubModelInvariantCheck());
  return true;
}

void FullToCoreModel::UpdatePricingPeriod(const DualState& full_dual_state,
                                          const PrimalDualState& core_state) {
  DCHECK_GE(core_state.dual_state.lower_bound() + 1e-6,
            full_dual_state.lower_bound());
  DCHECK_GE(core_state.solution.cost(), .0);

  Cost delta =
      core_state.dual_state.lower_bound() - full_dual_state.lower_bound();
  Cost ratio = DivideIfGE0(delta, core_state.solution.cost());
  if (ratio <= 1e-6) {
    update_period_ = std::min<BaseInt>(update_max_period_, 10 * update_period_);
  } else if (ratio <= 0.02) {
    update_period_ = std::min<BaseInt>(update_max_period_, 5 * update_period_);
  } else if (ratio <= 0.2) {
    update_period_ = std::min<BaseInt>(update_max_period_, 2 * update_period_);
  } else {
    update_period_ = 10;
  }
  update_countdown_ = update_period_;
}

void FullToCoreModel::UpdateDualState(const DualState& core_dual_state,
                                      DualState& full_dual_state,
                                      DualState& best_dual_state) {
  auto fixing_full_model = FixingFullModelView();
  full_dual_state_.DualUpdate(
      fixing_full_model, [&](ElementIndex full_i, Cost& i_mult) {
        ElementIndex core_i =
            MapFullToCoreElementIndex(static_cast<FullElementIndex>(full_i));
        i_mult = core_dual_state.multipliers()[core_i];
      });

  // Here, we simply check if any columns have been fixed, and only update the
  // best dual state when no fixing is in place.
  //
  // Mapping a "local" dual state to a global one is possible. This would
  // involve keeping the multipliers for non-focused elements fixed, updating
  // the multipliers for focused elements, and then computing the dual state for
  // the entire model. However, this approach is not implemented here. Such a
  // strategy is unlikely to improve the best dual state unless the focus is
  // *always* limited to a small subset of elements (and therefore the LB sucks
  // and it is easy to improve) and the CFT is applied exclusively within that
  // narrow scope, but this falls outside the current scope of this project.
  if (fixed_columns().empty() &&
      full_dual_state_.lower_bound() > best_dual_state_.lower_bound()) {
    best_dual_state_ = full_dual_state_;
  }
}

bool FullToCoreModel::FullToSubModelInvariantCheck() {
  const SubModel& sub_model = *this;
  StrongModelView typed_full_model = StrongTypedFullModelView();

  if (typed_full_model.num_subsets() < sub_model.num_subsets()) {
    std::cerr << absl::StrCat("SubModelView has ", sub_model.num_subsets(),
                              " subsets, but the full model has ",
                              typed_full_model.num_subsets(), " subsets.\n");
    return false;
  }
  if (typed_full_model.num_elements() != sub_model.num_elements()) {
    std::cerr << absl::StrCat("SubModelView has ", sub_model.num_elements(),
                              " elements, but the full model has ",
                              typed_full_model.num_elements(), " elements.\n");
    return false;
  }
  for (SubsetIndex core_j : sub_model.SubsetRange()) {
    FullSubsetIndex full_j = sub_model.MapCoreToFullSubsetIndex(core_j);
    if (!is_focus_col_[static_cast<SubsetIndex>(full_j)]) {
      std::cerr << absl::StrCat("Subset ", core_j,
                                " in sub-model but its mapped subset ", full_j,
                                " not found in full model view.\n");
      return false;
    }
  }
  for (ElementIndex core_i : sub_model.ElementRange()) {
    FullElementIndex full_i = sub_model.MapCoreToFullElementIndex(core_i);
    if (!is_focus_row_[static_cast<ElementIndex>(full_i)]) {
      std::cerr << absl::StrCat("Element ", core_i,
                                " in sub-model but its mapped element ", full_i,
                                " not found in full model view.\n");
      return false;
    }
  }
  for (FullElementIndex full_i : typed_full_model.ElementRange()) {
    if (!IsFocusRow(full_i)) {
      continue;
    }
    ElementIndex core_i = sub_model.MapFullToCoreElementIndex(full_i);
    if (core_i < ElementIndex() ||
        ElementIndex(sub_model.num_elements()) < core_i) {
      std::cerr << absl::StrCat(
          "Element ", full_i,
          " in full model view but has no mapped element in sub-model.\n");
      return false;
    }
  }
  return true;
}

}  // namespace operations_research::scp