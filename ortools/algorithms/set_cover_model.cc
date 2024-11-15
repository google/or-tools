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
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/numeric/bits.h"
#include "absl/random/discrete_distribution.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "ortools/algorithms/set_cover.pb.h"
#include "ortools/base/logging.h"

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
  SetCoverModel model;
  DCHECK_GT(row_scale, 0.0);
  DCHECK_GT(column_scale, 0.0);
  DCHECK_GT(cost_scale, 0.0);
  model.num_elements_ = num_elements;
  model.num_nonzeros_ = 0;
  model.ReserveNumSubsets(num_subsets);
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

  // Loop-local vector indicating whether the currently generated subset
  // contains an element.
  ElementBoolVector subset_contains_element(num_elements, false);

  for (SubsetIndex subset(0); subset.value() < num_subsets; ++subset) {
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
        if (num_tries > 10) {
          return SetCoverModel();
        }
      } while (subset_contains_element[element]);
      ++model.num_nonzeros_;
      model.columns_[subset].push_back(element);
      subset_contains_element[element] = true;
      if (!contains_element[element]) {
        contains_element[element] = true;
        ++num_elements_covered;
      }
    }
    for (const ElementIndex element : model.columns_[subset]) {
      subset_contains_element[element] = false;
    }
  }
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
  if (subset >= num_subsets()) {
    num_subsets_ = std::max(num_subsets_, subset + 1);
    columns_.resize(num_subsets_, SparseColumn());
    subset_costs_.resize(num_subsets_, 0.0);
    row_view_is_valid_ = false;
    UpdateAllSubsetsList();
  }
  subset_costs_[SubsetIndex(subset)] = cost;
}

void SetCoverModel::SetSubsetCost(SubsetIndex subset, Cost cost) {
  SetSubsetCost(subset.value(), cost);
}

void SetCoverModel::AddElementToSubset(BaseInt element, BaseInt subset) {
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
}

void SetCoverModel::AddElementToSubset(ElementIndex element,
                                       SubsetIndex subset) {
  AddElementToSubset(element.value(), subset.value());
}

// Reserves num_subsets columns in the model.
void SetCoverModel::ReserveNumSubsets(BaseInt num_subsets) {
  num_subsets_ = std::max(num_subsets_, num_subsets);
  columns_.resize(num_subsets_, SparseColumn());
  subset_costs_.resize(num_subsets_, 0.0);
  UpdateAllSubsetsList();
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
  for (const SubsetIndex subset : SubsetRange()) {
    // Sort the columns. It's not super-critical to improve performance here
    // as this needs to be done only once.
    std::sort(columns_[subset].begin(), columns_[subset].end());
    for (const ElementIndex element : columns_[subset]) {
      ++row_sizes[element];
    }
  }
  for (const ElementIndex element : ElementRange()) {
    rows_[element].reserve(RowEntryIndex(row_sizes[element]));
  }
  for (const SubsetIndex subset : SubsetRange()) {
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
      for (const BaseInt element : subset_proto.element()) {
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
    const BaseInt n = count_;
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

template <typename T>
std::vector<T> ComputeDeciles(std::vector<T> values) {
  const int kNumDeciles = 10;
  std::vector<T> deciles;
  deciles.reserve(kNumDeciles);
  for (int i = 1; i <= kNumDeciles; ++i) {
    const size_t point = values.size() * i / kNumDeciles - 1;
    std::nth_element(values.begin(), values.begin() + point, values.end());
    deciles.push_back(values[point]);
  }
  return deciles;
}

SetCoverModel::Stats SetCoverModel::ComputeCostStats() {
  std::vector<Cost> subset_costs(num_subsets());
  std::copy(subset_costs_.begin(), subset_costs_.end(), subset_costs.begin());
  return ComputeStats(std::move(subset_costs));
}

SetCoverModel::Stats SetCoverModel::ComputeRowStats() {
  std::vector<BaseInt> row_sizes(num_elements(), 0);
  for (const SparseColumn& column : columns_) {
    for (const ElementIndex element : column) {
      ++row_sizes[element.value()];
    }
  }
  return ComputeStats(std::move(row_sizes));
}

SetCoverModel::Stats SetCoverModel::ComputeColumnStats() {
  std::vector<BaseInt> column_sizes(columns_.size());
  for (const SubsetIndex subset : SubsetRange()) {
    column_sizes[subset.value()] = columns_[subset].size();
  }
  return ComputeStats(std::move(column_sizes));
}

std::vector<BaseInt> SetCoverModel::ComputeRowDeciles() const {
  std::vector<BaseInt> row_sizes(num_elements(), 0);
  for (const SparseColumn& column : columns_) {
    for (const ElementIndex element : column) {
      ++row_sizes[element.value()];
    }
  }
  return ComputeDeciles(std::move(row_sizes));
}

std::vector<BaseInt> SetCoverModel::ComputeColumnDeciles() const {
  std::vector<BaseInt> column_sizes(columns_.size());
  for (const SubsetIndex subset : SubsetRange()) {
    column_sizes[subset.value()] = columns_[subset].size();
  }
  return ComputeDeciles(std::move(column_sizes));
}

namespace {
// Returns the number of bytes needed to store x with a base-128 encoding.
BaseInt Base128SizeInBytes(BaseInt x) {
  const uint64_t u = x == 0 ? 1 : static_cast<uint64_t>(x);
  return (64 - absl::countl_zero(u) + 6) / 7;
}
}  // namespace

SetCoverModel::Stats SetCoverModel::ComputeColumnDeltaSizeStats() const {
  StatsAccumulator acc;
  for (const SparseColumn& column : columns_) {
    BaseInt previous = 0;
    for (const ElementIndex element : column) {
      const BaseInt delta = element.value() - previous;
      previous = element.value();
      acc.Register(Base128SizeInBytes(delta));
    }
  }
  return acc.ComputeStats();
}

SetCoverModel::Stats SetCoverModel::ComputeRowDeltaSizeStats() const {
  StatsAccumulator acc;
  for (const SparseRow& row : rows_) {
    BaseInt previous = 0;
    for (const SubsetIndex subset : row) {
      const BaseInt delta = subset.value() - previous;
      previous = subset.value();
      acc.Register(Base128SizeInBytes(delta));
    }
  }
  return acc.ComputeStats();
}

}  // namespace operations_research
