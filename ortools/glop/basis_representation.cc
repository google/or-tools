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


#include "ortools/glop/basis_representation.h"

#include "ortools/base/stl_util.h"
#include "ortools/glop/status.h"
#include "ortools/lp_data/lp_utils.h"

namespace operations_research {
namespace glop {

// --------------------------------------------------------
// EtaMatrix
// --------------------------------------------------------

const Fractional EtaMatrix::kSparseThreshold = 0.5;

EtaMatrix::EtaMatrix(ColIndex eta_col,
                     const std::vector<RowIndex>& eta_non_zeros,
                     DenseColumn* dense_eta)
    : eta_col_(eta_col),
      eta_col_coefficient_((*dense_eta)[ColToRowIndex(eta_col)]),
      eta_coeff_(),
      sparse_eta_coeff_() {
  DCHECK_NE(0.0, eta_col_coefficient_);
  eta_coeff_.swap(*dense_eta);
  eta_coeff_[ColToRowIndex(eta_col_)] = 0.0;

  // Only fill sparse_eta_coeff_ if it is sparse enough.
  if (eta_non_zeros.size() < kSparseThreshold * eta_coeff_.size().value()) {
    for (const RowIndex row : eta_non_zeros) {
      if (row == ColToRowIndex(eta_col)) continue;
      sparse_eta_coeff_.SetCoefficient(row, eta_coeff_[row]);
    }
    DCHECK(sparse_eta_coeff_.CheckNoDuplicates());
  }
}

EtaMatrix::~EtaMatrix() {}

void EtaMatrix::LeftSolve(DenseRow* y) const {
  RETURN_IF_NULL(y);
  DCHECK_EQ(RowToColIndex(eta_coeff_.size()), y->size());
  if (!sparse_eta_coeff_.IsEmpty()) {
    LeftSolveWithSparseEta(y);
  } else {
    LeftSolveWithDenseEta(y);
  }
}

void EtaMatrix::RightSolve(DenseColumn* d) const {
  RETURN_IF_NULL(d);
  DCHECK_EQ(eta_coeff_.size(), d->size());

  // Nothing to do if 'a' is zero at position eta_row.
  // This exploits the possible sparsity of the column 'a'.
  if ((*d)[ColToRowIndex(eta_col_)] == 0.0) return;
  if (!sparse_eta_coeff_.IsEmpty()) {
    RightSolveWithSparseEta(d);
  } else {
    RightSolveWithDenseEta(d);
  }
}

void EtaMatrix::SparseLeftSolve(DenseRow* y, ColIndexVector* pos) const {
  RETURN_IF_NULL(y);
  DCHECK_EQ(RowToColIndex(eta_coeff_.size()), y->size());

  Fractional y_value = (*y)[eta_col_];
  bool is_eta_col_in_pos = false;
  const int size = pos->size();
  for (int i = 0; i < size; ++i) {
    const ColIndex col = (*pos)[i];
    const RowIndex row = ColToRowIndex(col);
    if (col == eta_col_) {
      is_eta_col_in_pos = true;
      continue;
    }
    y_value -= (*y)[col] * eta_coeff_[row];
  }

  (*y)[eta_col_] = y_value / eta_col_coefficient_;

  // We add the new non-zero position if it wasn't already there.
  if (!is_eta_col_in_pos) pos->push_back(eta_col_);
}

void EtaMatrix::LeftSolveWithDenseEta(DenseRow* y) const {
  Fractional y_value = (*y)[eta_col_];
  const RowIndex num_rows(eta_coeff_.size());
  for (RowIndex row(0); row < num_rows; ++row) {
    y_value -= (*y)[RowToColIndex(row)] * eta_coeff_[row];
  }
  (*y)[eta_col_] = y_value / eta_col_coefficient_;
}

void EtaMatrix::LeftSolveWithSparseEta(DenseRow* y) const {
  Fractional y_value = (*y)[eta_col_];
  for (const SparseColumn::Entry e : sparse_eta_coeff_) {
    y_value -= (*y)[RowToColIndex(e.row())] * e.coefficient();
  }
  (*y)[eta_col_] = y_value / eta_col_coefficient_;
}

void EtaMatrix::RightSolveWithDenseEta(DenseColumn* d) const {
  const RowIndex eta_row = ColToRowIndex(eta_col_);
  const Fractional coeff = (*d)[eta_row] / eta_col_coefficient_;
  const RowIndex num_rows(eta_coeff_.size());
  for (RowIndex row(0); row < num_rows; ++row) {
    (*d)[row] -= eta_coeff_[row] * coeff;
  }
  (*d)[eta_row] = coeff;
}

void EtaMatrix::RightSolveWithSparseEta(DenseColumn* d) const {
  const RowIndex eta_row = ColToRowIndex(eta_col_);
  const Fractional coeff = (*d)[eta_row] / eta_col_coefficient_;
  for (const SparseColumn::Entry e : sparse_eta_coeff_) {
    (*d)[e.row()] -= e.coefficient() * coeff;
  }
  (*d)[eta_row] = coeff;
}

// --------------------------------------------------------
// EtaFactorization
// --------------------------------------------------------
EtaFactorization::EtaFactorization() : eta_matrix_() {}

EtaFactorization::~EtaFactorization() { Clear(); }

void EtaFactorization::Clear() { STLDeleteElements(&eta_matrix_); }

void EtaFactorization::Update(ColIndex entering_col,
                              RowIndex leaving_variable_row,
                              const std::vector<RowIndex>& eta_non_zeros,
                              DenseColumn* dense_eta) {
  const ColIndex leaving_variable_col = RowToColIndex(leaving_variable_row);
  EtaMatrix* const eta_factorization =
      new EtaMatrix(leaving_variable_col, eta_non_zeros, dense_eta);
  eta_matrix_.push_back(eta_factorization);
}

void EtaFactorization::LeftSolve(DenseRow* y) const {
  RETURN_IF_NULL(y);
  for (int i = eta_matrix_.size() - 1; i >= 0; --i) {
    eta_matrix_[i]->LeftSolve(y);
  }
}

void EtaFactorization::SparseLeftSolve(DenseRow* y, ColIndexVector* pos) const {
  RETURN_IF_NULL(y);
  for (int i = eta_matrix_.size() - 1; i >= 0; --i) {
    eta_matrix_[i]->SparseLeftSolve(y, pos);
  }
}

void EtaFactorization::RightSolve(DenseColumn* d) const {
  RETURN_IF_NULL(d);
  const size_t num_eta_matrices = eta_matrix_.size();
  for (int i = 0; i < num_eta_matrices; ++i) {
    eta_matrix_[i]->RightSolve(d);
  }
}

// --------------------------------------------------------
// BasisFactorization
// --------------------------------------------------------
BasisFactorization::BasisFactorization(const MatrixView& matrix,
                                       const RowToColMapping& basis)
    : stats_(),
      matrix_(matrix),
      basis_(basis),
      tau_is_computed_(false),
      max_num_updates_(0),
      num_updates_(0),
      eta_factorization_(),
      lu_factorization_(),
      deterministic_time_(0.0) {
  SetParameters(parameters_);
}

BasisFactorization::~BasisFactorization() {}

void BasisFactorization::Clear() {
  SCOPED_TIME_STAT(&stats_);
  num_updates_ = 0;
  tau_computation_can_be_optimized_ = false;
  eta_factorization_.Clear();
  lu_factorization_.Clear();
  rank_one_factorization_.Clear();
  storage_.Reset(matrix_.num_rows());
  right_storage_.Reset(matrix_.num_rows());
  left_pool_mapping_.assign(matrix_.num_cols(), kInvalidCol);
  right_pool_mapping_.assign(matrix_.num_cols(), kInvalidCol);
}

Status BasisFactorization::Initialize() {
  SCOPED_TIME_STAT(&stats_);
  Clear();
  if (IsIdentityBasis()) return Status::OK();
  MatrixView basis_matrix;
  basis_matrix.PopulateFromBasis(matrix_, basis_);
  return lu_factorization_.ComputeFactorization(basis_matrix);
}

bool BasisFactorization::IsRefactorized() const { return num_updates_ == 0; }

Status BasisFactorization::Refactorize() {
  if (IsRefactorized()) return Status::OK();
  return ForceRefactorization();
}

Status BasisFactorization::ForceRefactorization() {
  SCOPED_TIME_STAT(&stats_);
  stats_.refactorization_interval.Add(num_updates_);
  Clear();
  MatrixView basis_matrix;
  basis_matrix.PopulateFromBasis(matrix_, basis_);
  const Status status = lu_factorization_.ComputeFactorization(basis_matrix);

  const double kLuComplexityFactor = 10;
  deterministic_time_ +=
      kLuComplexityFactor * DeterministicTimeForFpOperations(
                                lu_factorization_.NumberOfEntries().value());
  return status;
}

// This update formula can be derived by:
// e = unit vector on the leaving_variable_row
// new B = L.U + (matrix.column(entering_col) - B.e).e^T
// new B = L.U + L.L^{-1}.(matrix.column(entering_col) - B.e).e^T.U^{-1}.U
// new B = L.(Identity +
//     (right_update_vector - U.column(leaving_column)).left_update_vector).U
// new B = L.RankOneUpdateElementatyMatrix(
//    right_update_vector - U.column(leaving_column), left_update_vector)
Status BasisFactorization::MiddleProductFormUpdate(
    ColIndex entering_col, RowIndex leaving_variable_row) {
  const ColIndex right_index = right_pool_mapping_[entering_col];
  const ColIndex left_index =
      left_pool_mapping_[RowToColIndex(leaving_variable_row)];
  if (right_index == kInvalidCol || left_index == kInvalidCol) {
    VLOG(0) << "One update vector is missing!!!";
    return ForceRefactorization();
  }

  // TODO(user): create a class for these operations.
  // Initialize scratchpad_ with the right update vector.
  DCHECK(IsAllZero(scratchpad_));
  scratchpad_.resize(right_storage_.num_rows(), 0.0);
  for (const EntryIndex i : right_storage_.Column(right_index)) {
    const RowIndex row = right_storage_.EntryRow(i);
    scratchpad_[row] = right_storage_.EntryCoefficient(i);
    scratchpad_non_zeros_.push_back(row);
  }
  // Subtract the column of U from scratchpad_.
  const SparseColumn& column_of_u =
      lu_factorization_.GetColumnOfU(RowToColIndex(leaving_variable_row));
  for (const SparseColumn::Entry e : column_of_u) {
    scratchpad_[e.row()] -= e.coefficient();
    scratchpad_non_zeros_.push_back(e.row());
  }

  // Creates the new rank one update matrix and update the factorization.
  const Fractional scalar_product =
      storage_.ColumnScalarProduct(left_index, Transpose(scratchpad_));
  const ColIndex u_index = storage_.AddAndClearColumnWithNonZeros(
      &scratchpad_, &scratchpad_non_zeros_);
  RankOneUpdateElementaryMatrix elementary_update_matrix(
      &storage_, u_index, left_index, scalar_product);
  if (elementary_update_matrix.IsSingular()) {
    GLOP_RETURN_AND_LOG_ERROR(Status::ERROR_LU, "Degenerate rank-one update.");
  }
  rank_one_factorization_.Update(elementary_update_matrix);
  return Status::OK();
}

Status BasisFactorization::Update(ColIndex entering_col,
                                  RowIndex leaving_variable_row,
                                  const std::vector<RowIndex>& eta_non_zeros,
                                  DenseColumn* dense_eta) {
  if (num_updates_ < max_num_updates_) {
    SCOPED_TIME_STAT(&stats_);
    if (use_middle_product_form_update_) {
      GLOP_RETURN_IF_ERROR(
          MiddleProductFormUpdate(entering_col, leaving_variable_row));
    } else {
      eta_factorization_.Update(entering_col, leaving_variable_row,
                                eta_non_zeros, dense_eta);
    }
    ++num_updates_;
    tau_computation_can_be_optimized_ = false;
    return Status::OK();
  }
  return ForceRefactorization();
}

void BasisFactorization::LeftSolve(DenseRow* y) const {
  SCOPED_TIME_STAT(&stats_);
  RETURN_IF_NULL(y);
  BumpDeterministicTimeForSolve(matrix_.num_rows().value());
  if (use_middle_product_form_update_) {
    lu_factorization_.LeftSolveU(y);
    rank_one_factorization_.LeftSolve(y);
    lu_factorization_.LeftSolveL(y);
  } else {
    eta_factorization_.LeftSolve(y);
    lu_factorization_.LeftSolve(y);
  }
}

void BasisFactorization::LeftSolveWithNonZeros(
    DenseRow* y, ColIndexVector* non_zeros) const {
  SCOPED_TIME_STAT(&stats_);
  RETURN_IF_NULL(y);
  BumpDeterministicTimeForSolve(matrix_.num_rows().value());
  if (use_middle_product_form_update_) {
    lu_factorization_.LeftSolveUWithNonZeros(y, non_zeros);
    rank_one_factorization_.LeftSolveWithNonZeros(y, non_zeros);
    lu_factorization_.LeftSolveLWithNonZeros(y, non_zeros, nullptr);
  } else {
    eta_factorization_.LeftSolve(y);
    lu_factorization_.LeftSolve(y);
    ComputeNonZeros(*y, non_zeros);
  }
}

void BasisFactorization::RightSolve(DenseColumn* d) const {
  SCOPED_TIME_STAT(&stats_);
  RETURN_IF_NULL(d);
  BumpDeterministicTimeForSolve(matrix_.num_rows().value());
  if (use_middle_product_form_update_) {
    lu_factorization_.RightSolveL(d);
    rank_one_factorization_.RightSolve(d);
    lu_factorization_.RightSolveU(d);
  } else {
    lu_factorization_.RightSolve(d);
    eta_factorization_.RightSolve(d);
  }
}

void BasisFactorization::RightSolveWithNonZeros(
    DenseColumn* d, std::vector<RowIndex>* non_zeros) const {
  SCOPED_TIME_STAT(&stats_);
  RETURN_IF_NULL(d);
  BumpDeterministicTimeForSolve(non_zeros->size());
  if (use_middle_product_form_update_) {
    lu_factorization_.RightSolveL(d);
    rank_one_factorization_.RightSolve(d);

    // We need to clear non-zeros because at this point in the code, they don't
    // correspond to the zeros of d. An empty non_zeros means that
    // RightSolveWithNonZeros() will recompute them.
    non_zeros->clear();
    lu_factorization_.RightSolveUWithNonZeros(d, non_zeros);
  } else {
    lu_factorization_.RightSolve(d);
    eta_factorization_.RightSolve(d);
    ComputeNonZeros(*d, non_zeros);
    return;
  }
}

DenseColumn* BasisFactorization::RightSolveForTau(ScatteredColumnReference a)
    const {
  SCOPED_TIME_STAT(&stats_);
  BumpDeterministicTimeForSolve(matrix_.num_rows().value());
  if (use_middle_product_form_update_) {
    if (tau_computation_can_be_optimized_) {
      // Once used, the intermediate result is overriden, so RightSolveForTau()
      // can no longer use the optimized algorithm.
      tau_computation_can_be_optimized_ = false;
      lu_factorization_.RightSolveLWithPermutedInput(a.dense_column, &tau_);
      tau_non_zeros_.clear();
    } else {
      if (tau_non_zeros_.empty()) {
        tau_.assign(matrix_.num_rows(), 0);
      } else {
        tau_.resize(matrix_.num_rows(), 0);
        for (const RowIndex row : tau_non_zeros_) {
          tau_[row] = 0.0;
        }
      }
      lu_factorization_.RightSolveLForScatteredColumn(a, &tau_,
                                                      &tau_non_zeros_);
    }
    rank_one_factorization_.RightSolveWithNonZeros(&tau_, &tau_non_zeros_);
    lu_factorization_.RightSolveUWithNonZeros(&tau_, &tau_non_zeros_);
  } else {
    tau_ = a.dense_column;
    lu_factorization_.RightSolve(&tau_);
    eta_factorization_.RightSolve(&tau_);
  }
  tau_is_computed_ = true;
  return &tau_;
}

void BasisFactorization::LeftSolveForUnitRow(ColIndex j, DenseRow* y,
                                             ColIndexVector* non_zeros) const {
  SCOPED_TIME_STAT(&stats_);
  RETURN_IF_NULL(y);
  BumpDeterministicTimeForSolve(1);
  ClearAndResizeVectorWithNonZeros(RowToColIndex(matrix_.num_rows()), y,
                                   non_zeros);

  if (!use_middle_product_form_update_) {
    (*y)[j] = 1.0;
    non_zeros->push_back(j);
    eta_factorization_.SparseLeftSolve(y, non_zeros);
    lu_factorization_.SparseLeftSolve(y, non_zeros);
    return;
  }

  // If the leaving index is the same, we can reuse the column! Note also that
  // since we do a left solve for a unit row using an upper triangular matrix,
  // all positions in front of the unit will be zero (modulo the column
  // permutation).
  if (left_pool_mapping_[j] == kInvalidCol) {
    const ColIndex start =
        lu_factorization_.LeftSolveUForUnitRow(j, y, non_zeros);
    if (non_zeros->empty()) {
      left_pool_mapping_[j] =
          storage_.AddDenseColumnPrefix(Transpose(*y), ColToRowIndex(start));
    } else {
      left_pool_mapping_[j] = storage_.AddDenseColumnWithNonZeros(
          Transpose(*y), *reinterpret_cast<RowIndexVector*>(non_zeros));
    }
  } else {
    DenseColumn* const x = reinterpret_cast<DenseColumn*>(y);
    RowIndexVector* const nz = reinterpret_cast<RowIndexVector*>(non_zeros);
    storage_.ColumnCopyToClearedDenseColumnWithNonZeros(left_pool_mapping_[j],
                                                        x, nz);
  }

  rank_one_factorization_.LeftSolveWithNonZeros(y, non_zeros);

  // We only keep the intermediate result needed for the optimized tau_
  // computation if it was computed after the last time this was called.
  if (tau_is_computed_) {
    tau_is_computed_ = false;
    tau_computation_can_be_optimized_ =
        lu_factorization_.LeftSolveLWithNonZeros(y, non_zeros, &tau_);
    tau_non_zeros_.clear();
  } else {
    tau_computation_can_be_optimized_ = false;
    lu_factorization_.LeftSolveLWithNonZeros(y, non_zeros, nullptr);
  }
}

void BasisFactorization::RightSolveForProblemColumn(
    ColIndex col, DenseColumn* d, std::vector<RowIndex>* non_zeros) const {
  SCOPED_TIME_STAT(&stats_);
  RETURN_IF_NULL(d);
  BumpDeterministicTimeForSolve(matrix_.column(col).num_entries().value());
  if (!use_middle_product_form_update_) {
    lu_factorization_.SparseRightSolve(matrix_.column(col), matrix_.num_rows(),
                                       d);
    eta_factorization_.RightSolve(d);
    ComputeNonZeros(*d, non_zeros);
    return;
  }

  // TODO(user): if right_pool_mapping_[col] != kInvalidCol, we can reuse it and
  // just apply the last rank one update since it was computed.
  ClearAndResizeVectorWithNonZeros(matrix_.num_rows(), d, non_zeros);
  lu_factorization_.RightSolveLForSparseColumn(matrix_.column(col), d,
                                               non_zeros);
  rank_one_factorization_.RightSolveWithNonZeros(d, non_zeros);
  if (col >= right_pool_mapping_.size()) {
    // This is needed because when we do an incremental solve with only new
    // columns, we still reuse the current factorization without calling
    // Refactorize() which would have resized this vector.
    right_pool_mapping_.resize(col + 1, kInvalidCol);
  }
  if (non_zeros->empty()) {
    right_pool_mapping_[col] = right_storage_.AddDenseColumn(*d);
  } else {
    // The sort is needed if we want to have the same behavior for the sparse or
    // hyper-sparse version.
    std::sort(non_zeros->begin(), non_zeros->end());
    right_pool_mapping_[col] =
        right_storage_.AddDenseColumnWithNonZeros(*d, *non_zeros);
  }
  lu_factorization_.RightSolveUWithNonZeros(d, non_zeros);
}

Fractional BasisFactorization::RightSolveSquaredNorm(const SparseColumn& a)
    const {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(IsRefactorized());
  BumpDeterministicTimeForSolve(a.num_entries().value());
  return lu_factorization_.RightSolveSquaredNorm(a);
}

Fractional BasisFactorization::DualEdgeSquaredNorm(RowIndex row) const {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(IsRefactorized());
  BumpDeterministicTimeForSolve(1);
  return lu_factorization_.DualEdgeSquaredNorm(row);
}

bool BasisFactorization::IsIdentityBasis() const {
  const RowIndex num_rows = matrix_.num_rows();
  for (RowIndex row(0); row < num_rows; ++row) {
    const ColIndex col = basis_[row];
    if (matrix_.column(col).num_entries().value() != 1) return false;
    const Fractional coeff = matrix_.column(col).GetFirstCoefficient();
    const RowIndex entry_row = matrix_.column(col).GetFirstRow();
    if (entry_row != row || coeff != 1.0) return false;
  }
  return true;
}

Fractional BasisFactorization::ComputeOneNorm() const {
  if (IsIdentityBasis()) return 1.0;
  MatrixView basis_matrix;
  basis_matrix.PopulateFromBasis(matrix_, basis_);
  return basis_matrix.ComputeOneNorm();
}

Fractional BasisFactorization::ComputeInfinityNorm() const {
  if (IsIdentityBasis()) return 1.0;
  MatrixView basis_matrix;
  basis_matrix.PopulateFromBasis(matrix_, basis_);
  return basis_matrix.ComputeInfinityNorm();
}

// TODO(user): try to merge the computation of the norm of inverses
// with that of MatrixView. Maybe use a wrapper class for InverseMatrix.

Fractional BasisFactorization::ComputeInverseOneNorm() const {
  if (IsIdentityBasis()) return 1.0;
  const RowIndex num_rows = matrix_.num_rows();
  const ColIndex num_cols = RowToColIndex(num_rows);
  Fractional norm = 0.0;
  for (ColIndex col(0); col < num_cols; ++col) {
    DenseColumn right_hand_side(num_rows, 0.0);
    right_hand_side[ColToRowIndex(col)] = 1.0;
    // Get a column of the matrix inverse.
    RightSolve(&right_hand_side);
    Fractional column_norm = 0.0;
    // Compute sum_i |inverse_ij|.
    for (RowIndex row(0); row < num_rows; ++row) {
      column_norm += std::abs(right_hand_side[row]);
    }
    // Compute max_j sum_i |inverse_ij|
    norm = std::max(norm, column_norm);
  }
  return norm;
}

Fractional BasisFactorization::ComputeInverseInfinityNorm() const {
  if (IsIdentityBasis()) return 1.0;
  const RowIndex num_rows = matrix_.num_rows();
  const ColIndex num_cols = RowToColIndex(num_rows);
  DenseColumn row_sum(num_rows, 0.0);
  for (ColIndex col(0); col < num_cols; ++col) {
    DenseColumn right_hand_side(num_rows, 0.0);
    right_hand_side[ColToRowIndex(col)] = 1.0;
    // Get a column of the matrix inverse.
    RightSolve(&right_hand_side);
    // Compute sum_j |inverse_ij|.
    for (RowIndex row(0); row < num_rows; ++row) {
      row_sum[row] += std::abs(right_hand_side[row]);
    }
  }
  // Compute max_i sum_j |inverse_ij|
  Fractional norm = 0.0;
  for (RowIndex row(0); row < num_rows; ++row) {
    norm = std::max(norm, row_sum[row]);
  }
  return norm;
}

Fractional BasisFactorization::ComputeOneNormConditionNumber() const {
  if (IsIdentityBasis()) return 1.0;
  return ComputeOneNorm() * ComputeInverseOneNorm();
}

Fractional BasisFactorization::ComputeInfinityNormConditionNumber() const {
  if (IsIdentityBasis()) return 1.0;
  return ComputeInfinityNorm() * ComputeInverseInfinityNorm();
}

double BasisFactorization::DeterministicTime() const {
  return deterministic_time_;
}

void BasisFactorization::BumpDeterministicTimeForSolve(int num_entries) const {
  // TODO(user): Spend more time finding a good approximation here.
  if (matrix_.num_rows().value() == 0) return;
  const double density = static_cast<double>(num_entries) /
                         static_cast<double>(matrix_.num_rows().value());
  deterministic_time_ +=
      (1.0 + density) * DeterministicTimeForFpOperations(
                            lu_factorization_.NumberOfEntries().value()) +
      DeterministicTimeForFpOperations(
          rank_one_factorization_.num_entries().value());
}

}  // namespace glop
}  // namespace operations_research
