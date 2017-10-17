// Copyright 2010-2017 Google
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

#include "ortools/glop/markowitz.h"

#include <limits>
#include "ortools/base/stringprintf.h"
#include "ortools/lp_data/lp_utils.h"

namespace operations_research {
namespace glop {

Status Markowitz::ComputeRowAndColumnPermutation(const MatrixView& basis_matrix,
                                                 RowPermutation* row_perm,
                                                 ColumnPermutation* col_perm) {
  SCOPED_TIME_STAT(&stats_);
  Clear();
  const RowIndex num_rows = basis_matrix.num_rows();
  const ColIndex num_cols = basis_matrix.num_cols();
  col_perm->assign(num_cols, kInvalidCol);
  row_perm->assign(num_rows, kInvalidRow);

  // Get the empty matrix corner case out of the way.
  if (basis_matrix.IsEmpty()) return Status::OK();
  basis_matrix_ = &basis_matrix;

  // Initialize all the matrices.
  lower_.Reset(num_rows);
  upper_.Reset(num_rows);
  permuted_lower_.Reset(num_cols);
  permuted_upper_.Reset(num_cols);
  permuted_lower_column_needs_solve_.assign(num_cols, false);
  contains_only_singleton_columns_ = true;

  // Start by moving the singleton columns to the front and by putting their
  // non-zero coefficient on the diagonal. The general algorithm below would
  // have the same effect, but this function is a lot faster.
  int index = 0;
  ExtractSingletonColumns(basis_matrix, row_perm, col_perm, &index);
  ExtractResidualSingletonColumns(basis_matrix, row_perm, col_perm, &index);
  int stats_num_pivots_without_fill_in = index;
  int stats_degree_two_pivot_columns = 0;

  // Initialize residual_matrix_non_zero_ with the submatrix left after we
  // removed the singleton and residual singleton columns.
  InitializeResidualMatrix(basis_matrix, *row_perm, *col_perm);

  // Perform Gaussian elimination.
  const int end_index = std::min(num_rows.value(), num_cols.value());
  const Fractional singularity_threshold =
      parameters_.markowitz_singularity_threshold();
  while (index < end_index) {
    Fractional pivot_coefficient = 0.0;
    RowIndex pivot_row = kInvalidRow;
    ColIndex pivot_col = kInvalidCol;

    // TODO(user): If we don't need L and U, we can abort when the residual
    // matrix becomes dense (i.e. when its density factor is above a certain
    // threshold). The residual size is 'end_index - index' and the
    // density can either be computed exactly or estimated from min_markowitz.
    const int64 min_markowitz = FindPivot(*row_perm, *col_perm, &pivot_row,
                                          &pivot_col, &pivot_coefficient);

    // Singular matrix? No pivot will be selected if a column has no entries. If
    // a column has some entries, then we are sure that a pivot will be selected
    // but its magnitude can be really close to zero. In both cases, we
    // report the singularity of the matrix.
    if (pivot_row == kInvalidRow || pivot_col == kInvalidCol ||
        std::abs(pivot_coefficient) <= singularity_threshold) {
      GLOP_RETURN_AND_LOG_ERROR(
          Status::ERROR_LU, StringPrintf("The matrix is singular! pivot = %E",
                                         pivot_coefficient));
    }
    DCHECK_EQ((*row_perm)[pivot_row], kInvalidRow);
    DCHECK_EQ((*col_perm)[pivot_col], kInvalidCol);

    // Update residual_matrix_non_zero_.
    // TODO(user): This step can be skipped, once a fully dense matrix is
    // obtained. But note that permuted_lower_column_needs_solve_ needs to be
    // updated.
    const int pivot_col_degree = residual_matrix_non_zero_.ColDegree(pivot_col);
    const int pivot_row_degree = residual_matrix_non_zero_.RowDegree(pivot_row);
    residual_matrix_non_zero_.DeleteRowAndColumn(pivot_row, pivot_col);
    if (min_markowitz == 0) {
      ++stats_num_pivots_without_fill_in;
      if (pivot_col_degree == 1) {
        RemoveRowFromResidualMatrix(pivot_row, pivot_col);
      } else {
        DCHECK_EQ(pivot_row_degree, 1);
        RemoveColumnFromResidualMatrix(pivot_row, pivot_col);
      }
    } else {
      // TODO(user): Note that in some rare cases, because of numerical
      // cancellation, the column degree may actually be smaller than
      // pivot_col_degree. Exploit that better?
      IF_STATS_ENABLED(
          if (pivot_col_degree == 2) { ++stats_degree_two_pivot_columns; });
      UpdateResidualMatrix(pivot_row, pivot_col);
    }

    if (contains_only_singleton_columns_) {
      DCHECK(permuted_upper_.column(pivot_col).IsEmpty());
      lower_.AddDiagonalOnlyColumn(1.0);
      upper_.AddTriangularColumn(basis_matrix.column(pivot_col), pivot_row);
    } else {
      lower_.AddAndNormalizeTriangularColumn(permuted_lower_.column(pivot_col),
                                             pivot_row, pivot_coefficient);
      permuted_lower_.ClearAndReleaseColumn(pivot_col);

      upper_.AddTriangularColumnWithGivenDiagonalEntry(
          permuted_upper_.column(pivot_col), pivot_row, pivot_coefficient);
      permuted_upper_.ClearAndReleaseColumn(pivot_col);
    }

    // Update the permutations.
    (*col_perm)[pivot_col] = ColIndex(index);
    (*row_perm)[pivot_row] = RowIndex(index);
    ++index;
  }

  stats_.pivots_without_fill_in_ratio.Add(
      1.0 * stats_num_pivots_without_fill_in / end_index);
  stats_.degree_two_pivot_columns.Add(1.0 * stats_degree_two_pivot_columns /
                                      end_index);
  return Status::OK();
}

Status Markowitz::ComputeLU(const MatrixView& basis_matrix,
                            RowPermutation* row_perm,
                            ColumnPermutation* col_perm,
                            TriangularMatrix* lower, TriangularMatrix* upper) {
  // The two first swaps allow to use less memory since this way upper_
  // and lower_ will always stay empty at the end of this function.
  lower_.Swap(lower);
  upper_.Swap(upper);
  GLOP_RETURN_IF_ERROR(
      ComputeRowAndColumnPermutation(basis_matrix, row_perm, col_perm));
  SCOPED_TIME_STAT(&stats_);
  lower_.ApplyRowPermutationToNonDiagonalEntries(*row_perm);
  upper_.ApplyRowPermutationToNonDiagonalEntries(*row_perm);
  lower_.Swap(lower);
  upper_.Swap(upper);
  DCHECK(lower->IsLowerTriangular());
  DCHECK(upper->IsUpperTriangular());
  return Status::OK();
}

void Markowitz::Clear() {
  SCOPED_TIME_STAT(&stats_);
  permuted_lower_.Clear();
  permuted_upper_.Clear();
  residual_matrix_non_zero_.Clear();
  col_by_degree_.Clear();
  examined_col_.clear();
  is_col_by_degree_initialized_ = false;
}

void Markowitz::InitializeResidualMatrix(const MatrixView& basis_matrix,
                                         const RowPermutation& row_perm,
                                         const ColumnPermutation& col_perm) {
  SCOPED_TIME_STAT(&stats_);
  residual_matrix_non_zero_.InitializeFromMatrixSubset(basis_matrix, row_perm,
                                                       col_perm);

  // Initialize singleton_column_.
  singleton_column_.clear();
  const ColIndex num_cols = basis_matrix.num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    if (!residual_matrix_non_zero_.IsColumnDeleted(col) &&
        residual_matrix_non_zero_.ColDegree(col) == 1) {
      singleton_column_.push_back(col);
    }
  }

