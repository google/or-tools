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
#include <absl/types/span.h>

#include <limits>

#include "ortools/base/accurate_sum.h"
#include "ortools/base/status_macros.h"
#include "ortools/base/stl_util.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/util/fp_utils.h"

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
    assert(i < cov_counters.size());
    return cov_counters[i];
  }

  template <typename IterableT>
  BaseInt Cover(IterableT const& subset) {
    BaseInt covered = 0;
    for (ElementIndex i : subset) {
      covered += cov_counters[i] == 0 ? 1ULL : 0ULL;
      cov_counters[i]++;
    }
    return covered;
  }

  template <typename IterableT>
  BaseInt Uncover(IterableT const& subset) {
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

Cost ComputeReducedCosts(const Model& model,
                         const ElementCostVector& multipliers,
                         SubsetCostVector& reduced_costs) {
  // Compute new reduced costs (O(nnz))
  Cost negative_sum = .0;
  for (SubsetIndex j : model.SubsetRange()) {
    reduced_costs[j] = model.subset_costs()[j];
    for (ElementIndex i : model.columns()[j]) {
      reduced_costs[j] -= multipliers[i];
    }
    if (reduced_costs[j] < .0) {
      negative_sum += reduced_costs[j];
    }
  }
  return negative_sum;
}

DualState::DualState(const Model& model)
    : lower_bound_(),
      multipliers_(model.num_elements(), std::numeric_limits<Cost>::max()),
      reduced_costs_(model.subset_costs()) {
  DualUpdate(model, [&](ElementIndex i, Cost& i_multiplier) {
    for (SubsetIndex j : model.rows()[i]) {
      Cost candidate = model.subset_costs()[j] / model.columns()[j].size();
      i_multiplier = std::min(i_multiplier, candidate);
    }
  });
}

absl::Status ValidateModel(const Model& model) {
  if (model.rows().size() != model.num_elements()) {
    return absl::InvalidArgumentError("Model has no rows.");
  }
  if (model.columns().size() != model.num_subsets()) {
    return absl::InvalidArgumentError("Model has no columns.");
  }
  if (model.subset_costs().size() != model.num_subsets()) {
    return absl::InvalidArgumentError("Model has no subset costs.");
  }
  if (model.num_elements() <= 0) {
    return absl::InvalidArgumentError("Model has no elements.");
  }
  if (model.num_subsets() <= 0) {
    return absl::InvalidArgumentError("Model has no subsets.");
  }
  if (!model.ComputeFeasibility()) {
    return absl::InvalidArgumentError("Model is infeasible.");
  }
  return absl::OkStatus();
}

absl::Status ValidateFeasibleSolution(const Model& model,
                                      const Solution& solution,
                                      Cost tolerance) {
  AccurateSum<Cost> solution_cost;
  CoverCounters coverage(model.num_elements());
  const SparseColumnView& columns = model.columns();
  const SubsetCostVector& subset_costs = model.subset_costs();
  for (SubsetIndex subset : solution.subsets()) {
    solution_cost.Add(subset_costs[subset]);
    coverage.Cover(columns[subset]);
  }
  if (!AreWithinAbsoluteOrRelativeTolerances(
          solution_cost.Value(), solution.cost(), tolerance, tolerance))
    return absl::InvalidArgumentError("Solution cost is incorrect.");
  for (ElementIndex element : model.ElementRange()) {
    if (coverage[element] == 0) {
      return absl::InvalidArgumentError("Solution is infeasible.");
    }
  }
  return absl::OkStatus();
}

///////////////////////////////////////////////////////////////////////
///////////////////////////// SUBGRADIENT /////////////////////////////
///////////////////////////////////////////////////////////////////////

BoundCBs::BoundCBs(const Model& model)
    : squared_norm_(static_cast<Cost>(model.num_elements())),
      direction_(ElementCostVector(model.num_elements(), .0)),
      prev_best_lb_(std::numeric_limits<Cost>::lowest()),
      max_iter_countdown_(10 * model.num_elements()),  // Arbitrary from [1]
      exit_test_countdown_(300),                       // Arbitrary from [1]
      exit_test_period_(300),                          // Arbitrary from [1]
      step_size_(0.1),                                 // Arbitrary from [1]
      last_min_lb_seen_(std::numeric_limits<Cost>::max()),
      last_max_lb_seen_(.0),
      step_size_update_countdown_(20),  // Arbitrary from [1]
      step_size_update_period_(20)      // Arbitrary from [1]
{}

bool BoundCBs::ExitCondition(const SubgradientContext& context) {
  if (--max_iter_countdown_ <= 0 || squared_norm_ <= kTol) {
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

  squared_norm_ = .0;
  for (ElementIndex i : context.model.ElementRange()) {
    squared_norm_ += direction_[i] * direction_[i];
  }

  Cost upper_bound = context.best_solution.cost();
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
    if (gap <= .001) {       //
      step_size_ *= 1.5;     // Arbitray
    } else if (gap > .01) {  // from [1]
      step_size_ /= 2.0;     //
    }
    last_min_lb_seen_ = std::numeric_limits<Cost>::max();
    last_max_lb_seen_ = .0;
    // Not described in the paper, but in rare cases the subgradient diverges
    step_size_ = std::clamp(step_size_, 1e-6, 10.0);
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

  const SparseColumnView& cols = context.model.columns();
  for (SubsetIndex j : lagrangian_solution_) {
    if (absl::c_all_of(cols[j], [&](auto i) { return subgradient[i] < 0; })) {
      absl::c_for_each(cols[j], [&](auto i) { subgradient[i] += 1.0; });
    }
  }
}

bool BoundCBs::UpdateCoreModel(CoreModel& core_model,
                               PrimalDualState& best_state) {
  if (core_model.UpdateCore(best_state)) {
    // grant at least 10 iterations before the next exit test
    prev_best_lb_ =
        std::min(prev_best_lb_, best_state.dual_state.lower_bound());
    exit_test_countdown_ = std::max(exit_test_countdown_, 10);
    max_iter_countdown_ = std::max(max_iter_countdown_, 10);
    return true;
  }
  return false;
}

void SubgradientOptimization(CoreModel& model, SubgradientCBs& cbs,
                             PrimalDualState& best_state) {
  DCHECK_OK(ValidateModel(model));
  DCHECK_OK(ValidateFeasibleSolution(model, best_state.solution));

  ElementCostVector subgradient = ElementCostVector(model.num_elements(), .0);
  DualState dual_state = best_state.dual_state;
  Solution solution = best_state.solution;

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
      i_mult = std::max(.0, i_mult + multipliers_delta[i]);
    });
    if (dual_state.lower_bound() > best_state.dual_state.lower_bound()) {
      best_state.dual_state = dual_state;
    }

    cbs.RunHeuristic(context, solution);
    if (!solution.subsets().empty() &&
        solution.cost() < best_state.solution.cost()) {
      best_state.solution = solution;
    }

    if (cbs.UpdateCoreModel(model, best_state)) {
      std::cout << "Core model has been updated.\n";
      dual_state = best_state.dual_state;
    }

    std::cout << "Subgradient Iteration: " << ++iter
              << "\n  Lower bound:      " << dual_state.lower_bound()
              << " (best: " << best_state.dual_state.lower_bound()
              << ")\n  Solution cost: " << best_state.solution.cost() << "\n";
  }
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

  GreedyScores(const Model& model, const DualState& dual_state)
      : bad_size_(),
        worst_good_score_(std::numeric_limits<Cost>::lowest()),
        scores_(),
        reduced_costs_(dual_state.reduced_costs()),
        covering_counts_(model.num_subsets()),
        score_map_(model.num_subsets()) {
    BaseInt s = 0;
    for (SubsetIndex j : model.SubsetRange()) {
      covering_counts_[j] = model.columns()[j].size();
      Cost j_score = ComputeScore(reduced_costs_[j], covering_counts_[j]);
      scores_.push_back({j_score, j});
      score_map_[j] = s++;
      DCHECK(std::isfinite(reduced_costs_[j]));
      DCHECK(std::isfinite(j_score));
    }
    bad_size_ = scores_.size();
  }

  SubsetIndex FindMinScoreColumn(const Model& model,
                                 const DualState& dual_state) {
    // Check if the bad/good partition should be updated
    if (bad_size_ == scores_.size()) {
      if (bad_size_ > model.num_elements()) {
        bad_size_ = bad_size_ - model.num_elements();
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
  template <typename ElementSpanT, typename CondT>
  BaseInt UpdateColumnsScoreOfRowsIf(const SparseRowView& rows,
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
  RedundancyRemover(const Model& model, CoverCounters& total_coverage)
      : redund_set_(),
        total_coverage_(total_coverage),
        partial_coverage_(model.num_elements()),
        partial_cost_(.0),
        partial_size_(0),
        partial_cov_count_(0),
        cols_to_remove_() {}

  Cost TryRemoveRedundantCols(const Model& model, Cost cost_cutoff,
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

    if (partial_cov_count_ < model.num_elements()) {
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
  void HeuristicRedundancyRemoval(const Model& model, Cost cost_cutoff) {
    while (!redund_set_.empty()) {
      if (partial_cov_count_ == model.num_elements()) {
        return;
      }
      SubsetIndex j = redund_set_.back().idx;
      const SparseColumn& j_col = model.columns()[j];
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

Solution RunMultiplierBasedGreedy(const Model& model,
                                  const DualState& dual_state,
                                  Cost cost_cutoff) {
  std::vector<SubsetIndex> sol_subsets;
  CoverGreedly(model, dual_state, cost_cutoff,
               std::numeric_limits<BaseInt>::max(), sol_subsets);
  return Solution(model, std::move(sol_subsets));
}

Cost CoverGreedly(const Model& model, const DualState& dual_state,
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
  BaseInt num_rows_to_cover = model.num_elements();
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
    const SparseColumn& column = model.columns()[j_star];
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

void FixColumns(CoreModel& model, const std::vector<SubsetIndex>& cols_to_fix,
                ElementMapVector& new_to_old_map) {
  // TOD(c4v4): implement
}

void FixBestColumns(CoreModel& model, PrimalDualState& state) {
  auto& [best_sol, dual_state] = state;

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
      cols_to_fix.size() + std::max(1, model.num_elements() / 200);
  CoverGreedly(model, state.dual_state, std::numeric_limits<Cost>::max(),
               fix_at_least, cols_to_fix);

  // Fix columns and update the model
  ElementMapVector new_to_old_map;
  FixColumns(model, cols_to_fix, new_to_old_map);  // TODO(c4v4): implement

  // Update multipliers for the reduced model
  dual_state.DualUpdate(model, [&](ElementIndex i, Cost& i_mult) {
    i_mult = state.dual_state.multipliers()[new_to_old_map[i]];
  });
}

void RandomizeDualState(const Model& model, DualState& dual_state,
                        absl::BitGen& rnd) {
  dual_state.DualUpdate(model, [&](ElementIndex, Cost i_multiplier) {
    i_multiplier *= absl::Uniform(rnd, 0.9, 1.1);
  });
}
}  // namespace

void HeuristicCBs::RunHeuristic(const SubgradientContext& context,
                                Solution& solution) {
  solution = RunMultiplierBasedGreedy(context.model, context.current_dual_state,
                                      context.best_solution.cost());
}

void HeuristicCBs::ComputeMultipliersDelta(const SubgradientContext& context,
                                           ElementCostVector& delta_mults) {
  Cost squared_norm = .0;
  for (ElementIndex i : context.model.ElementRange()) {
    squared_norm += context.subgradient[i] * context.subgradient[i];
  }

  Cost lower_bound = context.current_dual_state.lower_bound();
  Cost upper_bound = context.best_solution.cost();
  DCHECK_GE(upper_bound, lower_bound);
  Cost delta = upper_bound - lower_bound;
  Cost step_constant = step_size_ * delta / squared_norm;
  for (ElementIndex i : context.model.ElementRange()) {
    delta_mults[i] = step_constant * context.subgradient[i];
  }
}

absl::StatusOr<PrimalDualState> RunThreePhase(CoreModel& model,
                                              const Solution& init_solution) {
  model.CreateSparseRowView();
  RETURN_IF_ERROR(ValidateModel(model));

  PrimalDualState best_state = {.solution = init_solution,
                                .dual_state = DualState(model)};
  if (best_state.solution.Empty()) {
    best_state.solution =
        RunMultiplierBasedGreedy(model, best_state.dual_state);
  }
  RETURN_IF_ERROR(ValidateFeasibleSolution(model, best_state.solution));
  std::cout << "Initial lower bound: " << best_state.dual_state.lower_bound()
            << "\nInitial solution cost: " << best_state.solution.cost()
            << "\nStarting 3-phase algorithm\n";

  PrimalDualState curr_state = best_state;
  BoundCBs dual_bound_cbs(model);
  HeuristicCBs heuristic_cbs;
  BaseInt iter_count = 0;
  absl::BitGen rnd;
  while (model.num_elements() > 0) {
    ++iter_count;

    // Phase 1: refine the current dual_state and model
    SubgradientOptimization(model, dual_bound_cbs, curr_state);
    if (iter_count == 1) {
      best_state.dual_state = curr_state.dual_state;
    }
    // Phase 2: search for good solutions
    heuristic_cbs.set_step_size(dual_bound_cbs.step_size());
    SubgradientOptimization(model, heuristic_cbs, curr_state);
    if (curr_state.solution.cost() < best_state.solution.cost()) {
      best_state = curr_state;
    }
    std::cout << "Iterartion " << iter_count
              << "\n - Lower bound: " << curr_state.dual_state.lower_bound()
              << "\n - Solution cost: " << curr_state.solution.cost() << '\n';

    // Phase 3: Fix the best columns (diving)
    FixBestColumns(model, curr_state);
    RandomizeDualState(model, curr_state.dual_state, rnd);
  }

  return best_state;
}

///////////////////////////////////////////////////////////////////////
//////////////////////// FULL TO CORE PRICING /////////////////////////
///////////////////////////////////////////////////////////////////////

namespace {
void ExtractCoreModel(const Model& full_model,
                      const SubsetMapVector& columns_map, Model& core_model) {
  // Fill core model with the selected columns
  core_model = Model();
  core_model.ReserveNumSubsets(columns_map.size());
  for (SubsetIndex core_j : core_model.SubsetRange()) {
    SubsetIndex full_j = columns_map[core_j];
    const SparseColumn& full_column = full_model.columns()[full_j];
    core_model.SetSubsetCost(core_j, full_model.subset_costs()[full_j]);
    core_model.ReserveNumElementsInSubset(full_column.size(), core_j.value());
    DCHECK(core_model.columns()[core_j].empty());
    for (ElementIndex i : full_column) {
      core_model.AddElementToSubset(i, core_j);
    }
  }
  core_model.CreateSparseRowView();
}

static constexpr BaseInt kMinCov = 5;
void FillTentativeCoreModel(const Model& full_model,
                            SubsetMapVector& columns_map, Model& core_model) {
  SubsetBoolVector selected(full_model.num_subsets(), false);
  columns_map.reserve(full_model.num_elements() * kMinCov);

  // Select the first min_row_coverage columns for each row
  for (const SparseRow& row : full_model.rows()) {
    BaseInt countdown = kMinCov;
    for (SubsetIndex j : row) {
      if (--countdown > 0 && !selected[j]) {
        selected[j] = true;
        columns_map.push_back(j);
      }
    }
  }
  ExtractCoreModel(full_model, columns_map, core_model);
}

void SelecteMinRedCostColumns(const Model& full_model,
                              const SubsetCostVector& reduced_costs,
                              SubsetMapVector& columns_map,
                              SubsetBoolVector& selected) {
  DCHECK_EQ(reduced_costs.size(), full_model.num_subsets());
  DCHECK_EQ(selected.size(), full_model.num_subsets());
  SubsetMapVector candidates;
  for (SubsetIndex j : full_model.SubsetRange())
    if (reduced_costs[j] < 0.1) {
      candidates.push_back(j);
    }

  BaseInt max_size = 5 * full_model.num_elements();
  if (candidates.size() > max_size) {
    absl::c_nth_element(candidates, candidates.begin() + max_size - 1,
                        [&](SubsetIndex j1, SubsetIndex j2) {
                          return reduced_costs[j1] < reduced_costs[j2];
                        });
    candidates.resize(max_size);
  }
  for (SubsetIndex j : candidates) {
    if (!selected[j]) {
      selected[j] = true;
      columns_map.push_back(j);
    }
  }
}

static void SelectMinRedCostByRow(const Model& full_model,
                                  const SubsetCostVector& reduced_costs,
                                  SubsetMapVector& columns_map,
                                  SubsetBoolVector& selected) {
  DCHECK_EQ(reduced_costs.size(), full_model.num_subsets());
  DCHECK_EQ(selected.size(), full_model.num_subsets());

  for (ElementIndex i : full_model.ElementRange()) {
    // Collect best `kMinCov` columns covering row `i`
    SubsetIndex best_cols[kMinCov];
    BaseInt best_size = 0;
    for (SubsetIndex j : full_model.rows()[i]) {
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
      SubsetIndex j = best_cols[s];
      if (!selected[j]) {
        selected[j] = true;
        columns_map.push_back(j);
      }
    }
  }
}
}  // namespace

FullToCoreModel::FullToCoreModel(Model&& full_model)
    : CoreModel(),
      columns_map_(),
      full_model_(std::move(full_model)),
      full_dual_state_(full_model_),
      update_countdown_(10),
      update_period_(10),
      update_max_period_(std::min(1000, full_model_.num_elements() / 3)) {
  FillTentativeCoreModel(full_model_, columns_map_, static_cast<Model&>(*this));
  DCHECK_OK(ValidateModel(*this));
}

bool FullToCoreModel::UpdateCore(PrimalDualState& core_state) {
  if (--update_countdown_ > 0) {
    return false;
  }

  full_dual_state_.DualUpdate(full_model_, [&](ElementIndex i, Cost& i_mult) {
    i_mult = core_state.dual_state.multipliers()[i];
  });
  UpdatePricingPeriod(full_dual_state_, core_state);

  SubsetBoolVector selected(full_model_.num_subsets(), false);
  SubsetMapVector old_column_map = std::move(columns_map_);
  columns_map_.clear();

  // Always retain best solution in the core model
  for (SubsetIndex& old_j : core_state.solution.subsets()) {
    SubsetIndex full_j = old_column_map[old_j];
    SubsetIndex new_j = SubsetIndex(columns_map_.size());
    columns_map_.push_back(full_j);
    selected[full_j] = true;
    old_j = new_j;
  }

  SelecteMinRedCostColumns(full_model_, full_dual_state_.reduced_costs(),
                           columns_map_, selected);
  SelectMinRedCostByRow(full_model_, full_dual_state_.reduced_costs(),
                        columns_map_, selected);
  ExtractCoreModel(full_model_, columns_map_, *this);
  core_state.dual_state.DualUpdate(*this, [&](ElementIndex i, Cost& i_mult) {
    i_mult = full_dual_state_.multipliers()[i];
  });

  DCHECK_OK(ValidateModel(*this));
  DCHECK_OK(ValidateFeasibleSolution(*this, core_state.solution));
  DCHECK_GE(core_state.dual_state.lower_bound(),
            full_dual_state_.lower_bound());
  std::cout << "Real bound: " << full_dual_state_.lower_bound() << '\n';
  return true;
}

void FullToCoreModel::UpdatePricingPeriod(const DualState& full_dual_state,
                                          const PrimalDualState& core_state) {
  DCHECK_GE(core_state.dual_state.lower_bound(), full_dual_state.lower_bound());
  DCHECK_GE(core_state.solution.cost(), .0);

  Cost delta =
      core_state.dual_state.lower_bound() - full_dual_state.lower_bound();
  Cost ratio = DivideIfGE0(delta, core_state.solution.cost());
  if (ratio <= 1e-6) {
    update_period_ = std::min(update_max_period_, 10 * update_period_);
  } else if (ratio <= 0.02) {
    update_period_ = std::min(update_max_period_, 5 * update_period_);
  } else if (ratio <= 0.2) {
    update_period_ = std::min(update_max_period_, 2 * update_period_);
  } else {
    update_period_ = 10;
  }
  update_countdown_ = update_period_;
}

}  // namespace operations_research::scp