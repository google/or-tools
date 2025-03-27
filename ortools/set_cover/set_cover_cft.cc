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
  RedundancyRemover(const Model& model, CoverCounters& total_coverage) {}

  Cost TryRemoveRedundantCols(const Model& model, Cost cost_cutoff,
                              std::vector<SubsetIndex>& sol_subsets);
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
                ElementMapVector& new_to_old_map);
void FixBestColumns(CoreModel& model, PrimalDualState& state);
void RandomizeDualState(const Model& model, DualState& dual_state,
                        absl::BitGen& rnd);
}  // namespace

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
  BoundCBs dual_bound_cbs;
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

}  // namespace operations_research::scp