  // Initialize singleton_row_.
  singleton_row_.clear();
  const RowIndex num_rows = basis_matrix.num_rows();
  for (RowIndex row(0); row < num_rows; ++row) {
    if (residual_matrix_non_zero_.RowDegree(row) == 1) {
      singleton_row_.push_back(row);
    }
  }
}

namespace {
struct MatrixEntry {
  RowIndex row;
  ColIndex col;
  Fractional coefficient;
  MatrixEntry(RowIndex r, ColIndex c, Fractional coeff)
      : row(r), col(c), coefficient(coeff) {}
  bool operator<(const MatrixEntry& o) const {
    return (row == o.row) ? col < o.col : row < o.row;
  }
};

}  // namespace

void Markowitz::ExtractSingletonColumns(const MatrixView& basis_matrix,
                                        RowPermutation* row_perm,
                                        ColumnPermutation* col_perm,
                                        int* index) {
  SCOPED_TIME_STAT(&stats_);
  std::vector<MatrixEntry> singleton_entries;
  const ColIndex num_cols = basis_matrix.num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    const SparseColumn& column = basis_matrix.column(col);
    if (column.num_entries().value() == 1) {
      singleton_entries.push_back(MatrixEntry(
           column.GetFirstRow(), col, column.GetFirstCoefficient()));
    }
  }

