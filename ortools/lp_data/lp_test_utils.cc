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

#include "ortools/lp_data/lp_test_utils.h"

#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/lp_data/sparse_column.h"

namespace operations_research {
namespace glop {

void FillSparseColumnRandomly(RowIndex num_rows, double density,
                              absl::BitGenRef generator, SparseColumn* column) {
  column->Clear();
  for (RowIndex row(0); row < num_rows; ++row) {
    if (absl::Bernoulli(generator, density)) {
      column->SetCoefficient(row, absl::Uniform<float>(generator, -1.0, 1.0));
    }
  }
  column->CheckNoDuplicates();
}

void FillSparseMatrixRandomly(RowIndex num_rows, ColIndex num_cols,
                              double density, absl::BitGenRef generator,
                              SparseMatrix* matrix) {
  matrix->PopulateFromZero(num_rows, num_cols);
  for (ColIndex col(0); col < num_cols; ++col) {
    FillSparseColumnRandomly(num_rows, density, generator,
                             matrix->mutable_column(col));
  }
}

void FillSparseMatrixWithRandomLowerTriangularMatrix(RowIndex num_rows,
                                                     double density,
                                                     absl::BitGenRef generator,
                                                     SparseMatrix* matrix) {
  const ColIndex num_cols(num_rows.value());
  matrix->PopulateFromZero(num_rows, num_cols);
  for (ColIndex col(0); col < num_cols; ++col) {
    SparseColumn* column = matrix->mutable_column(col);
    column->SetCoefficient(ColToRowIndex(col), 1.0);
    for (RowIndex row(col.value() + 1); row < num_rows; ++row) {
      if (absl::Bernoulli(generator, density)) {
        column->SetCoefficient(row, absl::Uniform<float>(generator, -1.0, 1.0));
      }
    }
  }
}

void FillSparseMatrixWithRandomUpperTriangularMatrix(RowIndex num_rows,
                                                     double density,
                                                     absl::BitGenRef generator,
                                                     SparseMatrix* matrix) {
  const ColIndex num_cols(num_rows.value());
  matrix->PopulateFromZero(num_rows, num_cols);
  for (ColIndex col(0); col < num_cols; ++col) {
    SparseColumn* column = matrix->mutable_column(col);
    for (RowIndex row(0); row < ColToRowIndex(col); ++row) {
      if (absl::Bernoulli(generator, density)) {
        column->SetCoefficient(row, absl::Uniform<float>(generator, -1.0, 1.0));
      }
    }
    column->SetCoefficient(ColToRowIndex(col),
                           (absl::Bernoulli(generator, 1.0 / 2) ? 1.0 : -1.0) *
                               absl::Uniform<float>(generator, 0.5, 1.0));
  }
}

}  // namespace glop
}  // namespace operations_research
