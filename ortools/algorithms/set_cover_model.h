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

#ifndef OR_TOOLS_ALGORITHMS_SET_COVER_MODEL_H_
#define OR_TOOLS_ALGORITHMS_SET_COVER_MODEL_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "ortools/algorithms/set_cover.pb.h"
#include "ortools/base/strong_int.h"
#include "ortools/base/strong_vector.h"

// Representation class for the weighted set-covering problem.
//
// Let E be a "universe" set, let (S_j) be a family (j in J) of subsets of E,
// and c_j costs associated to each S_j. Note that J = {j in 1..|S|}.
//
// The minimum-cost set-covering problem consists in finding K (for covering),
// a subset of J such that the union of all the S_j for k in K is equal to E
// (the subsets indexed by K "cover" E), while minimizing total cost sum c_k (k
// in K).
//
// In Mixed-Integer Programming and matrix terms, the goal is to find values
// of binary variables x_j, where x_j is 1 when subset S_j is in K, 0
// otherwise, that minimize the sum of c_j * x_j subject to M.x >= 1. Each row
// corresponds to an element in E.
//
// The matrix M for linear constraints is defined as follows:
// - it has as many rows as there are elements in E.
// - its columns are such that M(i, j) = 1 iff the i-th element of E is present
//   in S_j.
//
// We also use m to denote |E|, the number of elements, and n to denote |S|, the
// number of subsets.
// Finally, NNZ denotes the numbers of non-zeros, i.e. the sum of the
// cardinalities of all the subsets.

namespace operations_research {
// Basic non-strict type for cost. The speed penalty for using double is ~2%.
using Cost = double;

// Base non-strict integer type for counting elements and subsets.
// Using ints makes it possible to represent problems with more than 2 billion
// (2e9) elements and subsets. If need arises one day, BaseInt can be split
// into SubsetBaseInt and ElementBaseInt.
// Quick testing has shown a slowdown of about 20-25% when using int64_t.
using BaseInt = int32_t;

// We make heavy use of strong typing to avoid obvious mistakes.
// Subset index.
DEFINE_STRONG_INT_TYPE(SubsetIndex, BaseInt);

// Element index.
DEFINE_STRONG_INT_TYPE(ElementIndex, BaseInt);

// Position in a vector. The vector may either represent a column, i.e. a
// subset with all its elements, or a row, i,e. the list of subsets which
// contain a given element.
DEFINE_STRONG_INT_TYPE(ColumnEntryIndex, BaseInt);
DEFINE_STRONG_INT_TYPE(RowEntryIndex, BaseInt);

using SubsetRange = util_intops::StrongIntRange<SubsetIndex>;
using ElementRange = util_intops::StrongIntRange<ElementIndex>;
using ColumnEntryRange = util_intops::StrongIntRange<ColumnEntryIndex>;

using SubsetCostVector = util_intops::StrongVector<SubsetIndex, Cost>;
using ElementCostVector = util_intops::StrongVector<ElementIndex, Cost>;

using SparseColumn = util_intops::StrongVector<ColumnEntryIndex, ElementIndex>;
using SparseRow = util_intops::StrongVector<RowEntryIndex, SubsetIndex>;

using ElementToIntVector = util_intops::StrongVector<ElementIndex, BaseInt>;
using SubsetToIntVector = util_intops::StrongVector<SubsetIndex, BaseInt>;

// Views of the sparse vectors. These need not be aligned as it's their contents
// that need to be aligned.
using SparseColumnView = util_intops::StrongVector<SubsetIndex, SparseColumn>;
using SparseRowView = util_intops::StrongVector<ElementIndex, SparseRow>;

using SubsetBoolVector = util_intops::StrongVector<SubsetIndex, bool>;
using ElementBoolVector = util_intops::StrongVector<ElementIndex, bool>;

// Useful for representing permutations,
using ElementToElementVector =
    util_intops::StrongVector<ElementIndex, ElementIndex>;
using SubsetToSubsetVector =
    util_intops::StrongVector<SubsetIndex, SubsetIndex>;

// Main class for describing a weighted set-covering problem.
class SetCoverModel {
 public:
  // Constructs an empty weighted set-covering problem.
  SetCoverModel()
      : num_elements_(0),
        num_subsets_(0),
        num_nonzeros_(0),
        row_view_is_valid_(false),
        elements_in_subsets_are_sorted_(false),
        subset_costs_(),
        columns_(),
        rows_(),
        all_subsets_() {}

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
  bool IsEmpty() const { return rows_.empty() || columns_.empty(); }

