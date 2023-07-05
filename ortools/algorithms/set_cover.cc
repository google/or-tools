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

#include "ortools/algorithms/set_cover.h"

#include <algorithm>
#include <limits>
#include <numeric>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/random.h"
#include "ortools/algorithms/set_cover_utils.h"
#include "ortools/base/logging.h"

namespace operations_research {

constexpr SubsetIndex kNotFound(-1);

// TrivialSolutionGenerator.

bool TrivialSolutionGenerator::NextSolution() {
  const SubsetIndex num_subsets(ledger_->model()->num_subsets());
  SubsetBoolVector choices(num_subsets, true);
  ledger_->LoadSolution(choices);
  return true;
}

// RandomSolutionGenerator.

bool RandomSolutionGenerator::NextSolution() {
  const SubsetIndex num_subsets(ledger_->model()->num_subsets());
  std::vector<SubsetIndex> random_subset_order(num_subsets.value());
  std::iota(random_subset_order.begin(), random_subset_order.end(),
            SubsetIndex(0));
  std::shuffle(random_subset_order.begin(), random_subset_order.end(),
               absl::BitGen());
  const ElementIndex num_elements(ledger_->model()->num_elements());
  for (const SubsetIndex subset : random_subset_order) {
    if (ledger_->num_elements_covered() == num_elements) {
      break;
    }
    if (ledger_->marginal_impacts(subset) != 0) {
      ledger_->Toggle(subset, true);
    }
  }
  DCHECK(ledger_->CheckConsistency());
  return true;
}

// GreedySolutionGenerator.

void GreedySolutionGenerator::UpdatePriorities(
    const std::vector<SubsetIndex>& impacted_subsets) {
  const SubsetCostVector& subset_costs = ledger_->model()->subset_costs();
  for (const SubsetIndex subset : impacted_subsets) {
    const ElementIndex marginal_impact(ledger_->marginal_impacts(subset));
    if (marginal_impact != 0) {
      const Cost marginal_cost_increase =
          subset_costs[subset] / marginal_impact.value();
      pq_.ChangePriority(subset, -marginal_cost_increase);
    } else {
      pq_.Remove(subset);
    }
  }
}

bool GreedySolutionGenerator::NextSolution() {
  const SubsetCostVector& subset_costs = ledger_->model()->subset_costs();
  const SubsetIndex num_subsets(ledger_->model()->num_subsets());
  ledger_->MakeDataConsistent();

  // The priority is the minimum marginal cost increase. Since the
  // priority queue returns the smallest value, we use the opposite.
  for (SubsetIndex subset(0); subset < num_subsets; ++subset) {
    if (!ledger_->is_selected(subset) &&
        ledger_->marginal_impacts(subset) != 0) {
      const Cost marginal_cost_increase =
          subset_costs[subset] / ledger_->marginal_impacts(subset).value();
      pq_.Add(subset, -marginal_cost_increase);
    }
  }
  const ElementIndex num_elements(ledger_->model()->num_elements());
  ElementIndex num_elements_covered(ledger_->num_elements_covered());
  while (num_elements_covered < num_elements && !pq_.IsEmpty()) {
    const SubsetIndex best_subset = pq_.TopSubset();
    DVLOG(1) << "Best subset: " << best_subset.value()
             << " Priority = " << pq_.Priority(best_subset)
             << " queue size = " << pq_.Size();
    const std::vector<SubsetIndex> impacted_subsets =
        ledger_->Toggle(best_subset, true);
    UpdatePriorities(impacted_subsets);
    num_elements_covered = ledger_->num_elements_covered();
    DVLOG(1) << "Cost = " << ledger_->cost() << " num_uncovered_elements = "
             << num_elements - num_elements_covered;
  }
  DCHECK(pq_.IsEmpty());
  DCHECK(ledger_->CheckConsistency());
  DCHECK(ledger_->CheckSolution());
  return true;
}

// SteepestSearch.

void SteepestSearch::UpdatePriorities(
    const std::vector<SubsetIndex>& impacted_subsets) {
  // Update priority queue. Since best_subset is in impacted_subsets, it will
  // be removed.
  for (const SubsetIndex subset : impacted_subsets) {
    pq_.Remove(subset);
  }
}

bool SteepestSearch::NextSolution(int num_iterations) {
  // Return false if ledger_ contains no solution.
  if (!ledger_->CheckSolution()) return false;
  const SparseColumnView& columns = ledger_->model()->columns();
  const SubsetCostVector& subset_costs = ledger_->model()->subset_costs();
  // Create priority queue with cost of using a subset, by decreasing order.
  // Do it only for removable subsets.
  for (SubsetIndex subset(0); subset < columns.size(); ++subset) {
    // The priority is the gain from removing the subset from the solution.
    if (ledger_->is_selected(subset) && ledger_->is_removable(subset)) {
      pq_.Add(subset, subset_costs[subset]);
    }
  }
  for (int iteration = 0; iteration < num_iterations && !pq_.IsEmpty();
       ++iteration) {
    const SubsetIndex best_subset = pq_.TopSubset();
    const Cost cost_decrease = subset_costs[best_subset];
    DCHECK_GT(cost_decrease, 0.0);
    DCHECK(ledger_->is_removable(best_subset));
    DCHECK(ledger_->is_selected(best_subset));
    const std::vector<SubsetIndex> impacted_subsets =
        ledger_->Toggle(best_subset, false);
    UpdatePriorities(impacted_subsets);
    DVLOG(1) << "Cost = " << ledger_->cost();
  }
  DCHECK(ledger_->CheckConsistency());
  DCHECK(ledger_->CheckSolution());
  return true;
}

// Guided Tabu Search

void GuidedTabuSearch::Initialize() {
  const SparseColumnView& columns = ledger_->model()->columns();
  const SubsetCostVector& subset_costs = ledger_->model()->subset_costs();
  times_penalized_.AssignToZero(columns.size());
  penalized_costs_ = subset_costs;
  gts_priorities_ = subset_costs;
  for (SubsetIndex subset(0); subset < gts_priorities_.size(); ++subset) {
    gts_priorities_[subset] /= columns[subset].size().value();
  }
}

namespace {
bool FlipCoin() {
  // TODO(user): use STL for repeatable testing.
  return absl::Bernoulli(absl::BitGen(), 0.5);
}
}  // namespace

void GuidedTabuSearch::UpdatePenalties(
    const std::vector<SubsetIndex>& impacted_subsets) {
  const SparseColumnView& columns = ledger_->model()->columns();
  const SubsetCostVector& subset_costs = ledger_->model()->subset_costs();
  const ElementIndex num_elements(ledger_->model()->num_elements());
  Cost largest_priority = -1.0;
  for (SubsetIndex subset(0); subset < columns.size(); ++subset) {
    if (ledger_->is_selected(subset)) {
      const ElementIndex num_elements_already_covered =
          num_elements - ledger_->marginal_impacts(subset);
      largest_priority = std::max(largest_priority, gts_priorities_[subset]) /
                         num_elements_already_covered.value();
    }
  }
  const double radius = radius_factor_ * largest_priority;
  for (SubsetIndex subset(0); subset < columns.size(); ++subset) {
    if (ledger_->is_selected(subset)) {
      const double subset_priority = gts_priorities_[subset];
      if ((largest_priority - subset_priority <= radius) && FlipCoin()) {
        ++times_penalized_[subset];
        const int times_penalized = times_penalized_[subset];
        const Cost cost = subset_costs[subset] / columns[subset].size().value();
        gts_priorities_[subset] = cost / (1 + times_penalized);
        penalized_costs_[subset] =
            cost * (1 + penalty_factor_ * times_penalized);
      }
    }
  }
}

bool GuidedTabuSearch::NextSolution(int num_iterations) {
  const SparseColumnView& columns = ledger_->model()->columns();
  const SubsetCostVector& subset_costs = ledger_->model()->subset_costs();
  constexpr Cost kMaxPossibleCost = std::numeric_limits<Cost>::max();
  Cost best_cost = ledger_->cost();
  Cost total_pen_cost = 0;
  for (const Cost pen_cost : penalized_costs_) {
    total_pen_cost += pen_cost;
  }
  SubsetBoolVector best_choices = ledger_->GetSolution();
  for (int iteration = 0; iteration < num_iterations; ++iteration) {
    Cost smallest_penalized_cost_increase = kMaxPossibleCost;
    SubsetIndex best_subset = kNotFound;
    for (SubsetIndex subset(0); subset < columns.size(); ++subset) {
      const Cost penalized_delta = penalized_costs_[subset];
      DVLOG(1) << "Subset: " << subset.value() << " at "
               << ledger_->is_selected(subset)
               << " is removable = " << ledger_->is_removable(subset)
               << " penalized_delta = " << penalized_delta
               << " smallest_penalized_cost_increase = "
               << smallest_penalized_cost_increase;
      if (!ledger_->is_selected(subset)) {
        // Try to use subset in solution, if its penalized delta is good.
        if (penalized_delta < smallest_penalized_cost_increase) {
          smallest_penalized_cost_increase = penalized_delta;
          best_subset = subset;
        }
      } else {
        // Try to remove subset from solution, if the gain from removing, is
        // OK:
        if (-penalized_delta < smallest_penalized_cost_increase &&
            // and it can be removed, and
            ledger_->is_removable(subset) &&
            // it is not Tabu OR decreases the actual cost:
            (!tabu_list_.Contains(subset) ||
             ledger_->cost() - subset_costs[subset] < best_cost)) {
          smallest_penalized_cost_increase = -penalized_delta;
          best_subset = subset;
        }
      }
    }
    if (best_subset == kNotFound) {  // Local minimum reached.
      ledger_->LoadSolution(best_choices);
      return true;
    }
    total_pen_cost += smallest_penalized_cost_increase;
    const std::vector<SubsetIndex> impacted_subsets =
        ledger_->Toggle(best_subset, !ledger_->is_selected(best_subset));
    UpdatePenalties(impacted_subsets);
    tabu_list_.Add(best_subset);
    if (ledger_->cost() < best_cost) {
      LOG(INFO) << "Iteration:" << iteration
                << ", current cost = " << ledger_->cost()
                << ", best cost = " << best_cost
                << ", penalized cost = " << total_pen_cost;
      best_cost = ledger_->cost();
      best_choices = ledger_->GetSolution();
    }
  }
  ledger_->LoadSolution(best_choices);
  DCHECK(ledger_->CheckConsistency());
  DCHECK(ledger_->CheckSolution());
  return true;
}

}  // namespace operations_research
