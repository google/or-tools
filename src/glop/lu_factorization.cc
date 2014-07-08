
#include "glop/lu_factorization.h"
#include "glop/lp_utils.h"

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
    RETURN_AND_LOG_ERROR(Status::ERROR_LU, "Not a square matrix!!");
  }

  RETURN_IF_ERROR(
      markowitz_.ComputeLU(matrix, &row_perm_, &col_perm_, &lower_, &upper_));
  inverse_col_perm_.PopulateFromInverse(col_perm_);
  inverse_row_perm_.PopulateFromInverse(row_perm_);
  ComputeTransposeUpper();

  is_identity_factorization_ = false;
  IF_STATS_ENABLED({
    stats_.lu_fill_in.Add(GetFillInPercentage(matrix));
    stats_.basis_num_entries.Add(matrix.num_entries().value());
  });
  DCHECK(CheckFactorization(matrix, Fractional(1e-6)));
  return Status::OK;
}

void LuFactorization::RightSolve(DenseColumn* x) const {
  SCOPED_TIME_STAT(&stats_);
  if (is_identity_factorization_) return;
  ApplyPermutation(row_perm_, *x, &dense_column_scratchpad_);
  lower_.LowerSolve(&dense_column_scratchpad_);
  upper_.UpperSolve(&dense_column_scratchpad_);
  ApplyPermutation(inverse_col_perm_, dense_column_scratchpad_, x);
}

void LuFactorization::SparseRightSolve(const SparseColumn& b, RowIndex num_rows,
                                       DenseColumn* x) const {
  SCOPED_TIME_STAT(&stats_);
  if (is_identity_factorization_) {
    b.CopyToDenseVector(num_rows, x);
    return;
  }
  DCHECK_EQ(num_rows, lower_.num_rows());
  DCHECK_EQ(num_rows, row_perm_.size());

  b.PermutedCopyToDenseVector(row_perm_, num_rows, &dense_column_scratchpad_);
  lower_.LowerSolve(&dense_column_scratchpad_);
  upper_.UpperSolve(&dense_column_scratchpad_);
  ApplyPermutation(inverse_col_perm_, dense_column_scratchpad_, x);
}

