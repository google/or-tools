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
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "ortools/algorithms/adjustable_k_ary_heap.h"
#include "ortools/algorithms/set_cover_invariant.h"
#include "ortools/algorithms/set_cover_model.h"
#include "ortools/base/logging.h"

namespace operations_research {

constexpr SubsetIndex kNotFound(-1);
static constexpr Cost kMaxPossibleCost = std::numeric_limits<Cost>::max();
static constexpr double kInfinity = std::numeric_limits<float>::infinity();

namespace {
SubsetBoolVector MakeBoolVector(absl::Span<const SubsetIndex> focus,
                                SubsetIndex size) {
  SubsetBoolVector result(SubsetIndex(size), false);
  for (const SubsetIndex subset : focus) {
    result[subset] = true;
  }
  return result;
}
}  // anonymous namespace

using CL = SetCoverInvariant::ConsistencyLevel;

// Preprocessor.

bool Preprocessor::NextSolution() {
  return NextSolution(inv_->model()->all_subsets());
}

bool Preprocessor::NextSolution(absl::Span<const SubsetIndex> focus) {
  DVLOG(1) << "Entering Preprocessor::NextSolution";
  const SubsetIndex num_subsets(inv_->model()->num_subsets());
  const ElementIndex num_elements(inv_->model()->num_elements());
  const SparseRowView& rows = inv_->model()->rows();
  SubsetBoolVector in_focus = MakeBoolVector(focus, num_subsets);
  for (const ElementIndex element : inv_->model()->ElementRange()) {
    if (rows[element].size() == 1) {
      const SubsetIndex subset = rows[element][RowEntryIndex(0)];
      if (in_focus[subset] && !inv_->is_selected()[subset]) {
        inv_->Select(subset, CL::kCostAndCoverage);
        ++num_columns_fixed_by_singleton_row_;
      }
    }
  }
  inv_->CompressTrace();
  inv_->Recompute(CL::kCostAndCoverage);
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
  inv_->Recompute(CL::kCostAndCoverage);
  return true;
}

// RandomSolutionGenerator.

bool RandomSolutionGenerator::NextSolution() {
  return NextSolution(inv_->model()->all_subsets());
}

bool RandomSolutionGenerator::NextSolution(
    const std::vector<SubsetIndex>& focus) {
  inv_->ClearTrace();
  std::vector<SubsetIndex> shuffled = focus;
  std::shuffle(shuffled.begin(), shuffled.end(), absl::BitGen());
  for (const SubsetIndex subset : shuffled) {
    if (inv_->is_selected()[subset]) continue;
    if (inv_->num_free_elements()[subset] != 0) {
      inv_->Select(subset, CL::kFreeAndUncovered);
    }
  }
  inv_->CompressTrace();
  DCHECK(inv_->CheckConsistency(CL::kFreeAndUncovered));
  return true;
}

// GreedySolutionGenerator.

bool GreedySolutionGenerator::NextSolution() {
  return NextSolution(inv_->model()->all_subsets(),
                      inv_->model()->subset_costs());
}

bool GreedySolutionGenerator::NextSolution(
    const std::vector<SubsetIndex>& focus) {
  return NextSolution(focus, inv_->model()->subset_costs());
}

bool GreedySolutionGenerator::NextSolution(absl::Span<const SubsetIndex> focus,
                                           const SubsetCostVector& costs) {
  DCHECK(inv_->CheckConsistency(CL::kCostAndCoverage));
  inv_->Recompute(CL::kFreeAndUncovered);
  inv_->ClearTrace();
  SubsetCostVector elements_per_cost(costs.size(), 0);
  for (const SubsetIndex subset : focus) {
    elements_per_cost[subset] = 1.0 / costs[subset];
  }
  std::vector<std::pair<float, SubsetIndex::ValueType>> subset_priorities;
  DVLOG(1) << "focus.size(): " << focus.size();
  subset_priorities.reserve(focus.size());
  for (const SubsetIndex subset : focus) {
    if (!inv_->is_selected()[subset] &&
        inv_->num_free_elements()[subset] != 0) {
      // NOMUTANTS -- reason, for C++
      const float priority =
          elements_per_cost[subset] * inv_->num_free_elements()[subset];
      subset_priorities.push_back({priority, subset.value()});
    }
  }
  // The priority queue maintains the maximum number of elements covered by unit
  // of cost. We chose 16 as the arity of the heap after some testing.
  // TODO(user): research more about the best value for Arity.
  AdjustableKAryHeap<float, SubsetIndex::ValueType, 16, true> pq(
      subset_priorities, inv_->model()->num_subsets());
  while (!pq.IsEmpty()) {
    const SubsetIndex best_subset(pq.TopIndex());
    pq.Pop();
    inv_->Select(best_subset, CL::kFreeAndUncovered);
    // NOMUTANTS -- reason, for C++
    if (inv_->num_uncovered_elements() == 0) break;
    for (IntersectingSubsetsIterator it(*inv_->model(), best_subset);
         !it.at_end(); ++it) {
      const SubsetIndex subset = *it;
      const BaseInt marginal_impact(inv_->num_free_elements()[subset]);
      if (marginal_impact > 0) {
        const float priority = marginal_impact * elements_per_cost[subset];
        pq.Update({priority, subset.value()});
      } else {
        pq.Remove(subset.value());
      }
    }
    DVLOG(1) << "Cost = " << inv_->cost()
             << " num_uncovered_elements = " << inv_->num_uncovered_elements();
  }
  inv_->CompressTrace();
  // Don't expect the queue to be empty, because of the break in the while
  // loop.
  DCHECK(inv_->CheckConsistency(CL::kFreeAndUncovered));
  return true;
}

namespace {
// This class gathers statistics about the usefulness of the ratio computation.
class ComputationUsefulnessStats {
 public:
  // If is_active is true, the stats are gathered, otherwise there is no
  // overhead, in particular no memory allocation.
  explicit ComputationUsefulnessStats(const SetCoverInvariant* inv,
                                      bool is_active)
      : inv_(inv),
        is_active_(is_active),
        num_ratio_computations_(),
        num_useless_computations_(),
        num_free_elements_() {
    if (is_active) {
      BaseInt num_subsets = inv_->model()->num_subsets();
      num_ratio_computations_.assign(num_subsets, 0);
      num_useless_computations_.assign(num_subsets, 0);
      num_free_elements_.assign(num_subsets, -1);  // -1 means not computed yet.
    }
  }

