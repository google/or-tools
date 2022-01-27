// Copyright 2010-2021 Google LLC
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

EtaMatrix::EtaMatrix(ColIndex eta_col, const ScatteredColumn& direction)
    : eta_col_(eta_col),
      eta_col_coefficient_(direction[ColToRowIndex(eta_col)]),
      eta_coeff_(),
      sparse_eta_coeff_() {
  DCHECK_NE(0.0, eta_col_coefficient_);
  eta_coeff_ = direction.values;
  eta_coeff_[ColToRowIndex(eta_col_)] = 0.0;

  // Only fill sparse_eta_coeff_ if it is sparse enough.
  if (direction.non_zeros.size() <
      kSparseThreshold * eta_coeff_.size().value()) {
    for (const RowIndex row : direction.non_zeros) {
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

void EtaFactorization::Clear() { gtl::STLDeleteElements(&eta_matrix_); }

void EtaFactorization::Update(ColIndex entering_col,
                              RowIndex leaving_variable_row,
                              const ScatteredColumn& direction) {
  const ColIndex leaving_variable_col = RowToColIndex(leaving_variable_row);
  EtaMatrix* const eta_factorization =
      new EtaMatrix(leaving_variable_col, direction);
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
BasisFactorization::BasisFactorization(
    const CompactSparseMatrix* compact_matrix, const RowToColMapping* basis)
    : stats_(),
      compact_matrix_(*compact_matrix),
      basis_(*basis),
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
  storage_.Reset(compact_matrix_.num_rows());
  right_storage_.Reset(compact_matrix_.num_rows());
  left_pool_mapping_.clear();
  right_pool_mapping_.clear();
}

Status BasisFactorization::Initialize() {
  SCOPED_TIME_STAT(&stats_);
  Clear();
  if (IsIdentityBasis()) return Status::OK();
  return ComputeFactorization();
}

RowToColMapping BasisFactorization::ComputeInitialBasis(
    const std::vector<ColIndex>& candidates) {
  const RowToColMapping basis =
      lu_factorization_.ComputeInitialBasis(compact_matrix_, candidates);
  deterministic_time_ +=
      lu_factorization_.DeterministicTimeOfLastFactorization();
  return basis;
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
  return ComputeFactorization();
}

Status BasisFactorization::ComputeFactorization() {
  CompactSparseMatrixView basis_matrix(&compact_matrix_, &basis_);
  const Status status = lu_factorization_.ComputeFactorization(basis_matrix);
  last_factorization_deterministic_time_ =
      lu_factorization_.DeterministicTimeOfLastFactorization();
  deterministic_time_ += last_factorization_deterministic_time_;
  rank_one_factorization_.ResetDeterministicTime();
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
  const ColIndex right_index = entering_col < right_pool_mapping_.size()
                                   ? right_pool_mapping_[entering_col]
                                   : kInvalidCol;
  const ColIndex left_index =
      RowToColIndex(leaving_variable_row) < left_pool_mapping_.size()
          ? left_pool_mapping_[RowToColIndex(leaving_variable_row)]
          : kInvalidCol;
  if (right_index == kInvalidCol || left_index == kInvalidCol) {
    LOG(INFO) << "One update vector is missing!!!";
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
                                  const ScatteredColumn& direction) {
  // Note that in addition to the logic here, we also refactorize when we detect
  // numerical imprecisions. There is various tests for that during an
  // iteration.
  if (num_updates_ >= max_num_updates_) {
    if (!parameters_.dynamically_adjust_refactorization_period()) {
      return ForceRefactorization();
    }

    // We try to equilibrate the factorization time with the EXTRA solve time
    // incurred since the last factorization.
    //
    // Note(user): The deterministic time is not really super precise for now.
    // We tend to undercount the factorization, but this tends to favorize more
    // refactorization which is good for numerical stability.
    if (last_factorization_deterministic_time_ <
        rank_one_factorization_.DeterministicTimeSinceLastReset()) {
      return ForceRefactorization();
    }
  }

  // Note(user): in some rare case (to investigate!) MiddleProductFormUpdate()
  // will trigger a full refactorization. Because of this, it is important to
  // increment num_updates_ first as this counter is used by IsRefactorized().
  SCOPED_TIME_STAT(&stats_);
  ++num_updates_;
  if (use_middle_product_form_update_) {
    GLOP_RETURN_IF_ERROR(
        MiddleProductFormUpdate(entering_col, leaving_variable_row));
  } else {
    eta_factorization_.Update(entering_col, leaving_variable_row, direction);
  }
  tau_computation_can_be_optimized_ = false;
  return Status::OK();
}

void BasisFactorization::LeftSolve(ScatteredRow* y) const {
  SCOPED_TIME_STAT(&stats_);
  RETURN_IF_NULL(y);
  if (use_middle_product_form_update_) {
    lu_factorization_.LeftSolveUWithNonZeros(y);
    rank_one_factorization_.LeftSolveWithNonZeros(y);
    lu_factorization_.LeftSolveLWithNonZeros(y);
    y->SortNonZerosIfNeeded();
  } else {
    y->non_zeros.clear();
    eta_factorization_.LeftSolve(&y->values);
    lu_factorization_.LeftSolve(&y->values);
  }
  BumpDeterministicTimeForSolve(y->NumNonZerosEstimate());
}

void BasisFactorization::RightSolve(ScatteredColumn* d) const {
  SCOPED_TIME_STAT(&stats_);
  RETURN_IF_NULL(d);
  if (use_middle_product_form_update_) {
    lu_factorization_.RightSolveLWithNonZeros(d);
    rank_one_factorization_.RightSolveWithNonZeros(d);
    lu_factorization_.RightSolveUWithNonZeros(d);
    d->SortNonZerosIfNeeded();
  } else {
    d->non_zeros.clear();
    lu_factorization_.RightSolve(&d->values);
    eta_factorization_.RightSolve(&d->values);
  }
  BumpDeterministicTimeForSolve(d->NumNonZerosEstimate());
}

const DenseColumn& BasisFactorization::RightSolveForTau(
    const ScatteredColumn& a) const {
  SCOPED_TIME_STAT(&stats_);
  if (use_middle_product_form_update_) {
    if (tau_computation_can_be_optimized_) {
      // Once used, the intermediate result is overwritten, so
      // RightSolveForTau() can no longer use the optimized algorithm.
      tau_computation_can_be_optimized_ = false;
      lu_factorization_.RightSolveLWithPermutedInput(a.values, &tau_);
    } else {
      ClearAndResizeVectorWithNonZeros(compact_matrix_.num_rows(), &tau_);
      lu_factorization_.RightSolveLForScatteredColumn(a, &tau_);
    }
    rank_one_factorization_.RightSolveWithNonZeros(&tau_);
    lu_factorization_.RightSolveUWithNonZeros(&tau_);
  } else {
    tau_.non_zeros.clear();
    tau_.values = a.values;
    lu_factorization_.RightSolve(&tau_.values);
    eta_factorization_.RightSolve(&tau_.values);
  }
  tau_is_computed_ = true;
  BumpDeterministicTimeForSolve(tau_.NumNonZerosEstimate());
  return tau_.values;
}

void BasisFactorization::LeftSolveForUnitRow(ColIndex j,
                                             ScatteredRow* y) const {
  SCOPED_TIME_STAT(&stats_);
  RETURN_IF_NULL(y);
  ClearAndResizeVectorWithNonZeros(RowToColIndex(compact_matrix_.num_rows()),
                                   y);
  if (!use_middle_product_form_update_) {
    (*y)[j] = 1.0;
    y->non_zeros.push_back(j);
    eta_factorization_.SparseLeftSolve(&y->values, &y->non_zeros);
    lu_factorization_.LeftSolve(&y->values);
    BumpDeterministicTimeForSolve(y->NumNonZerosEstimate());
    return;
  }

  // If the leaving index is the same, we can reuse the column! Note also that
  // since we do a left solve for a unit row using an upper triangular matrix,
  // all positions in front of the unit will be zero (modulo the column
  // permutation).
  if (j >= left_pool_mapping_.size()) {
    left_pool_mapping_.resize(j + 1, kInvalidCol);
  }
  if (left_pool_mapping_[j] == kInvalidCol) {
    const ColIndex start = lu_factorization_.LeftSolveUForUnitRow(j, y);
    if (y->non_zeros.empty()) {
      left_pool_mapping_[j] = storage_.AddDenseColumnPrefix(
          Transpose(y->values), ColToRowIndex(start));
    } else {
      left_pool_mapping_[j] = storage_.AddDenseColumnWithNonZeros(
          Transpose(y->values),
          *reinterpret_cast<RowIndexVector*>(&y->non_zeros));
    }
  } else {
    DenseColumn* const x = reinterpret_cast<DenseColumn*>(y);
    RowIndexVector* const nz = reinterpret_cast<RowIndexVector*>(&y->non_zeros);
    storage_.ColumnCopyToClearedDenseColumnWithNonZeros(left_pool_mapping_[j],
                                                        x, nz);
  }

  rank_one_factorization_.LeftSolveWithNonZeros(y);

  // We only keep the intermediate result needed for the optimized tau_
  // computation if it was computed after the last time this was called.
  if (tau_is_computed_) {
    tau_computation_can_be_optimized_ =
        lu_factorization_.LeftSolveLWithNonZeros(y, &tau_);
  } else {
    tau_computation_can_be_optimized_ = false;
    lu_factorization_.LeftSolveLWithNonZeros(y);
  }
  tau_is_computed_ = false;
  y->SortNonZerosIfNeeded();
  BumpDeterministicTimeForSolve(y->NumNonZerosEstimate());
}

void BasisFactorization::TemporaryLeftSolveForUnitRow(ColIndex j,
                                                      ScatteredRow* y) const {
  CHECK(IsRefactorized());
  SCOPED_TIME_STAT(&stats_);
  RETURN_IF_NULL(y);
  ClearAndResizeVectorWithNonZeros(RowToColIndex(compact_matrix_.num_rows()),
                                   y);
  lu_factorization_.LeftSolveUForUnitRow(j, y);
  lu_factorization_.LeftSolveLWithNonZeros(y);
  y->SortNonZerosIfNeeded();
  BumpDeterministicTimeForSolve(y->NumNonZerosEstimate());
}

void BasisFactorization::RightSolveForProblemColumn(ColIndex col,
                                                    ScatteredColumn* d) const {
  SCOPED_TIME_STAT(&stats_);
  RETURN_IF_NULL(d);
  ClearAndResizeVectorWithNonZeros(compact_matrix_.num_rows(), d);

  if (!use_middle_product_form_update_) {
    compact_matrix_.ColumnCopyToClearedDenseColumn(col, &d->values);
    lu_factorization_.RightSolve(&d->values);
    eta_factorization_.RightSolve(&d->values);
    BumpDeterministicTimeForSolve(d->NumNonZerosEstimate());
    return;
  }

  // TODO(user): if right_pool_mapping_[col] != kInvalidCol, we can reuse it and
  // just apply the last rank one update since it was computed.
  lu_factorization_.RightSolveLForColumnView(compact_matrix_.column(col), d);
  rank_one_factorization_.RightSolveWithNonZeros(d);
  if (col >= right_pool_mapping_.size()) {
    right_pool_mapping_.resize(col + 1, kInvalidCol);
  }
  if (d->non_zeros.empty()) {
    right_pool_mapping_[col] = right_storage_.AddDenseColumn(d->values);
  } else {
    // The sort is needed if we want to have the same behavior for the sparse or
    // hyper-sparse version.
    std::sort(d->non_zeros.begin(), d->non_zeros.end());
    right_pool_mapping_[col] =
        right_storage_.AddDenseColumnWithNonZeros(d->values, d->non_zeros);
  }
  lu_factorization_.RightSolveUWithNonZeros(d);
  d->SortNonZerosIfNeeded();
  BumpDeterministicTimeForSolve(d->NumNonZerosEstimate());
}

Fractional BasisFactorization::RightSolveSquaredNorm(
    const ColumnView& a) const {
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
  const RowIndex num_rows = compact_matrix_.num_rows();
  for (RowIndex row(0); row < num_rows; ++row) {
    const ColIndex col = basis_[row];
    if (compact_matrix_.column(col).num_entries().value() != 1) return false;
    const Fractional coeff = compact_matrix_.column(col).GetFirstCoefficient();
    const RowIndex entry_row = compact_matrix_.column(col).GetFirstRow();
    if (entry_row != row || coeff != 1.0) return false;
  }
  return true;
}

Fractional BasisFactorization::ComputeOneNorm() const {
  if (IsIdentityBasis()) return 1.0;
  CompactSparseMatrixView basis_matrix(&compact_matrix_, &basis_);
  return basis_matrix.ComputeOneNorm();
}

Fractional BasisFactorization::ComputeInfinityNorm() const {
  if (IsIdentityBasis()) return 1.0;
  CompactSparseMatrixView basis_matrix(&compact_matrix_, &basis_);
  return basis_matrix.ComputeInfinityNorm();
}

// TODO(user): try to merge the computation of the norm of inverses
// with that of MatrixView. Maybe use a wrapper class for InverseMatrix.

Fractional BasisFactorization::ComputeInverseOneNorm() const {
  if (IsIdentityBasis()) return 1.0;
  const RowIndex num_rows = compact_matrix_.num_rows();
  const ColIndex num_cols = RowToColIndex(num_rows);
  Fractional norm = 0.0;
  for (ColIndex col(0); col < num_cols; ++col) {
    ScatteredColumn right_hand_side;
    right_hand_side.values.AssignToZero(num_rows);
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
  const RowIndex num_rows = compact_matrix_.num_rows();
  const ColIndex num_cols = RowToColIndex(num_rows);
  DenseColumn row_sum(num_rows, 0.0);
  for (ColIndex col(0); col < num_cols; ++col) {
    ScatteredColumn right_hand_side;
    right_hand_side.values.AssignToZero(num_rows);
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

Fractional BasisFactorization::ComputeInfinityNormConditionNumberUpperBound()
    const {
  if (IsIdentityBasis()) return 1.0;
  BumpDeterministicTimeForSolve(compact_matrix_.num_rows().value());
  return ComputeInfinityNorm() *
         lu_factorization_.ComputeInverseInfinityNormUpperBound();
}

double BasisFactorization::DeterministicTime() const {
  return deterministic_time_;
}

void BasisFactorization::BumpDeterministicTimeForSolve(int num_entries) const {
  // TODO(user): Spend more time finding a good approximation here.
  if (compact_matrix_.num_rows().value() == 0) return;
  const double density =
      static_cast<double>(num_entries) /
      static_cast<double>(compact_matrix_.num_rows().value());
  deterministic_time_ +=
      density * DeterministicTimeForFpOperations(
                    lu_factorization_.NumberOfEntries().value()) +
      DeterministicTimeForFpOperations(
          rank_one_factorization_.num_entries().value());
}

}  // namespace glop
}  // namespace operations_research
