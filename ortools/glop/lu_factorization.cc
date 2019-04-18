// Copyright 2010-2018 Google LLC
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

#include "ortools/glop/lu_factorization.h"

#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_utils.h"

namespace operations_research {
namespace glop {

LuFactorization::LuFactorization()
    : is_identity_factorization_(true),
      col_perm_(),
      inverse_col_perm_(),
      row_perm_(),
      inverse_row_perm_() {}

void LuFactorization::Clear() {
  SCOPED_TIME_STAT(&stats_);
  lower_.Reset(RowIndex(0));
  upper_.Reset(RowIndex(0));
  transpose_upper_.Reset(RowIndex(0));
  transpose_lower_.Reset(RowIndex(0));
  is_identity_factorization_ = true;
  col_perm_.clear();
  row_perm_.clear();
  inverse_row_perm_.clear();
  inverse_col_perm_.clear();
}

Status LuFactorization::ComputeFactorization(const MatrixView& matrix) {
  SCOPED_TIME_STAT(&stats_);
  Clear();
  if (matrix.num_rows().value() != matrix.num_cols().value()) {
    GLOP_RETURN_AND_LOG_ERROR(Status::ERROR_LU, "Not a square matrix!!");
  }

  GLOP_RETURN_IF_ERROR(
      markowitz_.ComputeLU(matrix, &row_perm_, &col_perm_, &lower_, &upper_));
  inverse_col_perm_.PopulateFromInverse(col_perm_);
  inverse_row_perm_.PopulateFromInverse(row_perm_);
  ComputeTransposeUpper();
  ComputeTransposeLower();

  is_identity_factorization_ = false;
  IF_STATS_ENABLED({
    stats_.lu_fill_in.Add(GetFillInPercentage(matrix));
    stats_.basis_num_entries.Add(matrix.num_entries().value());
  });
  DCHECK(CheckFactorization(matrix, Fractional(1e-6)));
  return Status::OK();
}

void LuFactorization::RightSolve(DenseColumn* x) const {
  SCOPED_TIME_STAT(&stats_);
  if (is_identity_factorization_) return;

  ApplyPermutation(row_perm_, *x, &dense_column_scratchpad_);
  lower_.LowerSolve(&dense_column_scratchpad_);
  upper_.UpperSolve(&dense_column_scratchpad_);
  ApplyPermutation(inverse_col_perm_, dense_column_scratchpad_, x);
}

void LuFactorization::LeftSolve(DenseRow* y) const {
  SCOPED_TIME_STAT(&stats_);
  if (is_identity_factorization_) return;

  // We need to interpret y as a column for the permutation functions.
  DenseColumn* const x = reinterpret_cast<DenseColumn*>(y);
  ApplyInversePermutation(inverse_col_perm_, *x, &dense_column_scratchpad_);
  upper_.TransposeUpperSolve(&dense_column_scratchpad_);
  lower_.TransposeLowerSolve(&dense_column_scratchpad_, nullptr);
  ApplyInversePermutation(row_perm_, dense_column_scratchpad_, x);
}

namespace {
// If non_zeros is empty, uses a dense algorithm to compute the squared L2
// norm of the given column, otherwise do the same with a sparse version. In
// both cases column is cleared.
Fractional ComputeSquaredNormAndResetToZero(
    const std::vector<RowIndex>& non_zeros, DenseColumn* column) {
  Fractional sum = 0.0;
  if (non_zeros.empty()) {
    sum = SquaredNorm(*column);
    column->clear();
  } else {
    for (const RowIndex row : non_zeros) {
      sum += Square((*column)[row]);
      (*column)[row] = 0.0;
    }
  }
  return sum;
}
}  // namespace

Fractional LuFactorization::RightSolveSquaredNorm(const SparseColumn& a) const {
  SCOPED_TIME_STAT(&stats_);
  if (is_identity_factorization_) return SquaredNorm(a);

  non_zero_rows_.clear();
  dense_zero_scratchpad_.resize(lower_.num_rows(), 0.0);
  DCHECK(IsAllZero(dense_zero_scratchpad_));

  for (const SparseColumn::Entry e : a) {
    const RowIndex permuted_row = row_perm_[e.row()];
    dense_zero_scratchpad_[permuted_row] = e.coefficient();
    non_zero_rows_.push_back(permuted_row);
  }

  lower_.ComputeRowsToConsiderInSortedOrder(&non_zero_rows_);
  if (non_zero_rows_.empty()) {
    lower_.LowerSolve(&dense_zero_scratchpad_);
  } else {
    lower_.HyperSparseSolve(&dense_zero_scratchpad_, &non_zero_rows_);
    upper_.ComputeRowsToConsiderInSortedOrder(&non_zero_rows_);
  }
  if (non_zero_rows_.empty()) {
    upper_.UpperSolve(&dense_zero_scratchpad_);
  } else {
    upper_.HyperSparseSolveWithReversedNonZeros(&dense_zero_scratchpad_,
                                                &non_zero_rows_);
  }
  return ComputeSquaredNormAndResetToZero(non_zero_rows_,
                                          &dense_zero_scratchpad_);
}

Fractional LuFactorization::DualEdgeSquaredNorm(RowIndex row) const {
  if (is_identity_factorization_) return 1.0;
  SCOPED_TIME_STAT(&stats_);
  const RowIndex permuted_row =
      col_perm_.empty() ? row : ColToRowIndex(col_perm_[RowToColIndex(row)]);

  non_zero_rows_.clear();
  dense_zero_scratchpad_.resize(lower_.num_rows(), 0.0);
  DCHECK(IsAllZero(dense_zero_scratchpad_));
  dense_zero_scratchpad_[permuted_row] = 1.0;
  non_zero_rows_.push_back(permuted_row);

  transpose_upper_.ComputeRowsToConsiderInSortedOrder(&non_zero_rows_);
  if (non_zero_rows_.empty()) {
    transpose_upper_.LowerSolveStartingAt(RowToColIndex(permuted_row),
                                          &dense_zero_scratchpad_);
  } else {
    transpose_upper_.HyperSparseSolve(&dense_zero_scratchpad_, &non_zero_rows_);
    transpose_lower_.ComputeRowsToConsiderInSortedOrder(&non_zero_rows_);
  }
  if (non_zero_rows_.empty()) {
    transpose_lower_.UpperSolve(&dense_zero_scratchpad_);
  } else {
    transpose_lower_.HyperSparseSolveWithReversedNonZeros(
        &dense_zero_scratchpad_, &non_zero_rows_);
  }
  return ComputeSquaredNormAndResetToZero(non_zero_rows_,
                                          &dense_zero_scratchpad_);
}

namespace {
// Returns whether 'b' is equal to 'a' permuted by the given row permutation
// 'perm'.
bool AreEqualWithPermutation(const DenseColumn& a, const DenseColumn& b,
                             const RowPermutation& perm) {
  const RowIndex num_rows = perm.size();
  for (RowIndex row(0); row < num_rows; ++row) {
    if (a[row] != b[perm[row]]) return false;
  }
  return true;
}
}  // namespace

void LuFactorization::RightSolveLWithPermutedInput(const DenseColumn& a,
                                                   DenseColumn* x) const {
  SCOPED_TIME_STAT(&stats_);
  if (!is_identity_factorization_) {
    DCHECK(AreEqualWithPermutation(a, *x, row_perm_));
    lower_.LowerSolve(x);
  }
}

void LuFactorization::RightSolveLForColumnView(
    const CompactSparseMatrix::ColumnView& b, ScatteredColumn* x) const {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(IsAllZero(x->values));
  x->non_zeros.clear();
  if (is_identity_factorization_) {
    const EntryIndex num_entries = b.num_entries();
    for (EntryIndex i(0); i < num_entries; ++i) {
      (*x)[b.EntryRow(i)] = b.EntryCoefficient(i);
      x->non_zeros.push_back(b.EntryRow(i));
    }
    return;
  }

  // This code is equivalent to
  //    b.PermutedCopyToDenseVector(row_perm_, num_rows, x);
  // but it also computes the first column index which does not correspond to an
  // identity column of lower_ thus exploiting a bit the hyper-sparsity
  // of b.
  ColIndex first_column_to_consider(RowToColIndex(x->values.size()));
  const ColIndex limit = lower_.GetFirstNonIdentityColumn();
  const EntryIndex num_entries = b.num_entries();
  for (EntryIndex i(0); i < num_entries; ++i) {
    const RowIndex permuted_row = row_perm_[b.EntryRow(i)];
    (*x)[permuted_row] = b.EntryCoefficient(i);
    x->non_zeros.push_back(permuted_row);

    // The second condition only works because the elements on the diagonal of
    // lower_ are all equal to 1.0.
    const ColIndex col = RowToColIndex(permuted_row);
    if (col < limit || lower_.ColumnIsDiagonalOnly(col)) {
      DCHECK_EQ(1.0, lower_.GetDiagonalCoefficient(col));
      continue;
    }
    first_column_to_consider = std::min(first_column_to_consider, col);
  }

  lower_.ComputeRowsToConsiderInSortedOrder(&x->non_zeros);
  x->non_zeros_are_sorted = true;
  if (x->non_zeros.empty()) {
    lower_.LowerSolveStartingAt(first_column_to_consider, &x->values);
  } else {
    lower_.HyperSparseSolve(&x->values, &x->non_zeros);
  }
}

void LuFactorization::RightSolveLWithNonZeros(ScatteredColumn* x) const {
  if (is_identity_factorization_) return;
  if (x->non_zeros.empty()) {
    PermuteWithScratchpad(row_perm_, &dense_zero_scratchpad_, &x->values);
    lower_.LowerSolve(&x->values);
    return;
  }

  PermuteWithKnownNonZeros(row_perm_, &dense_zero_scratchpad_, &x->values,
                           &x->non_zeros);
  lower_.ComputeRowsToConsiderInSortedOrder(&x->non_zeros);
  x->non_zeros_are_sorted = true;
  if (x->non_zeros.empty()) {
    lower_.LowerSolve(&x->values);
  } else {
    lower_.HyperSparseSolve(&x->values, &x->non_zeros);
  }
}

// TODO(user): This code is almost the same a RightSolveLForSparseColumn()
// except that the API to iterate on the input is different. Find a way to
// deduplicate the code.
void LuFactorization::RightSolveLForScatteredColumn(const ScatteredColumn& b,
                                                    ScatteredColumn* x) const {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(IsAllZero(x->values));
  x->non_zeros.clear();

  if (is_identity_factorization_) {
    *x = b;
    return;
  }

  if (b.non_zeros.empty()) {
    *x = b;
    return RightSolveLWithNonZeros(x);
  }

  // This code is equivalent to
  //    b.PermutedCopyToDenseVector(row_perm_, num_rows, x);
  // but it also computes the first column index which does not correspond to an
  // identity column of lower_ thus exploiting a bit the hyper-sparsity of b.
  ColIndex first_column_to_consider(RowToColIndex(x->values.size()));
  const ColIndex limit = lower_.GetFirstNonIdentityColumn();
  for (const RowIndex row : b.non_zeros) {
    const RowIndex permuted_row = row_perm_[row];
    (*x)[permuted_row] = b[row];
    x->non_zeros.push_back(permuted_row);

    // The second condition only works because the elements on the diagonal of
    // lower_ are all equal to 1.0.
    const ColIndex col = RowToColIndex(permuted_row);
    if (col < limit || lower_.ColumnIsDiagonalOnly(col)) {
      DCHECK_EQ(1.0, lower_.GetDiagonalCoefficient(col));
      continue;
    }
    first_column_to_consider = std::min(first_column_to_consider, col);
  }

  lower_.ComputeRowsToConsiderInSortedOrder(&x->non_zeros);
  x->non_zeros_are_sorted = true;
  if (x->non_zeros.empty()) {
    lower_.LowerSolveStartingAt(first_column_to_consider, &x->values);
  } else {
    lower_.HyperSparseSolve(&x->values, &x->non_zeros);
  }
}

void LuFactorization::LeftSolveUWithNonZeros(ScatteredRow* y) const {
  SCOPED_TIME_STAT(&stats_);
  CHECK(col_perm_.empty());
  if (is_identity_factorization_) return;

  DenseColumn* const x = reinterpret_cast<DenseColumn*>(&y->values);
  RowIndexVector* const nz = reinterpret_cast<RowIndexVector*>(&y->non_zeros);
  transpose_upper_.ComputeRowsToConsiderInSortedOrder(nz);
  y->non_zeros_are_sorted = true;
  if (nz->empty()) {
    upper_.TransposeUpperSolve(x);
  } else {
    upper_.TransposeHyperSparseSolve(x, nz);
  }
}

void LuFactorization::RightSolveUWithNonZeros(ScatteredColumn* x) const {
  SCOPED_TIME_STAT(&stats_);
  CHECK(col_perm_.empty());
  if (is_identity_factorization_) return;

  // If non-zeros is non-empty, we use an hypersparse solve. Note that if
  // non_zeros starts to be too big, we clear it and thus switch back to a
  // normal sparse solve.
  upper_.ComputeRowsToConsiderInSortedOrder(&x->non_zeros);
  x->non_zeros_are_sorted = true;
  if (x->non_zeros.empty()) {
    upper_.UpperSolve(&x->values);
  } else {
    upper_.HyperSparseSolveWithReversedNonZeros(&x->values, &x->non_zeros);
  }
}

bool LuFactorization::LeftSolveLWithNonZeros(
    ScatteredRow* y, ScatteredColumn* result_before_permutation) const {
  SCOPED_TIME_STAT(&stats_);
  if (is_identity_factorization_) {
    // It is not advantageous to fill result_before_permutation in this case.
    return false;
  }
  DenseColumn* const x = reinterpret_cast<DenseColumn*>(&y->values);
  std::vector<RowIndex>* nz = reinterpret_cast<RowIndexVector*>(&y->non_zeros);

  // Hypersparse?
  RowIndex last_non_zero_row = ColToRowIndex(y->values.size() - 1);
  transpose_lower_.ComputeRowsToConsiderInSortedOrder(nz);
  y->non_zeros_are_sorted = true;
  if (nz->empty()) {
    lower_.TransposeLowerSolve(x, &last_non_zero_row);
  } else {
    lower_.TransposeHyperSparseSolveWithReversedNonZeros(x, nz);
  }

  if (result_before_permutation == nullptr || !nz->empty()) {
    // Note(user): For the behavior of the two functions to be exactly the same,
    // we need the positions listed in nz to be the "exact" non-zeros of x. This
    // should be the case because the hyper-sparse functions makes sure of that.
    // We also DCHECK() this below.
    if (nz->empty()) {
      PermuteWithScratchpad(inverse_row_perm_, &dense_zero_scratchpad_, x);
    } else {
      PermuteWithKnownNonZeros(inverse_row_perm_, &dense_zero_scratchpad_, x,
                               nz);
    }
    if (DEBUG_MODE) {
      for (const RowIndex row : *nz) {
        DCHECK_NE((*x)[row], 0.0);
      }
    }
    return false;
  } else {
    // This computes the same thing as in the other branch but also keeps the
    // original x in result_before_permutation. Because of this, it is faster to
    // use a different algorithm.
    result_before_permutation->non_zeros.clear();
    x->swap(result_before_permutation->values);
    x->AssignToZero(inverse_row_perm_.size());
    y->non_zeros.clear();
    for (RowIndex row(0); row <= last_non_zero_row; ++row) {
      const Fractional value = (*result_before_permutation)[row];
      if (value != 0.0) {
        const RowIndex permuted_row = inverse_row_perm_[row];
        (*x)[permuted_row] = value;
      }
    }
    return true;
  }
}

ColIndex LuFactorization::LeftSolveUForUnitRow(ColIndex col,
                                               ScatteredRow* y) const {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(IsAllZero(y->values));
  DCHECK(y->non_zeros.empty());
  if (is_identity_factorization_) {
    (*y)[col] = 1.0;
    y->non_zeros.push_back(col);
    return col;
  }
  const ColIndex permuted_col = col_perm_.empty() ? col : col_perm_[col];
  (*y)[permuted_col] = 1.0;
  y->non_zeros.push_back(permuted_col);

  // Using the transposed matrix here is faster (even accounting the time to
  // construct it). Note the small optimization in case the inversion is
  // trivial.
  if (transpose_upper_.ColumnIsDiagonalOnly(permuted_col)) {
    (*y)[permuted_col] /= transpose_upper_.GetDiagonalCoefficient(permuted_col);
  } else {
    RowIndexVector* const nz = reinterpret_cast<RowIndexVector*>(&y->non_zeros);
    DenseColumn* const x = reinterpret_cast<DenseColumn*>(&y->values);
    transpose_upper_.ComputeRowsToConsiderInSortedOrder(nz);
    y->non_zeros_are_sorted = true;
    if (y->non_zeros.empty()) {
      transpose_upper_.LowerSolveStartingAt(permuted_col, x);
    } else {
      transpose_upper_.HyperSparseSolve(x, nz);
    }
  }
  return permuted_col;
}

const SparseColumn& LuFactorization::GetColumnOfU(ColIndex col) const {
  if (is_identity_factorization_) {
    column_of_upper_.Clear();
    column_of_upper_.SetCoefficient(ColToRowIndex(col), 1.0);
    return column_of_upper_;
  }
  upper_.CopyColumnToSparseColumn(col_perm_.empty() ? col : col_perm_[col],
                                  &column_of_upper_);
  return column_of_upper_;
}

double LuFactorization::GetFillInPercentage(const MatrixView& matrix) const {
  const int initial_num_entries = matrix.num_entries().value();
  const int lu_num_entries =
      (lower_.num_entries() + upper_.num_entries()).value();
  if (is_identity_factorization_ || initial_num_entries == 0) return 1.0;
  return static_cast<double>(lu_num_entries) /
         static_cast<double>(initial_num_entries);
}

EntryIndex LuFactorization::NumberOfEntries() const {
  return is_identity_factorization_
             ? EntryIndex(0)
             : lower_.num_entries() + upper_.num_entries();
}

Fractional LuFactorization::ComputeDeterminant() const {
  if (is_identity_factorization_) return 1.0;
  DCHECK_EQ(upper_.num_rows().value(), upper_.num_cols().value());
  Fractional product(1.0);
  for (ColIndex col(0); col < upper_.num_cols(); ++col) {
    product *= upper_.GetDiagonalCoefficient(col);
  }
  return product * row_perm_.ComputeSignature() *
         inverse_col_perm_.ComputeSignature();
}

Fractional LuFactorization::ComputeInverseOneNorm() const {
  if (is_identity_factorization_) return 1.0;
  const RowIndex num_rows = lower_.num_rows();
  const ColIndex num_cols = lower_.num_cols();
  Fractional norm = 0.0;
  for (ColIndex col(0); col < num_cols; ++col) {
    DenseColumn right_hand_side(num_rows, 0.0);
    right_hand_side[ColToRowIndex(col)] = 1.0;
    // Get a column of the matrix inverse.
    RightSolve(&right_hand_side);
    Fractional column_norm = 0.0;
    // Compute sum_i |basis_matrix_ij|.
    for (RowIndex row(0); row < num_rows; ++row) {
      column_norm += std::abs(right_hand_side[row]);
    }
    // Compute max_j sum_i |basis_matrix_ij|
    norm = std::max(norm, column_norm);
  }
  return norm;
}

Fractional LuFactorization::ComputeInverseInfinityNorm() const {
  if (is_identity_factorization_) return 1.0;
  const RowIndex num_rows = lower_.num_rows();
  const ColIndex num_cols = lower_.num_cols();
  DenseColumn row_sum(num_rows, 0.0);
  for (ColIndex col(0); col < num_cols; ++col) {
    DenseColumn right_hand_side(num_rows, 0.0);
    right_hand_side[ColToRowIndex(col)] = 1.0;
    // Get a column of the matrix inverse.
    RightSolve(&right_hand_side);
    // Compute sum_j |basis_matrix_ij|.
    for (RowIndex row(0); row < num_rows; ++row) {
      row_sum[row] += std::abs(right_hand_side[row]);
    }
  }
  // Compute max_i sum_j |basis_matrix_ij|
  Fractional norm = 0.0;
  for (RowIndex row(0); row < num_rows; ++row) {
    norm = std::max(norm, row_sum[row]);
  }
  return norm;
}

Fractional LuFactorization::ComputeOneNormConditionNumber(
    const MatrixView& matrix) const {
  if (is_identity_factorization_) return 1.0;
  return matrix.ComputeOneNorm() * ComputeInverseOneNorm();
}

Fractional LuFactorization::ComputeInfinityNormConditionNumber(
    const MatrixView& matrix) const {
  if (is_identity_factorization_) return 1.0;
  return matrix.ComputeInfinityNorm() * ComputeInverseInfinityNorm();
}

Fractional LuFactorization::ComputeInverseInfinityNormUpperBound() const {
  return lower_.ComputeInverseInfinityNormUpperBound() *
         upper_.ComputeInverseInfinityNormUpperBound();
}

namespace {
// Returns the density of the sparse column 'b' w.r.t. the given permutation.
double ComputeDensity(const SparseColumn& b, const RowPermutation& row_perm) {
  double density = 0.0;
  for (const SparseColumn::Entry e : b) {
    if (row_perm[e.row()] != kNonPivotal && e.coefficient() != 0.0) {
      ++density;
    }
  }
  const RowIndex num_rows = row_perm.size();
  return density / num_rows.value();
}
}  // anonymous namespace

void LuFactorization::ComputeTransposeUpper() {
  SCOPED_TIME_STAT(&stats_);
  transpose_upper_.PopulateFromTranspose(upper_);
}

void LuFactorization::ComputeTransposeLower() const {
  SCOPED_TIME_STAT(&stats_);
  transpose_lower_.PopulateFromTranspose(lower_);
}

bool LuFactorization::CheckFactorization(const MatrixView& matrix,
                                         Fractional tolerance) const {
  if (is_identity_factorization_) return true;
  SparseMatrix lu;
  ComputeLowerTimesUpper(&lu);
  SparseMatrix paq;
  paq.PopulateFromPermutedMatrix(matrix, row_perm_, inverse_col_perm_);
  if (!row_perm_.Check()) {
    return false;
  }
  if (!inverse_col_perm_.Check()) {
    return false;
  }

  SparseMatrix should_be_zero;
  should_be_zero.PopulateFromLinearCombination(Fractional(1.0), paq,
                                               Fractional(-1.0), lu);

  for (ColIndex col(0); col < should_be_zero.num_cols(); ++col) {
    for (const SparseColumn::Entry e : should_be_zero.column(col)) {
      const Fractional magnitude = std::abs(e.coefficient());
      if (magnitude > tolerance) {
        VLOG(2) << magnitude << " != 0, at column " << col;
        return false;
      }
    }
  }
  return true;
}

}  // namespace glop
}  // namespace operations_research
