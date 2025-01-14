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

#include "ortools/sat/encoding.h"

#include <deque>
#include <random>
#include <vector>

#include "absl/random/distributions.h"
#include "gtest/gtest.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {
namespace {

TEST(MergeAllNodesWithDequeTest, BasicPropagation) {
  // We start with a sat solver and n Boolean variables.
  std::mt19937 random(12345);
  const int n = 456;
  SatSolver solver;
  solver.SetNumVariables(n);

  // We encode the full cardinality constraint on the n variables.
  std::deque<EncodingNode> repository;
  std::vector<EncodingNode*> nodes;
  for (int i = 0; i < n; ++i) {
    repository.push_back(EncodingNode::LiteralNode(
        Literal(BooleanVariable(i), true), Coefficient(0)));
    nodes.push_back(&repository.back());
  }
  const Coefficient an_upper_bound(1000);
  EncodingNode* root =
      MergeAllNodesWithDeque(an_upper_bound, nodes, &solver, &repository);
  EXPECT_EQ(root->lb(), 0);
  EXPECT_EQ(root->ub(), n);
  EXPECT_EQ(root->size(), n);
  EXPECT_EQ(root->depth(), 9);  // 2^9 = 512 which is the first value >= n.

  // We fix some of the n variables randomly, and check some property of the
  // Encoding nodes.
  for (int run = 0; run < 10; ++run) {
    const float density = run / 10;
    int exact_count = 0;
    solver.Backtrack(0);
    for (int i = 0; i < n; ++i) {
      const bool value = absl::Bernoulli(random, density);
      exact_count += value ? 1 : 0;
      EXPECT_TRUE(solver.EnqueueDecisionIfNotConflicting(
          Literal(BooleanVariable(i), value)));
    }
    EXPECT_EQ(solver.Solve(), SatSolver::FEASIBLE);

    // We use an exact encoding, so the number of affected variables at the root
    // level of the encoding should be exactly exact_count.
    if (exact_count > 0) {
      EXPECT_TRUE(solver.Assignment().LiteralIsTrue(
          root->GreaterThan(exact_count - 1)));
    }
    if (exact_count < n) {
      EXPECT_FALSE(
          solver.Assignment().LiteralIsTrue(root->GreaterThan(exact_count)));
    }
  }
}

TEST(LazyMergeAllNodeWithPQAndIncreaseLbTest, CorrectDepth) {
  // We start with a sat solver and n Boolean variables.
  std::mt19937 random(12345);
  const int n = 456;
  SatSolver solver;
  solver.SetNumVariables(n);

  // We encode the full cardinality constraint on the n variables.
  std::deque<EncodingNode> repository;
  std::vector<EncodingNode*> nodes;
  for (int i = 0; i < n; ++i) {
    repository.push_back(EncodingNode::LiteralNode(
        Literal(BooleanVariable(i), true), Coefficient(0)));
    nodes.push_back(&repository.back());
  }
  EncodingNode* root =
      LazyMergeAllNodeWithPQAndIncreaseLb(1, nodes, &solver, &repository);
  EXPECT_EQ(root->lb(), 1);
  EXPECT_EQ(root->ub(), n);
  EXPECT_EQ(root->size(), 0);
  EXPECT_EQ(root->depth(), 9);  // 2^9 = 512 which is the first value >= n.
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
