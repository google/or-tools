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
#include <cstdint>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "ortools/algorithms/set_cover.pb.h"
#include "ortools/base/logging.h"

namespace operations_research {

void SetCoverModel::UpdateAllSubsetsList() {
  const BaseInt old_size = all_subsets_.size();
  DCHECK_LE(old_size, num_subsets());
  all_subsets_.resize(num_subsets());
  for (BaseInt subset(old_size); subset < num_subsets(); ++subset) {
    all_subsets_[subset] = SubsetIndex(subset);
  }
}

void SetCoverModel::AddEmptySubset(Cost cost) {
  subset_costs_.push_back(cost);
  columns_.push_back(SparseColumn());
  all_subsets_.push_back(SubsetIndex(num_subsets_));
  ++num_subsets_;
  CHECK_EQ(columns_.size(), num_subsets());
  CHECK_EQ(subset_costs_.size(), num_subsets());
  CHECK_EQ(all_subsets_.size(), num_subsets());
  row_view_is_valid_ = false;
}

void SetCoverModel::AddElementToLastSubset(BaseInt element) {
  columns_.back().push_back(ElementIndex(element));
  num_elements_ = std::max(num_elements_, element + 1);
  // No need to update the list all_subsets_.
  ++num_nonzeros_;
  row_view_is_valid_ = false;
}

void SetCoverModel::AddElementToLastSubset(ElementIndex element) {
  AddElementToLastSubset(element.value());
}

void SetCoverModel::SetSubsetCost(BaseInt subset, Cost cost) {
  CHECK(std::isfinite(cost));
  DCHECK_GE(subset, 0);
  num_subsets_ = std::max(num_subsets_, subset + 1);
  columns_.resize(num_subsets_, SparseColumn());
  subset_costs_.resize(num_subsets_, 0.0);
  subset_costs_[SubsetIndex(subset)] = cost;
  UpdateAllSubsetsList();
  row_view_is_valid_ = false;  // Probably overkill, but better safe than sorry.
}

void SetCoverModel::SetSubsetCost(SubsetIndex subset, Cost cost) {
  SetSubsetCost(subset.value(), cost);
}

void SetCoverModel::AddElementToSubset(BaseInt element, BaseInt subset) {
  num_subsets_ = std::max(num_subsets_, subset + 1);
  subset_costs_.resize(num_subsets_, 0.0);
  columns_.resize(num_subsets_, SparseColumn());
  UpdateAllSubsetsList();
  columns_[SubsetIndex(subset)].push_back(ElementIndex(element));
  num_elements_ = std::max(num_elements_, element + 1);
  ++num_nonzeros_;
  row_view_is_valid_ = false;
}

void SetCoverModel::AddElementToSubset(ElementIndex element,
                                       SubsetIndex subset) {
  AddElementToSubset(element.value(), subset.value());
}

// Reserves num_subsets columns in the model.
void SetCoverModel::ReserveNumSubsets(BaseInt number_of_subsets) {
  num_subsets_ = std::max(num_subsets_, number_of_subsets);
  columns_.resize(num_subsets_, SparseColumn());
  subset_costs_.resize(num_subsets_, 0.0);
}

void SetCoverModel::ReserveNumSubsets(SubsetIndex num_subsets) {
  ReserveNumSubsets(num_subsets.value());
}

// Reserves num_elements rows in the column indexed by subset.
void SetCoverModel::ReserveNumElementsInSubset(BaseInt num_elements,
                                               BaseInt subset) {
  ReserveNumSubsets(subset);
  columns_[SubsetIndex(subset)].reserve(ColumnEntryIndex(num_elements));
}

void SetCoverModel::ReserveNumElementsInSubset(ElementIndex num_elements,
                                               SubsetIndex subset) {
  ReserveNumElementsInSubset(num_elements.value(), subset.value());
}

void SetCoverModel::CreateSparseRowView() {
  if (row_view_is_valid_) {
    return;
  }
  rows_.resize(num_elements_, SparseRow());
  ElementToIntVector row_sizes(num_elements_, 0);
  for (SubsetIndex subset : SubsetRange()) {
    // Sort the columns. It's not super-critical to improve performance here as
    // this needs to be done only once.
    std::sort(columns_[subset].begin(), columns_[subset].end());
    for (const ElementIndex element : columns_[subset]) {
      ++row_sizes[element];
    }
  }
  for (ElementIndex element : ElementRange()) {
    rows_[element].reserve(RowEntryIndex(row_sizes[element]));
  }
  for (SubsetIndex subset : SubsetRange()) {
    for (const ElementIndex element : columns_[subset]) {
      rows_[element].push_back(subset);
    }
  }
  row_view_is_valid_ = true;
}

bool SetCoverModel::ComputeFeasibility() const {
  CHECK_GT(num_elements(), 0);
  CHECK_GT(num_subsets(), 0);
  CHECK_EQ(columns_.size(), num_subsets());
  CHECK_EQ(subset_costs_.size(), num_subsets());
  CHECK_EQ(all_subsets_.size(), num_subsets());
  ElementToIntVector coverage(num_elements_, 0);
  for (const Cost cost : subset_costs_) {
    CHECK_GT(cost, 0.0);
  }
  for (const SparseColumn& column : columns_) {
    CHECK_GT(column.size(), 0);
    for (const ElementIndex element : column) {
      ++coverage[element];
    }
  }
  for (const ElementIndex element : ElementRange()) {
    CHECK_GE(coverage[element], 0);
    if (coverage[element] == 0) {
      return false;
    }
  }
  VLOG(1) << "Max possible coverage = "
          << *std::max_element(coverage.begin(), coverage.end());
  for (const SubsetIndex subset : SubsetRange()) {
    CHECK_EQ(all_subsets_[subset.value()], subset) << "subset = " << subset;
  }
  return true;
}

SetCoverProto SetCoverModel::ExportModelAsProto() {
  SetCoverProto message;
  for (const SubsetIndex subset : SubsetRange()) {
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
    subset_costs_[subset_index] = subset_proto.cost();
    if (subset_proto.element_size() > 0) {
      columns_[subset_index].reserve(
          ColumnEntryIndex(subset_proto.element_size()));
      for (BaseInt element : subset_proto.element()) {
        columns_[subset_index].push_back(ElementIndex(element));
        num_elements_ = std::max(num_elements_, element + 1);
      }
      ++subset_index;
    }
  }
  UpdateAllSubsetsList();
  CreateSparseRowView();
}

namespace {
// Returns the standard deviation of the vector v, excluding those values that
// are zero.
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
    const ssize_t point = values.size() * i / kNumDeciles - 1;
    std::nth_element(values.begin(), values.begin() + point, values.end());
    deciles.push_back(values[point]);
  }
  return deciles;
}
}  // namespace

