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

#include "ortools/algorithms/set_cover_invariant.h"

#include <algorithm>
#include <limits>
#include <tuple>
#include <vector>

#include "absl/log/check.h"
#include "ortools/algorithms/set_cover_model.h"
#include "ortools/base/logging.h"

namespace operations_research {
// Note: in many of the member functions, variables have "crypterse" names
// to avoid confusing them with member data. For example mrgnl_impcts is used
// to avoid confusion with num_free_elements_.
void SetCoverInvariant::Initialize() {
  DCHECK(model_->ComputeFeasibility());
  model_->CreateSparseRowView();

  cost_ = 0.0;

  const SubsetIndex num_subsets(model_->num_subsets());
  is_selected_.assign(num_subsets, false);
  is_removable_.assign(num_subsets, false);
  num_free_elements_.assign(num_subsets, ElementIndex(0));
  num_coverage_le_1_elements_.assign(num_subsets, ElementIndex(0));

  const SparseColumnView& columns = model_->columns();
  for (SubsetIndex subset(0); subset < num_subsets; ++subset) {
    num_free_elements_[subset] = columns[subset].size().value();
    num_coverage_le_1_elements_[subset] = columns[subset].size().value();
  }

  const ElementIndex num_elements(model_->num_elements());
  coverage_.assign(num_elements, SubsetIndex(0));

  num_uncovered_elements_ = num_elements;
}

bool SetCoverInvariant::CheckConsistency() const {
  auto [cst, cvrg] = ComputeCostAndCoverage(is_selected_);
  CHECK_EQ(cost_, cst);
  for (ElementIndex element(0); element < model_->num_elements(); ++element) {
    CHECK_EQ(cvrg[element], coverage_[element]);
  }
  auto [num_uncvrd_elts, mrgnl_impcts, num_01_elts, is_rdndnt] =
      ComputeImpliedData(cvrg);
  const SparseColumnView& columns = model_->columns();
  for (SubsetIndex subset(0); subset < columns.size(); ++subset) {
    CHECK_EQ(mrgnl_impcts[subset], num_free_elements_[subset]);
    CHECK_EQ(is_rdndnt[subset], is_removable_[subset]);
    CHECK_EQ(is_rdndnt[subset], num_01_elts[subset] == 0);
  }
  return true;
}

void SetCoverInvariant::LoadSolution(const SubsetBoolVector& c) {
  is_selected_ = c;
  RecomputeInvariant();
}

void SetCoverInvariant::RecomputeInvariant() {
  std::tie(cost_, coverage_) = ComputeCostAndCoverage(is_selected_);
  std::tie(num_uncovered_elements_, num_free_elements_,
           num_coverage_le_1_elements_, is_removable_) =
      ComputeImpliedData(coverage_);
}

std::tuple<Cost, ElementToSubsetVector>
SetCoverInvariant::ComputeCostAndCoverage(
    const SubsetBoolVector& choices) const {
  Cost cst = 0.0;
  ElementToSubsetVector cvrg(model_->num_elements(), SubsetIndex(0));
  const SparseColumnView& columns = model_->columns();
  // Initialize coverage, update cost, and compute the coverage for
  // all the elements covered by the selected subsets.
  const SubsetCostVector& subset_costs = model_->subset_costs();
  for (SubsetIndex subset(0); bool b : choices) {
    if (b) {
      cst += subset_costs[subset];
      for (ElementIndex element : columns[subset]) {
        ++cvrg[element];
      }
    }
    ++subset;
  }
  return {cst, cvrg};
}

std::tuple<ElementIndex,           // Number of uncovered elements
           SubsetToElementVector,  // Number of free elements for each subset
           SubsetToElementVector,  // Number of elem covered 0 or 1 times.
           SubsetBoolVector>       // Removability for each subset
SetCoverInvariant::ComputeImpliedData(const ElementToSubsetVector& cvrg) const {
  const ElementIndex num_elements(model_->num_elements());
  ElementIndex num_uncvrd_elts(num_elements);

  const SubsetIndex num_subsets(model_->num_subsets());
  SubsetToElementVector num_ovrcvrd_elts(num_subsets, ElementIndex(0));
  SubsetToElementVector num_free_elts(num_subsets, ElementIndex(0));
  SubsetToElementVector num_cvrg_le_1_elts(num_subsets, ElementIndex(0));
  SubsetBoolVector is_rdndnt(num_subsets, false);

  const SparseColumnView& columns = model_->columns();

  // Initialize number of free elements and number of elements covered 0 or 1.
  for (SubsetIndex subset(0); subset < num_subsets; ++subset) {
    num_free_elts[subset] = columns[subset].size().value();
    num_cvrg_le_1_elts[subset] = columns[subset].size().value();
  }

  // Update num_ovrcvrd_elts[subset] and num_elts_cvrd.
  // When all elements in subset are overcovered, is_removable_[subset] is true.
  const SparseRowView& rows = model_->rows();

  for (ElementIndex element(0); element < num_elements; ++element) {
    if (cvrg[element] == 1) {
      --num_uncvrd_elts;  // Just need to update this ...
      for (SubsetIndex subset : rows[element]) {
        --num_free_elts[subset];
      }
    } else {
      if (cvrg[element] >= 2) {
        --num_uncvrd_elts;  // Same as when cvrg[element] == 1
        for (SubsetIndex subset : rows[element]) {
          --num_free_elts[subset];  // Same as when cvrg[element] == 1
          --num_cvrg_le_1_elts[subset];
          if (num_cvrg_le_1_elts[subset] == 0) {
            is_rdndnt[subset] = true;
          }
        }
      }
    }
  }
  return {num_uncvrd_elts, num_free_elts, num_cvrg_le_1_elts, is_rdndnt};
}

std::vector<SubsetIndex> SetCoverInvariant::Toggle(SubsetIndex subset,
                                                   bool value) {
  if (value) {
    DCHECK(!is_removable_[subset]);
    DCHECK_GT(num_free_elements_[subset], 0);
    return UnsafeUse(subset);
  } else {
    DCHECK(is_removable_[subset]);
    return UnsafeRemove(subset);
  }
}

std::vector<SubsetIndex> SetCoverInvariant::UnsafeToggle(SubsetIndex subset,
                                                         bool value) {
  if (value) {
    return UnsafeUse(subset);
  } else {
    return UnsafeRemove(subset);
  }
}

namespace {
void CollectSubsets(SubsetIndex s, std::vector<SubsetIndex>* subsets,
                    SubsetBoolVector* subset_seen) {
  if (!(*subset_seen)[s]) {
    (*subset_seen)[s] = true;
    subsets->push_back(s);
  }
}
}  // namespace

std::vector<SubsetIndex> SetCoverInvariant::UnsafeUse(SubsetIndex subset) {
  DCHECK(!is_selected_[subset]);
  DVLOG(1) << "Selecting subset " << subset;
  is_selected_[subset] = true;
  const SubsetCostVector& subset_costs = model_->subset_costs();
  cost_ += subset_costs[subset];

  const SparseColumnView& columns = model_->columns();

  SubsetBoolVector subset_seen(columns.size(), false);
  std::vector<SubsetIndex> impacted_subsets;
  impacted_subsets.reserve(columns.size().value());

  // Initialize subset_seen so that `subset` not be in impacted_subsets.
  subset_seen[subset] = true;

  const SparseRowView& rows = model_->rows();
  for (const ElementIndex element : columns[subset]) {
    // Update coverage.
    ++coverage_[element];
    if (coverage_[element] == 1) {
      // `element` is newly covered.
      --num_uncovered_elements_;
      for (const SubsetIndex impacted_subset : rows[element]) {
        CollectSubsets(impacted_subset, &impacted_subsets, &subset_seen);
        --num_free_elements_[impacted_subset];
      }
    } else if (coverage_[element] == 2) {
      // `element` is newly overcovered.
      for (const SubsetIndex impacted_subset : rows[element]) {
        CollectSubsets(impacted_subset, &impacted_subsets, &subset_seen);
        --num_coverage_le_1_elements_[impacted_subset];
        if (num_coverage_le_1_elements_[impacted_subset] == 0) {
          // All the elements in impacted_subset are now overcovered, so it
          // is removable. Note that this happens only when the last element
          // of impacted_subset becomes overcovered.
          is_removable_[impacted_subset] = true;
        }
      }
    }
  }
  DCHECK(CheckConsistency());
  return impacted_subsets;
}

std::vector<SubsetIndex> SetCoverInvariant::UnsafeRemove(SubsetIndex subset) {
  DCHECK(is_selected_[subset]);
  // If already selected, then num_free_elements == 0.
  DCHECK_EQ(num_free_elements_[subset], 0);
  DVLOG(1) << "Deselecting subset " << subset;
  is_selected_[subset] = false;
  const SubsetCostVector& subset_costs = model_->subset_costs();
  cost_ -= subset_costs[subset];

  const SparseColumnView& columns = model_->columns();

  SubsetBoolVector subset_seen(columns.size(), false);
  std::vector<SubsetIndex> impacted_subsets;
  impacted_subsets.reserve(columns.size().value());

  // Initialize subset_seen so that `subset` not be in impacted_subsets.
  subset_seen[subset] = true;

  const SparseRowView& rows = model_->rows();
  for (const ElementIndex element : columns[subset]) {
    // Update coverage.
    --coverage_[element];
    if (coverage_[element] == 0) {
      // `element` is no longer covered.
      ++num_uncovered_elements_;
      for (const SubsetIndex impacted_subset : rows[element]) {
        CollectSubsets(impacted_subset, &impacted_subsets, &subset_seen);
        ++num_free_elements_[impacted_subset];
      }
    } else if (coverage_[element] == 1) {
      // `element` is no longer overcovered.
      for (const SubsetIndex impacted_subset : rows[element]) {
        CollectSubsets(impacted_subset, &impacted_subsets, &subset_seen);
        ++num_coverage_le_1_elements_[impacted_subset];
        if (num_coverage_le_1_elements_[impacted_subset] == 1) {
          // There is one element of impacted_subset which is not overcovered.
          // impacted_subset has just become non-removable.
          is_removable_[impacted_subset] = false;
        }
      }
    }
  }
  DCHECK(CheckConsistency());
  return impacted_subsets;
}

SetCoverSolutionResponse SetCoverInvariant::ExportSolutionAsProto() const {
  SetCoverSolutionResponse message;
  message.set_num_subsets(is_selected_.size().value());
  Cost lower_bound = std::numeric_limits<Cost>::max();
  for (SubsetIndex subset(0); subset < model_->num_subsets(); ++subset) {
    if (is_selected_[subset]) {
      message.add_subset(subset.value());
    }
    lower_bound = std::min(model_->subset_costs()[subset], lower_bound);
  }
  message.set_cost(cost_);
  message.set_cost_lower_bound(lower_bound);
  return message;
}

void SetCoverInvariant::ImportSolutionFromProto(
    const SetCoverSolutionResponse& message) {
  is_selected_.resize(SubsetIndex(message.num_subsets()), false);
  for (auto s : message.subset()) {
    is_selected_[SubsetIndex(s)] = true;
  }
  RecomputeInvariant();
  Cost cost = message.cost();
  CHECK_EQ(cost, cost_);
}

}  // namespace operations_research
