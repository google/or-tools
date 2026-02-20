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

#include "ortools/lp_data/matrix_scaler.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "ortools/base/strong_vector.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/revised_simplex.h"
#include "ortools/glop/status.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_utils.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/lp_data/sparse_column.h"
#include "ortools/util/return_macros.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace glop {

SparseMatrixScaler::SparseMatrixScaler()
    : matrix_(nullptr), row_scales_(), col_scales_() {}

void SparseMatrixScaler::Init(SparseMatrix* matrix) {
  DCHECK(matrix != nullptr);
  matrix_ = matrix;
  row_scales_.resize(matrix_->num_rows(), 1.0);
  col_scales_.resize(matrix_->num_cols(), 1.0);
}

void SparseMatrixScaler::Clear() {
  matrix_ = nullptr;
  row_scales_.clear();
  col_scales_.clear();
}

Fractional SparseMatrixScaler::RowUnscalingFactor(RowIndex row) const {
  DCHECK_GE(row, 0);
  return row < row_scales_.size() ? row_scales_[row] : 1.0;
}

Fractional SparseMatrixScaler::ColUnscalingFactor(ColIndex col) const {
  DCHECK_GE(col, 0);
  return col < col_scales_.size() ? col_scales_[col] : 1.0;
}

Fractional SparseMatrixScaler::RowScalingFactor(RowIndex row) const {
  return 1.0 / RowUnscalingFactor(row);
}

Fractional SparseMatrixScaler::ColScalingFactor(ColIndex col) const {
  return 1.0 / ColUnscalingFactor(col);
}

std::string SparseMatrixScaler::DebugInformationString() const {
  // Note that some computations are redundant with the computations made in
  // some callees, but we do not care as this function is supposed to be called
  // with FLAGS_v set to 1.
  DCHECK(!row_scales_.empty());
  DCHECK(!col_scales_.empty());
  Fractional max_magnitude;
  Fractional min_magnitude;
  matrix_->ComputeMinAndMaxMagnitudes(&min_magnitude, &max_magnitude);
  const Fractional dynamic_range = max_magnitude / min_magnitude;
  std::string output = absl::StrFormat(
      "Min magnitude = %g, max magnitude = %g\n"
      "Dynamic range = %g\n"
      "Variance = %g\n"
      "Minimum row scale = %g, maximum row scale = %g\n"
      "Minimum col scale = %g, maximum col scale = %g\n",
      min_magnitude, max_magnitude, dynamic_range,
      VarianceOfAbsoluteValueOfNonZeros(),
      *std::min_element(row_scales_.begin(), row_scales_.end()),
      *std::max_element(row_scales_.begin(), row_scales_.end()),
      *std::min_element(col_scales_.begin(), col_scales_.end()),
      *std::max_element(col_scales_.begin(), col_scales_.end()));
  return output;
}

void SparseMatrixScaler::Scale(GlopParameters::ScalingAlgorithm method) {
  // This is an implementation of the algorithm described in
  // Benichou, M., Gauthier, J-M., Hentges, G., and Ribiere, G.,
  // "The efficient solution of large-scale linear programming problems —
  // some algorithmic techniques and computational results,"
  // Mathematical Programming 13(3) (December 1977).
  // http://www.springerlink.com/content/j3367676856m0064/
  DCHECK(matrix_ != nullptr);
  Fractional max_magnitude;
  Fractional min_magnitude;
  matrix_->ComputeMinAndMaxMagnitudes(&min_magnitude, &max_magnitude);
  if (min_magnitude == 0.0) {
    DCHECK_EQ(0.0, max_magnitude);
    return;  // Null matrix: nothing to do.
  }
  VLOG(1) << "Before scaling:\n" << DebugInformationString();
  if (method == GlopParameters::LINEAR_PROGRAM) {
    Status lp_status = LPScale();
    // Revert to the default scaling method if there is an error with the LP.
    if (lp_status.ok()) {
      return;
    } else {
      VLOG(1) << "Error with LP scaling: " << lp_status.error_message();
    }
  }
  // TODO(user): Decide precisely for which value of dynamic range we should cut
  // off geometric scaling.
  const Fractional dynamic_range = max_magnitude / min_magnitude;
  const Fractional kMaxDynamicRangeForGeometricScaling = 1e20;
  if (dynamic_range < kMaxDynamicRangeForGeometricScaling) {
    const int kScalingIterations = 4;
    const Fractional kVarianceThreshold(10.0);
    for (int iteration = 0; iteration < kScalingIterations; ++iteration) {
      const RowIndex num_rows_scaled = ScaleRowsGeometrically();
      const ColIndex num_cols_scaled = ScaleColumnsGeometrically();
      const Fractional variance = VarianceOfAbsoluteValueOfNonZeros();
      VLOG(1) << "Geometric scaling iteration " << iteration
              << ". Rows scaled = " << num_rows_scaled
              << ", columns scaled = " << num_cols_scaled << "\n";
      VLOG(1) << DebugInformationString();
      if (variance < kVarianceThreshold ||
          (num_cols_scaled == 0 && num_rows_scaled == 0)) {
        break;
      }
    }
  }
  const RowIndex rows_equilibrated = EquilibrateRows();
  const ColIndex cols_equilibrated = EquilibrateColumns();
  VLOG(1) << "Equilibration step: Rows scaled = " << rows_equilibrated
          << ", columns scaled = " << cols_equilibrated << "\n";
  VLOG(1) << DebugInformationString();
}

