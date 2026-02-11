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

#ifndef ORTOOLS_SET_COVER_SET_COVER_MODEL_H_
#define ORTOOLS_SET_COVER_SET_COVER_MODEL_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/strong_int.h"
#include "ortools/base/strong_vector.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover.pb.h"

// Representation class for the weighted set-covering problem.
//
// Let U = {1, ..., m} be a "universe" set, let (S_j) be a family (j in J)
// of subsets of U, and c_j costs associated to each S_j. Note that J = {j
// in 1..|S|}.
//
// The minimum-cost set-covering problem consists in finding K (for covering),
// a subset of J such that the union of all the S_j for k in K is equal to U
// (the subsets indexed by K "cover" U), while minimizing total cost sum c_k (k
// in K).
//
// In Mixed-Integer Programming and matrix terms, the goal is to find values
// of binary variables x_j, where x_j is 1 when subset S_j is in K, 0
// otherwise, that minimize the sum of c_j * x_j subject to M.x >= 1. Each row
// corresponds to an element in U.
//
// The matrix M for linear constraints is defined as follows:
// - it has as many rows as there are elements in U.
// - its columns are such that M(i, j) = 1 iff i in S_j.
//
// We also use m to denote |U|, the number of elements, and n to denote |S|, the
// number of subsets.
// Finally, nnz denotes the numbers of non-zeros, i.e. the sum of the
// cardinalities of all the subsets.
// We use nnz(S) = sum_{j in J} |S_j| and nnz(K) = sum_{k in K} |S_k|).

namespace operations_research {

// Main class for describing a weighted set-covering problem.
class SetCoverModel {
 public:
  // Constructs an empty weighted set-covering problem.
  explicit SetCoverModel(const absl::string_view name = "SetCoverModel")
      : name_(name),
        timestamp_(0),
        num_elements_(0),
        num_subsets_(0),
        num_nonzeros_(0),
        row_view_is_valid_(false),
        elements_in_columns_are_sorted_(false),
        subset_costs_(),
        is_unicost_(true),
        is_unicost_valid_(false),
        columns_(),
        rows_(),
        all_subsets_() {}

  std::string name() const { return name_; }

  int64_t timestamp() const { return timestamp_; }

  void SetName(const absl::string_view name) { name_ = name; }

  // Constructs a weighted set-covering problem from a seed model, with
  // num_elements elements and num_subsets subsets.
  // - The distributions of the degrees of the elements and the cardinalities of
  // the subsets are based on those of the seed model. They are scaled
  // affininely by row_scale and column_scale respectively.
  // - By affine scaling, we mean that the minimum value of the distribution is
  // not scaled, but the variation above this minimum value is.
  // - For a given subset with a given cardinality in the generated model, its
  // elements are sampled from the distribution of the degrees as computed
  // above.
  // - The costs of the subsets in the new model are sampled from the
  // distribution of the costs of the subsets in the seed model, scaled by
  // cost_scale.
  // IMPORTANT NOTICE: The algorithm may not succeed in generating a model
  // where all the elements can be covered. In that case, the model will be
  // empty.

  static SetCoverModel GenerateRandomModelFrom(const SetCoverModel& seed_model,
                                               BaseInt num_elements,
                                               BaseInt num_subsets,
                                               double row_scale,
                                               double column_scale,
                                               double cost_scale);

  // Returns true if the model is empty, i.e. has no elements, no subsets, and
  // no nonzeros.
  bool IsEmpty() const { return columns_.empty(); }

  // Current number of elements to be covered in the model, i.e. the number of
  // elements in S. In matrix terms, this is the number of rows.
  BaseInt num_elements() const { return num_elements_; }

  // Current number of subsets in the model. In matrix terms, this is the
  // number of columns.
  BaseInt num_subsets() const {
    CHECK_NE(this, nullptr);
    return num_subsets_;
  }

  // Current number of nonzeros in the matrix.
  int64_t num_nonzeros() const { return num_nonzeros_; }

