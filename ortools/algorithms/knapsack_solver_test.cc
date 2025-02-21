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

#include "ortools/algorithms/knapsack_solver.h"

#include <cstdint>
#include <limits>
#include <vector>

#include "absl/base/macros.h"
#include "gtest/gtest.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace {

const int kInvalidSolution = -1;

bool IsSolutionValid(const std::vector<int64_t>& profits,
                     const std::vector<std::vector<int64_t> >& weights,
                     const std::vector<int64_t>& capacities,
                     const std::vector<bool>& best_solution,
                     int64_t optimal_profit) {
  std::vector<int64_t> remaining_capacities = capacities;
  int64_t profit = 0;
  for (int i = 0; i < profits.size(); ++i) {
    if (best_solution.at(i)) {
      profit += profits[i];
      for (int j = 0; j < capacities.size(); ++j) {
        remaining_capacities[j] -= (weights[j])[i];
      }
    }
  }

  for (int j = 0; j < capacities.size(); ++j) {
    if (remaining_capacities[j] < 0) {
      return false;
    }
  }
  return profit == optimal_profit;
}

int64_t SolveKnapsackProblemUsingSpecificSolverAndReduction(
    const int64_t* profit_array, int number_of_items,
    const int64_t* weight_array, const int64_t* capacity_array,
    int number_of_dimensions, KnapsackSolver::SolverType type,
    bool use_reduction,
    double time_limit = std::numeric_limits<double>::infinity()) {
  std::vector<int64_t> profits(profit_array, profit_array + number_of_items);
  std::vector<int64_t> capacities(capacity_array,
                                  capacity_array + number_of_dimensions);
  std::vector<std::vector<int64_t> > weights;
  for (int i = 0; i < number_of_dimensions; ++i) {
    const int64_t* one_dimension = weight_array + number_of_items * i;
    std::vector<int64_t> weights_one_dimension(one_dimension,
                                               one_dimension + number_of_items);
    weights.push_back(weights_one_dimension);
  }

  KnapsackSolver solver(type, "solver");
  solver.set_use_reduction(use_reduction);
  solver.set_time_limit(time_limit);
  solver.Init(profits, weights, capacities);
  int64_t profit = solver.Solve();

  std::vector<bool> best_solution(number_of_items, false);
  for (int item_id = 0; item_id < number_of_items; ++item_id) {
    best_solution.at(item_id) = solver.BestSolutionContains(item_id);
  }

  return (IsSolutionValid(profits, weights, capacities, best_solution, profit))
             ? profit
             : kInvalidSolution;
}

int64_t SolveKnapsackProblemUsingSpecificSolver(
    const int64_t* profit_array, int number_of_items,
    const int64_t* weight_array, const int64_t* capacity_array,
    int number_of_dimensions, KnapsackSolver::SolverType type,
    double time_limit = std::numeric_limits<double>::infinity()) {
  const int64_t result_when_reduction =
      SolveKnapsackProblemUsingSpecificSolverAndReduction(
          profit_array, number_of_items, weight_array, capacity_array,
          number_of_dimensions, type, true, time_limit);
  const int64_t result_when_no_reduction =
      SolveKnapsackProblemUsingSpecificSolverAndReduction(
          profit_array, number_of_items, weight_array, capacity_array,
          number_of_dimensions, type, false, time_limit);
  return (result_when_reduction == result_when_no_reduction)
             ? result_when_reduction
             : kInvalidSolution;
}

