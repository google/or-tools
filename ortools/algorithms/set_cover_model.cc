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

#include "ortools/algorithms/set_cover_model.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

#include "absl/log/check.h"
#include "ortools/algorithms/set_cover.pb.h"
#include "ortools/base/logging.h"
#include "ortools/lp_data/lp_types.h"  // For StrictITIVector.

namespace operations_research {

void SetCoverModel::UpdateAllSubsetsList() {
  const SubsetIndex new_size = columns_.size();
  const SubsetIndex old_size(all_subsets_.size());
  DCHECK_LE(old_size, new_size);
  all_subsets_.resize(new_size.value());
  for (SubsetIndex subset(old_size); subset < new_size; ++subset) {
    all_subsets_[subset.value()] = subset;
  }
}

void SetCoverModel::AddEmptySubset(Cost cost) {
  subset_costs_.push_back(cost);
  columns_.push_back(SparseColumn());
  const SubsetIndex num_subsets(all_subsets_.size());
  all_subsets_.push_back(num_subsets);
  CHECK_EQ(all_subsets_.size(), columns_.size());
  CHECK_EQ(all_subsets_.size(), subset_costs_.size());
  row_view_is_valid_ = false;
}

void SetCoverModel::AddElementToLastSubset(const ElementIndex element) {
  columns_.back().push_back(element);
  num_elements_ = std::max(num_elements_, element + 1);
  // No need to update the list all_subsets_.
  ++num_nonzeros_;
  row_view_is_valid_ = false;
}

void SetCoverModel::AddElementToLastSubset(int element) {
  AddElementToLastSubset(ElementIndex(element));
}

void SetCoverModel::SetSubsetCost(int subset, Cost cost) {
  CHECK(std::isfinite(cost));
  DCHECK_GE(subset, 0);
  const SubsetIndex subset_index(subset);
  const SubsetIndex num_subsets = columns_.size();
  const SubsetIndex new_size = std::max(num_subsets, subset_index + 1);
  columns_.resize(new_size, SparseColumn());
  subset_costs_.resize(new_size, 0.0);
  subset_costs_[subset_index] = cost;
  UpdateAllSubsetsList();
  row_view_is_valid_ = false;  // Probably overkill, but better safe than sorry.
}

void SetCoverModel::AddElementToSubset(int element, int subset) {
  const SubsetIndex subset_index(subset);
  const SubsetIndex new_size = std::max(columns_.size(), subset_index + 1);
  subset_costs_.resize(new_size, 0.0);
  columns_.resize(new_size, SparseColumn());
  UpdateAllSubsetsList();
  const ElementIndex new_element(element);
  columns_[subset_index].push_back(new_element);
  num_elements_ = std::max(num_elements_, new_element + 1);
  ++num_nonzeros_;
  row_view_is_valid_ = false;
}

// Reserves num_subsets columns in the model.
void SetCoverModel::ReserveNumSubsets(int num_subsets) {
  SubsetIndex size(num_subsets);
  columns_.resize(size, SparseColumn());
  subset_costs_.resize(size, 0.0);
}

// Reserves num_elements rows in the column indexed by subset.
void SetCoverModel::ReserveNumElementsInSubset(int num_elements, int subset) {
  const SubsetIndex size = std::max(columns_.size(), SubsetIndex(subset + 1));
  subset_costs_.resize(size, 0.0);
  columns_.resize(size, SparseColumn());
  const EntryIndex num_entries(num_elements);
  columns_[SubsetIndex(subset)].reserve(num_entries);
}

void SetCoverModel::CreateSparseRowView() {
  if (row_view_is_valid_) {
    return;
  }
  rows_.resize(num_elements_, SparseRow());
  glop::StrictITIVector<ElementIndex, int> row_sizes(num_elements_, 0);
  for (SubsetIndex subset(0); subset < columns_.size(); ++subset) {
    std::sort(columns_[subset].begin(), columns_[subset].end());
    for (const ElementIndex element : columns_[subset]) {
      ++row_sizes[element];
    }
  }
  for (ElementIndex element(0); element < num_elements_; ++element) {
    rows_[element].reserve(EntryIndex(row_sizes[element]));
  }
  for (SubsetIndex subset(0); subset < columns_.size(); ++subset) {
    for (const ElementIndex element : columns_[subset]) {
      rows_[element].push_back(subset);
    }
  }
  row_view_is_valid_ = true;
}

bool SetCoverModel::ComputeFeasibility() const {
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
  for (SubsetIndex subset(0); subset < columns_.size(); ++subset) {
    CHECK_EQ(all_subsets_[subset.value()], subset) << "subset = " << subset;
  }
  return true;
}

SetCoverProto SetCoverModel::ExportModelAsProto() {
  SetCoverProto message;
  for (SubsetIndex subset(0); subset < columns_.size(); ++subset) {
    SetCoverProto::Subset* subset_proto = message.add_subset();
    subset_proto->set_cost(subset_costs_[subset]);
    std::sort(columns_[subset].begin(), columns_[subset].end());
    for (const ElementIndex element : columns_[subset]) {
      subset_proto->add_element(element.value());
    }
  }
  return message;
}

void SetCoverModel::ImportModelFromProto(const SetCoverProto& message) {
  columns_.clear();
  subset_costs_.clear();
  ReserveNumSubsets(message.subset_size());
  SubsetIndex subset_index(0);
  for (const SetCoverProto::Subset& subset_proto : message.subset()) {
    subset_costs_[SubsetIndex(subset_index)] = subset_proto.cost();
    if (subset_proto.element_size() > 0) {
      columns_[subset_index].reserve(EntryIndex(subset_proto.element_size()));
      for (auto element : subset_proto.element()) {
        columns_[subset_index].push_back(ElementIndex(element));
        num_elements_ =
            ElementIndex(std::max(num_elements_.value(), element + 1));
      }
      ++subset_index;
    }
  }
  UpdateAllSubsetsList();
  CreateSparseRowView();
}

namespace {
double StandardDeviation(const std::vector<ssize_t>& v) {
  const ssize_t size = v.size();
  double n = 0.0;  // n is used in a calculation involving doubles.
  double sum_of_squares = 0.0;
  double sum = 0.0;
  for (ssize_t i = 0; i < size; ++i) {
    double sample = static_cast<double>(v[i]);
    if (sample == 0.0) continue;
    sum_of_squares += sample * sample;
    sum += sample;
    ++n;
  }
  return n == 0.0 ? 0.0 : sqrt((sum_of_squares - sum * sum / n) / n);
}

template <typename T>
SetCoverModel::Stats ComputeStats(std::vector<T> sizes) {
  SetCoverModel::Stats stats;
  stats.min = *std::min_element(sizes.begin(), sizes.end());
  stats.max = *std::max_element(sizes.begin(), sizes.end());
  stats.mean = std::accumulate(sizes.begin(), sizes.end(), 0.0) / sizes.size();
  std::nth_element(sizes.begin(), sizes.begin() + sizes.size() / 2,
                   sizes.end());
  stats.median = sizes[sizes.size() / 2];
  stats.stddev = StandardDeviation(sizes);
  return stats;
}

std::vector<ssize_t> ComputeDeciles(std::vector<ssize_t> values) {
  const int kNumDeciles = 10;
  std::vector<ssize_t> deciles;
  deciles.reserve(kNumDeciles);
  for (int i = 1; i <= kNumDeciles; ++i) {
    std::nth_element(values.begin(),
                     values.begin() + values.size() * i / kNumDeciles - 1,
                     values.end());
    deciles.push_back(values[values.size() * i / kNumDeciles - 1]);
  }
  return deciles;
}
}  // namespace

SetCoverModel::Stats SetCoverModel::ComputeRowStats() {
  std::vector<ssize_t> row_sizes(num_elements().value(), 0);
  for (const SparseColumn& column : columns_) {
    for (const ElementIndex element : column) {
      ++row_sizes[element.value()];
    }
  }
  return ComputeStats(row_sizes);
}

SetCoverModel::Stats SetCoverModel::ComputeColumnStats() {
  std::vector<ssize_t> column_sizes(columns_.size().value());
  for (SubsetIndex subset(0); subset < num_subsets(); ++subset) {
    column_sizes[subset.value()] = columns_[subset].size().value();
  }
  return ComputeStats(column_sizes);
}

std::vector<ssize_t> SetCoverModel::ComputeRowDeciles() const {
  std::vector<ssize_t> row_sizes(num_elements().value(), 0);
  for (const SparseColumn& column : columns_) {
    for (const ElementIndex element : column) {
      ++row_sizes[element.value()];
    }
  }
  return ComputeDeciles(row_sizes);
}

std::vector<ssize_t> SetCoverModel::ComputeColumnDeciles() const {
  std::vector<ssize_t> column_sizes(columns_.size().value());
  for (SubsetIndex subset(0); subset < num_subsets(); ++subset) {
    column_sizes[subset.value()] = columns_[subset].size().value();
  }
  return ComputeDeciles(column_sizes);
}

}  // namespace operations_research