  // Current number of elements to be covered in the model, i.e. the number of
  // elements in S. In matrix terms, this is the number of rows.
  BaseInt num_elements() const { return num_elements_; }

  // Current number of subsets in the model. In matrix terms, this is the
  // number of columns.
  BaseInt num_subsets() const { return num_subsets_; }

  // Current number of nonzeros in the matrix.
  int64_t num_nonzeros() const { return num_nonzeros_; }

  double FillRate() const {
    return 1.0 * num_nonzeros() / (1.0 * num_elements() * num_subsets());
  }

  // Vector of costs for each subset.
  const SubsetCostVector& subset_costs() const { return subset_costs_; }

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

  // Returns true if the problem is feasible, i.e. if the subsets cover all
  // the elements.
  bool ComputeFeasibility() const;

  // Reserves num_subsets columns in the model.
  void ReserveNumSubsets(BaseInt num_subsets);
  void ReserveNumSubsets(SubsetIndex num_subsets);

  // Reserves num_elements rows in the column indexed by subset.
  void ReserveNumElementsInSubset(BaseInt num_elements, BaseInt subset);
  void ReserveNumElementsInSubset(ElementIndex num_elements,
                                  SubsetIndex subset);

  // Returns the model as a SetCoverProto. Note that the elements of each subset
  // are sorted locally before being exported to the proto. This is done to
  // ensure that the proto is deterministic. The function is const because it
  // does not modify the model. Therefore, the model as exported by this
  // function may be different from the initial model.
  SetCoverProto ExportModelAsProto() const;

  // Imports the model from a SetCoverProto.
  void ImportModelFromProto(const SetCoverProto& message);

  // A struct enabling to show basic statistics on rows and columns.
  // The meaning of the fields is obvious.
  struct Stats {
    double min;
    double max;
    double median;
    double mean;
    double stddev;

    std::string DebugString() const {
      return absl::StrCat("min = ", min, ", max = ", max, ", mean = ", mean,
                          ", median = ", median, ", stddev = ", stddev, ", ");
    }
  };

  // Computes basic statistics on costs and returns a Stats structure.
  Stats ComputeCostStats();

  // Computes basic statistics on rows and returns a Stats structure.
  Stats ComputeRowStats();

  // Computes basic statistics on columns and returns a Stats structure.
  Stats ComputeColumnStats();

  // Computes deciles on rows and returns a vector of deciles.
  std::vector<int64_t> ComputeRowDeciles() const;

  // Computes deciles on columns and returns a vector of deciles.
  std::vector<int64_t> ComputeColumnDeciles() const;

  // Computes basic statistics on the deltas of the row and column elements and
  // returns a Stats structure. The deltas are computed as the difference
  // between two consecutive indices in rows or columns. The number of bytes
  // computed is meant using a variable-length base-128 encoding.
  // TODO(user): actually use this to compress the rows and columns.
  Stats ComputeRowDeltaSizeStats() const;
  Stats ComputeColumnDeltaSizeStats() const;

 private:
  // Updates the all_subsets_ vector so that it always contains 0 to
  // columns.size() - 1
  void UpdateAllSubsetsList();

  // Number of elements.
  BaseInt num_elements_;

  // Number of subsets. Maintained for ease of access.
  BaseInt num_subsets_;

  // Number of nonzeros in the matrix. The value is an int64_t because there can
  // be more than 1 << 31 nonzeros even with BaseInt = int32_t.
  int64_t num_nonzeros_;

