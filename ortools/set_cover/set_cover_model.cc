// Copyright 2010-2025 Google LLC
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

#include "ortools/set_cover/set_cover_model.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/discrete_distribution.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/algorithms/radix_sort.h"
#include "ortools/base/strong_vector.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover.pb.h"

namespace operations_research {

namespace {

// Returns a value in [min, min + scaling_factor * (raw_value - min +
// random_term)], where raw_value is drawn from a discrete distribution, and
// random_term is a double drawn uniformly in [0, 1].
BaseInt DiscreteAffine(absl::BitGen& bitgen,
                       absl::discrete_distribution<BaseInt>& dist, BaseInt min,
                       double scaling_factor) {
  const BaseInt raw_value = dist(bitgen);
  const double random_term = absl::Uniform<double>(bitgen, 0, 1.0);
  const BaseInt affine_value =
      static_cast<BaseInt>(
          std::floor((raw_value - min + random_term) * scaling_factor)) +
      min;
  return affine_value;
}

// For a given view (SparseColumnView or SparseRowView), returns the
// distribution of the sizes of the vectors in the view, which can be used in
// an absl::discrete_distribution.
template <typename View>
std::tuple<BaseInt, std::vector<BaseInt>> ComputeSizeHistogram(
    const View& view) {
  BaseInt max_size = 0;
  BaseInt min_size = std::numeric_limits<BaseInt>::max();
  for (const auto& vec : view) {
    const BaseInt size = vec.size();
    min_size = std::min(min_size, size);
    max_size = std::max(max_size, size);
  }
  std::vector<BaseInt> weights(max_size + 1, 0);
  for (const auto& vec : view) {
    const BaseInt size = vec.size();
    ++weights[size];
  }
  return {min_size, weights};
}

template <typename View>
std::tuple<BaseInt, absl::discrete_distribution<BaseInt>>
ComputeSizeDistribution(const View& view) {
  const auto [min_size, weights] = ComputeSizeHistogram(view);
  absl::discrete_distribution<BaseInt> dist(weights.begin(), weights.end());
  return {min_size, dist};
}
}  // namespace

SetCoverModel SetCoverModel::GenerateRandomModelFrom(
    const SetCoverModel& seed_model, BaseInt num_elements, BaseInt num_subsets,
    double row_scale, double column_scale, double cost_scale) {
  CHECK(seed_model.row_view_is_valid());
  SetCoverModel model;
  StopWatch stop_watch(&(model.generation_duration_));
  DCHECK_GT(row_scale, 0.0);
  DCHECK_GT(column_scale, 0.0);
  DCHECK_GT(cost_scale, 0.0);
  model.num_elements_ = num_elements;
  model.num_nonzeros_ = 0;
  model.ResizeNumSubsets(num_subsets);
  model.UpdateAllSubsetsList();
  absl::BitGen bitgen;

  // Create the distribution of the cardinalities of the subsets based on the
  // histogram of column sizes in the seed model.
  auto [min_column_size, column_dist] =
      ComputeSizeDistribution(seed_model.columns());

  // Create the distribution of the degrees of the elements based on the
  // histogram of row sizes in the seed model.
  auto [min_row_size, row_dist] = ComputeSizeDistribution(seed_model.rows());

  // Prepare the degrees of the elements in the generated model, and use them
  // in a distribution to generate the columns. This ponderates the columns
  // towards the elements with higher degrees. ???
  ElementToIntVector degrees(num_elements);
  for (ElementIndex element(0); element.value() < num_elements; ++element) {
    degrees[element] =
        DiscreteAffine(bitgen, row_dist, min_row_size, row_scale);
  }
  absl::discrete_distribution<BaseInt> degree_dist(degrees.begin(),
                                                   degrees.end());

  // Vector indicating whether the generated model covers an element.
  ElementBoolVector contains_element(num_elements, false);

  // Number of elements in the generated model, using the above vector.
  BaseInt num_elements_covered(0);

  // Maximum number of tries to generate a random element that is not yet in
  // the subset, or a random subset that does not contain the element.
  const int kMaxTries = 10;

  // Loop-local vector indicating whether the currently generated subset
  // contains an element.
  ElementBoolVector subset_already_contains_element(num_elements, false);
  for (SubsetIndex subset(0); subset.value() < num_subsets; ++subset) {
    LOG_EVERY_N_SEC(INFO, 5)
        << absl::StrFormat("Generating subset %d (%.1f%%)", subset.value(),
                           100.0 * subset.value() / num_subsets);
    const BaseInt cardinality =
        DiscreteAffine(bitgen, column_dist, min_column_size, column_scale);
    model.columns_[subset].reserve(cardinality);
    for (BaseInt iter = 0; iter < cardinality; ++iter) {
      int num_tries = 0;
      ElementIndex element;
      // Choose an element that is not yet in the subset at random with a
      // distribution that is proportional to the degree of the element.
      do {
        element = ElementIndex(degree_dist(bitgen));
        CHECK_LT(element.value(), num_elements);
        ++num_tries;
      } while (num_tries < kMaxTries &&
               subset_already_contains_element[element]);
      ++model.num_nonzeros_;
      model.columns_[subset].push_back(element);
      subset_already_contains_element[element] = true;
      if (!contains_element[element]) {
        contains_element[element] = true;
        ++num_elements_covered;
      }
    }
    for (const ElementIndex element : model.columns_[subset]) {
      subset_already_contains_element[element] = false;
    }
  }
  VLOG(1) << "Finished generating the model with " << num_elements_covered
          << " elements covered.";

  // It can happen -- rarely in practice -- that some of the elements cannot be
  // covered. Let's add them to randomly chosen subsets.
  if (num_elements_covered != num_elements) {
    VLOG(1) << "Generated model with " << num_elements - num_elements_covered
            << " elements that cannot be covered. Adding them to random "
               "subsets.";
    SubsetBoolVector element_already_in_subset(num_subsets, false);
    for (ElementIndex element(0); element.value() < num_elements; ++element) {
      VLOG_EVERY_N_SEC(1, 5) << absl::StrFormat(
          "Generating subsets for element %d (%.1f%%)", element.value(),
          100.0 * element.value() / num_elements);
      if (!contains_element[element]) {
        const BaseInt degree =
            DiscreteAffine(bitgen, row_dist, min_row_size, row_scale);
        std::vector<SubsetIndex> generated_subsets;
        generated_subsets.reserve(degree);
        for (BaseInt i = 0; i < degree; ++i) {
          int num_tries = 0;
          SubsetIndex subset_index;
          // The method is the same as above.
          do {
            subset_index = SubsetIndex(DiscreteAffine(
                bitgen, column_dist, min_column_size, column_scale));
            ++num_tries;
          } while (num_tries < kMaxTries &&
                   element_already_in_subset[subset_index]);
          ++model.num_nonzeros_;
          model.columns_[subset_index].push_back(element);
          element_already_in_subset[subset_index] = true;
          generated_subsets.push_back(subset_index);
        }
        for (const SubsetIndex subset_index : generated_subsets) {
          element_already_in_subset[subset_index] = false;
        }
        contains_element[element] = true;
        ++num_elements_covered;
      }
    }
    VLOG(1) << "Finished generating subsets for elements that were not "
               "covered in the original model.";
  }
  VLOG(1) << "Finished generating the model. There are "
          << num_elements - num_elements_covered << " uncovered elements.";

  CHECK_EQ(num_elements_covered, num_elements);

  // TODO(user): if necessary, use a better distribution for the costs.
  // The generation of the costs is done in two steps. First, compute the
  // minimum and maximum costs.
  Cost min_cost = std::numeric_limits<Cost>::infinity();
  Cost max_cost = -min_cost;
  for (const Cost cost : seed_model.subset_costs()) {
    min_cost = std::min(min_cost, cost);
    max_cost = std::max(max_cost, cost);
  }
  // Then, generate random numbers in [min_cost, min_cost + cost_range], where
  // cost_range is defined as:
  const Cost cost_range = cost_scale * (max_cost - min_cost);
  for (Cost& cost : model.subset_costs_) {
    cost = min_cost + absl::Uniform<double>(bitgen, 0, cost_range);
  }
  model.CreateSparseRowView();
  return model;
}

void SetCoverModel::UpdateAllSubsetsList() {
  const BaseInt old_size = all_subsets_.size();
  DCHECK_LE(old_size, num_subsets());
  all_subsets_.resize(num_subsets());
  for (BaseInt subset(old_size); subset < num_subsets(); ++subset) {
    all_subsets_[subset] = SubsetIndex(subset);
  }
}

void SetCoverModel::AddEmptySubset(Cost cost) {
  // TODO(user): refine the logic for is_unicost_ and is_unicost_valid_.
  is_unicost_valid_ = false;
  elements_in_columns_are_sorted_ = false;
  subset_costs_.push_back(cost);
  columns_.push_back(SparseColumn());
  all_subsets_.push_back(SubsetIndex(num_subsets_));
  ++num_subsets_;
  CHECK_EQ(columns_.size(), num_subsets());
  CHECK_EQ(subset_costs_.size(), num_subsets());
  CHECK_EQ(all_subsets_.size(), num_subsets());
  row_view_is_valid_ = false;
  ++timestamp_;
}

void SetCoverModel::AddElementToLastSubset(BaseInt element) {
  elements_in_columns_are_sorted_ = false;
  columns_.back().push_back(ElementIndex(element));
  num_elements_ = std::max(num_elements_, element + 1);
  // No need to update the list all_subsets_.
  ++num_nonzeros_;
  row_view_is_valid_ = false;
  ++timestamp_;
}

void SetCoverModel::AddElementToLastSubset(ElementIndex element) {
  AddElementToLastSubset(element.value());
}

void SetCoverModel::SetSubsetCost(BaseInt subset, Cost cost) {
  // TODO(user): refine the logic for is_unicost_ and is_unicost_valid_.
  // NOMUTANTS -- this is a performance optimization.
  is_unicost_valid_ = false;
  elements_in_columns_are_sorted_ = false;
  CHECK(std::isfinite(cost));
  DCHECK_GE(subset, 0);
  if (subset >= num_subsets()) {
    num_subsets_ = std::max(num_subsets_, subset + 1);
    columns_.resize(num_subsets_, SparseColumn());
    subset_costs_.resize(num_subsets_, 0.0);
    row_view_is_valid_ = false;
    UpdateAllSubsetsList();
  }
  subset_costs_[SubsetIndex(subset)] = cost;
  ++timestamp_;
}

void SetCoverModel::SetSubsetCost(SubsetIndex subset, Cost cost) {
  SetSubsetCost(subset.value(), cost);
}

void SetCoverModel::AddElementToSubset(BaseInt element, BaseInt subset) {
  elements_in_columns_are_sorted_ = false;
  if (subset >= num_subsets()) {
    num_subsets_ = subset + 1;
    subset_costs_.resize(num_subsets_, 0.0);
    columns_.resize(num_subsets_, SparseColumn());
    UpdateAllSubsetsList();
  }
  columns_[SubsetIndex(subset)].push_back(ElementIndex(element));
  num_elements_ = std::max(num_elements_, element + 1);
  ++num_nonzeros_;
  row_view_is_valid_ = false;
  ++timestamp_;
}

void SetCoverModel::AddElementToSubset(ElementIndex element,
                                       SubsetIndex subset) {
  AddElementToSubset(element.value(), subset.value());
}

void SetCoverModel::ResizeNumSubsets(BaseInt num_subsets) {
  num_subsets_ = std::max(num_subsets_, num_subsets);
  columns_.resize(num_subsets_, SparseColumn());
  compressed_columns_.resize(num_subsets_);
  subset_costs_.resize(num_subsets_, 0.0);
  UpdateAllSubsetsList();
  ++timestamp_;
}

void SetCoverModel::ResizeNumElements(BaseInt num_elements) {
  num_elements_ = std::max(num_elements_, num_elements);
  rows_.resize(num_elements_, SparseRow());
  compressed_rows_.resize(num_elements_);
}

void SetCoverModel::ReserveNumElementsInSubset(BaseInt num_elements,
                                               BaseInt subset) {
  ResizeNumSubsets(subset);
  columns_[SubsetIndex(subset)].reserve(ColumnEntryIndex(num_elements));
}

namespace {
template <typename T, typename U>
void RadixSort(util_intops::StrongVector<T, U>& v) {
  BaseInt* data = reinterpret_cast<BaseInt*>(v.data());
  ::operations_research::RadixSort(absl::MakeSpan(data, v.size()));
}
}  // namespace

void SetCoverModel::SortElementsInSubsets() {
  for (const SubsetIndex subset : SubsetRange()) {
    // std::sort(columns_[subset].begin(), columns_[subset].end());
    RadixSort(columns_[subset]);
  }
  elements_in_columns_are_sorted_ = true;
}

void SetCoverModel::CreateSparseRowView() {
  StopWatch stop_watch(&create_sparse_row_view_duration_);
  if (row_view_is_valid_) {
    VLOG(1) << "CreateSparseRowView: already valid";
    return;
  }
  VLOG(1) << "CreateSparseRowView started";
  rows_.resize(num_elements_, SparseRow());
  ElementToIntVector row_sizes(num_elements_, 0);
  for (const SubsetIndex subset : SubsetRange()) {
    RadixSort(columns_[subset]);

    ElementIndex previous_element(-1);
    for (const ElementIndex element : columns_[subset]) {
      CHECK_GT(element, previous_element)
          << "Repetition in column "
          << subset;  // Fail if there is a repetition.
      ++row_sizes[element];
      previous_element = element;
    }
  }
  for (const ElementIndex element : ElementRange()) {
    rows_[element].reserve(RowEntryIndex(row_sizes[element]));
  }
  for (const SubsetIndex subset : SubsetRange()) {
    for (const ElementIndex element : columns_[subset]) {
      rows_[element].emplace_back(subset);
    }
  }
  row_view_is_valid_ = true;
  elements_in_columns_are_sorted_ = true;
  VLOG(1) << "CreateSparseRowView finished";
}

void SetCoverModel::CreateSparseColumnView() {
  StopWatch stop_watch(&create_sparse_column_view_duration_);
  if (column_view_is_valid_) {
    VLOG(1) << "CreateSparseColumnView: already valid";
    return;
  }
  VLOG(1) << "CreateSparseColumnView started";
  columns_.resize(num_subsets_, SparseColumn());
  SubsetToIntVector column_sizes(num_subsets_, 0);
  for (const ElementIndex element : ElementRange()) {
    RadixSort(rows_[element]);

    SubsetIndex previous_subset(-1);
    for (const SubsetIndex subset : rows_[element]) {
      CHECK_GT(subset, previous_subset)
          << "Repetition in row " << element
          << " subset = " << subset;  // Fail if there is a repetition.
      ++column_sizes[subset];
      previous_subset = subset;
    }
  }
  for (const SubsetIndex subset : SubsetRange()) {
    columns_[subset].reserve(ColumnEntryIndex(column_sizes[subset]));
    for (const ElementIndex element : columns_[subset]) {
      columns_[subset].push_back(element);
    }
  }
  column_view_is_valid_ = true;
  subsets_in_rows_are_sorted_ = true;
  VLOG(1) << "CreateSparseColumnView finished";
}

void SetCoverModel::CompressAndAddColumn(SubsetIndex subset,
                                         const SparseColumn& column) {
  compressed_columns_[subset].Load(column);
  num_nonzeros_ += column.size();
}

void SetCoverModel::CompressAndAddRow(ElementIndex element,
                                      const SparseRow& row) {
  compressed_rows_[element].Load(row);
  num_nonzeros_ += row.size();
}

void SetCoverModel::CreateCompressedRowView() {
  compressed_rows_.resize(num_elements_);
  // First, compute the sizes of each row using a dry run.
  using DryRunCompressedRow =
      CompressedStrongList<RowEntryIndex, SubsetIndex, true>;
  using DryRunCompressedRowView =
      util_intops::StrongVector<ElementIndex, DryRunCompressedRow>;
  DryRunCompressedRowView dry_run_rows(num_elements_);
  for (const SubsetIndex subset : SubsetRange()) {
    for (const ElementIndex element : columns_[subset]) {
      dry_run_rows[element].push_back(subset);
    }
  }
  // Now we have the sizes for each row.
  for (const ElementIndex element : ElementRange()) {
    compressed_rows_[element].Reserve(dry_run_rows[element].size_in_bytes());
  }
  // Perform the copy-compression.
  for (const SubsetIndex subset : SubsetRange()) {
    for (const ElementIndex element : columns_[subset]) {
      compressed_rows_[element].push_back(subset);
    }
  }
}

// The code is exactly the same as CreateCompressedRowView, modulo a permutation
// of rows and columns. We have tried to use a template to avoid duplication,
// but this ends up creating more lines of code, that is less readable and more
// difficult to modify.
void SetCoverModel::CreateCompressedColumnView() {
  compressed_columns_.resize(num_subsets_);
  using DryRunCompressedColumn =
      CompressedStrongList<ColumnEntryIndex, ElementIndex, true>;
  using DryRunCompressedColumnView =
      util_intops::StrongVector<SubsetIndex, DryRunCompressedColumn>;
  DryRunCompressedColumnView dry_run_columns(num_subsets_);
  for (const ElementIndex element : ElementRange()) {
    for (const SubsetIndex subset : rows_[element]) {
      dry_run_columns[subset].push_back(element);
    }
  }
  for (const SubsetIndex subset : SubsetRange()) {
    compressed_columns_[subset].Reserve(
        dry_run_columns[subset].size_in_bytes());
  }
  for (const ElementIndex element : ElementRange()) {
    for (const SubsetIndex subset : rows_[element]) {
      compressed_columns_[subset].push_back(element);
    }
  }
}

namespace {
// Returns true if the sparse and compressed columns are equal.
bool Equal(const SparseColumn& sparse_col,
           const CompressedColumn& compressed_col, SubsetIndex subset) {
  CHECK_EQ(sparse_col.empty(), compressed_col.empty());
  ColumnEntryIndex entry(0);
  for (const ElementIndex element : compressed_col) {
    CHECK_EQ(element, sparse_col[entry])
        << "subset = " << subset << " entry = " << entry;
    ++entry;
  }
  return true;
}

// Returns true if the sparse and compressed rows are equal.
bool Equal(const SparseRow& sparse_row, const CompressedRow& compressed_row,
           ElementIndex element) {
  CHECK_EQ(sparse_row.empty(), compressed_row.empty());
  RowEntryIndex entry(0);
  for (const SubsetIndex subset : compressed_row) {
    CHECK_EQ(subset, sparse_row[entry])
        << "element = " << element << " entry = " << entry;
    ++entry;
  }
  return true;
}

// Returns true if the sparse and compressed column views are equal.
bool CheckCompressedColumnView(
    const SparseColumnView& sparse_col_view,
    const CompressedColumnView& compressed_col_view) {
  CHECK_EQ(sparse_col_view.size(), compressed_col_view.size());
  const SubsetIndex num_subsets(sparse_col_view.size());
  for (const SubsetIndex subset : IndexRange(num_subsets)) {
    CHECK(Equal(sparse_col_view[subset], compressed_col_view[subset], subset));
  }
  return true;
}

// Returns true if the sparse and compressed row views are equal.
bool CheckCompressedRowView(const SparseRowView& sparse_row_view,
                            const CompressedRowView& compressed_row_view) {
  CHECK_EQ(sparse_row_view.size(), compressed_row_view.size());
  const ElementIndex num_elements(sparse_row_view.size());
  for (const ElementIndex element : IndexRange(num_elements)) {
    CHECK(
        Equal(sparse_row_view[element], compressed_row_view[element], element));
  }
  return true;
}
}  // namespace

bool SetCoverModel::CheckCompressedColumnView() const {
  return ::operations_research::CheckCompressedColumnView(columns_,
                                                          compressed_columns_);
}

bool SetCoverModel::CheckCompressedRowView() const {
  return ::operations_research::CheckCompressedRowView(rows_, compressed_rows_);
}

void SetCoverModel::CreateCompressedViews() {
  compressed_columns_.resize(num_subsets_);
  for (const SubsetIndex subset : SubsetRange()) {
    compressed_columns_[subset].Load(columns_[subset]);
  }
  compressed_rows_.resize(num_elements_);
  for (const ElementIndex element : ElementRange()) {
    compressed_rows_[element].Load(rows_[element]);
  }
  VLOG(1) << "row compression ratio = " << ComputeRowCompressionRatio();
  VLOG(1) << "column compression ratio = " << ComputeColumnCompressionRatio();
  DCHECK(CheckCompressedColumnView());
  DCHECK(CheckCompressedRowView());
  compressed_views_are_valid_ = true;
}

void SetCoverModel::ConvertCompressedToSparseColumnView() {
  columns_.resize(num_subsets_);
  for (const SubsetIndex subset : SubsetRange()) {
    for (const ElementIndex element : compressed_columns_[subset]) {
      columns_[subset].push_back(element);
    }
  }
}

void SetCoverModel::ConvertCompressedToSparseRowView() {
  rows_.resize(num_elements_);
  for (const ElementIndex element : ElementRange()) {
    for (const SubsetIndex subset : compressed_rows_[element]) {
      rows_[element].push_back(subset);
    }
  }
}

void SetCoverModel::ConvertSparseToCompressedColumnView() {
  compressed_columns_.resize(num_subsets_);
  for (const SubsetIndex subset : SubsetRange()) {
    compressed_columns_[subset].Load(columns_[subset]);
  }
}

void SetCoverModel::ConvertSparseToCompressedRowView() {
  compressed_rows_.resize(num_elements_);
  for (const ElementIndex element : ElementRange()) {
    compressed_rows_[element].Load(rows_[element]);
  }
}

std::vector<SubsetIndex> SetCoverModel::ComputeSliceIndices(
    int num_partitions) {
  if (num_partitions <= 1 || columns_.empty()) {
    return {SubsetIndex(columns_.size())};
  }

  std::vector<BaseInt> partial_sum_nnz(columns_.size());
  partial_sum_nnz[0] = columns_[SubsetIndex(0)].size();
  for (BaseInt col = 1; col < columns_.size(); ++col) {
    partial_sum_nnz[col] =
        partial_sum_nnz[col - 1] + columns_[SubsetIndex(col)].size();
  }
  const BaseInt total_nnz = partial_sum_nnz.back();
  const BaseInt target_nnz = (total_nnz + num_partitions - 1) / num_partitions;

  std::vector<SubsetIndex> partition_points(num_partitions);
  BaseInt threshold = target_nnz;
  BaseInt pos = 0;
  for (const SubsetIndex col : SubsetRange()) {
    if (partial_sum_nnz[col.value()] >= threshold) {
      partition_points[pos] = col;
      ++pos;
      threshold += target_nnz;
    }
  }
  partition_points[num_partitions - 1] = SubsetIndex(columns_.size());
  return partition_points;
}

SparseRowView SetCoverModel::ComputeSparseRowViewSlice(SubsetIndex begin_subset,
                                                       SubsetIndex end_subset) {
  SparseRowView rows;
  rows.reserve(num_elements_);
  ElementToIntVector row_sizes(num_elements_, 0);
  for (SubsetIndex subset = begin_subset; subset < end_subset; ++subset) {
    RadixSort(columns_[subset]);

    ElementIndex previous_element(-1);
    for (const ElementIndex element : columns_[subset]) {
      CHECK_GT(element, previous_element)
          << "Repetition in column "
          << subset;  // Fail if there is a repetition.
      ++row_sizes[element];
      previous_element = element;
    }
  }
  for (const ElementIndex element : ElementRange()) {
    rows[element].reserve(RowEntryIndex(row_sizes[element]));
  }
  for (SubsetIndex subset = begin_subset; subset < end_subset; ++subset) {
    for (const ElementIndex element : columns_[subset]) {
      rows[element].emplace_back(subset);
    }
  }
  return rows;
}

std::vector<SparseRowView> SetCoverModel::CutSparseRowViewInSlices(
    absl::Span<const SubsetIndex> partition_points) {
  std::vector<SparseRowView> row_views;
  row_views.reserve(partition_points.size());
  SubsetIndex begin_subset(0);
  // This should be done in parallel. It is a bottleneck.
  for (SubsetIndex end_subset : partition_points) {
    row_views.push_back(ComputeSparseRowViewSlice(begin_subset, end_subset));
    begin_subset = end_subset;
  }
  return row_views;
}

SparseRowView SetCoverModel::ReduceSparseRowViewSlices(
    absl::Span<const SparseRowView> slices) {
  SparseRowView result_rows;
  // This is not a ReduceTree. This will be done later through parallelism.
  result_rows.reserve(num_elements_);
  ElementToIntVector row_sizes(num_elements_, 0);
  for (const SparseRowView& slice : slices) {
    // This should done as a reduce tree, in parallel.
    for (const ElementIndex element : ElementRange()) {
      result_rows[element].insert(result_rows[element].end(),
                                  slice[element].begin(), slice[element].end());
    }
  }
  return result_rows;
}

SparseRowView SetCoverModel::ComputeSparseRowViewUsingSlices() {
  StopWatch stop_watch(&compute_sparse_row_view_using_slices_duration_);
  SparseRowView rows;
  VLOG(1) << "CreateSparseRowViewUsingSlices started";
  const std::vector<SubsetIndex> partition_points =
      ComputeSliceIndices(num_subsets());
  const std::vector<SparseRowView> slices =
      CutSparseRowViewInSlices(partition_points);
  rows = ReduceSparseRowViewSlices(slices);
  VLOG(1) << "CreateSparseRowViewUsingSlices finished";
  return rows;
}

bool SetCoverModel::ComputeFeasibility() {
  StopWatch stop_watch(&feasibility_duration_);
  CHECK_GT(num_elements(), 0);
  CHECK_GT(num_subsets(), 0);
  CHECK_EQ(columns_.size(), num_subsets());
  CHECK_EQ(subset_costs_.size(), num_subsets());
  CHECK_EQ(all_subsets_.size(), num_subsets());
  for (const Cost cost : subset_costs_) {
    // TODO(user): We can preprocess subsets with cost 0, for performance.
    CHECK_GE(cost, 0.0);
  }
  ElementToIntVector possible_coverage(num_elements_, 0);
  SubsetIndex column_index(0);
  for (const SparseColumn& column : columns_) {
    DLOG_IF(INFO, column.empty()) << "Empty column " << column_index.value();
    for (const ElementIndex element : column) {
      ++possible_coverage[element];
    }
    // NOMUTANTS -- column_index is only used for logging in debug mode.
    ++column_index;
  }
  int64_t num_uncoverable_elements = 0;
  for (const ElementIndex element : ElementRange()) {
    // NOMUTANTS -- num_uncoverable_elements is only used for logging.
    if (possible_coverage[element] == 0) {
      ++num_uncoverable_elements;
    }
  }
  VLOG(1) << "num_uncoverable_elements = " << num_uncoverable_elements;
  for (const ElementIndex element : ElementRange()) {
    if (possible_coverage[element] > 0) continue;
    LOG(ERROR) << "Element " << element << " is not covered.";
    return false;
  }
  VLOG(1) << "Max possible coverage = "
          << *std::max_element(possible_coverage.begin(),
                               possible_coverage.end());
  for (const SubsetIndex subset : SubsetRange()) {
    if (all_subsets_[subset.value()] == subset) continue;
    LOG(ERROR) << "subset = " << subset << " all_subsets_[subset.value()] = "
               << all_subsets_[subset.value()];
    return false;
  }
  return true;
}

SetCoverProto SetCoverModel::ExportModelAsProto() const {
  CHECK(elements_in_columns_are_sorted_);
  SetCoverProto message;
  for (const SubsetIndex subset : SubsetRange()) {
    VLOG_EVERY_N_SEC(1, 5) << absl::StrFormat(
        "Exporting subset %d (%.1f%%)", subset.value(),
        100.0 * subset.value() / num_subsets());
    SetCoverProto::Subset* subset_proto = message.add_subset();
    subset_proto->set_cost(subset_costs_[subset]);
    SparseColumn column = columns_[subset];  // Copy is intentional.
    RadixSort(column);
    for (const ElementIndex element : column) {
      subset_proto->add_element(element.value());
    }
  }
  VLOG(1) << "Finished exporting the model.";
  return message;
}

void SetCoverModel::ImportModelFromProto(const SetCoverProto& message) {
  columns_.clear();
  subset_costs_.clear();
  num_nonzeros_ = 0;
  ResizeNumSubsets(message.subset_size());
  SubsetIndex subset_index(0);
  for (const SetCoverProto::Subset& subset_proto : message.subset()) {
    subset_costs_[subset_index] = subset_proto.cost();
    if (subset_proto.element_size() > 0) {
      columns_[subset_index].reserve(
          ColumnEntryIndex(subset_proto.element_size()));
      for (const BaseInt element : subset_proto.element()) {
        columns_[subset_index].push_back(ElementIndex(element));
        num_elements_ = std::max(num_elements_, element + 1);
      }
      num_nonzeros_ += subset_proto.element_size();
      ++subset_index;
    }
  }
  UpdateAllSubsetsList();
  CreateSparseRowView();
}

std::string SetCoverModel::ToVerboseString(absl::string_view sep) const {
  return absl::StrJoin(
      std::make_tuple("num_elements", num_elements(), "num_subsets",
                      num_subsets(), "num_nonzeros", num_nonzeros(),
                      "fill_rate", FillRate()),
      sep);
}

std::string SetCoverModel::ToString(absl::string_view sep) const {
  return absl::StrJoin(std::make_tuple(num_elements(), num_subsets(),
                                       num_nonzeros(), FillRate()),
                       sep);
}

namespace {
// Returns the standard deviation of the vector v, excluding those values that
// are zero.
template <typename T>
double StandardDeviation(const std::vector<T>& values) {
  const size_t size = values.size();
  double n = 0.0;  // n is used in a calculation involving doubles.
  double sum_of_squares = 0.0;
  double sum = 0.0;
  for (size_t i = 0; i < size; ++i) {
    double sample = static_cast<double>(values[i]);
    if (sample == 0.0) continue;
    sum_of_squares += sample * sample;
    sum += sample;
    ++n;
  }
  // Since we know all the values, we can compute the standard deviation
  // exactly.
  return n == 0.0 ? 0.0 : sqrt((sum_of_squares - sum * sum / n) / n);
}

// Statistics accumulation class used to compute statistics on the deltas of
// the row and column elements and their sizes in bytes.
// Since the values are not all stored, it's not possible to compute the median
// exactly. It is returned as 0.0. NaN would be a better choice, but it's just
// not a good idea as NaNs can propagate and cause problems.
class StatsAccumulator {
 public:
  StatsAccumulator()
      : count_(0),
        min_(kInfinity),
        max_(-kInfinity),
        sum_(0.0),
        sum_of_squares_(0.0) {}

