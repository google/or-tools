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

#include "ortools/math_opt/core/sparse_collection_matchers.h"

#include <initializer_list>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::testing::Not;

TEST(SparseVectorMatcherTest, Bool) {
  const SparseBoolVectorProto v = MakeSparseBoolVector({{3, true}, {4, false}});
  EXPECT_THAT(v, SparseVectorMatcher(Pairs<double>{{3, true}, {4, false}}));
  EXPECT_THAT(v, Not(SparseVectorMatcher(Pairs<double>{{3, true}, {4, true}})));
  EXPECT_THAT(v, Not(SparseVectorMatcher(
                     Pairs<double>{{3, true}, {4, false}, {5, true}})));
  EXPECT_THAT(v, Not(SparseVectorMatcher(Pairs<double>{{3, true}})));
}

TEST(SparseVectorMatcherTest, Double) {
  const SparseDoubleVectorProto v =
      MakeSparseDoubleVector({{3, 2.5}, {4, 4.0}});
  EXPECT_THAT(v, SparseVectorMatcher(Pairs<double>{{3, 2.5}, {4, 4.0}}));
  EXPECT_THAT(v, Not(SparseVectorMatcher(Pairs<double>{{3, 2.5}, {4, -4.0}})));
  EXPECT_THAT(
      v, Not(SparseVectorMatcher(Pairs<double>{{3, 2.5}, {4, 4.0}, {5, 2.0}})));
  EXPECT_THAT(v, Not(SparseVectorMatcher(Pairs<double>{{3, 2.5}})));
}

TEST(SparseDoubleMatrixMatcherTest, Double) {
  const auto m = MakeSparseDoubleMatrix({{3, 1, 2.5}, {4, 0, 4.0}});
  EXPECT_THAT(
      m, SparseDoubleMatrixMatcher(Coefficients{{3, 1, 2.5}, {4, 0, 4.0}}));
  EXPECT_THAT(
      m,
      Not(SparseDoubleMatrixMatcher(Coefficients{{3, 1, 2.5}, {4, 0, -4.0}})));
  EXPECT_THAT(m, Not(SparseDoubleMatrixMatcher(
                     Coefficients{{3, 1, 2.5}, {4, 0, 4.0}, {5, 1, 2.0}})));
  EXPECT_THAT(m, Not(SparseDoubleMatrixMatcher(Coefficients{{3, 1, 2.5}})));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