  // To be called each time a num_free_elements is computed.
  void Update(SubsetIndex subset, BaseInt new_num_free_elements) {
    if (is_active_) {
      if (new_num_free_elements == num_free_elements_[subset]) {
        ++num_useless_computations_[subset];
      }
      ++num_ratio_computations_[subset];
      num_free_elements_[subset] = new_num_free_elements;
    }
  }

  // To be called at the end of the algorithm.
  void PrintStats() {
    if (is_active_) {
      BaseInt num_subsets_considered = 0;
      BaseInt num_ratio_updates = 0;
      BaseInt num_wasted_ratio_updates = 0;
      for (const SubsetIndex subset : inv_->model()->SubsetRange()) {
        if (num_ratio_computations_[subset] > 0) {
          ++num_subsets_considered;
          if (num_ratio_computations_[subset] > 1) {
            num_ratio_updates += num_ratio_computations_[subset] - 1;
          }
        }
        num_wasted_ratio_updates += num_useless_computations_[subset];
      }
      LOG(INFO) << "num_subsets_considered = " << num_subsets_considered;
      LOG(INFO) << "num_ratio_updates = " << num_ratio_updates;
      LOG(INFO) << "num_wasted_ratio_updates = " << num_wasted_ratio_updates;
    }
  }

 private:
  // The invariant on which the stats are performed.
  const SetCoverInvariant* inv_;

  // Whether the stats are active or not.
  bool is_active_;

  // Number of times the ratio was computed for a subset.
  SubsetToIntVector num_ratio_computations_;

  // Number of times the ratio was computed for a subset and was the same as the
  // previous one.
  SubsetToIntVector num_useless_computations_;

