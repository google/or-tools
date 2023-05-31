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

#include "ortools/algorithms/knapsack_solver_for_cuts.h"

#include <limits>
#include <memory>
#include <vector>

#include "gtest/gtest.h"

namespace operations_research {
namespace {

const int kInvalidSolution = -1;
bool IsSolutionValid(const std::vector<double>& profits,
                     const std::vector<double>& weights, const double capacity,
                     const std::vector<bool>& best_solution,
                     double optimal_profit) {
  double remaining_capacity = capacity;
  double profit = 0;
  const int number_of_items(profits.size());
  for (int item_id(0); item_id < number_of_items; ++item_id) {
    if (best_solution.at(item_id)) {
      profit += profits[item_id];
      remaining_capacity -= weights[item_id];
    }
  }

  if (remaining_capacity < 0) {
    return false;
  }
  return profit == optimal_profit;
}

double SolveKnapsackProblem(KnapsackSolverForCuts* solver) {
  bool is_solution_optimal = false;
  auto time_limit =
      std::make_unique<TimeLimit>(std::numeric_limits<double>::infinity());
  return solver->Solve(time_limit.get(), &is_solution_optimal);
}

TEST(KnapsackSearchNodeForCutsTest, Depth) {
  KnapsackAssignmentForCuts assignment(0, false);
  KnapsackSearchNodeForCuts root(nullptr, assignment);
  EXPECT_EQ(0, root.depth());

  KnapsackSearchNodeForCuts node_0(&root, assignment);
  EXPECT_EQ(1, node_0.depth());

  KnapsackSearchNodeForCuts node_00(&node_0, assignment);
  EXPECT_EQ(2, node_00.depth());
}

TEST(KnapsackSearchPathTest, MoveUpToDepth) {
  KnapsackAssignmentForCuts assignment(0, false);
  KnapsackSearchNodeForCuts root(nullptr, assignment);
  KnapsackSearchNodeForCuts node_0(&root, assignment);
  KnapsackSearchPathForCuts from_root_to_0(&root, &node_0);
  const KnapsackSearchNodeForCuts* root_ptr = MoveUpToDepth(&node_0, 0);
  EXPECT_EQ(&root, root_ptr);
}

TEST(KnapsackSearchPathTest, InitAndMoveUpToDepth) {
  KnapsackAssignmentForCuts assignment(0, false);
  KnapsackSearchNodeForCuts root(nullptr, assignment);
  KnapsackSearchNodeForCuts node_0(&root, assignment);
  KnapsackSearchNodeForCuts node_00(&node_0, assignment);
  KnapsackSearchNodeForCuts node_01(&node_0, assignment);
  KnapsackSearchNodeForCuts node_001(&node_00, assignment);
  KnapsackSearchNodeForCuts node_010(&node_01, assignment);
  KnapsackSearchNodeForCuts node_0101(&node_010, assignment);
  KnapsackSearchNodeForCuts node_01011(&node_0101, assignment);

  KnapsackSearchPathForCuts from_01011_to_001(&node_01011, &node_001);
  const KnapsackSearchNodeForCuts* node_01_ptr = MoveUpToDepth(&node_01011, 2);
  EXPECT_EQ(&node_01, node_01_ptr);

  from_01011_to_001.Init();
  EXPECT_EQ(&node_0, &from_01011_to_001.via());

  KnapsackSearchPathForCuts from_001_to_01011(&node_001, &node_01011);
  from_001_to_01011.Init();
  EXPECT_EQ(&from_01011_to_001.via(), &from_001_to_01011.via());
}

TEST(KnapsackItemForCutsTest, GetEfficiency) {
  const int kId(7);
  const double kWeight = 52;
  const double kProfit = 130;
  const double kEfficiency = 2.5;
  const double kProfitMax = 1000;
  const double kNullWeight = 0;

  const KnapsackItemForCuts item(kId, kWeight, kProfit);
  EXPECT_EQ(kId, item.id);
  EXPECT_EQ(kWeight, item.weight);
  EXPECT_EQ(kProfit, item.profit);
  EXPECT_EQ(kEfficiency, item.GetEfficiency(kProfitMax));

  const KnapsackItemForCuts item2(kId, kNullWeight, kProfit);
  EXPECT_EQ(kProfitMax, item2.GetEfficiency(kProfitMax));
}

TEST(KnapsackStateForCutsTest, Init) {
  const int kNumberOfItems(12);
  KnapsackStateForCuts state;
  state.Init(kNumberOfItems);
  for (int i(0); i < kNumberOfItems; ++i) {
    EXPECT_FALSE(state.is_bound(i));
  }
  EXPECT_EQ(kNumberOfItems, state.GetNumberOfItems());
}

TEST(KnapsackStateForCutsTest, UpdateState) {
  const int kNumberOfItems(12);
  KnapsackStateForCuts state;
  state.Init(kNumberOfItems);

  const int item_id(7);
  bool is_in = true;
  KnapsackAssignmentForCuts assignment1(item_id, is_in);
  bool no_fail = state.UpdateState(false, assignment1);
  for (int i(0); i < kNumberOfItems; ++i) {
    EXPECT_EQ(i == item_id, state.is_bound(i));
  }
  EXPECT_EQ(is_in, state.is_in(item_id));
  EXPECT_TRUE(no_fail);

  is_in = false;
  KnapsackAssignmentForCuts assignment2(item_id, is_in);
  no_fail = state.UpdateState(false, assignment2);
  EXPECT_TRUE(state.is_bound(item_id));
  EXPECT_FALSE(no_fail);

  no_fail = state.UpdateState(true, assignment2);
  EXPECT_FALSE(state.is_bound(item_id));
  EXPECT_TRUE(no_fail);
}

TEST(KnapsackPropagatorForCutsTest, InitAndUpdatePropagator) {
  const std::vector<double> profits = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  const std::vector<double> weights = {1, 1, 1, 1, 1, 1, 1, 1, 1};
  ASSERT_EQ(profits.size(), weights.size());
  const int kNumItems(profits.size());
  const int kNoSelection(-1);

  KnapsackStateForCuts state;
  state.Init(kNumItems);

  KnapsackPropagatorForCuts capacity_propagator(&state);
  capacity_propagator.Init(profits, weights, 2);
  EXPECT_EQ(kNoSelection, capacity_propagator.GetNextItemId());

  KnapsackAssignmentForCuts assignment1(3, true);
  EXPECT_TRUE(state.UpdateState(false, assignment1));
  EXPECT_TRUE(capacity_propagator.Update(false, assignment1));
  EXPECT_EQ(4, capacity_propagator.current_profit());
  capacity_propagator.ComputeProfitBounds();
  EXPECT_EQ(7, capacity_propagator.GetNextItemId());
  const double kProfit13 = profits[3] + profits[8];
  EXPECT_EQ(kProfit13, capacity_propagator.profit_lower_bound());
  EXPECT_EQ(kProfit13, capacity_propagator.profit_upper_bound());

  KnapsackAssignmentForCuts assignment2(8, true);
  EXPECT_TRUE(state.UpdateState(false, assignment2));
  EXPECT_TRUE(capacity_propagator.Update(false, assignment2));
  EXPECT_EQ(kProfit13, capacity_propagator.current_profit());
  capacity_propagator.ComputeProfitBounds();
  EXPECT_EQ(7, capacity_propagator.GetNextItemId());
  EXPECT_EQ(kProfit13, capacity_propagator.profit_lower_bound());
  EXPECT_EQ(kProfit13, capacity_propagator.profit_upper_bound());

  KnapsackAssignmentForCuts assignment3(5, true);
  EXPECT_TRUE(state.UpdateState(false, assignment3));
  EXPECT_FALSE(capacity_propagator.Update(false, assignment3));
  const double kProfit19 = profits[3] + profits[8] + profits[5];
  EXPECT_EQ(kProfit19, capacity_propagator.current_profit());

  EXPECT_TRUE(state.UpdateState(true, assignment2));
  EXPECT_TRUE(capacity_propagator.Update(true, assignment2));
  const double kProfit10 = profits[3] + profits[5];
  EXPECT_EQ(kProfit10, capacity_propagator.current_profit());
  capacity_propagator.ComputeProfitBounds();
  EXPECT_EQ(8, capacity_propagator.GetNextItemId());
  EXPECT_EQ(kProfit10, capacity_propagator.profit_lower_bound());
  EXPECT_EQ(kProfit10, capacity_propagator.profit_upper_bound());
}

TEST(KnapsackSolverForCutsTest, SolveOneDimension) {
  const std::vector<double> profits = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  const std::vector<double> weights = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  ASSERT_EQ(profits.size(), weights.size());
  const double kCapacity = 34;
  const double kOptimalProfit = 34;
  KnapsackSolverForCuts solver("solver");
  solver.Init(profits, weights, kCapacity);
  const double profit = SolveKnapsackProblem(&solver);
  EXPECT_EQ(kOptimalProfit, profit);
}

TEST(KnapsackSolverForCutsTest, SolveOneDimensionInfeasible) {
  const std::vector<double> profits = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  const std::vector<double> weights = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  ASSERT_EQ(profits.size(), weights.size());
  const double kCapacity = -1;
  KnapsackSolverForCuts solver("solver");
  solver.Init(profits, weights, kCapacity);
  const double profit = SolveKnapsackProblem(&solver);
  const int number_of_items(profits.size());
  std::vector<bool> best_solution(number_of_items, false);
  for (int item_id(0); item_id < number_of_items; ++item_id) {
    best_solution.at(item_id) = solver.best_solution(item_id);
  }
  EXPECT_FALSE(
      IsSolutionValid(profits, weights, kCapacity, best_solution, profit));
}

TEST(KnapsackSolverForCutsTest, MultipleSolves) {
  KnapsackSolverForCuts solver("solver");
  {
    const std::vector<double> profits = {1, 2, 3};
    const std::vector<double> weights = {4, 5, 6};
    ASSERT_EQ(profits.size(), weights.size());
    const double kCapacity = 10;
    const double kOptimalProfit = 4;
    solver.Init(profits, weights, kCapacity);
    const double profit = SolveKnapsackProblem(&solver);
    EXPECT_EQ(kOptimalProfit, profit);
  }
  {
    const std::vector<double> profits = {1, 2, 3, 7};
    const std::vector<double> weights = {4, 5, 6, 8};
    ASSERT_EQ(profits.size(), weights.size());
    const double kCapacity = 10;
    const double kOptimalProfit = 7;
    solver.Init(profits, weights, kCapacity);
    const double profit = SolveKnapsackProblem(&solver);
    EXPECT_EQ(kOptimalProfit, profit);
  }
  {
    const std::vector<double> profits = {1, 2};
    const std::vector<double> weights = {4, 5};
    ASSERT_EQ(profits.size(), weights.size());
    const double kCapacity = 10;
    const double kOptimalProfit = 3;
    solver.Init(profits, weights, kCapacity);
    const double profit = SolveKnapsackProblem(&solver);
    EXPECT_EQ(kOptimalProfit, profit);
  }
}

TEST(KnapsackSolverForCutsTest, SolveBigOneDimension) {
  const std::vector<double> profits = {
      360, 83, 59,  130, 431, 67, 230, 52,  93,  125, 670, 892, 600,
      38,  48, 147, 78,  256, 63, 17,  120, 164, 432, 35,  92,  110,
      22,  42, 50,  323, 514, 28, 87,  73,  78,  15,  26,  78,  210,
      36,  85, 189, 274, 43,  33, 10,  19,  389, 276, 312};
  const std::vector<double> weights = {
      7,  0,  30, 22, 80, 94, 11, 81, 70, 64, 59, 18, 0,  36, 3,  8,  15,
      42, 9,  0,  42, 47, 52, 32, 26, 48, 55, 6,  29, 84, 2,  4,  18, 56,
      7,  29, 93, 44, 71, 3,  86, 66, 31, 65, 0,  79, 20, 65, 52, 13};
  ASSERT_EQ(profits.size(), weights.size());
  const double kCapacity = 850;
  const double kOptimalProfit = 7534;
  KnapsackSolverForCuts solver("solver");
  {
    solver.Init(profits, weights, kCapacity);
    const double profit = SolveKnapsackProblem(&solver);
    EXPECT_EQ(kOptimalProfit, profit);
  }
  {
    // Solve with lower bound threshold.
    solver.Init(profits, weights, kCapacity);
    solver.set_solution_lower_bound_threshold(100);
    const double profit = SolveKnapsackProblem(&solver);
    EXPECT_GT(kOptimalProfit, profit);
  }
  {
    // Solve with upper bound threshold.
    solver.Init(profits, weights, kCapacity);
    solver.set_solution_upper_bound_threshold(10000);
    const double profit = SolveKnapsackProblem(&solver);
    EXPECT_GT(kOptimalProfit, profit);
  }
  {
    solver.Init(profits, weights, kCapacity);
    solver.set_node_limit(1);
    const double profit = SolveKnapsackProblem(&solver);
    EXPECT_GT(kOptimalProfit, profit);
  }
}

TEST(KnapsackSolverForCutsTest, SolveOneDimensionFractionalProfits) {
  const std::vector<double> profits = {0, 0.5, 0.4, 1, 1, 1.1};
  const std::vector<double> weights = {9, 6, 2, 1, 1, 1};
  ASSERT_EQ(profits.size(), weights.size());
  const double kCapacity = 4;
  const double kOptimalProfit = 3.1;
  KnapsackSolverForCuts solver("solver");
  solver.Init(profits, weights, kCapacity);
  const double profit = SolveKnapsackProblem(&solver);
  EXPECT_EQ(kOptimalProfit, profit);
}

TEST(KnapsackSolverForCutsTest, SolveOneDimensionFractionalWeights) {
  const std::vector<double> profits = {0, 1, 1, 1, 1, 2};
  const std::vector<double> weights = {9, 6, 2, 1.5, 1.5, 1.5};
  ASSERT_EQ(profits.size(), weights.size());
  const double kCapacity = 4;
  const double kOptimalProfit = 3;
  KnapsackSolverForCuts solver("solver");
  solver.Init(profits, weights, kCapacity);
  const double profit = SolveKnapsackProblem(&solver);
  EXPECT_EQ(kOptimalProfit, profit);
}

TEST(KnapsackSolverForCutsTest, SolveOneDimensionFractional) {
  const std::vector<double> profits = {0, 0.5, 0.4, 1, 1, 1.1};
  const std::vector<double> weights = {9, 6, 2, 1.5, 1.5, 1.5};
  ASSERT_EQ(profits.size(), weights.size());
  const double kCapacity = 4;
  const double kOptimalProfit = 2.1;
  KnapsackSolverForCuts solver("solver");
  solver.Init(profits, weights, kCapacity);
  const double profit = SolveKnapsackProblem(&solver);
  EXPECT_EQ(kOptimalProfit, profit);
}

}  // namespace
}  // namespace operations_research