namespace {
template <class I>
void ScaleVector(const util_intops::StrongVector<I, Fractional>& scale, bool up,
                 util_intops::StrongVector<I, Fractional>* vector_to_scale) {
  RETURN_IF_NULL(vector_to_scale);
  const I size(std::min(scale.size(), vector_to_scale->size()));
  if (up) {
    for (I i(0); i < size; ++i) {
      (*vector_to_scale)[i] *= scale[i];
    }
  } else {
    for (I i(0); i < size; ++i) {
      (*vector_to_scale)[i] /= scale[i];
    }
  }
}

template <typename InputIndexType>
ColIndex CreateOrGetScaleIndex(
    InputIndexType num, LinearProgram* lp,
    util_intops::StrongVector<InputIndexType, ColIndex>* scale_var_indices) {
  if ((*scale_var_indices)[num] == -1) {
    (*scale_var_indices)[num] = lp->CreateNewVariable();
  }
  return (*scale_var_indices)[num];
}
}  // anonymous namespace

void SparseMatrixScaler::ScaleRowVector(bool up, DenseRow* row_vector) const {
  DCHECK(row_vector != nullptr);
  ScaleVector(col_scales_, up, row_vector);
}

void SparseMatrixScaler::ScaleColumnVector(bool up,
                                           DenseColumn* column_vector) const {
  DCHECK(column_vector != nullptr);
  ScaleVector(row_scales_, up, column_vector);
}

Fractional SparseMatrixScaler::VarianceOfAbsoluteValueOfNonZeros() const {
  DCHECK(matrix_ != nullptr);
  Fractional sigma_square(0.0);
  Fractional sigma_abs(0.0);
  double n = 0.0;  // n is used in a calculation involving doubles.
  const ColIndex num_cols = matrix_->num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    for (const SparseColumn::Entry e : matrix_->column(col)) {
      const Fractional magnitude = fabs(e.coefficient());
      sigma_square += magnitude * magnitude;
      sigma_abs += magnitude;
      ++n;
    }
  }
  if (n == 0.0) return 0.0;

  // Since we know all the population (the non-zeros) and we are not using a
  // sample, the variance is defined as below.
  // For an explanation, see:
  // http://en.wikipedia.org/wiki/Variance
  //        #Population_variance_and_sample_variance
  return (sigma_square - sigma_abs * sigma_abs / n) / n;
}

// For geometric scaling, we compute the maximum and minimum magnitudes
// of non-zeros in a row (resp. column). Let us denote these numbers as
// max and min. We then scale the row (resp. column) by dividing the
// coefficients by sqrt(min * max).

RowIndex SparseMatrixScaler::ScaleRowsGeometrically() {
  DCHECK(matrix_ != nullptr);
  DenseColumn max_in_row(matrix_->num_rows(), 0.0);
  DenseColumn min_in_row(matrix_->num_rows(), kInfinity);
  const ColIndex num_cols = matrix_->num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    for (const SparseColumn::Entry e : matrix_->column(col)) {
      const Fractional magnitude = fabs(e.coefficient());
      const RowIndex row = e.row();
      if (magnitude != 0.0) {
        max_in_row[row] = std::max(max_in_row[row], magnitude);
        min_in_row[row] = std::min(min_in_row[row], magnitude);
      }
    }
  }
  const RowIndex num_rows = matrix_->num_rows();
  DenseColumn scaling_factor(num_rows, 0.0);
  for (RowIndex row(0); row < num_rows; ++row) {
    if (max_in_row[row] == 0.0) {
      scaling_factor[row] = 1.0;
    } else {
      DCHECK_NE(kInfinity, min_in_row[row]);
      scaling_factor[row] = std::sqrt(max_in_row[row] * min_in_row[row]);
    }
  }
  return ScaleMatrixRows(scaling_factor);
}