  // The value num_free_elements_ for the subset the last time it was computed.
  // Used to detect useless computations.
  SubsetToIntVector num_free_elements_;
};
}  // namespace

// ElementDegreeSolutionGenerator.
// There is no need to use a priority queue here, as the ratios are computed
// on-demand. Also elements are sorted based on degree once and for all and
// moved past when the elements become already covered.
bool ElementDegreeSolutionGenerator::NextSolution() {
  const SubsetIndex num_subsets(inv_->model()->num_subsets());
  const SubsetBoolVector in_focus(num_subsets, true);
  return NextSolution(in_focus, inv_->model()->subset_costs());
}

bool ElementDegreeSolutionGenerator::NextSolution(
    absl::Span<const SubsetIndex> focus) {
  const SubsetIndex num_subsets(inv_->model()->num_subsets());
  const SubsetBoolVector in_focus = MakeBoolVector(focus, num_subsets);
  return NextSolution(in_focus, inv_->model()->subset_costs());
}

bool ElementDegreeSolutionGenerator::NextSolution(
    absl::Span<const SubsetIndex> focus, const SubsetCostVector& costs) {
  const SubsetIndex num_subsets(inv_->model()->num_subsets());
  const SubsetBoolVector in_focus = MakeBoolVector(focus, num_subsets);
  return NextSolution(in_focus, costs);
}

bool ElementDegreeSolutionGenerator::NextSolution(
    const SubsetBoolVector& in_focus, const SubsetCostVector& costs) {
  DVLOG(1) << "Entering ElementDegreeSolutionGenerator::NextSolution";
  inv_->Recompute(CL::kFreeAndUncovered);
  // Create the list of all the indices in the problem.
  const BaseInt num_elements = inv_->model()->num_elements();
  std::vector<ElementIndex> degree_sorted_elements(num_elements);
  std::iota(degree_sorted_elements.begin(), degree_sorted_elements.end(),
            ElementIndex(0));
  const SparseRowView& rows = inv_->model()->rows();
  // Sort indices by degree i.e. the size of the row corresponding to an
  // element.
  std::sort(degree_sorted_elements.begin(), degree_sorted_elements.end(),
            [&rows](const ElementIndex a, const ElementIndex b) {
              if (rows[a].size() < rows[b].size()) return true;
              if (rows[a].size() == rows[b].size()) return a < b;
              return false;
            });
  ComputationUsefulnessStats stats(inv_, false);
  for (const ElementIndex element : degree_sorted_elements) {
    // No need to cover an element that is already covered.
    if (inv_->coverage()[element] != 0) continue;
    Cost min_ratio = std::numeric_limits<Cost>::max();
    SubsetIndex best_subset(-1);
    for (const SubsetIndex subset : rows[element]) {
      if (!in_focus[subset]) continue;
      const BaseInt num_free_elements = inv_->ComputeNumFreeElements(subset);
      const Cost ratio = costs[subset] / num_free_elements;
      stats.Update(subset, num_free_elements);
      if (ratio < min_ratio) {
        min_ratio = ratio;
        best_subset = subset;
      } else if (ratio == min_ratio) {
        // If the ratio is the same, we choose the subset with the most free
        // elements.
        // TODO(user): What about adding a tolerance for equality, which could
        // further favor larger columns?
        if (inv_->num_free_elements()[subset] >
            inv_->num_free_elements()[best_subset]) {
          best_subset = subset;
        }
      }
    }
    if (best_subset.value() == -1) {
      LOG(WARNING) << "Best subset not found. Algorithmic error or invalid "
                      "input.";
      continue;
    }
    DCHECK_NE(best_subset.value(), -1);
    inv_->Select(best_subset, CL::kFreeAndUncovered);
    DVLOG(1) << "Cost = " << inv_->cost()
             << " num_uncovered_elements = " << inv_->num_uncovered_elements();
  }
  inv_->CompressTrace();
  stats.PrintStats();
  DCHECK(inv_->CheckConsistency(CL::kFreeAndUncovered));
  return true;
}

// LazyElementDegreeSolutionGenerator.
// There is no need to use a priority queue here, as the ratios are computed
// on-demand. Also elements are sorted based on degree once and for all and
// moved past when the elements become already covered.
bool LazyElementDegreeSolutionGenerator::NextSolution() {
  const SubsetIndex num_subsets(inv_->model()->num_subsets());
  const SubsetBoolVector in_focus(num_subsets, true);
  return NextSolution(in_focus, inv_->model()->subset_costs());
}

bool LazyElementDegreeSolutionGenerator::NextSolution(
    absl::Span<const SubsetIndex> focus) {
  const SubsetIndex num_subsets(inv_->model()->num_subsets());
  const SubsetBoolVector in_focus = MakeBoolVector(focus, num_subsets);
  return NextSolution(in_focus, inv_->model()->subset_costs());
}

bool LazyElementDegreeSolutionGenerator::NextSolution(
    absl::Span<const SubsetIndex> focus, const SubsetCostVector& costs) {
  const SubsetIndex num_subsets(inv_->model()->num_subsets());
  const SubsetBoolVector in_focus = MakeBoolVector(focus, num_subsets);
  return NextSolution(in_focus, costs);
}

bool LazyElementDegreeSolutionGenerator::NextSolution(
    const SubsetBoolVector& in_focus, const SubsetCostVector& costs) {
  DVLOG(1) << "Entering LazyElementDegreeSolutionGenerator::NextSolution";
  DCHECK(inv_->CheckConsistency(
      SetCoverInvariant::ConsistencyLevel::kCostAndCoverage));
  // Create the list of all the indices in the problem.
  const BaseInt num_elements = inv_->model()->num_elements();
  std::vector<ElementIndex> degree_sorted_elements(num_elements);
  std::iota(degree_sorted_elements.begin(), degree_sorted_elements.end(),
            ElementIndex(0));
  const SparseRowView& rows = inv_->model()->rows();
  // Sort indices by degree i.e. the size of the row corresponding to an
  // element.
  std::sort(degree_sorted_elements.begin(), degree_sorted_elements.end(),
            [&rows](const ElementIndex a, const ElementIndex b) {
              if (rows[a].size() < rows[b].size()) return true;
              if (rows[a].size() == rows[b].size()) return a < b;
              return false;
            });
  ComputationUsefulnessStats stats(inv_, false);
  for (const ElementIndex element : degree_sorted_elements) {
    // No need to cover an element that is already covered.
    if (inv_->coverage()[element] != 0) continue;
    Cost min_ratio = std::numeric_limits<Cost>::max();
    SubsetIndex best_subset(-1);
    for (const SubsetIndex subset : rows[element]) {
      if (!in_focus[subset]) continue;
      const BaseInt num_free_elements = inv_->ComputeNumFreeElements(subset);
      const Cost ratio = costs[subset] / num_free_elements;
      stats.Update(subset, num_free_elements);
      if (ratio < min_ratio) {
        min_ratio = ratio;
        best_subset = subset;
      } else if (ratio == min_ratio) {
        // If the ratio is the same, we choose the subset with the most free
        // elements.
        // TODO(user): What about adding a tolerance for equality, which could
        // further favor larger columns?
        if (num_free_elements > inv_->num_free_elements()[best_subset]) {
          best_subset = subset;
        }
      }
    }
    DCHECK_NE(best_subset, SubsetIndex(-1));
    inv_->Select(best_subset, CL::kCostAndCoverage);
    DVLOG(1) << "Cost = " << inv_->cost()
             << " num_uncovered_elements = " << inv_->num_uncovered_elements();
  }
  inv_->CompressTrace();
  DCHECK(inv_->CheckConsistency(CL::kCostAndCoverage));
  stats.PrintStats();
  return true;
}

// SteepestSearch.

void SteepestSearch::UpdatePriorities(absl::Span<const SubsetIndex>) {}

bool SteepestSearch::NextSolution(int num_iterations) {
  const SubsetIndex num_subsets(inv_->model()->num_subsets());
  const SubsetBoolVector in_focus(num_subsets, true);
  return NextSolution(in_focus, inv_->model()->subset_costs(), num_iterations);
}

bool SteepestSearch::NextSolution(absl::Span<const SubsetIndex> focus,
                                  int num_iterations) {
  const SubsetIndex num_subsets(inv_->model()->num_subsets());
  const SubsetBoolVector in_focus = MakeBoolVector(focus, num_subsets);
  return NextSolution(focus, inv_->model()->subset_costs(), num_iterations);
}

bool SteepestSearch::NextSolution(absl::Span<const SubsetIndex> focus,
                                  const SubsetCostVector& costs,
                                  int num_iterations) {
  const SubsetIndex num_subsets(inv_->model()->num_subsets());
  const SubsetBoolVector in_focus = MakeBoolVector(focus, num_subsets);
  return NextSolution(in_focus, costs, num_iterations);
}

bool SteepestSearch::NextSolution(const SubsetBoolVector& in_focus,
                                  const SubsetCostVector& costs,
                                  int num_iterations) {
  DCHECK(inv_->CheckConsistency(CL::kCostAndCoverage));
  inv_->Recompute(CL::kFreeAndUncovered);
  DVLOG(1) << "Entering SteepestSearch::NextSolution, num_iterations = "
           << num_iterations;
  // Return false if inv_ contains no solution.
  // TODO(user): This should be relaxed for partial solutions.
  if (inv_->num_uncovered_elements() != 0) {
    return false;
  }

  // Create priority queue with cost of using a subset, by decreasing order.
  // Do it only for selected AND removable subsets.
  std::vector<std::pair<float, SubsetIndex::ValueType>> subset_priorities;
  subset_priorities.reserve(in_focus.size());
  for (const SetCoverDecision& decision : inv_->trace()) {
    const SubsetIndex subset = decision.subset();
    if (in_focus[subset] && inv_->is_selected()[subset] &&
        inv_->ComputeIsRedundant(subset)) {
      const float delta_per_element = costs[subset];
      subset_priorities.push_back({delta_per_element, subset.value()});
    }
  }
  DVLOG(1) << "subset_priorities.size(): " << subset_priorities.size();
  AdjustableKAryHeap<float, SubsetIndex::ValueType, 16, true> pq(
      subset_priorities, inv_->model()->num_subsets());
  for (int iteration = 0; iteration < num_iterations && !pq.IsEmpty();
       ++iteration) {
    const SubsetIndex best_subset(pq.TopIndex());
    pq.Pop();
    DCHECK(inv_->is_selected()[best_subset]);
    DCHECK(inv_->ComputeIsRedundant(best_subset));
    DCHECK_GT(costs[best_subset], 0.0);
    inv_->Deselect(best_subset, CL::kFreeAndUncovered);

    for (IntersectingSubsetsIterator it(*inv_->model(), best_subset);
         !it.at_end(); ++it) {
      const SubsetIndex subset = *it;
      if (!inv_->ComputeIsRedundant(subset)) {
        pq.Remove(subset.value());
      }
    }
    DVLOG(1) << "Cost = " << inv_->cost();
  }
  inv_->CompressTrace();
  // TODO(user): change this to enable working on partial solutions.
  DCHECK_EQ(inv_->num_uncovered_elements(), 0);
  DCHECK(inv_->CheckConsistency(CL::kFreeAndUncovered));
  return true;
}

// Guided Tabu Search

void GuidedTabuSearch::Initialize() {
  const SubsetIndex num_subsets(inv_->model()->num_subsets());
  const SubsetCostVector& subset_costs = inv_->model()->subset_costs();
  times_penalized_.assign(num_subsets.value(), 0);
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

bool GuidedTabuSearch::NextSolution(absl::Span<const SubsetIndex> focus,
                                    int num_iterations) {
  DCHECK(inv_->CheckConsistency(CL::kFreeAndUncovered));
  DVLOG(1) << "Entering GuidedTabuSearch::NextSolution, num_iterations = "
           << num_iterations;
  const SubsetCostVector& subset_costs = inv_->model()->subset_costs();
  Cost best_cost = inv_->cost();
  SubsetBoolVector best_choices = inv_->is_selected();
  Cost augmented_cost =
      std::accumulate(augmented_costs_.begin(), augmented_costs_.end(), 0.0);
  BaseInt trace_size = inv_->trace().size();
  for (int iteration = 0; iteration < num_iterations; ++iteration) {
    if (inv_->trace().size() > 2 * trace_size) {
      inv_->CompressTrace();
      trace_size = inv_->trace().size();
    }
    Cost best_delta = kMaxPossibleCost;
    SubsetIndex best_subset = kNotFound;
    for (const SubsetIndex subset : focus) {
      const Cost delta = augmented_costs_[subset];
      DVLOG(1) << "Subset, " << subset.value() << ", at ,"
               << inv_->is_selected()[subset] << ", delta =, " << delta
               << ", best_delta =, " << best_delta;
      if (inv_->is_selected()[subset]) {
        // Try to remove subset from solution, if the gain from removing is
        // worth it:
        if (-delta < best_delta &&
            // and it can be removed, and
            inv_->ComputeIsRedundant(subset) &&
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
             << inv_->is_selected()[best_subset] << ", best_delta = ,"
             << best_delta;

    UpdatePenalties(focus);
    tabu_list_.Add(best_subset);
    inv_->Flip(best_subset, CL::kFreeAndUncovered);
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
  inv_->CompressTrace();
  DCHECK(inv_->CheckConsistency(CL::kFreeAndUncovered));
  return true;
}

// Guided Local Search
void GuidedLocalSearch::Initialize() {
  const SparseColumnView& columns = inv_->model()->columns();
  penalties_.assign(columns.size(), 0);
  penalization_factor_ = alpha_ * inv_->cost() * 1.0 / (columns.size());
  for (const SetCoverDecision& decision : inv_->trace()) {
    const SubsetIndex subset = decision.subset();
    if (inv_->is_selected()[subset]) {
      utility_heap_.Insert(
          {static_cast<float>(inv_->model()->subset_costs()[subset] /
                              (1 + penalties_[subset])),
           subset.value()});
    }
  }
}

bool GuidedLocalSearch::NextSolution(int num_iterations) {
  return NextSolution(inv_->model()->all_subsets(), num_iterations);
}

Cost GuidedLocalSearch::ComputeDelta(SubsetIndex subset) const {
  float delta = (penalization_factor_ * penalties_[subset] +
                 inv_->model()->subset_costs()[subset]);
  if (inv_->is_selected()[subset] && inv_->ComputeIsRedundant(subset)) {
    return delta;
  } else if (!inv_->is_selected()[subset]) {
    return -delta;
  }
  return kInfinity;
}

bool GuidedLocalSearch::NextSolution(absl::Span<const SubsetIndex> focus,
                                     int num_iterations) {
  inv_->Recompute(CL::kRedundancy);
  Cost best_cost = inv_->cost();
  SubsetBoolVector best_choices = inv_->is_selected();

  for (const SubsetIndex& subset : focus) {
    const float delta = ComputeDelta(subset);
    if (delta < kInfinity) {
      priority_heap_.Insert({delta, subset.value()});
    }
  }

  for (int iteration = 0;
       !priority_heap_.IsEmpty() && iteration < num_iterations; ++iteration) {
    // Improve current solution respective to the current penalties.
    const SubsetIndex best_subset(priority_heap_.TopIndex());
    if (inv_->is_selected()[best_subset]) {
      utility_heap_.Insert({0, best_subset.value()});
    } else {
      utility_heap_.Insert(
          {static_cast<float>(inv_->model()->subset_costs()[best_subset] /
                              (1 + penalties_[best_subset])),
           best_subset.value()});
    }
    inv_->Flip(best_subset, CL::kRedundancy);  // Flip the best subset.
    DCHECK(!utility_heap_.IsEmpty());

    // Getting the subset with highest utility. utility_heap_ is not empty,
    // because we just inserted a pair.
    const SubsetIndex penalized_subset(utility_heap_.TopIndex());
    utility_heap_.Pop();
    ++penalties_[penalized_subset];
    utility_heap_.Insert(
        {static_cast<float>(inv_->model()->subset_costs()[penalized_subset] /
                            (1 + penalties_[penalized_subset])),
         penalized_subset.value()});
    DCHECK(!utility_heap_.IsEmpty());

    // Get removable subsets (Add them to the heap).
    for (const SubsetIndex subset : inv_->newly_removable_subsets()) {
      const float delta_selected = (penalization_factor_ * penalties_[subset] +
                                    inv_->model()->subset_costs()[subset]);
      priority_heap_.Insert({delta_selected, subset.value()});
    }
    DCHECK(!priority_heap_.IsEmpty());

    for (const SubsetIndex subset : {penalized_subset, best_subset}) {
      const float delta = ComputeDelta(subset);
      if (delta < kInfinity) {
        priority_heap_.Insert({delta, subset.value()});
      }
    }
    DCHECK(!priority_heap_.IsEmpty());

    // Get new non removable subsets and remove them from the heap.
    // This is when the priority_heap_ can become empty and end the outer loop
    // early.
    for (const SubsetIndex subset : inv_->newly_non_removable_subsets()) {
      priority_heap_.Remove(subset.value());
    }

    if (inv_->cost() < best_cost) {
      best_cost = inv_->cost();
      best_choices = inv_->is_selected();
    }
  }
  inv_->LoadSolution(best_choices);

  // Improve the solution by removing redundant subsets.
  for (const SubsetIndex& subset : focus) {
    if (inv_->is_selected()[subset] && inv_->ComputeIsRedundant(subset))
      inv_->Deselect(subset, CL::kRedundancy);
  }
  DCHECK_EQ(inv_->num_uncovered_elements(), 0);
  return true;
}

namespace {
void SampleSubsets(std::vector<SubsetIndex>* list, BaseInt num_subsets) {
  num_subsets = std::min(num_subsets, static_cast<BaseInt>(list->size()));
  CHECK_GE(num_subsets, 0);
  std::shuffle(list->begin(), list->end(), absl::BitGen());
  list->resize(num_subsets);
}
}  // namespace

std::vector<SubsetIndex> ClearRandomSubsets(BaseInt num_subsets,
                                            SetCoverInvariant* inv) {
  return ClearRandomSubsets(inv->model()->all_subsets(), num_subsets, inv);
}

std::vector<SubsetIndex> ClearRandomSubsets(absl::Span<const SubsetIndex> focus,
                                            BaseInt num_subsets,
                                            SetCoverInvariant* inv) {
  num_subsets = std::min(num_subsets, static_cast<BaseInt>(focus.size()));
  CHECK_GE(num_subsets, 0);
  std::vector<SubsetIndex> chosen_indices;
  for (const SubsetIndex subset : focus) {
    if (inv->is_selected()[subset]) {
      chosen_indices.push_back(subset);
    }
  }
  SampleSubsets(&chosen_indices, num_subsets);
  BaseInt num_deselected = 0;
  for (const SubsetIndex subset : chosen_indices) {
    inv->Deselect(subset, CL::kCostAndCoverage);
    ++num_deselected;
    for (IntersectingSubsetsIterator it(*inv->model(), subset); !it.at_end();
         ++it) {
      if (!inv->is_selected()[subset]) continue;
      inv->Deselect(subset, CL::kCostAndCoverage);
      ++num_deselected;
    }
    // Note that num_deselected may exceed num_subsets by more than 1.
    if (num_deselected > num_subsets) break;
  }
  return chosen_indices;
}

std::vector<SubsetIndex> ClearMostCoveredElements(BaseInt max_num_subsets,
                                                  SetCoverInvariant* inv) {
  return ClearMostCoveredElements(inv->model()->all_subsets(), max_num_subsets,
                                  inv);
}

std::vector<SubsetIndex> ClearMostCoveredElements(
    absl::Span<const SubsetIndex> focus, BaseInt max_num_subsets,
    SetCoverInvariant* inv) {
  // This is the vector we will return.
  std::vector<SubsetIndex> sampled_subsets;

  const ElementToIntVector& coverage = inv->coverage();
  const BaseInt num_subsets = inv->model()->num_subsets();
  const SparseRowView& rows = inv->model()->rows();

  // Collect the sets which have at least one element whose coverage > 1,
  // even if those sets are not removable.
  SubsetBoolVector subset_is_collected(num_subsets, false);
  for (const ElementIndex element : inv->model()->ElementRange()) {
    if (coverage[element] <= 1) continue;
    for (const SubsetIndex subset : rows[element]) {
      if (inv->is_selected()[subset] && !subset_is_collected[subset]) {
        subset_is_collected[subset] = true;
      }
    }
  }

  // Now intersect with focus: sampled_subsets = focus ⋂ impacted_subsets.
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
  sampled_subsets.resize(
      std::min(static_cast<BaseInt>(sampled_subsets.size()), max_num_subsets));

  // Testing has shown that sorting sampled_subsets is not necessary.
  // Now, un-select the subset in sampled_subsets.
  for (const SubsetIndex subset : sampled_subsets) {
    inv->Deselect(subset, CL::kCostAndCoverage);
  }
  return sampled_subsets;
}

}  // namespace operations_research
