// Copyright 2010-2024 Google LLC
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

#include "ortools/graph/one_tree_lower_bound.h"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/logging.h"
#include "ortools/base/path.h"
#include "ortools/routing/parsers/tsplib_parser.h"

namespace operations_research {
namespace {

TEST(OneTreeLBTest, VolgenantJonkerEmpty) {
  const double cost = ComputeOneTreeLowerBound(0, [](int /*from*/, int /*to*/) {
    ADD_FAILURE();  // Making sure the function is not being called.
    return 0;
  });
  EXPECT_EQ(0, cost);
}

TEST(OneTreeLBTest, HeldWolfeCrowderEmpty) {
  TravelingSalesmanLowerBoundParameters parameters;
  parameters.algorithm =
      TravelingSalesmanLowerBoundParameters::HeldWolfeCrowder;
  const double cost = ComputeOneTreeLowerBoundWithParameters(
      0,
      [](int /*from*/, int /*to*/) {
        ADD_FAILURE();  // Making sure the function is not being called.
        return 0;
      },
      parameters);
  EXPECT_EQ(0, cost);
}

TEST(OneTreeLBTest, VolgenantJonkerOneNode) {
  const double cost =
      ComputeOneTreeLowerBound(1, [](int /*from*/, int /*to*/) { return 0; });
  EXPECT_EQ(0, cost);
}

TEST(OneTreeLBTest, HeldWolfeCrowderOneNode) {
  TravelingSalesmanLowerBoundParameters parameters;
  parameters.algorithm =
      TravelingSalesmanLowerBoundParameters::HeldWolfeCrowder;
  const double cost = ComputeOneTreeLowerBoundWithParameters(
      1, [](int /*from*/, int /*to*/) { return 0; }, parameters);
  EXPECT_EQ(0, cost);
}

TEST(OneTreeLBTest, VolgenantJonkerTwoNodes) {
  const double cost =
      ComputeOneTreeLowerBound(2, [](int /*from*/, int /*to*/) { return 1; });
  EXPECT_EQ(2, cost);
}

TEST(OneTreeLBTest, HeldWolfeCrowderTwoNodes) {
  TravelingSalesmanLowerBoundParameters parameters;
  parameters.algorithm =
      TravelingSalesmanLowerBoundParameters::HeldWolfeCrowder;
  const double cost = ComputeOneTreeLowerBoundWithParameters(
      2, [](int /*from*/, int /*to*/) { return 1; }, parameters);
  EXPECT_EQ(2, cost);
}

TEST(OneTreeLBTest, Small) {
  const std::vector<int> x = {0, 0, 0, 1, 1, 1, 2, 2, 2};
  const std::vector<int> y = {0, 1, 2, 0, 1, 2, 0, 1, 2};
  double cost = ComputeOneTreeLowerBound(9, [&x, &y](int from, int to) {
    const int dx = std::abs(x[from] - x[to]);
    const int dy = std::abs(y[from] - y[to]);
    return dx + dy;
  });
  EXPECT_EQ(9, cost);
}

}  // namespace
}  // namespace operations_research
