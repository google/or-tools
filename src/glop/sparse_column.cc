#include <algorithm>

#include "base/stringprintf.h"
#include "glop/lp_types.h"
#include "glop/sparse_column.h"
#include "glop/status.h"

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