  // Sorting the entries by row indices allows the row_permutation to be closer
  // to identity which seems like a good idea.
  std::sort(singleton_entries.begin(), singleton_entries.end());
  for (MatrixEntry e : singleton_entries) {
    if ((*row_perm)[e.row] == kInvalidRow) {
      (*col_perm)[e.col] = ColIndex(*index);
      (*row_perm)[e.row] = RowIndex(*index);
      lower_.AddDiagonalOnlyColumn(1.0);
      upper_.AddDiagonalOnlyColumn(e.coefficient);
      ++(*index);
    }
  }
  stats_.basis_singleton_column_ratio.Add(static_cast<double>(*index) /
                                          num_cols.value());
}

void Markowitz::ExtractResidualSingletonColumns(const MatrixView& basis_matrix,
                                                RowPermutation* row_perm,
                                                ColumnPermutation* col_perm,
                                                int* index) {
  SCOPED_TIME_STAT(&stats_);
  const ColIndex num_cols = basis_matrix.num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    if ((*col_perm)[col] != kInvalidCol) continue;
    const SparseColumn& column = basis_matrix.column(col);
    int residual_degree = 0;
    RowIndex row;
    for (SparseColumn::Entry e : column) {
      if ((*row_perm)[e.row()] == kInvalidRow) {
        ++residual_degree;
        if (residual_degree > 1) break;
        row = e.row();
      }
    }
    if (residual_degree == 1) {
      (*col_perm)[col] = ColIndex(*index);
      (*row_perm)[row] = RowIndex(*index);
      lower_.AddDiagonalOnlyColumn(1.0);
      upper_.AddTriangularColumn(basis_matrix.column(col), row);
      ++(*index);
    }
  }
  stats_.basis_residual_singleton_column_ratio.Add(static_cast<double>(*index) /
                                                   num_cols.value());
}

const SparseColumn& Markowitz::ComputeColumn(const RowPermutation& row_perm,
                                             ColIndex col) {
  SCOPED_TIME_STAT(&stats_);
  // Is this the first time ComputeColumn() sees this column? This is a bit
  // tricky because just one of the tests is not sufficient in case the matrix
  // is degenerate.
  const bool first_time = permuted_lower_.column(col).IsEmpty() &&
                          permuted_upper_.column(col).IsEmpty();

  // If !permuted_lower_column_needs_solve_[col] then the result of the
  // PermutedLowerSparseSolve() below is already stored in
  // permuted_lower_.column(col) and we just need to split this column. Note
  // that this is just an optimization and the code would work if we just
  // assumed permuted_lower_column_needs_solve_[col] to be always true.
  SparseColumn* lower_column = permuted_lower_.mutable_column(col);
  if (permuted_lower_column_needs_solve_[col]) {
    // Solve a sparse triangular system. If the column 'col' of permuted_lower_
    // was never computed before by ComputeColumn(), we use the column 'col' of
    // the matrix to factorize.
    const SparseColumn& input =
        first_time ? basis_matrix_->column(col) : *lower_column;
    lower_.PermutedLowerSparseSolve(input, row_perm, lower_column,
                                    permuted_upper_.mutable_column(col));
    permuted_lower_column_needs_solve_[col] = false;
    return *lower_column;
  }

  // All the symbolic non-zeros are always present in lower. So if this test is
  // true, we can conclude that there is no entries from upper that need to be
  // moved by a cardinality argument.
  if (lower_column->num_entries() == residual_matrix_non_zero_.ColDegree(col)) {
    return *lower_column;
  }

  // In this case, we just need to "split" the lower column.
  if (first_time) {
    lower_column->PopulateFromSparseVector(basis_matrix_->column(col));
  }
  lower_column->MoveTaggedEntriesTo(row_perm,
                                    permuted_upper_.mutable_column(col));
  return *lower_column;
}