  // Returns the fill rate of the matrix.
  double FillRate() const {
    return 1.0 * num_nonzeros() / (1.0 * num_elements() * num_subsets());
  }

  // Computes the number of singleton columns in the model, i.e. subsets
  // covering only one element.
  BaseInt ComputeNumSingletonColumns() const {
    BaseInt num_singleton_columns = 0;
    for (const SparseColumn& column : columns_) {
      if (column.size() == 1) {
        ++num_singleton_columns;
      }
    }
    return num_singleton_columns;
  }

  // Computes the number of singleton rows in the model, i.e. elements in the
  // model that can be covered by one subset only.
  BaseInt ComputeNumSingletonRows() const {
    BaseInt num_singleton_rows = 0;
    for (const SparseRow& row : rows_) {
      if (row.size() == 1) {
        ++num_singleton_rows;
      }
    }
    return num_singleton_rows;
  }

  // Vector of costs for each subset.
  const SubsetCostVector& subset_costs() const { return subset_costs_; }

  void set_subset_costs(const SubsetCostVector& subset_costs) {
    subset_costs_ = subset_costs;
    is_unicost_valid_ = false;
    ++timestamp_;
  }

  // Returns true if all subset costs are equal to 1.0. This is a fast check
  // that is only valid if the subset costs are not modified.
  bool is_unicost() {
    if (is_unicost_valid_) {
      return is_unicost_;
    }
    is_unicost_ = true;
    for (const Cost cost : subset_costs_) {
      if (cost != 1.0) {
        is_unicost_ = false;
        break;
      }
    }
    is_unicost_valid_ = true;
    return is_unicost_;
  }

  // Column view of the set covering problem.
  const SparseColumnView& columns() const { return columns_; }

  // Row view of the set covering problem.
  const SparseRowView& rows() const {
    DCHECK(row_view_is_valid_);
    return rows_;
  }

  // Returns true if rows_ and columns_ represent the same problem.
  bool row_view_is_valid() const { return row_view_is_valid_; }

  // Access to the ranges of subsets and elements.
  util_intops::StrongIntRange<SubsetIndex> SubsetRange() const {
    return util_intops::StrongIntRange<SubsetIndex>(SubsetIndex(num_subsets_));
  }

  util_intops::StrongIntRange<ElementIndex> ElementRange() const {
    return util_intops::StrongIntRange<ElementIndex>(
        ElementIndex(num_elements_));
  }

  // Returns the list of indices for all the subsets in the model.
  std::vector<SubsetIndex> all_subsets() const { return all_subsets_; }

  // Accessors to the durations of the different stages of the model creation.
  absl::Duration generation_duration() const { return generation_duration_; }

  absl::Duration create_sparse_row_view_duration() const {
    return create_sparse_row_view_duration_;
  }

  absl::Duration compute_sparse_row_view_using_slices_duration() const {
    return compute_sparse_row_view_using_slices_duration_;
  }

  // Adds an empty subset with a cost to the problem. In matrix terms, this
  // adds a column to the matrix.
  void AddEmptySubset(Cost cost);

  // Adds an element to the last subset created. In matrix terms, this adds a
  // 1 on row 'element' of the current last column of the matrix.
  void AddElementToLastSubset(BaseInt element);
  void AddElementToLastSubset(ElementIndex element);

  // Sets 'cost' to an already existing 'subset'.
  // This will CHECK-fail if cost is infinite or a NaN.
  void SetSubsetCost(BaseInt subset, Cost cost);
  void SetSubsetCost(SubsetIndex subset, Cost cost);

  // Adds 'element' to an already existing 'subset'.
  // No check is done if element is already in the subset.
  void AddElementToSubset(BaseInt element, BaseInt subset);
  void AddElementToSubset(ElementIndex element, SubsetIndex subset);

  // Sorts the elements in each subset. Should be called before exporting the
  // model to a proto.
  void SortElementsInSubsets();

  // Creates the sparse ("dual") representation of the problem. This also sorts
  // the elements in each subset.
  void CreateSparseRowView();

  void CreateSparseColumnView();