int64_t SolveKnapsackProblem(
    const int64_t* profit_array, int number_of_items,
    const int64_t* weight_array, const int64_t* capacity_array,
    int number_of_dimensions,
    double time_limit = std::numeric_limits<double>::infinity()) {
  const int kMaxNumberOfItemsForBruteForceSolver = 15;
  const int kMaxNumberOfItemsForDivideAndConquerSolver = 32;
  const int kMaxNumberOfItemsFor64ItemsSolver = 64;
  // This test is ran as "size = 'small'". To be fast enough, the dynamic
  // programming solver should be limited to instances with capacities smaller
  // than 10^6.
  const int64_t kMaxCapacityForDynamicProgrammingSolver = 1000000;
  const int64_t generic_profit = SolveKnapsackProblemUsingSpecificSolver(
      profit_array, number_of_items, weight_array, capacity_array,
      number_of_dimensions,
      KnapsackSolver::KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER,
      time_limit);

  if (generic_profit == kInvalidSolution) {
    return kInvalidSolution;
  }

#if defined(USE_SCIP)
  const int64_t scip_profit = SolveKnapsackProblemUsingSpecificSolver(
      profit_array, number_of_items, weight_array, capacity_array,
      number_of_dimensions,
      KnapsackSolver::KNAPSACK_MULTIDIMENSION_SCIP_MIP_SOLVER);
  if (scip_profit != generic_profit) {
    return kInvalidSolution;
  }
#else  // !defined(USE_SCIP)
#warning SCIP support disable
#endif  // !defined(USE_SCIP)

  const int64_t cpsat_profit = SolveKnapsackProblemUsingSpecificSolver(
      profit_array, number_of_items, weight_array, capacity_array,
      number_of_dimensions,
      KnapsackSolver::KNAPSACK_MULTIDIMENSION_CP_SAT_SOLVER);
  if (cpsat_profit != generic_profit) {
    return kInvalidSolution;
  }

  if (number_of_dimensions > 1) {
    return generic_profit;
  }

  if (number_of_items <= kMaxNumberOfItemsForBruteForceSolver) {
    const int64_t brute_force_profit = SolveKnapsackProblemUsingSpecificSolver(
        profit_array, number_of_items, weight_array, capacity_array,
        number_of_dimensions, KnapsackSolver::KNAPSACK_BRUTE_FORCE_SOLVER);
    if (brute_force_profit != generic_profit) {
      return kInvalidSolution;
    }
  }

  if (number_of_items <= kMaxNumberOfItemsFor64ItemsSolver) {
    const int64_t items64_profit = SolveKnapsackProblemUsingSpecificSolver(
        profit_array, number_of_items, weight_array, capacity_array,
        number_of_dimensions, KnapsackSolver::KNAPSACK_64ITEMS_SOLVER);
    if (items64_profit != generic_profit) {
      return kInvalidSolution;
    }
  }

  if (capacity_array[0] <= kMaxCapacityForDynamicProgrammingSolver) {
    const int64_t dynamic_programming_profit =
        SolveKnapsackProblemUsingSpecificSolver(
            profit_array, number_of_items, weight_array, capacity_array,
            number_of_dimensions,
            KnapsackSolver::KNAPSACK_DYNAMIC_PROGRAMMING_SOLVER);
    if (dynamic_programming_profit != generic_profit) {
      return kInvalidSolution;
    }
  }

  if (number_of_items <= kMaxNumberOfItemsForDivideAndConquerSolver) {
    const int64_t divide_and_conquer_profit =
        SolveKnapsackProblemUsingSpecificSolver(
            profit_array, number_of_items, weight_array, capacity_array,
            number_of_dimensions,
            KnapsackSolver::KNAPSACK_DIVIDE_AND_CONQUER_SOLVER);
    if (divide_and_conquer_profit != generic_profit) {
      return kInvalidSolution;
    }
  }

  return generic_profit;
}

TEST(KnapsackItemTest, GetEfficiency) {
  const int kId = 7;
  const int64_t kWeight = 52;
  const int64_t kProfit = 130;
  const double kEfficiency = 2.5;
  const int64_t kProfitMax = 1000;
  const int kNullWeight = 0;

  const KnapsackItem item(kId, kWeight, kProfit);
  EXPECT_EQ(kId, item.id);
  EXPECT_EQ(kWeight, item.weight);
  EXPECT_EQ(kProfit, item.profit);
  EXPECT_EQ(kEfficiency, item.GetEfficiency(kProfitMax));

  const KnapsackItem item2(kId, kNullWeight, kProfit);
  EXPECT_EQ(kProfitMax, item2.GetEfficiency(kProfitMax));
}

