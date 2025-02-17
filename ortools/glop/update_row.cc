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

#include "ortools/glop/update_row.h"

#include <cstdlib>
#include <string>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/glop/basis_representation.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_utils.h"
#include "ortools/lp_data/scattered_vector.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/util/stats.h"

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
      num_operations_(0),
      parameters_(),
      stats_() {}

void UpdateRow::Invalidate() {
  SCOPED_TIME_STAT(&stats_);
  left_inverse_computed_for_ = kInvalidRow;
  update_row_computed_for_ = kInvalidRow;
}

const ScatteredRow& UpdateRow::GetUnitRowLeftInverse() const {
  return unit_row_left_inverse_;
}

const ScatteredRow& UpdateRow::ComputeAndGetUnitRowLeftInverse(
    RowIndex leaving_row) {
  Invalidate();
  basis_factorization_.TemporaryLeftSolveForUnitRow(RowToColIndex(leaving_row),
                                                    &unit_row_left_inverse_);
  return unit_row_left_inverse_;
}

void UpdateRow::ComputeUnitRowLeftInverse(RowIndex leaving_row) {
  if (left_inverse_computed_for_ == leaving_row) return;
  left_inverse_computed_for_ = leaving_row;
  SCOPED_TIME_STAT(&stats_);

  basis_factorization_.LeftSolveForUnitRow(RowToColIndex(leaving_row),
                                           &unit_row_left_inverse_);

  // TODO(user): Refactorize if the estimated accuracy is above a threshold.
  IF_STATS_ENABLED(stats_.unit_row_left_inverse_accuracy.Add(
      matrix_.ColumnScalarProduct(basis_[leaving_row],
                                  unit_row_left_inverse_.values) -
      1.0));
  IF_STATS_ENABLED(stats_.unit_row_left_inverse_density.Add(
      Density(unit_row_left_inverse_.values)));
}

