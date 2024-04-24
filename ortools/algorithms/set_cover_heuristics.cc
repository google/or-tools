// Copyright 2010-2024 Google LLC
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

#include "ortools/algorithms/set_cover_heuristics.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <numeric>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "ortools/algorithms/adjustable_k_ary_heap.h"
#include "ortools/algorithms/set_cover_invariant.h"
#include "ortools/algorithms/set_cover_model.h"
#include "ortools/base/logging.h"

namespace operations_research {

constexpr SubsetIndex kNotFound(-1);

// Preprocessor.

bool Preprocessor::NextSolution() {
  return NextSolution(inv_->model()->all_subsets());
}

bool Preprocessor::NextSolution(absl::Span<const SubsetIndex> focus) {
  DVLOG(1) << "Entering Preprocessor::NextSolution";
  const SubsetIndex num_subsets(inv_->model()->num_subsets());
  SubsetBoolVector choices(num_subsets, false);
  const ElementIndex num_elements(inv_->model()->num_elements());
  const SparseRowView& rows = inv_->model()->rows();
  for (ElementIndex element(0); element < num_elements; ++element) {
    if (rows[element].size() == 1) {
      const SubsetIndex subset = rows[element][EntryIndex(0)];
      choices[subset] = true;
      ++num_columns_fixed_by_singleton_row_;
    }
  }
  inv_->LoadSolution(choices);
  return true;
}

// TrivialSolutionGenerator.

bool TrivialSolutionGenerator::NextSolution() {
  return NextSolution(inv_->model()->all_subsets());
}

bool TrivialSolutionGenerator::NextSolution(
    absl::Span<const SubsetIndex> focus) {
  const SubsetIndex num_subsets(inv_->model()->num_subsets());
  SubsetBoolVector choices(num_subsets, false);
  for (const SubsetIndex subset : focus) {
    choices[subset] = true;
  }
  inv_->LoadSolution(choices);
  return true;
}

// RandomSolutionGenerator.

bool RandomSolutionGenerator::NextSolution() {
  return NextSolution(inv_->model()->all_subsets());
}

bool RandomSolutionGenerator::NextSolution(
    const std::vector<SubsetIndex>& focus) {
  std::vector<SubsetIndex> shuffled = focus;
  std::shuffle(shuffled.begin(), shuffled.end(), absl::BitGen());
  for (const SubsetIndex subset : shuffled) {
    if (inv_->is_selected()[subset]) continue;
    if (inv_->num_free_elements()[subset] != 0) {
      inv_->UnsafeUse(subset);
    }
  }
  DCHECK(inv_->CheckConsistency());
  return true;
}

namespace {
bool LogAndCheck(const SetCoverInvariant* inv,
                 const std::vector<SubsetIndex>& focus) {
  for (const SubsetIndex subset : focus) {
    DVLOG(1) << "subset: " << subset.value()
             << " is_selected: " << inv->is_selected()[subset]
             << " is_removable: " << inv->is_removable()[subset]
             << " cardinality:  " << inv->model()->columns()[subset].size()
             << " marginal_impact: " << inv->num_free_elements()[subset]
             << " num overcovered elements: "
             << inv->model()->columns()[subset].size().value() -
                    inv->num_coverage_le_1_elements()[subset].value();
  }
  DCHECK(inv->CheckConsistency());
  for (const SubsetIndex subset : focus) {
    DCHECK_EQ(inv->is_removable()[subset],
              inv->num_coverage_le_1_elements()[subset] == 0);
  }
  DCHECK_EQ(inv->num_uncovered_elements(), 0);
  return true;
}
}  // anonymous namespace

// GreedySolutionGenerator.

bool GreedySolutionGenerator::NextSolution() {
  return NextSolution(inv_->model()->all_subsets(),
                      inv_->model()->subset_costs());
}

bool GreedySolutionGenerator::NextSolution(
    const std::vector<SubsetIndex>& focus) {
  return NextSolution(focus, inv_->model()->subset_costs());
}

bool GreedySolutionGenerator::NextSolution(
    const std::vector<SubsetIndex>& focus, const SubsetCostVector& costs) {
  inv_->RecomputeInvariant();
  SubsetCostVector elements_per_cost(costs.size(), 0.0);
  for (const SubsetIndex subset : focus) {
    elements_per_cost[subset] = 1.0 / costs[subset];
  }
  std::vector<SubsetIndexWithPriority> subset_priorities;
  DVLOG(1) << "focus.size(): " << focus.size();
  subset_priorities.reserve(focus.size());
  for (const SubsetIndex subset : focus) {
    if (!inv_->is_selected()[subset] &&
        inv_->num_free_elements()[subset] != 0) {
      // NOMUTANTS -- reason, for C++
      const float priority =
          elements_per_cost[subset] * inv_->num_free_elements()[subset].value();
      subset_priorities.push_back({priority, subset.value()});
    }
  }
  // The priority queue maintains the maximum number of elements covered by unit
  // of cost. We chose 16 as the arity of the heap after some testing.
  // TODO(user): research more about the best value for Arity.
  AdjustableKAryHeap<SubsetIndexWithPriority, 16, true> pq(
      subset_priorities, inv_->model()->num_subsets().value());
  while (!pq.IsEmpty()) {
    const SubsetIndex best_subset(pq.Pop().index());
    const std::vector<SubsetIndex> impacted_subsets =
        inv_->UnsafeUse(best_subset);
    // NOMUTANTS -- reason, for C++
    if (inv_->num_uncovered_elements() == 0) break;
    for (const SubsetIndex subset : impacted_subsets) {
      const ElementIndex marginal_impact(inv_->num_free_elements()[subset]);
      if (marginal_impact > 0) {
        const float priority =
            marginal_impact.value() * elements_per_cost[subset];
        pq.Update({priority, subset.value()});
      } else {
        pq.Remove(subset.value());
      }
    }
    for (const SubsetIndex subset : focus) {
      DCHECK_EQ(inv_->is_removable()[subset],
                inv_->num_coverage_le_1_elements()[subset] == 0);
    }
    DVLOG(1) << "Cost = " << inv_->cost()
             << " num_uncovered_elements = " << inv_->num_uncovered_elements();
  }
  // Don't expect the queue to be empty, because of the break in the while loop.
  DCHECK(LogAndCheck(inv_, focus));
  return true;
}

class ElementIndexWithDegree {
 public:
  using Index = int;
  using Priority = int;
  ElementIndexWithDegree() = default;
  ElementIndexWithDegree(Priority priority, Index index)
      : index_(index), priority_(priority) {}
  Priority priority() const { return priority_; }
  Index index() const { return index_; }
  inline bool operator<(const ElementIndexWithDegree other) const {
    if (other.priority() != priority()) {
      return priority() < other.priority();
    }
    return index() < other.index();
  }