TEST(KnapsackSearchNodeTest, Depth) {
  KnapsackAssignment assignment(0, false);
  KnapsackSearchNode root(nullptr, assignment);
  EXPECT_EQ(0, root.depth());

  KnapsackSearchNode node_0(&root, assignment);
  EXPECT_EQ(1, node_0.depth());

  KnapsackSearchNode node_00(&node_0, assignment);
  EXPECT_EQ(2, node_00.depth());
}

TEST(KnapsackSearchPathTest, MoveUpToDepth) {
  KnapsackAssignment assignment(0, false);
  KnapsackSearchNode root(nullptr, assignment);
  KnapsackSearchNode node_0(&root, assignment);
  KnapsackSearchPath from_root_to_0(root, node_0);
  const KnapsackSearchNode* root_ptr = from_root_to_0.MoveUpToDepth(node_0, 0);
  EXPECT_EQ(&root, root_ptr);
}

TEST(KnapsackSearchPathTest, InitAndMoveUpToDepth) {
  KnapsackAssignment assignment(0, false);
  KnapsackSearchNode root(nullptr, assignment);
  KnapsackSearchNode node_0(&root, assignment);
  KnapsackSearchNode node_00(&node_0, assignment);
  KnapsackSearchNode node_01(&node_0, assignment);
  KnapsackSearchNode node_001(&node_00, assignment);
  KnapsackSearchNode node_010(&node_01, assignment);
  KnapsackSearchNode node_0101(&node_010, assignment);
  KnapsackSearchNode node_01011(&node_0101, assignment);

  KnapsackSearchPath from_01011_to_001(node_01011, node_001);
  const KnapsackSearchNode* node_01_ptr =
      from_01011_to_001.MoveUpToDepth(node_01011, 2);
  EXPECT_EQ(&node_01, node_01_ptr);

  from_01011_to_001.Init();
  EXPECT_EQ(&node_0, &from_01011_to_001.via());

  KnapsackSearchPath from_001_to_01011(node_001, node_01011);
  from_001_to_01011.Init();
  EXPECT_EQ(&from_01011_to_001.via(), &from_001_to_01011.via());
}

TEST(KnapsackStateTest, Init) {
  const int kNumberOfItems = 12;
  KnapsackState state;
  state.Init(kNumberOfItems);
  for (int i = 0; i < kNumberOfItems; ++i) {
    EXPECT_FALSE(state.is_bound(i));
  }
  EXPECT_EQ(kNumberOfItems, state.GetNumberOfItems());
}

TEST(KnapsackStateTest, UpdateState) {
  const int kNumberOfItems = 12;
  KnapsackState state;
  state.Init(kNumberOfItems);

  const int item_id = 7;
  bool is_in = true;
  KnapsackAssignment assignment1(item_id, is_in);
  bool no_fail = state.UpdateState(false, assignment1);
  for (int i = 0; i < kNumberOfItems; ++i) {
    EXPECT_EQ(i == item_id, state.is_bound(i));
  }
  EXPECT_EQ(is_in, state.is_in(item_id));
  EXPECT_TRUE(no_fail);

  is_in = false;
  KnapsackAssignment assignment2(item_id, is_in);
  no_fail = state.UpdateState(false, assignment2);
  EXPECT_TRUE(state.is_bound(item_id));
  EXPECT_FALSE(no_fail);

  no_fail = state.UpdateState(true, assignment2);
  EXPECT_FALSE(state.is_bound(item_id));
  EXPECT_TRUE(no_fail);
}

class KnapsackFakePropagator : public KnapsackPropagator {
 public:
  explicit KnapsackFakePropagator(const KnapsackState& state)
      : KnapsackPropagator(state) {}
  int GetNextItemId() const override { return 0; }
  void ComputeProfitBounds() override {
    set_profit_upper_bound(profit_lower_bound());
  }