ColIndex SparseMatrixScaler::ScaleColumnsGeometrically() {
  DCHECK(matrix_ != nullptr);
  ColIndex num_cols_scaled(0);
  const ColIndex num_cols = matrix_->num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    Fractional max_in_col(0.0);
    Fractional min_in_col(kInfinity);
    for (const SparseColumn::Entry e : matrix_->column(col)) {
      const Fractional magnitude = fabs(e.coefficient());
      if (magnitude != 0.0) {
        max_in_col = std::max(max_in_col, magnitude);
        min_in_col = std::min(min_in_col, magnitude);
      }
    }
    if (max_in_col != 0.0) {
      const Fractional factor(std::sqrt(ToDouble(max_in_col * min_in_col)));
      ScaleMatrixColumn(col, factor);
      num_cols_scaled++;
    }
  }
  return num_cols_scaled;
}

// For equilibration, we compute the maximum magnitude of non-zeros
// in a row (resp. column), and then scale the row (resp. column) by dividing
// the coefficients this maximum magnitude.
// This brings the largest coefficient in a row equal to 1.0.

RowIndex SparseMatrixScaler::EquilibrateRows() {
  DCHECK(matrix_ != nullptr);
  const RowIndex num_rows = matrix_->num_rows();
  DenseColumn max_magnitudes(num_rows, 0.0);
  const ColIndex num_cols = matrix_->num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    for (const SparseColumn::Entry e : matrix_->column(col)) {
      const Fractional magnitude = fabs(e.coefficient());
      if (magnitude != 0.0) {
        const RowIndex row = e.row();
        max_magnitudes[row] = std::max(max_magnitudes[row], magnitude);
      }
    }
  }
  for (RowIndex row(0); row < num_rows; ++row) {
    if (max_magnitudes[row] == 0.0) {
      max_magnitudes[row] = 1.0;
    }
  }
  return ScaleMatrixRows(max_magnitudes);
}

ColIndex SparseMatrixScaler::EquilibrateColumns() {
  DCHECK(matrix_ != nullptr);
  ColIndex num_cols_scaled(0);
  const ColIndex num_cols = matrix_->num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional max_magnitude = InfinityNorm(matrix_->column(col));
    if (max_magnitude != 0.0 && max_magnitude != 1.0) {
      ScaleMatrixColumn(col, max_magnitude);
      num_cols_scaled++;
    }
  }
  return num_cols_scaled;
}

RowIndex SparseMatrixScaler::ScaleMatrixRows(const DenseColumn& factors) {
  // Matrix rows are scaled by dividing their coefficients by factors[row].
  DCHECK(matrix_ != nullptr);
  const RowIndex num_rows = matrix_->num_rows();
  DCHECK_EQ(num_rows, factors.size());
  RowIndex num_rows_scaled(0);
  for (RowIndex row(0); row < num_rows; ++row) {
    const Fractional factor = factors[row];
    DCHECK_NE(0.0, factor);
    if (factor != 1.0) {
      ++num_rows_scaled;
      row_scales_[row] *= factor;
    }
  }

  const ColIndex num_cols = matrix_->num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    matrix_->mutable_column(col)->ComponentWiseDivide(factors);
  }

  return num_rows_scaled;
}

void SparseMatrixScaler::ScaleMatrixColumn(ColIndex col, Fractional factor) {
  // A column is scaled by dividing by factor.
  DCHECK(matrix_ != nullptr);
  DCHECK_NE(0.0, factor);
  col_scales_[col] *= factor;
  matrix_->mutable_column(col)->DivideByConstant(factor);
}

