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


// The SparseMatrixScaler class provides tools to scale a SparseMatrix, i.e.
// reduce the range of its coefficients and make for each column and each row
// the maximum magnitude of its coefficients equal to 1.
//
// In the case there are bounds or costs on the columns or a right-hand side
// related to each row, SparseMatrixScaler provides the tools to scale those
// appropriately.
//
// More precisely, suppose we have the Linear Program:
//   min c.x
//   s.t A.x = b
//   with l <= x <= u
//
// The rows of A are scaled by left-multiplying it by a diagonal matrix R
// whose elements R_ii correspond to the scaling factor of row i.
// The columns of A are scaled by right-multiplying it by a diagonal matrix C
// whose elements C_jj correspond to the scaling factor of column j.
//
// We obtain a matrix A' = R.A.C.
//
// We wish to scale x, c, b, l, u so as to work on the scaled problem:
//   min c'.x'
//   s.t A'.x' = b'
//   with l' <= x' <= u'
//
// The right-hand side b needs to be left-multiplied by R, the cost function c
// needs to be right-multiplied by C. Finally, x, and its lower- and upper-bound
// vectors l and u need to be left-multiplied by C^-1.
//
// Note also that the dual vector y is scaled as follows:
// y'.R = y, thus y' = y.R^-1.
//
// The complete transformation is thus:
//   A' = R.A.C,
//   b' = R.b,
//   c' = c.C,
//   x' = C^-1.x,
//   l' = C^-1.l,
//   u' = C^-1.u,
//   y' = y.R^-1.
//
// The validity of the above transformation can be checked by computing:
//   c'.x' =  c.C.C^-1.x = c.x.
// and:
//   A'.x' = R.A.C.C^-1.x = R.A.x = R.b = b'.

#ifndef OR_TOOLS_LP_DATA_MATRIX_SCALER_H_
#define OR_TOOLS_LP_DATA_MATRIX_SCALER_H_

#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/macros.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/lp_data/lp_types.h"

namespace operations_research {
namespace glop {

class SparseMatrix;

class SparseMatrixScaler {
 public:
  SparseMatrixScaler();

  // Initializes the object with the SparseMatrix passed as argument.
  // The row and column scaling factors are all set to 1.0 (i.e. no scaling.)
  // In terms of matrices, R and C are set to identity matrices.
  void Init(SparseMatrix* matrix);

  // Clears the object, and puts it back into the same state as after being
  // constructed.
  void Clear();

  // Returns the scaling factor of the given row/col. If the given row/col is
  // outside the range of the matrix used in Init(), returns 1.0. This is to
  // simplify the use of the scaler if the matrix was extended afterwards.
  Fractional row_scale(RowIndex row) const;
  Fractional col_scale(ColIndex col) const;

  // Scales the matrix.
  void Scale();

  // Unscales the matrix.
  void Unscale();

  // Scales a row vector up or down (depending whether parameter up is true or
  // false) using the scaling factors determined by Scale().
  // Scaling up means multiplying by the scaling factors, while scaling down
  // means dividing by the scaling factors.
  void ScaleRowVector(bool up, DenseRow* row_vector) const;

  // Scales a column vector up or down (depending whether parameter up is true
  // or false) using the scaling factors determined by Scale().
  // Scaling up means multiplying by the scaling factors, while scaling down
  // means dividing by the scaling factors.
  void ScaleColumnVector(bool up, DenseColumn* column_vector) const;

  // Computes the variance of the non-zero coefficients of the matrix.
  // Used by Scale() do decide when to stop.
  Fractional VarianceOfAbsoluteValueOfNonZeros() const;

  // Scales the rows. Returns the number of rows that have been scaled.
  // Helper function to Scale().
  RowIndex ScaleRowsGeometrically();

  // Scales the columns. Returns the number of columns that have been scaled.
  // Helper function to Scale().
  ColIndex ScaleColumnsGeometrically();

  // Equilibrates the rows. Returns the number of rows that have been scaled.
  // Helper function to Scale().
  RowIndex EquilibrateRows();

  // Equilibrates the Columns. Returns the number of columns that have been
  // scaled. Helper function to Scale().
  ColIndex EquilibrateColumns();

 private:
  // Scales the row indexed by row by 1/factor.
  // Used by ScaleMatrixRowsGeometrically and EquilibrateRows.
  RowIndex ScaleMatrixRows(const DenseColumn& factors);

  // Scales the column index by col by 1/factor.
  // Used by ScaleColumnsGeometrically and EquilibrateColumns.
  void ScaleMatrixColumn(ColIndex col, Fractional factor);

  // Returns a std::string containing information on the progress of the scaling
  // algorithm. This is not meant to be called in an optimized mode as it takes
  // some time to compute the displayed quantities.
  std::string DebugInformationString() const;

  // Pointer to the matrix to be scaled.
  SparseMatrix* matrix_;

  // Array of scaling factors for each row. Indexed by row number.
  DenseColumn row_scale_;

  // Array of scaling factors for each column. Indexed by column number.
  DenseRow col_scale_;

  DISALLOW_COPY_AND_ASSIGN(SparseMatrixScaler);
};

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_LP_DATA_MATRIX_SCALER_H_
