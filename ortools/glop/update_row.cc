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

#include "ortools/glop/update_row.h"

#ifdef OMP
#include <omp.h>
#endif

#include "ortools/lp_data/lp_utils.h"

namespace operations_research {
namespace glop {

UpdateRow::UpdateRow(const CompactSparseMatrix& matrix,
                     const CompactSparseMatrix& transposed_matrix,
                     const VariablesInfo& variables_info,
                     const RowToColMapping& basis,
                     const BasisFactorization& basis_factorization)
    : matrix_(matrix),
      transposed_matrix_(transposed_matrix),
      variables_info_(variables_info),
      basis_(basis),
      basis_factorization_(basis_factorization),
      unit_row_left_inverse_(),
      non_zero_position_list_(),
      non_zero_position_set_(),
      coefficient_(),
      compute_update_row_(true),
      num_operations_(0),
      parameters_(),
      stats_() {}

void UpdateRow::Invalidate() {
  SCOPED_TIME_STAT(&stats_);
  compute_update_row_ = true;
}

void UpdateRow::IgnoreUpdatePosition(ColIndex col) {
  SCOPED_TIME_STAT(&stats_);
  if (col >= coefficient_.size()) return;
  coefficient_[col] = 0.0;
}

ScatteredColumnReference UpdateRow::GetUnitRowLeftInverse() const {
  DCHECK(!compute_update_row_);
  return ScatteredColumnReference(unit_row_left_inverse_,
                                  unit_row_left_inverse_non_zeros_);
}

void UpdateRow::ComputeUnitRowLeftInverse(RowIndex leaving_row) {
  SCOPED_TIME_STAT(&stats_);
  basis_factorization_.LeftSolveForUnitRow(RowToColIndex(leaving_row),
                                           &unit_row_left_inverse_,
                                           &unit_row_left_inverse_non_zeros_);

  // TODO(user): Refactorize if the estimated accuracy is above a threshold.
  IF_STATS_ENABLED(stats_.unit_row_left_inverse_accuracy.Add(
      matrix_.ColumnScalarProduct(basis_[leaving_row], unit_row_left_inverse_) -
      1.0));
  IF_STATS_ENABLED(stats_.unit_row_left_inverse_density.Add(
      static_cast<double>(unit_row_left_inverse_non_zeros_.size()) /
      static_cast<double>(matrix_.num_rows().value())));
}

void UpdateRow::ComputeUpdateRow(RowIndex leaving_row) {
  if (!compute_update_row_) return;
  compute_update_row_ = false;
  ComputeUnitRowLeftInverse(leaving_row);
  SCOPED_TIME_STAT(&stats_);

  if (parameters_.use_transposed_matrix()) {
    // Number of entries that ComputeUpdatesRowWise() will need to look at.
    EntryIndex num_row_wise_entries(0);
    for (const ColIndex col : unit_row_left_inverse_non_zeros_) {
      num_row_wise_entries += transposed_matrix_.ColumnNumEntries(col);
    }

    // Number of entries that ComputeUpdatesColumnWise() will need to look at.
    const EntryIndex num_col_wise_entries =
        variables_info_.GetNumEntriesInRelevantColumns();

    // Note that the thresholds were chosen (more or less) from the result of
    // the microbenchmark tests of this file in September 2013.
    // TODO(user): automate the computation of these constants at run-time?
    const double lhs = static_cast<double>(num_row_wise_entries.value());
    if (lhs < 0.5 * static_cast<double>(num_col_wise_entries.value())) {
      if (lhs < 1.1 * static_cast<double>(matrix_.num_cols().value())) {
        ComputeUpdatesRowWiseHypersparse();
        num_operations_ += num_row_wise_entries.value();
      } else {
        ComputeUpdatesRowWise();
        num_operations_ +=
            num_row_wise_entries.value() + matrix_.num_rows().value();
      }
    } else {
      ComputeUpdatesColumnWise();
      num_operations_ +=
          num_col_wise_entries.value() + matrix_.num_cols().value();
    }
  } else {
    ComputeUpdatesColumnWise();
    num_operations_ +=
        variables_info_.GetNumEntriesInRelevantColumns().value() +
        matrix_.num_cols().value();
  }
  IF_STATS_ENABLED(stats_.update_row_density.Add(
      static_cast<double>(non_zero_position_list_.size()) /
      static_cast<double>(matrix_.num_cols().value())));
}

void UpdateRow::ComputeUpdateRowForBenchmark(const DenseRow& lhs,
                                             const std::string& algorithm) {
  unit_row_left_inverse_ = lhs;
  ComputeNonZeros(lhs, &unit_row_left_inverse_non_zeros_);
  if (algorithm == "column") {
    ComputeUpdatesColumnWise();
  } else if (algorithm == "row") {
    ComputeUpdatesRowWise();
  } else if (algorithm == "row_hypersparse") {
    ComputeUpdatesRowWiseHypersparse();
  } else {
    LOG(DFATAL) << "Unknown algorithm in ComputeUpdateRowForBenchmark(): '"
                << algorithm << "'";
  }
}

const DenseRow& UpdateRow::GetCoefficients() const { return coefficient_; }

const ColIndexVector& UpdateRow::GetNonZeroPositions() const {
  return non_zero_position_list_;
}

void UpdateRow::SetParameters(const GlopParameters& parameters) {
  parameters_ = parameters;
}

// This is optimized for the case when the total number of entries is about
// the same as, or greater than, the number of columns.
void UpdateRow::ComputeUpdatesRowWise() {
  SCOPED_TIME_STAT(&stats_);
  const ColIndex num_cols = matrix_.num_cols();
  coefficient_.AssignToZero(num_cols);
  for (ColIndex col : unit_row_left_inverse_non_zeros_) {
    const Fractional multiplier = unit_row_left_inverse_[col];
    for (const EntryIndex i : transposed_matrix_.Column(col)) {
      const ColIndex pos = RowToColIndex(transposed_matrix_.EntryRow(i));
      coefficient_[pos] += multiplier * transposed_matrix_.EntryCoefficient(i);
    }
  }

  non_zero_position_list_.clear();
  const Fractional drop_tolerance = parameters_.drop_tolerance();
  for (const ColIndex col : variables_info_.GetIsRelevantBitRow()) {
    if (std::abs(coefficient_[col]) > drop_tolerance) {
      non_zero_position_list_.push_back(col);
    }
  }
}

// This is optimized for the case when the total number of entries is smaller
// than the number of columns.
void UpdateRow::ComputeUpdatesRowWiseHypersparse() {
  SCOPED_TIME_STAT(&stats_);
  const ColIndex num_cols = matrix_.num_cols();
  non_zero_position_set_.ClearAndResize(num_cols);
  coefficient_.resize(num_cols, 0.0);
  for (ColIndex col : unit_row_left_inverse_non_zeros_) {
    const Fractional multiplier = unit_row_left_inverse_[col];
    for (const EntryIndex i : transposed_matrix_.Column(col)) {
      const ColIndex pos = RowToColIndex(transposed_matrix_.EntryRow(i));
      const Fractional v = multiplier * transposed_matrix_.EntryCoefficient(i);
      if (!non_zero_position_set_.IsSet(pos)) {
        // Note that we could create the non_zero_position_list_ here, but we
        // prefer to keep the non-zero positions sorted, so using the bitset is
        // a good alernative. Of course if the solution is really really sparse,
        // then sorting non_zero_position_list_ will be faster.
        coefficient_[pos] = v;
        non_zero_position_set_.Set(pos);
      } else {
        coefficient_[pos] += v;
      }
    }
  }

  // Only keep in non_zero_position_set_ the relevant positions.
  non_zero_position_set_.Intersection(variables_info_.GetIsRelevantBitRow());
  non_zero_position_list_.clear();
  const Fractional drop_tolerance = parameters_.drop_tolerance();
  for (const ColIndex col : non_zero_position_set_) {
    // TODO(user): Since the solution is really sparse, maybe storing the
    // non-zero coefficients contiguously in a vector is better than keeping
    // them as they are. Note however that we will iterate only twice on the
    // update row coefficients during an iteration.
    if (std::abs(coefficient_[col]) > drop_tolerance) {
      non_zero_position_list_.push_back(col);
    }
  }
}

// Note that we use the same algo as ComputeUpdatesColumnWise() here. The
// others version might be faster, but this is called only once per solve, so
// it shouldn't be too bad.
void UpdateRow::RecomputeFullUpdateRow(RowIndex leaving_row) {
  CHECK(!compute_update_row_);
  const ColIndex num_cols = matrix_.num_cols();
  const Fractional drop_tolerance = parameters_.drop_tolerance();
  coefficient_.resize(num_cols, 0.0);
  non_zero_position_list_.clear();

  // Fills the only position at one in the basic columns.
  coefficient_[basis_[leaving_row]] = 1.0;
  non_zero_position_list_.push_back(basis_[leaving_row]);

  // Fills the non-basic column.
  for (const ColIndex col : variables_info_.GetNotBasicBitRow()) {
    const Fractional coeff =
        matrix_.ColumnScalarProduct(col, unit_row_left_inverse_);
    if (std::abs(coeff) > drop_tolerance) {
      non_zero_position_list_.push_back(col);
      coefficient_[col] = coeff;
    }
  }
}

void UpdateRow::ComputeUpdatesColumnWise() {
  SCOPED_TIME_STAT(&stats_);

  const ColIndex num_cols = matrix_.num_cols();
  const Fractional drop_tolerance = parameters_.drop_tolerance();
  coefficient_.resize(num_cols, 0.0);
  non_zero_position_list_.clear();
#ifdef OMP
  const int num_omp_threads = parameters_.num_omp_threads();
  if (num_omp_threads == 1) {
#endif
    for (const ColIndex col : variables_info_.GetIsRelevantBitRow()) {
      // Coefficient of the column right inverse on the 'leaving_row'.
      const Fractional coeff =
          matrix_.ColumnScalarProduct(col, unit_row_left_inverse_);
      // Nothing to do if 'coeff' is (almost) zero which does happen due to
      // sparsity. Note that it shouldn't be too bad to use a non-zero drop
      // tolerance here because even if we introduce some precision issues, the
      // quantities updated by this update row will eventually be recomputed.
      if (std::abs(coeff) > drop_tolerance) {
        non_zero_position_list_.push_back(col);
        coefficient_[col] = coeff;
      }
    }
#ifdef OMP
  } else {
    // In the multi-threaded case, perform the same computation as in the
    // single-threaded case above.
    const DenseBitRow& is_relevant = variables_info_.GetIsRelevantBitRow();
    const int parallel_loop_size = is_relevant.size().value();
#pragma omp parallel for num_threads(num_omp_threads)
    for (int i = 0; i < parallel_loop_size; i++) {
      const ColIndex col(i);
      if (is_relevant.IsSet(col)) {
        coefficient_[col] =
            matrix_.ColumnScalarProduct(col, unit_row_left_inverse_);
      }
    }
    // End of omp parallel for.
    for (const ColIndex col : variables_info_.GetIsRelevantBitRow()) {
      if (std::abs(coefficient_[col]) > drop_tolerance) {
        non_zero_position_list_.push_back(col);
      }
    }
  }
#endif  // OMP
}

}  // namespace glop
}  // namespace operations_research