Status SparseMatrixScaler::LPScale() {
  DCHECK(matrix_ != nullptr);

  auto linear_program = std::make_unique<LinearProgram>();
  GlopParameters params;
  auto simplex = std::make_unique<RevisedSimplex>();
  simplex->SetParameters(params);

  // Begin linear program construction.
  // Beta represents the largest distance from zero among the constraint pairs.
  // It resembles a slack variable because the 'objective' of each constraint is
  // to cancel out the log "w" of the original nonzero |a_ij| (a.k.a. |a_rc|).
  // Approaching 0 by addition in log space is the same as approaching 1 by
  // multiplication in linear space. Hence, each variable's log magnitude is
  // subtracted from the log row scale and log column scale. If the sum is
  // positive, the positive constraint is trivially satisfied, but the negative
  // constraint will determine the minimum necessary value of beta for that
  // variable and scaling factors, and vice versa.
  // For an MxN matrix, the resulting scaling LP has M+N+1 variables and
  // O(M*N) constraints (2*M*N at maximum). As a result, using this LP to scale
  // another linear program, will typically increase the time to
  // optimization by a factor of 4, and has increased the time of some benchmark
  // LPs by up to 10.

  // Indices to variables in the LinearProgram populated by
  // GenerateLinearProgram.
  util_intops::StrongVector<ColIndex, ColIndex> col_scale_var_indices;
  util_intops::StrongVector<RowIndex, ColIndex> row_scale_var_indices;
  row_scale_var_indices.resize(RowToIntIndex(matrix_->num_rows()), kInvalidCol);
  col_scale_var_indices.resize(ColToIntIndex(matrix_->num_cols()), kInvalidCol);
  const ColIndex beta = linear_program->CreateNewVariable();
  linear_program->SetVariableBounds(beta, -kInfinity, kInfinity);
  // Default objective is to minimize.
  linear_program->SetObjectiveCoefficient(beta, 1);
  matrix_->CleanUp();
  const ColIndex num_cols = matrix_->num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    // This is the variable representing the log of the scale factor for col.
    const ColIndex column_scale = CreateOrGetScaleIndex<ColIndex>(
        col, linear_program.get(), &col_scale_var_indices);
    linear_program->SetVariableBounds(column_scale, -kInfinity, kInfinity);
    for (const SparseColumn::Entry e : matrix_->column(col)) {
      const Fractional log_magnitude = log2(std::abs(e.coefficient()));
      const RowIndex row = e.row();

      // This is the variable representing the log of the scale factor for row.
      const ColIndex row_scale = CreateOrGetScaleIndex<RowIndex>(
          row, linear_program.get(), &row_scale_var_indices);

      linear_program->SetVariableBounds(row_scale, -kInfinity, kInfinity);
      // clang-format off
      // This is derived from the formulation in
      // min β
      // Subject to:
      // ∀ c∈C, v∈V, p_{c,v} ≠ 0.0, w_{c,v} + s^{var}_v + s^{comb}_c + β ≥ 0.0
      // ∀ c∈C, v∈V, p_{c,v} ≠ 0.0, w_{c,v} + s^{var}_v + s^{comb}_c     ≤ β
      // If a variable is integer, its scale factor is zero.
      // clang-format on

      // Start with the constraint w_cv + s_c + s_v + beta >= 0.
      const RowIndex positive_constraint =
          linear_program->CreateNewConstraint();
      // Subtract the constant w_cv from both sides.
      linear_program->SetConstraintBounds(positive_constraint, -log_magnitude,
                                          kInfinity);
      // +s_c, meaning (log) scale of the constraint C, pointed by row_scale.
      linear_program->SetCoefficient(positive_constraint, row_scale, 1);
      // +s_v, meaning (log) scale of the variable V, pointed by column_scale.
      linear_program->SetCoefficient(positive_constraint, column_scale, 1);
      // +beta
      linear_program->SetCoefficient(positive_constraint, beta, 1);

      // Construct the constraint w_cv + s_c + s_v <= beta.
      const RowIndex negative_constraint =
          linear_program->CreateNewConstraint();
      // Subtract w (and beta) from both sides.
      linear_program->SetConstraintBounds(negative_constraint, -kInfinity,
                                          -log_magnitude);
      // +s_c, meaning (log) scale of the constraint C, pointed by row_scale.
      linear_program->SetCoefficient(negative_constraint, row_scale, 1);
      // +s_v, meaning (log) scale of the variable V, pointed by column_scale.
      linear_program->SetCoefficient(negative_constraint, column_scale, 1);
      // -beta
      linear_program->SetCoefficient(negative_constraint, beta, -1);
    }
  }
  // End linear program construction.

  linear_program->AddSlackVariablesWhereNecessary(false);
  const Status simplex_status =
      simplex->Solve(*linear_program, TimeLimit::Infinite().get());
  if (!simplex_status.ok()) {
    return simplex_status;
  } else {
    // Now the solution variables can be interpreted and translated from log
    // space.
    // For each row scale, unlog it and scale the constraints and constraint
    // bounds.
    const ColIndex num_cols = matrix_->num_cols();
    for (ColIndex col(0); col < num_cols; ++col) {
      const Fractional column_scale =
          exp2(-simplex->GetVariableValue(CreateOrGetScaleIndex<ColIndex>(
              col, linear_program.get(), &col_scale_var_indices)));
      ScaleMatrixColumn(col, column_scale);
    }
    const RowIndex num_rows = matrix_->num_rows();
    DenseColumn row_scale(num_rows, 0.0);
    for (RowIndex row(0); row < num_rows; ++row) {
      row_scale[row] =
          exp2(-simplex->GetVariableValue(CreateOrGetScaleIndex<RowIndex>(
              row, linear_program.get(), &row_scale_var_indices)));
    }
    ScaleMatrixRows(row_scale);
    return Status::OK();
  }
}

}  // namespace glop
}  // namespace operations_research
