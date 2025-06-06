// Copyright 2010-2025 Google LLC
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

#include <algorithm>
#include <cstdlib>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "ortools/glop/basis_representation.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/update_row.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_utils.h"
#include "ortools/lp_data/scattered_vector.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/util/stats.h"

namespace operations_research {
namespace glop {

PrimalEdgeNorms::PrimalEdgeNorms(const CompactSparseMatrix& compact_matrix,
                                 const VariablesInfo& variables_info,
                                 const BasisFactorization& basis_factorization)
    : compact_matrix_(compact_matrix),
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
  matrix_column_norms_.clear();
  recompute_edge_squared_norms_ = true;
  reset_devex_weights_ = true;
  for (bool* watcher : watchers_) *watcher = true;
}

bool PrimalEdgeNorms::NeedsBasisRefactorization() const {
  if (pricing_rule_ != GlopParameters ::STEEPEST_EDGE) return false;
  return recompute_edge_squared_norms_;
}

DenseRow::ConstView PrimalEdgeNorms::GetSquaredNorms() {
  switch (pricing_rule_) {
    case GlopParameters::DANTZIG:
      return GetMatrixColumnNorms().const_view();
    case GlopParameters::STEEPEST_EDGE:
      return GetEdgeSquaredNorms().const_view();
    case GlopParameters::DEVEX:
      return GetDevexWeights().const_view();
  }
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

bool PrimalEdgeNorms::TestEnteringEdgeNormPrecision(
    ColIndex entering_col, const ScatteredColumn& direction) {
  if (!recompute_edge_squared_norms_) {
    SCOPED_TIME_STAT(&stats_);
    // Recompute the squared norm of the edge used during this
    // iteration, i.e. the entering edge.
    const Fractional old_squared_norm = edge_squared_norms_[entering_col];
    const Fractional precise_squared_norm = 1.0 + SquaredNorm(direction);
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
      for (bool* watcher : watchers_) *watcher = true;
    }

    if (old_squared_norm < 0.25 * precise_squared_norm) {
      VLOG(1) << "Imprecise norm, reprice. old=" << old_squared_norm
              << " new=" << precise_squared_norm;
      return false;
    }
  }
  return true;
}

void PrimalEdgeNorms::UpdateBeforeBasisPivot(ColIndex entering_col,
                                             ColIndex leaving_col,
                                             RowIndex leaving_row,
                                             const ScatteredColumn& direction,
                                             UpdateRow* update_row) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK_NE(entering_col, leaving_col);
  if (!recompute_edge_squared_norms_) {
    update_row->ComputeUpdateRow(leaving_row);
    ComputeDirectionLeftInverse(entering_col, direction);
    UpdateEdgeSquaredNorms(entering_col, leaving_col, leaving_row,
                           direction.values, *update_row);
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
                         direction.values, *update_row);
    }
  }
}

void PrimalEdgeNorms::ComputeMatrixColumnNorms() {
  SCOPED_TIME_STAT(&stats_);
  matrix_column_norms_.resize(compact_matrix_.num_cols(), 0.0);
  for (ColIndex col(0); col < compact_matrix_.num_cols(); ++col) {
    matrix_column_norms_[col] = SquaredNorm(compact_matrix_.column(col));
    num_operations_ += compact_matrix_.column(col).num_entries().value();
  }
}

void PrimalEdgeNorms::ComputeEdgeSquaredNorms() {
  SCOPED_TIME_STAT(&stats_);

  // time_limit_->LimitReached() can be costly sometimes, so we only do that
  // if we feel this will be slow anyway.
  const bool test_limit = (time_limit_ != nullptr) &&
                          basis_factorization_.NumberOfEntriesInLU() > 10'000;

  // Since we will do a lot of inversions, it is better to be as efficient and
  // precise as possible by refactorizing the basis.
  DCHECK(basis_factorization_.IsRefactorized());
  edge_squared_norms_.resize(compact_matrix_.num_cols(), 1.0);
  for (const ColIndex col : variables_info_.GetIsRelevantBitRow()) {
    // Note the +1.0 in the squared norm for the component of the edge on the
    // 'entering_col'.
    edge_squared_norms_[col] = 1.0 + basis_factorization_.RightSolveSquaredNorm(
                                         compact_matrix_.column(col));

    // This operation can be costly, and we abort if we are stuck here.
    // Note that we still mark edges as "recomputed" otherwise we can runs into
    // some DCHECK before we actually abort the solve.
    if (test_limit && time_limit_->LimitReached()) break;
  }

  recompute_edge_squared_norms_ = false;
}