 protected:
  void InitPropagator() override { set_profit_lower_bound(items().size()); }
  bool UpdatePropagator(bool revert,
                        const KnapsackAssignment& /*assignment*/) override {
    set_profit_lower_bound(profit_lower_bound() + ((revert) ? -4 : 4));
    return profit_lower_bound() > 0;
  }
  void CopyCurrentStateToSolutionPropagator(
      std::vector<bool>* /*solution*/) const override {}
};

TEST(KnapsackPropagatorTest, InitAndUpdatePropagator) {
  const int64_t kProfitArray[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  const int64_t kWeightArray[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  const int kArraySize = ABSL_ARRAYSIZE(kProfitArray);

  KnapsackState state;
  state.Init(kArraySize);

  std::vector<int64_t> profits(kProfitArray, kProfitArray + kArraySize);
  std::vector<int64_t> weights(kWeightArray, kWeightArray + kArraySize);

  KnapsackFakePropagator fake_propagator(state);
  fake_propagator.Init(profits, weights);
  EXPECT_EQ(kArraySize, fake_propagator.profit_lower_bound());
  EXPECT_EQ(0, fake_propagator.GetNextItemId());

  KnapsackAssignment assignment(3, true);
  bool no_fail = fake_propagator.Update(true, assignment);
  EXPECT_TRUE(no_fail);
  no_fail = fake_propagator.Update(true, assignment);
  EXPECT_TRUE(no_fail);
  no_fail = fake_propagator.Update(true, assignment);
  EXPECT_FALSE(no_fail);
  no_fail = fake_propagator.Update(false, assignment);
  EXPECT_TRUE(no_fail);
}

TEST(KnapsackCapacityPropagatorTest, InitAndUpdatePropagator) {
  const int64_t kProfitArray[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  const int64_t kWeightArray[] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
  const int kArraySize = ABSL_ARRAYSIZE(kProfitArray);
  const int kNoSelection = -1;

  KnapsackState state;
  state.Init(kArraySize);

  std::vector<int64_t> profits(kProfitArray, kProfitArray + kArraySize);
  std::vector<int64_t> weights(kWeightArray, kWeightArray + kArraySize);

  KnapsackCapacityPropagator capacity_propagator(state, 2);
  capacity_propagator.Init(profits, weights);
  EXPECT_EQ(kNoSelection, capacity_propagator.GetNextItemId());

  KnapsackAssignment assignment1(3, true);
  bool no_fail = state.UpdateState(false, assignment1);
  no_fail = capacity_propagator.Update(false, assignment1) && no_fail;
  EXPECT_TRUE(no_fail);
  EXPECT_EQ(4, capacity_propagator.current_profit());
  capacity_propagator.ComputeProfitBounds();
  EXPECT_EQ(7, capacity_propagator.GetNextItemId());
  const int64_t kProfit13 = kProfitArray[3] + kProfitArray[8];
  EXPECT_EQ(kProfit13, capacity_propagator.profit_lower_bound());
  EXPECT_EQ(kProfit13, capacity_propagator.profit_upper_bound());

  KnapsackAssignment assignment2(8, true);
  no_fail = state.UpdateState(false, assignment2);
  no_fail = capacity_propagator.Update(false, assignment2) && no_fail;
  EXPECT_TRUE(no_fail);
  EXPECT_EQ(kProfit13, capacity_propagator.current_profit());
  capacity_propagator.ComputeProfitBounds();
  EXPECT_EQ(7, capacity_propagator.GetNextItemId());
  EXPECT_EQ(kProfit13, capacity_propagator.profit_lower_bound());
  EXPECT_EQ(kProfit13, capacity_propagator.profit_upper_bound());

  KnapsackAssignment assignment3(5, true);
  no_fail = state.UpdateState(false, assignment3);
  no_fail = capacity_propagator.Update(false, assignment3) && no_fail;
  EXPECT_FALSE(no_fail);
  const int64_t kProfit19 = kProfitArray[3] + kProfitArray[8] + kProfitArray[5];
  EXPECT_EQ(kProfit19, capacity_propagator.current_profit());

  no_fail = state.UpdateState(true, assignment2);
  no_fail = capacity_propagator.Update(true, assignment2) && no_fail;
  EXPECT_TRUE(no_fail);
  const int64_t kProfit10 = kProfitArray[3] + kProfitArray[5];
  EXPECT_EQ(kProfit10, capacity_propagator.current_profit());
  capacity_propagator.ComputeProfitBounds();
  EXPECT_EQ(8, capacity_propagator.GetNextItemId());
  EXPECT_EQ(kProfit10, capacity_propagator.profit_lower_bound());
  EXPECT_EQ(kProfit10, capacity_propagator.profit_upper_bound());
}

TEST(KnapsackSolverTest, SolveOneDimension) {
  const int64_t kProfitArray[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  const int64_t kWeightArray[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  const int64_t kCapacityArray[] = {34};
  const int kArraySize = ABSL_ARRAYSIZE(kProfitArray);
  const int kNumberOfDimensions = ABSL_ARRAYSIZE(kCapacityArray);
  const int kOptimalProfit = 34;
  const int64_t profit =
      SolveKnapsackProblem(kProfitArray, kArraySize, kWeightArray,
                           kCapacityArray, kNumberOfDimensions);
  EXPECT_EQ(kOptimalProfit, profit);
}

TEST(KnapsackSolverTest, SolveOneDimensionReducedToNone) {
  const int64_t kProfitArray[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  const int64_t kWeightArray[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  const int64_t kCapacityArray[] = {50};
  const int kArraySize = ABSL_ARRAYSIZE(kProfitArray);
  const int kNumberOfDimensions = ABSL_ARRAYSIZE(kCapacityArray);
  const int kOptimalProfit = 45;
  const int64_t profit =
      SolveKnapsackProblem(kProfitArray, kArraySize, kWeightArray,
                           kCapacityArray, kNumberOfDimensions);
  EXPECT_EQ(kOptimalProfit, profit);
}

TEST(KnapsackSolverTest, SolveOneDimensionWithZeroTimeLimit) {
  const int64_t kProfitArray[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  const int64_t kWeightArray[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  const int64_t kCapacityArray[] = {34};
  const int kArraySize = ABSL_ARRAYSIZE(kProfitArray);
  const int kNumberOfDimensions = ABSL_ARRAYSIZE(kCapacityArray);
  const int kNoProfit = -1;
  const int64_t profit =
      SolveKnapsackProblem(kProfitArray, kArraySize, kWeightArray,
                           kCapacityArray, kNumberOfDimensions, 0.0);
  EXPECT_EQ(kNoProfit, profit);
}

TEST(KnapsackSolverTest, SolveTwoDimensions) {
  const int64_t kProfitArray[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  const int64_t kWeightArray[] = {1, 2, 3, 4, 5, 6, 7, 8, 9,
                                  1, 1, 1, 1, 1, 1, 1, 1, 1};
  const int64_t kCapacityArray[] = {34, 4};
  const int kArraySize = ABSL_ARRAYSIZE(kProfitArray);
  const int kNumberOfDimensions = ABSL_ARRAYSIZE(kCapacityArray);
  const int kOptimalProfit = 30;
  const int64_t profit =
      SolveKnapsackProblem(kProfitArray, kArraySize, kWeightArray,
                           kCapacityArray, kNumberOfDimensions);
  EXPECT_EQ(kOptimalProfit, profit);
}

TEST(KnapsackSolverTest, SolveTwoDimensionsReducedToOne) {
  const int64_t kProfitArray[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  const int64_t kWeightArray[] = {1, 2, 3, 4, 5, 6, 7, 8, 9,
                                  1, 1, 1, 1, 1, 1, 1, 1, 1};
  const int64_t kCapacityArray[] = {50, 4};
  const int kArraySize = ABSL_ARRAYSIZE(kProfitArray);
  const int kNumberOfDimensions = ABSL_ARRAYSIZE(kCapacityArray);
  const int kOptimalProfit = 30;
  const int64_t profit =
      SolveKnapsackProblem(kProfitArray, kArraySize, kWeightArray,
                           kCapacityArray, kNumberOfDimensions);
  EXPECT_EQ(kOptimalProfit, profit);
}

TEST(KnapsackSolverTest, SolveTwoDimensionsReducedToNone) {
  const int64_t kProfitArray[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  const int64_t kWeightArray[] = {1, 2, 3, 4, 5, 6, 7, 8, 9,
                                  1, 1, 1, 1, 1, 1, 1, 1, 1};
  const int64_t kCapacityArray[] = {50, 10};
  const int kArraySize = ABSL_ARRAYSIZE(kProfitArray);
  const int kNumberOfDimensions = ABSL_ARRAYSIZE(kCapacityArray);
  const int kOptimalProfit = 45;
  const int64_t profit =
      SolveKnapsackProblem(kProfitArray, kArraySize, kWeightArray,
                           kCapacityArray, kNumberOfDimensions);
  EXPECT_EQ(kOptimalProfit, profit);
}

TEST(KnapsackSolverTest, SolveTwoDimensionsSettingPrimaryPropagator) {
  const int64_t kProfitArray[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  const int64_t kWeightArray[] = {1, 2, 3, 4, 5, 6, 7, 8, 9,
                                  1, 1, 1, 1, 1, 1, 1, 1, 1};
  const int64_t kCapacityArray[] = {34, 4};
  const int kArraySize = ABSL_ARRAYSIZE(kProfitArray);
  const int kNumberOfDimensions = ABSL_ARRAYSIZE(kCapacityArray);
  const int kOptimalProfit = 30;

  std::vector<int64_t> profits(kProfitArray, kProfitArray + kArraySize);
  std::vector<int64_t> capacities(kCapacityArray,
                                  kCapacityArray + kNumberOfDimensions);
  std::vector<std::vector<int64_t> > weights;
  for (int i = 0; i < kNumberOfDimensions; ++i) {
    const int64_t* one_dimension = kWeightArray + kArraySize * i;
    std::vector<int64_t> weights_one_dimension(one_dimension,
                                               one_dimension + kArraySize);
    weights.push_back(weights_one_dimension);
  }

  KnapsackGenericSolver solver("solver");
  solver.Init(profits, weights, capacities);
  solver.set_primary_propagator_id(1);
  bool is_solution_optimal = false;
  const double inf = std::numeric_limits<double>::infinity();
  operations_research::TimeLimit time_limit(inf);
  int64_t profit = solver.Solve(&time_limit, inf, &is_solution_optimal);
  EXPECT_TRUE(is_solution_optimal);

  std::vector<bool> best_solution(kArraySize, false);
  for (int item_id = 0; item_id < kArraySize; ++item_id) {
    best_solution.at(item_id) = solver.best_solution(item_id);
  }

  EXPECT_TRUE(
      IsSolutionValid(profits, weights, capacities, best_solution, profit));
  EXPECT_EQ(kOptimalProfit, profit);
}

TEST(KnapsackSolverTest, SolveBigOneDimension) {
  const int64_t kProfitArray[] = {
      360, 83, 59,  130, 431, 67, 230, 52,  93,  125, 670, 892, 600,
      38,  48, 147, 78,  256, 63, 17,  120, 164, 432, 35,  92,  110,
      22,  42, 50,  323, 514, 28, 87,  73,  78,  15,  26,  78,  210,
      36,  85, 189, 274, 43,  33, 10,  19,  389, 276, 312};
  const int64_t kWeightArray[] = {
      7,  0,  30, 22, 80, 94, 11, 81, 70, 64, 59, 18, 0,  36, 3,  8,  15,
      42, 9,  0,  42, 47, 52, 32, 26, 48, 55, 6,  29, 84, 2,  4,  18, 56,
      7,  29, 93, 44, 71, 3,  86, 66, 31, 65, 0,  79, 20, 65, 52, 13};
  const int64_t kCapacityArray[] = {850};
  const int kArraySize = ABSL_ARRAYSIZE(kProfitArray);
  const int kNumberOfDimensions = ABSL_ARRAYSIZE(kCapacityArray);
  const int kOptimalProfit = 7534;
  const int64_t profit =
      SolveKnapsackProblem(kProfitArray, kArraySize, kWeightArray,
                           kCapacityArray, kNumberOfDimensions);
  EXPECT_EQ(kOptimalProfit, profit);
}

TEST(KnapsackSolverTest, SolveBigFiveDimensions) {
  const int64_t kProfitArray[] = {
      360, 83, 59,  130, 431, 67, 230, 52,  93,  125, 670, 892, 600,
      38,  48, 147, 78,  256, 63, 17,  120, 164, 432, 35,  92,  110,
      22,  42, 50,  323, 514, 28, 87,  73,  78,  15,  26,  78,  210,
      36,  85, 189, 274, 43,  33, 10,  19,  389, 276, 312};
  const int64_t kWeightArray[] = {
      7,  0,  30, 22, 80, 94, 11, 81, 70, 64, 59, 18, 0,  36, 3,  8,  15, 42,
      9,  0,  42, 47, 52, 32, 26, 48, 55, 6,  29, 84, 2,  4,  18, 56, 7,  29,
      93, 44, 71, 3,  86, 66, 31, 65, 0,  79, 20, 65, 52, 13, 8,  66, 98, 50,
      0,  30, 0,  88, 15, 37, 26, 72, 61, 57, 17, 27, 83, 3,  9,  66, 97, 42,
      2,  44, 71, 11, 25, 74, 90, 20, 0,  38, 33, 14, 9,  23, 12, 58, 6,  14,
      78, 0,  12, 99, 84, 31, 16, 7,  33, 20, 3,  74, 88, 50, 55, 19, 0,  6,
      30, 62, 17, 81, 25, 46, 67, 28, 36, 8,  1,  52, 19, 37, 27, 62, 39, 84,
      16, 14, 21, 5,  60, 82, 72, 89, 16, 5,  29, 7,  80, 97, 41, 46, 15, 92,
      51, 76, 57, 90, 10, 37, 21, 40, 0,  6,  82, 91, 43, 30, 62, 91, 10, 41,
      12, 4,  80, 77, 98, 50, 78, 35, 7,  1,  96, 67, 85, 4,  23, 38, 2,  57,
      4,  53, 0,  33, 2,  25, 14, 97, 87, 42, 15, 65, 19, 83, 67, 70, 80, 39,
      9,  5,  94, 86, 80, 92, 31, 17, 65, 51, 46, 66, 44, 3,  26, 0,  39, 20,
      11, 6,  55, 70, 11, 75, 82, 35, 47, 99, 5,  14, 23, 38, 94, 66, 64, 27,
      77, 50, 28, 25, 61, 10, 30, 15, 12, 24, 90, 25, 39, 47, 98, 83};

  const int64_t kCapacityArray[] = {850, 1400, 1500, 450, 1100};
  const int kArraySize = ABSL_ARRAYSIZE(kProfitArray);
  const int kNumberOfDimensions = ABSL_ARRAYSIZE(kCapacityArray);
  const int kOptimalProfit = 6339;
  const int64_t profit =
      SolveKnapsackProblem(kProfitArray, kArraySize, kWeightArray,
                           kCapacityArray, kNumberOfDimensions);
  EXPECT_EQ(kOptimalProfit, profit);
}

TEST(KnapsackSolverTest, SolveVeryDifficultInstanceForMIPSolvers) {
  const int64_t kProfitArray[] = {
      840350, 492312, 1032839, 811082, 382846, 441114, 386610, 783293, 998199,
      579384, 642499, 247987,  892801, 193061, 494328, 314360, 730783, 308562,
      799683, 676459, 591170,  551284, 218343, 730920, 513370, 531444, 214762,
      316396, 350372, 961409,  894479, 83114,  195613, 383992, 823112, 193730,
      198549, 454831, 239367,  712908, 819866, 156561, 445686, 668469, 526442,
      36085,  414327, 10450,   524913, 553583};
  const int64_t kWeightArray[] = {
      893821, 405554, 713114, 498726, 230483, 640836, 226067, 975043, 700562,
      627861, 720734, 205614, 458490, 181755, 616093, 447249, 852340, 415851,
      454072, 598218, 549422, 699689, 163910, 954988, 625015, 394931, 310015,
      207170, 194778, 758551, 956952, 74310,  276930, 313596, 481395, 170299,
      115532, 515859, 189626, 959419, 686824, 183455, 568483, 409119, 655220,
      24540,  523383, 9381,   735775, 812811};
  const int64_t kCapacityArray[] = {10922833};
  const int kArraySize = ABSL_ARRAYSIZE(kProfitArray);
  const int kNumberOfDimensions = ABSL_ARRAYSIZE(kCapacityArray);
  const int kOptimalProfit = 14723396;
  const int64_t profit =
      SolveKnapsackProblem(kProfitArray, kArraySize, kWeightArray,
                           kCapacityArray, kNumberOfDimensions);
  EXPECT_EQ(kOptimalProfit, profit);
}

TEST(KnapsackSolverTest, Item0IsNotPartOfTheOptimalSolution) {
  const int64_t kProfitArray[] = {16, 11, 26, 30, 31};
  const int64_t kWeightArray[] = {32, 9, 11, 9, 30};
  const int64_t kCapacityArray[] = {23};
  const int kArraySize = ABSL_ARRAYSIZE(kProfitArray);
  const int kNumberOfDimensions = ABSL_ARRAYSIZE(kCapacityArray);
  const int kOptimalProfit = 56;
  const int64_t profit =
      SolveKnapsackProblem(kProfitArray, kArraySize, kWeightArray,
                           kCapacityArray, kNumberOfDimensions);
  EXPECT_EQ(kOptimalProfit, profit);
}

TEST(KnapsackSolverTest, CheckNoRoudingIssues) {
  const int64_t kProfitArray[] = {2, 28, 30, 29, 7, 27, 18, 13, 32, 9};
  const int64_t kWeightArray[] = {1, 16, 22, 13, 11, 23, 5, 21, 29, 18};
  const int64_t kCapacityArray[] = {97};
  const int kArraySize = ABSL_ARRAYSIZE(kProfitArray);
  const int kNumberOfDimensions = ABSL_ARRAYSIZE(kCapacityArray);
  const int kOptimalProfit = 146;
  const int64_t profit =
      SolveKnapsackProblem(kProfitArray, kArraySize, kWeightArray,
                           kCapacityArray, kNumberOfDimensions);
  EXPECT_EQ(kOptimalProfit, profit);
}

TEST(KnapsackSolverTest, AllItemsReduced) {
  const int64_t kProfitArray[] = {30, 9, 21, 5, 19};
  const int64_t kWeightArray[] = {10, 4, 19, 6, 28};
  const int64_t kCapacityArray[] = {34};
  const int kArraySize = ABSL_ARRAYSIZE(kProfitArray);
  const int kNumberOfDimensions = ABSL_ARRAYSIZE(kCapacityArray);
  const int kOptimalProfit = 60;
  const int64_t profit =
      SolveKnapsackProblem(kProfitArray, kArraySize, kWeightArray,
                           kCapacityArray, kNumberOfDimensions);
  EXPECT_EQ(kOptimalProfit, profit);
}

}  // namespace
}  // namespace operations_research