  // Same as CreateSparseRowView, but uses a slicing algorithm, more prone to
  // parallelism.
  SparseRowView ComputeSparseRowViewUsingSlices();

  // The following functions are used by CreateSparseRowViewUsingSlices.
  // They are exposed for testing purposes.

  // Returns a vector of subset indices that partition columns into
  // `num_partitions` partitions of roughly equal size in number of non-zeros.
  // The resulting vector contains the index of the first subset in the next
  // partition. Therefore the last element is the total number of subsets.
  // Also the vector is sorted.
  std::vector<SubsetIndex> ComputeSliceIndices(int num_partitions);

  // Returns a view of the rows of the problem with subset in the range
  // [begin_subset, end_subset).
  SparseRowView ComputeSparseRowViewSlice(SubsetIndex begin_subset,
                                          SubsetIndex end_subset);

  // Returns a vector of row views, each corresponding to a partition of the
  // problem. The partitions are defined by the given partition points.
  std::vector<SparseRowView> CutSparseRowViewInSlices(
      absl::Span<const SubsetIndex> partition_points);

  // Returns the union of the rows of the given row views.
  // The returned view is valid only as long as the given row views are valid.
  // The indices in the rows are sorted.
  SparseRowView ReduceSparseRowViewSlices(
      absl::Span<const SparseRowView> row_slices);

  // Returns true if the problem is feasible, i.e. if the subsets cover all
  // the elements. Could be const, but it updates the feasibility_duration_
  // field.
  bool ComputeFeasibility();

  // Resizes the model to have at least num_subsets columns. The new columns
  // correspond to empty subsets.
  // This function will never remove a column, i.e. if num_subsets is smaller
  // than the current instance size the function does nothing.
  void ResizeNumSubsets(BaseInt num_subsets);
  void ResizeNumSubsets(SubsetIndex num_subsets) {
    ResizeNumSubsets(num_subsets.value());
  }

  // Resizes the model to have at least num_elements in rows_.
  void ResizeNumElements(BaseInt num_elements);
  void ResizeNumElements(ElementIndex num_elements) {
    ResizeNumElements(num_elements.value());
  }

  // Reserves num_elements rows in the column indexed by subset.
  void ReserveNumElementsInSubset(BaseInt num_elements, BaseInt subset);

  // Returns the model as a SetCoverProto. Note that the elements of each subset
  // are sorted locally before being exported to the proto. This is done to
  // ensure that the proto is deterministic. The function is const because it
  // does not modify the model. Therefore, the model as exported by this
  // function may be different from the initial model.
  SetCoverProto ExportModelAsProto() const;

  // Imports the model from a SetCoverProto.
  void ImportModelFromProto(const SetCoverProto& message);

  // Returns a verbose string representation of the model, using the given
  // separator.
  std::string ToVerboseString(absl::string_view sep) const;

  // Returns a string representation of the model, using the given separator.
  std::string ToString(absl::string_view sep) const;

  // A struct enabling to show basic statistics on rows and columns.
  // The meaning of the fields is obvious.
  struct Stats {
    double min;
    double max;
    double median;
    double mean;
    double stddev;
    double iqr;  // Interquartile range.

    // Returns a string representation of the stats. TODO(user): remove this as
    // it is obsolete.
    std::string DebugString() const { return ToVerboseString(", "); }

    // Returns a string representation of the stats, using the given separator.
    std::string ToString(absl::string_view sep) const;

    // Returns a verbose string representation of the stats, using the given
    // separator.
    std::string ToVerboseString(absl::string_view sep) const;
  };

  // Computes basic statistics on costs and returns a Stats structure.
  Stats ComputeCostStats() const;

  // Computes basic statistics on rows and returns a Stats structure.
  Stats ComputeRowStats() const;

  // Computes basic statistics on columns and returns a Stats structure.
  Stats ComputeColumnStats() const;

  // Computes deciles on rows and returns a vector of deciles.
  std::vector<int64_t> ComputeRowDeciles() const;

  // Computes deciles on columns and returns a vector of deciles.
  std::vector<int64_t> ComputeColumnDeciles() const;