int64 Markowitz::FindPivot(const RowPermutation& row_perm,
                           const ColumnPermutation& col_perm,
                           RowIndex* pivot_row, ColIndex* pivot_col,
                           Fractional* pivot_coefficient) {
  SCOPED_TIME_STAT(&stats_);

  // Fast track for singleton columns.
  while (!singleton_column_.empty()) {
    const ColIndex col = singleton_column_.back();
    singleton_column_.pop_back();
    DCHECK_EQ(kInvalidCol, col_perm[col]);

    // This can only happen if the matrix is singular. Continuing will cause
    // the algorithm to detect the singularity at the end when we stop before
    // the end.
    //
    // TODO(user): We could detect the singularity at this point, but that
    // may make the code more complex.
    if (residual_matrix_non_zero_.ColDegree(col) != 1) continue;

    // ComputeColumn() is not used as long as only singleton columns of the
    // residual matrix are used. See the other condition in
    // ComputeRowAndColumnPermutation().
    if (contains_only_singleton_columns_) {
      *pivot_col = col;
      for (const SparseColumn::Entry e : basis_matrix_->column(col)) {
        if (row_perm[e.row()] == kInvalidRow) {
          *pivot_row = e.row();
          *pivot_coefficient = e.coefficient();
          break;
        }
      }
      return 0;
    }
    const SparseColumn& column = ComputeColumn(row_perm, col);
    if (column.IsEmpty()) continue;
    *pivot_col = col;
    *pivot_row = column.GetFirstRow();
    *pivot_coefficient = column.GetFirstCoefficient();
    return 0;
  }
  contains_only_singleton_columns_ = false;

  // Fast track for singleton rows. Note that this is actually more than a fast
  // track because of the Zlatev heuristic. Such rows may not be processed as
  // soon as possible otherwise, resulting in more fill-in.
  while (!singleton_row_.empty()) {
    const RowIndex row = singleton_row_.back();
    singleton_row_.pop_back();

    // A singleton row could have been processed when processing a singleton
    // column. Skip if this is the case.
    if (row_perm[row] != kInvalidRow) continue;

    // This shows that the matrix is singular, see comment above for the same
    // case when processing singleton columns.
    if (residual_matrix_non_zero_.RowDegree(row) != 1) continue;
    const ColIndex col =
        residual_matrix_non_zero_.GetFirstNonDeletedColumnFromRow(row);
    if (col == kInvalidCol) continue;
    const SparseColumn& column = ComputeColumn(row_perm, col);
    if (column.IsEmpty()) continue;

    *pivot_col = col;
    *pivot_row = row;
    *pivot_coefficient = column.LookUpCoefficient(row);
    return 0;
  }

  // col_by_degree_ is not needed before we reach this point. Exploit this with
  // a lazy initialization.
  if (!is_col_by_degree_initialized_) {
    is_col_by_degree_initialized_ = true;
    const ColIndex num_cols = col_perm.size();
    col_by_degree_.Reset(row_perm.size().value(), num_cols);
    for (ColIndex col(0); col < num_cols; ++col) {
      if (col_perm[col] != kInvalidCol) continue;
      const int degree = residual_matrix_non_zero_.ColDegree(col);
      DCHECK_NE(degree, 1);
      UpdateDegree(col, degree);
    }
  }

  // Note(user): we use int64 since this is a product of two ints, moreover
  // the ints should be relatively small, so that should be fine for a while.
  int64 min_markowitz_number = std::numeric_limits<int64>::max();
  examined_col_.clear();
  const int num_columns_to_examine = parameters_.markowitz_zlatev_parameter();
  const Fractional threshold = parameters_.lu_factorization_pivot_threshold();
  while (examined_col_.size() < num_columns_to_examine) {
    const ColIndex col = col_by_degree_.Pop();
    if (col == kInvalidCol) break;
    if (col_perm[col] != kInvalidCol) continue;
    const int col_degree = residual_matrix_non_zero_.ColDegree(col);
    examined_col_.push_back(col);

    // Because of the two singleton special cases at the beginning of this
    // function and because we process columns by increasing degree, we can
    // derive a lower bound on the best markowitz number we can get by exploring
    // this column. If we cannot beat this number, we can stop here.
    //
    // Note(user): we still process extra column if we can meet the lower bound
    // to eventually have a better pivot.
    //
    // Todo(user): keep the minimum row degree to have a better bound?
    const int64 markowitz_lower_bound = col_degree - 1;
    if (min_markowitz_number < markowitz_lower_bound) break;

    // TODO(user): col_degree (which is the same as column.num_entries()) is
    // actually an upper bound on the number of non-zeros since there may be
    // numerical cancellations. Exploit this here? Note that it is already used
    // when we update the non_zero pattern of the residual matrix.
    const SparseColumn& column = ComputeColumn(row_perm, col);
    DCHECK_EQ(column.num_entries(), col_degree);

    Fractional max_magnitude = 0.0;
    for (const SparseColumn::Entry e : column) {
      max_magnitude = std::max(max_magnitude, std::abs(e.coefficient()));
    }
    if (max_magnitude == 0.0) {
      // All symbolic non-zero entries have been cancelled!
      // The matrix is singular, but we continue with the other columns.
      examined_col_.pop_back();
      continue;
    }

    const Fractional skip_threshold = threshold * max_magnitude;
    for (const SparseColumn::Entry e : column) {
      const Fractional magnitude = std::abs(e.coefficient());
      if (magnitude < skip_threshold) continue;

      const int row_degree = residual_matrix_non_zero_.RowDegree(e.row());
      const int64 markowitz_number = (col_degree - 1) * (row_degree - 1);
      DCHECK_NE(markowitz_number, 0);
      if (markowitz_number < min_markowitz_number ||
          ((markowitz_number == min_markowitz_number) &&
           magnitude > std::abs(*pivot_coefficient))) {
        min_markowitz_number = markowitz_number;
        *pivot_col = col;
        *pivot_row = e.row();
        *pivot_coefficient = e.coefficient();

        // Note(user): We could abort early here if the markowitz_lower_bound is
        // reached, but finishing to loop over this column is fast and may lead
        // to a pivot with a greater magnitude (i.e. a more robust
        // factorization).
      }
    }
    DCHECK_NE(min_markowitz_number, 0);
    DCHECK_GE(min_markowitz_number, markowitz_lower_bound);
  }

  // Push back the columns that we just looked at in the queue since they
  // are candidates for the next pivot.
  //
  // TODO(user): Do that after having updated the matrix? Rationale:
  // - col_by_degree_ is LIFO, so that may save work in ComputeColumn() by
  //   calling it again on the same columns.
  // - Maybe the earliest low-degree columns have a better precision? This
  //   actually depends on the number of operations so is not really true.
  // - Maybe picking the column randomly from the ones with lowest degree would
  //   help having more diversity from one factorization to the next. This is
  //   for the case we do implement this TODO.
  for (const ColIndex col : examined_col_) {
    if (col != *pivot_col) {
      const int degree = residual_matrix_non_zero_.ColDegree(col);
      col_by_degree_.PushOrAdjust(col, degree);
    }
  }
  return min_markowitz_number;
}