void UpdateRow::ComputeUpdateRow(RowIndex leaving_row) {
  if (update_row_computed_for_ == leaving_row) return;
  update_row_computed_for_ = leaving_row;
  ComputeUnitRowLeftInverse(leaving_row);
  SCOPED_TIME_STAT(&stats_);

  if (parameters_.use_transposed_matrix()) {
    // Number of entries that ComputeUpdatesRowWise() will need to look at.
    EntryIndex num_row_wise_entries(0);

    // Because we are about to do an expensive matrix-vector product, we make
    // sure we drop small entries in the vector for the row-wise algorithm. We
    // also computes its non-zeros to simplify the code below.
    //
    // TODO(user): So far we didn't generalize the use of drop tolerances
    // everywhere in the solver, so we make sure to not modify
    // unit_row_left_inverse_ that is also used elsewhere. However, because of
    // that, we will not get the exact same result depending on the algortihm
    // used below because the ComputeUpdatesColumnWise() will still use these
    // small entries (no complexity changes).
    const Fractional drop_tolerance = parameters_.drop_tolerance();
    unit_row_left_inverse_filtered_non_zeros_.clear();
    const auto matrix_view = transposed_matrix_.view();
    if (unit_row_left_inverse_.non_zeros.empty()) {
      const ColIndex size = unit_row_left_inverse_.values.size();
      const auto values = unit_row_left_inverse_.values.view();
      for (ColIndex col(0); col < size; ++col) {
        if (std::abs(values[col]) > drop_tolerance) {
          unit_row_left_inverse_filtered_non_zeros_.push_back(col);
          num_row_wise_entries += matrix_view.ColumnNumEntries(col);
        }
      }
    } else {
      for (const auto e : unit_row_left_inverse_) {
        if (std::abs(e.coefficient()) > drop_tolerance) {
          unit_row_left_inverse_filtered_non_zeros_.push_back(e.column());
          num_row_wise_entries += matrix_view.ColumnNumEntries(e.column());
        }
      }
    }

    // The case of size 1 happens often enough to deserve special code.
    //
    // TODO(user): The impact is not as high as I hopped though, so not too
    // important.
    if (unit_row_left_inverse_filtered_non_zeros_.size() == 1) {
      ComputeUpdatesForSingleRow(
          unit_row_left_inverse_filtered_non_zeros_.front());
      num_operations_ += num_row_wise_entries.value();
      IF_STATS_ENABLED(stats_.update_row_density.Add(
          static_cast<double>(num_non_zeros_) /
          static_cast<double>(matrix_.num_cols().value())));
      return;
    }

    // Number of entries that ComputeUpdatesColumnWise() will need to look at.
    const EntryIndex num_col_wise_entries =
        variables_info_.GetNumEntriesInRelevantColumns();

    // Note that the thresholds were chosen (more or less) from the result of
    // the microbenchmark tests of this file in September 2013.
    // TODO(user): automate the computation of these constants at run-time?
    const double row_wise = static_cast<double>(num_row_wise_entries.value());
    if (row_wise < 0.5 * static_cast<double>(num_col_wise_entries.value())) {
      if (row_wise < 1.1 * static_cast<double>(matrix_.num_cols().value())) {
        ComputeUpdatesRowWiseHypersparse();

        // We use a multiplicative factor because these entries are often widely
        // spread in memory. There is also some overhead to each fp operations.
        num_operations_ +=
            5 * num_row_wise_entries.value() + matrix_.num_cols().value() / 64;
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
      static_cast<double>(num_non_zeros_) /
      static_cast<double>(matrix_.num_cols().value())));
}

void UpdateRow::ComputeUpdateRowForBenchmark(const DenseRow& lhs,
                                             const std::string& algorithm) {
  unit_row_left_inverse_.values = lhs;
  ComputeNonZeros(lhs, &unit_row_left_inverse_filtered_non_zeros_);
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

absl::Span<const ColIndex> UpdateRow::GetNonZeroPositions() const {
  return absl::MakeSpan(non_zero_position_list_.data(), num_non_zeros_);
}

void UpdateRow::SetParameters(const GlopParameters& parameters) {
  parameters_ = parameters;
}

// This is optimized for the case when the total number of entries is about
// the same as, or greater than, the number of columns.
void UpdateRow::ComputeUpdatesRowWise() {
  SCOPED_TIME_STAT(&stats_);
  coefficient_.AssignToZero(matrix_.num_cols());
  const auto output_coeffs = coefficient_.view();
  const auto view = transposed_matrix_.view();
  for (const ColIndex col : unit_row_left_inverse_filtered_non_zeros_) {
    const Fractional multiplier = unit_row_left_inverse_[col];
    for (const EntryIndex i : view.Column(col)) {
      const ColIndex pos = RowToColIndex(view.EntryRow(i));
      output_coeffs[pos] += multiplier * view.EntryCoefficient(i);
    }
  }

  non_zero_position_list_.resize(matrix_.num_cols().value());
  auto* non_zeros = non_zero_position_list_.data();
  const Fractional drop_tolerance = parameters_.drop_tolerance();
  for (const ColIndex col : variables_info_.GetIsRelevantBitRow()) {
    if (std::abs(output_coeffs[col]) > drop_tolerance) {
      *non_zeros++ = col;
    }
  }
  num_non_zeros_ = non_zeros - non_zero_position_list_.data();
}

// This is optimized for the case when the total number of entries is smaller
// than the number of columns.
void UpdateRow::ComputeUpdatesRowWiseHypersparse() {
  SCOPED_TIME_STAT(&stats_);
  const ColIndex num_cols = matrix_.num_cols();
  non_zero_position_set_.ClearAndResize(num_cols);
  coefficient_.resize(num_cols, 0.0);

  const auto output_coeffs = coefficient_.view();
  const auto view = transposed_matrix_.view();
  const auto nz_set = non_zero_position_set_.const_view();
  for (const ColIndex col : unit_row_left_inverse_filtered_non_zeros_) {
    const Fractional multiplier = unit_row_left_inverse_[col];
    for (const EntryIndex i : view.Column(col)) {
      const ColIndex pos = RowToColIndex(view.EntryRow(i));
      const Fractional v = multiplier * view.EntryCoefficient(i);
      if (!nz_set[pos]) {
        // Note that we could create the non_zero_position_list_ here, but we
        // prefer to keep the non-zero positions sorted, so using the bitset is
        // a good alternative. Of course if the solution is really really
        // sparse, then sorting non_zero_position_list_ will be faster.
        output_coeffs[pos] = v;
        non_zero_position_set_.Set(pos);
      } else {
        output_coeffs[pos] += v;
      }
    }
  }

  // Only keep in non_zero_position_set_ the relevant positions.
  non_zero_position_set_.Intersection(variables_info_.GetIsRelevantBitRow());
  non_zero_position_list_.resize(matrix_.num_cols().value());
  auto* non_zeros = non_zero_position_list_.data();
  const Fractional drop_tolerance = parameters_.drop_tolerance();
  for (const ColIndex col : non_zero_position_set_) {
    // TODO(user): Since the solution is really sparse, maybe storing the
    // non-zero coefficients contiguously in a vector is better than keeping
    // them as they are. Note however that we will iterate only twice on the
    // update row coefficients during an iteration.
    if (std::abs(output_coeffs[col]) > drop_tolerance) {
      *non_zeros++ = col;
    }
  }
  num_non_zeros_ = non_zeros - non_zero_position_list_.data();
}

void UpdateRow::ComputeUpdatesForSingleRow(ColIndex row_as_col) {
  coefficient_.resize(matrix_.num_cols(), 0.0);
  non_zero_position_list_.resize(matrix_.num_cols().value());
  auto* non_zeros = non_zero_position_list_.data();

  const DenseBitRow& is_relevant = variables_info_.GetIsRelevantBitRow();
  const Fractional drop_tolerance = parameters_.drop_tolerance();
  const Fractional multiplier = unit_row_left_inverse_[row_as_col];
  const auto output_coeffs = coefficient_.view();
  const auto view = transposed_matrix_.view();
  for (const EntryIndex i : view.Column(row_as_col)) {
    const ColIndex pos = RowToColIndex(view.EntryRow(i));
    if (!is_relevant[pos]) continue;

    const Fractional v = multiplier * view.EntryCoefficient(i);
    if (std::abs(v) > drop_tolerance) {
      output_coeffs[pos] = v;
      *non_zeros++ = pos;
    }
  }
  num_non_zeros_ = non_zeros - non_zero_position_list_.data();
}

void UpdateRow::ComputeUpdatesColumnWise() {
  SCOPED_TIME_STAT(&stats_);

  coefficient_.resize(matrix_.num_cols(), 0.0);
  non_zero_position_list_.resize(matrix_.num_cols().value());
  auto* non_zeros = non_zero_position_list_.data();

  const Fractional drop_tolerance = parameters_.drop_tolerance();
  const auto output_coeffs = coefficient_.view();
  const auto view = matrix_.view();
  const auto unit_row_left_inverse = unit_row_left_inverse_.values.const_view();
  for (const ColIndex col : variables_info_.GetIsRelevantBitRow()) {
    // Coefficient of the column right inverse on the 'leaving_row'.
    const Fractional coeff =
        view.ColumnScalarProduct(col, unit_row_left_inverse);

    // Nothing to do if 'coeff' is (almost) zero which does happen due to
    // sparsity. Note that it shouldn't be too bad to use a non-zero drop
    // tolerance here because even if we introduce some precision issues, the
    // quantities updated by this update row will eventually be recomputed.
    if (std::abs(coeff) > drop_tolerance) {
      *non_zeros++ = col;
      output_coeffs[col] = coeff;
    }
  }
  num_non_zeros_ = non_zeros - non_zero_position_list_.data();
}

// Note that we use the same algo as ComputeUpdatesColumnWise() here. The
// others version might be faster, but this is called at most once per solve, so
// it shouldn't be too bad.
void UpdateRow::ComputeFullUpdateRow(RowIndex leaving_row,
                                     DenseRow* output) const {
  CHECK_EQ(leaving_row, left_inverse_computed_for_);

  const ColIndex num_cols = matrix_.num_cols();
  output->AssignToZero(num_cols);

  // Fills the only position at one in the basic columns.
  (*output)[basis_[leaving_row]] = 1.0;

  // Fills the non-basic column.
  const Fractional drop_tolerance = parameters_.drop_tolerance();
  const auto view = matrix_.view();
  const auto unit_row_left_inverse = unit_row_left_inverse_.values.const_view();
  for (const ColIndex col : variables_info_.GetNotBasicBitRow()) {
    const Fractional coeff =
        view.ColumnScalarProduct(col, unit_row_left_inverse);
    if (std::abs(coeff) > drop_tolerance) {
      (*output)[col] = coeff;
    }
  }
}

}  // namespace glop
}  // namespace operations_research
