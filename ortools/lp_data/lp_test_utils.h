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
#include <tuple>

#include "absl/random/bit_gen_ref.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_vector.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/lp_data/sparse_column.h"
#include "ortools/util/fp_utils_testing.h"

namespace operations_research {
namespace glop {

static const Fractional kComparableEpsilon(sqrt(kEpsilon));

// Returns WithinAbsoluteOrRelativeTolerances() matcher with Fractional type and
// using kComparableEpsilon for both absolute and relative tolerance.
inline testing::Matcher<Fractional> ComparableFractional(
    const Fractional expected) {
  return WithinSameAbsoluteOrRelativeTolerance<Fractional>(expected,
                                                           kComparableEpsilon);
}

// Overload of ComparableFractional() that compares pairs of Fractional
// (represented as std::tuple<>), typically used with testing::Pointwise().
inline testing::Matcher<std::tuple<Fractional, Fractional>>
ComparableFractional() {
  return WithinSameAbsoluteOrRelativeTolerance<Fractional>(kComparableEpsilon);
}

// Returns a matcher for vectors of Fractional using ComparableFractional()
// matcher.
template <class IndexType>
testing::Matcher<util_intops::StrongVector<IndexType, Fractional>>
FractionalVectorComparable(
    const util_intops::StrongVector<IndexType, Fractional>& expected) {
  return testing::Pointwise(ComparableFractional(), expected);
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