// TODO(user): It should be possible to reorganize the code and call this when
// the value of direction is no longer needed. This will simplify the code and
// avoid a copy here.
void PrimalEdgeNorms::ComputeDirectionLeftInverse(
    ColIndex entering_col, const ScatteredColumn& direction) {
  SCOPED_TIME_STAT(&stats_);

  // Initialize direction_left_inverse_ to direction. Note the special case when
  // the non-zero vector is empty which means we don't know and need to use the
  // dense version.
  const ColIndex size = RowToColIndex(direction.values.size());
  const double kThreshold = 0.05 * size.value();
  if (!direction_left_inverse_.non_zeros.empty() &&
      (direction_left_inverse_.non_zeros.size() + direction.non_zeros.size() <
       2 * kThreshold)) {
    ClearAndResizeVectorWithNonZeros(size, &direction_left_inverse_);
    for (const auto e : direction) {
      direction_left_inverse_[RowToColIndex(e.row())] = e.coefficient();
    }
  } else {
    direction_left_inverse_.values = Transpose(direction.values);
    direction_left_inverse_.non_zeros.clear();
  }

  if (direction.non_zeros.size() < kThreshold) {
    direction_left_inverse_.non_zeros = TransposedView(direction).non_zeros;
  }
  basis_factorization_.LeftSolve(&direction_left_inverse_);

  // TODO(user): Refactorize if estimated accuracy above a threshold.
  IF_STATS_ENABLED(stats_.direction_left_inverse_accuracy.Add(
      compact_matrix_.ColumnScalarProduct(entering_col,
                                          direction_left_inverse_.values) -
      SquaredNorm(direction.values)));
  IF_STATS_ENABLED(stats_.direction_left_inverse_density.Add(
      Density(direction_left_inverse_.values)));
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

  const ColIndex first_slack =
      compact_matrix_.num_cols() - RowToColIndex(compact_matrix_.num_rows());

  // Note that this should be precise because of the call to
  // TestEnteringEdgeNormPrecision().
  const Fractional entering_squared_norm = edge_squared_norms_[entering_col];
  const Fractional leaving_squared_norm =
      std::max(Fractional(1.0), entering_squared_norm / Square(pivot));

  int stat_lower_bounded_norms = 0;
  const Fractional factor = 2.0 / pivot;
  const auto view = compact_matrix_.view();
  auto output = edge_squared_norms_.view();
  const auto direction_left_inverse =
      direction_left_inverse_.values.const_view();
  for (const ColIndex col : update_row.GetNonZeroPositions()) {
    const Fractional coeff = update_row.GetCoefficient(col);
    const Fractional scalar_product =
        col >= first_slack
            ? direction_left_inverse_[col - first_slack]
            : view.ColumnScalarProduct(col, direction_left_inverse);
    num_operations_ += view.ColumnNumEntries(col).value();

    // Update the edge squared norm of this column. Note that the update
    // formula used is important to maximize the precision. See an explanation
    // in the dual context in Koberstein's PhD thesis, section 8.2.2.1.
    output[col] +=
        coeff * (coeff * leaving_squared_norm + factor * scalar_product);

    // Make sure it doesn't go under a known lower bound (TODO(user): ref?).
    // This way norms are always >= 1.0 .
    // TODO(user): precompute 1 / Square(pivot) or 1 / pivot? it will be
    // slightly faster, but may introduce numerical issues. More generally,
    // this test is only needed in a few cases, so is it worth it?
    const Fractional lower_bound = 1.0 + Square(coeff / pivot);
    if (output[col] < lower_bound) {
      output[col] = lower_bound;
      ++stat_lower_bounded_norms;
    }
  }
  output[leaving_col] = leaving_squared_norm;
  stats_.lower_bounded_norms.Add(stat_lower_bounded_norms);
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
      std::max(Fractional(1.0), entering_norm / pivot_magnitude);
  for (const ColIndex col : update_row.GetNonZeroPositions()) {
    const Fractional coeff = update_row.GetCoefficient(col);
    const Fractional update_vector_norm = std::abs(coeff) * leaving_norm;
    devex_weights_[col] =
        std::max(devex_weights_[col], Square(update_vector_norm));
  }
  devex_weights_[leaving_col] = Square(leaving_norm);
}

void PrimalEdgeNorms::ResetDevexWeights() {
  SCOPED_TIME_STAT(&stats_);
  if (parameters_.initialize_devex_with_column_norms()) {
    devex_weights_ = GetMatrixColumnNorms();
  } else {
    devex_weights_.assign(compact_matrix_.num_cols(), 1.0);
  }
  num_devex_updates_since_reset_ = 0;
  reset_devex_weights_ = false;
}

}  // namespace glop
}  // namespace operations_research