 private:
  // Updates the all_subsets_ vector so that it always contains 0 to
  // columns.size() - 1
  void UpdateAllSubsetsList();

  // The name of the model, "SetCoverModel" as default.
  std::string name_;

  // Time stamp (i.e. "version number") of the model. Used to check if the model
  // has been modified since the last time it was used.
  int64_t timestamp_;

  // Number of elements.
  BaseInt num_elements_;

  // Number of subsets. Maintained for ease of access.
  BaseInt num_subsets_;

  // Number of nonzeros in the matrix. The value is an int64_t because there can
  // be more than 1 << 31 nonzeros even with BaseInt = int32_t.
  int64_t num_nonzeros_;

  // True when the SparseRowView is up-to-date.
  bool row_view_is_valid_;

  // True when the SparseColumnView is up-to-date.
  bool column_view_is_valid_;

  // True when the elements in each column are sorted.
  bool elements_in_columns_are_sorted_;

  // True when the subsets in each row are sorted.
  bool subsets_in_rows_are_sorted_;

  // Costs for each subset.
  SubsetCostVector subset_costs_;

  // True when all subset costs are equal to 1.0.
  bool is_unicost_;

  // True when is_unicost_ is up-to-date.
  bool is_unicost_valid_;

  // Stores the run time for CreateSparseRowView. Interesting to compare with
  // the time spent to actually generate a solution to the model.
  absl::Duration create_sparse_row_view_duration_;

  // Stores the run time for CreateSparseColumnView.
  absl::Duration create_sparse_column_view_duration_;

  // Stores the run time for ComputeSparseRowViewUsingSlices.
  absl::Duration compute_sparse_row_view_using_slices_duration_;

  // Stores the run time for GenerateRandomModelFrom.
  absl::Duration generation_duration_;

  // Stores the run time for ComputeFeasibility. A good estimate of the time
  // needed to traverse the complete model.
  absl::Duration feasibility_duration_;

  // Vector of columns. Each column corresponds to a subset and contains the
  // elements of the given subset.
  // This takes nnz(S) (number of non-zeros) BaseInts, or |U| * |S| * fill_rate.
  // On classical benchmarks, the fill rate is in the 2 to 5% range.
  // Some synthetic benchmarks have fill rates of 20%, while benchmarks for
  // rail rotations have a fill rate of 0.2 to 0.4%.
  SparseColumnView columns_;

  // Vector of rows. Each row corresponds to an element and contains the
  // subsets containing the element.
  // The size is exactly the same as for columns_ (or there would be a bug.)
  SparseRowView rows_;

  // Vector of indices from 0 to columns.size() - 1. (Like std::iota, but built
  // incrementally.) Used to (un)focus optimization algorithms on the complete
  // problem.
  // This takes |S| BaseInts.
  // TODO(user): use an IndexRange(0, columns_.size()) instead.
  std::vector<SubsetIndex> all_subsets_;
};

// IntersectingSubsetsIterator is a forward iterator that returns the next
// intersecting subset for a fixed seed_subset.
// The iterator is initialized with a model and a seed_subset and
// allows a speedup in getting the intersecting subsets
// by not storing them in memory.
// The iterator is at the end when the last intersecting subset has been
// returned.
class IntersectingSubsetsIterator {
 public:
  IntersectingSubsetsIterator(const SetCoverModel& model,
                              SubsetIndex seed_subset, bool at_end = false)
      : model_(model),
        seed_subset_(seed_subset),
        intersecting_subset_(0),  // meaningless, will be overwritten.
        new_row_(true),
        seed_column_(model_.columns()[seed_subset]),
        seed_column_it_(),
        rows_(model_.rows()),
        row_it_(),
        subset_seen_() {
    // For the iterator to be as light as possible when created, we do not
    // reserve space for the subset_seen_ vector, and we do not initialize it.
    // This is done to avoid the overhead of creating the vector and
    // initializing it when the iterator is created. The vector is created on
    // the first call to the ++ operator.
    DCHECK(model_.row_view_is_valid());
    if (at_end) {
      seed_column_it_ = seed_column_.end();  // set iterator to end
      return;
    }
    for (seed_column_it_ = seed_column_.begin();
         seed_column_it_ != seed_column_.end(); ++seed_column_it_) {
      const ElementIndex current_element = *seed_column_it_;
      const SparseRow& current_row = rows_[current_element];
      if (new_row_) {
        row_it_ = current_row.begin();
        new_row_ = false;
      }
      for (; row_it_ != current_row.end(); ++row_it_) {
        if (*row_it_ == seed_subset_) continue;
        intersecting_subset_ = *row_it_;
        return;
      }
      new_row_ = true;  // 'carriage-return'
    }
  }

