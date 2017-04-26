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

#include "ortools/lp_data/sparse_column.h"
#include <algorithm>

#include "ortools/base/stringprintf.h"
#include "ortools/lp_data/lp_types.h"

namespace operations_research {
namespace glop {

// --------------------------------------------------------
// RandomAccessSparseColumn
// --------------------------------------------------------
RandomAccessSparseColumn::RandomAccessSparseColumn(RowIndex num_rows)
    : column_(num_rows, 0.0), changed_(num_rows, false), row_change_() {}

RandomAccessSparseColumn::~RandomAccessSparseColumn() {}

void RandomAccessSparseColumn::Clear() {
  const size_t num_changes = row_change_.size();
  for (int i = 0; i < num_changes; ++i) {
    const RowIndex row = row_change_[i];
    column_[row] = Fractional(0.0);
    changed_[row] = false;
  }
  row_change_.clear();
}

void RandomAccessSparseColumn::Resize(RowIndex num_rows) {
  if (num_rows <= column_.size()) {
    return;
  }
  column_.resize(num_rows, 0.0);
  changed_.resize(num_rows, false);
}

void RandomAccessSparseColumn::PopulateFromSparseColumn(
    const SparseColumn& sparse_column) {
  Clear();
  for (const SparseColumn::Entry e : sparse_column) {
    SetCoefficient(e.row(), e.coefficient());
  }
}

void RandomAccessSparseColumn::PopulateSparseColumn(SparseColumn* sparse_column)
    const {
  RETURN_IF_NULL(sparse_column);

  sparse_column->Clear();
  const size_t num_changes = row_change_.size();
  for (int change_id = 0; change_id < num_changes; ++change_id) {
    const RowIndex row = row_change_[change_id];
    const Fractional value = column_[row];

    // TODO(user): Do that only if (value != 0.0) ?
    sparse_column->SetCoefficient(row, value);
  }

  DCHECK(sparse_column->CheckNoDuplicates());
}

}  // namespace glop
}  // namespace operations_research
