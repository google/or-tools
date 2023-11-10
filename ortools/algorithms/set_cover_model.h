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

#ifndef OR_TOOLS_ALGORITHMS_SET_COVER_MODEL_H_
#define OR_TOOLS_ALGORITHMS_SET_COVER_MODEL_H_

#include <vector>

#include "ortools/lp_data/lp_types.h"  // For StrictITIVector.

// Representation class for the weighted set-covering problem.
//
// Let S be a set, let (T_j) be a family (j in J) of subsets of S, and c_j costs
// associated to each T_j.
//
// The minimum-cost set-covering problem consists in finding K, a subset of J
// such that the union of all the T_j for k in K is equal to S (the subsets
// indexed by K "cover" S), with the minimal total cost sum c_k (k in K).
//
// In Mixed-Integer Programming and matrix terms, the goal is to find values
// of binary variables x_j, where x_j is 1 when subset T_j is in K, 0
// otherwise, that minimize the sum of c_j * x_j subject to M.x >= 1. Each row
// corresponds to an element in S.
//
// The matrix M for linear constraints is defined as follows:
// - it has as many rows as there are elements in S.
// - its columns are such that M(i, j) = 1 iff the i-th element of S is present
//   in T_j.

namespace operations_research {
// Basic non-strict type for cost.
typedef double Cost;

// We make heavy use of strong typing to avoid obvious mistakes.
// Subset index.
DEFINE_STRONG_INDEX_TYPE(SubsetIndex);

// Element index.
DEFINE_STRONG_INDEX_TYPE(ElementIndex);

// Position in a vector. The vector may either represent a column, i.e. a
// subset with all its elements, or a row, i,e. the list of subsets which
// contain a given element.
DEFINE_STRONG_INDEX_TYPE(EntryIndex);

// TODO(user): consider replacing with StrongVectors, which behave differently.
// The return type for size() is a simple size_t and not an Index as in
// StrictITIVector, which makes the code less elegant.
using SubsetCostVector = glop::StrictITIVector<SubsetIndex, Cost>;
using SparseColumn = glop::StrictITIVector<EntryIndex, ElementIndex>;
using SparseRow = glop::StrictITIVector<EntryIndex, SubsetIndex>;

using SparseColumnView = glop::StrictITIVector<SubsetIndex, SparseColumn>;
using SparseRowView = glop::StrictITIVector<ElementIndex, SparseRow>;
using ElementToSubsetVector = glop::StrictITIVector<ElementIndex, SubsetIndex>;
using SubsetToElementVector = glop::StrictITIVector<SubsetIndex, ElementIndex>;

// Main class for describing a weighted set-covering problem.
class SetCoverModel {
 public:
  // Constructs an empty weighted set-covering problem.
  SetCoverModel()
      : num_elements_(0),
        row_view_is_valid_(false),
        subset_costs_(),
        columns_(),
        rows_(),
        all_subsets_() {}

  // Current number of elements to be covered in the model, i.e. the number of
  // elements in S. In matrix terms, this is the number of rows.
  ElementIndex num_elements() const { return num_elements_; }

  // Current number of subsets in the model. In matrix terms, this is the
  // number of columns.
  SubsetIndex num_subsets() const { return columns_.size(); }

  const SubsetCostVector& subset_costs() { return subset_costs_; }

  const SparseColumnView& columns() { return columns_; }

  const SparseRowView& rows() {
    DCHECK(row_view_is_valid_);
    return rows_;
  }

  // Returns true if rows_ and columns_ represent the same problem.
  bool row_view_is_valid() const { return row_view_is_valid_; }

  // Returns the list of indices for the subsets in the model.
  std::vector<SubsetIndex> all_subsets() const { return all_subsets_; }

  // Adds an empty subset with a cost to the problem. In matrix terms, this
  // adds a column to the matrix.
  void AddEmptySubset(Cost cost);

  // Adds an element to the last subset created. In matrix terms, this adds a
  // 1 on row 'element' of the current last column of the matrix.
  void AddElementToLastSubset(int element);
  void AddElementToLastSubset(ElementIndex element);

  // Sets 'cost' to an already existing 'subset'.
  void SetSubsetCost(int subset, Cost cost);

  // Adds 'element' to and already existing 'subset'.
  void AddElementToSubset(int element, int subset);

  // Creates the sparse ("dual") representation of the problem.
  void CreateSparseRowView();

  // Returns true if the problem is feasible, i.e. if the subsets cover all
  // the elements.
  bool ComputeFeasibility() const;

  // Reserves num_subsets columns in the model.
  void ReserveNumSubsets(int num_subsets);

  // Reserves num_elements rows in the column indexed by subset.
  void ReserveNumElementsInSubset(int num_elements, int subset);

 private:
  // Updates the all_subsets_ vector so that it always contains 0 to
  // columns.size() - 1
  void UpdateAllSubsetsList();

  // Number of elements.
  ElementIndex num_elements_;

  // True when the SparseRowView is up-to-date.
  bool row_view_is_valid_;

  // Costs for each subset.
  SubsetCostVector subset_costs_;

  // Vector of columns. Each column corresponds to a subset and contains the
  // elements of the given subset.
  SparseColumnView columns_;

  // Vector of rows. Each row corresponds to an element and contains the
  // subsets containing the element.
  SparseRowView rows_;

  // Vector of indices from 0 to columns.size() - 1. (Like std::iota, but built
  // incrementally.) Used to (un)focus optimization algorithms on the complete
  // problem.
  // TODO(user): use this to enable deletion and recycling of columns/subsets.
  std::vector<SubsetIndex> all_subsets_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_SET_COVER_MODEL_H_
