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

#include "ortools/algorithms/weighted_set_covering.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/random/random.h"
#include "ortools/algorithms/weighted_set_covering_model.h"
#include "ortools/base/adjustable_priority_queue-inl.h"  // IWYU pragma: keep
#include "ortools/base/logging.h"

namespace operations_research {

constexpr SubsetIndex kNotFound(-1);

void WeightedSetCoveringSolver::Initialize() {
  DCHECK(model_.ComputeFeasibility());
  model_.CreateSparseRowView();
  const SubsetIndex num_subsets(model_.num_subsets());
  choices_.assign(num_subsets, false);
  is_removable_.assign(num_subsets, false);
  times_penalized_.assign(num_subsets, 0);
  marginal_impacts_.assign(num_subsets, ElementIndex(0));
  const ElementIndex num_elements(model_.num_elements());
  coverage_.assign(num_elements, SubsetIndex(0));
  cost_ = 0.0;
  const SparseColumnView& columns = model_.columns();
  pq_elements_.assign(num_subsets, SubsetPriority());
  pq_.Clear();
  const SubsetCostVector& subset_costs = model_.subset_costs();
  for (SubsetIndex subset(0); subset < num_subsets; ++subset) {
    marginal_impacts_[subset] = columns[subset].size().value();
  }
  penalized_costs_ = model_.subset_costs();
  gts_priorities_ = model_.subset_costs();
}

void WeightedSetCoveringSolver::StoreSolution() {
  best_solution_.StoreCostAndSolution(cost_, choices_);
}

void WeightedSetCoveringSolver::RestoreSolution() {
  choices_ = best_solution_.choices();
  cost_ = best_solution_.cost();
  coverage_ = ComputeCoverage(choices_);
  DCHECK(CheckSolution());
}

bool WeightedSetCoveringSolver::CheckSolution() const {
  Cost total_cost = 0;
  const SubsetCostVector& subset_costs = model_.subset_costs();
  const SubsetIndex num_subsets(model_.num_subsets());
  for (SubsetIndex subset(0); subset < num_subsets; ++subset) {
    if (choices_[subset]) {
      total_cost += subset_costs[subset];
    }
  }
  DCHECK_EQ(cost_, total_cost);
  return CheckCoverageAndMarginalImpacts(choices_);
}

ElementToSubsetVector WeightedSetCoveringSolver::ComputeCoverage(
    const ChoiceVector& choices) const {
  const ElementIndex num_elements(model_.num_elements());
  const SparseRowView& rows = model_.rows();
  // Use "crypterse" naming to avoid confusing with coverage_.
  ElementToSubsetVector cvrg(num_elements, SubsetIndex(0));
  for (ElementIndex element(0); element < num_elements; ++element) {
    for (SubsetIndex subset : rows[element]) {
      if (choices[subset]) {
        ++cvrg[element];
      }
    }
    DCHECK_LE(cvrg[element], rows[element].size().value());
    DCHECK_GE(cvrg[element], 0);
  }
  return cvrg;
}

SubsetToElementVector WeightedSetCoveringSolver::ComputeMarginalImpacts(
    const ElementToSubsetVector& cvrg) const {
  const ElementIndex num_elements(model_.num_elements());
  DCHECK_EQ(num_elements, cvrg.size());
  const SparseColumnView& columns = model_.columns();
  const SubsetIndex num_subsets(model_.num_subsets());
  // Use "crypterse" naming to avoid confusing with marginal_impacts_.
  SubsetToElementVector mrgnl_impcts(num_subsets, ElementIndex(0));
  for (SubsetIndex subset(0); subset < num_subsets; ++subset) {
    for (ElementIndex element : columns[subset]) {
      if (cvrg[element] == 0) {
        ++mrgnl_impcts[subset];
      }
    }
    DCHECK_LE(mrgnl_impcts[subset], columns[subset].size().value());
    DCHECK_GE(mrgnl_impcts[subset], 0);
  }
  return mrgnl_impcts;
}

bool WeightedSetCoveringSolver::CheckCoverage(
    const ChoiceVector& choices) const {
  const SubsetIndex num_subsets(model_.num_subsets());
  DCHECK_EQ(num_subsets, choices.size());
  const ElementIndex num_elements(model_.num_elements());
  const ElementToSubsetVector cvrg = ComputeCoverage(choices);
  for (ElementIndex element(0); element < num_elements; ++element) {
    // TODO(user): do a LOG(ERROR) instead
    DCHECK_EQ(coverage_[element], cvrg[element]) << " Element = " << element;
  }
  return true;
}

bool WeightedSetCoveringSolver::CheckCoverageAndMarginalImpacts(
    const ChoiceVector& choices) const {
  const SubsetIndex num_subsets(model_.num_subsets());
  DCHECK_EQ(num_subsets, choices.size());
  const ElementToSubsetVector cvrg = ComputeCoverage(choices);
  const ElementIndex num_elements(model_.num_elements());
  for (ElementIndex element(0); element < num_elements; ++element) {
    DCHECK_EQ(coverage_[element], cvrg[element]) << " Element = " << element;
  }
  const SubsetToElementVector mrgnl_impcts = ComputeMarginalImpacts(cvrg);
  for (SubsetIndex subset(0); subset < num_subsets; ++subset) {
    DCHECK_EQ(marginal_impacts_[subset], mrgnl_impcts[subset])
        << " Subset = " << subset;
  }
  return true;
}

ElementToSubsetVector WeightedSetCoveringSolver::ComputeSingleSubsetCoverage(
    SubsetIndex subset) const {
  const SparseColumnView& columns = model_.columns();
  const ElementIndex num_elements(model_.num_elements());
  ElementToSubsetVector cvrg(num_elements, SubsetIndex(0));
  const SparseRowView& rows = model_.rows();
  for (const ElementIndex element : columns[subset]) {
    for (SubsetIndex subset : rows[element]) {
      if (choices_[subset]) {
        ++cvrg[element];
      }
    }
    DCHECK_LE(cvrg[element], rows[element].size().value());
    DCHECK_GE(cvrg[element], 0);
  }
  return cvrg;
}

bool WeightedSetCoveringSolver::CheckSingleSubsetCoverage(
    SubsetIndex subset) const {
  ElementToSubsetVector cvrg = ComputeSingleSubsetCoverage(subset);
  const SparseColumnView& columns = model_.columns();
  for (const ElementIndex element : columns[subset]) {
    DCHECK_EQ(coverage_[element], cvrg[element]) << " Element = " << element;
  }
  return true;
}

void WeightedSetCoveringSolver::UpdateCoverage(SubsetIndex subset, bool value) {
  const SparseColumnView& columns = model_.columns();
  const int delta = value ? 1 : -1;
  for (const ElementIndex element : columns[subset]) {
#ifndef NDEBUG
    const SubsetIndex previous_coverage = coverage_[element];
#endif  // NDEBUG
    coverage_[element] += delta;
#ifndef NDEBUG
    VLOG(1) << "Coverage of element " << element << " changed from "
            << previous_coverage << " to " << coverage_[element];
#endif  // NDEBUG
    DCHECK_GE(coverage_[element], 0);
    DCHECK_LE(coverage_[element], columns[subset].size().value());
  }
#ifndef NDEBUG
  ElementToSubsetVector cvrg = ComputeSingleSubsetCoverage(subset);
  for (const ElementIndex element : columns[subset]) {
    DCHECK_EQ(coverage_[element], cvrg[element]) << " Element = " << element;
  }
#endif
}

// Compute the impact of the change in the assignment for each subset
// containing element. Store this in a flat_hash_set so as to buffer the
// change.
std::vector<SubsetIndex> WeightedSetCoveringSolver::ComputeImpactedSubsets(
    SubsetIndex subset) const {
  const SparseColumnView& columns = model_.columns();
  const SparseRowView& rows = model_.rows();
  absl::flat_hash_set<SubsetIndex> impacted_subsets_collection;
  for (const ElementIndex element : columns[subset]) {
    for (const SubsetIndex subset : rows[element]) {
      impacted_subsets_collection.insert(subset);
    }
  }
  DCHECK(impacted_subsets_collection.find(subset) !=
         impacted_subsets_collection.end());
  std::vector<SubsetIndex> impacted_subsets(impacted_subsets_collection.begin(),
                                            impacted_subsets_collection.end());
  DCHECK_LE(impacted_subsets.size(), model_.num_subsets());
  std::sort(impacted_subsets.begin(), impacted_subsets.end());
  return impacted_subsets;
}

void WeightedSetCoveringSolver::UpdateIsRemovable(
    const std::vector<SubsetIndex>& impacted_subsets) {
  const SparseColumnView& columns = model_.columns();
  for (const SubsetIndex subset : impacted_subsets) {
    is_removable_[subset] = true;
    for (const ElementIndex element : columns[subset]) {
      if (coverage_[element] == 1) {
        is_removable_[subset] = false;
        break;
      }
    }
    DCHECK_EQ(is_removable_[subset], CanBeRemoved(subset));
  }
}

void WeightedSetCoveringSolver::UpdateMarginalImpacts(
    const std::vector<SubsetIndex>& impacted_subsets) {
  const SparseColumnView& columns = model_.columns();
  for (const SubsetIndex subset : impacted_subsets) {
    ElementIndex impact(0);
    for (const ElementIndex element : columns[subset]) {
      if (coverage_[element] == 0) {
        ++impact;
      }
    }
#ifndef NDEBUG
    VLOG(1) << "Changing impact of subset " << subset << " from "
            << marginal_impacts_[subset] << " to " << impact;
#endif
    marginal_impacts_[subset] = impact;
    DCHECK_LE(marginal_impacts_[subset], columns[subset].size().value());
    DCHECK_GE(marginal_impacts_[subset], 0);
  }
}

void WeightedSetCoveringSolver::ToggleChoice(SubsetIndex subset, bool value) {
#ifndef NDEBUG
  VLOG(1) << "Changing assignment of subset " << subset << " to " << value;
#endif
  DCHECK(choices_[subset] != value);
  const SubsetCostVector& subset_costs = model_.subset_costs();
  cost_ += value ? subset_costs[subset] : -subset_costs[subset];
  choices_[subset] = value;
}

std::vector<SubsetIndex> WeightedSetCoveringSolver::Toggle(SubsetIndex subset,
                                                           bool value) {
  ToggleChoice(subset, value);
  UpdateCoverage(subset, value);
  const std::vector<SubsetIndex> impacted_subsets =
      ComputeImpactedSubsets(subset);
  UpdateIsRemovable(impacted_subsets);
  return impacted_subsets;
}

void WeightedSetCoveringSolver::GenerateTrivialSolution() {
  const SubsetCostVector& subset_costs = model_.subset_costs();
  const SparseColumnView& columns = model_.columns();
  const SubsetIndex num_subsets(model_.num_subsets());
  for (SubsetIndex subset(0); subset < num_subsets; ++subset) {
    choices_[subset] = true;
    cost_ += subset_costs[subset];
  }
  coverage_ = ComputeCoverage(choices_);
  const ElementIndex num_elements = model_.num_elements();
  for (ElementIndex element(0); element < num_elements; ++element) {
    DCHECK_GT(coverage_[element], SubsetIndex(0));
  }
  for (SubsetIndex subset(0); subset < num_subsets; ++subset) {
    marginal_impacts_[subset] = 0;
  }
  DCHECK(CheckSolution());
  DCHECK(CheckCoverageAndMarginalImpacts(choices_));
  StoreSolution();
}

void WeightedSetCoveringSolver::UpdateGreedyPriorities(
    const std::vector<SubsetIndex>& impacted_subsets) {
  const SubsetCostVector& subset_costs = model_.subset_costs();
  for (const SubsetIndex subset : impacted_subsets) {
    const ElementIndex marginal_impact(marginal_impacts_[subset]);
    if (marginal_impact != 0) {
      const Cost marginal_cost_increase =
          subset_costs[subset] / marginal_impact.value();
      pq_elements_[subset].SetPriority(-marginal_cost_increase);
      pq_.NoteChangedPriority(&pq_elements_[subset]);
#ifndef NDEBUG
      VLOG(1) << "Priority of subset " << subset << " is now "
              << pq_elements_[subset].GetPriority();
#endif
    } else if (pq_.Contains(&pq_elements_[subset])) {
#ifndef NDEBUG
      VLOG(1) << "Removing subset " << subset << " from priority queue";
#endif
      pq_.Remove(&pq_elements_[subset]);
    }
  }
}

void WeightedSetCoveringSolver::GenerateGreedySolution() {
  const SparseColumnView& columns = model_.columns();
  const SubsetCostVector& subset_costs = model_.subset_costs();
  const SubsetIndex num_subsets(model_.num_subsets());
  pq_elements_.assign(num_subsets, SubsetPriority());
  pq_.Clear();
  // The priority is the minimum marginal cost increase. Since the
  // priority queue returns the smallest value, we use the opposite.
  for (SubsetIndex subset(0); subset < num_subsets; ++subset) {
    marginal_impacts_[subset] = columns[subset].size().value();
    const Cost marginal_cost_increase =
        subset_costs[subset] / marginal_impacts_[subset].value();
    pq_elements_[subset] =
        SubsetPriority(subset.value(), subset, -marginal_cost_increase);
    pq_.Add(&pq_elements_[subset]);
  }
  const ElementIndex num_elements(model_.num_elements());
  ElementIndex num_uncovered_elements(num_elements);
  while (num_uncovered_elements > 0) {
    const auto top = pq_.Top();
    const SubsetIndex best_subset = top->GetSubset();
#ifndef NDEBUG
    VLOG(1) << "Best subset: " << best_subset.value()
            << " Priority = " << top->GetPriority()
            << " queue size = " << pq_.Size();
#endif
    const std::vector<SubsetIndex> impacted_subsets = Toggle(best_subset, true);
    UpdateMarginalImpacts(impacted_subsets);
    DCHECK(CheckCoverageAndMarginalImpacts(choices_));
    DCHECK_EQ(marginal_impacts_[best_subset], 0);
    UpdateGreedyPriorities(impacted_subsets);
    // Update num_uncovered_elements_.
    // By definition the elements of best_subset are all covered. Only the ones
    // that are covered only once are the ones that are newly covered
    for (const ElementIndex element : columns[best_subset]) {
      if (coverage_[element] == 1) {
        --num_uncovered_elements;
      }
    }
  }
  DCHECK_EQ(num_uncovered_elements, 0);
  DCHECK(CheckSolution());
  StoreSolution();
}

bool WeightedSetCoveringSolver::CanBeRemoved(SubsetIndex subset) const {
  DCHECK(CheckSingleSubsetCoverage(subset));
  const SparseColumnView& columns = model_.columns();
  for (const ElementIndex element : columns[subset]) {
    if (coverage_[element] == 1) {
      return false;
    }
  }
  return true;
}

void WeightedSetCoveringSolver::UpdateSteepestPriorities(
    const std::vector<SubsetIndex>& impacted_subsets) {
  // Update priority queue.
  const SubsetCostVector& subset_costs = model_.subset_costs();
  for (const SubsetIndex subset : impacted_subsets) {
    if (choices_[subset] && is_removable_[subset]) {
#ifndef NDEBUG
      VLOG(1) << "Removing subset " << subset << " from priority queue";
#endif
    } else {
      pq_elements_[subset].SetPriority(-subset_costs[subset]);
      pq_.NoteChangedPriority(&pq_elements_[subset]);
    }
  }
}

void WeightedSetCoveringSolver::Steepest(int num_iterations) {
  const SparseColumnView& columns = model_.columns();
  const SubsetCostVector& subset_costs = model_.subset_costs();
  // Create priority queue with cost of using a subset, by decreasing order.
  // Do it only for removable subsets.
  pq_elements_.clear();
  pq_elements_.assign(columns.size(), SubsetPriority());
  pq_.Clear();
  for (SubsetIndex subset(0); subset < columns.size(); ++subset) {
    // The priority is the gain from removing the subset from the solution.
    const Cost priority = (choices_[subset] && CanBeRemoved(subset))
                              ? subset_costs[subset]
                              : -subset_costs[subset];
    pq_elements_[subset] = SubsetPriority(subset.value(), subset, priority);
    pq_.Add(&pq_elements_[subset]);
  }
  // TODO(user): Pop() the priority queue.
  for (int iteration = 0; iteration < num_iterations; ++iteration) {
    const Cost priority = pq_.Top()->GetPriority();
    if (priority < 0) {
      break;
    }
    const SubsetIndex best_subset = pq_.Top()->GetSubset();
    const Cost cost_decrease = subset_costs[best_subset];
#ifndef NDEBUG
    VLOG(1) << "Iteration " << iteration << " Subset: " << best_subset.value()
            << " at " << choices_[best_subset]
            << " can be removed = " << CanBeRemoved(best_subset)
            << " is removable = " << is_removable_[best_subset]
            << " cost_decrease = " << cost_decrease
            << " priority = " << pq_.Top()->GetPriority();
#endif
    DCHECK_EQ(cost_decrease, pq_.Top()->GetPriority());
    DCHECK(choices_[best_subset]);
    DCHECK(CanBeRemoved(best_subset));
    DCHECK_EQ(is_removable_[best_subset], CanBeRemoved(best_subset));
    const std::vector<SubsetIndex> impacted_subsets =
        Toggle(best_subset, false);
    UpdateSteepestPriorities(impacted_subsets);
    DCHECK_EQ(pq_elements_.size(), columns.size());
  }
  StoreSolution();
}

void WeightedSetCoveringSolver::ResetGuidedTabuSearch() {
  const SparseColumnView& columns = model_.columns();
  const SubsetCostVector& subset_costs = model_.subset_costs();
  penalized_costs_ = subset_costs;
  gts_priorities_ = subset_costs;
  for (SubsetIndex subset(0); subset < gts_priorities_.size(); ++subset) {
    gts_priorities_[subset] /= columns[subset].size().value();
  }
}

void WeightedSetCoveringSolver::GuidedTabuSearch(int num_iterations) {
  const SparseColumnView& columns = model_.columns();
  const SubsetCostVector& subset_costs = model_.subset_costs();
  constexpr Cost kMaxPossibleCost = std::numeric_limits<Cost>::max();
  Cost best_cost = best_solution_.cost();
  Cost total_pen_cost = 0;
  for (const Cost pen_cost : penalized_costs_) {
    total_pen_cost += pen_cost;
  }
  for (int iteration = 0; iteration < num_iterations; ++iteration) {
    Cost smallest_penalized_cost_increase = kMaxPossibleCost;
    SubsetIndex best_subset = kNotFound;
    for (SubsetIndex subset(0); subset < columns.size(); ++subset) {
      const Cost penalized_delta = penalized_costs_[subset];
      // TODO(user): re-check that CanBeRemoved is computed/checked properly.
      VLOG(1) << "Subset: " << subset.value() << " at " << choices_[subset]
              << " can be removed = " << CanBeRemoved(subset)
              << " is removable = " << is_removable_[subset]
              << " penalized_delta = " << penalized_delta
              << " smallest_penalized_cost_increase = "
              << smallest_penalized_cost_increase;
      if (!choices_[subset]) {
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
            is_removable_[subset] &&
            // it is not Tabu OR decreases the actual cost:
            (!tabu_list_.Contains(subset) ||
             cost_ - subset_costs[subset] < best_cost)) {
          smallest_penalized_cost_increase = -penalized_delta;
          best_subset = subset;
        }
      }
    }
    if (best_subset == kNotFound) {  // Local minimum reached.
      RestoreSolution();
      return;
    }
    total_pen_cost += smallest_penalized_cost_increase;
    const std::vector<SubsetIndex> impacted_subsets =
        Toggle(best_subset, !choices_[best_subset]);
    UpdateMarginalImpacts(impacted_subsets);
    DCHECK(CheckCoverageAndMarginalImpacts(choices_));
    DCHECK_EQ(marginal_impacts_[best_subset], 0);
    UpdatePenalties();
    tabu_list_.Add(best_subset);
    if (cost_ < best_cost) {
      LOG(INFO) << "It;eration:" << iteration << ", current cost = " << cost_
                << ", best cost = " << best_cost
                << ", penalized cost = " << total_pen_cost;
      best_cost = cost_;
    }
  }
  RestoreSolution();
}

bool WeightedSetCoveringSolver::FlipCoin() const {
  // TODO(user): use STL for repeatable testing.
  return absl::Bernoulli(absl::BitGen(), 0.5);
}

ElementIndex WeightedSetCoveringSolver::ComputeNumElementsAlreadyCovered(
    SubsetIndex subset) const {
  const SparseColumnView& columns = model_.columns();
  ElementIndex num_elements_covered(0);
  for (const ElementIndex element : columns[subset]) {
    if (coverage_[element] >= 1) {
      ++num_elements_covered;
    }
  }
  return num_elements_covered;
}

void WeightedSetCoveringSolver::UpdatePenalties() {
  const SparseColumnView& columns = model_.columns();
  const SubsetCostVector& subset_costs = model_.subset_costs();
  Cost largest_priority = -1.0;
  for (SubsetIndex subset(0); subset < columns.size(); ++subset) {
    if (choices_[subset]) {
      largest_priority = std::max(largest_priority, gts_priorities_[subset]) /
                         ComputeNumElementsAlreadyCovered(subset).value();
    }
  }
  const double radius = radius_factor_ * largest_priority;
  for (SubsetIndex subset(0); subset < columns.size(); ++subset) {
    if (choices_[subset]) {
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

}  // namespace operations_research