void Markowitz::UpdateDegree(ColIndex col, int degree) {
  DCHECK(is_col_by_degree_initialized_);

  // Separating the degree one columns work because we always select such
  // a column first and pivoting by such columns does not affect the degree of
  // any other singleton columns (except if the matrix is not inversible).
  //
  // Note that using this optimization does change the order in which the
  // degree one columns are taken compared to pushing them in the queue.
  if (degree == 1) {
    // Note that there is no need to remove this column from col_by_degree_
    // because it will be processed before col_by_degree_.Pop() is called and
    // then just be ignored.
    singleton_column_.push_back(col);
  } else {
    col_by_degree_.PushOrAdjust(col, degree);
  }
}

void Markowitz::RemoveRowFromResidualMatrix(RowIndex pivot_row,
                                            ColIndex pivot_col) {
  SCOPED_TIME_STAT(&stats_);
  // Note that instead of calling:
  //   residual_matrix_non_zero_.RemoveDeletedColumnsFromRow(pivot_row);
  // it is a bit faster to test each position with IsColumnDeleted() since we
  // will not need the pivot row anymore.
  if (is_col_by_degree_initialized_) {
    for (const ColIndex col : residual_matrix_non_zero_.RowNonZero(pivot_row)) {
      if (residual_matrix_non_zero_.IsColumnDeleted(col)) continue;
      UpdateDegree(col, residual_matrix_non_zero_.DecreaseColDegree(col));
    }
  } else {
    for (const ColIndex col : residual_matrix_non_zero_.RowNonZero(pivot_row)) {
      if (residual_matrix_non_zero_.IsColumnDeleted(col)) continue;
      if (residual_matrix_non_zero_.DecreaseColDegree(col) == 1) {
        singleton_column_.push_back(col);
      }
    }
  }
}

void Markowitz::RemoveColumnFromResidualMatrix(RowIndex pivot_row,
                                               ColIndex pivot_col) {
  SCOPED_TIME_STAT(&stats_);
  // The entries of the pivot column are exactly the symbolic non-zeros of the
  // residual matrix, since we didn't remove the entries with a coefficient of
  // zero during PermutedLowerSparseSolve().
  //
  // Note that it is okay to decrease the degree of a previous pivot row since
  // it was set to 0 and will never trigger this test. Even if it triggers it,
  // we just ignore such singleton rows in FindPivot().
  for (const SparseColumn::Entry e : permuted_lower_.column(pivot_col)) {
    const RowIndex row = e.row();
    if (residual_matrix_non_zero_.DecreaseRowDegree(row) == 1) {
      singleton_row_.push_back(row);
    }
  }
}