 private:
  Index index_;
  Priority priority_;
};

bool ElementDegreeSolutionGenerator::NextSolution() {
  return NextSolution(inv_->model()->all_subsets(),
                      inv_->model()->subset_costs());
}

bool ElementDegreeSolutionGenerator::NextSolution(
    const std::vector<SubsetIndex>& focus) {
  return NextSolution(focus, inv_->model()->subset_costs());
}

bool ElementDegreeSolutionGenerator::NextSolution(
    const std::vector<SubsetIndex>& focus, const SubsetCostVector& costs) {
  DVLOG(1) << "Entering ElementDegreeSolutionGenerator::NextSolution";
  inv_->RecomputeInvariant();
  const ElementIndex num_elements(inv_->model()->num_elements());
  std::vector<ElementIndexWithDegree> element_degrees;
  element_degrees.reserve(num_elements.value());
  const SparseRowView& rows = inv_->model()->rows();
  for (ElementIndex element(0); element < num_elements; ++element) {
    if (inv_->coverage()[element] == 0) {
      element_degrees.push_back(
          {rows[element].size().value(), element.value()});
    }
  }

  // The priority queue maintains the non-covered element with the smallest
  // degree.
  AdjustableKAryHeap<ElementIndexWithDegree, 16, false> pq(
      element_degrees, num_elements.value());
  const SparseColumnView& columns = inv_->model()->columns();
  while (!pq.IsEmpty()) {
    const ElementIndex best_element(pq.Pop().index());
    DCHECK_EQ(inv_->coverage()[best_element], 0)
        << "Element " << best_element.value()
        << " is already covered. num_uncovered_elements: "
        << inv_->num_uncovered_elements()
        << " / num_elements: " << num_elements;
    Cost min_ratio = std::numeric_limits<Cost>::max();
    SubsetIndex best_subset(-1);
    for (const SubsetIndex subset : rows[best_element]) {
      const Cost ratio =
          costs[subset] / inv_->num_free_elements()[subset].value();
      if (ratio < min_ratio) {
        min_ratio = ratio;
        best_subset = subset;
      }
    }
    DCHECK_NE(best_subset, -1);
    inv_->UnsafeUse(best_subset);
    if (inv_->num_uncovered_elements() == 0) break;
    for (const ElementIndex element : columns[best_subset]) {
      if (element != best_element) {
        pq.Remove(element.value());
      }
    }
    DVLOG(1) << "Cost = " << inv_->cost()
             << " num_uncovered_elements = " << inv_->num_uncovered_elements();
  }
  // Don't expect the queue to be empty, because of the break in the while loop.
  DCHECK(LogAndCheck(inv_, focus));
  return true;
}

// SteepestSearch.

void SteepestSearch::UpdatePriorities(absl::Span<const SubsetIndex>) {}

bool SteepestSearch::NextSolution(int num_iterations) {
  return NextSolution(inv_->model()->all_subsets(),
                      inv_->model()->subset_costs(), num_iterations);
}

bool SteepestSearch::NextSolution(absl::Span<const SubsetIndex> focus,
                                  int num_iterations) {
  return NextSolution(focus, inv_->model()->subset_costs(), num_iterations);
}

bool SteepestSearch::NextSolution(absl::Span<const SubsetIndex> focus,
                                  const SubsetCostVector& costs,
                                  int num_iterations) {
  DVLOG(1) << "Entering SteepestSearch::NextSolution, num_iterations = "
           << num_iterations;
  // Return false if inv_ contains no solution.
  if (inv_->num_uncovered_elements() != 0) {
    return false;
  }

  // Create priority queue with cost of using a subset, by decreasing order.
  // Do it only for selected AND removable subsets.
  std::vector<SubsetIndexWithPriority> subset_priorities;
  subset_priorities.reserve(focus.size());
  for (const SubsetIndex subset : focus) {
    if (inv_->is_selected()[subset] && inv_->is_removable()[subset]) {
      const float delta_per_element = costs[subset];
      subset_priorities.push_back({delta_per_element, subset.value()});
    }
  }
  DVLOG(1) << "subset_priorities.size(): " << subset_priorities.size();
  AdjustableKAryHeap<SubsetIndexWithPriority, 16, true> pq(
      subset_priorities, inv_->model()->num_subsets().value());
  for (int iteration = 0; iteration < num_iterations && !pq.IsEmpty();
       ++iteration) {
    const SubsetIndex best_subset(pq.Pop().index());
    if (!inv_->is_removable()[best_subset]) continue;
    DCHECK_GT(costs[best_subset], 0.0);
    const std::vector<SubsetIndex> impacted_subsets =
        inv_->UnsafeRemove(best_subset);
    for (const SubsetIndex subset : impacted_subsets) {
      if (!inv_->is_removable()[subset]) {
        pq.Remove(subset.value());
      }
    }
    DVLOG(1) << "Cost = " << inv_->cost();
  }
  DCHECK_EQ(inv_->num_uncovered_elements(), 0);
  DCHECK(inv_->CheckConsistency());
  return true;
}

// Guided Tabu Search

void GuidedTabuSearch::Initialize() {
  const SparseColumnView& columns = inv_->model()->columns();
  const SubsetCostVector& subset_costs = inv_->model()->subset_costs();
  times_penalized_.AssignToZero(columns.size());
  augmented_costs_ = subset_costs;
  utilities_ = subset_costs;
}

namespace {
bool FlipCoin() {
  // TODO(user): use STL for repeatable testing.
  return absl::Bernoulli(absl::BitGen(), 0.5);
}
}  // namespace

void GuidedTabuSearch::UpdatePenalties(absl::Span<const SubsetIndex> focus) {
  const SubsetCostVector& subset_costs = inv_->model()->subset_costs();
  Cost max_utility = -1.0;
  for (const SubsetIndex subset : focus) {
    if (inv_->is_selected()[subset]) {
      max_utility = std::max(max_utility, utilities_[subset]);
    }
  }
  const double epsilon_utility = epsilon_ * max_utility;
  for (const SubsetIndex subset : focus) {
    if (inv_->is_selected()[subset]) {
      const double utility = utilities_[subset];
      if ((max_utility - utility <= epsilon_utility) && FlipCoin()) {
        ++times_penalized_[subset];
        const int times_penalized = times_penalized_[subset];
        const Cost cost =
            subset_costs[subset];  // / columns[subset].size().value();
        utilities_[subset] = cost / (1 + times_penalized);
        augmented_costs_[subset] =
            cost * (1 + penalty_factor_ * times_penalized);
      }
    }
  }
}

bool GuidedTabuSearch::NextSolution(int num_iterations) {
  return NextSolution(inv_->model()->all_subsets(), num_iterations);
}

bool GuidedTabuSearch::NextSolution(const std::vector<SubsetIndex>& focus,
                                    int num_iterations) {
  DVLOG(1) << "Entering GuidedTabuSearch::NextSolution, num_iterations = "
           << num_iterations;
  const SubsetCostVector& subset_costs = inv_->model()->subset_costs();
  constexpr Cost kMaxPossibleCost = std::numeric_limits<Cost>::max();
  Cost best_cost = inv_->cost();
  SubsetBoolVector best_choices = inv_->is_selected();
  Cost augmented_cost =
      std::accumulate(augmented_costs_.begin(), augmented_costs_.end(), 0.0);
  for (int iteration = 0; iteration < num_iterations; ++iteration) {
    Cost best_delta = kMaxPossibleCost;
    SubsetIndex best_subset = kNotFound;
    for (const SubsetIndex subset : focus) {
      const Cost delta = augmented_costs_[subset];
      DVLOG(1) << "Subset, " << subset.value() << ", at ,"
               << inv_->is_selected()[subset] << ", is removable =, "
               << inv_->is_removable()[subset] << ", delta =, " << delta
               << ", best_delta =, " << best_delta;
      if (inv_->is_selected()[subset]) {
        // Try to remove subset from solution, if the gain from removing is
        // worth it:
        if (-delta < best_delta &&
            // and it can be removed, and
            inv_->is_removable()[subset] &&
            // it is not Tabu OR decreases the actual cost (aspiration):
            (!tabu_list_.Contains(subset) ||
             inv_->cost() - subset_costs[subset] < best_cost)) {
          best_delta = -delta;
          best_subset = subset;
        }
      } else {
        // Try to use subset in solution, if its penalized delta is good.
        if (delta < best_delta) {
          // The limit kMaxPossibleCost is ill-defined,
          // there is always a best_subset. Is it intended?
          if (!tabu_list_.Contains(subset)) {
            best_delta = delta;
            best_subset = subset;
          }
        }
      }
    }
    if (best_subset == kNotFound) {  // Local minimum reached.
      inv_->LoadSolution(best_choices);
      return true;
    }
    DVLOG(1) << "Best subset, " << best_subset.value() << ", at ,"
             << inv_->is_selected()[best_subset] << ", is removable = ,"
             << inv_->is_removable()[best_subset] << ", best_delta = ,"
             << best_delta;

    UpdatePenalties(focus);
    tabu_list_.Add(best_subset);
    const std::vector<SubsetIndex> impacted_subsets =
        inv_->UnsafeToggle(best_subset, !inv_->is_selected()[best_subset]);
    // TODO(user): make the cost computation incremental.
    augmented_cost =
        std::accumulate(augmented_costs_.begin(), augmented_costs_.end(), 0.0);

    DVLOG(1) << "Iteration, " << iteration << ", current cost = ,"
             << inv_->cost() << ", best cost = ," << best_cost
             << ", penalized cost = ," << augmented_cost;
    if (inv_->cost() < best_cost) {
      LOG(INFO) << "Updated best cost, " << "Iteration, " << iteration
                << ", current cost = ," << inv_->cost() << ", best cost = ,"
                << best_cost << ", penalized cost = ," << augmented_cost;
      best_cost = inv_->cost();
      best_choices = inv_->is_selected();
    }
  }
  inv_->LoadSolution(best_choices);
  DCHECK_EQ(inv_->num_uncovered_elements(), 0);
  DCHECK(inv_->CheckConsistency());
  return true;
}

namespace {
void SampleSubsets(std::vector<SubsetIndex>* list, std::size_t num_subsets) {
  num_subsets = std::min(num_subsets, list->size());
  CHECK_GE(num_subsets, 0);
  std::shuffle(list->begin(), list->end(), absl::BitGen());
  list->resize(num_subsets);
}
}  // namespace

std::vector<SubsetIndex> ClearRandomSubsets(std::size_t num_subsets,
                                            SetCoverInvariant* inv) {
  return ClearRandomSubsets(inv->model()->all_subsets(), num_subsets, inv);
}

std::vector<SubsetIndex> ClearRandomSubsets(absl::Span<const SubsetIndex> focus,
                                            std::size_t num_subsets,
                                            SetCoverInvariant* inv) {
  num_subsets = std::min(num_subsets, focus.size());
  CHECK_GE(num_subsets, 0);
  std::vector<SubsetIndex> chosen_indices;
  for (const SubsetIndex subset : focus) {
    if (inv->is_selected()[subset]) {
      chosen_indices.push_back(subset);
    }
  }
  SampleSubsets(&chosen_indices, num_subsets);
  for (const SubsetIndex subset : chosen_indices) {
    // We can use UnsafeRemove because we allow non-solutions.
    inv->UnsafeRemove(subset);
  }
  return chosen_indices;
}

std::vector<SubsetIndex> ClearMostCoveredElements(std::size_t max_num_subsets,
                                                  SetCoverInvariant* inv) {
  return ClearMostCoveredElements(inv->model()->all_subsets(), max_num_subsets,
                                  inv);
}

std::vector<SubsetIndex> ClearMostCoveredElements(
    absl::Span<const SubsetIndex> focus, std::size_t max_num_subsets,
    SetCoverInvariant* inv) {
  // This is the vector we will return.
  std::vector<SubsetIndex> sampled_subsets;

  const ElementToSubsetVector& coverage = inv->coverage();
  const ElementIndex num_elements = inv->model()->num_elements();
  const SubsetIndex num_subsets = inv->model()->num_subsets();
  const SparseRowView& rows = inv->model()->rows();

  // Collect the sets which have at least one element whose coverage > 1,
  // even if those sets are not removable.
  SubsetBoolVector subset_is_collected(num_subsets, false);
  for (ElementIndex element(0); element < num_elements; ++element) {
    if (coverage[element] <= 1) continue;
    for (const SubsetIndex subset : rows[element]) {
      if (inv->is_selected()[subset] && !subset_is_collected[subset]) {
        subset_is_collected[subset] = true;
      }
    }
  }

  // Now interset with focus: sampled_subsets = focus ⋂ impacted_subsets.
  // NOTE(user): this might take too long. TODO(user):find another algorithm if
  // necessary.
  for (const SubsetIndex subset : focus) {
    if (subset_is_collected[subset]) {
      sampled_subsets.push_back(subset);
    }
  }

  // Actually *sample* sampled_subset.
  // TODO(user): find another algorithm if necessary.
  std::shuffle(sampled_subsets.begin(), sampled_subsets.end(), absl::BitGen());
  sampled_subsets.resize(std::min(sampled_subsets.size(), max_num_subsets));

  // Testing has shown that sorting sampled_subsets is not necessary.
  // Now, un-select the subset in sampled_subsets.
  for (const SubsetIndex subset : sampled_subsets) {
    // Use UnsafeRemove because we allow non-solutions.
    inv->UnsafeRemove(subset);
  }
  return sampled_subsets;
}

}  // namespace operations_research
