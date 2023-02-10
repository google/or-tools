// Copyright 2010-2022 Google LLC
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

#ifndef PDLP_TEST_UTIL_H_
#define PDLP_TEST_UTIL_H_

#include <cstdint>
#include <sstream>
#include <string>
#include <tuple>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ortools/pdlp/quadratic_program.h"

namespace operations_research::pdlp {

// Returns a small LP with all 4 patterns of which lower and upper bounds on the
// constraints are finite and similarly for the variables.
// min 5.5 x_0 - 2 x_1 - x_2 +   x_3 - 14 s.t.
//     2 x_0 +     x_1 +   x_2 + 2 x_3  = 12
//       x_0 +             x_2          <=  7
//     4 x_0                            >=  -4
//    -1 <=            1.5 x_2 -   x_3  <= 1
//   -infinity <= x_0 <= infinity
//          -2 <= x_1 <= infinity
//   -infinity <= x_2 <= 6
//         2.5 <= x_3 <= 3.5
//
// Optimal solutions:
//  Primal: [-1, 8, 1, 2.5]
//  Dual: [-2, 0, 2.375, 2.0/3]
//  Value: -5.5 - 16 -1 + 2.5 - 14 = -34
QuadraticProgram TestLp();

// Verifies that the given QuadraticProgram equals TestLp().
void VerifyTestLp(const QuadraticProgram& qp, bool maximize = false);

// Returns a "tiny" test LP.
//   min 5 x_1 + 2 x_2 + x_3 +   x_4 - 14 s.t.
//   2 x_1 +   x_2 + x_3 + 2 x_4  = 12
//     x_1 +         x_3         >=  7
//                   x_3 -   x_4 >=  1
//   0 <= x_1 <= 2
//   0 <= x_2 <= 4
//   0 <= x_3 <= 6
//   0 <= x_4 <= 3
//
// Optimum solutions:
//   Primal: x_1 = 1, x_2 = 0, x_3 = 6, x_4 = 2. Value: 5 + 0 + 6 + 2 - 14 = -1.
//   Dual: [0.5, 4.0, 0.0]  Value: 6 + 28 - 3.5*6 - 14 = -1
//   Reduced costs: [0.0, 1.5, -3.5, 0.0]
QuadraticProgram TinyLp();

// Returns a correlation clustering LP.
// This is the LP for minimizing disagreements for correlation clustering for
// the 4-vertex graph
//    1 - 3 - 4
//    | /
//    2
// In integer solutions x_ij is 1 if i and j are in the same cluster and 0
// otherwise. The 6 variables are in the order
//  x_12, x_13, x_14, x_23, x_24, x_34.
// For any distinct i,j,k there's a triangle inequality
//   (1-x_ik) <= (1-x_ij) + (1-x_jk)
// i.e.
//   -x_ij - x_jk + x_ik >= -1.
// For brevity we only include 3 out of the 12 possible triangle inequalities:
// two needed in the optimal solution and 1 other.
//
// Optimal solutions:
// Primal: [1, 1, 0, 1, 0, 0]
// Dual: Multiple.
// Value: 1.
QuadraticProgram CorrelationClusteringLp();

// Returns another 4-vertex correlation clustering LP.
//
// The variables are x_12, x_13, x_14, x_23, x_24, and x_34.
// This time the graph is a star centered at vertex 1.
// Only the three triangle inequalities that are needed are included.
// Optimal solutions:
// Primal: [0.5, 0.5, 0.5, 0.0, 0.0, 0.0]
// Dual: [0.5, 0.5, 0.5]
// Value: 1.5
QuadraticProgram CorrelationClusteringStarLp();

// Returns a small test QP.
//   min 2 x_0^2 + 0.5 x_1^2 - x_0 - x_1 + 5 s.t.
//   x_0 + x_1 <= 1
//    1 <= x_0 <= 2
//   -2 <= x_1 <= 4
//
// Optimal solutions:
//   Primal: [1.0, 0.0]
//   Dual: [-1.0]
//   Reduced costs: [4.0, 0.0]
//   Value: 2 - 1 + 5 = 6
QuadraticProgram TestDiagonalQp1();

// Verifies that the given QuadraticProgram equals TestDiagonalQp1().
void VerifyTestDiagonalQp1(const QuadraticProgram& qp, bool maximize = false);

// Returns a small diagonal QP.
//   min 0.5 x_0^2 + 0.5 x_1^2 - 3 x_0 - x_1 s.t.
//   x_0 - x_1 = 2
//   x_0 >= 0
//   x_1 >= 0
// Optimal solutions:
//   Primal: [3, 1]
//   Dual: [0]
//   Value: -5
//   Reduced costs: [0, 0]
QuadraticProgram TestDiagonalQp2();

// Returns a small diagonal QP.
//   min 0.5 x_1^2 + x_2^2 + x_0 - x_2 s.t.
//   x_0 - x_2 = 1
//  2x_0       = 4
//   x_0, x_1, x_2 >= 0
// Optimal solutions:
//   Primal: [2, 0, 1]
//   Dual: [-1, 1]
//   Value: 2
//   Reduced costs: [0, 0, 0]
QuadraticProgram TestDiagonalQp3();

// Returns a small invalid LP.
//   min x_0 + x_1 s.t.
//    2.0 <= x_0 - x_1 <= 1.0
//    0.0 <= x_0
//    0.0 <= x_1
QuadraticProgram SmallInvalidProblemLp();

// Returns a small LP that's invalid due to inconsistent variable bounds.
//   min x_0 + x_1 s.t.
//           x_0 - x_1 <= 1.0
//    2.0 <= x_0 <= 1.0
//    0.0 <= x_1
QuadraticProgram SmallInconsistentVariableBoundsLp();

// Returns a small test LP with infeasible primal.
//   min x_0 + x_1 s.t.
//           x_0 - x_1 <= 1.0
//          -x_0 + x_1 <= -2.0
//    0.0 <= x_0
//    0.0 <= x_1
QuadraticProgram SmallPrimalInfeasibleLp();

// Returns a small test LP with infeasible dual.
//   min - x_0 - x_1 s.t.
//            x_0 - x_1 <= 1.0
//           -x_0 + x_1 <= 2.0
//    0.0 <= x_0
//    0.0 <= x_1
// This is the SmallPrimalInfeasibleLp with the objective vector negated and
// with the second constraint changed to make it feasible.
QuadraticProgram SmallDualInfeasibleLp();

// Returns a small test LP with infeasible primal and dual.
//   min - x_0 - x_1 s.t.
//           x_0 - x_1 <= 1.0
//          -x_0 + x_1 <= -2.0
//    0.0 <= x_0
//    0.0 <= x_1
// This is just the SmallPrimalInfeasibleLp with the objective vector
// negated.
QuadraticProgram SmallPrimalDualInfeasibleLp();

// Returns a small lp for which optimality conditions are met by x=(0, 0), y=(0,
// 0) if one doesn't check that x satisfies the variable bounds. Analogously,
// the assignment x=(1, 0), y = -(1, 1) also satisfies the optimality conditions
// if one doesn't check dual variable bounds.
//   min  -4 x_0 s.t.
//           x_0 + x_1 <= 2.0
//           x_0 + 2x_1 <= 2.0
//    0.5 <= x_0 <= 2.0
//    0.5 <= x_1 <= 2.0
QuadraticProgram SmallInitializationLp();

// Returns a small LP with 2 variables and zero constraints (excluding variable
// bounds), resulting in an empty constraint matrix (zero rows) and empty lower
// and upper constraint bounds.
//   min   4 x_0 s.t.
//    0 <= x_0
//         x_1 <= 0
QuadraticProgram LpWithoutConstraints();

// Converts a sparse matrix into a dense matrix in the format suitable for
// the matcher `EigenArrayEq`. Example usage:
//   `EXPECT_THAT(ToDense(sparse_mat), EigenArrayEq<double>({{1, 1}}));`
::Eigen::ArrayXXd ToDense(
    const Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t>& sparse_mat);

// gMock matchers for Eigen.

namespace internal {

MATCHER_P(TupleIsNear, tolerance, "is near") {
  return std::abs(std::get<0>(arg) - std::get<1>(arg)) <= tolerance;
}

MATCHER(TupleFloatEq, "is almost equal to") {
  testing::Matcher<float> matcher = testing::FloatEq(std::get<1>(arg));
  return matcher.Matches(std::get<0>(arg));
}

// Convert a nested `absl::Span` to a 2D `Eigen::Array`. Spans are implicitly
// constructable from initializer lists and vectors, so this conversion is used
// in `EigenArrayNear` and `EigenArrayEq` to support syntax like
// `EXPECT_THAT(array2d, EigenArrayNear<int>({{1, 2}, {3, 4}}, tolerance);`
// This conversion creates a copy of the slice data, so it is safe to use the
// result even after the original slices vanish.
template <typename T>
Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> EigenArray2DFromNestedSpans(
    absl::Span<const absl::Span<const T>> rows) {
  Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> result(0, rows.size());
  if (!rows.empty()) {
    result.resize(rows.size(), rows[0].size());
  }
  for (int i = 0; i < rows.size(); ++i) {
    CHECK_EQ(rows[0].size(), rows[i].size());
    result.row(i) = Eigen::Map<const Eigen::Array<T, Eigen::Dynamic, 1>>(
        &rows[i][0], rows[i].size());
  }
  return result;
}

}  // namespace internal

// Defines a gMock matcher that tests whether two numeric arrays are
// approximately equal in the sense of maximum absolute difference. The element
// value type may be float, double, or integral types.
//
// Example:
// `vector<double> output = ComputeVector();`
// `vector<double> expected({-1.5333, sqrt(2), M_PI});`
// `EXPECT_THAT(output, FloatArrayNear(expected, 1.0e-3));`
template <typename ContainerType>
decltype(testing::Pointwise(internal::TupleIsNear(0.0), ContainerType()))
FloatArrayNear(const ContainerType& container, double tolerance) {
  return testing::Pointwise(internal::TupleIsNear(tolerance), container);
}

// Defines a gMock matcher acting as an elementwise version of `FloatEq()` for
// arrays of real floating point types. It tests whether two arrays are
// pointwise equal within 4 units in the last place (ULP) in float precision
// [http://en.wikipedia.org/wiki/Unit_in_the_last_place]. Roughly, 4 ULPs is
// 2^-21 times the absolute value, or 0.00005%. Exceptionally, zero matches
// values with magnitude less than 5.6e-45 (2^-147), infinities match infinities
// of the same sign, and NaNs don't match anything.
//
// Example:
// `vector<float> output = ComputeVector();`
// `vector<float> expected({-1.5333, sqrt(2), M_PI});`
// `EXPECT_THAT(output, FloatArrayEq(expected));`
template <typename ContainerType>
decltype(testing::Pointwise(internal::TupleFloatEq(), ContainerType()))
FloatArrayEq(const ContainerType& container) {
  return testing::Pointwise(internal::TupleFloatEq(), container);
}

// Call `.eval()` on input and convert it to a column major representation.
template <typename EigenType>
Eigen::Array<typename EigenType::Scalar, Eigen::Dynamic, Eigen::Dynamic,
             Eigen::ColMajor>
EvalAsColMajorEigenArray(const EigenType& input) {
  return input.eval();
}

// Wrap a column major `Eigen::Array` as an `absl::Span`.
template <typename Scalar>
absl::Span<const Scalar> EigenArrayAsSpan(
    const Eigen::Array<Scalar, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor>&
        array) {
  return absl::Span<const Scalar>(array.data(), array.size());
}

// Gmock matcher to test whether all elements in an array match expected_array
// within the specified tolerance, and print a detailed error message pointing
// to the first mismatched element if they do not. Essentially an elementwise
// version of `testing::DoubleNear` for Eigen arrays.
//
// Example:
// `Eigen::ArrayXf expected = ...`
// `EXPECT_THAT(actual_arrayxf, EigenArrayNear(expected, 1.0e-5));`
MATCHER_P2(EigenArrayNear, expected_array, tolerance,
           "array is near " + testing::PrintToString(expected_array) +
               " within tolerance " + testing::PrintToString(tolerance)) {
  if (arg.rows() != expected_array.rows() ||
      arg.cols() != expected_array.cols()) {
    *result_listener << "where shape (" << expected_array.rows() << ", "
                     << expected_array.cols() << ") doesn't match ("
                     << arg.rows() << ", " << arg.cols() << ")";
    return false;
  }
  // Call `.eval()` to allow callers to pass in Eigen expressions and possibly
  // noncontiguous objects, e.g. `Eigen::ArrayXf::Zero(10)` or `Map` with a
  // stride. Arrays are represented in column major order for consistent
  // comparison.
  auto realized_expected_array = EvalAsColMajorEigenArray(expected_array);
  auto realized_actual_array = EvalAsColMajorEigenArray(arg);
  return ExplainMatchResult(
      FloatArrayNear(EigenArrayAsSpan(realized_expected_array), tolerance),
      EigenArrayAsSpan(realized_actual_array), result_listener);
}

// Gmock matcher to test whether all elements in an array match expected_array
// within 4 units of least precision (ULP) in float precision. Essentially an
// elementwise version of `testing::FloatEq` for Eigen arrays.
//
// Example:
// `Eigen::ArrayXf expected = ...`
// `EXPECT_THAT(actual_arrayxf, EigenArrayEq(expected));`
MATCHER_P(EigenArrayEq, expected_array,
          "array is almost equal to " +
              testing::PrintToString(expected_array)) {
  if (arg.rows() != expected_array.rows() ||
      arg.cols() != expected_array.cols()) {
    *result_listener << "where shape (" << expected_array.rows() << ", "
                     << expected_array.cols() << ") doesn't match ("
                     << arg.rows() << ", " << arg.cols() << ")";
    return false;
  }
  // Call `.eval()` to allow callers to pass in Eigen expressions and possibly
  // noncontiguous objects, e.g. `Eigen::ArrayXf::Zero(10)` or `Map` with a
  // stride. Arrays are represented in column major order for consistent
  // comparison.
  auto realized_expected_array = EvalAsColMajorEigenArray(expected_array);
  auto realized_actual_array = EvalAsColMajorEigenArray(arg);
  return ExplainMatchResult(
      FloatArrayEq(EigenArrayAsSpan(realized_expected_array)),
      EigenArrayAsSpan(realized_actual_array), result_listener);
}

// The next few functions are syntactic sugar for `EigenArrayNear` and
// `EigenArrayEq` to allow callers to pass in non-Eigen types that can be
// statically initialized like (nested in the 2D case) initializer_lists, or
// vectors, etc. For example this specialization lets one make calls inlining
// expected_array like:
//   `EXPECT_THAT(array1d, EigenArrayNear<float>({0.1, 0.2}, tolerance));`
// or in the 2D case:
//   `EXPECT_THAT(array2d, EigenArrayNear<int>({{1, 2}, {3, 4}}, tolerance);`

template <typename T>
EigenArrayNearMatcherP2<Eigen::Array<T, Eigen::Dynamic, 1>, double>
EigenArrayNear(absl::Span<const T> data, double tolerance) {
  Eigen::Array<T, Eigen::Dynamic, 1> temp_array =
      Eigen::Map<const Eigen::Array<T, Eigen::Dynamic, 1>>(&data[0],
                                                           data.size());
  return EigenArrayNear(temp_array, tolerance);
}

template <typename T>
EigenArrayNearMatcherP2<Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic>, double>
EigenArrayNear(absl::Span<const absl::Span<const T>> rows, double tolerance) {
  return EigenArrayNear(internal::EigenArray2DFromNestedSpans(rows), tolerance);
}

template <typename T>
EigenArrayEqMatcherP<Eigen::Array<T, Eigen::Dynamic, 1>> EigenArrayEq(
    absl::Span<const T> data) {
  Eigen::Array<T, Eigen::Dynamic, 1> temp_array =
      Eigen::Map<const Eigen::Array<T, Eigen::Dynamic, 1>>(&data[0],
                                                           data.size());
  return EigenArrayEq(temp_array);
}

template <typename T>
EigenArrayEqMatcherP<Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic>>
EigenArrayEq(absl::Span<const absl::Span<const T>> rows) {
  return EigenArrayEq(internal::EigenArray2DFromNestedSpans(rows));
}

}  // namespace operations_research::pdlp

namespace Eigen {
// Pretty prints an `Eigen::Array` on a gunit test failures. See
// https://google.github.io/googletest/advanced.html#teaching-googletest-how-to-print-your-values
template <typename Scalar, int Rows, int Cols, int Options, int MaxRows,
          int MaxCols>
void PrintTo(const Array<Scalar, Rows, Cols, Options, MaxRows, MaxCols>& array,
             std::ostream* os) {
  IOFormat format(StreamPrecision, 0, ", ", ",\n", "[", "]", "[", "]");
  *os << "\n" << array.format(format);
}
}  // namespace Eigen

#endif  // PDLP_TEST_UTIL_H_
