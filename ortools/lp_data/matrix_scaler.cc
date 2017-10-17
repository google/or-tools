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


#include "ortools/lp_data/matrix_scaler.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/lp_data/lp_utils.h"
#include "ortools/lp_data/sparse.h"

namespace operations_research {
namespace glop {

SparseMatrixScaler::SparseMatrixScaler()
    : matrix_(nullptr), row_scale_(), col_scale_() {}

void SparseMatrixScaler::Init(SparseMatrix* matrix) {
  DCHECK(matrix != nullptr);
  matrix_ = matrix;
  row_scale_.resize(matrix_->num_rows(), 1.0);
  col_scale_.resize(matrix_->num_cols(), 1.0);
}

void SparseMatrixScaler::Clear() {
  matrix_ = nullptr;
  row_scale_.clear();
  col_scale_.clear();
}

Fractional SparseMatrixScaler::row_scale(RowIndex row) const {
  DCHECK_GE(row, 0);
  return row < row_scale_.size() ? row_scale_[row] : 1.0;
}

Fractional SparseMatrixScaler::col_scale(ColIndex col) const {
  DCHECK_GE(col, 0);
  return col < col_scale_.size() ? col_scale_[col] : 1.0;
}

std::string SparseMatrixScaler::DebugInformationString() const {
  // Note that some computations are redundant with the computations made in
  // some callees, but we do not care as this function is supposed to be called
  // with FLAGS_v set to 1.
  DCHECK(!row_scale_.empty());
  DCHECK(!col_scale_.empty());
  Fractional max_magnitude;
  Fractional min_magnitude;
  matrix_->ComputeMinAndMaxMagnitudes(&min_magnitude, &max_magnitude);
  const Fractional dynamic_range = max_magnitude / min_magnitude;
  std::string output = StringPrintf(
      "Min magnitude = %g, max magnitude = %g\n"
      "Dynamic range = %g\n"
      "Variance = %g\n"
      "Minimum row scale = %g, maximum row scale = %g\n"
      "Minimum col scale = %g, maximum col scale = %g\n",
      min_magnitude, max_magnitude, dynamic_range,
      VarianceOfAbsoluteValueOfNonZeros(),
      *std::min_element(row_scale_.begin(), row_scale_.end()),
      *std::max_element(row_scale_.begin(), row_scale_.end()),
      *std::min_element(col_scale_.begin(), col_scale_.end()),
      *std::max_element(col_scale_.begin(), col_scale_.end()));
  return output;
}

void SparseMatrixScaler::Scale() {
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
  RowIndex rows_equilibrated = EquilibrateRows();
  ColIndex cols_equilibrated = EquilibrateColumns();
  VLOG(1) << "Equilibration step: Rows scaled = " << rows_equilibrated
          << ", columns scaled = " << cols_equilibrated << "\n";
  VLOG(1) << DebugInformationString();
}

namespace {
template <class I>
void ScaleVector(const ITIVector<I, Fractional>& scale, bool up,
                 ITIVector<I, Fractional>* vector_to_scale) {
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

}  // anonymous namespace

void SparseMatrixScaler::ScaleRowVector(bool up, DenseRow* row_vector) const {
  DCHECK(row_vector != nullptr);
  ScaleVector(col_scale_, up, row_vector);
}

void SparseMatrixScaler::ScaleColumnVector(bool up,
                                           DenseColumn* column_vector) const {
  DCHECK(column_vector != nullptr);
  ScaleVector(row_scale_, up, column_vector);
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
      if (magnitude != 0.0) {
        sigma_square += magnitude * magnitude;
        sigma_abs += magnitude;
        ++n;
      }
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
      scaling_factor[row] = sqrt(max_in_row[row] * min_in_row[row]);
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
      const Fractional factor(sqrt(ToDouble(max_in_col * min_in_col)));
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
  DenseColumn max_magnitude(num_rows, 0.0);
  const ColIndex num_cols = matrix_->num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    for (const SparseColumn::Entry e : matrix_->column(col)) {
      const Fractional magnitude = fabs(e.coefficient());
      if (magnitude != 0.0) {
        const RowIndex row = e.row();
        max_magnitude[row] = std::max(max_magnitude[row], magnitude);
      }
    }
  }
  for (RowIndex row(0); row < num_rows; ++row) {
    if (max_magnitude[row] == 0.0) {
      max_magnitude[row] = 1.0;
    }
  }
  return ScaleMatrixRows(max_magnitude);
}

ColIndex SparseMatrixScaler::EquilibrateColumns() {
  DCHECK(matrix_ != nullptr);
  ColIndex num_cols_scaled(0);
  const ColIndex num_cols = matrix_->num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional max_magnitude = InfinityNorm(matrix_->column(col));
    if (max_magnitude != 0.0) {
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
      row_scale_[row] *= factor;
    }
  }

  const ColIndex num_cols = matrix_->num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    SparseColumn* const column = matrix_->mutable_column(col);
    if (column != nullptr) {
      column->ComponentWiseDivide(factors);
    }
  }

  return num_rows_scaled;
}

void SparseMatrixScaler::ScaleMatrixColumn(ColIndex col, Fractional factor) {
  // A column is scaled by dividing by factor.
  DCHECK(matrix_ != nullptr);
  col_scale_[col] *= factor;
  DCHECK_NE(0.0, factor);

  SparseColumn* const column = matrix_->mutable_column(col);
  if (column != nullptr) {
    column->DivideByConstant(factor);
  }
}

void SparseMatrixScaler::Unscale() {
  // Unscaling is easier than scaling since all scaling factors are stored.
  DCHECK(matrix_ != nullptr);
  const ColIndex num_cols = matrix_->num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional column_scale = col_scale_[col];
    DCHECK_NE(0.0, column_scale);

    SparseColumn* const column = matrix_->mutable_column(col);
    if (column != nullptr) {
      column->MultiplyByConstant(column_scale);
      column->ComponentWiseMultiply(row_scale_);
    }
  }
}

}  // namespace glop
}  // namespace operations_research
