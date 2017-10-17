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

#include "ortools/glop/primal_edge_norms.h"

#ifdef OMP
#include <omp.h>
#endif

#include "ortools/base/timer.h"
#include "ortools/lp_data/lp_utils.h"

namespace operations_research {
namespace glop {

PrimalEdgeNorms::PrimalEdgeNorms(const MatrixView& matrix,
                                 const CompactSparseMatrix& compact_matrix,
                                 const VariablesInfo& variables_info,
                                 const BasisFactorization& basis_factorization)
    : matrix_(matrix),
      compact_matrix_(compact_matrix),
      variables_info_(variables_info),
      basis_factorization_(basis_factorization),
      stats_(),
      recompute_edge_squared_norms_(true),
      reset_devex_weights_(true),
      edge_squared_norms_(),
      matrix_column_norms_(),
      devex_weights_(),
      direction_left_inverse_(),
      num_operations_(0) {}

void PrimalEdgeNorms::Clear() {
  SCOPED_TIME_STAT(&stats_);
  recompute_edge_squared_norms_ = true;
  reset_devex_weights_ = true;
}

bool PrimalEdgeNorms::NeedsBasisRefactorization() const {
  return recompute_edge_squared_norms_;
}

const DenseRow& PrimalEdgeNorms::GetEdgeSquaredNorms() {
  if (recompute_edge_squared_norms_) ComputeEdgeSquaredNorms();
  return edge_squared_norms_;
}

const DenseRow& PrimalEdgeNorms::GetDevexWeights() {
  if (reset_devex_weights_) ResetDevexWeights();
  return devex_weights_;
}

const DenseRow& PrimalEdgeNorms::GetMatrixColumnNorms() {
  if (matrix_column_norms_.empty()) ComputeMatrixColumnNorms();
  return matrix_column_norms_;
}

void PrimalEdgeNorms::TestEnteringEdgeNormPrecision(
    ColIndex entering_col, ScatteredColumnReference direction) {
  if (!recompute_edge_squared_norms_) {
    SCOPED_TIME_STAT(&stats_);
    // Recompute the squared norm of the edge used during this
    // iteration, i.e. the entering edge. Note the PreciseSquaredNorm()
    // since it is a small price to pay for an increased precision.
    const Fractional old_squared_norm = edge_squared_norms_[entering_col];
    const Fractional precise_squared_norm = 1.0 + PreciseSquaredNorm(direction);
    edge_squared_norms_[entering_col] = precise_squared_norm;

    const Fractional precise_norm = sqrt(precise_squared_norm);
    const Fractional estimated_edges_norm_accuracy =
        (precise_norm - sqrt(old_squared_norm)) / precise_norm;
    stats_.edges_norm_accuracy.Add(estimated_edges_norm_accuracy);
    if (std::abs(estimated_edges_norm_accuracy) >
        parameters_.recompute_edges_norm_threshold()) {
      VLOG(1) << "Recomputing edge norms: " << sqrt(precise_squared_norm)
              << " vs " << sqrt(old_squared_norm);
      recompute_edge_squared_norms_ = true;
    }
  }
}

void PrimalEdgeNorms::UpdateBeforeBasisPivot(ColIndex entering_col,
                                             ColIndex leaving_col,
                                             RowIndex leaving_row,
                                             ScatteredColumnReference direction,
                                             UpdateRow* update_row) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK_NE(entering_col, leaving_col);
  if (!recompute_edge_squared_norms_) {
    update_row->ComputeUpdateRow(leaving_row);
    ComputeDirectionLeftInverse(entering_col, direction);
    UpdateEdgeSquaredNorms(entering_col, leaving_col, leaving_row,
                           direction.dense_column, *update_row);
  }
  if (!reset_devex_weights_) {
    // Resets devex weights once in a while. If so, no need to update them
    // before.
    ++num_devex_updates_since_reset_;
    if (num_devex_updates_since_reset_ >
        parameters_.devex_weights_reset_period()) {
      reset_devex_weights_ = true;
    } else {
      update_row->ComputeUpdateRow(leaving_row);
      UpdateDevexWeights(entering_col, leaving_col, leaving_row,
                         direction.dense_column, *update_row);
    }
  }
}

void PrimalEdgeNorms::ComputeMatrixColumnNorms() {
  SCOPED_TIME_STAT(&stats_);
  matrix_column_norms_.resize(matrix_.num_cols(), 0.0);
  for (ColIndex col(0); col < matrix_.num_cols(); ++col) {
    matrix_column_norms_[col] = sqrt(SquaredNorm(matrix_.column(col)));
    num_operations_ += matrix_.column(col).num_entries().value();
  }
}

void PrimalEdgeNorms::ComputeEdgeSquaredNorms() {
  SCOPED_TIME_STAT(&stats_);

  // Since we will do a lot of inversions, it is better to be as efficient and
  // precise as possible by refactorizing the basis.
  DCHECK(basis_factorization_.IsRefactorized());
  edge_squared_norms_.resize(matrix_.num_cols(), 0.0);
  for (const ColIndex col : variables_info_.GetIsRelevantBitRow()) {
    // Note the +1.0 in the squared norm for the component of the edge on the
    // 'entering_col'.
    edge_squared_norms_[col] =
        1.0 + basis_factorization_.RightSolveSquaredNorm(matrix_.column(col));
  }
  recompute_edge_squared_norms_ = false;
}

void PrimalEdgeNorms::ComputeDirectionLeftInverse(
    ColIndex entering_col, ScatteredColumnReference direction) {
  SCOPED_TIME_STAT(&stats_);

  // Initialize direction_left_inverse_ to direction.dense_column .
  // Note the special case when the non-zero vector is empty which means we
  // don't know and need to use the dense version.
  const ColIndex size = RowToColIndex(direction.dense_column.size());
  const double kThreshold = 0.05 * size.value();
  if (!direction_left_inverse_non_zeros_.empty() &&
      (direction_left_inverse_non_zeros_.size() +
           direction.non_zero_rows.size() <
       2 * kThreshold)) {
    ClearAndResizeVectorWithNonZeros(size, &direction_left_inverse_,
                                     &direction_left_inverse_non_zeros_);
    for (const RowIndex row : direction.non_zero_rows) {
      direction_left_inverse_[RowToColIndex(row)] = direction.dense_column[row];
    }
  } else {
    direction_left_inverse_ = Transpose(direction.dense_column);
    direction_left_inverse_non_zeros_.clear();
  }

  // Depending on the sparsity of the input, we decide which version to use.
  if (direction.non_zero_rows.size() < kThreshold) {
    direction_left_inverse_non_zeros_ =
        *reinterpret_cast<ColIndexVector const*>(&direction.non_zero_rows);
    basis_factorization_.LeftSolveWithNonZeros(
        &direction_left_inverse_, &direction_left_inverse_non_zeros_);
  } else {
    basis_factorization_.LeftSolve(&direction_left_inverse_);
  }

  // TODO(user): Refactorize if estimated accuracy above a threshold.
  IF_STATS_ENABLED(stats_.direction_left_inverse_accuracy.Add(
      ScalarProduct(direction_left_inverse_, matrix_.column(entering_col)) -
      SquaredNorm(direction.dense_column)));
  IF_STATS_ENABLED(stats_.direction_left_inverse_density.Add(
      Density(direction_left_inverse_)));
}

// Let new_edge denote the edge of 'col' in the new basis. We want:
//   reduced_costs_[col] = ScalarProduct(new_edge, basic_objective_);
//   edge_squared_norms_[col] = SquaredNorm(new_edge);
//
// In order to compute this, we use the formulas:
//   new_leaving_edge = old_entering_edge / divisor.
//   new_edge = old_edge + update_coeff * new_leaving_edge.
void PrimalEdgeNorms::UpdateEdgeSquaredNorms(ColIndex entering_col,
                                             ColIndex leaving_col,
                                             RowIndex leaving_row,
                                             const DenseColumn& direction,
                                             const UpdateRow& update_row) {
  SCOPED_TIME_STAT(&stats_);

  // 'pivot' is the value of the entering_edge at 'leaving_row'.
  // The edge of the 'leaving_col' in the new basis is equal to
  // entering_edge / 'pivot'.
  const Fractional pivot = -direction[leaving_row];
  DCHECK_NE(pivot, 0.0);

  // Note that this should be precise because of the call to
  // TestEnteringEdgeNormPrecision().
  const Fractional entering_squared_norm = edge_squared_norms_[entering_col];
  const Fractional leaving_squared_norm =
      std::max(1.0, entering_squared_norm / Square(pivot));

  int stat_lower_bounded_norms = 0;
  const Fractional factor = 2.0 / pivot;
#ifdef OMP
  const int num_omp_threads = parameters_.num_omp_threads();
#else
  const int num_omp_threads = 1;
#endif
  if (num_omp_threads == 1) {
    for (const ColIndex col : update_row.GetNonZeroPositions()) {
      const Fractional coeff = update_row.GetCoefficient(col);
      const Fractional scalar_product =
          compact_matrix_.ColumnScalarProduct(col, direction_left_inverse_);
      num_operations_ += compact_matrix_.column(col).num_entries().value();

      // Update the edge squared norm of this column. Note that the update
      // formula used is important to maximize the precision. See an explanation
      // in the dual context in Koberstein's PhD thesis, section 8.2.2.1.
      edge_squared_norms_[col] +=
          coeff * (coeff * leaving_squared_norm + factor * scalar_product);

      // Make sure it doesn't go under a known lower bound (TODO(user): ref?).
      // This way norms are always >= 1.0 .
      // TODO(user): precompute 1 / Square(pivot) or 1 / pivot? it will be
      // slightly faster, but may introduce numerical issues. More generally,
      // this test is only needed in a few cases, so is it worth it?
      const Fractional lower_bound = 1.0 + Square(coeff / pivot);
      if (edge_squared_norms_[col] < lower_bound) {
        edge_squared_norms_[col] = lower_bound;
        ++stat_lower_bounded_norms;
      }
    }
    edge_squared_norms_[leaving_col] = leaving_squared_norm;
    stats_.lower_bounded_norms.Add(stat_lower_bounded_norms);
  } else {
#ifdef OMP
    // In the multi-threaded case, perform the same computation as in the
    // single-threaded case above.
    //
    // TODO(user): also update num_operations_.
    std::vector<int> thread_local_stat_lower_bounded_norms(num_omp_threads, 0);
    const std::vector<ColIndex>& relevant_rows =
        update_row.GetNonZeroPositions();
    const int parallel_loop_size = relevant_rows.size();
#pragma omp parallel for num_threads(num_omp_threads)
    for (int i = 0; i < parallel_loop_size; i++) {
      const ColIndex col(relevant_rows[i]);
      const Fractional coeff = update_row.GetCoefficient(col);
      const Fractional scalar_product =
          compact_matrix_.ColumnScalarProduct(col, direction_left_inverse_);
      edge_squared_norms_[col] +=
          coeff * (coeff * leaving_squared_norm + factor * scalar_product);
      const Fractional lower_bound = 1.0 + Square(coeff / pivot);
      if (edge_squared_norms_[col] < lower_bound) {
        edge_squared_norms_[col] = lower_bound;
        ++thread_local_stat_lower_bounded_norms[omp_get_thread_num()];
      }
    }
    // end of omp parallel for
    edge_squared_norms_[leaving_col] = leaving_squared_norm;
    for (int i = 0; i < num_omp_threads; i++) {
      stat_lower_bounded_norms += thread_local_stat_lower_bounded_norms[i];
    }
    stats_.lower_bounded_norms.Add(stat_lower_bounded_norms);
#endif  // OMP
  }
}

void PrimalEdgeNorms::UpdateDevexWeights(
    ColIndex entering_col /* index q in the paper */,
    ColIndex leaving_col /* index p in the paper */, RowIndex leaving_row,
    const DenseColumn& direction, const UpdateRow& update_row) {
  SCOPED_TIME_STAT(&stats_);

  // Compared to steepest edge update, the DEVEX weight uses the largest of the
  // norms of two vectors to approximate the norm of the sum.
  const Fractional entering_norm = sqrt(PreciseSquaredNorm(direction));
  const Fractional pivot_magnitude = std::abs(direction[leaving_row]);
  const Fractional leaving_norm =
      std::max(1.0, entering_norm / pivot_magnitude);
  for (const ColIndex col : update_row.GetNonZeroPositions()) {
    const Fractional coeff = update_row.GetCoefficient(col);
    const Fractional update_vector_norm = std::abs(coeff) * leaving_norm;
    devex_weights_[col] = std::max(devex_weights_[col], update_vector_norm);
  }
  devex_weights_[leaving_col] = leaving_norm;
}

void PrimalEdgeNorms::ResetDevexWeights() {
  SCOPED_TIME_STAT(&stats_);
  if (parameters_.initialize_devex_with_column_norms()) {
    devex_weights_ = GetMatrixColumnNorms();
  } else {
    devex_weights_.assign(matrix_.num_cols(), 1.0);
  }
  num_devex_updates_since_reset_ = 0;
  reset_devex_weights_ = false;
}

}  // namespace glop
}  // namespace operations_research
