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

#ifndef OR_TOOLS_MATH_OPT_CORE_SPARSE_COLLECTION_MATCHERS_H_
#define OR_TOOLS_MATH_OPT_CORE_SPARSE_COLLECTION_MATCHERS_H_

#include <cstdint>
#include <initializer_list>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {

SparseDoubleVectorProto MakeSparseDoubleVector(
    std::initializer_list<std::pair<int64_t, double>> pairs);

SparseBoolVectorProto MakeSparseBoolVector(
    std::initializer_list<std::pair<int64_t, bool>> pairs);

SparseDoubleMatrixProto MakeSparseDoubleMatrix(
    std::initializer_list<std::tuple<int64_t, int64_t, double>> values);

// Type of the argument of SparseVectorMatcher.
template <typename T>
using Pairs = std::initializer_list<std::pair<int64_t, const T>>;

// Here `pairs` must be a Pairs<T>.
//
// Usage:
//   EXPECT_THAT(v, SparseVectorMatcher(Pairs<double>{}));
//   EXPECT_THAT(v, SparseVectorMatcher(Pairs<double>{{2, 3.0}, {3, 2.0}}));
MATCHER_P(SparseVectorMatcher, pairs, "") {
  const auto iterable = MakeView(arg);
  const std::vector v(iterable.begin(), iterable.end());
  const std::vector<typename decltype(v)::value_type> expected(pairs.begin(),
                                                               pairs.end());

  return ::testing::ExplainMatchResult(::testing::ContainerEq(expected), v,
                                       result_listener);
}

// Type of the argument of SparseDoubleMatrixMatcher.
using Coefficient = std::tuple<int64_t, int64_t, const double>;
using Coefficients = std::initializer_list<Coefficient>;

// Here `coefficients` must be a Coefficients<T>.
//
// Usage:
//   EXPECT_THAT(v, SparseDoubleMatrixMatcher(Coefficients{}));
//   EXPECT_THAT(v, SparseDoubleMatrixMatcher(Coefficients{{2, 1, 3.0}, {3,
//   0, 2.0}}));
MATCHER_P(SparseDoubleMatrixMatcher, coefficients, "") {
  std::vector<Coefficient> v;
  for (int i = 0; i < arg.row_ids_size(); ++i) {
    v.emplace_back(arg.row_ids(i), arg.column_ids(i), arg.coefficients(i));
  }
  const std::vector<Coefficient> expected(coefficients.begin(),
                                          coefficients.end());

  return ::testing::ExplainMatchResult(::testing::ContainerEq(expected), v,
                                       result_listener);
}

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CORE_SPARSE_COLLECTION_MATCHERS_H_