namespace {
// If non_zeros is empty, uses a dense algorithm to compute the squared L2
// norm of the given column, otherwise do the same with a sparse version. In
// both cases column is cleared.
Fractional ComputeSquaredNormAndResetToZero(const std::vector<RowIndex>& non_zeros,
                                            DenseColumn* column) {
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

  lower_.TriangularComputeRowsToConsider(&non_zero_rows_);
  if (non_zero_rows_.empty()) {
    lower_.LowerSolve(&dense_zero_scratchpad_);
  } else {
    lower_.SparseTriangularSolve(non_zero_rows_, &dense_zero_scratchpad_);
    upper_.TriangularComputeRowsToConsider(&non_zero_rows_);
  }
  if (non_zero_rows_.empty()) {
    upper_.UpperSolve(&dense_zero_scratchpad_);
  } else {
    upper_.SparseTriangularSolve(non_zero_rows_, &dense_zero_scratchpad_);
  }
  return ComputeSquaredNormAndResetToZero(non_zero_rows_,
                                          &dense_zero_scratchpad_);
}

void LuFactorization::LeftSolveScratchpad() const {
  upper_.TransposeUpperSolve(&dense_column_scratchpad_);
  lower_.TransposeLowerSolve(&dense_column_scratchpad_, nullptr);
}

Fractional LuFactorization::DualEdgeSquaredNorm(RowIndex row) const {
  if (is_identity_factorization_) return 1.0;
  if (transpose_lower_.IsEmpty()) {
    ComputeTransposeLower();
  }
  SCOPED_TIME_STAT(&stats_);
  const RowIndex permuted_row =
      col_perm_.empty() ? row : ColToRowIndex(col_perm_[RowToColIndex(row)]);

  non_zero_rows_.clear();
  dense_zero_scratchpad_.resize(lower_.num_rows(), 0.0);
  DCHECK(IsAllZero(dense_zero_scratchpad_));
  dense_zero_scratchpad_[permuted_row] = 1.0;
  non_zero_rows_.push_back(permuted_row);

  transpose_upper_.TriangularComputeRowsToConsider(&non_zero_rows_);
  if (non_zero_rows_.empty()) {
    transpose_upper_.LowerSolveStartingAt(RowToColIndex(permuted_row),
                                          &dense_zero_scratchpad_);
  } else {
    transpose_upper_.SparseTriangularSolve(non_zero_rows_,
                                           &dense_zero_scratchpad_);
    transpose_lower_.TriangularComputeRowsToConsider(&non_zero_rows_);
  }
  if (non_zero_rows_.empty()) {
    lower_.TransposeLowerSolve(&dense_zero_scratchpad_, nullptr);
  } else {
    transpose_lower_.SparseTriangularSolve(non_zero_rows_,
                                           &dense_zero_scratchpad_);
  }
  return ComputeSquaredNormAndResetToZero(non_zero_rows_,
                                          &dense_zero_scratchpad_);
}

void LuFactorization::LeftSolve(DenseRow* y) const {
  SCOPED_TIME_STAT(&stats_);
  if (is_identity_factorization_) return;
  // We need to interpret y as a column for the permutation functions.
  DenseColumn* const x = reinterpret_cast<DenseColumn*>(y);
  ApplyInversePermutation(inverse_col_perm_, *x, &dense_column_scratchpad_);
  LeftSolveScratchpad();
  ApplyInversePermutation(row_perm_, dense_column_scratchpad_, x);
}

namespace {

// Interprets a DenseRow as a DenseColumn. This is useful to reduce the number
// of different functions needed.
DenseColumn* DenseRowAsColumn(DenseRow* y) {
  return reinterpret_cast<DenseColumn*>(y);
}

}  // namespace

void LuFactorization::RightSolveL(DenseColumn* x) const {
  SCOPED_TIME_STAT(&stats_);
  if (!is_identity_factorization_) {
    ApplyPermutationWhenInputIsProbablySparse(row_perm_,
                                              &dense_zero_scratchpad_, x);
    lower_.LowerSolve(x);
  }
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
  if (is_identity_factorization_) {
    *x = a;
  } else {
    DCHECK(AreEqualWithPermutation(a, *x, row_perm_));
    lower_.LowerSolve(x);
  }
}

void LuFactorization::RightSolveU(DenseColumn* x) const {
  SCOPED_TIME_STAT(&stats_);
  if (is_identity_factorization_) return;
  if (col_perm_.empty()) {
    upper_.UpperSolve(x);
  } else {
    x->swap(dense_column_scratchpad_);
    upper_.UpperSolve(&dense_column_scratchpad_);
    ApplyPermutation(inverse_col_perm_, dense_column_scratchpad_, x);
  }
}

void LuFactorization::LeftSolveU(DenseRow* y) const {
  SCOPED_TIME_STAT(&stats_);
  if (is_identity_factorization_) return;
  if (col_perm_.empty()) {
    upper_.TransposeUpperSolve(DenseRowAsColumn(y));
  } else {
    DenseColumn* const x = DenseRowAsColumn(y);
    ApplyInversePermutation(inverse_col_perm_, *x, &dense_column_scratchpad_);
    upper_.TransposeUpperSolve(&dense_column_scratchpad_);
    x->swap(dense_column_scratchpad_);
  }
}

void LuFactorization::LeftSolveL(DenseRow* y) const {
  SCOPED_TIME_STAT(&stats_);
  if (is_identity_factorization_) return;
  DenseColumn* const x = DenseRowAsColumn(y);
  x->swap(dense_column_scratchpad_);
  lower_.TransposeLowerSolve(&dense_column_scratchpad_, nullptr);
  ApplyInversePermutation(row_perm_, dense_column_scratchpad_, x);
}

void LuFactorization::RightSolveLForSparseColumn(const SparseColumn& b,
                                                 DenseColumn* x) const {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(IsAllZero(*x));
  if (is_identity_factorization_) {
    for (const SparseColumn::Entry e : b) {
      (*x)[e.row()] = e.coefficient();
    }
    return;
  }

  // This code is equivalent to
  //    b.PermutedCopyToDenseVector(row_perm_, num_rows, x);
  // but it also computes the first column index which does not correspond to an
  // identity column of lower_ thus exploiting a bit the hyper-sparsity
  // of b.
  ColIndex first_column_to_consider(RowToColIndex(x->size()));
  const ColIndex limit = lower_.GetFirstNonIdentityColumn();
  for (const SparseColumn::Entry e : b) {
    const RowIndex permuted_row = row_perm_[e.row()];
    (*x)[permuted_row] = e.coefficient();

    // The second condition only works because lower_ only has 1.0
    // element on its diagonal.
    const ColIndex col = RowToColIndex(permuted_row);
    if (col < limit || lower_.ColumnIsDiagonalOnly(col)) {
      DCHECK_EQ(1.0, lower_.GetDiagonalCoefficient(col));
      continue;
    }
    first_column_to_consider = std::min(first_column_to_consider, col);
  }

  // TODO(user): Use an hyper-sparse solve here?
  lower_.LowerSolveStartingAt(first_column_to_consider, x);
}

void LuFactorization::RightSolveUWithNonZeros(
    DenseColumn* x, std::vector<RowIndex>* non_zeros) const {
  SCOPED_TIME_STAT(&stats_);
  if (is_identity_factorization_) {
    ComputeNonZeros(*x, non_zeros);
    return;
  }
  upper_.UpperSolveWithNonZeros(x, non_zeros);
  if (!col_perm_.empty()) {
    PermuteAndComputeNonZeros(inverse_col_perm_, &dense_zero_scratchpad_, x,
                              non_zeros);
  }
}

void LuFactorization::LeftSolveLWithNonZeros(
    DenseRow* y, ColIndexVector* non_zeros,
    DenseColumn* result_before_permutation) const {
  SCOPED_TIME_STAT(&stats_);
  if (is_identity_factorization_) {
    ComputeNonZeros(*y, non_zeros);
    return;
  }
  DenseColumn* const x = DenseRowAsColumn(y);
  RowIndex last_non_zero_row;
  lower_.TransposeLowerSolve(x, &last_non_zero_row);
  if (result_before_permutation == nullptr) {
    PermuteAndComputeNonZeros(inverse_row_perm_, &dense_zero_scratchpad_, x,
                              non_zeros);
  } else {
    // This computes the same thing as in the other branch but also keeps the
    // original x in result_before_permutation. Because of this, it is faster to
    // use a different algorithm.
    x->swap(*result_before_permutation);
    x->AssignToZero(inverse_row_perm_.size());
    non_zeros->clear();
    for (RowIndex row(0); row <= last_non_zero_row; ++row) {
      const Fractional value = (*result_before_permutation)[row];
      if (value != 0.0) {
        const RowIndex permuted_row = inverse_row_perm_[row];
        (*x)[permuted_row] = value;
        non_zeros->push_back(RowToColIndex(permuted_row));
      }
    }
  }
}

ColIndex LuFactorization::LeftSolveUForUnitRow(
    ColIndex col, DenseRow* y, std::vector<ColIndex>* non_zeros) const {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(IsAllZero(*y));
  DCHECK(non_zeros->empty());
  DenseColumn* const x = reinterpret_cast<DenseColumn*>(y);
  if (is_identity_factorization_) {
    (*y)[col] = 1.0;
    non_zeros->push_back(col);
    return col;
  }
  const ColIndex permuted_col = col_perm_.empty() ? col : col_perm_[col];
  (*y)[permuted_col] = 1.0;

  // Using the transposed matrix here is faster (even accounting the time to
  // construct it). Note the small optimization in case the inversion is
  // trivial.
  if (transpose_upper_.ColumnIsDiagonalOnly(permuted_col)) {
    (*y)[permuted_col] /= transpose_upper_.GetDiagonalCoefficient(permuted_col);
    non_zeros->push_back(permuted_col);
  } else {
    // TODO(user): use an hyper-sparse solve here?
    // Note that leaving non_zeros empty() will be interpreted as this column
    // is dense for the rest of the algorithm.
    transpose_upper_.LowerSolveStartingAt(permuted_col, x);
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

void LuFactorization::SparseLeftSolve(DenseRow* y,
                                      ColIndexVector* non_zero_indices) const {
  SCOPED_TIME_STAT(&stats_);
  if (is_identity_factorization_) return;
  const RowIndex num_rows = ColToRowIndex(y->size());
  dense_column_scratchpad_.AssignToZero(num_rows);
  for (const ColIndex col : *non_zero_indices) {
    dense_column_scratchpad_[ColToRowIndex(col_perm_[col])] = (*y)[col];

    // Note that we also clear y here.
    (*y)[col] = 0.0;
  }
  LeftSolveScratchpad();
  non_zero_indices->clear();
  for (RowIndex row(0); row < num_rows; ++row) {
    // Using the inverse_row_perm_ is faster but results in a non_zero_indices
    // which is not filled in order. This is bad for cache, but not a problem
    // for using the vector. Note as well that when the vector is really sparse,
    // the cache behavior is not perfect anyway, so this is not a big issue.
    const Fractional value = dense_column_scratchpad_[row];
    if (value != 0.0) {
      const ColIndex corresponding_col = RowToColIndex(inverse_row_perm_[row]);
      (*y)[corresponding_col] = value;
      non_zero_indices->push_back(corresponding_col);
    }
  }
}

double LuFactorization::GetFillInPercentage(const MatrixView& matrix) const {
  const int initial_num_entries = matrix.num_entries().value();
  const int lu_num_entries =
      (lower_.num_entries() + upper_.num_entries()).value();
  if (is_identity_factorization_ || initial_num_entries == 0) return 1.0;
  return static_cast<double>(lu_num_entries) /
         static_cast<double>(initial_num_entries);
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
      column_norm += fabs(right_hand_side[row]);
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
      row_sum[row] += fabs(right_hand_side[row]);
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
      const Fractional magnitude = fabs(e.coefficient());
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