void Markowitz::UpdateResidualMatrix(RowIndex pivot_row, ColIndex pivot_col) {
  SCOPED_TIME_STAT(&stats_);
  const SparseColumn& pivot_column = permuted_lower_.column(pivot_col);
  residual_matrix_non_zero_.Update(pivot_row, pivot_col, pivot_column);
  for (const ColIndex col : residual_matrix_non_zero_.RowNonZero(pivot_row)) {
    DCHECK_NE(col, pivot_col);
    UpdateDegree(col, residual_matrix_non_zero_.ColDegree(col));
    permuted_lower_column_needs_solve_[col] = true;
  }
  RemoveColumnFromResidualMatrix(pivot_row, pivot_col);
}

void MatrixNonZeroPattern::Clear() {
  row_degree_.clear();
  col_degree_.clear();
  row_non_zero_.clear();
  deleted_columns_.clear();
  bool_scratchpad_.clear();
  num_non_deleted_columns_ = 0;
}

void MatrixNonZeroPattern::Reset(RowIndex num_rows, ColIndex num_cols) {
  Clear();
  row_degree_.resize(num_rows, 0);
  col_degree_.resize(num_cols, 0);
  row_non_zero_.resize(num_rows.value());
  deleted_columns_.resize(num_cols, false);
  bool_scratchpad_.resize(num_cols, false);
  num_non_deleted_columns_ = num_cols;
}

void MatrixNonZeroPattern::InitializeFromMatrixSubset(
    const MatrixView& basis_matrix, const RowPermutation& row_perm,
    const ColumnPermutation& col_perm) {
  const ColIndex num_cols = basis_matrix.num_cols();
  const RowIndex num_rows = basis_matrix.num_rows();

  // Reset the matrix and initialize the vectors to the correct sizes.
  Reset(num_rows, num_cols);

  // Compute the number of entries in each row.
  for (ColIndex col(0); col < num_cols; ++col) {
    if (col_perm[col] != kInvalidCol) {
      deleted_columns_[col] = true;
      --num_non_deleted_columns_;
      continue;
    }
    for (const SparseColumn::Entry e : basis_matrix.column(col)) {
      ++row_degree_[e.row()];
    }
  }

  // Reserve the row_non_zero_ vector sizes.
  for (RowIndex row(0); row < num_rows; ++row) {
    if (row_perm[row] == kInvalidRow) {
      row_non_zero_[row].reserve(row_degree_[row]);
    } else {
      // This is needed because in the row degree computation above, we do not
      // test for row_perm[row] == kInvalidRow because it is a bit faster.
      row_degree_[row] = 0;
    }
  }

  // Initialize row_non_zero_.
  for (ColIndex col(0); col < num_cols; ++col) {
    if (col_perm[col] != kInvalidCol) continue;
    int32 col_degree = 0;
    for (const SparseColumn::Entry e : basis_matrix.column(col)) {
      const RowIndex row = e.row();
      if (row_perm[row] == kInvalidRow) {
        ++col_degree;
        row_non_zero_[row].push_back(col);
      }
    }
    col_degree_[col] = col_degree;
  }
}

void MatrixNonZeroPattern::AddEntry(RowIndex row, ColIndex col) {
  ++row_degree_[row];
  ++col_degree_[col];
  row_non_zero_[row].push_back(col);
}

int32 MatrixNonZeroPattern::DecreaseColDegree(ColIndex col) {
  return --col_degree_[col];
}

int32 MatrixNonZeroPattern::DecreaseRowDegree(RowIndex row) {
  return --row_degree_[row];
}

void MatrixNonZeroPattern::DeleteRowAndColumn(RowIndex pivot_row,
                                              ColIndex pivot_col) {
  DCHECK(!deleted_columns_[pivot_col]);
  deleted_columns_[pivot_col] = true;
  --num_non_deleted_columns_;

  // We do that to optimize RemoveColumnFromResidualMatrix().
  row_degree_[pivot_row] = 0;
}

bool MatrixNonZeroPattern::IsColumnDeleted(ColIndex col) const {
  return deleted_columns_[col];
}