  void Register(double value) {
    ++count_;
    min_ = std::min(min_, value);
    max_ = std::max(max_, value);
    sum_ += value;
    sum_of_squares_ += value * value;
  }

  SetCoverModel::Stats ComputeStats() const {
    const int64_t n = count_;
    // Since the code is used on a known number of values, we can compute the
    // standard deviation exactly, even if the values are not all stored.
    const double stddev =
        n == 0 ? 0.0 : sqrt((sum_of_squares_ - sum_ * sum_ / n) / n);
    return SetCoverModel::Stats{min_, max_, 0.0, sum_ / n, stddev};
  }

 private:
  static constexpr double kInfinity = std::numeric_limits<double>::infinity();
  int64_t count_;
  double min_;
  double max_;
  double sum_;
  double sum_of_squares_;
};
}  // namespace

template <typename T>
SetCoverModel::Stats ComputeStats(std::vector<T> samples) {
  SetCoverModel::Stats stats;
  stats.min = *std::min_element(samples.begin(), samples.end());
  stats.max = *std::max_element(samples.begin(), samples.end());
  stats.mean =
      std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
  auto const q1 = samples.size() / 4;
  auto const q2 = samples.size() / 2;
  auto const q3 = q1 + q2;
  // The first call to nth_element is O(n). The 2nd and 3rd calls are O(n / 2).
  // Basically it's equivalent to running nth_element twice.
  // One should be tempted to use a faster sorting algorithm like radix sort,
  // it is not sure that we would gain a lot. There would be no gain in
  // complexity whatsoever anyway. On top of that, this code is called at most
  // one per problem, so it's not worth the effort.
  std::nth_element(samples.begin(), samples.begin() + q2, samples.end());
  std::nth_element(samples.begin(), samples.begin() + q1, samples.begin() + q2);
  std::nth_element(samples.begin() + q2, samples.begin() + q3, samples.end());
  if ((samples.size() & 1) == 1) {  // Odd number of samples.
    stats.median = samples[q2];
  } else {
    const double first_half_largest =
        *std::max_element(samples.begin(), samples.begin() + q2);
    stats.median = (samples[q2] + first_half_largest) / 2;
  }
  stats.iqr = samples[q3] - samples[q1];
  stats.stddev = StandardDeviation(samples);
  return stats;
}

template <typename T>
std::vector<T> ComputeDeciles(std::vector<T> values) {
  const int kNumDeciles = 9;  // Yes, there are 9 deciles, not 10.
  std::vector<T> deciles(kNumDeciles, T{0});
  const size_t size = values.size();
  const double step = size / kNumDeciles;
  for (int i = 0; i < kNumDeciles; ++i) {
    const size_t point = std::clamp<double>((i + 1) * step, 0, size - 1);
    std::nth_element(values.begin(), values.begin() + point, values.end());
    deciles[i] = values[point];
  }
  return deciles;
}

SetCoverModel::Stats SetCoverModel::ComputeCostStats() const {
  std::vector<Cost> subset_costs(num_subsets());
  std::copy(subset_costs_.begin(), subset_costs_.end(), subset_costs.begin());
  return ComputeStats(std::move(subset_costs));
}

SetCoverModel::Stats SetCoverModel::ComputeRowStats() const {
  std::vector<int64_t> row_sizes(num_elements(), 0);
  for (const SparseColumn& column : columns_) {
    for (const ElementIndex element : column) {
      ++row_sizes[element.value()];
    }
  }
  return ComputeStats(std::move(row_sizes));
}

SetCoverModel::Stats SetCoverModel::ComputeColumnStats() const {
  std::vector<int64_t> column_sizes(columns_.size());
  for (const SubsetIndex subset : SubsetRange()) {
    column_sizes[subset.value()] = columns_[subset].size();
  }
  return ComputeStats(std::move(column_sizes));
}

std::string SetCoverModel::Stats::ToString(absl::string_view sep) const {
  return absl::StrJoin(std::make_tuple(min, max, median, mean, stddev, iqr),
                       sep);
}

std::string SetCoverModel::Stats::ToVerboseString(absl::string_view sep) const {
  return absl::StrJoin(
      std::make_tuple("min", min, "max", max, "median", median, "mean", mean,
                      "stddev", stddev, "iqr", iqr),
      sep);
}

std::vector<int64_t> SetCoverModel::ComputeRowDeciles() const {
  std::vector<int64_t> row_sizes(num_elements(), 0);
  for (const SparseColumn& column : columns_) {
    for (const ElementIndex element : column) {
      ++row_sizes[element.value()];
    }
  }
  return ComputeDeciles(std::move(row_sizes));
}

std::vector<int64_t> SetCoverModel::ComputeColumnDeciles() const {
  std::vector<int64_t> column_sizes(num_subsets());
  for (const SubsetIndex subset : SubsetRange()) {
    column_sizes[subset.value()] = columns_[subset].size();
  }
  return ComputeDeciles(std::move(column_sizes));
}

double SetCoverModel::ComputeRowCompressionRatio() const {
  int64_t total_compressed_rows_size = 0;
  for (const CompressedRow& row : compressed_rows_) {
    total_compressed_rows_size += row.size_in_bytes();
  }
  return static_cast<double>(num_nonzeros()) * sizeof(SubsetIndex) /
         total_compressed_rows_size;
}

double SetCoverModel::ComputeColumnCompressionRatio() const {
  int64_t total_compressed_columns_size = 0;
  for (const CompressedColumn& column : compressed_columns_) {
    total_compressed_columns_size += column.size_in_bytes();
  }
  return static_cast<double>(num_nonzeros()) * sizeof(ElementIndex) /
         total_compressed_columns_size;
}

}  // namespace operations_research
