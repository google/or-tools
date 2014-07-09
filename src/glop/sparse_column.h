// Copyright 2010-2014 Google
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
#ifndef OR_TOOLS_GLOP_SPARSE_COLUMN_H_
#define OR_TOOLS_GLOP_SPARSE_COLUMN_H_

#include "glop/sparse_vector.h"

namespace operations_research {
namespace glop {

// TODO(user): Consider using kInvalidRow for this?
const RowIndex kNonPivotal(-1);

// A SparseColumn is a SparseVector<RowIndex>, with a few methods renamed
// to help readability on the client side.
class SparseColumn : public SparseVector<RowIndex> {
 public:
  SparseColumn() : SparseVector<RowIndex>() {}
  // Use a separate API to get the row and coefficient of entry #i.
  RowIndex EntryRow(EntryIndex i) const { return entry(i).index; }
  Fractional EntryCoefficient(EntryIndex i) const {
    return entry(i).coefficient;
  }
  RowIndex GetFirstRow() const { return GetFirstIndex(); }
  RowIndex GetLastRow() const { return GetLastIndex(); }
  void ApplyRowPermutation(const RowPermutation& p) {
    ApplyIndexPermutation(p);
  }
  void ApplyPartialRowPermutation(const RowPermutation& p) {
    ApplyPartialIndexPermutation(p);
  }
};

// Specialization of the Entry API for SparseColumn, to gain the 'row' accessor.
template <>
class SparseVector<RowIndex>::Entry {
 public:
  RowIndex row() const { return sparse_column_.EntryRow(i_); }
  Fractional coefficient() const { return sparse_column_.EntryCoefficient(i_); }

 protected:
  Entry(const SparseVector<RowIndex>& sparse_vector, EntryIndex i)
      : sparse_column_(*static_cast<const SparseColumn*>(&sparse_vector)),
        i_(i) {}
  const SparseColumn& sparse_column_;
  EntryIndex i_;
};

// TODO(user): create SparseRow and use it where appropriate.

// --------------------------------------------------------
// RandomAccessSparseColumn
// --------------------------------------------------------
// A RandomAccessSparseColumn is a mix between a DenseColumn and a SparseColumn.
// It makes it possible to populate a dense column from a sparse column in
// O(num_entries) instead of O(num_rows), and to access an entry in O(1).
// As the constructor runs in O(num_rows), a RandomAccessSparseColumn should be
// used several times to amortize the creation cost.
class RandomAccessSparseColumn {
 public:
  // Creates a RandomAccessSparseColumn.
  // Runs in O(num_rows).
  explicit RandomAccessSparseColumn(RowIndex num_rows);
  virtual ~RandomAccessSparseColumn();

  // Clears the column.
  // Runs in O(num_entries).
  void Clear();

  void Resize(RowIndex num_rows);

  // Sets value at row.
  // Runs in O(1).
  void SetCoefficient(RowIndex row, Fractional value) {
    column_[row] = value;
    MarkRowAsChanged(row);
  }

  // Adds value to the current value at row.
  // Runs in O(1).
  void AddToCoefficient(RowIndex row, Fractional value) {
    column_[row] += value;
    MarkRowAsChanged(row);
  }

  // Populates from a sparse column.
  // Runs in O(num_entries).
  void PopulateFromSparseColumn(const SparseColumn& sparse_column);

  // Populates a sparse column from the lazy dense column.
  // Runs in O(num_entries).
  void PopulateSparseColumn(SparseColumn* sparse_column) const;

  // Returns the number of rows.
  // Runs in O(1).
  RowIndex GetNumberOfRows() const { return RowIndex(column_.size()); }

  // Returns the value in position row.
  // Runs in O(1).
  Fractional GetCoefficient(RowIndex row) const { return column_[row]; }

 private:
  // Keeps a trace of which rows have been changed.
  void MarkRowAsChanged(RowIndex row) {
    if (!changed_[row]) {
      changed_[row] = true;
      row_change_.push_back(row);
    }
  }

  // The dense version of the column.
  DenseColumn column_;

  // Dense Boolean vector used to mark changes.
  DenseBooleanColumn changed_;

  // Stack to store changes.
  std::vector<RowIndex> row_change_;

  DISALLOW_COPY_AND_ASSIGN(RandomAccessSparseColumn);
};

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_SPARSE_COLUMN_H_