SetCoverModel::Stats SetCoverModel::ComputeRowStats() {
  std::vector<ssize_t> row_sizes(num_elements(), 0);
  for (const SparseColumn& column : columns_) {
    for (const ElementIndex element : column) {
      ++row_sizes[element.value()];
    }
  }
  return ComputeStats(std::move(row_sizes));
}

SetCoverModel::Stats SetCoverModel::ComputeColumnStats() {
  std::vector<ssize_t> column_sizes(columns_.size());
  for (const SubsetIndex subset : SubsetRange()) {
    column_sizes[subset.value()] = columns_[subset].size();
  }
  return ComputeStats(std::move(column_sizes));
}

std::vector<ssize_t> SetCoverModel::ComputeRowDeciles() const {
  std::vector<ssize_t> row_sizes(num_elements(), 0);
  for (const SparseColumn& column : columns_) {
    for (const ElementIndex element : column) {
      ++row_sizes[element.value()];
    }
  }
  return ComputeDeciles(std::move(row_sizes));
}

std::vector<ssize_t> SetCoverModel::ComputeColumnDeciles() const {
  std::vector<ssize_t> column_sizes(columns_.size());
  for (const SubsetIndex subset : SubsetRange()) {
    column_sizes[subset.value()] = columns_[subset].size();
  }
  return ComputeDeciles(std::move(column_sizes));
}

}  // namespace operations_research
