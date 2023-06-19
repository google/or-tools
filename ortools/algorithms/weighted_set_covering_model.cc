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

#include "ortools/algorithms/weighted_set_covering_model.h"

#include <algorithm>

#include "absl/log/check.h"

namespace operations_research {

void WeightedSetCoveringModel::AddEmptySubset(Cost cost) {
  subset_costs_.push_back(cost);
  columns_.push_back(SparseColumn());
  row_view_is_valid_ = false;
}

void WeightedSetCoveringModel::AddElementToLastSubset(int element) {
  ElementIndex new_element(element);
  columns_.back().push_back(new_element);
  num_elements_ = std::max(num_elements_, new_element + 1);
  row_view_is_valid_ = false;
}

void WeightedSetCoveringModel::SetSubsetCost(int subset, Cost cost) {
  const SubsetIndex subset_index(subset);
  const SubsetIndex size = std::max(columns_.size(), subset_index + 1);
  columns_.resize(size, SparseColumn());
  subset_costs_.resize(size, 0.0);
  subset_costs_[subset_index] = cost;
  row_view_is_valid_ = false;  // Probably overkill, but better safe than sorry.
}

void WeightedSetCoveringModel::AddElementToSubset(int element, int subset) {
  const SubsetIndex subset_index(subset);
  const SubsetIndex size = std::max(columns_.size(), subset_index + 1);
  subset_costs_.resize(size, 0.0);
  columns_.resize(size, SparseColumn());
  const ElementIndex new_element(element);
  columns_[subset_index].push_back(new_element);
  num_elements_ = std::max(num_elements_, new_element + 1);
  row_view_is_valid_ = false;
}

void WeightedSetCoveringModel::CreateSparseRowView() {
  if (row_view_is_valid_) {
    return;
  }
  rows_.resize(num_elements_, SparseRow());
  for (SubsetIndex subset(0); subset < columns_.size(); ++subset) {
    std::sort(columns_[subset].begin(), columns_[subset].end());
    for (const ElementIndex element : columns_[subset]) {
      rows_[element].push_back(subset);
    }
  }
  row_view_is_valid_ = true;
}

bool WeightedSetCoveringModel::ComputeFeasibility() const {
  CHECK_GT(num_elements_, 0);
  CHECK_GT(columns_.size(), 0);
  CHECK_EQ(columns_.size(), subset_costs_.size());

  ElementToSubsetVector coverage(num_elements_, SubsetIndex(0));
  for (const Cost cost : subset_costs_) {
    CHECK_GT(cost, 0.0);
  }
  for (const SparseColumn& column : columns_) {
    CHECK_GT(column.size(), 0);
    for (const ElementIndex element : column) {
      ++coverage[element];
    }
  }
  for (ElementIndex element(0); element < num_elements_; ++element) {
    CHECK_GE(coverage[element], 0);
    if (coverage[element] == 0) {
      return false;
    }
  }
  VLOG(1) << "Max possible coverage = "
          << *std::max_element(coverage.begin(), coverage.end());
  return true;
}
}  // namespace operations_research