void MatrixNonZeroPattern::RemoveDeletedColumnsFromRow(RowIndex row) {
  auto& ref = row_non_zero_[row];
  int new_index = 0;
  const int end = ref.size();
  for (int i = 0; i < end; ++i) {
    const ColIndex col = ref[i];
    if (!deleted_columns_[col]) {
      ref[new_index] = col;
      ++new_index;
    }
  }
  ref.resize(new_index);
}

ColIndex MatrixNonZeroPattern::GetFirstNonDeletedColumnFromRow(RowIndex row)
    const {
  for (const ColIndex col : RowNonZero(row)) {
    if (!IsColumnDeleted(col)) return col;
  }
  return kInvalidCol;
}

void MatrixNonZeroPattern::Update(RowIndex pivot_row, ColIndex pivot_col,
                                  const SparseColumn& column) {
  // Since DeleteRowAndColumn() must be called just before this function,
  // the pivot column has been marked as deleted but degrees have not been
  // updated yet. Hence the +1.
  DCHECK(deleted_columns_[pivot_col]);
  const int max_row_degree = num_non_deleted_columns_.value() + 1;

  RemoveDeletedColumnsFromRow(pivot_row);
  for (const ColIndex col : row_non_zero_[pivot_row]) {
    DecreaseColDegree(col);
    bool_scratchpad_[col] = false;
  }

  // We only need to merge the row for the position with a coefficient different
  // from 0.0. Note that the column must contain all the symbolic non-zeros for
  // the row degree to be updated correctly. Note also that decreasing the row
  // degrees due to the deletion of pivot_col will happen outside this function.
  for (const SparseColumn::Entry e : column) {
    const RowIndex row = e.row();
    if (row == pivot_row) continue;

    // If the row is fully dense, there is nothing to do (the merge below will
    // not change anything). This is a small price to pay for a huge gain when
    // the matrix become dense.
    if (e.coefficient() == 0.0 || row_degree_[row] == max_row_degree) continue;
    DCHECK_LT(row_degree_[row], max_row_degree);

    // We only clean row_non_zero_[row] if there are more than 4 entries to
    // delete. Note(user): the 4 is somewhat arbitrary, but gives good results
    // on the Netlib (23/04/2013). Note that calling
    // RemoveDeletedColumnsFromRow() is not mandatory and does not change the LU
    // decomposition, so we could call it all the time or never and the
    // algorithm would still work.
    const int kDeletionThreshold = 4;
    if (row_non_zero_[row].size() > row_degree_[row] + kDeletionThreshold) {
      RemoveDeletedColumnsFromRow(row);
    }
    // TODO(user): Special case if row_non_zero_[pivot_row].size() == 1?
    if (/* DISABLES CODE */ (true)) {
      MergeInto(pivot_row, row);
    } else {
      // This is currently not used, but kept as an alternative algorithm to
      // investigate. The performance is really similar, but the final L.U is
      // different. Note that when this is used, there is no need to modify
      // bool_scratchpad_ at the beginning of this function.
      //
      // TODO(user): Add unit tests before using this.
      MergeIntoSorted(pivot_row, row);
    }
  }
}

void MatrixNonZeroPattern::MergeInto(RowIndex pivot_row, RowIndex row) {
  // Note that bool_scratchpad_ must be already false on the positions in
  // row_non_zero_[pivot_row].
  for (const ColIndex col : row_non_zero_[row]) {
    bool_scratchpad_[col] = true;
  }

  auto& non_zero = row_non_zero_[row];
  const int old_size = non_zero.size();
  for (const ColIndex col : row_non_zero_[pivot_row]) {
    if (bool_scratchpad_[col]) {
      bool_scratchpad_[col] = false;
    } else {
      non_zero.push_back(col);
      ++col_degree_[col];
    }
  }
  row_degree_[row] += non_zero.size() - old_size;
}

namespace {

// Given two sorted vectors (the second one is the initial value of out), merges
// them and outputs the sorted result in out. The merge is stable and an element
// of input_a will appear before the identical elements of the second input.
template <typename V, typename W>
void MergeSortedVectors(const V& input_a, W* out) {
  if (input_a.empty()) return;
  const auto& input_b = *out;
  int index_a = input_a.size() - 1;
  int index_b = input_b.size() - 1;
  int index_out = input_a.size() + input_b.size();
  out->resize(index_out);
  while (index_a >= 0) {
    if (index_b < 0) {
      while (index_a >= 0) {
        --index_out;
        (*out)[index_out] = input_a[index_a];
        --index_a;
      }
      return;
    }
    --index_out;
    if (input_a[index_a] > input_b[index_b]) {
      (*out)[index_out] = input_a[index_a];
      --index_a;
    } else {
      (*out)[index_out] = input_b[index_b];
      --index_b;
    }
  }
}

}  // namespace

