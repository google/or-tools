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

#include "ortools/sat/routing_cuts.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#include "ortools/graph/max_flow.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {
namespace {

using ::testing::ElementsAre;

// Test on a simple tree:
//      3
//     / \ \
//    1   0 5
//   / \
//  2   4
TEST(ExtractAllSubsetsFromForestTest, Basic) {
  std::vector<int> parents = {3, 3, 1, 3, 1, 3};

  std::vector<int> buffer;
  std::vector<absl::Span<const int>> subsets;
  ExtractAllSubsetsFromForest(parents, &buffer, &subsets);

  // Post order but we explore high number first.
  // Alternatively, we could use unordered here, but the order is stable.
  EXPECT_THAT(buffer, ElementsAre(5, 4, 2, 1, 0, 3));
  EXPECT_THAT(subsets,
              ElementsAre(ElementsAre(5), ElementsAre(4), ElementsAre(2),
                          ElementsAre(4, 2, 1), ElementsAre(0),
                          ElementsAre(5, 4, 2, 1, 0, 3)));
}

//
//   0     3   4
//  / \        |
// 1   2       5
TEST(ExtractAllSubsetsFromForestTest, BasicForest) {
  std::vector<int> parents = {0, 0, 0, 3, 4, 4};

  std::vector<int> buffer;
  std::vector<absl::Span<const int>> subsets;
  ExtractAllSubsetsFromForest(parents, &buffer, &subsets);

  // Post order but we explore high number first.
  // Alternatively, we could use unordered here, but the order is stable.
  EXPECT_THAT(buffer, ElementsAre(2, 1, 0, 3, 5, 4));
  EXPECT_THAT(subsets,
              ElementsAre(ElementsAre(2), ElementsAre(1), ElementsAre(2, 1, 0),
                          ElementsAre(3), ElementsAre(5), ElementsAre(5, 4)));
}

TEST(ExtractAllSubsetsFromForestTest, Random) {
  const int num_nodes = 20;
  absl::BitGen random;

  // Create a random tree rooted at zero.
  std::vector<int> parents(num_nodes, 0);
  for (int i = 2; i < num_nodes; ++i) {
    parents[i] = absl::Uniform<int>(random, 0, i);  // in [0, i - 1].
  }

  std::vector<int> buffer;
  std::vector<absl::Span<const int>> subsets;
  ExtractAllSubsetsFromForest(parents, &buffer, &subsets);

  // We don't test that we are exhaustive, but we check basic property.
  std::vector<int> in_subset(num_nodes, false);
  for (const auto subset : subsets) {
    for (const int n : subset) in_subset[n] = true;

    // There should be at most one out edge.
    int root = -1;
    for (const int n : subset) {
      if (in_subset[parents[n]]) continue;
      if (root != -1) EXPECT_EQ(parents[n], root);
      root = parents[n];
    }

    // No node outside should point inside.
    for (int n = 0; n < num_nodes; ++n) {
      if (in_subset[n]) continue;
      EXPECT_TRUE(!in_subset[parents[n]]);
    }

    for (const int n : subset) in_subset[n] = false;
  }
}

TEST(SymmetrizeArcsTest, BasicTest) {
  std::vector<ArcWithLpValue> arcs{{.tail = 0, .head = 1, .lp_value = 0.5},
                                   {.tail = 2, .head = 0, .lp_value = 0.5},
                                   {.tail = 1, .head = 0, .lp_value = 0.5}};
  SymmetrizeArcs(&arcs);
  EXPECT_THAT(
      arcs, ElementsAre(ArcWithLpValue{.tail = 0, .head = 1, .lp_value = 1.0},
                        ArcWithLpValue{.tail = 0, .head = 2, .lp_value = 0.5}));
}

TEST(ComputeGomoryHuTreeTest, Random) {
  absl::BitGen random;

  // Lets generate a random graph on a small number of nodes.
  const int num_nodes = 10;
  const int num_arcs = 100;
  std::vector<ArcWithLpValue> arcs;
  for (int i = 0; i < num_arcs; ++i) {
    const int tail = absl::Uniform<int>(random, 0, num_nodes);
    const int head = absl::Uniform<int>(random, 0, num_nodes);
    if (tail == head) continue;
    const double lp_value = absl::Uniform<double>(random, 0, 1);
    arcs.push_back({tail, head, lp_value});
  }

  // Get all cut from Gomory-Hu tree.
  const std::vector<int> parents = ComputeGomoryHuTree(num_nodes, arcs);
  std::vector<int> buffer;
  std::vector<absl::Span<const int>> subsets;
  ExtractAllSubsetsFromForest(parents, &buffer, &subsets);

  // Compute the cost of entering (resp. leaving) each subset.
  // TODO(user): We need the same scaling as in ComputeGomoryHu(), not super
  // clean. We might want an integer input to the function, but ok for now.
  std::vector<bool> in_subset(num_nodes, false);
  std::vector<int64_t> out_costs(subsets.size(), 0);
  std::vector<int64_t> in_costs(subsets.size(), 0);
  for (int i = 0; i < subsets.size(); ++i) {
    for (const int n : subsets[i]) in_subset[n] = true;
    for (const auto& arc : arcs) {
      if (in_subset[arc.tail] && !in_subset[arc.head]) {
        out_costs[i] += std::round(1.0e6 * arc.lp_value);
      }
      if (!in_subset[arc.tail] && in_subset[arc.head]) {
        in_costs[i] += std::round(1.0e6 * arc.lp_value);
      }
    }
    for (const int n : subsets[i]) in_subset[n] = false;
  }

  // We will test with an exhaustive comparison. We are in n ^ 3 !
  // For all (s,t) pair, get the actual max-flow on the scaled graph.
  // Check than one of the cuts separate s and t, with this exact weight.
  SimpleMaxFlow max_flow;
  for (const auto& [tail, head, lp_value] : arcs) {
    // TODO(user): the algo only seems to work on an undirected graph, or
    // equivalently when we always have a reverse arc with the same weight.
    // Note that you can see below that we compute "min" cut for the sum of
    // outgoing + incoming arcs this way.
    max_flow.AddArcWithCapacity(tail, head, std::round(1.0e6 * lp_value));
    max_flow.AddArcWithCapacity(head, tail, std::round(1.0e6 * lp_value));
  }
  for (int s = 0; s < num_nodes; ++s) {
    for (int t = s + 1; t < num_nodes; ++t) {
      ASSERT_EQ(max_flow.Solve(s, t), SimpleMaxFlow::OPTIMAL);
      const int64_t flow = max_flow.OptimalFlow();
      bool found = false;
      for (int i = 0; i < subsets.size(); ++i) {
        bool s_out = true;
        bool t_out = true;
        for (const int n : subsets[i]) {
          if (n == s) s_out = false;
          if (n == t) t_out = false;
        }
        if (!s_out && t_out && out_costs[i] + in_costs[i] == flow) found = true;
        if (s_out && !t_out && in_costs[i] + out_costs[i] == flow) found = true;
        if (found) break;
      }

      // Debug.
      if (!found) {
        LOG(INFO) << s << " -> " << t << " flow= " << flow;
        for (int i = 0; i < subsets.size(); ++i) {
          bool s_out = true;
          bool t_out = true;
          for (const int n : subsets[i]) {
            if (n == s) s_out = false;
            if (n == t) t_out = false;
          }
          if (!s_out && t_out) {
            LOG(INFO) << i << " out= " << out_costs[i] + in_costs[i];
          }
          if (s_out && !t_out) {
            LOG(INFO) << i << " in= " << in_costs[i] + out_costs[i];
          }
        }
      }
      ASSERT_TRUE(found);
    }
  }
}

TEST(CreateStronglyConnectedGraphCutGeneratorTest, BasicExample) {
  Model model;

  // Lets create a simple square graph with arcs in both directions:
  //
  // 0 ---- 1
  // |      |
  // |      |
  // 2 ---- 3
  const int num_nodes = 4;
  std::vector<int> tails{0, 1, 1, 3, 3, 2, 2, 0};
  std::vector<int> heads{1, 0, 3, 1, 2, 3, 0, 2};
  std::vector<Literal> literals;
  std::vector<IntegerVariable> vars;
  for (int i = 0; i < 2 * num_nodes; ++i) {
    literals.push_back(Literal(model.Add(NewBooleanVariable()), true));
    vars.push_back(model.Add(NewIntegerVariableFromLiteral(literals.back())));
  }

  CutGenerator generator = CreateStronglyConnectedGraphCutGenerator(
      num_nodes, tails, heads, literals, &model);

  // Suppose only 0-1 and 2-3 are in the lp solution (values do not matter).
  auto& lp_values = *model.GetOrCreate<ModelLpValues>();
  lp_values.resize(16, 0.0);
  lp_values[vars[0]] = 0.5;
  lp_values[vars[1]] = 0.5;
  lp_values[vars[4]] = 1.0;
  lp_values[vars[5]] = 1.0;
  LinearConstraintManager manager(&model);
  generator.generate_cuts(&manager);

  // We should get two cuts.
  EXPECT_EQ(manager.num_cuts(), 2);
  EXPECT_THAT(manager.AllConstraints().front().constraint.VarsAsSpan(),
              ElementsAre(vars[3], vars[6]));
  EXPECT_THAT(manager.AllConstraints().back().constraint.VarsAsSpan(),
              ElementsAre(vars[2], vars[7]));
}

TEST(CreateStronglyConnectedGraphCutGeneratorTest, AnotherExample) {
  // This time, the graph is fully connected, but we still detect that {1, 2, 3}
  // do not have enough outgoing flow:
  //
  //           0.5
  //        0 <--> 1
  //        ^      |               0.5
  //   0.5  |      |  1     and  2 ----> 1
  //        v      v
  //        2 <--- 3
  //            1
  const int num_nodes = 4;
  std::vector<int> tails{0, 1, 0, 2, 1, 3, 2};
  std::vector<int> heads{1, 0, 2, 0, 3, 2, 1};
  std::vector<double> values{0.5, 0.0, 0.5, 0.0, 1.0, 1.0, 0.5};

  Model model;
  std::vector<Literal> literals;
  auto& lp_values = *model.GetOrCreate<ModelLpValues>();
  lp_values.resize(16, 0.0);
  for (int i = 0; i < values.size(); ++i) {
    literals.push_back(Literal(model.Add(NewBooleanVariable()), true));
    lp_values[model.Add(NewIntegerVariableFromLiteral(literals.back()))] =
        values[i];
  }

  CutGenerator generator = CreateStronglyConnectedGraphCutGenerator(
      num_nodes, tails, heads, literals, &model);

  LinearConstraintManager manager(&model);
  generator.generate_cuts(&manager);

  // The sets {2, 3} and {1, 2, 3} will generate cuts.
  // However as an heuristic, we will wait another round to generate {1, 2 ,3}.
  EXPECT_EQ(manager.num_cuts(), 1);
  EXPECT_THAT(manager.AllConstraints().back().constraint.DebugString(),
              ::testing::StartsWith("1 <= 1*X3 1*X6"));
}

TEST(GenerateInterestingSubsetsTest, BasicExample) {
  const int num_nodes = 6;
  const std::vector<std::pair<int, int>> arcs = {{0, 5}, {2, 3}, {3, 4}};

  // Note that the order is not important, but is currently fixed.
  // This document the actual order.
  std::vector<int> subset_data;
  std::vector<absl::Span<const int>> subsets;
  GenerateInterestingSubsets(num_nodes, arcs,
                             /*stop_at_num_components=*/2, &subset_data,
                             &subsets);
  EXPECT_THAT(
      subsets,
      ElementsAre(ElementsAre(1), ElementsAre(5), ElementsAre(0),
                  ElementsAre(5, 0), ElementsAre(3), ElementsAre(2),
                  ElementsAre(3, 2), ElementsAre(4), ElementsAre(3, 2, 4)));

  // We can call it more than once.
  GenerateInterestingSubsets(num_nodes, arcs,
                             /*stop_at_num_components=*/2, &subset_data,
                             &subsets);
  EXPECT_THAT(
      subsets,
      ElementsAre(ElementsAre(1), ElementsAre(5), ElementsAre(0),
                  ElementsAre(5, 0), ElementsAre(3), ElementsAre(2),
                  ElementsAre(3, 2), ElementsAre(4), ElementsAre(3, 2, 4)));
}

TEST(CreateFlowCutGeneratorTest, BasicExample) {
  //
  //            /---> 2
  //    0 ---> 1      ^
  //            \---> 3
  //
  // With a flow of 2 leaving 0 and a flow of 1 requested at 2 and 3.
  // On each arc the flow <= max_flow * arc_indicator where max_flow = 2.
  const int num_nodes = 4;
  std::vector<int> tails{0, 1, 1, 3};
  std::vector<int> heads{1, 2, 3, 2};
  std::vector<double> values{1.0, 0.5, 0.5, 0.0};

  Model model;
  std::vector<AffineExpression> capacities;
  auto& lp_values = *model.GetOrCreate<ModelLpValues>();
  lp_values.resize(16, 0.0);
  for (int i = 0; i < values.size(); ++i) {
    AffineExpression expr;
    expr.var = model.Add(NewIntegerVariable(0, 1));
    expr.coeff = 2;
    expr.constant = 0;
    capacities.emplace_back(expr);
    lp_values[capacities.back().var] = values[i];
  }

  const auto get_flows = [](const std::vector<bool>& in_subset,
                            IntegerValue* min_incoming_flow,
                            IntegerValue* min_outgoing_flow) {
    IntegerValue demand(0);
    if (in_subset[0]) demand -= 2;
    if (in_subset[2]) demand += 1;
    if (in_subset[3]) demand += 1;
    *min_incoming_flow = std::max(IntegerValue(0), demand);
    *min_outgoing_flow = std::max(IntegerValue(0), -demand);
  };
  const CutGenerator generator = CreateFlowCutGenerator(
      num_nodes, tails, heads, capacities, get_flows, &model);

  LinearConstraintManager manager(&model);
  generator.generate_cuts(&manager);

  // The sets {2} and {3} will generate incoming flow cuts.
  EXPECT_EQ(manager.num_cuts(), 2);
  EXPECT_THAT(manager.AllConstraints().front().constraint.DebugString(),
              ::testing::StartsWith("1 <= 1*X2"));
  EXPECT_THAT(manager.AllConstraints().back().constraint.DebugString(),
              ::testing::StartsWith("1 <= 1*X1 1*X3"));
}

TEST(CreateFlowCutGeneratorTest, WithMinusOneArcs) {
  //    0 ---> 1 -->
  //           |
  //           \ -->
  const int num_nodes = 2;
  std::vector<int> tails{0, 1, 1};
  std::vector<int> heads{1, -1, -1};
  std::vector<double> values{1.0, 0.5, 0.0};

  Model model;
  std::vector<AffineExpression> capacities;
  auto& lp_values = *model.GetOrCreate<ModelLpValues>();
  lp_values.resize(16, 0.0);
  for (int i = 0; i < values.size(); ++i) {
    AffineExpression expr;
    expr.var = model.Add(NewIntegerVariable(0, 1));
    expr.coeff = 2;
    expr.constant = 0;
    capacities.emplace_back(expr);
    lp_values[capacities.back().var] = values[i];
  }

  const auto get_flows = [](const std::vector<bool>& in_subset,
                            IntegerValue* min_incoming_flow,
                            IntegerValue* min_outgoing_flow) {
    IntegerValue demand(0);
    if (in_subset[0]) demand -= 2;
    *min_incoming_flow = std::max(IntegerValue(0), demand);
    *min_outgoing_flow = std::max(IntegerValue(0), -demand);
  };
  const CutGenerator generator = CreateFlowCutGenerator(
      num_nodes, tails, heads, capacities, get_flows, &model);

  LinearConstraintManager manager(&model);
  generator.generate_cuts(&manager);

  // We artificially put bad LP values so that {1} generate outgoing flow cut.
  EXPECT_EQ(manager.num_cuts(), 1);
  EXPECT_THAT(manager.AllConstraints().front().constraint.DebugString(),
              ::testing::StartsWith("1 <= 1*X1 1*X2"));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