  // Returns (true) whether the iterator is at the end.
  bool at_end() const { return seed_column_it_ == seed_column_.end(); }

  // Returns the intersecting subset.
  SubsetIndex operator*() const { return intersecting_subset_; }

  // Disequality operator.
  bool operator!=(const IntersectingSubsetsIterator& other) const {
    return seed_subset_ != other.seed_subset_ ||
           seed_column_it_ != other.seed_column_it_;
  }

  // Advances the iterator to the next intersecting subset.
  IntersectingSubsetsIterator& operator++() {
    DCHECK(!at_end());
    if (subset_seen_.empty()) {
      subset_seen_.resize(model_.num_subsets(), false);
      subset_seen_[seed_subset_] = true;
    }
    subset_seen_[intersecting_subset_] = true;
    for (; seed_column_it_ != seed_column_.end(); ++seed_column_it_) {
      const ElementIndex current_element = *seed_column_it_;
      const SparseRow& current_row = rows_[current_element];
      if (new_row_) {
        row_it_ = current_row.begin();
        new_row_ = false;
      }
      for (; row_it_ < current_row.end(); ++row_it_) {
        intersecting_subset_ = *row_it_;
        if (!subset_seen_[intersecting_subset_]) {
          return *this;
        }
      }
      new_row_ = true;  // 'carriage-return'
    }
    return *this;
  }

 private:
  // The model to which the iterator is applying.
  const SetCoverModel& model_;

  // The seed subset.
  SubsetIndex seed_subset_;

  // The intersecting subset.
  SubsetIndex intersecting_subset_;

  // True if the iterator is at the beginning of a new lrow. This is used to
  // recover the exact position of the iterator in the row when calling
  // operator++().
  bool new_row_;

  // A reference to the column of the seed subset, kept here for ease of access.
  const SparseColumn& seed_column_;

  // Iterator on the column of the seed subset.
  SparseColumnConstIterator seed_column_it_;

  // A reference to the rows of the model, kept here for ease of access.
  const SparseRowView& rows_;

  // Iterator on the current row.
  SparseRowConstIterator row_it_;

  // A vector of Booleans indicating whether the current subset has been
  // already seen by the iterator.
  SubsetBoolVector subset_seen_;
};

// IntersectingSubsetsRange is a range of intersecting subsets for a fixed seed
// subset. Can be used with range-based for-loops.
class IntersectingSubsetsRange {
 public:
  using iterator = IntersectingSubsetsIterator;
  using const_iterator = IntersectingSubsetsIterator;

  IntersectingSubsetsRange(const SetCoverModel& model, SubsetIndex seed_subset)
      : model_(model), seed_subset_(seed_subset) {}

  iterator begin() { return IntersectingSubsetsIterator(model_, seed_subset_); }

  iterator end() {
    return IntersectingSubsetsIterator(model_, seed_subset_, true);
  }

  const_iterator begin() const {
    return IntersectingSubsetsIterator(model_, seed_subset_);
  }

  const_iterator end() const {
    return IntersectingSubsetsIterator(model_, seed_subset_, true);
  }

 private:
  // The model to which the range is applying.
  const SetCoverModel& model_;

  // The seed subset for which the intersecting subsets are computed.
  SubsetIndex seed_subset_;
};

}  // namespace operations_research

#endif  // ORTOOLS_SET_COVER_SET_COVER_MODEL_H_