  // True when the SparseRowView is up-to-date.
  bool row_view_is_valid_;

  // True when the elements in each subset are sorted.
  bool elements_in_subsets_are_sorted_;

  // Costs for each subset.

  SubsetCostVector subset_costs_;

  // Vector of columns. Each column corresponds to a subset and contains the
  // elements of the given subset.
  // This takes NNZ (number of non-zeros) BaseInts, or |E| * |S| * fill_rate.
  // On classical benchmarks, the fill rate is in the 2 to 5% range.
  // Some synthetic benchmarks have fill rates of 20%, while benchmarks for
  // rail rotations have a fill rate of 0.2 to 0.4%.
  // TODO(user): try using a compressed representation like VarInt or LEB128,
  // since the data is only iterated upon.
  SparseColumnView columns_;

  // Vector of rows. Each row corresponds to an element and contains the
  // subsets containing the element.
  // The size is exactly the same as for columns_.
  SparseRowView rows_;

  // Vector of indices from 0 to columns.size() - 1. (Like std::iota, but built
  // incrementally.) Used to (un)focus optimization algorithms on the complete
  // problem.
  // This takes |S| BaseInts.
  // TODO(user): use this to enable deletion and recycling of columns/subsets.
  // TODO(user): replace this with an iterator?
  std::vector<SubsetIndex> all_subsets_;
};

// The IntersectingSubsetsIterator is a forward iterator that returns the next
// intersecting subset for a fixed seed_subset.
// The iterator is initialized with a model and a seed_subset and
// allows a speedup in getting the intersecting subsets
// by not storing them in memory.
// The iterator is at the end when the last intersecting subset has been
// returned.
// TODO(user): Add the possibility for range-for loops.
class IntersectingSubsetsIterator {
 public:
  IntersectingSubsetsIterator(const SetCoverModel& model,
                              SubsetIndex seed_subset)
      : intersecting_subset_(-1),
        element_entry_(0),
        subset_entry_(0),
        seed_subset_(seed_subset),
        model_(model),
        subset_seen_(model_.columns().size(), false) {
    CHECK(model_.row_view_is_valid());
    subset_seen_[seed_subset] = true;  // Avoid iterating on `seed_subset`.
    ++(*this);                         // Move to the first intersecting subset.
  }

  // Returns (true) whether the iterator is at the end.
  bool at_end() const {
    return element_entry_.value() == model_.columns()[seed_subset_].size();
  }

  // Returns the intersecting subset.
  SubsetIndex operator*() const { return intersecting_subset_; }

  // Move the iterator to the next intersecting subset.
  IntersectingSubsetsIterator& operator++() {
    DCHECK(model_.row_view_is_valid());
    DCHECK(!at_end());
    const SparseRowView& rows = model_.rows();
    const SparseColumn& column = model_.columns()[seed_subset_];
    for (; element_entry_ < ColumnEntryIndex(column.size()); ++element_entry_) {
      const ElementIndex current_element = column[element_entry_];
      const SparseRow& current_row = rows[current_element];
      for (; subset_entry_ < RowEntryIndex(current_row.size());
           ++subset_entry_) {
        intersecting_subset_ = current_row[subset_entry_];
        if (!subset_seen_[intersecting_subset_]) {
          subset_seen_[intersecting_subset_] = true;
          return *this;
        }
      }
      subset_entry_ = RowEntryIndex(0);  // 'carriage-return'
    }
    return *this;
  }

 private:
  // The intersecting subset.
  SubsetIndex intersecting_subset_;

  // The position of the entry in the column corresponding to `seed_subset_`.
  ColumnEntryIndex element_entry_;

  // The position of the entry in the row corresponding to `element_entry`.
  RowEntryIndex subset_entry_;

  // The seed subset.
  SubsetIndex seed_subset_;

  // The model to which the iterator is applying.
  const SetCoverModel& model_;

  // A vector of Booleans indicating whether the current subset has been
  // already seen by the iterator.
  SubsetBoolVector subset_seen_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_SET_COVER_MODEL_H_
