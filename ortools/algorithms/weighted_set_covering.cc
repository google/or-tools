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
#include <vector>

#include "absl/random/random.h"
#include "ortools/base/adjustable_priority_queue-inl.h"
#include "ortools/base/logging.h"

using operations_research::glop::StrictITIVector;

namespace operations_research {

void WeightedSetCovering::Init() {
  penalized_cost_ = subset_cost_;
  priority_ = subset_cost_;
  for (SubsetIndex subset(0); subset < priority_.size(); ++subset) {
    priority_[subset] /= columns_[subset].size().value();
  }
  SubsetIndex size = subset_cost_.size();
  assignment_.resize(size, false);
  assignment_.assign(size, false);
  times_penalized_.resize(size, 0);
  times_penalized_.assign(size, 0);
  num_subsets_covering_element_.resize(num_elements_, SubsetIndex(0));
  num_subsets_covering_element_.assign(num_elements_, SubsetIndex(0));
  iteration_ = 0;
  cost_ = 0.0;
  best_cost_ = 0.0;
  // TODO(user): make these changeable by the user.
  penalty_factor_ = 0.2;
  lagrangian_factor_ = 100.0;
}

void WeightedSetCovering::AddEmptySubset(Cost cost) {
  subset_cost_.push_back(cost);
  columns_.push_back(StrictITIVector<EntryIndex, ElementIndex>());
}

void WeightedSetCovering::AddElementToLastSubset(int element) {
  ElementIndex new_element(element);
  columns_.back().push_back(new_element);
  num_elements_ = std::max(num_elements_, new_element + 1);
}

void WeightedSetCovering::SetCostOfSubset(Cost cost, int subset) {
  const SubsetIndex subset_index(subset);
  const SubsetIndex size = std::max(columns_.size(), subset_index + 1);
  columns_.resize(size, StrictITIVector<EntryIndex, ElementIndex>());
  subset_cost_.resize(size, 0.0);
  subset_cost_[subset_index] = cost;
}

void WeightedSetCovering::AddElementToSubset(int element, int subset) {
  const SubsetIndex subset_index(subset);
  const SubsetIndex size = std::max(columns_.size(), subset_index + 1);
  subset_cost_.resize(size, 0.0);
  columns_.resize(size, StrictITIVector<EntryIndex, ElementIndex>());
  const ElementIndex new_element(element);
  columns_[subset_index].push_back(new_element);
  num_elements_ = std::max(num_elements_, new_element + 1);
}

void WeightedSetCovering::GenerateGreedySolution() {
  StrictITIVector<SubsetIndex, bool> covers_non_covered_elements(
      columns_.size(), true);
  for (ElementIndex num_zeros(num_elements_); num_zeros > 0;) {
    VLOG(1) << "Remaining zeros: " << num_zeros
            << ", matrix size: " << columns_.size().value();
    Cost min_reduced_cost = kMaxCost;
    SubsetIndex best_subset = kNotFound;
    Cost best_cost_increase = 1e308;

    for (SubsetIndex subset(0); subset < columns_.size(); ++subset) {
      if (!covers_non_covered_elements[subset]) {
        continue;
      }
      if (columns_[subset].size().value() * min_reduced_cost <
          subset_cost_[subset]) {
        continue;
      }
      ElementIndex num_elements_covered(0);
      ElementIndex num_elements_already_covered(0);
      for (const ElementIndex element : columns_[subset]) {
        if (num_subsets_covering_element_[element] == 0) {
          ++num_elements_covered;
        } else {
          ++num_elements_already_covered;
        }
      }
      if (num_elements_covered == 0) {
        covers_non_covered_elements[subset] = false;
        continue;
      }
      Cost cost_increase =
          subset_cost_[subset] +
          lagrangian_factor_ * num_elements_already_covered.value();
      if (cost_increase < min_reduced_cost * num_elements_covered.value()) {
        min_reduced_cost = cost_increase / num_elements_covered.value();
        best_subset = subset;
        best_cost_increase = cost_increase;
      }
    }
    CHECK_NE(kNotFound, best_subset);
    covers_non_covered_elements[best_subset] = false;
    assignment_[best_subset] = true;
    cost_ += subset_cost_[best_subset];
    for (const ElementIndex element : columns_[best_subset]) {
      if (num_subsets_covering_element_[element] == 0) {
        --num_zeros;
      }
      ++num_subsets_covering_element_[element];
    }
  }
  DCHECK(CheckSolution());
  DCHECK(CheckFeasibility());
  StoreSolution();
}

void WeightedSetCovering::UseEverything() {
  for (SubsetIndex subset(0); subset < columns_.size(); ++subset) {
    cost_ += subset_cost_[subset];
    assignment_[subset] = true;
    for (const ElementIndex element : columns_[subset]) {
      ++num_subsets_covering_element_[element];
    }
  }
  DCHECK(CheckFeasibility());
  StoreSolution();
}

void WeightedSetCovering::StoreSolution() {
  VLOG(1) << "Storing solution with cost " << cost_;
  best_assignment_ = assignment_;
  best_cost_ = cost_;
}

void WeightedSetCovering::RestoreSolution() {
  assignment_ = best_assignment_;
  cost_ = best_cost_;
  num_subsets_covering_element_.assign(num_elements_, SubsetIndex(0));
  for (SubsetIndex subset(0); subset < columns_.size(); ++subset) {
    if (assignment_[subset]) {
      for (const ElementIndex element : columns_[subset]) {
        ++num_subsets_covering_element_[element];
      }
    }
  }
  DCHECK(CheckSolution());
  DCHECK(CheckFeasibility());
}

bool WeightedSetCovering::CheckSolution() const {
  Cost cost = 0;
  StrictITIVector<ElementIndex, SubsetIndex> coverage(num_elements_,
                                                      SubsetIndex(0));
  for (SubsetIndex subset(0); subset < columns_.size(); ++subset) {
    if (assignment_[subset]) {
      cost += subset_cost_[subset];
      for (const ElementIndex element : columns_[subset]) {
        ++coverage[element];
      }
    }
  }
  if (cost != cost_) {
    LOG(ERROR) << "Error on cost.";
    return false;
  }
  for (ElementIndex element(0); element < num_elements_; ++element) {
    if (coverage[element] != num_subsets_covering_element_[element]) {
      LOG(ERROR) << "Error on the coverage of element " << element.value();
      return false;
    }
  }
  LOG(INFO) << "Solution cost: " << cost;
  return true;
}

bool WeightedSetCovering::CheckFeasibility() const {
  StrictITIVector<ElementIndex, SubsetIndex> coverage(num_elements_,
                                                      SubsetIndex(0));
  for (SubsetIndex subset(0); subset < columns_.size(); ++subset) {
    for (const ElementIndex element : columns_[subset]) {
      ++coverage[element];
    }
  }
  for (ElementIndex element(0); element < num_elements_; ++element) {
    if (coverage[element] == 0) {
      LOG(ERROR) << "Element " << element.value() << " is not covered.";
      return false;
    }
  }
  SubsetIndex max_coverage(0);
  for (auto element_coverage : coverage) {
    max_coverage = std::max(element_coverage, max_coverage);
    VLOG(2) << element_coverage;
  }
  LOG(INFO) << "Max coverage = " << max_coverage;
  return true;
}

ElementIndex WeightedSetCovering::ComputeNumElementsCovered(
    SubsetIndex subset) const {
  ElementIndex num_elements_covered(0);
  for (const ElementIndex element : columns_[subset]) {
    if (num_subsets_covering_element_[element] > 1) {
      ++num_elements_covered;
    }
  }
  return num_elements_covered;
}

void WeightedSetCovering::Flip(SubsetIndex subset) {
  tabu_list_.Add(subset);
  if (!assignment_[subset]) {
    assignment_[subset] = true;
    cost_ += subset_cost_[subset];
    for (const ElementIndex element : columns_[subset]) {
      ++num_subsets_covering_element_[element];
    }
  } else {
    assignment_[subset] = false;
    cost_ -= subset_cost_[subset];
    for (const ElementIndex element : columns_[subset]) {
      --num_subsets_covering_element_[element];
    }
  }
  LOG(INFO) << "Flipping " << subset.value() << " cost = " << cost_
            << " best cost = " << best_cost_;
  if (cost_ < best_cost_) {
    StoreSolution();
  }
}

bool WeightedSetCovering::CanBeRemoved(SubsetIndex subset) const {
  for (const ElementIndex element : columns_[subset]) {
    DCHECK_LT(0, num_subsets_covering_element_[element]);
    if (num_subsets_covering_element_[element] == 1) {
      return false;
    }
  }
  return true;
}

void WeightedSetCovering::Steepest(int num_iterations) {
  for (int iteration_ = 0; iteration_ < num_iterations; ++iteration_) {
    Cost largest_cost_decrease = 0;
    SubsetIndex best_subset = kNotFound;
    for (SubsetIndex subset(0); subset < columns_.size(); ++subset) {
      if (!assignment_[subset]) {
        continue;
      }
      const Cost cost_decrease = subset_cost_[subset];
      VLOG(1) << "Subset: " << subset.value() << " at " << assignment_[subset]
              << " can be removed = " << CanBeRemoved(subset)
              << " cost_decrease = " << cost_decrease;
      if (cost_decrease > largest_cost_decrease && CanBeRemoved(subset)) {
        largest_cost_decrease = cost_decrease;
        best_subset = subset;
      }
    }
    if (best_subset == kNotFound) {
      StoreSolution();
      return;
    }
    Flip(best_subset);
  }
  StoreSolution();
}

void WeightedSetCovering::ResetGuidedTabuSearch() {
  penalized_cost_ = subset_cost_;
  priority_ = subset_cost_;
  for (SubsetIndex subset(0); subset < priority_.size(); ++subset) {
    priority_[subset] /= columns_[subset].size().value();
  }
  iteration_ = 0;
}

void WeightedSetCovering::GuidedTabuSearch(int num_iterations) {
  Cost gts_cost = 0;
  for (const Cost cost : penalized_cost_) {
    gts_cost += cost;
  }
  for (int iteration_ = 0; iteration_ < num_iterations; ++iteration_) {
    Cost smallest_penalized_cost_increase = 1e308;
    SubsetIndex best_subset = kNotFound;
    for (SubsetIndex subset(0); subset < columns_.size(); ++subset) {
      const Cost penalized_delta = penalized_cost_[subset];
      VLOG(1) << "Subset: " << subset.value() << " at " << assignment_[subset]
              << " can be removed = " << CanBeRemoved(subset)
              << " penalized_delta = " << penalized_delta
              << " smallest_penalized_cost_increase = "
              << smallest_penalized_cost_increase;
      if (!assignment_[subset]) {
        if (penalized_delta < smallest_penalized_cost_increase) {
          smallest_penalized_cost_increase = penalized_delta;
          best_subset = subset;
        }
      } else {
        if (-penalized_delta < smallest_penalized_cost_increase &&
            CanBeRemoved(subset) &&
            (!tabu_list_.Contains(subset) ||
             cost_ - subset_cost_[subset] < best_cost_)) {
          smallest_penalized_cost_increase = -penalized_delta;
          best_subset = subset;
        }
      }
    }
    if (best_subset == kNotFound) {  // Local minimum reached.
      RestoreSolution();
      return;
    }
    gts_cost += smallest_penalized_cost_increase;
    UpdatePenalties();
    Flip(best_subset);
    VLOG(2) << "Iteration:" << iteration_ << ", current cost = " << cost_
            << ", best cost = " << best_cost_
            << ", penalized cost = " << gts_cost;
  }
  RestoreSolution();
}

bool WeightedSetCovering::Probability() const {
  // Flip a coin.
  return absl::Bernoulli(absl::BitGen(), 0.5);
}

void WeightedSetCovering::UpdatePenalties() {
  double largest_priority = -1.0;
  for (SubsetIndex subset(0); subset < columns_.size(); ++subset) {
    if (assignment_[subset]) {
      largest_priority = std::max(largest_priority, priority_[subset]) /
                         ComputeNumElementsCovered(subset).value();
      // divide priority_ by ComputeNumElementsCovered(subset).value()) ?
    }
  }
  const double radius = 1e-8 * largest_priority;
  for (SubsetIndex subset(0); subset < columns_.size(); ++subset) {
    if (assignment_[subset]) {
      const double subset_priority = priority_[subset];
      if (largest_priority - subset_priority <= radius && Probability()) {
        ++times_penalized_[subset];
        const int times_penalized = times_penalized_[subset];
        const Cost cost =
            subset_cost_[subset] / columns_[subset].size().value();
        priority_[subset] = cost / (1 + times_penalized);
        penalized_cost_[subset] =
            cost * (1 + penalty_factor_ * times_penalized);
      }
    }
  }
}

}  // namespace operations_research
