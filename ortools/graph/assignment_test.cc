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

#include "ortools/graph/assignment.h"

#include <limits>
#include <random>
#include <tuple>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"

namespace operations_research {

TEST(SimpleLinearSumAssignmentTest, Empty) {
  SimpleLinearSumAssignment assignment;
  EXPECT_EQ(SimpleLinearSumAssignment::OPTIMAL, assignment.Solve());
  EXPECT_EQ(0, assignment.OptimalCost());
  EXPECT_EQ(0, assignment.NumArcs());
  EXPECT_EQ(0, assignment.NumNodes());
}

TEST(SimpleLinearSumAssignmentTest, OptimumMatching) {
  SimpleLinearSumAssignment assignment;
  EXPECT_EQ(0, assignment.AddArcWithCost(0, 0, 0));
  EXPECT_EQ(1, assignment.AddArcWithCost(0, 1, 2));
  EXPECT_EQ(2, assignment.AddArcWithCost(1, 0, 3));
  EXPECT_EQ(3, assignment.AddArcWithCost(1, 1, 4));
  EXPECT_EQ(2, assignment.NumNodes());
  EXPECT_EQ(4, assignment.NumArcs());
  EXPECT_EQ(SimpleLinearSumAssignment::OPTIMAL, assignment.Solve());
  EXPECT_EQ(4, assignment.OptimalCost());
  EXPECT_EQ(0, assignment.RightMate(0));
  EXPECT_EQ(0, assignment.AssignmentCost(0));
  EXPECT_EQ(1, assignment.RightMate(1));
  EXPECT_EQ(4, assignment.AssignmentCost(1));
}

TEST(SimpleLinearSumAssignmentTest, OptimumMatchingWithNegativeCost) {
  SimpleLinearSumAssignment assignment;
  assignment.AddArcWithCost(0, 0, 2);
  assignment.AddArcWithCost(0, 1, -10);
  assignment.AddArcWithCost(1, 0, 3);
  assignment.AddArcWithCost(1, 1, -20);
  EXPECT_EQ(SimpleLinearSumAssignment::OPTIMAL, assignment.Solve());
  EXPECT_EQ(-18, assignment.OptimalCost());
  EXPECT_EQ(0, assignment.RightMate(0));
  EXPECT_EQ(2, assignment.AssignmentCost(0));
  EXPECT_EQ(1, assignment.RightMate(1));
  EXPECT_EQ(-20, assignment.AssignmentCost(1));
}

TEST(SimpleLinearSumAssignmentTest, InfeasibleProblem) {
  SimpleLinearSumAssignment assignment;
  assignment.AddArcWithCost(0, 1, 2);
  assignment.AddArcWithCost(0, 1, -10);
  assignment.AddArcWithCost(1, 1, 3);
  assignment.AddArcWithCost(1, 1, -20);
  EXPECT_EQ(SimpleLinearSumAssignment::INFEASIBLE, assignment.Solve());
  EXPECT_EQ(0, assignment.OptimalCost());
}

TEST(SimpleLinearSumAssignmentTest, Overflow) {
  using CostValue = SimpleLinearSumAssignment::CostValue;
  SimpleLinearSumAssignment assignment;
  assignment.AddArcWithCost(0, 0, std::numeric_limits<CostValue>::max());
  assignment.AddArcWithCost(0, 1, std::numeric_limits<CostValue>::max());
  assignment.AddArcWithCost(1, 0, std::numeric_limits<CostValue>::max());
  assignment.AddArcWithCost(1, 1, std::numeric_limits<CostValue>::max());
  EXPECT_EQ(SimpleLinearSumAssignment::POSSIBLE_OVERFLOW, assignment.Solve());
  EXPECT_EQ(0, assignment.OptimalCost());
}

void BM_SimpleLinearSumAssignment(benchmark::State& state) {
  using CostValue = SimpleLinearSumAssignment::CostValue;
  constexpr CostValue kCostLimit = 1000000;

  const int num_left = state.range(0);
  const int degree = state.range(1);

  std::mt19937 rng(12345);

  // Each left node is connected to `degree` right nodes.
  std::vector<std::tuple<int, int, CostValue>> arcs;
  arcs.reserve(num_left * degree);
  for (int left = 0; left < num_left; ++left) {
    for (int i = 0; i < degree; ++i) {
      const int right = (left + i) % num_left;
      const CostValue cost =
          absl::Uniform(rng, 0, absl::Uniform(rng, 0, kCostLimit));
      arcs.emplace_back(left, right, cost);
    }
  }

  for (auto _ : state) {
    SimpleLinearSumAssignment assignment;
    assignment.ReserveArcs(num_left * degree);
    for (const auto& [left, right, cost] : arcs) {
      assignment.AddArcWithCost(left, right, cost);
    }
    CHECK_EQ(assignment.Solve(), SimpleLinearSumAssignment::OPTIMAL);
  }
}
BENCHMARK(BM_SimpleLinearSumAssignment)->ArgPair(100, 2)->ArgPair(100, 20);

}  // namespace operations_research
