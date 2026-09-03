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

#ifndef ORTOOLS_LP_DATA_LP_TEST_UTILS_H_
#define ORTOOLS_LP_DATA_LP_TEST_UTILS_H_

#include <cmath>

#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "ortools/base/strong_vector.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/lp_data/sparse_column.h"
#include "ortools/util/fp_utils.h"

namespace operations_research {
namespace glop {

static const Fractional kComparableEpsilon(sqrt(kEpsilon));

template <class IndexType, typename TestObjectType, typename ObjectType>
void CheckValues(const util_intops::StrongVector<IndexType, ObjectType>& values,
                 int expected_num_values,
                 const TestObjectType* expected_value) {
  CHECK(expected_value != nullptr);
  EXPECT_EQ(expected_num_values, values.size());
  for (int i = 0; i < values.size(); ++i) {
    EXPECT_EQ(ObjectType(expected_value[i]), values[IndexType(i)])
        << "At index i=" << i;
  }
}

template <class IndexType, class FractionalType, class TestObjectType>
void CheckFractionalValues(
    const util_intops::StrongVector<IndexType, FractionalType>& values,
    int expected_num_values, const TestObjectType* expected_value) {
  CHECK(expected_value != nullptr);
  EXPECT_EQ(expected_num_values, values.size());
  for (int i = 0; i < values.size(); ++i) {
    EXPECT_COMPARABLE(FractionalType(expected_value[i]), values[IndexType(i)],
                      kComparableEpsilon);
  }
}

template <class IndexType>
void ExpectFractionalVectorComparable(
    const util_intops::StrongVector<IndexType, Fractional>& v1,
    const util_intops::StrongVector<IndexType, Fractional>& v2) {
  EXPECT_EQ(v1.size(), v2.size());
  for (IndexType i(0); i < v1.size(); ++i) {
    EXPECT_COMPARABLE(v1[i], v2[i], kComparableEpsilon);
  }
}

//----------------------------------------------------------------------
// Random column and matrix generator.
//----------------------------------------------------------------------

// Generates a random sparse column of the given size and average density.
void FillSparseColumnRandomly(RowIndex num_rows, double density,
                              absl::BitGenRef generator, SparseColumn* column);

// Generates a random matrix of the given sizes.
// - Each coefficient is non-null with probability given by "density".
// - A non-zero coefficient is distributed uniformly in [-1.0, 1.0].
// - The columns are ordered by row.
void FillSparseMatrixRandomly(RowIndex num_rows, ColIndex num_cols,
                              double density, absl::BitGenRef generator,
                              SparseMatrix* matrix);

// Some precisions:
// - The diagonal coefficients will always be ones.
// - The other non-zero entries will uniformly be in [-1.0, 1.0].
void FillSparseMatrixWithRandomLowerTriangularMatrix(RowIndex num_rows,
                                                     double density,
                                                     absl::BitGenRef generator,
                                                     SparseMatrix* matrix);

// Some precisions:
// - The diagonal coefficients magnitude will be in [0.5, 1.0].
// - The other non-zero entries will uniformly be in [-1.0, 1.0].
void FillSparseMatrixWithRandomUpperTriangularMatrix(RowIndex num_rows,
                                                     double density,
                                                     absl::BitGenRef generator,
                                                     SparseMatrix* matrix);

}  // namespace glop
}  // namespace operations_research

#endif  // ORTOOLS_LP_DATA_LP_TEST_UTILS_H_