// The algorithm first computes into col_scratchpad_ the entries in pivot_row
// that are not in the row (i.e. the fill-in). It then updates the non-zero
// pattern using this temporary vector.
void MatrixNonZeroPattern::MergeIntoSorted(RowIndex pivot_row, RowIndex row) {
  // We want to add the entries of the input not already in the output.
  const auto& input = row_non_zero_[pivot_row];
  const auto& output = row_non_zero_[row];

  // These two resizes are because of the set_difference() output iterator api.
  col_scratchpad_.resize(input.size());
  col_scratchpad_.resize(std::set_difference(input.begin(), input.end(),
                                             output.begin(), output.end(),
                                             col_scratchpad_.begin()) -
                         col_scratchpad_.begin());

  // Add the fill-in to the pattern.
  for (const ColIndex col : col_scratchpad_) {
    ++col_degree_[col];
  }
  row_degree_[row] += col_scratchpad_.size();
  MergeSortedVectors(col_scratchpad_, &row_non_zero_[row]);
}

void ColumnPriorityQueue::Clear() {
  col_degree_.clear();
  col_index_.clear();
  col_by_degree_.clear();
}

void ColumnPriorityQueue::Reset(int max_degree, ColIndex num_cols) {
  Clear();
  col_degree_.assign(num_cols, 0);
  col_index_.assign(num_cols, -1);
  col_by_degree_.resize(max_degree + 1);
  min_degree_ = num_cols.value();
}

void ColumnPriorityQueue::PushOrAdjust(ColIndex col, int32 degree) {
  DCHECK_GE(degree, 0);
  DCHECK_LT(degree, col_by_degree_.size());
  const int32 old_degree = col_degree_[col];
  if (degree != old_degree) {
    const int32 old_index = col_index_[col];
    if (old_index != -1) {
      col_by_degree_[old_degree][old_index] = col_by_degree_[old_degree].back();
      col_index_[col_by_degree_[old_degree].back()] = old_index;
      col_by_degree_[old_degree].pop_back();
    }
    if (degree > 0) {
      col_index_[col] = col_by_degree_[degree].size();
      col_degree_[col] = degree;
      col_by_degree_[degree].push_back(col);
      min_degree_ = std::min(min_degree_, degree);
    } else {
      col_index_[col] = -1;
      col_degree_[col] = 0;
    }
  }
}

ColIndex ColumnPriorityQueue::Pop() {
  while (col_by_degree_[min_degree_].empty()) {
    min_degree_++;
    if (min_degree_ == col_by_degree_.size()) return kInvalidCol;
  }
  const ColIndex col = col_by_degree_[min_degree_].back();
  col_by_degree_[min_degree_].pop_back();
  col_index_[col] = -1;
  col_degree_[col] = 0;
  return col;
}

void SparseMatrixWithReusableColumnMemory::Reset(ColIndex num_cols) {
  mapping_.assign(num_cols.value(), -1);
  free_columns_.clear();
  columns_.clear();
}

const SparseColumn& SparseMatrixWithReusableColumnMemory::column(
    ColIndex col) const {
  if (mapping_[col] == -1) return empty_column_;
  return columns_[mapping_[col]];
}

SparseColumn* SparseMatrixWithReusableColumnMemory::mutable_column(
    ColIndex col) {
  if (mapping_[col] != -1) return &columns_[mapping_[col]];
  int new_col_index;
  if (free_columns_.empty()) {
    new_col_index = columns_.size();
    columns_.push_back(SparseColumn());
  } else {
    new_col_index = free_columns_.back();
    free_columns_.pop_back();
  }
  mapping_[col] = new_col_index;
  return &columns_[new_col_index];
}

void SparseMatrixWithReusableColumnMemory::ClearAndReleaseColumn(ColIndex col) {
  DCHECK_NE(mapping_[col], -1);
  free_columns_.push_back(mapping_[col]);
  columns_[mapping_[col]].Clear();
  mapping_[col] = -1;
}

void SparseMatrixWithReusableColumnMemory::Clear() {
  mapping_.clear();
  free_columns_.clear();
  columns_.clear();
}

}  // namespace glop
}  // namespace operations_research
