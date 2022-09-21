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

#ifndef OR_TOOLS_LP_DATA_SPARSE_ROW_H_
#define OR_TOOLS_LP_DATA_SPARSE_ROW_H_

#include "ortools/base/strong_vector.h"
#include "ortools/lp_data/sparse_vector.h"

namespace operations_research {
namespace glop {

// Specialization of SparseVectorEntry and SparseVectorIterator for the
// SparseRow class. In addition to index(), it also provides col() for better
// readability on the client side.
class SparseRowEntry : public SparseVectorEntry<ColIndex> {
 public:
  // Returns the row of the current entry.
  ColIndex col() const { return index(); }

 protected:
  SparseRowEntry(const ColIndex* indices, const Fractional* coefficients,
                 EntryIndex i)
      : SparseVectorEntry<ColIndex>(indices, coefficients, i) {}
};
using SparseRowIterator = VectorIterator<SparseRowEntry>;

// TODO(user): Use this class where appropriate, i.e. when a SparseColumn is
// used to store a row vector (by means of RowIndex to ColIndex casting).

// A SparseRow is a SparseVector<ColIndex>, with a few methods renamed
// to help readability on the client side.
class SparseRow : public SparseVector<ColIndex, SparseRowIterator> {
 public:
  SparseRow() : SparseVector<ColIndex, SparseRowIterator>() {}

  // Use a separate API to get the column and coefficient of entry #i.
  ColIndex EntryCol(EntryIndex i) const { return GetIndex(i); }
  Fractional EntryCoefficient(EntryIndex i) const { return GetCoefficient(i); }
  ColIndex GetFirstCol() const { return GetFirstIndex(); }
  ColIndex GetLastCol() const { return GetLastIndex(); }
  void ApplyColPermutation(const ColumnPermutation& p) {
    ApplyIndexPermutation(p);
  }
  void ApplyPartialColPermutation(const ColumnPermutation& p) {
    ApplyPartialIndexPermutation(p);
  }
};

// A matrix stored by rows.
typedef absl::StrongVector<RowIndex, SparseRow> RowMajorSparseMatrix;

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_LP_DATA_SPARSE_ROW_H_
