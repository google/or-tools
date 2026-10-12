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

#include "ortools/routing/neighborhoods.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/functional/bind_front.h"
#include "absl/types/span.h"
#include "google/protobuf/duration.pb.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"
#include "ortools/routing/enums.pb.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/parameters_utils.h"
#include "ortools/routing/routing.h"
#include "ortools/routing/routing_test_utils.h"
#include "ortools/routing/types.h"
#include "ortools/util/optional_boolean.pb.h"
#include "ortools/util/testing_utils.h"

namespace operations_research::routing {
namespace {

#define RELOCATE_NEIGHBORS_OPERATOR_TEST(name, init_group, init_forbidden)    \
  class MakeRelocateNeighbors##name##OperatorTest : public BaseOperatorTest { \
   protected:                                                                 \
    LocalSearchOperator* BuildOperator(const std::vector<IntVar*>& vars,      \
                                       Solver* solver) const override {       \
      const absl::flat_hash_set<int64_t> group = init_group;                  \
      const std::vector<int64_t> forbidden = init_forbidden;                  \
      for (int i = 0; i < init_forbidden.size(); i += 2) {                    \
        vars[forbidden[i]]->RemoveValue(forbidden[i + 1]);                    \
      }                                                                       \
      return MakeRelocateNeighbors(solver, vars, {}, nullptr,                 \
                                   [group](int64_t, int64_t j) {              \
                                     if (group.count(j) == 1) {               \
                                       return 0;                              \
                                     }                                        \
                                     return 10;                               \
                                   });                                        \
    }                                                                         \
  };

RELOCATE_NEIGHBORS_OPERATOR_TEST(NoGroup, absl::flat_hash_set<int64_t>{},
                                 std::vector<int64_t>{});

TEST_F(MakeRelocateNeighborsNoGroupOperatorTest, ExampleWithOnePath) {
  // This is an example similar to the one described in
  // ./routing_neighborhoods.h, in the comment of the
  // MakeRelocateNeighborsOperator class, where we
  // rename the nodes of the base path 0->1->2->3->4->5->6, eg. the A-F
  // nodes are mapped to 1-5, but we also remove all constraints (no precedence
  // constraint between A and B, no start/end constraint).
  SetFirstPaths(6, {{0, 1, 2, 3, 4, 5, 6}});
  AddExpectedPaths({{0, 2, 3, 4, 5, 1, 6}});
  AddExpectedPaths({{0, 3, 4, 5, 1, 2, 6}});
  AddExpectedPaths({{0, 1, 3, 4, 5, 2, 6}});
  AddExpectedPaths({{0, 4, 5, 1, 2, 3, 6}});
  AddExpectedPaths({{0, 1, 4, 5, 2, 3, 6}});
  AddExpectedPaths({{0, 1, 2, 4, 5, 3, 6}});
  AddExpectedPaths({{0, 5, 1, 2, 3, 4, 6}});
  AddExpectedPaths({{0, 1, 5, 2, 3, 4, 6}});
  AddExpectedPaths({{0, 1, 2, 5, 3, 4, 6}});
  AddExpectedPaths({{0, 1, 2, 3, 5, 4, 6}});
  // If we allowed permutation within a group the neighbors would be:
  // AddExpectedPaths({{0, 2, 1, 3, 4, 5, 6}});
  // AddExpectedPaths({{0, 3, 1, 2, 4, 5, 6}});
  // AddExpectedPaths({{0, 4, 1, 2, 3, 5, 6}});
  // AddExpectedPaths({{0, 5, 1, 2, 3, 4, 6}});
  // AddExpectedPaths({{0, 2, 3, 4, 5, 1, 6}});
  // AddExpectedPaths({{0, 1, 3, 2, 4, 5, 6}});
  // AddExpectedPaths({{0, 1, 4, 2, 3, 5, 6}});
  // AddExpectedPaths({{0, 1, 5, 2, 3, 4, 6}});
  // AddExpectedPaths({{0, 3, 4, 5, 1, 2, 6}});
  // AddExpectedPaths({{0, 1, 3, 4, 5, 2, 6}});
  // AddExpectedPaths({{0, 1, 2, 4, 3, 5, 6}});
  // AddExpectedPaths({{0, 1, 2, 5, 3, 4, 6}});
  // AddExpectedPaths({{0, 4, 5, 1, 2, 3, 6}});
  // AddExpectedPaths({{0, 1, 4, 5, 2, 3, 6}});
  // AddExpectedPaths({{0, 1, 2, 4, 5, 3, 6}});
  // AddExpectedPaths({{0, 1, 2, 3, 5, 4, 6}});
  // AddExpectedPaths({{0, 5, 1, 2, 3, 4, 6}});
  // AddExpectedPaths({{0, 1, 5, 2, 3, 4, 6}});
  // AddExpectedPaths({{0, 1, 2, 5, 3, 4, 6}});
  // AddExpectedPaths({{0, 1, 2, 3, 5, 4, 6}});
  BaseSolveAndTest(/*match_all=*/true);
}

RELOCATE_NEIGHBORS_OPERATOR_TEST(SmallGroup,
                                 (absl::flat_hash_set<int64_t>{1, 2}),
                                 std::vector<int64_t>{});

TEST_F(MakeRelocateNeighborsSmallGroupOperatorTest, ExampleWithOnePath) {
  // Same example as MakeRelocateNeighborsNoGroupOperatorTest.ExampleWithOnePath
  // but adds a group of nodes which will be moved together due to the cost
  // definition.
  SetFirstPaths(6, {{0, 1, 2, 3, 4, 5, 6}});
  AddExpectedPaths({{0, 3, 1, 2, 4, 5, 6}});
  AddExpectedPaths({{0, 3, 4, 1, 2, 5, 6}});
  AddExpectedPaths({{0, 3, 4, 5, 1, 2, 6}});
  AddExpectedPaths({{0, 2, 1, 3, 4, 5, 6}});
  AddExpectedPaths({{0, 1, 3, 2, 4, 5, 6}});
  AddExpectedPaths({{0, 1, 3, 4, 2, 5, 6}});
  AddExpectedPaths({{0, 1, 3, 4, 5, 2, 6}});
  AddExpectedPaths({{0, 3, 4, 5, 1, 2, 6}});
  AddExpectedPaths({{0, 1, 3, 4, 5, 2, 6}});
  AddExpectedPaths({{0, 4, 5, 1, 2, 3, 6}});
  AddExpectedPaths({{0, 1, 4, 5, 2, 3, 6}});
  AddExpectedPaths({{0, 1, 2, 4, 5, 3, 6}});
  AddExpectedPaths({{0, 5, 1, 2, 3, 4, 6}});
  AddExpectedPaths({{0, 1, 5, 2, 3, 4, 6}});
  AddExpectedPaths({{0, 1, 2, 5, 3, 4, 6}});
  AddExpectedPaths({{0, 1, 2, 3, 5, 4, 6}});
  // If we allowed permutation within a group the neighbors would be:
  // AddExpectedPaths({{0, 2, 1, 3, 4, 5, 6}});
  // AddExpectedPaths({{0, 3, 1, 2, 4, 5, 6}});
  // AddExpectedPaths({{0, 3, 4, 1, 2, 5, 6}});
  // AddExpectedPaths({{0, 3, 4, 5, 1, 2, 6}});
  // AddExpectedPaths({{0, 2, 1, 3, 4, 5, 6}});
  // AddExpectedPaths({{0, 1, 3, 2, 4, 5, 6}});
  // AddExpectedPaths({{0, 1, 3, 4, 2, 5, 6}});
  // AddExpectedPaths({{0, 1, 3, 4, 5, 2, 6}});
  // AddExpectedPaths({{0, 3, 4, 5, 1, 2, 6}});
  // AddExpectedPaths({{0, 1, 3, 4, 5, 2, 6}});
  // AddExpectedPaths({{0, 1, 2, 4, 3, 5, 6}});
  // AddExpectedPaths({{0, 1, 2, 5, 3, 4, 6}});
  // AddExpectedPaths({{0, 4, 5, 1, 2, 3, 6}});
  // AddExpectedPaths({{0, 1, 4, 5, 2, 3, 6}});
  // AddExpectedPaths({{0, 1, 2, 4, 5, 3, 6}});
  // AddExpectedPaths({{0, 1, 2, 3, 5, 4, 6}});
  // AddExpectedPaths({{0, 5, 1, 2, 3, 4, 6}});
  // AddExpectedPaths({{0, 1, 5, 2, 3, 4, 6}});
  // AddExpectedPaths({{0, 1, 2, 5, 3, 4, 6}});
  // AddExpectedPaths({{0, 1, 2, 3, 5, 4, 6}});
  BaseSolveAndTest(/*match_all=*/true);
}

RELOCATE_NEIGHBORS_OPERATOR_TEST(SmallGroupWithForbidden,
                                 (absl::flat_hash_set<int64_t>{1, 2}),
                                 (std::vector<int64_t>{4, 1}));

TEST_F(MakeRelocateNeighborsSmallGroupWithForbiddenOperatorTest,
       ExampleWithOnePath) {
  // Similar example to MakeRelocateNeighborsSmallGroupOperatorTest
  // .ExampleWithOnePath but adds a forbidden arc.
  SetFirstPaths(5, {{0, 1, 2, 3, 4, 5}});
  AddExpectedPaths({{0, 3, 1, 2, 4, 5}});
  AddExpectedPaths({{0, 3, 1, 4, 2, 5}});
  AddExpectedPaths({{0, 2, 1, 3, 4, 5}});
  AddExpectedPaths({{0, 1, 3, 2, 4, 5}});
  AddExpectedPaths({{0, 1, 3, 4, 2, 5}});
  AddExpectedPaths({{0, 3, 4, 1, 2, 5}});
  AddExpectedPaths({{0, 1, 3, 4, 2, 5}});
  AddExpectedPaths({{0, 4, 1, 2, 3, 5}});
  AddExpectedPaths({{0, 1, 4, 2, 3, 5}});
  AddExpectedPaths({{0, 1, 2, 4, 3, 5}});
  // If we allowed permutation within a group the neighbors would be:
  // AddExpectedPaths({{0, 2, 1, 3, 4, 5}});
  // AddExpectedPaths({{0, 3, 1, 2, 4, 5}});
  // AddExpectedPaths({{0, 3, 1, 4, 2, 5}});
  // AddExpectedPaths({{0, 2, 1, 3, 4, 5}});
  // AddExpectedPaths({{0, 1, 3, 2, 4, 5}});
  // AddExpectedPaths({{0, 1, 3, 4, 2, 5}});
  // AddExpectedPaths({{0, 3, 4, 1, 2, 5}});
  // AddExpectedPaths({{0, 1, 3, 4, 2, 5}});
  // AddExpectedPaths({{0, 1, 2, 4, 3, 5}});
  // AddExpectedPaths({{0, 4, 1, 2, 3, 5}});
  // AddExpectedPaths({{0, 1, 4, 2, 3, 5}});
  // AddExpectedPaths({{0, 1, 2, 4, 3, 5}});
  BaseSolveAndTest(/*match_all=*/true);
}

#undef RELOCATE_NEIGHBORS_OPERATOR_TEST

#define RELOCATE_NEIGHBORS_WITH_NEIGHBORS_OPERATOR_TEST(                 \
    name, init_group, init_forbidden, init_neighbors)                    \
  class MakeRelocateNeighborsWithNeighbors##name##OperatorTest           \
      : public BaseOperatorTest {                                        \
   protected:                                                            \
    LocalSearchOperator* BuildOperator(const std::vector<IntVar*>& vars, \
                                       Solver* solver) const override {  \
      const absl::flat_hash_set<int64_t> group = init_group;             \
      const std::vector<int64_t> forbidden = init_forbidden;             \
      for (int i = 0; i < init_forbidden.size(); i += 2) {               \
        vars[forbidden[i]]->RemoveValue(forbidden[i + 1]);               \
      }                                                                  \
      const std::vector<int> neighbors = init_neighbors;                 \
      return MakeRelocateNeighbors(                                      \
          solver, vars, {}, nullptr, nullptr,                            \
          [neighbors](int, int) -> const std::vector<int>& {             \
            return neighbors;                                            \
          },                                                             \
          [group](int64_t, int64_t j) {                                  \
            if (group.count(j) == 1) {                                   \
              return 0;                                                  \
            }                                                            \
            return 10;                                                   \
          });                                                            \
    }                                                                    \
  };

RELOCATE_NEIGHBORS_WITH_NEIGHBORS_OPERATOR_TEST(NoGroup,
                                                absl::flat_hash_set<int64_t>{},
                                                std::vector<int64_t>{},
                                                (std::vector<int>{1}));

TEST_F(MakeRelocateNeighborsWithNeighborsNoGroupOperatorTest,
       ExampleWithOnePath) {
  SetFirstPaths(6, {{0, 1, 2, 3, 4, 5, 6}});
  BaseSolveAndTest(/*match_all=*/true);
}

RELOCATE_NEIGHBORS_WITH_NEIGHBORS_OPERATOR_TEST(
    SmallGroup, (absl::flat_hash_set<int64_t>{1, 2}), std::vector<int64_t>{},
    (std::vector<int>{1}));

TEST_F(MakeRelocateNeighborsWithNeighborsSmallGroupOperatorTest,
       ExampleWithOnePath) {
  SetFirstPaths(6, {{0, 1, 2, 3, 4, 5, 6}});
  AddExpectedPaths({{0, 3, 1, 2, 4, 5, 6}});
  AddExpectedPaths({{0, 3, 4, 1, 2, 5, 6}});
  AddExpectedPaths({{0, 3, 4, 5, 1, 2, 6}});
  BaseSolveAndTest(/*match_all=*/true);
}

RELOCATE_NEIGHBORS_WITH_NEIGHBORS_OPERATOR_TEST(
    SmallGroupWithForbidden, (absl::flat_hash_set<int64_t>{1, 2}),
    (std::vector<int64_t>{4, 1}), (std::vector<int>{1}));

TEST_F(MakeRelocateNeighborsWithNeighborsSmallGroupWithForbiddenOperatorTest,
       ExampleWithOnePath) {
  SetFirstPaths(5, {{0, 1, 2, 3, 4, 5}});
  AddExpectedPaths({{0, 3, 1, 2, 4, 5}});
  AddExpectedPaths({{0, 3, 1, 4, 2, 5}});
  BaseSolveAndTest(/*match_all=*/true);
}

#undef RELOCATE_NEIGHBORS_WITH_NEIGHBORS_OPERATOR_TEST

#define TWO_OPT_SHORTEST_PATH_TEST(name, init_alternatives, callback)      \
  class TwoOptShortestPath##name##OperatorTest : public BaseOperatorTest { \
   protected:                                                              \
    LocalSearchOperator* BuildOperator(const std::vector<IntVar*>& vars,   \
                                       Solver* solver) const override {    \
      const std::vector<std::vector<int64_t>> alternatives =               \
          init_alternatives;                                               \
      return MakeTwoOptWithShortestPath(solver, vars, {}, nullptr,         \
                                        alternatives, callback);           \
    }                                                                      \
  };

TWO_OPT_SHORTEST_PATH_TEST(NoAlternatives,
                           (std::vector<std::vector<int64_t>>{}),
                           ([](int64_t i, int64_t) { return i; }));

TEST_F(TwoOptShortestPathNoAlternativesOperatorTest, OnePath) {
  SetFirstPaths(4, {{0, 1, 2, 3, 4}});
  BaseSolveAndTest(/*match_all=*/true);
}

TWO_OPT_SHORTEST_PATH_TEST(WithAlternatives,
                           (std::vector<std::vector<int64_t>>{
                               {1, 4}, {2, 5}, {3, 6}}),
                           ([](int64_t i, int64_t) { return i; }));

TEST_F(TwoOptShortestPathWithAlternativesOperatorTest, OnePath) {
  SetFirstPaths(7, {{0, 4, 5, 6, 7}});
  AddExpectedPaths({{0, 1, 5, 6, 7}});
  AddExpectedPaths({{0, 2, 1, 6, 7}});
  AddExpectedPaths({{0, 3, 2, 1, 7}});
  AddExpectedPaths({{0, 4, 2, 6, 7}});
  AddExpectedPaths({{0, 4, 3, 2, 7}});
  AddExpectedPaths({{0, 4, 5, 3, 7}});
  BaseSolveAndTest(/*match_all=*/true);
}

TWO_OPT_SHORTEST_PATH_TEST(WithAlternativeMix,
                           (std::vector<std::vector<int64_t>>{{1, 3}, {2, 5}}),
                           ([](int64_t i, int64_t) { return i; }));

TEST_F(TwoOptShortestPathWithAlternativeMixOperatorTest, OnePath) {
  SetFirstPaths(6, {{0, 3, 4, 5, 6}});
  AddExpectedPaths({{0, 1, 4, 5, 6}});
  AddExpectedPaths({{0, 4, 1, 5, 6}});
  AddExpectedPaths({{0, 2, 4, 1, 6}});
  AddExpectedPaths({{0, 3, 2, 4, 6}});
  AddExpectedPaths({{0, 3, 4, 2, 6}});
  BaseSolveAndTest(/*match_all=*/true);
}

#undef TWO_OPT_SHORTEST_PATH_TEST

#define SWAP_ACTIVE_TO_SHORTEST_PATH_TEST(name, init_alternatives, callback) \
  class SwapActiveToShortestPath##name##OperatorTest                         \
      : public BaseOperatorTest {                                            \
   protected:                                                                \
    LocalSearchOperator* BuildOperator(const std::vector<IntVar*>& vars,     \
                                       Solver* solver) const override {      \
      const std::vector<std::vector<int64_t>> alternatives =                 \
          init_alternatives;                                                 \
      return MakeSwapActiveToShortestPath(solver, vars, {}, nullptr,         \
                                          alternatives, callback);           \
    }                                                                        \
  };

SWAP_ACTIVE_TO_SHORTEST_PATH_TEST(NoAlternatives,
                                  (std::vector<std::vector<int64_t>>{}),
                                  ([](int64_t, int64_t) { return 1; }));

TEST_F(SwapActiveToShortestPathNoAlternativesOperatorTest, EmptyPath) {
  SetFirstPaths(1, {{0, 2}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(SwapActiveToShortestPathNoAlternativesOperatorTest, OnePath) {
  SetFirstPaths(3, {{0, 1, 2, 3}});
  BaseSolveAndTest(/*match_all=*/true);
}

// One node with one alternative (1,2). Cost function makes using 1 expensive.
SWAP_ACTIVE_TO_SHORTEST_PATH_TEST(OneAlternative,
                                  (std::vector<std::vector<int64_t>>{{1, 2}}),
                                  ([](int64_t i, int64_t j) {
                                    if (i == j) return 0;
                                    if (i == 2 && j != 1) return 1;
                                    return 10;
                                  }));

TEST_F(SwapActiveToShortestPathOneAlternativeOperatorTest, OnePath) {
  SetFirstPaths(3, {{0, 1, 3}});
  AddExpectedPaths({{0, 2, 3}});
  BaseSolveAndTest(/*match_all=*/true);
}

// Two nodes with alternatives (1,2) and (4,3). Cost function makes transitions
// towards 1 and 4 expensive, favoring 2 and 3.
SWAP_ACTIVE_TO_SHORTEST_PATH_TEST(SeveralAlternatives,
                                  (std::vector<std::vector<int64_t>>{{1, 2},
                                                                     {4, 3}}),
                                  ([](int64_t, int64_t j) {
                                    if (j == 1 || j == 4) return 2;
                                    return 1;
                                  }));

TEST_F(SwapActiveToShortestPathSeveralAlternativesOperatorTest, EmptyPath) {
  SetFirstPaths(1, {{0, 2}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(SwapActiveToShortestPathSeveralAlternativesOperatorTest, OnePath) {
  SetFirstPaths(5, {{0, 1, 4, 6}});
  AddExpectedPaths({{0, 2, 3, 6}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(SwapActiveToShortestPathSeveralAlternativesOperatorTest,
       OnePathExtraNodeBefore) {
  SetFirstPaths(6, {{0, 5, 1, 4, 6}});
  AddExpectedPaths({{0, 5, 2, 3, 6}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(SwapActiveToShortestPathSeveralAlternativesOperatorTest,
       OnePathExtraNodeAfter) {
  SetFirstPaths(6, {{0, 1, 4, 5, 6}});
  AddExpectedPaths({{0, 2, 3, 5, 6}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(SwapActiveToShortestPathSeveralAlternativesOperatorTest,
       OnePathExtraNodeBetween) {
  SetFirstPaths(6, {{0, 1, 5, 4, 6}});
  AddExpectedPaths({{0, 2, 5, 4, 6}});
  AddExpectedPaths({{0, 1, 5, 3, 6}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(SwapActiveToShortestPathSeveralAlternativesOperatorTest, TwoPaths) {
  SetFirstPaths(6, {{0, 1, 6}, {5, 4, 7}});
  AddExpectedPaths({{0, 2, 6}, {5, 4, 7}});
  AddExpectedPaths({{0, 1, 6}, {5, 3, 7}});
  BaseSolveAndTest(/*match_all=*/true);
}

// Two nodes with duplicate alternatives (1,2) and (1,2) which leads to cycles.
// Cost function makes transitions towards 2 expensive, favoring 1.
SWAP_ACTIVE_TO_SHORTEST_PATH_TEST(SeveralAlternativesWithCycles,
                                  (std::vector<std::vector<int64_t>>{{1, 2}}),
                                  ([](int64_t, int64_t j) {
                                    if (j == 2) return 2;
                                    return 1;
                                  }));

TEST_F(SwapActiveToShortestPathSeveralAlternativesWithCyclesOperatorTest,
       OnePath) {
  SetFirstPaths(3, {{0, 1, 2, 3}});
  AddExpectedPaths({{0, 3}});  // 1 and 2 unperformed.
  BaseSolveAndTest(/*match_all=*/true);
}

#undef SWAP_ACTIVE_TO_SHORTEST_PATH_TEST

#define PAIR_ACTIVE_OPERATOR_TEST(name, init_index_pairs)                   \
  class PairActive##name##OperatorTest : public BaseOperatorTest {          \
   protected:                                                               \
    LocalSearchOperator* BuildOperatorWithSecondaryVars(                    \
        const std::vector<IntVar*>& vars,                                   \
        const std::vector<IntVar*>& secondary_vars,                         \
        Solver* solver) const override {                                    \
      const std::vector<PickupDeliveryPair> index_pairs = init_index_pairs; \
      return MakePairActive(solver, vars, secondary_vars, nullptr,          \
                            index_pairs);                                   \
    }                                                                       \
  };

PAIR_ACTIVE_OPERATOR_TEST(NoPairs, (std::vector<PickupDeliveryPair>{}));

TEST_F(PairActiveNoPairsOperatorTest, OnePath) {
  SetFirstSolution({2, 1, 3, 4, 5, 6},
                   {0, -1, 0, 0, 0, 0, 0});  // 0->2->3->4->5->6 & 1 unperformed
  BaseSolveAndTest(/*match_all=*/true);
}

PAIR_ACTIVE_OPERATOR_TEST(OnePair,
                          (std::vector<PickupDeliveryPair>{{{1}, {2}}}));

TEST_F(PairActiveOnePairOperatorTest, MixedPair) {
  SetFirstSolution({2, 1, 3, 4, 5, 6});  // 0->2->3->4->5->6 & 1 unperformed
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairActiveOnePairOperatorTest, OnePath) {
  SetFirstSolution({3, 1, 2, 4});   // 0->3->4 & 1,2 unperformed
  AddExpectedDelta({1, 2, 3, -1});  // 0->1->2->3->4
  AddExpectedDelta({1, 3, 4, 2});   // 0->1->3->2->4
  AddExpectedDelta({-1, 2, 4, 1});  // 0->3->1->2->4
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairActiveOnePairOperatorTest, TwoPaths) {
  SetFirstSolution({3, 1, 2, 5, 6});    // 0->3->5 4->6 & 1,2 unperformed
  AddExpectedDelta({1, 2, 3, -1, -1});  // 0->1->2->3->5
  AddExpectedDelta({1, 3, 5, 2, -1});   // 0->1->3->2->5
  AddExpectedDelta({-1, 2, 5, 1, -1});  // 0->3->1->2->5
  AddExpectedDelta({-1, 2, 6, -1, 1});  // 4->1->2->6
  BaseSolveAndTest(/*match_all=*/true, [this, called = false]() mutable {
    if (called) return false;
    called = true;
    // On the second round of solve the move detects that all paths are
    // optimal and bails out.
    ClearDeltas();
    return true;
  });
}

TEST_F(PairActiveOnePairOperatorTest, TwoPathsWithUpdatedSolution) {
  SetFirstSolution({3, 1, 2, 5, 6});    // 0->3->5 4->6 & 1,2 unperformed
  AddExpectedDelta({1, 2, 3, -1, -1});  // 0->1->2->3->5
  AddExpectedDelta({1, 3, 5, 2, -1});   // 0->1->3->2->5
  AddExpectedDelta({-1, 2, 5, 1, -1});  // 0->3->1->2->5
  AddExpectedDelta({-1, 2, 6, -1, 1});  // 4->1->2->6
  BaseSolveAndTest(/*match_all=*/true, [this, called = false]() mutable {
    if (called) return false;
    called = true;
    // On the second round we modify the current solution, triggering new
    // moves.
    SetFirstSolution({5, 1, 2, 6, 3});  // 0->5 4->3->6 & 1,2 unperformed
    ClearDeltas();
    AddExpectedDelta({1, 2, 5, -1, -1});  // 0->1->2->5
    AddExpectedDelta({-1, 2, 3, -1, 1});  // 4->1->2->3->6
    AddExpectedDelta({-1, 3, 6, 2, 1});   // 4->1->3->2->6
    AddExpectedDelta({-1, 2, 6, 1, -1});  // 4->3->1->2->6
    return true;
  });
}

TEST_F(PairActiveOnePairOperatorTest, TwoPathsWithAllowedPaths) {
  SetFirstSolution({3, 1, 2, 5, 6},
                   {0, -1, -1, 0, 1, 0, 1});  // 0->3->5 4->6 & 1,2 unperformed
  SetAllowedSecondaryValues({{}, {1}, {1}, {}, {}, {}, {}});
  AddExpectedDelta({-1, 2, 6, -1, 1});  // 4->1->2->6
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairActiveOnePairOperatorTest, ThreePathsWithAllowedPaths) {
  SetFirstSolution({3, 1, 2, 7, 9, 8, 4},
                   {0, -1, -1, 0, 2, 1, 2, 0, 1,
                    2});  // 0->3->7 5->8 6->4->9 & 1,2 unperformed
  SetAllowedSecondaryValues({{}, {1}, {1}});
  AddExpectedDelta({-1, 2, 8, -1, -1, 1, -1});  // 5->1->2->8
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairActiveOnePairOperatorTest, ThreePathsWithMoreAllowedPaths) {
  SetFirstSolution({3, 1, 2, 7, 9, 8, 4},
                   {0, -1, -1, 0, 2, 1, 2, 0, 1,
                    2});  // 0->3->7 5->8 6->4->9 & 1,2 unperformed
  SetAllowedSecondaryValues({{}, {1, 2}, {1, 2}});
  AddExpectedDelta({-1, 2, 8, -1, -1, 1, -1});  // 5->1->2->8
  AddExpectedDelta({-1, 2, 4, -1, -1, -1, 1});  // 6->1->2->4->9
  AddExpectedDelta({-1, 4, 9, -1, 2, -1, 1});   // 6->1->4->2->9
  AddExpectedDelta({-1, 2, 9, -1, 1, -1, -1});  // 6->4->1->2->9
  BaseSolveAndTest(/*match_all=*/true);
}

PAIR_ACTIVE_OPERATOR_TEST(TwoPairs, (std::vector<PickupDeliveryPair>{
                                        {{1}, {3}}, {{2}, {4}}}));

TEST_F(PairActiveTwoPairsOperatorTest, OnePathAllPairsUnperformed) {
  SetFirstSolution({5, 1, 2, 3, 4, 6});     // 0->5->6 & 1,2,3,4 unperformed
  AddExpectedDelta({1, 3, -1, 5, -1, -1});  // 0->1->3->5->6
  AddExpectedDelta({1, 5, -1, 6, -1, 3});   // 0->1->5->3->6
  AddExpectedDelta({-1, 3, -1, 6, -1, 1});  // 0->5->1->3->6
  AddExpectedDelta({2, -1, 4, -1, 5, -1});  // 0->2->4->5->6
  AddExpectedDelta({2, -1, 5, -1, 6, 4});   // 0->2->5->4->6
  AddExpectedDelta({-1, -1, 4, -1, 6, 2});  // 0->5->2->4->6
  BaseSolveAndTest(/*match_all=*/true);
}

PAIR_ACTIVE_OPERATOR_TEST(OneMultiplePair, (std::vector<PickupDeliveryPair>{
                                               {{1, 2}, {4, 3, 5}}}));

TEST_F(PairActiveOneMultiplePairOperatorTest, OnePathNoPairsUnperformed) {
  SetFirstSolution({1, 3, 2, 6, 4, 5, 7});  // 0->1->3->6->7 & 2,4,5 unperformed
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairActiveOneMultiplePairOperatorTest, OnePathAllPairsUnperformed) {
  SetFirstSolution({6, 1, 2, 3, 4, 5, 7});  // 0->6->7 & 1,2,3,4,5 unperformed
  for (int b : {4, 3, 5}) {
    for (int a : {1, 2}) {
      std::vector<int> d1 = {a, -1, -1, -1, -1, -1, -1};
      d1[a] = b;
      d1[b] = 6;
      AddExpectedDelta(d1);  // 0->a->b->6->7
      std::vector<int> d2 = {a, -1, -1, -1, -1, -1, b};
      d2[a] = 6;
      d2[b] = 7;
      AddExpectedDelta(d2);  // 0->a->6->b->7
      std::vector<int> d3 = {-1, -1, -1, -1, -1, -1, a};
      d3[a] = b;
      d3[b] = 7;
      AddExpectedDelta(d3);  // 0->6->a->b->7
    }
  }
  BaseSolveAndTest(/*match_all=*/true);
}

PAIR_ACTIVE_OPERATOR_TEST(TwoMultiplePair,
                          (std::vector<PickupDeliveryPair>{{{1, 2}, {3, 4}},
                                                           {{5, 6}, {7, 8}}}));

TEST_F(PairActiveTwoMultiplePairOperatorTest, OnePathNoPairsUnperformed) {
  SetFirstSolution({1, 3, 2, 5, 4, 7, 6, 9, 8, 10});  // 0->1->3->5->7->9->10
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairActiveTwoMultiplePairOperatorTest, OnePathAllPairsUnperformed) {
  SetFirstSolution({9, 1, 2, 3, 4, 5, 6, 7, 8, 10});  // 0->9->10
  const auto AddDelta = [this](absl::Span<const int> first,
                               absl::Span<const int> second) {
    for (int b : second) {
      for (int a : first) {
        std::vector<int> d1 = {a, -1, -1, -1, -1, -1, -1, -1, -1, -1};
        d1[a] = b;
        d1[b] = 9;
        AddExpectedDelta(d1);  // 0->a->b->9->10
        std::vector<int> d2 = {a, -1, -1, -1, -1, -1, -1, -1, -1, b};
        d2[a] = 9;
        d2[b] = 10;
        AddExpectedDelta(d2);  // 0->a->9->b->10
        std::vector<int> d3 = {-1, -1, -1, -1, -1, -1, -1, -1, -1, a};
        d3[a] = b;
        d3[b] = 10;
        AddExpectedDelta(d3);  // 0->9->a->b->10
      }
    }
  };
  AddDelta({1, 2}, {3, 4});
  AddDelta({5, 6}, {7, 8});
  BaseSolveAndTest(/*match_all=*/true);
}

#undef PAIR_ACTIVE_OPERATOR_TEST

#define PAIR_INACTIVE_OPERATOR_TEST(name, init_index_pairs)                 \
  class PairInactive##name##OperatorTest : public BaseOperatorTest {        \
   protected:                                                               \
    LocalSearchOperator* BuildOperator(const std::vector<IntVar*>& vars,    \
                                       Solver* solver) const override {     \
      const std::vector<PickupDeliveryPair> index_pairs = init_index_pairs; \
      return MakePairInactive(solver, vars, {}, nullptr, index_pairs);      \
    }                                                                       \
  };

PAIR_INACTIVE_OPERATOR_TEST(NoPairs, (std::vector<PickupDeliveryPair>{}));

TEST_F(PairInactiveNoPairsOperatorTest, OnePath) {
  SetFirstSolution({2, 1, 3, 4, 5, 6});  // 0->2->3->4->5->6 & 1 unperformed
  BaseSolveAndTest(/*match_all=*/true);
}

PAIR_INACTIVE_OPERATOR_TEST(OnePair,
                            (std::vector<PickupDeliveryPair>{{{1}, {2}}}));

TEST_F(PairInactiveNoPairsOperatorTest, MixedPair) {
  SetFirstSolution({2, 1, 3, 4, 5, 6});  // 0->2->3->4->5->6 & 1 unperformed
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairInactiveOnePairOperatorTest, OnePath) {
  SetFirstSolution({1, 2, 3, 4});   // 0->1->2->3->4
  AddExpectedDelta({3, 1, 2, -1});  // 0->3->4 & 1,2 unperformed
  BaseSolveAndTest(/*match_all=*/true);
}

PAIR_INACTIVE_OPERATOR_TEST(OneMultiplePair, (std::vector<PickupDeliveryPair>{
                                                 {{1, 2}, {4, 3, 5}}}));

TEST_F(PairInactiveOneMultiplePairOperatorTest, OnePathNoPairsPerformed) {
  SetFirstSolution({6, 1, 2, 3, 4, 5, 7});  // 0->6->7 & 1,2,3,4,5 unperformed
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairInactiveOneMultiplePairOperatorTest, OnePathPairsPerformed) {
  SetFirstSolution({2, 1, 3, 6, 4, 5, 7});  // 0->2->3->6->7 & 1,4,5 unperformed
  AddExpectedDelta(
      {6, -1, 2, 3, -1, -1, -1});  // 0->6->7 & 1,2,3,4,5 unperformed
  BaseSolveAndTest(/*match_all=*/true);
}

PAIR_INACTIVE_OPERATOR_TEST(TwoPairs, (std::vector<PickupDeliveryPair>{
                                          {{1}, {3}}, {{2}, {4}}}));

TEST_F(PairInactiveTwoPairsOperatorTest, OnePathAllPairsPerformed) {
  SetFirstSolution({1, 2, 3, 4, 5, 6});    // 0->1->2->3->4->5->6
  AddExpectedDelta({2, 1, 4, 3, -1, -1});  // 0->2->4->5->6
  AddExpectedDelta({-1, 3, 2, 5, 4, -1});  // 0->1->3->5->6
  BaseSolveAndTest(/*match_all=*/true);
}

PAIR_INACTIVE_OPERATOR_TEST(TwoMultiplePair,
                            (std::vector<PickupDeliveryPair>{
                                {{1, 2}, {3, 4}}, {{5, 6}, {7, 8}}}));
TEST_F(PairInactiveTwoMultiplePairOperatorTest, OnePathNoPairsPerformed) {
  SetFirstSolution({9, 1, 2, 3, 4, 5, 6, 7, 8, 10});  // 0->9->10
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairInactiveTwoMultiplePairOperatorTest, OnePathPairsUnperformed) {
  SetFirstSolution({2, 1, 3, 5, 4, 8, 6, 7, 9, 10});  // 0->2->3->5->8->9->10
  AddExpectedDelta({5, -1, 2, 3, -1, -1, -1, -1, -1, -1});  // 0->5->8->9->10
  AddExpectedDelta({-1, -1, -1, 9, -1, 5, -1, -1, 8, -1});  // 0->2->3->9->10
  BaseSolveAndTest(/*match_all=*/true);
}

#undef PAIR_INACTIVE_OPERATOR_TEST

#define SWAP_INDEX_PAIR_OPERATOR_TEST(name, init_index_pairs)               \
  class SwapIndexPair##name##OperatorTest : public BaseOperatorTest {       \
   protected:                                                               \
    LocalSearchOperator* BuildOperator(const std::vector<IntVar*>& vars,    \
                                       Solver* solver) const override {     \
      const std::vector<PickupDeliveryPair> index_pairs = init_index_pairs; \
      return solver->RevAlloc(                                              \
          new SwapIndexPairOperator(vars, {}, index_pairs));                \
    }                                                                       \
  };

SWAP_INDEX_PAIR_OPERATOR_TEST(NoPairs, std::vector<PickupDeliveryPair>{});

TEST_F(SwapIndexPairNoPairsOperatorTest, OnePath) {
  SetFirstSolution({1, 2, 3, 4, 5, 6});  // 0->1->2->3->4->5->6
  BaseSolveAndTest(/*match_all=*/true);
}

SWAP_INDEX_PAIR_OPERATOR_TEST(Singleton1,
                              (std::vector<PickupDeliveryPair>{{{1}, {}}}));

TEST_F(SwapIndexPairSingleton1OperatorTest, OnePath) {
  SetFirstSolution({1, 2, 3, 4, 5, 6});  // 0->1->2->3->4->5->6
  BaseSolveAndTest(/*match_all=*/true);
}

SWAP_INDEX_PAIR_OPERATOR_TEST(Singleton2,
                              (std::vector<PickupDeliveryPair>{{{}, {1}}}));

TEST_F(SwapIndexPairSingleton2OperatorTest, OnePath) {
  SetFirstSolution({1, 2, 3, 4, 5, 6});  // 0->1->2->3->4->5->6
  BaseSolveAndTest(/*match_all=*/true);
}

SWAP_INDEX_PAIR_OPERATOR_TEST(OneSimplePair,
                              (std::vector<PickupDeliveryPair>{{{1}, {2}}}));

TEST_F(SwapIndexPairOneSimplePairOperatorTest, OnePath) {
  SetFirstSolution({1, 2, 3, 4, 5, 6});  // 0->1->2->3->4->5->6
  // No expected deltas.
  BaseSolveAndTest(/*match_all=*/true);
}

SWAP_INDEX_PAIR_OPERATOR_TEST(TwoSimplePairs, (std::vector<PickupDeliveryPair>{
                                                  {{1}, {2}}, {{3}, {4}}}));

TEST_F(SwapIndexPairTwoSimplePairsOperatorTest, OnePathAndUnperformedPair1) {
  SetFirstSolution({3, 1, 2, 4, 5, 6});  // 0->3->4->5->6, 1,2 unperformed
  // No expected deltas.
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(SwapIndexPairTwoSimplePairsOperatorTest, OnePathAndUnperformedPair2) {
  SetFirstSolution({1, 2, 5, 3, 4, 6});  // 0->1->2->5->6, 3,4 unperformed
  // No expected deltas.
  BaseSolveAndTest(/*match_all=*/true);
}

SWAP_INDEX_PAIR_OPERATOR_TEST(OneMultiplePair, (std::vector<PickupDeliveryPair>{
                                                   {{1, 2}, {4, 3, 5}}}));

TEST_F(SwapIndexPairOneMultiplePairOperatorTest, OnePath) {
  SetFirstSolution({1, 4, 2, 3, 6, 5});     // 0->1->4->6
  AddExpectedDelta({1, 4, -1, -1, 6, -1});  // 0->1->4->6
  AddExpectedDelta({1, 3, -1, 6, 4, -1});   // 0->1->3->6
  AddExpectedDelta({1, 5, -1, -1, 4, 6});   // 0->1->5->6
  AddExpectedDelta({2, 1, 4, -1, 6, -1});   // 0->2->4->6
  AddExpectedDelta({2, 1, 3, 6, 4, -1});    // 0->2->3->6
  AddExpectedDelta({2, 1, 5, -1, 4, 6});    // 0->2->5->6
  BaseSolveAndTest(/*match_all=*/true);
}

#undef SWAP_INDEX_PAIR_OPERATOR_TEST

#define PAIR_RELOCATE_OPERATOR_TEST(name, init_index_pairs)                 \
  class PairRelocate##name##OperatorTest : public BaseOperatorTest {        \
   protected:                                                               \
    LocalSearchOperator* BuildOperator(const std::vector<IntVar*>& vars,    \
                                       Solver* solver) const override {     \
      const std::vector<PickupDeliveryPair> index_pairs = init_index_pairs; \
      return MakePairRelocate(solver, vars, {}, nullptr, index_pairs);      \
    }                                                                       \
  };

PAIR_RELOCATE_OPERATOR_TEST(OnePair,
                            (std::vector<PickupDeliveryPair>{{{1}, {2}}}));

TEST_F(PairRelocateOnePairOperatorTest, OnePath1) {
  SetFirstSolution({1, 2, 3, 4});   // 0->1->2->3->4
  AddExpectedDelta({-1, 3, 4, 2});  // 0->1->3->2->4
  AddExpectedDelta({3, -1, 4, 1});  // 0->3->1->2->4
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairRelocateOnePairOperatorTest, OnePath2) {
  SetFirstSolution({1, 3, 4, 2});   // 0->1->3->2->4
  AddExpectedDelta({-1, 2, 3, 4});  // 0->1->2->3->4
  AddExpectedDelta({-1, 2, 3, 4});  // 0->1->2->3->4
  AddExpectedDelta({3, 2, -1, 1});  // 0->3->1->2->4
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairRelocateOnePairOperatorTest, OnePath3) {
  SetFirstSolution({3, 2, 4, 1});   // 0->3->1->2->4
  AddExpectedDelta({1, -1, 3, 4});  // 0->1->2->3->4
  AddExpectedDelta({1, 3, -1, 2});  // 0->1->3->2->4
  AddExpectedDelta({1, 3, -1, 2});  // 0->1->3->2->4
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairRelocateOnePairOperatorTest, TwoPaths) {
  SetFirstSolution({1, 2, 4, 5});   // 0->1->2->4 3->5
  AddExpectedDelta({4, -1, 5, 1});  // 0->4 3->1->2->5
  BaseSolveAndTest(/*match_all=*/true);
}

PAIR_RELOCATE_OPERATOR_TEST(OneMultiplePair, (std::vector<PickupDeliveryPair>{
                                                 {{1, 3}, {2, 4}}}));

TEST_F(PairRelocateOneMultiplePairOperatorTest, OnePath1) {
  SetFirstSolution({3, 1, 5, 2, 4, 6});     // 0->3->2->5->6
  AddExpectedDelta({1, 5, 6, 3, -1, 2});    // 0->1->5->2->6
  AddExpectedDelta({5, 2, 6, 3, -1, 1});    // 0->5->1->2->6
  AddExpectedDelta({1, 5, 2, 3, 6, 4});     // 0->1->5->4->6
  AddExpectedDelta({5, 4, 2, 3, 6, 1});     // 0->5->1->4->6
  AddExpectedDelta({-1, -1, 6, 5, -1, 2});  // 0->3->5->2->6
  AddExpectedDelta({5, -1, 6, -1, -1, 3});  // 0->5->3->2->6
  AddExpectedDelta({-1, -1, 2, 5, 6, 4});   // 0->3->5->4->6
  AddExpectedDelta({5, -1, 2, 4, 6, 3});    // 0->5->3->4->6
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairRelocateOneMultiplePairOperatorTest, OnePath2) {
  SetFirstSolution({3, 1, 6, 5, 4, 2});     // 0->3->5->2->6
  AddExpectedDelta({1, 2, 5, 3, -1, 6});    // 0->1->2->5->6
  AddExpectedDelta({1, 2, 5, 3, -1, 6});    // 0->1->2->5->6
  AddExpectedDelta({5, 2, -1, 3, -1, 1});   // 0->5->1->2->6
  AddExpectedDelta({1, 4, 2, 3, 5, 6});     // 0->1->4->5->6
  AddExpectedDelta({1, 4, 2, 3, 5, 6});     // 0->1->4->5->6
  AddExpectedDelta({5, 4, 2, 3, 6, 1});     // 0->5->1->4->6
  AddExpectedDelta({-1, -1, 5, 2, -1, 6});  // 0->3->2->5->6
  AddExpectedDelta({-1, -1, 5, 2, -1, 6});  // 0->3->2->5->6
  AddExpectedDelta({5, -1, -1, 2, -1, 3});  // 0->5->3->2->6
  AddExpectedDelta({-1, -1, 2, 4, 5, 6});   // 0->3->4->5->6
  AddExpectedDelta({-1, -1, 2, 4, 5, 6});   // 0->3->4->5->6
  AddExpectedDelta({5, -1, 2, 4, 6, 3});    // 0->5->3->4->6
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairRelocateOneMultiplePairOperatorTest, OnePath3) {
  SetFirstSolution({5, 1, 6, 2, 4, 3});     // 0->5->3->2->6
  AddExpectedDelta({1, 2, 5, 3, -1, 6});    // 0->1->2->5->6
  AddExpectedDelta({1, 5, -1, 3, -1, 2});   // 0->1->5->2->6
  AddExpectedDelta({1, 5, -1, 3, -1, 2});   // 0->1->5->2->6
  AddExpectedDelta({1, 4, 2, 3, 5, 6});     // 0->1->4->5->6
  AddExpectedDelta({1, 5, 2, 3, 6, 4});     // 0->1->5->4->6
  AddExpectedDelta({1, 5, 2, 3, 6, 4});     // 0->1->5->4->6
  AddExpectedDelta({3, -1, 5, -1, -1, 6});  // 0->3->2->5->6
  AddExpectedDelta({3, -1, -1, 5, -1, 2});  // 0->3->5->2->6
  AddExpectedDelta({3, -1, -1, 5, -1, 2});  // 0->3->5->2->6
  AddExpectedDelta({3, -1, 2, 4, 5, 6});    // 0->3->4->5->6
  AddExpectedDelta({3, -1, 2, 5, 6, 4});    // 0->3->5->4->6
  AddExpectedDelta({3, -1, 2, 5, 6, 4});    // 0->3->5->4->6
  BaseSolveAndTest(/*match_all=*/true);
}

#undef PAIR_RELOCATE_OPERATOR_TEST

#define LIGHT_PAIR_RELOCATE_OPERATOR_TEST(name, init_index_pairs, lifo)     \
  class LightPairRelocate##name##OperatorTest : public BaseOperatorTest {   \
   protected:                                                               \
    LocalSearchOperator* BuildOperator(const std::vector<IntVar*>& vars,    \
                                       Solver* solver) const override {     \
      const std::vector<PickupDeliveryPair> index_pairs = init_index_pairs; \
      return MakeLightPairRelocate(solver, vars, {}, nullptr, index_pairs,  \
                                   lifo);                                   \
    }                                                                       \
  };

LIGHT_PAIR_RELOCATE_OPERATOR_TEST(OnePair,
                                  (std::vector<PickupDeliveryPair>{{{1}, {2}}}),
                                  ([](int64_t) { return false; }));

TEST_F(LightPairRelocateOnePairOperatorTest, OnePath1) {
  SetFirstPaths(4, {{0, 1, 2, 3, 4}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(LightPairRelocateOnePairOperatorTest, OnePath2) {
  SetFirstPaths(4, {{0, 1, 3, 2, 4}});
  AddExpectedPaths({{0, 1, 2, 3, 4}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(LightPairRelocateOnePairOperatorTest, TwoPaths) {
  SetFirstPaths(4, {{0, 1, 2, 4}, {3, 5}});
  AddExpectedPaths({{0, 4}, {3, 1, 2, 5}});
  BaseSolveAndTest(/*match_all=*/true);
}

LIGHT_PAIR_RELOCATE_OPERATOR_TEST(OnePairLifo,
                                  (std::vector<PickupDeliveryPair>{{{1}, {2}}}),
                                  ([](int64_t) { return true; }));

TEST_F(LightPairRelocateOnePairLifoOperatorTest, OnePath1) {
  SetFirstPaths(4, {{0, 1, 2, 3, 4}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(LightPairRelocateOnePairLifoOperatorTest, OnePath2) {
  SetFirstPaths(4, {{0, 1, 3, 2, 4}});
  AddExpectedPaths({{0, 1, 2, 3, 4}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(LightPairRelocateOnePairLifoOperatorTest, TwoPaths) {
  SetFirstPaths(4, {{0, 1, 2, 4}, {3, 5}});
  AddExpectedPaths({{0, 4}, {3, 1, 2, 5}});
  BaseSolveAndTest(/*match_all=*/true);
}

LIGHT_PAIR_RELOCATE_OPERATOR_TEST(TwoMultiplePair,
                                  (std::vector<PickupDeliveryPair>{
                                      {{1, 3}, {2, 4}}, {{5}, {6}}}),
                                  ([](int64_t) { return false; }));

TEST_F(LightPairRelocateTwoMultiplePairOperatorTest, OnePath) {
  SetFirstPaths(7, {{0, 3, 2, 5, 6, 7}});
  AddExpectedPaths({{0, 5, 3, 6, 2, 7}});
  AddExpectedPaths({{0, 5, 3, 6, 2, 7}});
  AddExpectedPaths({{0, 3, 5, 2, 6, 7}});
  BaseSolveAndTest(/*match_all=*/true);
}

LIGHT_PAIR_RELOCATE_OPERATOR_TEST(TwoMultiplePairLifo,
                                  (std::vector<PickupDeliveryPair>{
                                      {{1, 3}, {2, 4}}, {{5}, {6}}}),
                                  ([](int64_t) { return true; }));

TEST_F(LightPairRelocateTwoMultiplePairLifoOperatorTest, OnePath) {
  SetFirstPaths(7, {{0, 3, 2, 5, 6, 7}});
  AddExpectedPaths({{0, 5, 3, 2, 6, 7}});
  AddExpectedPaths({{0, 5, 3, 2, 6, 7}});
  AddExpectedPaths({{0, 3, 5, 6, 2, 7}});
  BaseSolveAndTest(/*match_all=*/true);
}

LIGHT_PAIR_RELOCATE_OPERATOR_TEST(TwoPairs,
                                  (std::vector<PickupDeliveryPair>{{{1}, {2}},
                                                                   {{3}, {4}}}),
                                  ([](int64_t) { return false; }));

TEST_F(LightPairRelocateTwoPairsOperatorTest, OnePath1) {
  SetFirstPaths(5, {{0, 1, 2, 3, 4, 5}});
  AddExpectedPaths({{0, 3, 1, 4, 2, 5}});
  AddExpectedPaths({{0, 3, 1, 4, 2, 5}});
  AddExpectedPaths({{0, 1, 3, 2, 4, 5}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(LightPairRelocateTwoPairsOperatorTest, OnePath2) {
  SetFirstPaths(5, {{0, 1, 3, 2, 4, 5}});
  AddExpectedPaths({{0, 3, 1, 4, 2, 5}});
  AddExpectedPaths({{0, 3, 1, 4, 2, 5}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(LightPairRelocateTwoPairsOperatorTest, OnePath3) {
  SetFirstPaths(5, {{0, 1, 3, 4, 2, 5}});
  AddExpectedPaths({{0, 1, 3, 2, 4, 5}});
  AddExpectedPaths({{0, 3, 1, 4, 2, 5}});
  AddExpectedPaths({{0, 3, 1, 4, 2, 5}});
  AddExpectedPaths({{0, 1, 3, 2, 4, 5}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(LightPairRelocateTwoPairsOperatorTest, TwoPaths1) {
  SetFirstPaths(6, {{0, 1, 2, 6}, {5, 3, 4, 7}});
  AddExpectedPaths({{0, 6}, {5, 1, 3, 2, 4, 7}});
  AddExpectedPaths({{0, 6}, {5, 3, 1, 4, 2, 7}});
  AddExpectedPaths({{0, 3, 1, 4, 2, 6}, {5, 7}});
  AddExpectedPaths({{0, 1, 3, 2, 4, 6}, {5, 7}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(LightPairRelocateTwoPairsOperatorTest, TwoPaths2) {
  SetFirstPaths(8, {{0, 1, 6, 2, 8}, {5, 3, 7, 4, 9}});
  AddExpectedPaths({{0, 1, 2, 6, 8}, {5, 3, 7, 4, 9}});
  AddExpectedPaths({{0, 6, 8}, {5, 1, 3, 7, 2, 4, 9}});
  AddExpectedPaths({{0, 6, 8}, {5, 3, 1, 7, 4, 2, 9}});
  AddExpectedPaths({{0, 3, 1, 6, 4, 2, 8}, {5, 7, 9}});
  AddExpectedPaths({{0, 1, 3, 6, 2, 4, 8}, {5, 7, 9}});
  AddExpectedPaths({{0, 1, 6, 2, 8}, {5, 3, 4, 7, 9}});
  BaseSolveAndTest(/*match_all=*/true);
}

LIGHT_PAIR_RELOCATE_OPERATOR_TEST(TwoPairsLifo,
                                  (std::vector<PickupDeliveryPair>{{{1}, {2}},
                                                                   {{3}, {4}}}),
                                  ([](int64_t) { return true; }));

TEST_F(LightPairRelocateTwoPairsLifoOperatorTest, OnePath1) {
  SetFirstPaths(5, {{0, 1, 2, 3, 4, 5}});
  AddExpectedPaths({{0, 3, 1, 2, 4, 5}});
  AddExpectedPaths({{0, 3, 1, 2, 4, 5}});
  AddExpectedPaths({{0, 1, 3, 4, 2, 5}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(LightPairRelocateTwoPairsLifoOperatorTest, OnePath2) {
  SetFirstPaths(5, {{0, 1, 3, 2, 4, 5}});
  AddExpectedPaths({{0, 1, 3, 4, 2, 5}});
  AddExpectedPaths({{0, 3, 1, 2, 4, 5}});
  AddExpectedPaths({{0, 3, 1, 2, 4, 5}});
  AddExpectedPaths({{0, 1, 3, 4, 2, 5}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(LightPairRelocateTwoPairsLifoOperatorTest, OnePath3) {
  SetFirstPaths(5, {{0, 1, 3, 4, 2, 5}});
  AddExpectedPaths({{0, 3, 1, 2, 4, 5}});
  AddExpectedPaths({{0, 3, 1, 2, 4, 5}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(LightPairRelocateTwoPairsLifoOperatorTest, TwoPaths1) {
  SetFirstPaths(6, {{0, 1, 2, 6}, {5, 3, 4, 7}});
  AddExpectedPaths({{0, 6}, {5, 1, 3, 4, 2, 7}});
  AddExpectedPaths({{0, 6}, {5, 3, 1, 2, 4, 7}});
  AddExpectedPaths({{0, 3, 1, 2, 4, 6}, {5, 7}});
  AddExpectedPaths({{0, 1, 3, 4, 2, 6}, {5, 7}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(LightPairRelocateTwoPairsLifoOperatorTest, TwoPaths2) {
  SetFirstPaths(8, {{0, 1, 6, 2, 8}, {5, 3, 7, 4, 9}});
  AddExpectedPaths({{0, 1, 2, 6, 8}, {5, 3, 7, 4, 9}});
  AddExpectedPaths({{0, 6, 8}, {5, 1, 3, 7, 4, 2, 9}});
  AddExpectedPaths({{0, 6, 8}, {5, 3, 1, 7, 2, 4, 9}});
  AddExpectedPaths({{0, 3, 1, 6, 2, 4, 8}, {5, 7, 9}});
  AddExpectedPaths({{0, 1, 3, 6, 4, 2, 8}, {5, 7, 9}});
  AddExpectedPaths({{0, 1, 6, 2, 8}, {5, 3, 4, 7, 9}});
  BaseSolveAndTest(/*match_all=*/true);
}

LIGHT_PAIR_RELOCATE_OPERATOR_TEST(TwoPairsPartialLifo,
                                  (std::vector<PickupDeliveryPair>{{{1}, {2}},
                                                                   {{3}, {4}}}),
                                  ([](int64_t i) { return i == 0; }));

TEST_F(LightPairRelocateTwoPairsPartialLifoOperatorTest, TwoPaths1) {
  SetFirstPaths(6, {{0, 1, 2, 6}, {5, 3, 4, 7}});
  AddExpectedPaths({{0, 6}, {5, 1, 3, 2, 4, 7}});
  AddExpectedPaths({{0, 6}, {5, 3, 1, 4, 2, 7}});
  AddExpectedPaths({{0, 3, 1, 2, 4, 6}, {5, 7}});
  AddExpectedPaths({{0, 1, 3, 4, 2, 6}, {5, 7}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(LightPairRelocateTwoPairsPartialLifoOperatorTest, TwoPaths2) {
  SetFirstPaths(8, {{0, 1, 6, 2, 8}, {5, 3, 7, 4, 9}});
  AddExpectedPaths({{0, 1, 2, 6, 8}, {5, 3, 7, 4, 9}});
  AddExpectedPaths({{0, 6, 8}, {5, 1, 3, 7, 2, 4, 9}});
  AddExpectedPaths({{0, 6, 8}, {5, 3, 1, 7, 4, 2, 9}});
  AddExpectedPaths({{0, 3, 1, 6, 2, 4, 8}, {5, 7, 9}});
  AddExpectedPaths({{0, 1, 3, 6, 4, 2, 8}, {5, 7, 9}});
  AddExpectedPaths({{0, 1, 6, 2, 8}, {5, 3, 4, 7, 9}});
  BaseSolveAndTest(/*match_all=*/true);
}

#undef LIGHT_PAIR_RELOCATE_OPERATOR_TEST

#define LIGHT_PAIR_RELOCATE_WITH_NEIGHBORS_OPERATOR_TEST(name, init_neighbors, \
                                                         init_index_pairs)     \
  class LightPairRelocateWithNeighbors##name##OperatorTest                     \
      : public BaseOperatorTest {                                              \
   protected:                                                                  \
    LocalSearchOperator* BuildOperator(const std::vector<IntVar*>& vars,       \
                                       Solver* solver) const override {        \
      const std::vector<PickupDeliveryPair> index_pairs = init_index_pairs;    \
      const std::vector<int> neighbors = init_neighbors;                       \
      return MakeLightPairRelocate(                                            \
          solver, vars, {}, nullptr, nullptr,                                  \
          [neighbors](int, int) -> const std::vector<int>& {                   \
            return neighbors;                                                  \
          },                                                                   \
          index_pairs);                                                        \
    }                                                                          \
  };

LIGHT_PAIR_RELOCATE_WITH_NEIGHBORS_OPERATOR_TEST(
    OnePair, (std::vector<int>{1}),
    (std::vector<PickupDeliveryPair>{{{1}, {2}}}));

TEST_F(LightPairRelocateWithNeighborsOnePairOperatorTest, OnePath1) {
  SetFirstPaths(4, {{0, 1, 2, 3, 4}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(LightPairRelocateWithNeighborsOnePairOperatorTest, OnePath2) {
  SetFirstPaths(4, {{0, 1, 3, 2, 4}});
  AddExpectedPaths({{0, 1, 2, 3, 4}});
  BaseSolveAndTest(/*match_all=*/true);
}

LIGHT_PAIR_RELOCATE_WITH_NEIGHBORS_OPERATOR_TEST(
    OnePairTwoNeighbors, (std::vector<int>{3, 1}),
    (std::vector<PickupDeliveryPair>{{{1}, {2}}}));

TEST_F(LightPairRelocateWithNeighborsOnePairTwoNeighborsOperatorTest,
       TwoPaths1) {
  SetFirstPaths(6, {{0, 1, 2, 6}, {5, 3, 4, 7}});
  AddExpectedPaths({{0, 6}, {5, 1, 2, 3, 4, 7}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(LightPairRelocateWithNeighborsOnePairTwoNeighborsOperatorTest,
       TwoPaths2) {
  SetFirstPaths(8, {{0, 1, 6, 2, 8}, {5, 3, 7, 4, 9}});
  AddExpectedPaths({{0, 1, 2, 6, 8}, {5, 3, 7, 4, 9}});
  AddExpectedPaths({{0, 6, 8}, {5, 1, 2, 3, 7, 4, 9}});
  BaseSolveAndTest(/*match_all=*/true);
}

LIGHT_PAIR_RELOCATE_WITH_NEIGHBORS_OPERATOR_TEST(
    TwoPairs, (std::vector<int>{3, 4}),
    (std::vector<PickupDeliveryPair>{{{1}, {2}}, {{3}, {4}}}));

TEST_F(LightPairRelocateWithNeighborsTwoPairsOperatorTest, OnePath1) {
  SetFirstPaths(5, {{0, 1, 2, 3, 4, 5}});
  AddExpectedPaths({{0, 3, 1, 4, 2, 5}});
  AddExpectedPaths({{0, 1, 3, 2, 4, 5}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(LightPairRelocateWithNeighborsTwoPairsOperatorTest, OnePath2) {
  SetFirstPaths(5, {{0, 1, 3, 2, 4, 5}});
  AddExpectedPaths({{0, 3, 1, 4, 2, 5}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(LightPairRelocateWithNeighborsTwoPairsOperatorTest, TwoPaths1) {
  SetFirstPaths(6, {{0, 1, 2, 6}, {5, 3, 4, 7}});
  AddExpectedPaths({{0, 3, 1, 4, 2, 6}, {5, 7}});
  AddExpectedPaths({{0, 1, 3, 2, 4, 6}, {5, 7}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(LightPairRelocateWithNeighborsTwoPairsOperatorTest, TwoPaths2) {
  SetFirstPaths(8, {{0, 1, 6, 2, 8}, {5, 3, 7, 4, 9}});
  AddExpectedPaths({{0, 3, 1, 6, 4, 2, 8}, {5, 7, 9}});
  AddExpectedPaths({{0, 1, 3, 6, 2, 4, 8}, {5, 7, 9}});
  AddExpectedPaths({{0, 1, 6, 2, 8}, {5, 3, 4, 7, 9}});
  BaseSolveAndTest(/*match_all=*/true);
}

#undef LIGHT_PAIR_RELOCATE_WITH_NEIGHBORS_OPERATOR_TEST

#define GROUP_PAIR_RELOCATE_OPERATOR_TEST(name, init_index_pairs)              \
  class GroupPairRelocate##name##OperatorTest : public BaseOperatorTest {      \
   protected:                                                                  \
    LocalSearchOperator* BuildOperator(const std::vector<IntVar*>& vars,       \
                                       Solver* solver) const override {        \
      const std::vector<PickupDeliveryPair> index_pairs = init_index_pairs;    \
      return MakeGroupPairAndRelocate(solver, vars, {}, nullptr, index_pairs); \
    }                                                                          \
  };

GROUP_PAIR_RELOCATE_OPERATOR_TEST(OnePair, (std::vector<PickupDeliveryPair>{
                                               {{1}, {2}}}));

TEST_F(GroupPairRelocateOnePairOperatorTest, OnePath1) {
  SetFirstPaths(4, {{0, 1, 2, 3, 4}});
  AddExpectedPaths({{0, 3, 1, 2, 4}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(GroupPairRelocateOnePairOperatorTest, OnePath2) {
  SetFirstPaths(4, {{0, 1, 3, 2, 4}});
  AddExpectedPaths({{0, 1, 2, 3, 4}});
  AddExpectedPaths({{0, 3, 1, 2, 4}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(GroupPairRelocateOnePairOperatorTest, TwoPaths) {
  SetFirstPaths(4, {{0, 1, 2, 4}, {3, 5}});
  AddExpectedPaths({{0, 4}, {3, 1, 2, 5}});
  BaseSolveAndTest(/*match_all=*/true);
}

GROUP_PAIR_RELOCATE_OPERATOR_TEST(TwoMultiplePair,
                                  (std::vector<PickupDeliveryPair>{
                                      {{1, 3}, {2, 4}}, {{5}, {6}}}));

TEST_F(GroupPairRelocateTwoMultiplePairOperatorTest, OnePath) {
  SetFirstPaths(7, {{0, 3, 2, 5, 6, 7}});
  AddExpectedPaths({{0, 5, 3, 2, 6, 7}});
  AddExpectedPaths({{0, 5, 6, 3, 2, 7}});
  AddExpectedPaths({{0, 5, 6, 3, 2, 7}});
  AddExpectedPaths({{0, 3, 5, 6, 2, 7}});
  BaseSolveAndTest(/*match_all=*/true);
}

GROUP_PAIR_RELOCATE_OPERATOR_TEST(TwoPairs, (std::vector<PickupDeliveryPair>{
                                                {{1}, {2}}, {{3}, {4}}}));

TEST_F(GroupPairRelocateTwoPairsOperatorTest, OnePath1) {
  SetFirstPaths(5, {{0, 1, 2, 3, 4, 5}});
  AddExpectedPaths({{0, 3, 1, 2, 4, 5}});
  AddExpectedPaths({{0, 3, 4, 1, 2, 5}});
  AddExpectedPaths({{0, 3, 4, 1, 2, 5}});
  AddExpectedPaths({{0, 1, 3, 4, 2, 5}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(GroupPairRelocateTwoPairsOperatorTest, OnePath2) {
  SetFirstPaths(5, {{0, 1, 3, 2, 4, 5}});
  AddExpectedPaths({{0, 1, 2, 3, 4, 5}});
  AddExpectedPaths({{0, 3, 1, 2, 4, 5}});
  AddExpectedPaths({{0, 3, 4, 1, 2, 5}});
  AddExpectedPaths({{0, 3, 4, 1, 2, 5}});
  AddExpectedPaths({{0, 1, 3, 4, 2, 5}});
  AddExpectedPaths({{0, 1, 2, 3, 4, 5}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(GroupPairRelocateTwoPairsOperatorTest, OnePath3) {
  SetFirstPaths(5, {{0, 1, 3, 4, 2, 5}});
  AddExpectedPaths({{0, 1, 2, 3, 4, 5}});
  AddExpectedPaths({{0, 3, 1, 2, 4, 5}});
  AddExpectedPaths({{0, 3, 4, 1, 2, 5}});
  AddExpectedPaths({{0, 3, 4, 1, 2, 5}});
  AddExpectedPaths({{0, 1, 2, 3, 4, 5}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(GroupPairRelocateTwoPairsOperatorTest, TwoPaths1) {
  SetFirstPaths(6, {{0, 1, 2, 6}, {5, 3, 4, 7}});
  AddExpectedPaths({{0, 6}, {5, 1, 2, 3, 4, 7}});
  AddExpectedPaths({{0, 6}, {5, 3, 1, 2, 4, 7}});
  AddExpectedPaths({{0, 6}, {5, 3, 4, 1, 2, 7}});
  AddExpectedPaths({{0, 3, 4, 1, 2, 6}, {5, 7}});
  AddExpectedPaths({{0, 1, 3, 4, 2, 6}, {5, 7}});
  AddExpectedPaths({{0, 1, 2, 3, 4, 6}, {5, 7}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(GroupPairRelocateTwoPairsOperatorTest, TwoPaths2) {
  SetFirstPaths(8, {{0, 1, 6, 2, 8}, {5, 3, 7, 4, 9}});
  AddExpectedPaths({{0, 1, 2, 6, 8}, {5, 3, 7, 4, 9}});
  AddExpectedPaths({{0, 6, 1, 2, 8}, {5, 3, 7, 4, 9}});
  AddExpectedPaths({{0, 6, 8}, {5, 1, 2, 3, 7, 4, 9}});
  AddExpectedPaths({{0, 6, 8}, {5, 3, 1, 2, 7, 4, 9}});
  AddExpectedPaths({{0, 6, 8}, {5, 3, 7, 1, 2, 4, 9}});
  AddExpectedPaths({{0, 6, 8}, {5, 3, 7, 4, 1, 2, 9}});
  AddExpectedPaths({{0, 3, 4, 1, 6, 2, 8}, {5, 7, 9}});
  AddExpectedPaths({{0, 1, 3, 4, 6, 2, 8}, {5, 7, 9}});
  AddExpectedPaths({{0, 1, 6, 3, 4, 2, 8}, {5, 7, 9}});
  AddExpectedPaths({{0, 1, 6, 2, 3, 4, 8}, {5, 7, 9}});
  AddExpectedPaths({{0, 1, 6, 2, 8}, {5, 3, 4, 7, 9}});
  AddExpectedPaths({{0, 1, 6, 2, 8}, {5, 7, 3, 4, 9}});
  BaseSolveAndTest(/*match_all=*/true);
}

#undef GROUP_PAIR_RELOCATE_OPERATOR_TEST

#define GROUP_PAIR_RELOCATE_WITH_NEIGHBORS_OPERATOR_TEST(name, init_neighbors, \
                                                         init_index_pairs)     \
  class GroupPairRelocateWithNeighbors##name##OperatorTest                     \
      : public BaseOperatorTest {                                              \
   protected:                                                                  \
    LocalSearchOperator* BuildOperator(const std::vector<IntVar*>& vars,       \
                                       Solver* solver) const override {        \
      const std::vector<int> neighbors = init_neighbors;                       \
      const std::vector<PickupDeliveryPair> index_pairs = init_index_pairs;    \
      return MakeGroupPairAndRelocate(                                         \
          solver, vars, {}, nullptr, nullptr,                                  \
          [neighbors](int, int) -> const std::vector<int>& {                   \
            return neighbors;                                                  \
          },                                                                   \
          index_pairs);                                                        \
    }                                                                          \
  };

GROUP_PAIR_RELOCATE_WITH_NEIGHBORS_OPERATOR_TEST(
    OnePair, (std::vector<int>{1}),
    (std::vector<PickupDeliveryPair>{{{1}, {2}}}));

TEST_F(GroupPairRelocateWithNeighborsOnePairOperatorTest, OnePath1) {
  SetFirstPaths(4, {{0, 1, 2, 3, 4}});
  AddExpectedPaths({{0, 3, 1, 2, 4}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(GroupPairRelocateWithNeighborsOnePairOperatorTest, OnePath2) {
  SetFirstPaths(4, {{0, 1, 3, 2, 4}});
  AddExpectedPaths({{0, 1, 2, 3, 4}});
  AddExpectedPaths({{0, 3, 1, 2, 4}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(GroupPairRelocateWithNeighborsOnePairOperatorTest, TwoPaths) {
  SetFirstPaths(4, {{0, 1, 2, 4}, {3, 5}});
  AddExpectedPaths({{0, 4}, {3, 1, 2, 5}});
  BaseSolveAndTest(/*match_all=*/true);
}

GROUP_PAIR_RELOCATE_WITH_NEIGHBORS_OPERATOR_TEST(
    TwoMultiplePair, (std::vector<int>{1}),
    (std::vector<PickupDeliveryPair>{{{1, 3}, {2, 4}}, {{5}, {6}}}));

TEST_F(GroupPairRelocateWithNeighborsTwoMultiplePairOperatorTest, OnePath1) {
  SetFirstPaths(7, {{0, 3, 2, 5, 6, 7}});
  // TODO(user): Make the operator support alternative neighbors.
  // That way the following would be found:
  // AddExpectedPaths({{0, 5, 1, 2, 6, 7}});
  // AddExpectedPaths({{0, 5, 6, 1, 2, 7}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(GroupPairRelocateWithNeighborsTwoMultiplePairOperatorTest, OnePath2) {
  SetFirstPaths(7, {{0, 1, 2, 5, 6, 7}});
  AddExpectedPaths({{0, 5, 1, 2, 6, 7}});
  AddExpectedPaths({{0, 5, 6, 1, 2, 7}});
  BaseSolveAndTest(/*match_all=*/true);
}

GROUP_PAIR_RELOCATE_WITH_NEIGHBORS_OPERATOR_TEST(
    TwoPairs, (std::vector<int>{1}),
    (std::vector<PickupDeliveryPair>{{{1}, {2}}, {{3}, {4}}}));

TEST_F(GroupPairRelocateWithNeighborsTwoPairsOperatorTest, OnePath1) {
  SetFirstPaths(5, {{0, 1, 2, 3, 4, 5}});
  AddExpectedPaths({{0, 3, 1, 2, 4, 5}});
  AddExpectedPaths({{0, 3, 4, 1, 2, 5}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(GroupPairRelocateWithNeighborsTwoPairsOperatorTest, OnePath2) {
  SetFirstPaths(5, {{0, 1, 3, 2, 4, 5}});
  AddExpectedPaths({{0, 1, 2, 3, 4, 5}});
  AddExpectedPaths({{0, 3, 1, 2, 4, 5}});
  AddExpectedPaths({{0, 3, 4, 1, 2, 5}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(GroupPairRelocateWithNeighborsTwoPairsOperatorTest, OnePath3) {
  SetFirstPaths(5, {{0, 1, 3, 4, 2, 5}});
  AddExpectedPaths({{0, 1, 2, 3, 4, 5}});
  AddExpectedPaths({{0, 3, 1, 2, 4, 5}});
  AddExpectedPaths({{0, 3, 4, 1, 2, 5}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(GroupPairRelocateWithNeighborsTwoPairsOperatorTest, TwoPaths1) {
  SetFirstPaths(6, {{0, 1, 2, 6}, {5, 3, 4, 7}});
  AddExpectedPaths({{0, 6}, {5, 1, 2, 3, 4, 7}});
  AddExpectedPaths({{0, 6}, {5, 3, 1, 2, 4, 7}});
  AddExpectedPaths({{0, 6}, {5, 3, 4, 1, 2, 7}});
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(GroupPairRelocateWithNeighborsTwoPairsOperatorTest, TwoPaths2) {
  SetFirstPaths(8, {{0, 1, 6, 2, 8}, {5, 3, 7, 4, 9}});
  AddExpectedPaths({{0, 1, 2, 6, 8}, {5, 3, 7, 4, 9}});
  AddExpectedPaths({{0, 6, 1, 2, 8}, {5, 3, 7, 4, 9}});
  AddExpectedPaths({{0, 6, 8}, {5, 1, 2, 3, 7, 4, 9}});
  AddExpectedPaths({{0, 6, 8}, {5, 3, 1, 2, 7, 4, 9}});
  AddExpectedPaths({{0, 6, 8}, {5, 3, 7, 1, 2, 4, 9}});
  AddExpectedPaths({{0, 6, 8}, {5, 3, 7, 4, 1, 2, 9}});
  BaseSolveAndTest(/*match_all=*/true);
}

#undef GROUP_PAIR_RELOCATE_WITH_NEIGHBORS_OPERATOR_TEST

#define PAIR_EXCHANGE_OPERATOR_TEST(name, init_index_pairs)                 \
  class PairExchange##name##OperatorTest : public BaseOperatorTest {        \
   protected:                                                               \
    LocalSearchOperator* BuildOperator(const std::vector<IntVar*>& vars,    \
                                       Solver* solver) const override {     \
      const std::vector<PickupDeliveryPair> index_pairs = init_index_pairs; \
      return MakePairExchange(solver, vars, {}, nullptr, index_pairs);      \
    }                                                                       \
  };

PAIR_EXCHANGE_OPERATOR_TEST(NoPairs, std::vector<PickupDeliveryPair>{});

TEST_F(PairExchangeNoPairsOperatorTest, OnePath) {
  SetFirstSolution({1, 2, 3, 4, 5, 6});  // 0->1->2->3->4->5->6
  BaseSolveAndTest(/*match_all=*/true);
}

PAIR_EXCHANGE_OPERATOR_TEST(OnePair,
                            (std::vector<PickupDeliveryPair>{{{1}, {2}}}));

TEST_F(PairExchangeOnePairOperatorTest, OnePath) {
  SetFirstSolution({1, 2, 3, 4, 5, 6});  // 0->1->2->3->4->5->6
  BaseSolveAndTest(/*match_all=*/true);
}

PAIR_EXCHANGE_OPERATOR_TEST(TwoMultiplePairs,
                            (std::vector<PickupDeliveryPair>{{{1, 3}, {2, 4}},
                                                             {{5}, {6}}}));

TEST_F(PairExchangeTwoMultiplePairsOperatorTest, OnePath) {
  SetFirstSolution({3, 1, 5, 2, 4, 6, 7});      // 0->3->2->5->6->7
  AddExpectedDelta({5, 2, 7, 3, -1, -1, 1});    // 0->5->6->1->2->7
  AddExpectedDelta({5, 4, 2, 3, 7, -1, 1});     // 0->5->6->1->4->7
  AddExpectedDelta({5, -1, 7, -1, -1, -1, 3});  // 0->5->6->3->2->7
  AddExpectedDelta({5, -1, 2, 4, 7, -1, 3});    // 0->5->6->3->4->7
  AddExpectedDelta({5, 2, 7, 3, -1, -1, 1});    // 0->5->6->1->2->7
  AddExpectedDelta({5, 4, 2, 3, 7, -1, 1});     // 0->5->6->1->4->7
  AddExpectedDelta({5, -1, 7, -1, -1, -1, 3});  // 0->5->6->3->2->7
  AddExpectedDelta({5, -1, 2, 4, 7, -1, 3});    // 0->5->6->3->4->7
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeTwoMultiplePairsOperatorTest, TwoPaths) {
  SetFirstSolution({3, 1, 8, 2, 4, 6, 9, 5});      // 0->3->2->8  7->5->6->9
  AddExpectedDelta({5, 2, 9, 3, -1, -1, 8, 1});    // 0->5->6->8  7->1->2->9
  AddExpectedDelta({5, 4, 2, 3, 9, -1, 8, 1});     // 0->5->6->8  7->1->4->9
  AddExpectedDelta({5, -1, 9, -1, -1, -1, 8, 3});  // 0->5->6->8  7->3->2->9
  AddExpectedDelta({5, -1, 2, 4, 9, -1, 8, 3});    // 0->5->6->8  7->3->4->9
  AddExpectedDelta({5, 2, 9, 3, -1, -1, 8, 1});    // 0->5->6->8  7->1->2->9
  AddExpectedDelta({5, 4, 2, 3, 9, -1, 8, 1});     // 0->5->6->8  7->1->4->9
  AddExpectedDelta({5, -1, 9, -1, -1, -1, 8, 3});  // 0->5->6->8  7->3->2->9
  AddExpectedDelta({5, -1, 2, 4, 9, -1, 8, 3});    // 0->5->6->8  7->3->4->9
  BaseSolveAndTest(/*match_all=*/true);
}

PAIR_EXCHANGE_OPERATOR_TEST(TwoPairs, (std::vector<PickupDeliveryPair>{
                                          {{1}, {2}}, {{3}, {4}}}));

TEST_F(PairExchangeTwoPairsOperatorTest, OnePath1) {
  SetFirstSolution({1, 2, 3, 4, 5, 6});     // 0->1->2->3->4->5->6
  AddExpectedDelta({3, -1, 5, -1, 1, -1});  // 0->3->4->1->2->5->6
  AddExpectedDelta({3, -1, 5, -1, 1, -1});  // 0->3->4->1->2->5->6
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeTwoPairsOperatorTest, OnePath2) {
  SetFirstSolution({1, 2, 5, 4, 6, 3});    // 0->1->2->5->3->4->6
  AddExpectedDelta({3, -1, 6, -1, 5, 1});  // 0->3->4->5->1->2->6
  AddExpectedDelta({3, -1, 6, -1, 5, 1});  // 0->3->4->5->1->2->6
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeTwoPairsOperatorTest, OnePath3) {
  SetFirstSolution({1, 5, 3, 4, 6, 2});  // 0->1->5->2->3->4->6
  AddExpectedDelta({3, 2, 6, 5, 1, 4});  // 0->3->5->4->1->2->6
  AddExpectedDelta({3, 2, 6, 5, 1, 4});  // 0->3->5->4->1->2->6
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeTwoPairsOperatorTest, OnePath4) {
  SetFirstSolution({1, 2, 3, 5, 6, 4});  // 0->1->2->3->5->4->6
  AddExpectedDelta({3, 5, 6, 4, 1, 2});  // 0->3->4->1->5->2->6
  AddExpectedDelta({3, 5, 6, 4, 1, 2});  // 0->3->4->1->5->2->6
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeTwoPairsOperatorTest, OnePath5) {
  SetFirstSolution({1, 5, 3, 6, 7, 2, 4});  // 0->1->5->2->3->6->4->7
  AddExpectedDelta({3, 6, 7, 5, 1, 4, 2});  // 0->3->5->4->1->6->2->7
  AddExpectedDelta({3, 6, 7, 5, 1, 4, 2});  // 0->3->5->4->1->6->2->7
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeTwoPairsOperatorTest, OnePath6) {
  SetFirstSolution({1, 5, 6, 7, 8, 2, 3, 4});  // 0->1->5->2->6->3->7->4->8
  AddExpectedDelta({3, 7, 8, 5, 6, 4, 1, 2});  // 0->3->5->4->6->1->7->2->8
  AddExpectedDelta({3, 7, 8, 5, 6, 4, 1, 2});  // 0->3->5->4->6->1->7->2->8
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeTwoPairsOperatorTest, OnePathInterleaved1) {
  SetFirstSolution({1, 3, 4, 2, 5, 6});   // 0->1->3->2->4->5->6
  AddExpectedDelta({3, 4, 5, 1, 2, -1});  // 0->3->1->4->2->5->6
  AddExpectedDelta({3, 4, 5, 1, 2, -1});  // 0->3->1->4->2->5->6
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeTwoPairsOperatorTest, OnePathInterleaved2) {
  SetFirstSolution({1, 3, 5, 4, 2, 6});   // 0->1->3->4->2->5->6
  AddExpectedDelta({3, 2, 4, 1, 5, -1});  // 0->3->1->2->4->5->6
  AddExpectedDelta({3, 2, 4, 1, 5, -1});  // 0->3->1->2->4->5->6
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeTwoPairsOperatorTest, TwoPaths1) {
  SetFirstSolution({1, 2, 6, 4, 7, 3});    // 0->1->2->6  5->3->4->7
  AddExpectedDelta({3, -1, 7, -1, 6, 1});  // 0->3->4->6  5->1->2->7
  AddExpectedDelta({3, -1, 7, -1, 6, 1});  // 0->3->4->6  5->1->2->7
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeTwoPairsOperatorTest, TwoPaths2) {
  SetFirstSolution({1, 6, 8, 7, 9, 3, 2, 4});  // 0->1->6->2->8  5->3->7->4->9
  AddExpectedDelta({3, 7, 9, 6, 8, 1, 4, 2});  // 0->3->6->4->8  5->1->7->2->9
  AddExpectedDelta({3, 7, 9, 6, 8, 1, 4, 2});  // 0->3->6->4->8  5->1->7->2->9
  BaseSolveAndTest(/*match_all=*/true);
}

#undef PAIR_EXCHANGE_OPERATOR_TEST

#define PAIR_EXCHANGE_WITH_NEIGHBORS_OPERATOR_TEST(                         \
    name, init_in_neighbors, init_out_neighbors, init_index_pairs)          \
  class PairExchangeWithNeighbors##name##OperatorTest                       \
      : public BaseOperatorTest {                                           \
   protected:                                                               \
    LocalSearchOperator* BuildOperator(const std::vector<IntVar*>& vars,    \
                                       Solver* solver) const override {     \
      const std::vector<int> in_neighbors = init_in_neighbors;              \
      const std::vector<int> out_neighbors = init_out_neighbors;            \
      const std::vector<PickupDeliveryPair> index_pairs = init_index_pairs; \
      return MakePairExchange(                                              \
          solver, vars, {}, nullptr,                                        \
          [in_neighbors](int, int) -> const std::vector<int>& {             \
            return in_neighbors;                                            \
          },                                                                \
          [out_neighbors](int, int) -> const std::vector<int>& {            \
            return out_neighbors;                                           \
          },                                                                \
          index_pairs);                                                     \
    }                                                                       \
  };

PAIR_EXCHANGE_WITH_NEIGHBORS_OPERATOR_TEST(NoPairs, std::vector<int>{1},
                                           std::vector<int>{2},
                                           std::vector<PickupDeliveryPair>{});

TEST_F(PairExchangeWithNeighborsNoPairsOperatorTest, OnePath) {
  SetFirstSolution({1, 2, 3, 4, 5, 6});  // 0->1->2->3->4->5->6
  BaseSolveAndTest(/*match_all=*/true);
}

PAIR_EXCHANGE_WITH_NEIGHBORS_OPERATOR_TEST(OnePair, std::vector<int>{1},
                                           std::vector<int>{1},
                                           (std::vector<PickupDeliveryPair>{
                                               {{1}, {2}}}));

TEST_F(PairExchangeWithNeighborsOnePairOperatorTest, OnePath) {
  SetFirstSolution({1, 2, 3, 4, 5, 6});  // 0->1->2->3->4->5->6
  BaseSolveAndTest(/*match_all=*/true);
}

PAIR_EXCHANGE_WITH_NEIGHBORS_OPERATOR_TEST(
    TwoMultiplePairsOutgoingNeighbors, std::vector<int>{1}, std::vector<int>{2},
    (std::vector<PickupDeliveryPair>{{{1, 3}, {2, 4}}, {{5}, {6}}}));

TEST_F(PairExchangeWithNeighborsTwoMultiplePairsOutgoingNeighborsOperatorTest,
       OnePath) {
  SetFirstSolution({3, 1, 5, 2, 4, 6, 7});      // 0->3->2->5->6->7
  AddExpectedDelta({5, -1, 7, -1, -1, -1, 3});  // 0->5->6->3->2->7
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeWithNeighborsTwoMultiplePairsOutgoingNeighborsOperatorTest,
       TwoPaths) {
  SetFirstSolution({3, 1, 8, 2, 4, 6, 9, 5});      // 0->3->2->8  7->5->6->9
  AddExpectedDelta({5, -1, 9, -1, -1, -1, 8, 3});  // 0->5->6->8  7->3->2->9
  BaseSolveAndTest(/*match_all=*/true);
}

PAIR_EXCHANGE_WITH_NEIGHBORS_OPERATOR_TEST(
    TwoMultiplePairsIncomingNeighbors, std::vector<int>{6}, std::vector<int>{1},
    (std::vector<PickupDeliveryPair>{{{1, 3}, {2, 4}}, {{5}, {6}}}));

TEST_F(PairExchangeWithNeighborsTwoMultiplePairsIncomingNeighborsOperatorTest,
       OnePath) {
  SetFirstSolution({3, 1, 5, 2, 4, 6, 7});  // 0->3->2->5->6->7
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeWithNeighborsTwoMultiplePairsIncomingNeighborsOperatorTest,
       TwoPaths) {
  SetFirstSolution({3, 1, 8, 2, 4, 6, 9, 5});  // 0->3->2->8  7->5->6->9
  BaseSolveAndTest(/*match_all=*/true);
}

PAIR_EXCHANGE_WITH_NEIGHBORS_OPERATOR_TEST(TwoPairs, std::vector<int>{5},
                                           std::vector<int>{5},
                                           (std::vector<PickupDeliveryPair>{
                                               {{1}, {2}}, {{3}, {4}}}));

TEST_F(PairExchangeWithNeighborsTwoPairsOperatorTest, OnePath1) {
  SetFirstSolution({1, 2, 3, 4, 5, 6});  // 0->1->2->3->4->5->6
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeWithNeighborsTwoPairsOperatorTest, OnePath2) {
  SetFirstSolution({1, 2, 5, 4, 6, 3});    // 0->1->2->5->3->4->6
  AddExpectedDelta({3, -1, 6, -1, 5, 1});  // 0->3->4->5->1->2->6
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeWithNeighborsTwoPairsOperatorTest, OnePath3) {
  SetFirstSolution({1, 5, 3, 4, 6, 2});  // 0->1->5->2->3->4->6
  AddExpectedDelta({3, 2, 6, 5, 1, 4});  // 0->3->5->4->1->2->6
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeWithNeighborsTwoPairsOperatorTest, OnePath4) {
  SetFirstSolution({1, 2, 3, 5, 6, 4});  // 0->1->2->3->5->4->6
  AddExpectedDelta({3, 5, 6, 4, 1, 2});  // 0->3->4->1->5->2->6
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeWithNeighborsTwoPairsOperatorTest, OnePath5) {
  SetFirstSolution({5, 2, 3, 4, 6, 1});     // 0->5->1->2->3->4->6
  AddExpectedDelta({-1, -1, 6, -1, 1, 3});  // 0->5->3->4->1->2->6
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeWithNeighborsTwoPairsOperatorTest, OnePath6) {
  SetFirstSolution({1, 5, 3, 6, 7, 2, 4});  // 0->1->5->2->3->6->4->7
  AddExpectedDelta({3, 6, 7, 5, 1, 4, 2});  // 0->3->5->4->1->6->2->7
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeWithNeighborsTwoPairsOperatorTest, OnePath7) {
  SetFirstSolution({1, 5, 6, 7, 8, 2, 3, 4});  // 0->1->5->2->6->3->7->4->8
  AddExpectedDelta({3, 7, 8, 5, 6, 4, 1, 2});  // 0->3->5->4->6->1->7->2->8
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeWithNeighborsTwoPairsOperatorTest, OnePathInterleaved1) {
  SetFirstSolution({1, 3, 4, 2, 5, 6});  // 0->1->3->2->4->5->6
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeWithNeighborsTwoPairsOperatorTest, OnePathInterleaved2) {
  SetFirstSolution({1, 3, 5, 4, 2, 6});  // 0->1->3->4->2->5->6
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeWithNeighborsTwoPairsOperatorTest, OnePathInterleaved3) {
  SetFirstSolution({1, 5, 4, 2, 6, 3});  // 0->1->5->3->2->4->6
  AddExpectedDelta({3, 4, 6, 5, 2, 1});  // 0->3->5->1->4->2->6 - incoming
  AddExpectedDelta({3, 4, 6, 5, 2, 1});  // 0->3->5->1->4->2->6 - outgoing
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeWithNeighborsTwoPairsOperatorTest, OnePathInterleaved4) {
  SetFirstSolution({5, 3, 4, 2, 6, 1});   // 0->5->1->3->2->4->6
  AddExpectedDelta({-1, 4, 6, 1, 2, 3});  // 0->5->3->1->4->2->6
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeWithNeighborsTwoPairsOperatorTest, TwoPaths1) {
  SetFirstSolution({1, 2, 6, 4, 7, 3});    // 0->1->2->6  5->3->4->7
  AddExpectedDelta({3, -1, 7, -1, 6, 1});  // 0->3->4->6  5->1->2->7
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(PairExchangeWithNeighborsTwoPairsOperatorTest, TwoPaths2) {
  SetFirstSolution({1, 6, 8, 7, 9, 3, 2, 4});  // 0->1->6->2->8  5->3->7->4->9
  AddExpectedDelta({3, 7, 9, 6, 8, 1, 4, 2});  // 0->3->6->4->8  5->1->7->2->9
  BaseSolveAndTest(/*match_all=*/true);
}

#undef PAIR_EXCHANGE_WITH_NEIGHBORS_OPERATOR_TEST

#define PAIR_EXCHANGE_RELOCATE_OPERATOR_TEST(name, init_index_pairs)           \
  class PairExchangeRelocate##name##OperatorTest : public BaseOperatorTest {   \
   protected:                                                                  \
    LocalSearchOperator* BuildOperator(const std::vector<IntVar*>& vars,       \
                                       Solver* solver) const override {        \
      const std::vector<PickupDeliveryPair> index_pairs = init_index_pairs;    \
      return MakePairExchangeRelocate(solver, vars, {}, nullptr, index_pairs); \
    }                                                                          \
  };

PAIR_EXCHANGE_RELOCATE_OPERATOR_TEST(TwoPairs, (std::vector<PickupDeliveryPair>{
                                                   {{1}, {2}}, {{3}, {4}}}));

TEST_F(PairExchangeRelocateTwoPairsOperatorTest, TwoPaths) {
  SetFirstSolution({1, 2, 5, 4, 7, 8, 3, 9});  // 0->1->2->5->8 6->3->4->7->9
  AddExpectedDelta(
      {3, -1, 7, -1, 5, -1, 1, -1});              // 0->3->4->5->8 6->1->2->7->9
  AddExpectedDelta({3, -1, 7, 5, 8, 4, 1, -1});   // 0->3->5->4->8 6->1->2->7->9
  AddExpectedDelta({5, -1, 7, -1, 8, 3, 1, -1});  // 0->5->3->4->8 6->1->2->7->9
  AddExpectedDelta({3, 7, 9, -1, 5, -1, 1, 2});   // 0->3->4->5->8 6->1->7->2->9
  AddExpectedDelta({3, 7, 9, 5, 8, 4, 1, 2});     // 0->3->5->4->8 6->1->7->2->9
  AddExpectedDelta({5, 7, 9, -1, 8, 3, 1, 2});    // 0->5->3->4->8 6->1->7->2->9
  AddExpectedDelta({3, -1, 9, -1, 5, -1, 7, 1});  // 0->3->4->5->8 6->7->1->2->9
  AddExpectedDelta({3, -1, 9, 5, 8, 4, 7, 1});    // 0->3->5->4->8 6->7->1->2->9
  AddExpectedDelta({5, -1, 9, -1, 8, 3, 7, 1});   // 0->5->3->4->8 6->7->1->2->9
  BaseSolveAndTest(/*match_all=*/true);
}

PAIR_EXCHANGE_RELOCATE_OPERATOR_TEST(TwoMultiplePairs,
                                     (std::vector<PickupDeliveryPair>{
                                         {{1, 2}, {3, 4}}, {{5}, {6}}}));

TEST_F(PairExchangeRelocateTwoMultiplePairsOperatorTest, TwoPaths) {
  SetFirstSolution(
      {2, 1, 3, 7, 4, 6, 8, 10, 11, 5});  // 0->2->3->7->10 9->5->6->8->11
  AddExpectedDelta(
      {5, -1, -1, 8, -1, -1, 7, -1, -1, 2});  // 0->5->6->7->10 9->2->3->8->11
  AddExpectedDelta(
      {5, -1, -1, 8, -1, 7, 10, 6, -1, 2});  // 0->5->7->6->10 9->2->3->8->11
  AddExpectedDelta(
      {7, -1, -1, 8, -1, -1, 10, 5, -1, 2});  // 0->7->5->6->10 9->2->3->8->11
  AddExpectedDelta(
      {5, -1, 8, 11, -1, -1, 7, -1, 3, 2});  // 0->5->6->7->10 9->2->8->3->11
  AddExpectedDelta(
      {5, -1, 8, 11, -1, 7, 10, 6, 3, 2});  // 0->5->7->6->10 9->2->8->3->11
  AddExpectedDelta(
      {7, -1, 8, 11, -4, -1, 10, 5, 3, 2});  // 0->7->5->6->10 9->2->8->3->11
  AddExpectedDelta(
      {5, -1, -1, 11, -4, -1, 7, -1, 2, 8});  // 0->5->6->7->10 9->8->2->3->11
  AddExpectedDelta(
      {5, -1, -1, 11, -1, 7, 10, 6, 2, 8});  // 0->5->7->6->10 9->8->2->3->11
  AddExpectedDelta(
      {7, -1, -1, 11, -4, -1, 10, 5, 2, 8});  // 0->7->5->6->10 9->8->2->3->11
  BaseSolveAndTest(/*match_all=*/true);
}

#undef PAIR_EXCHANGE_RELOCATE_OPERATOR_TEST

#define INDEX_PAIR_SWAP_ACTIVE_OPERATOR_TEST(name, init_index_pairs)          \
  class IndexPairSwapActive##name##OperatorTest : public BaseOperatorTest {   \
   protected:                                                                 \
    LocalSearchOperator* BuildOperator(const std::vector<IntVar*>& vars,      \
                                       Solver* solver) const override {       \
      const std::vector<PickupDeliveryPair> index_pairs = init_index_pairs;   \
      return MakeIndexPairSwapActive(solver, vars, {}, nullptr, index_pairs); \
    }                                                                         \
  };

INDEX_PAIR_SWAP_ACTIVE_OPERATOR_TEST(NoPairs,
                                     (std::vector<PickupDeliveryPair>{}));

TEST_F(IndexPairSwapActiveNoPairsOperatorTest, OnePath) {
  SetFirstSolution({1, 2});  // 0->1->2
  BaseSolveAndTest(/*match_all=*/true);
}

INDEX_PAIR_SWAP_ACTIVE_OPERATOR_TEST(OnePair, (std::vector<PickupDeliveryPair>{
                                                  {{1}, {2}}}));

TEST_F(IndexPairSwapActiveOnePairOperatorTest, NoUnperformedSingleton) {
  SetFirstSolution({1, 2, 3, 4});  // 0->1->2->3->4
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(IndexPairSwapActiveOnePairOperatorTest, NoPairPerformed) {
  SetFirstSolution({4, 1, 2, 3});  // 0->4 & 1,2,3 unperformed
  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(IndexPairSwapActiveOnePairOperatorTest, OnePairPerformed) {
  SetFirstSolution({1, 2, 4, 3});  // 0->1->2->4 & 3 unperformed
  AddExpectedDelta({3, 1, 2, 4});  // 0->3->4 & 1,2 unperformed
  BaseSolveAndTest(/*match_all=*/true);
}

INDEX_PAIR_SWAP_ACTIVE_OPERATOR_TEST(
    OneMultiplePair, (std::vector<PickupDeliveryPair>{{{1, 3}, {2, 4}}}));

TEST_F(IndexPairSwapActiveOneMultiplePairOperatorTest, OnePairPerformed) {
  SetFirstSolution({3, 1, 6, 2, 4, 5});    // 0->3->2->6 & 1,4,5 unperformed
  AddExpectedDelta({1, 6, 2, 3, -1, -1});  // 0->1->6 & 2,3,4,5 unperformed
  AddExpectedDelta({4, -1, 2, 3, 6, -1});  // 0->4->6 & 1,2,3,5 unperformed
  AddExpectedDelta({5, -1, 2, 3, -1, 6});  // 0->5->6 & 1,2,3,4 unperformed
  BaseSolveAndTest(/*match_all=*/true);
}

#undef INDEX_PAIR_SWAP_ACTIVE_OPERATOR_TEST

// Arc i-->j costs 10*max(i, j), except for those in  expensive_arcs_, which
// cost 1000.
class RelocateExpensiveChainOperatorTest : public BaseOperatorTest {
 protected:
  LocalSearchOperator* BuildOperator(const std::vector<IntVar*>& vars,
                                     Solver* solver) const override {
    return MakeRelocateExpensiveChain(
        solver, vars, {}, nullptr,
        expensive_arcs_.size() < 2 ? 2 : expensive_arcs_.size(),
        [this](int64_t before, int64_t after, int64_t /*start*/) -> int64_t {
          if (before == after) return 0;
          const std::pair<int, int> arc = {before, after};
          for (const auto& expensive_arc : expensive_arcs_) {
            if (arc == expensive_arc) {
              return 1000;
            }
          }
          return 10 * std::max(before, after);
        });
  }

  void SetExpensiveArcs(std::vector<std::pair<int, int>> arcs) {
    expensive_arcs_.swap(arcs);
  }

 private:
  std::vector<std::pair<int, int>> expensive_arcs_;
};

// Arcs 2-->3 and 4-->5 are more expensive than other arcs.
TEST_F(RelocateExpensiveChainOperatorTest, TwoPaths) {
  SetExpensiveArcs({{2, 3}, {4, 5}});

  // First solution: 0 -> 7,   1 -> 2 ---> 3 -> 4 ---> 5 -> 6
  SetFirstSolution({7, 2, 3, 4, 5, 6});

  // 0 -> 3 -> 4 -> 7,   1 -> 2 -> 5 -> 6
  AddExpectedDelta({3, -1, 5, -1, 7, -1});
  // 0 -> 7,   1 -> 3 -> 4 -> 2 -> 5 -> 6
  AddExpectedDelta({-1, 3, 5, -1, 2, -1});
  // 0 -> 7,   1 -> 2 -> 5 -> 3 -> 4 -> 6
  AddExpectedDelta({-1, -1, 5, -1, 6, 3});
  BaseSolveAndTest(/*match_all=*/true);
}

// Same costs as above, but the initial solution is two empty paths, so there
// are no "expensive chains" to relocate, therefore no deltas possible.
TEST_F(RelocateExpensiveChainOperatorTest, TwoEmptyPaths) {
  SetExpensiveArcs({{2, 3}, {4, 5}});
  SetFirstSolution({7, 6, 2, 3, 4, 5});
  BaseSolveAndTest(/*match_all=*/true);
}

// Single empty path, so no deltas possible.
TEST_F(RelocateExpensiveChainOperatorTest, SingleEmptyPath) {
  SetExpensiveArcs({{2, 3}, {4, 5}});
  SetFirstSolution({7, 1, 2, 3, 4, 5});
  BaseSolveAndTest(/*match_all=*/true);
}

// Example where we take the 3 most expensive arcs into account.
TEST_F(RelocateExpensiveChainOperatorTest, FourExpensiveArcs) {
  SetExpensiveArcs({{1, 2}, {2, 3}, {4, 5}});

  // First solution: 0 -> 7,   1 ---> 2 ---> 3 -> 4 ---> 5 -> 6
  SetFirstSolution({7, 2, 3, 4, 5, 6});

  // Removing 1->2 and 2->3, i.e. relocating node 2
  //  - after node 0: 0 -> 2 -> 7,   1 -> 3 -> 4 -> 5 -> 6
  AddExpectedDelta({2, 3, 7, -1, -1, -1});
  //  - after node 3: 0 -> 7,   1 -> 3 -> 2 -> 4 -> 5 -> 6
  AddExpectedDelta({-1, 3, 4, 2, -1, -1});
  //  - after node 4: 0 -> 7,   1 -> 3 -> 4 -> 2 -> 5 -> 6
  AddExpectedDelta({-1, 3, 5, -1, 2, -1});
  //  - after node 5: 0 -> 7,   1 -> 3 -> 4 -> 5 -> 2 -> 6
  AddExpectedDelta({-1, 3, 6, -1, -1, 2});

  // Removing 1->2 and 4->5, i.e. relocating chain 2->3->4
  //  - after node 0: 0 -> 2 -> 3 -> 4 -> 7,   1 -> 5 -> 6
  AddExpectedDelta({2, 5, -1, -1, 7, -1});
  //  - after node 5: 0 -> 7,   1 -> 5 -> 2 -> 3 -> 4 -> 6
  AddExpectedDelta({-1, 5, -1, -1, 6, 2});

  // Removing 2->3 and 4->5, i.e. relocating chain 3->4
  //  - after node 0: 0 -> 3 -> 4 -> 7,   1 -> 2 -> 5 -> 6
  AddExpectedDelta({3, -1, 5, -1, 7, -1});
  //  - after node 1: 0 -> 7,   1 -> 3 -> 4 -> 2 -> 5 -> 6
  AddExpectedDelta({-1, 3, 5, -1, 2, -1});
  //  - after node 5: 0 -> 7,   1 -> 2 -> 5 -> 3 -> 4 -> 6
  AddExpectedDelta({-1, -1, 5, -1, 6, 3});

  BaseSolveAndTest(/*match_all=*/true);
}

#define RELOCATE_SUBTRIP_OPERATOR_TEST(name, init_index_pairs)              \
  class RelocateSubtrip##name##OperatorTest : public BaseOperatorTest {     \
   protected:                                                               \
    LocalSearchOperator* BuildOperatorWithSecondaryVars(                    \
        const std::vector<IntVar*>& vars,                                   \
        const std::vector<IntVar*>& secondary_vars,                         \
        Solver* solver) const override {                                    \
      const std::vector<PickupDeliveryPair> index_pairs = init_index_pairs; \
      return MakeRelocateSubtrip(solver, vars, secondary_vars, nullptr,     \
                                 index_pairs);                              \
    }                                                                       \
  };

RELOCATE_SUBTRIP_OPERATOR_TEST(OneFullOneEmpty,
                               (std::vector<PickupDeliveryPair>{{{1}, {2}},
                                                                {{3}, {4}}}));

// Simple test: the problem has two pickup/delivery pairs, and the initial set
// of paths is made of one full path and one empty path.
TEST_F(RelocateSubtripOneFullOneEmptyOperatorTest, OneFullOneEmpty) {
  SetFirstPaths(8, {{0, 1, 3, 2, 4, 5, 6, 8}, {7, 9}});
  // Move {1, 3, 2, 4} after 5.
  AddExpectedPaths({{0, 5, 1, 3, 2, 4, 6, 8}, {7, 9}});
  // Move {1, 3, 2, 4} after 6.
  AddExpectedPaths({{0, 5, 6, 1, 3, 2, 4, 8}, {7, 9}});
  // Move subsequence {3, 4} after 0.
  AddExpectedPaths({{0, 3, 4, 1, 2, 5, 6, 8}, {7, 9}});
  // Move subsequence {3, 4} after 5.
  AddExpectedPaths({{0, 1, 2, 5, 3, 4, 6, 8}, {7, 9}});
  // Move subsequence {3, 4} after 6.
  AddExpectedPaths({{0, 1, 2, 5, 6, 3, 4, 8}, {7, 9}});
  // Move subsequence {1, 2} after 4.
  AddExpectedPaths({{0, 3, 4, 1, 2, 5, 6, 8}, {7, 9}});
  // Move subsequence {1, 2} after 5.
  AddExpectedPaths({{0, 3, 4, 5, 1, 2, 6, 8}, {7, 9}});
  // Move subsequence {1, 2} after 6.
  AddExpectedPaths({{0, 3, 4, 5, 6, 1, 2, 8}, {7, 9}});
  // {1, 3, 2, 4} is detected when using basenodes 1 and 4,
  // so the following paths are generated again.
  // Move {1, 3, 2, 4} after 5.
  AddExpectedPaths({{0, 5, 1, 3, 2, 4, 6, 8}, {7, 9}});
  // Move {1, 3, 2, 4} after 6.
  AddExpectedPaths({{0, 5, 6, 1, 3, 2, 4, 8}, {7, 9}});

  // Move {1, 3, 2, 4} after 7.
  AddExpectedPaths({{0, 5, 6, 8}, {7, 1, 3, 2, 4, 9}});
  // Move {3, 4} after 7.
  AddExpectedPaths({{0, 1, 2, 5, 6, 8}, {7, 3, 4, 9}});
  // Move {1, 2} after 7.
  AddExpectedPaths({{0, 3, 4, 5, 6, 8}, {7, 1, 2, 9}});
  // Move {1, 3, 2, 4} after 7.
  AddExpectedPaths({{0, 5, 6, 8}, {7, 1, 3, 2, 4, 9}});

  BaseSolveAndTest(/*match_all=*/true);
}

TEST_F(RelocateSubtripOneFullOneEmptyOperatorTest,
       OneFullOneEmptyWithAllowedPaths) {
  SetFirstPaths(8, {{0, 1, 3, 2, 4, 5, 6, 8}, {7, 9}},
                /*fill_secondary_vars=*/true);
  SetAllowedSecondaryValues({{}, {0}, {0}, {}, {}, {}, {}, {}});
  // Move {1, 3, 2, 4} after 5.
  AddExpectedPaths({{0, 5, 1, 3, 2, 4, 6, 8}, {7, 9}});
  // Move {1, 3, 2, 4} after 6.
  AddExpectedPaths({{0, 5, 6, 1, 3, 2, 4, 8}, {7, 9}});
  // Move subsequence {3, 4} after 0.
  AddExpectedPaths({{0, 3, 4, 1, 2, 5, 6, 8}, {7, 9}});
  // Move subsequence {3, 4} after 5.
  AddExpectedPaths({{0, 1, 2, 5, 3, 4, 6, 8}, {7, 9}});
  // Move subsequence {3, 4} after 6.
  AddExpectedPaths({{0, 1, 2, 5, 6, 3, 4, 8}, {7, 9}});
  // Move subsequence {1, 2} after 4.
  AddExpectedPaths({{0, 3, 4, 1, 2, 5, 6, 8}, {7, 9}});
  // Move subsequence {1, 2} after 5.
  AddExpectedPaths({{0, 3, 4, 5, 1, 2, 6, 8}, {7, 9}});
  // Move subsequence {1, 2} after 6.
  AddExpectedPaths({{0, 3, 4, 5, 6, 1, 2, 8}, {7, 9}});
  // {1, 3, 2, 4} is detected when using basenodes 1 and 4,
  // so the following paths are generated again.
  // Move {1, 3, 2, 4} after 5.
  AddExpectedPaths({{0, 5, 1, 3, 2, 4, 6, 8}, {7, 9}});
  // Move {1, 3, 2, 4} after 6.
  AddExpectedPaths({{0, 5, 6, 1, 3, 2, 4, 8}, {7, 9}});
  // Move {3, 4} after 7.
  AddExpectedPaths({{0, 1, 2, 5, 6, 8}, {7, 3, 4, 9}});
  BaseSolveAndTest(/*match_all=*/true);
}

RELOCATE_SUBTRIP_OPERATOR_TEST(Alternatives, (std::vector<PickupDeliveryPair>{
                                                 {{1, 2}, {3, 4}}}));

// In this test, the problem has alternative visits and one pickup/delivery,
// pair, the initial set of paths has two paths.
TEST_F(RelocateSubtripAlternativesOperatorTest, Alternatives) {
  // First solution paths are:
  // 1 3 not performed, will never appear in neighboring paths.
  SetFirstPaths(7, {{0, 2, 5, 4, 7}, {6, 8}});
  AddExpectedPaths({{0, 7}, {6, 2, 5, 4, 8}});
  // {2, 4} is detected when using basenodes 2 and 4,
  // so the following paths are generated again.
  AddExpectedPaths({{0, 7}, {6, 2, 5, 4, 8}});

  BaseSolveAndTest(/*match_all=*/true);
}

#undef RELOCATE_SUBTRIP_OPERATOR_TEST

#define RELOCATE_SUBTRIP_WITH_NEIGHBORS_OPERATOR_TEST(                      \
    name, init_out_neighbors, init_index_pairs)                             \
  class RelocateSubtripWithNeighbors##name##OperatorTest                    \
      : public BaseOperatorTest {                                           \
   protected:                                                               \
    LocalSearchOperator* BuildOperator(const std::vector<IntVar*>& vars,    \
                                       Solver* solver) const override {     \
      const std::vector<int> out_neighbors = init_out_neighbors;            \
      const std::vector<PickupDeliveryPair> index_pairs = init_index_pairs; \
      return MakeRelocateSubtrip(                                           \
          solver, vars, {}, nullptr, nullptr,                               \
          [out_neighbors](int, int) -> const std::vector<int>& {            \
            return out_neighbors;                                           \
          },                                                                \
          index_pairs);                                                     \
    }                                                                       \
  };

RELOCATE_SUBTRIP_WITH_NEIGHBORS_OPERATOR_TEST(OneFullOneEmpty,
                                              (std::vector<int>{1}),
                                              (std::vector<PickupDeliveryPair>{
                                                  {{1}, {2}}, {{3}, {4}}}));

// Simple test: the problem has two pickup/delivery pairs, and the initial set
// of paths is made of one full path and one empty path. Only chains starting
// with 1 are moved.
TEST_F(RelocateSubtripWithNeighborsOneFullOneEmptyOperatorTest,
       OneFullOneEmpty) {
  SetFirstPaths(8, {{0, 1, 3, 2, 4, 5, 6, 8}, {7, 9}});
  // Move {1, 3, 2, 4} after 5.
  AddExpectedPaths({{0, 5, 1, 3, 2, 4, 6, 8}, {7, 9}});
  // Move {1, 3, 2, 4} after 6.
  AddExpectedPaths({{0, 5, 6, 1, 3, 2, 4, 8}, {7, 9}});
  // Move {1, 3, 2, 4} after 7.
  AddExpectedPaths({{0, 5, 6, 8}, {7, 1, 3, 2, 4, 9}});
  BaseSolveAndTest(/*match_all=*/true);
}
RELOCATE_SUBTRIP_WITH_NEIGHBORS_OPERATOR_TEST(Alternatives,
                                              (std::vector<int>{1}),
                                              (std::vector<PickupDeliveryPair>{
                                                  {{1, 2}, {3, 4}}}));

// In this test, the problem has alternative visits and one pickup/delivery,
// pair, the initial set of paths has two paths but the neighboring pair is
// unperformed, preventing any move.
TEST_F(RelocateSubtripWithNeighborsAlternativesOperatorTest, Alternatives) {
  // First solution paths are:
  // 1 3 not performed, will never appear in neighboring paths.
  SetFirstPaths(7, {{0, 2, 5, 4, 7}, {6, 8}});
  BaseSolveAndTest(/*match_all=*/true);
}

#undef RELOCATE_SUBTRIP_WITH_NEIGHBORS_OPERATOR_TEST

#define EXCHANGE_SUBTRIP_OPERATOR_TEST(name, init_index_pairs)              \
  class ExchangeSubtrip##name##OperatorTest : public BaseOperatorTest {     \
   protected:                                                               \
    LocalSearchOperator* BuildOperatorWithSecondaryVars(                    \
        const std::vector<IntVar*>& vars,                                   \
        const std::vector<IntVar*>& secondary_vars,                         \
        Solver* solver) const override {                                    \
      const std::vector<PickupDeliveryPair> index_pairs = init_index_pairs; \
      return MakeExchangeSubtrip(solver, vars, secondary_vars, nullptr,     \
                                 index_pairs);                              \
    }                                                                       \
  };

EXCHANGE_SUBTRIP_OPERATOR_TEST(TwoPairs, (std::vector<PickupDeliveryPair>{
                                             {{1}, {2}}, {{3}, {4}}}));

// One path that has interleaved pickup/delivery pairs, cannot be exchanged.
TEST_F(ExchangeSubtripTwoPairsOperatorTest, InterleavedPairs) {
  SetFirstPaths(8, {{0, 1, 5, 3, 6, 2, 7, 4, 8}});
  // We expect the neighborhood to generate nothing.
  BaseSolveAndTest(/*match_all*/ true);
}

// One path where the subtrips of pairs are concatenated, should be exchanged.
TEST_F(ExchangeSubtripTwoPairsOperatorTest, ConcatenatedPairs) {
  SetFirstPaths(7, {{0, 3, 6, 4, 1, 5, 2, 7}});
  AddExpectedPaths({{0, 1, 5, 2, 3, 6, 4, 7}});  // BaseNodes 1, 3
  BaseSolveAndTest(/*match_all*/ true);
}

// Path with two pairs, general case, should be exchanged.
TEST_F(ExchangeSubtripTwoPairsOperatorTest, DisjointPairs) {
  SetFirstPaths(8, {{0, 1, 5, 2, 7, 3, 6, 4, 8}});
  AddExpectedPaths({{0, 3, 6, 4, 7, 1, 5, 2, 8}});  // BaseNodes 1, 3
  BaseSolveAndTest(/*match_all*/ true);
}

EXCHANGE_SUBTRIP_OPERATOR_TEST(ThreePairs,
                               (std::vector<PickupDeliveryPair>{
                                   {{1}, {2}}, {{3}, {4}}, {{5}, {6}}}));

// There are 3 pairs forming a subtrip, all disjoint, allowing 3 neighbors.
TEST_F(ExchangeSubtripThreePairsOperatorTest, ConcatenatedPairs) {
  SetFirstPaths(8, {{0, 1, 2, 3, 4, 8}, {7, 5, 6, 9}});
  AddExpectedPaths({{0, 3, 4, 1, 2, 8}, {7, 5, 6, 9}});  // BaseNodes 1, 3
  AddExpectedPaths({{0, 5, 6, 3, 4, 8}, {7, 1, 2, 9}});  // BaseNodes 1, 5
  AddExpectedPaths({{0, 1, 2, 5, 6, 8}, {7, 3, 4, 9}});  // BaseNodes 3, 5
  BaseSolveAndTest(/*match_all*/ true);
}

TEST_F(ExchangeSubtripThreePairsOperatorTest,
       ConcatenatedPairsWithCompatibility) {
  SetFirstPaths(8, {{0, 1, 2, 3, 4, 8}, {7, 5, 6, 9}},
                /*fill_secondary_vars=*/true);
  SetAllowedSecondaryValues({{}, {}, {}, {}, {}, {1}, {}, {}});
  AddExpectedPaths({{0, 3, 4, 1, 2, 8}, {7, 5, 6, 9}});  // BaseNodes 1, 3
  BaseSolveAndTest(/*match_all*/ true);
}

// The two pairs in the first path are interleaved.
TEST_F(ExchangeSubtripThreePairsOperatorTest, InterleavedPairs) {
  SetFirstPaths(8, {{0, 1, 3, 2, 4, 8}, {7, 5, 6, 9}});
  AddExpectedPaths({{0, 5, 6, 8}, {7, 1, 3, 2, 4, 9}});  // BaseNodes 1, 5
  AddExpectedPaths({{0, 1, 5, 6, 2, 8}, {7, 3, 4, 9}});  // BaseNodes 3, 5
  AddExpectedPaths({{0, 3, 5, 6, 4, 8}, {7, 1, 2, 9}});  // BaseNodes 2, 5
  AddExpectedPaths({{0, 5, 6, 8}, {7, 1, 3, 2, 4, 9}});  // BaseNodes 4, 5
  BaseSolveAndTest(/*match_all*/ true);
}

TEST_F(ExchangeSubtripThreePairsOperatorTest,
       InterleavedPairsWithCompatibility) {
  SetFirstPaths(8, {{0, 1, 3, 2, 4, 8}, {7, 5, 6, 9}},
                /*fill_secondary_vars=*/true);
  SetAllowedSecondaryValues({{}, {}, {}, {}, {}, {1}, {}, {}});
  BaseSolveAndTest(/*match_all*/ true);
}

// One pair in the first path is included in the other.
TEST_F(ExchangeSubtripThreePairsOperatorTest, IncludedPairs) {
  SetFirstPaths(8, {{0, 1, 3, 4, 2, 8}, {7, 5, 6, 9}});
  AddExpectedPaths({{0, 5, 6, 8}, {7, 1, 3, 4, 2, 9}});  // BaseNodes 1, 5
  AddExpectedPaths({{0, 1, 5, 6, 2, 8}, {7, 3, 4, 9}});  // BaseNodes 3, 5
  BaseSolveAndTest(/*match_all*/ true);
}

TEST_F(ExchangeSubtripThreePairsOperatorTest, IncludedPairsWithCompatibility) {
  SetFirstPaths(8, {{0, 1, 3, 4, 2, 8}, {7, 5, 6, 9}},
                /*fill_secondary_vars=*/true);
  SetAllowedSecondaryValues({{}, {}, {}, {}, {}, {1}, {}, {}});
  BaseSolveAndTest(/*match_all*/ true);
}

// All three pairs in the same path.
TEST_F(ExchangeSubtripThreePairsOperatorTest, ConcatenatedPairsSamePath) {
  SetFirstPaths(7, {{0, 1, 2, 3, 4, 5, 6, 7}});
  AddExpectedPaths({{0, 3, 4, 1, 2, 5, 6, 7}});  // BaseNodes 1, 3
  AddExpectedPaths({{0, 5, 6, 3, 4, 1, 2, 7}});  // BaseNodes 1, 5
  AddExpectedPaths({{0, 1, 2, 5, 6, 3, 4, 7}});  // BaseNodes 3, 5
  BaseSolveAndTest(/*match_all*/ true);
}

#undef EXCHANGE_SUBTRIP_OPERATOR_TEST

#define EXCHANGE_SUBTRIP_WITH_NEIGHBORS_OPERATOR_TEST(name, init_neighbors, \
                                                      init_index_pairs)     \
  class ExchangeSubtripWithNeighbors##name##OperatorTest                    \
      : public BaseOperatorTest {                                           \
   protected:                                                               \
    LocalSearchOperator* BuildOperator(const std::vector<IntVar*>& vars,    \
                                       Solver* solver) const override {     \
      const std::vector<int> neighbors = init_neighbors;                    \
      const std::vector<PickupDeliveryPair> index_pairs = init_index_pairs; \
      return MakeExchangeSubtrip(                                           \
          solver, vars, {}, nullptr, nullptr,                               \
          [neighbors](int, int) -> const std::vector<int>& {                \
            return neighbors;                                               \
          },                                                                \
          index_pairs);                                                     \
    }                                                                       \
  };

EXCHANGE_SUBTRIP_WITH_NEIGHBORS_OPERATOR_TEST(TwoPairs, (std::vector<int>{3}),
                                              (std::vector<PickupDeliveryPair>{
                                                  {{1}, {2}}, {{3}, {4}}}));

// One path that has interleaved pickup/delivery pairs, cannot be exchanged.
TEST_F(ExchangeSubtripWithNeighborsTwoPairsOperatorTest, InterleavedPairs) {
  SetFirstPaths(8, {{0, 1, 5, 3, 6, 2, 7, 4, 8}});
  // We expect the neighborhood to generate nothing.
  BaseSolveAndTest(/*match_all*/ true);
}

// One path where the subtrips of pairs are concatenated, should be exchanged.
TEST_F(ExchangeSubtripWithNeighborsTwoPairsOperatorTest, ConcatenatedPairs) {
  SetFirstPaths(7, {{0, 3, 6, 4, 1, 5, 2, 7}});
  AddExpectedPaths({{0, 1, 5, 2, 3, 6, 4, 7}});  // BaseNodes 1, 3
  BaseSolveAndTest(/*match_all*/ true);
}

// Path with two pairs, general case, should be exchanged.
TEST_F(ExchangeSubtripWithNeighborsTwoPairsOperatorTest, DisjointPairs) {
  SetFirstPaths(8, {{0, 1, 5, 2, 7, 3, 6, 4, 8}});
  AddExpectedPaths({{0, 3, 6, 4, 7, 1, 5, 2, 8}});  // BaseNodes 1, 3
  BaseSolveAndTest(/*match_all*/ true);
}

EXCHANGE_SUBTRIP_WITH_NEIGHBORS_OPERATOR_TEST(
    ThreePairs, (std::vector<int>{3}),
    (std::vector<PickupDeliveryPair>{{{1}, {2}}, {{3}, {4}}, {{5}, {6}}}));

// There are 3 pairs forming a subtrip, all disjoint, allowing neighbors
// involving node 3.
TEST_F(ExchangeSubtripWithNeighborsThreePairsOperatorTest, ConcatenatedPairs) {
  SetFirstPaths(8, {{0, 1, 2, 3, 4, 8}, {7, 5, 6, 9}});
  AddExpectedPaths({{0, 3, 4, 1, 2, 8}, {7, 5, 6, 9}});  // BaseNodes 1, 3
  BaseSolveAndTest(/*match_all*/ true);
}

// The two pairs in the first path are interleaved.
TEST_F(ExchangeSubtripWithNeighborsThreePairsOperatorTest, InterleavedPairs) {
  SetFirstPaths(8, {{0, 1, 3, 2, 4, 8}, {7, 5, 6, 9}});
  BaseSolveAndTest(/*match_all*/ true);
}

// One pair in the first path is included in the other.
TEST_F(ExchangeSubtripWithNeighborsThreePairsOperatorTest, IncludedPairs) {
  SetFirstPaths(8, {{0, 1, 3, 4, 2, 8}, {7, 5, 6, 9}});
  BaseSolveAndTest(/*match_all*/ true);
}

// All three pairs in the same path.
TEST_F(ExchangeSubtripWithNeighborsThreePairsOperatorTest,
       ConcatenatedPairsSamePath) {
  SetFirstPaths(7, {{0, 1, 2, 3, 4, 5, 6, 7}});
  AddExpectedPaths({{0, 3, 4, 1, 2, 5, 6, 7}});  // BaseNodes 1, 3
  BaseSolveAndTest(/*match_all*/ true);
}

#undef EXCHANGE_SUBTRIP_WITH_NEIGHBORS_OPERATOR_TEST

// TODO(user): Add unit tests for all other operators.
// TODO(user): Add unit tests with vehicle variables in neighbors.

/////////////////////// Operators in Model tests ///////////////////////

// Non-regression test: corner case found by an external user requiring
// MAKE_CHAIN_INACTIVE to be on by default, to escape a plateau where
// unperforming nodes 1 by 1 does not change the cost (e.g. if nodes are at the
// same location).
TEST(MakeChainInactiveOperatorTest, EscapesPlateau) {
  for (bool deactivate_make_chain_inactive : {true, false}) {
    IndexManager manager(4, 1, NodeIndex(0));
    Model model(manager);
    // Make node 2 and 3 optional
    model.AddDisjunction({manager.NodeToIndex(NodeIndex(2))}, 0);
    model.AddDisjunction({manager.NodeToIndex(NodeIndex(3))}, 0);
    const std::vector<std::vector<int64_t>> costs = {
        {0, 1, 2, 2}, {1, 0, 1, 1}, {2, 1, 0, 0}, {2, 1, 0, 0}};
    const int callback = model.RegisterTransitCallback(
        [&costs, &manager](int from_index, int to_index) {
          return costs[manager.IndexToNode(from_index).value()]
                      [manager.IndexToNode(to_index).value()];
        });
    model.SetArcCostEvaluatorOfAllVehicles(callback);
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    if (deactivate_make_chain_inactive) {
      parameters.mutable_local_search_operators()->set_use_make_chain_inactive(
          OptionalBoolean::BOOL_FALSE);
    }
    const Assignment* solution = model.SolveWithParameters(parameters);
    if (deactivate_make_chain_inactive) {
      EXPECT_NE(solution->Value(model.NextVar(2)), 2);
      EXPECT_NE(solution->Value(model.NextVar(3)), 3);
    } else {
      EXPECT_EQ(solution->Value(model.NextVar(2)), 2);
      EXPECT_EQ(solution->Value(model.NextVar(3)), 3);
    }
  }
}

// Real-life problem taken from GSX where the solution can only be improved
// using RelocateNeighbors.
// Initial solution (# marks boundary between physically different places):
// 30 # 26 28 22 # 29 # 32 # 27 # 31 # 20 24 34 18 # 19 # 33 # 23 # 35 # 25 # 21
// 30 at the same place as {20, 24, 34, 18}, so {20, 24, 34, 18} should be
// moved after 30 (30 cannot be moved before 20 due to precedence constraints).

TEST(RelocateNeighborsOperator, MoveStopNodes) {
  const FirstSolutionStrategy::Value kInsertionHeuristics[] = {
      FirstSolutionStrategy::BEST_INSERTION,
      FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION,
      FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION};
  const bool kOptionalOnly[] = {true, false, false};
  const int64_t kExpectedCosts[] = {11895, 11895, 12076};
  const int kVehicleCount = 9;
  const int kParcelCount = 9;
  const int64_t kOptionalNodeCost = 1000000L;
  std::vector<std::pair<NodeIndex, NodeIndex>> start_end;
  for (int i = 0; i < kVehicleCount; ++i) {
    start_end.push_back({NodeIndex(2 * i), NodeIndex(2 * i + 1)});
  }
  const int node_count = 2 * kVehicleCount + 2 * kParcelCount;
  for (int insertion = 0; insertion < std::size(kInsertionHeuristics);
       ++insertion) {
    IndexManager manager(36, kVehicleCount, start_end);
    Model model(manager);
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(kInsertionHeuristics[insertion]);

    const std::vector<std::vector<int64_t>> costs = {
        {0,    0,    0,    0,    0,    0,    0,    0,    0,
         0,    0,    0,    0,    0,    0,    0,    0,    0,
         1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200,
         1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0,    0,    0,    0,    0,    0,    0,    0,    0,
         0,    0,    0,    0,    0,    0,    0,    0,    0,
         1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200,
         1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0,    0,    0,    0,    0,    0,    0,    0,    0,
         0,    0,    0,    0,    0,    0,    0,    0,    0,
         1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200,
         1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0,    0,    0,    0,    0,    0,    0,    0,    0,
         0,    0,    0,    0,    0,    0,    0,    0,    0,
         1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200,
         1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0,    0,    0,    0,    0,    0,    0,    0,    0,
         0,    0,    0,    0,    0,    0,    0,    0,    0,
         1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200,
         1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0,    0,    0,    0,    0,    0,    0,    0,    0,
         0,    0,    0,    0,    0,    0,    0,    0,    0,
         1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200,
         1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0,    0,    0,    0,    0,    0,    0,    0,    0,
         0,    0,    0,    0,    0,    0,    0,    0,    0,
         1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200,
         1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0,    0,    0,    0,    0,    0,    0,    0,    0,
         0,    0,    0,    0,    0,    0,    0,    0,    0,
         1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200,
         1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0,    0,    0,    0,    0,    0,    0,    0,    0,
         0,    0,    0,    0,    0,    0,    0,    0,    0,
         1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200,
         1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {630, 0,    630, 0,    630, 0,    630, 0,    630,  0,    630, 0,
         630, 0,    630, 0,    630, 0,    30,  728,  30,   1325, 949, 912,
         30,  1017, 949, 1954, 949, 1363, 30,  1405, 2400, 912,  30,  912},
        {630, 0,    630,  0,    630,  0,    630, 0,    630,  0,    630,  0,
         630, 0,    630,  0,    630,  0,    728, 30,   728,  1242, 1047, 942,
         728, 1065, 1047, 2032, 1047, 1457, 728, 1480, 2486, 942,  728,  942},
        {630, 0,    630, 0,    630, 0,    630, 0,    630,  0,    630, 0,
         630, 0,    630, 0,    630, 0,    30,  728,  30,   1325, 949, 912,
         30,  1017, 949, 1954, 949, 1363, 30,  1405, 2400, 912,  30,  912},
        {630,  0,    630,  0,    630,  0,    630,  0,    630,
         0,    630,  0,    630,  0,    630,  0,    630,  0,
         1325, 1242, 1325, 30,   1620, 1275, 1325, 1385, 1620,
         2644, 1620, 2053, 1325, 2091, 3095, 1275, 1325, 1275},
        {630, 0,    630, 0,    630, 0,    630, 0,    630,  0,    630, 0,
         630, 0,    630, 0,    630, 0,    949, 1047, 949,  1620, 30,  1037,
         949, 1037, 30,  1710, 30,  1066, 949, 1197, 2120, 1037, 949, 1037},
        {630, 0,   630,  0,    630,  0,    630, 0,    630,  0,    630,  0,
         630, 0,   630,  0,    630,  0,    912, 942,  912,  1275, 1037, 30,
         912, 766, 1037, 2117, 1037, 1466, 912, 1594, 2520, 30,   912,  30},
        {630, 0,    630, 0,    630, 0,    630, 0,    630,  0,    630, 0,
         630, 0,    630, 0,    630, 0,    30,  728,  30,   1325, 949, 912,
         30,  1017, 949, 1954, 949, 1363, 30,  1405, 2400, 912,  30,  912},
        {630,  0,  630,  0,    630,  0,    630,  0,    630,  0,    630,  0,
         630,  0,  630,  0,    630,  0,    1017, 1065, 1017, 1385, 1037, 766,
         1017, 30, 1037, 2103, 1037, 1436, 1017, 1605, 2481, 766,  1017, 766},
        {630, 0,    630, 0,    630, 0,    630, 0,    630,  0,    630, 0,
         630, 0,    630, 0,    630, 0,    949, 1047, 949,  1620, 30,  1037,
         949, 1037, 30,  1710, 30,  1066, 949, 1197, 2120, 1037, 949, 1037},
        {630,  0,    630,  0,  630,  0,    630,  0,    630,  0,    630,  0,
         630,  0,    630,  0,  630,  0,    1954, 2032, 1954, 2644, 1710, 2117,
         1954, 2103, 1710, 30, 1710, 1304, 1954, 1183, 1139, 2117, 1954, 2117},
        {630, 0,    630, 0,    630, 0,    630, 0,    630,  0,    630, 0,
         630, 0,    630, 0,    630, 0,    949, 1047, 949,  1620, 30,  1037,
         949, 1037, 30,  1710, 30,  1066, 949, 1197, 2120, 1037, 949, 1037},
        {630,  0,    630,  0,    630,  0,  630,  0,    630,  0,    630,  0,
         630,  0,    630,  0,    630,  0,  1363, 1457, 1363, 2053, 1066, 1466,
         1363, 1436, 1066, 1304, 1066, 30, 1363, 937,  1684, 1466, 1363, 1466},
        {630, 0,    630, 0,    630, 0,    630, 0,    630,  0,    630, 0,
         630, 0,    630, 0,    630, 0,    30,  728,  30,   1325, 949, 912,
         30,  1017, 949, 1954, 949, 1363, 30,  1405, 2400, 912,  30,  912},
        {630,  0,    630,  0,    630,  0,   630,  0,    630,  0,    630,  0,
         630,  0,    630,  0,    630,  0,   1405, 1480, 1405, 2091, 1197, 1594,
         1405, 1605, 1197, 1183, 1197, 937, 1405, 30,   1661, 1594, 1405, 1594},
        {630,  0,    630,  0,    630,  0,    630,  0,    630,
         0,    630,  0,    630,  0,    630,  0,    630,  0,
         2400, 2486, 2400, 3095, 2120, 2520, 2400, 2481, 2120,
         1139, 2120, 1684, 2400, 1661, 30,   2520, 2400, 2520},
        {630, 0,   630,  0,    630,  0,    630, 0,    630,  0,    630,  0,
         630, 0,   630,  0,    630,  0,    912, 942,  912,  1275, 1037, 30,
         912, 766, 1037, 2117, 1037, 1466, 912, 1594, 2520, 30,   912,  30},
        {630, 0,    630, 0,    630, 0,    630, 0,    630,  0,    630, 0,
         630, 0,    630, 0,    630, 0,    30,  728,  30,   1325, 949, 912,
         30,  1017, 949, 1954, 949, 1363, 30,  1405, 2400, 912,  30,  912},
        {630, 0,   630,  0,    630,  0,    630, 0,    630,  0,    630,  0,
         630, 0,   630,  0,    630,  0,    912, 942,  912,  1275, 1037, 30,
         912, 766, 1037, 2117, 1037, 1466, 912, 1594, 2520, 30,   912,  30}};
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    model.AddConstantDimension(1, node_count, true, "sequence");
    Dimension* const sequence_dimension = model.GetMutableDimension("sequence");
    Solver* const solver = model.solver();
    for (int i = 0; i < kParcelCount; ++i) {
      const NodeIndex pickup_node(2 * kVehicleCount + 2 * i);
      const NodeIndex dropoff_node(pickup_node + 1);
      const int64_t pickup_index = manager.NodeToIndex(pickup_node);
      const int64_t dropoff_index = manager.NodeToIndex(dropoff_node);
      solver->AddConstraint(solver->MakeEquality(
          model.VehicleVar(pickup_index), model.VehicleVar(dropoff_index)));
      solver->AddConstraint(
          solver->MakeLess(sequence_dimension->CumulVar(pickup_index),
                           sequence_dimension->CumulVar(dropoff_index)));
      model.AddPickupAndDelivery(manager.NodeToIndex(pickup_node),
                                 manager.NodeToIndex(dropoff_node));
    }
    if (kOptionalOnly[insertion]) {
      for (int i = 2 * kVehicleCount; i < node_count; ++i) {
        std::vector<int64_t> index = {manager.NodeToIndex(NodeIndex(i))};
        model.AddDisjunction(index, kOptionalNodeCost);
      }
    }

    const Assignment* solution = model.SolveWithParameters(parameters);
    ASSERT_NE(solution, nullptr);
    EXPECT_EQ(kExpectedCosts[insertion], solution->ObjectiveValue());
  }
}

void ExpectEqualRoutes(const Model& model, absl::Span<const int> solution,
                       const Assignment& assignment) {
  ASSERT_LE(solution.size(), model.Nexts().size());
  for (int i = 0; i < solution.size(); ++i) {
    ASSERT_GE(solution[i], 0);
    ASSERT_LT(solution[i], model.Size() + model.vehicles());
    ASSERT_TRUE(assignment.Contains(model.NextVar(i)));
    EXPECT_EQ(solution[i], assignment.Value(model.NextVar(i)));
  }
}

// Tests the FilteredHeuristicPathLNSOperator on a model with different vehicle
// fixed costs.
// In the example below, the initial solution uses the vehicle with higher fixed
// cost to serve 2 out of 3 nodes. Both GlobalCheapestInsertion and
// LocalCheapestInsertion should modify the solution to use the vehicle with a
// lower fixed cost only.
TEST(FilteredHeuristicPathLNSOperator, UseVehicleWithLowerFixedCost) {
  const int kNumNodes = 4;
  const int kNumVehicles = 2;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  const std::vector<int> initial_solution = {1, 2, 5, 6, 3};
  const std::vector<int> expected_result_with_heuristic_lns = {5, 2, 3, 6, 1};

  for (const bool use_gci_path_lns : {true, false}) {
    for (const bool use_lci_path_lns : {true, false}) {
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      DisableAllLocalSearchOperators(
          parameters.mutable_local_search_operators());
      parameters.mutable_local_search_operators()
          ->set_use_global_cheapest_insertion_path_lns(
              use_gci_path_lns ? BOOL_TRUE : BOOL_FALSE);
      parameters.mutable_local_search_operators()
          ->set_use_local_cheapest_insertion_path_lns(
              use_lci_path_lns ? BOOL_TRUE : BOOL_FALSE);

      Model model(manager);

      const auto arc_cost = [&model](int64_t from, int64_t to) {
        if (!model.IsStart(from) && from > to) {
          // Arc is more expensive.
          return 2;
        }
        return 1;
      };
      const int vehicle_cost = model.RegisterTransitCallback(arc_cost);
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
      model.SetFixedCostOfVehicle(10000, 0);

      const Assignment* const solution =
          model.SolveFromAssignmentWithParameters(
              BuildAssignmentFromSolution(model, initial_solution), parameters);
      ASSERT_NE(solution, nullptr);

      if (use_gci_path_lns || use_lci_path_lns) {
        EXPECT_EQ(solution->ObjectiveValue(), 4);
        ExpectEqualRoutes(model, expected_result_with_heuristic_lns, *solution);
      } else {
        EXPECT_EQ(solution->ObjectiveValue(), 10005);
        ExpectEqualRoutes(model, initial_solution, *solution);
      }
    }
  }
}

// Tests the RelocatePathAndHeuristicInsertUnperformedOperator on a model with
// 3 vehicles and 2 nodes:
// - The second node requires the first node on the same vehicle when visited.
// - The second node is only allowed on the last vehicle.
// - The last vehicle has a higher fixed cost.
// In the example below, the initial solution has the first node on the first
// vehicle with a lower fixed cost, and the second node unperformed.
// Only RelocatePathAndHeuristicInsertOperator operator effectively moves
// the first node to the last path, and successfully inserts the second node
// on this route.
TEST(RelocatePathAndHeuristicInsertOperator, InsertNodeOnAllowedVehicle) {
  const int kNumNodes = 3;
  const int kNumVehicles = 3;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  const std::vector<int> initial_solution = {1, 5, 2, 6, 7};
  const std::vector<int> expected_result_with_heuristic_lns = {5, 2, 7, 6, 1};

  for (const bool use_relocate_path_heuristic_insert : {true, false}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.mutable_local_search_operators()
        ->set_use_relocate_path_global_cheapest_insertion_insert_unperformed(
            use_relocate_path_heuristic_insert ? BOOL_TRUE : BOOL_FALSE);
    parameters.mutable_local_search_operators()
        ->set_use_global_cheapest_insertion_visit_types_lns(BOOL_FALSE);
    parameters.mutable_local_search_operators()
        ->set_use_local_cheapest_insertion_visit_types_lns(BOOL_FALSE);
    parameters.set_log_search(true);

    Model model(manager);

    model.SetFixedCostOfVehicle(1000, 2);
    // Node2 can only be on the second vehicle.
    model.SetAllowedVehiclesForIndex({2}, 2);
    // Make second node optional with a cost of 1e6.
    model.AddDisjunction({2}, 1e6);
    // Specify Node2 requires Node1 on the same vehicle.
    model.SetVisitType(1, 1, Model::TYPE_ADDED_TO_VEHICLE);
    model.SetVisitType(2, 2, Model::TYPE_ADDED_TO_VEHICLE);
    model.AddRequiredTypeAlternativesWhenAddingType(2, {1});

    const Assignment* const solution = model.SolveFromAssignmentWithParameters(
        BuildAssignmentFromSolution(model, initial_solution), parameters);
    ASSERT_NE(solution, nullptr);

    if (use_relocate_path_heuristic_insert) {
      EXPECT_EQ(solution->ObjectiveValue(), 1000);
      ExpectEqualRoutes(model, expected_result_with_heuristic_lns, *solution);
    } else {
      EXPECT_EQ(solution->ObjectiveValue(), 1e6);
      ExpectEqualRoutes(model, initial_solution, *solution);
    }
  }
}

void DisableHeuristicLNSOperators(
    RoutingSearchParameters::LocalSearchNeighborhoodOperators* operators) {
  operators->set_use_global_cheapest_insertion_path_lns(BOOL_FALSE);
  operators->set_use_local_cheapest_insertion_path_lns(BOOL_FALSE);
  operators->set_use_global_cheapest_insertion_expensive_chain_lns(BOOL_FALSE);
  operators->set_use_local_cheapest_insertion_expensive_chain_lns(BOOL_FALSE);
  operators->set_use_global_cheapest_insertion_close_nodes_lns(BOOL_FALSE);
  operators->set_use_local_cheapest_insertion_close_nodes_lns(BOOL_FALSE);
}

// The following test showcases an example where only the
// FilteredHeuristic LNS operators can improve the solution by merging the
// two routes. The model has 2 vehicles, one pickup/delivery pair and 2 single
// nodes. The first solution is the following:
// Vehicle 0: START --> Pickup --> Node1 --> Delivery --> END
// Vehicle 1: START --> Node2 --> END
// The Next vars are constrained such that the only possible solutions are the
// given first solution, and
// Vehicle 0: START --> END
// Vehicle 1: START --> Pickup --> Node1 --> Node2 --> Delivery --> END
// Since the vehicles have fixed costs, this solution has a lower cost than the
// initial solution, and the only way to go from the initial solution to this
// one while respecting the constraints is by emptying the first route and
// reinserting the nodes, done by the
// FilteredHeuristic[Path|ExpensiveChain|CloseNodes]LNSOperator.
void TestMergingRoutesWithHeuristicLNS(
    const std::function<
        void(RoutingSearchParameters::LocalSearchNeighborhoodOperators*)>&
        activate_heuristic_lns,
    absl::Span<const double> neighborhood_ratios) {
  const int kNumNodes = 5;
  const int kNumVehicles = 2;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  const std::vector<int> kFixedCosts = {20000, 10000};

  const int pickup = 3;
  const int delivery = 4;
  const std::vector<int> initial_solution = {pickup, delivery, 7, 1, 6, 2};
  const std::vector<int> expected_result_with_heuristic_lns = {6, 2, delivery,
                                                               1, 7, pickup};

  for (const bool use_heuristic_lns : {true, false}) {
    for (const double neighbors_ratio : neighborhood_ratios) {
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();

      RoutingSearchParameters::LocalSearchNeighborhoodOperators* operators =
          parameters.mutable_local_search_operators();
      if (!use_heuristic_lns) {
        // Disable only heuristic LNS operators.
        DisableHeuristicLNSOperators(operators);
      } else {
        DisableAllLocalSearchOperators(operators);
        activate_heuristic_lns(operators);
      }

      parameters.mutable_global_cheapest_insertion_ls_operator_parameters()
          ->set_neighbors_ratio(neighbors_ratio);

      Model model(manager);

      const int vehicle_cost =
          model.RegisterTransitCallback([](int64_t, int64_t) { return 1; });
      for (int v = 0; v < kNumVehicles; v++) {
        model.SetFixedCostOfVehicle(kFixedCosts[v], v);
      }
      model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
      model.AddPickupAndDelivery(pickup, delivery);

      // Restrict Node2 to the second vehicle.
      model.VehicleVar(2)->SetValue(1);
      // Restrict Next vars.
      for (int n = 0; n < model.Size(); n++) {
        if (model.IsStart(n) || n == pickup) {
          continue;
        }
        model.NextVar(n)->SetValues(
            {initial_solution[n], expected_result_with_heuristic_lns[n]});
      }
      model.NextVar(pickup)->SetValues({initial_solution[pickup], 2, delivery});

      const Assignment* const solution =
          model.SolveFromAssignmentWithParameters(
              BuildAssignmentFromSolution(model, initial_solution), parameters);
      ASSERT_NE(solution, nullptr);

      if (use_heuristic_lns) {
        EXPECT_EQ(solution->ObjectiveValue(), kFixedCosts[1] + 5);
        ExpectEqualRoutes(model, expected_result_with_heuristic_lns, *solution);
      } else {
        EXPECT_EQ(solution->ObjectiveValue(),
                  kFixedCosts[0] + kFixedCosts[1] + 6);
        ExpectEqualRoutes(model, initial_solution, *solution);
      }
    }
  }
}

TEST(FilteredHeuristicLNSMergeRoutesTest, GlobalCheapestInsertionPathLNS) {
  TestMergingRoutesWithHeuristicLNS(
      [](RoutingSearchParameters::LocalSearchNeighborhoodOperators* ops) {
        ops->set_use_global_cheapest_insertion_path_lns(BOOL_TRUE);
      },
      /*neighborhood_ratios*/ {0.1, 0.2, 0.5, 1.0});
}

TEST(FilteredHeuristicLNSMergeRoutesTest, LocalCheapestInsertionPathLNS) {
  TestMergingRoutesWithHeuristicLNS(
      [](RoutingSearchParameters::LocalSearchNeighborhoodOperators* ops) {
        ops->set_use_local_cheapest_insertion_path_lns(BOOL_TRUE);
      },
      /*neighborhood_ratios*/ {1.0});
}

TEST(FilteredHeuristicLNSMergeRoutesTest, GlobalCheapestInsertionChainLNS) {
  TestMergingRoutesWithHeuristicLNS(
      [](RoutingSearchParameters::LocalSearchNeighborhoodOperators* ops) {
        ops->set_use_global_cheapest_insertion_expensive_chain_lns(BOOL_TRUE);
      },
      /*neighborhood_ratios*/ {0.1, 0.2, 0.5, 1.0});
}

TEST(FilteredHeuristicLNSMergeRoutesTest, LocalCheapestInsertionChainLNS) {
  TestMergingRoutesWithHeuristicLNS(
      [](RoutingSearchParameters::LocalSearchNeighborhoodOperators* ops) {
        ops->set_use_local_cheapest_insertion_expensive_chain_lns(BOOL_TRUE);
      },
      /*neighborhood_ratios*/ {1.0});
}

TEST(FilteredHeuristicLNSMergeRoutesTest,
     GlobalCheapestInsertionCloseNodesLNS) {
  TestMergingRoutesWithHeuristicLNS(
      [](RoutingSearchParameters::LocalSearchNeighborhoodOperators* ops) {
        ops->set_use_global_cheapest_insertion_close_nodes_lns(BOOL_TRUE);
      },
      /*neighborhood_ratios*/ {0.1, 0.2, 0.7, 1.0});
}

TEST(FilteredHeuristicLNSMergeRoutesTest, LocalCheapestInsertionCloseNodesLNS) {
  TestMergingRoutesWithHeuristicLNS(
      [](RoutingSearchParameters::LocalSearchNeighborhoodOperators* ops) {
        ops->set_use_local_cheapest_insertion_close_nodes_lns(BOOL_TRUE);
      },
      /*neighborhood_ratios*/ {1.0});
}

// Tests that RelocateExpensiveChain detects and relocates an expensive chain
// in a path to a better position.
// The example is the one below:
//
//    -- 10 -- Node2 -------- 50 -------- Node3
//   |           |                          |  \
//   |           |                          |   \
// DEPOT0        5                          5    -- 15 -- DEPOT1
//   |           |                          |   /
//   |           |                          |  /
//    -- 10 -- Node5 -------- 50 -------- Node4
//
// The original assignment given is DEPOT0 --> 2 --> 3 --> 4 --> 5 --> DEPOT0,
// leaving the second vehicle empty (cost of 125).
// The RelocateExpensiveChain operator should move the arc 3 --> 4 to the second
// route, resulting in an overall cost of 25 + 35 = 60.
// Besides path LNS neighborhoods, no other move is able to improve the solution
// initially given, so disabling RelocateExpensiveChain prevents from improving
// the solution.
TEST(RelocateExpensiveChainOperatorInModelTest, TwoRoutes) {
  const std::vector<int> initial_solution = {2, 7, 3, 4, 5, 6};
  const std::vector<int> expected_result_with_rec = {2, 3, 5, 4, 7, 6};
  for (bool use_relocate_expensive_chain : {true, false}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    RoutingSearchParameters::LocalSearchNeighborhoodOperators* operators =
        parameters.mutable_local_search_operators();
    if (use_relocate_expensive_chain) {
      DisableAllLocalSearchOperators(operators);
      operators->set_use_relocate_expensive_chain(BOOL_TRUE);
    } else {
      DisableHeuristicLNSOperators(operators);
      operators->set_use_relocate_expensive_chain(BOOL_FALSE);
    }

    const int kNumNodes = 6;
    const int kNumVehicles = 2;
    const auto startend1 = std::make_pair(NodeIndex(0), NodeIndex(0));
    const auto startend2 = std::make_pair(NodeIndex(1), NodeIndex(1));
    IndexManager manager(kNumNodes, kNumVehicles, {startend1, startend2});
    Model model(manager);

    const std::vector<std::vector<int64_t>> costs = {
        {0, 100, 10, 60, 60, 10}, {100, 0, 65, 15, 15, 65},
        {10, 65, 0, 50, 55, 5},   {60, 15, 50, 0, 5, 55},
        {60, 15, 55, 5, 0, 50},   {10, 65, 5, 55, 50, 0}};
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    const Assignment* const solution = model.SolveFromAssignmentWithParameters(
        BuildAssignmentFromSolution(model, initial_solution), parameters);
    ASSERT_NE(solution, nullptr);

    if (use_relocate_expensive_chain) {
      EXPECT_EQ(solution->ObjectiveValue(), 60);
      ExpectEqualRoutes(model, expected_result_with_rec, *solution);
    } else {
      EXPECT_EQ(solution->ObjectiveValue(), 125);
      ExpectEqualRoutes(model, initial_solution, *solution);
    }
  }
}

// Tests that RelocateExpensiveChain handles routes becoming empty properly.
// The example is the one below:
//
//    -- 10 -- Node2 ------ 50 ------ Node3
//   |           |                        |  \
//   |           |                        |   \
// NODE0         5                        5    ------ 75 ------ NODE1 (Depot1&2)
//   |           |                        |   /
//   |           |                        |  /
//    -- 10 -- Node5 ------ 50 ------ Node4
//
// The original assignment given is DEPOT0 --> 2 --> 5 --> DEPOT0, and
// DEPOT2 --> 3 --> 4 --> DEPOT2 (second vehicle empty, cost of 25 + 155 = 180).
// The RelocateExpensiveChain operator should move the arc 3 --> 4 to the first
// route, emptying the second vehicle, and resulting in an overall cost of 125.
TEST(RelocateExpensiveChainOperatorInModelTest, LastRouteEmptied) {
  const std::vector<int> initial_solution = {2, 8, 5, 4, 9, 7, 3};
  const std::vector<int> expected_result = {2, 8, 3, 4, 5, 7, 9};

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  DisableAllLocalSearchOperators(parameters.mutable_local_search_operators());
  parameters.mutable_local_search_operators()->set_use_relocate_expensive_chain(
      BOOL_TRUE);

  const int kNumNodes = 6;
  const int kNumVehicles = 3;
  const auto startend1 = std::make_pair(NodeIndex(0), NodeIndex(0));
  const auto startend2 = std::make_pair(NodeIndex(1), NodeIndex(1));
  const auto startend3 = std::make_pair(NodeIndex(1), NodeIndex(1));
  IndexManager manager(kNumNodes, kNumVehicles,
                       {startend1, startend2, startend3});
  Model model(manager);

  const std::vector<std::vector<int64_t>> costs = {
      {0, 135, 10, 60, 60, 10}, {135, 0, 125, 75, 75, 125},
      {10, 125, 0, 50, 55, 5},  {60, 75, 50, 0, 5, 55},
      {60, 75, 55, 5, 0, 50},   {10, 125, 5, 55, 50, 0}};
  const int vehicle_cost = model.RegisterTransitCallback(
      absl::bind_front(GetCostFromMatrix, manager, costs));
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

  const Assignment* const solution = model.SolveFromAssignmentWithParameters(
      BuildAssignmentFromSolution(model, initial_solution), parameters);
  ASSERT_NE(solution, nullptr);

  EXPECT_EQ(solution->ObjectiveValue(), 125);
  ExpectEqualRoutes(model, expected_result, *solution);
}

// The following test shows an example where the solution can only be improved
// with RelocateExpensiveChain with num_arcs_to_consider set to 4.
//
//                                                     ----- Node2
//                                                    /           |
//                                                   /            |
//    -- 10 -- Node6 ---- 10 ---- Node3 -----------95             25
//   |           |                  |                             |
// DEPOT0        5                  5         DEPOT1 ----- 75 -----
//   |           |                  |                             |
//    -- 10 -- Node7 ---- 10 ---- Node4 -----------95             25
//                                                   \            |
//                                                    \           |
//                                                     ----- Node5
//
// In this example with two vehicles with a capacity of 4 and 6 nodes to serve,
// the initial solution is DEPOT0->6->7->DEPOT0, and DEPOT1->2->3->4->5->DEPOT1,
// with an overall cost of 25 + 395 = 420.
// The two most expensive arcs on the second route are DEPOT1-->Node2 and
// DEPOT1-->Node5 with a cost of 100. These two arcs can however not be removed
// as the resulting chain (2->3->4->5) cannot fit on the first route.
// With RelocateExpensiveChain, this solution can be improved by considering the
// 4 most expensive arcs on the route, therefore allowing to remove 2-->3 and
// 4-->5 and placing the chain 3->4 on the first vehicle, resulting in the
// routes DEPOT0->6->3->4->7->DEPOT0 and DEPOT1->2->5->DEPOT1, with a lower cost
// of 45 + 250 = 295.
TEST(RelocateExpensiveChainOperatorInModelTest, ConsiderFourArcs) {
  const std::vector<int> initial_solution = {6, 2, 3, 4, 5, 9, 7, 8};
  const std::vector<int> expected_result_with_rec4 = {6, 2, 5, 4, 7, 9, 3, 8};
  for (int num_arcs_to_consider : {0, 2, 3, 4}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    RoutingSearchParameters::LocalSearchNeighborhoodOperators* operators =
        parameters.mutable_local_search_operators();
    if (num_arcs_to_consider >= 2) {
      DisableAllLocalSearchOperators(operators);
      operators->set_use_relocate_expensive_chain(BOOL_TRUE);
      parameters.set_relocate_expensive_chain_num_arcs_to_consider(
          num_arcs_to_consider);
    } else {
      operators->set_use_relocate_expensive_chain(BOOL_FALSE);
      // Also disable heuristic LNS operators.
      DisableHeuristicLNSOperators(operators);
    }

    const int kNumNodes = 8;
    const int kNumVehicles = 2;
    const auto startend1 = std::make_pair(NodeIndex(0), NodeIndex(0));
    const auto startend2 = std::make_pair(NodeIndex(1), NodeIndex(1));
    IndexManager manager(kNumNodes, kNumVehicles, {startend1, startend2});
    Model model(manager);

    const std::vector<std::vector<int64_t>> costs = {
        {0, 1000, 120, 20, 20, 120, 10, 10},
        {1000, 0, 100, 150, 150, 100, 160, 160},
        {120, 100, 0, 95, 100, 50, 110, 115},
        {20, 150, 95, 0, 5, 100, 10, 15},
        {20, 150, 100, 5, 0, 95, 15, 10},
        {120, 100, 50, 100, 95, 0, 115, 110},
        {10, 150, 110, 10, 15, 115, 0, 5},
        {10, 150, 115, 15, 10, 110, 5, 0}};
    const int vehicle_cost = model.RegisterTransitCallback(
        absl::bind_front(GetCostFromMatrix, manager, costs));
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    model.AddConstantDimension(1, 5, true, "capacity");

    const Assignment* const solution = model.SolveFromAssignmentWithParameters(
        BuildAssignmentFromSolution(model, initial_solution), parameters);
    ASSERT_NE(solution, nullptr);

    if (num_arcs_to_consider == 4) {
      EXPECT_EQ(solution->ObjectiveValue(), 295);
      ExpectEqualRoutes(model, expected_result_with_rec4, *solution);
    } else {
      EXPECT_EQ(solution->ObjectiveValue(), 420);
      ExpectEqualRoutes(model, initial_solution, *solution);
    }
  }
}

struct PairRelocateTestParameters {
  int pairs;
  int vehicles;
  std::vector<int> fixed_costs;
  std::vector<int> initial_solution;
  std::vector<int> target_solution;
  int accepted_moves;
};

using NodePairSet = absl::flat_hash_set<std::pair<int, int>>;

int64_t ReturnZeroIfInTarget(const NodePairSet& target, NodeIndex i,
                             NodeIndex j) {
  return (target.count({i.value(), j.value()}) == 0) ? 1 : 0;
}

void TestPairRelocateOperator(PairRelocateTestParameters params) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  DisableAllLocalSearchOperators(parameters.mutable_local_search_operators());
  parameters.mutable_local_search_operators()->set_use_relocate_pair(BOOL_TRUE);
  parameters.set_log_search(true);
  IndexManager manager(params.pairs * 2 + 1, params.vehicles, NodeIndex(0));
  Model model(manager);
  for (int i = 0; i < params.pairs; ++i) {
    model.AddPickupAndDelivery(manager.NodeToIndex(NodeIndex(2 * i + 1)),
                               manager.NodeToIndex(NodeIndex(2 * i + 2)));
  }
  NodePairSet target;
  for (int i = 0; i < params.target_solution.size(); ++i) {
    const int from = model.IsStart(i) ? 0 : i;
    const int to =
        model.IsEnd(params.target_solution[i]) ? 0 : params.target_solution[i];
    target.insert({from, to});
  }
  for (int i = 0; i < params.fixed_costs.size(); ++i) {
    model.SetFixedCostOfVehicle(params.fixed_costs[i], i);
  }
  const int vehicle_cost =
      model.RegisterTransitCallback([target, manager](int64_t i, int64_t j) {
        return ReturnZeroIfInTarget(target, manager.IndexToNode(i),
                                    manager.IndexToNode(j));
      });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  const Assignment* const solution = model.SolveFromAssignmentWithParameters(
      BuildAssignmentFromSolution(model, params.initial_solution), parameters);
  EXPECT_EQ(0, solution->ObjectiveValue());
  ExpectEqualRoutes(model, params.target_solution, *solution);
  EXPECT_EQ(params.accepted_moves, model.solver()->accepted_neighbors());
}

// Testing relocate-pair on model with one route and two pickup and delivery
// pairs which are initially consecutive. The cost function is tweaked such that
// the optimal solution interleaves both pairs.
// Initial solution (cost 3):
// 0 -> 1(p1) -> 2(d1) -> 3(p2) -> 4(d2) -> 5
// Optimal solution (cost 0):
// 0 -> 1(p1) -> 3(p2) -> 2(d1) -> 4(d2) -> 5
TEST(PairRelocateOperatorTest, OneRouteTwoPairsToInterleave) {
  TestPairRelocateOperator({2, 1, {}, {1, 2, 3, 4, 5}, {1, 3, 4, 2, 5}, 1});
}

// Reverse of OneRouteTwoPairsToInterleave.
TEST(PairRelocateOperatorTest, OneRouteTwoInterleavedPairs) {
  TestPairRelocateOperator({2, 1, {}, {1, 3, 4, 2, 5}, {1, 2, 3, 4, 5}, 1});
}

// Testing relocate-pair on model with two routes and two pickup and delivery
// pairs initially on separate routes. The cost function is tweaked such that
// the optimal solution performs both pairs one after the other on the same
// vehicle.
// Initial solution (cost 2):
// 0 -> 1(p1) -> 2(d1) -> 6
// 5 -> 3(p2) -> 4(d2) -> 7
// Optimal solution (cost 0):
// 0 -> 6
// 5 -> 1(p1) -> 2(d1) -> 3(p2) -> 4(d2) -> 7
TEST(PairRelocateOperatorTest, TwoPairsToSingleRoute) {
  TestPairRelocateOperator(
      {2, 2, {}, {1, 2, 6, 4, 7, 3}, {6, 2, 3, 4, 7, 1}, 1});
}

// Same as TwoPairsToSingleRoute except both pairs get interleaved:
// 0 -> 6
// 5 -> 1(p1) -> 3(p2) -> 2(d1) -> 4(d2) -> 7
TEST(PairRelocateOperatorTest, TwoPairsToSingleInterleavedRoute) {
  TestPairRelocateOperator(
      {2, 2, {}, {1, 2, 6, 4, 7, 3}, {6, 3, 4, 2, 7, 1}, 2});
}

// Reverse of TwoPairsToSingleRoute.
TEST(PairRelocateOperatorTest, TwoPairsToTwoRoutes) {
  TestPairRelocateOperator(
      {2, 2, {}, {6, 2, 3, 4, 7, 1}, {1, 2, 6, 4, 7, 3}, 1});
}

// Reverse of TwoPairsToSingleInterleavedRoute.
TEST(PairRelocateOperatorTest, TwoInterleavedPairsToTwoRoutes) {
  TestPairRelocateOperator(
      {2, 2, {}, {6, 4, 7, 1, 2, 3}, {3, 2, 7, 4, 6, 1}, 1});
}

// Testing relocate-pair on model with two routes and one pickup and delivery
// pair initially on a route with a non-zero fixed cost (the other route has no
// fixed cost). The cost function is tweaked such that the optimal solution
// performs the pair on the route with no fixed cost.
// Initial solution (cost 10):
// 0 -> 4
// 3 -> 1(p1) -> 2(d1) -> 5
// Optimal solution (cost 0):
// 0 -> 1(p1) -> 2(d1) -> 4
// 3 -> 5
TEST(PairRelocateOperatorTest, OneCheapRouteOneExpensiveRouteOnePair) {
  TestPairRelocateOperator({1, 2, {0, 10}, {4, 2, 5, 1}, {1, 2, 4, 5}, 1});
}

// Testing relocate neighborhood on model with a set {1,2,3,4,5} of nodes at the
// same place with precedence constraints such that
// 1 < 2 < 3 < 4 < 5.
// Initial solution is 0 -> 2 -> 5 -> 6 -> 1 -> 3 -> 4  and the optimal
// solution is 0 -> 6 -> 1 -> 2 -> 3 -> 4 -> 5 (forcing merge of 2,5 and 1,3,4).
TEST(RelocateNeighborsOperator, MoveStopNodesWithPrecedence) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.mutable_local_search_operators()->set_use_relocate_neighbors(
      BOOL_TRUE);
  IndexManager manager(7, 1, NodeIndex(0));
  Model model(manager);
  const std::vector<std::vector<int64_t>> costs = {
      {0, 10, 10, 10, 10, 10, 10}, {10, 0, 0, 0, 0, 0, 10},
      {10, 0, 0, 0, 0, 0, 10},     {10, 0, 0, 0, 0, 0, 10},
      {10, 0, 0, 0, 0, 0, 10},     {10, 0, 0, 0, 0, 0, 10},
      {10, 10, 10, 10, 10, 10, 10}};
  const int vehicle_cost = model.RegisterTransitCallback(
      absl::bind_front(GetCostFromMatrix, manager, costs));
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  const int64_t kPrecedence[] = {1, 2, 3, 4, 5};
  for (int i = 0; i < std::size(kPrecedence); ++i) {
    for (int j = 0; j < i; ++j) {
      model.NextVar(kPrecedence[i])->RemoveValue(kPrecedence[j]);
    }
  }
  const Assignment* const solution = model.SolveFromAssignmentWithParameters(
      BuildAssignmentFromSolution(model, {2, 3, 5, 4, 7, 6, 1}), parameters);
  EXPECT_EQ(30, solution->ObjectiveValue());
  ExpectEqualRoutes(model, {6, 2, 3, 4, 5, 7, 1}, *solution);
}

// Testing RelocateAndMakeActive neighborhood on model with 2 non-start/end
// nodes and 2 vehicles. Node 1 is unperformed and can only go on vehicle 0.
// Node 2 can go on any vehicle. Both nodes have high penalties if unperformed
// but cannot appear on the same route.
// We force node 2 to be on vehicle 0 in the first solution, node 1 being
// skipped. The neighborhood will make both nodes performed.
TEST(RelocateAndMakeActiveTest, OneUnperformedNode) {
  for (bool use_relocate_and_make_active : {true, false}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    RoutingSearchParameters::LocalSearchNeighborhoodOperators* operators =
        parameters.mutable_local_search_operators();
    operators->set_use_relocate_and_make_active(
        use_relocate_and_make_active ? BOOL_TRUE : BOOL_FALSE);
    DisableHeuristicLNSOperators(operators);
    operators
        ->set_use_relocate_path_global_cheapest_insertion_insert_unperformed(
            BOOL_FALSE);
    IndexManager manager(3, 2, NodeIndex(0));
    Model model(manager);
    model.AddDisjunction(manager.NodesToIndices({NodeIndex(1)}), 10);
    model.AddDisjunction(manager.NodesToIndices({NodeIndex(2)}), 10);
    model.VehicleVar(1)->RemoveValue(1);
    Solver* const solver = model.solver();
    solver->AddConstraint(
        solver->MakeNonEquality(model.VehicleVar(1), model.VehicleVar(2)));
    const Assignment* const solution = model.SolveFromAssignmentWithParameters(
        BuildAssignmentFromSolution(model, {2, 1, 4, 5}), parameters);
    if (use_relocate_and_make_active) {
      EXPECT_EQ(0, solution->ObjectiveValue());
      ExpectEqualRoutes(model, {1, 4, 5, 2}, *solution);
    } else {
      EXPECT_EQ(10, solution->ObjectiveValue());
    }
  }
}

// Testing MakeActiveAndRelocate neighborhood on model with 2 non-start/end
// nodes and 2 vehicles. Node 1 is unperformed with no penalty. Node 2 is on
// vehicle 0 but can go on vehicle 1 (which is cheaper) if preceded by node 1.
// We force node 2 to be on vehicle 0 in the first solution, node 1 being
// skipped. The neighborhood will make both nodes performed on vehicle 1.
TEST(MakeActiveAndRelocateTest, OneUnperformedNode) {
  for (bool use_relocate_and_make_active : {true, false}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.mutable_local_search_operators()
        ->set_use_relocate_and_make_active(
            use_relocate_and_make_active ? BOOL_TRUE : BOOL_FALSE);
    parameters.mutable_local_search_operators()
        ->set_use_global_cheapest_insertion_path_lns(BOOL_FALSE);
    parameters.mutable_local_search_operators()
        ->set_use_local_cheapest_insertion_path_lns(BOOL_FALSE);
    parameters.set_log_search(true);
    IndexManager manager(3, 2, NodeIndex(0));
    Model model(manager);
    model.AddDisjunction(manager.NodesToIndices({NodeIndex(1)}), 0);
    model.AddDisjunction(manager.NodesToIndices({NodeIndex(2)}), 1000);
    model.VehicleVar(1)->RemoveValue(0);
    model.SetFixedCostOfVehicle(100, 0);
    const int vehicle_cost =
        model.RegisterTransitCallback([](int64_t, int64_t) { return 0LL; });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    Solver* const solver = model.solver();
    // VehicleVar(2) == 1 => NextVar(1) == 2
    IntVar* vehicle_2_is_1 = solver->MakeIsEqualCstVar(model.VehicleVar(2), 1);
    IntVar* next_1_is_2 = solver->MakeIsEqualCstVar(model.NextVar(1), 2);
    solver->AddConstraint(solver->MakeLessOrEqual(vehicle_2_is_1, next_1_is_2));
    const Assignment* const solution = model.SolveFromAssignmentWithParameters(
        BuildAssignmentFromSolution(model, {2, 1, 4, 5}), parameters);
    if (use_relocate_and_make_active) {
      EXPECT_EQ(0, solution->ObjectiveValue());
      ExpectEqualRoutes(model, {4, 2, 5, 1}, *solution);
    } else {
      EXPECT_EQ(100, solution->ObjectiveValue());
    }
  }
}

TEST(NodeAlternativesTest, AlternativeChainAtDifferentLocations) {
  for (OptionalBoolean use_shortest_path_swap_active :
       {BOOL_FALSE, BOOL_TRUE}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    RoutingSearchParameters::LocalSearchNeighborhoodOperators* operators =
        parameters.mutable_local_search_operators();
    operators->set_use_shortest_path_swap_active(use_shortest_path_swap_active);
    operators->set_use_shortest_path_two_opt(BOOL_FALSE);
    parameters.set_log_search(true);
    DisableHeuristicLNSOperators(operators);
    IndexManager manager(12, 1, NodeIndex(0));
    Model model(manager);
    model.AddDisjunction(std::vector<int64_t>{1, 2, 3});
    model.AddDisjunction(std::vector<int64_t>{5, 4});
    model.AddDisjunction(std::vector<int64_t>{6, 8, 7});
    model.AddDisjunction(std::vector<int64_t>{9, 10, 11});
    const int vehicle_cost = model.RegisterTransitCallback(
        [](int64_t i, int64_t j) { return (i % 2 == j % 2) ? 0 : 1; });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    const Assignment* const solution = model.SolveFromAssignmentWithParameters(
        BuildAssignmentFromSolution(model,
                                    {2, 1, 4, 3, 7, 5, 6, 11, 8, 9, 10, 12}),
        parameters);
    ASSERT_NE(solution, nullptr);
    if (use_shortest_path_swap_active == BOOL_TRUE) {
      EXPECT_EQ(0, solution->ObjectiveValue());
    } else {
      EXPECT_EQ(2, solution->ObjectiveValue());
    }
  }
}

// Exchange an unperformed pair (2,3) with a single node 1. The final solution
// is cheaper due to penalties.
TEST(NodePairSwapActiveTest, UnperformedPairPerformedNode) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  RoutingSearchParameters::LocalSearchNeighborhoodOperators* operators =
      parameters.mutable_local_search_operators();
  operators->set_use_node_pair_swap_active(BOOL_TRUE);
  operators->set_use_make_active(BOOL_FALSE);
  DisableHeuristicLNSOperators(operators);
  IndexManager manager(4, 1, NodeIndex(0));
  Model model(manager);
  model.AddPickupAndDelivery(manager.NodeToIndex(NodeIndex(2)),
                             manager.NodeToIndex(NodeIndex(3)));
  for (NodeIndex i(1); i < NodeIndex(4); ++i) {
    model.AddDisjunction(manager.NodesToIndices({i}), 10);
  }

  const Assignment* const solution = model.SolveFromAssignmentWithParameters(
      BuildAssignmentFromSolution(model, {1, 4, 2, 3}), parameters);
  EXPECT_EQ(10, solution->ObjectiveValue());
  ExpectEqualRoutes(model, {2, 1, 3, 4}, *solution);
}

int64_t ReturnValueExceptPair(int64_t value, NodeIndex exception_i,
                              NodeIndex exception_j, int64_t exception_value,
                              NodeIndex i, NodeIndex j) {
  return (i != exception_i || j != exception_j) ? value : exception_value;
}

// Make a pair of nodes on a route inactive: node penalties are zero, arc costs
// are 1, so making the pair inactive will improve the cost.
TEST(PairInactiveTest, MakePairInactive) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.mutable_local_search_operators()->set_use_make_inactive(BOOL_TRUE);
  IndexManager manager(3, 1, NodeIndex(0));

  const bool kConsiderEmptyVehicleCosts[] = {true, false};
  const int kExpectedCost[] = {1, 0};

  for (int c = 0; c < std::size(kConsiderEmptyVehicleCosts); c++) {
    Model model(manager);
    const int vehicle_cost =
        model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
    model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

    SetConsiderEmptyRouteCostsForAllVehicles(&model,
                                             kConsiderEmptyVehicleCosts[c]);
    model.AddPickupAndDelivery(manager.NodeToIndex(NodeIndex(1)),
                               manager.NodeToIndex(NodeIndex(2)));
    model.AddDisjunction(manager.NodesToIndices({NodeIndex(1)}), 0);
    model.AddDisjunction(manager.NodesToIndices({NodeIndex(2)}), 0);
    const Assignment* const solution = model.SolveFromAssignmentWithParameters(
        BuildAssignmentFromSolution(model, {1, 2, 3}), parameters);
    EXPECT_EQ(kExpectedCost[c], solution->ObjectiveValue());
    ExpectEqualRoutes(model, {3, 1, 2}, *solution);
  }
}

// Check that if no pickup and delivery pairs are created nothing happens.
// The test prevents other operators from making the nodes inactive by
// constraining both nodes to be on the same route (only a pair operator could
// move such nodes).
TEST(PairInactiveTest, NoPairs) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.mutable_local_search_operators()->set_use_make_inactive(BOOL_TRUE);
  IndexManager manager(3, 1, NodeIndex(0));
  Model model(manager);
  const int vehicle_cost =
      model.RegisterTransitCallback([](int64_t, int64_t) { return 2LL; });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

  model.solver()->AddConstraint(
      model.solver()->MakeEquality(model.VehicleVar(1), model.VehicleVar(2)));
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(1)}), 3);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(2)}), 3);
  const Assignment* const solution = model.SolveFromAssignmentWithParameters(
      BuildAssignmentFromSolution(model, {1, 2, 3}), parameters);
  EXPECT_EQ(6, solution->ObjectiveValue());
  ExpectEqualRoutes(model, {1, 2, 3}, *solution);
}

// Exchange an unperformed pair (3,4) with a single node 1. There is another
// node (2) which cannot be made unperformed and which has to be between 3 and
// 4. The final solution is cheaper due to arc costs and penalties.
TEST(NodePairSwapActiveTest, UnperformedPairPerformedNodeFirstInRoute) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  RoutingSearchParameters::LocalSearchNeighborhoodOperators* operators =
      parameters.mutable_local_search_operators();
  operators->set_use_node_pair_swap_active(BOOL_TRUE);
  operators->set_use_make_active(BOOL_FALSE);
  operators->set_use_relocate_and_make_active(BOOL_FALSE);
  DisableHeuristicLNSOperators(operators);

  IndexManager manager(5, 1, NodeIndex(0));
  Model model(manager);
  const int vehicle_cost =
      model.RegisterTransitCallback([&manager](int64_t i, int64_t j) {
        return ReturnValueExceptPair(0, NodeIndex(4), NodeIndex(2), 100,
                                     manager.IndexToNode(i),
                                     manager.IndexToNode(j));
      });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  model.AddPickupAndDelivery(manager.NodeToIndex(NodeIndex(3)),
                             manager.NodeToIndex(NodeIndex(4)));
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(1)}), 10);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(2)}), 1000);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(3)}), 10);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(4)}), 10);

  const Assignment* const solution = model.SolveFromAssignmentWithParameters(
      BuildAssignmentFromSolution(model, {1, 2, 5, 3, 4}), parameters);
  EXPECT_EQ(10, solution->ObjectiveValue());
  ExpectEqualRoutes(model, {3, 1, 4, 2, 5}, *solution);
}

// Exchange an unperformed pair (3,4) with a single node 2. There is another
// node (1) which cannot be made unperformed and which has to be between 3 and
// 4. The final solution is cheaper due to arc costs and penalties.
TEST(NodePairSwapActiveTest, UnperformedPairPerformedNodeSecondInRoute) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  // The default LNS time limit is 0.1s, we bump it if running under {A,M,T}SAN.
  if (kAnyXsanEnabled) parameters.mutable_lns_time_limit()->set_seconds(1);
  RoutingSearchParameters::LocalSearchNeighborhoodOperators* operators =
      parameters.mutable_local_search_operators();
  operators->set_use_node_pair_swap_active(BOOL_TRUE);
  operators->set_use_make_active(BOOL_FALSE);
  operators->set_use_relocate_and_make_active(BOOL_FALSE);
  DisableHeuristicLNSOperators(operators);
  IndexManager manager(5, 1, NodeIndex(0));
  Model model(manager);
  const int vehicle_cost =
      model.RegisterTransitCallback([&manager](int64_t i, int64_t j) {
        return ReturnValueExceptPair(0, NodeIndex(1), NodeIndex(3), 100,
                                     manager.IndexToNode(i),
                                     manager.IndexToNode(j));
      });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  model.AddPickupAndDelivery(manager.NodeToIndex(NodeIndex(3)),
                             manager.NodeToIndex(NodeIndex(4)));
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(1)}), 1000);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(2)}), 10);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(3)}), 10);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(4)}), 10);

  const Assignment* const solution = model.SolveFromAssignmentWithParameters(
      BuildAssignmentFromSolution(model, {1, 2, 5, 3, 4}), parameters);
  EXPECT_EQ(10, solution->ObjectiveValue());
  ExpectEqualRoutes(model, {3, 4, 2, 1, 5}, *solution);
}

// Make single node (1) performed in model with a pickup and delivery pair
// (2,3). The final solution is cheaper due to penalties.
TEST(NodeActiveTest, UnperformedNodeWithPair) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  IndexManager manager(4, 1, NodeIndex(0));
  Model model(manager);
  model.AddPickupAndDelivery(manager.NodeToIndex(NodeIndex(2)),
                             manager.NodeToIndex(NodeIndex(3)));
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(1)}), 10);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(2)}), 10);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(3)}), 10);

  const Assignment* const solution = model.SolveFromAssignmentWithParameters(
      BuildAssignmentFromSolution(model, {2, 1, 3, 4}), parameters);
  EXPECT_EQ(0, solution->ObjectiveValue());
  ExpectEqualRoutes(model, {1, 2, 3, 4}, *solution);
}

int64_t ReturnValueExcept(int64_t value, NodeIndex exception_index,
                          int64_t exception_value, NodeIndex i,
                          NodeIndex /*j*/) {
  return (i != exception_index) ? value : exception_value;
}

// Exchange an unperformed node 1 with a pair of nodes (2,3). The final solution
// is cheaper due to arc costs.
TEST(NodePairSwapActiveTest, UnperformedNodePerformedPair) {
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.mutable_local_search_operators()->set_use_node_pair_swap_active(
      BOOL_TRUE);
  IndexManager manager(4, 1, NodeIndex(0));
  Model model(manager);
  const int vehicle_cost =
      model.RegisterTransitCallback([&manager](int64_t i, int64_t j) {
        return ReturnValueExcept(0, NodeIndex(2), 100, manager.IndexToNode(i),
                                 manager.IndexToNode(j));
      });
  model.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  model.AddPickupAndDelivery(manager.NodeToIndex(NodeIndex(2)),
                             manager.NodeToIndex(NodeIndex(3)));
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(1), NodeIndex(2)}), 1);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(3)}), 1);

  const Assignment* const solution = model.SolveFromAssignmentWithParameters(
      BuildAssignmentFromSolution(model, {2, 1, 3, 4}), parameters);
  EXPECT_EQ(1, solution->ObjectiveValue());
  ExpectEqualRoutes(model, {1, 4, 2, 3}, *solution);
}

TEST(ShortestPathOnAlternativesTest, NoAlternatives) {
  ShortestPathOnAlternatives shortest_path_manager(
      /*num_nodes=*/3, /*alternative_sets=*/{},
      /*arc_evaluator=*/[](int64_t i, int64_t) { return i; });
  EXPECT_FALSE(shortest_path_manager.HasAlternatives(0));
  EXPECT_FALSE(shortest_path_manager.HasAlternatives(1));
  EXPECT_FALSE(shortest_path_manager.HasAlternatives(2));
  EXPECT_THAT(shortest_path_manager.GetShortestPath(
                  /*source=*/0, /*sink=*/1, /*chain=*/{}),
              ::testing::ElementsAre());
  EXPECT_THAT(shortest_path_manager.GetShortestPath(
                  /*source=*/0, /*sink=*/2, /*chain=*/{1}),
              ::testing::ElementsAre(1));
}

TEST(ShortestPathOnAlternativesTest, WithOneAlternative) {
  ShortestPathOnAlternatives shortest_path_manager(
      /*num_nodes=*/4, /*alternative_sets=*/{{1, 2}, {3}},
      // Prefer low index nodes
      /*arc_evaluator=*/[](int64_t i, int64_t) { return i; });
  EXPECT_FALSE(shortest_path_manager.HasAlternatives(0));
  EXPECT_TRUE(shortest_path_manager.HasAlternatives(1));
  EXPECT_TRUE(shortest_path_manager.HasAlternatives(2));
  EXPECT_FALSE(shortest_path_manager.HasAlternatives(3));
  EXPECT_THAT(shortest_path_manager.GetShortestPath(
                  /*source=*/0, /*sink=*/1, /*chain=*/{}),
              ::testing::ElementsAre());
  EXPECT_THAT(shortest_path_manager.GetShortestPath(
                  /*source=*/0, /*sink=*/3, /*chain=*/{1}),
              ::testing::ElementsAre(1));
  EXPECT_THAT(shortest_path_manager.GetShortestPath(
                  /*source=*/0, /*sink=*/3, /*chain=*/{2}),
              ::testing::ElementsAre(1));
}

TEST(ShortestPathOnAlternativesTest, WithAlternatives) {
  ShortestPathOnAlternatives shortest_path_manager(
      /*num_nodes=*/6, /*alternative_sets=*/{{1, 3}, {2, 4}},
      /*arc_evaluator=*/[](int64_t i, int64_t j) { return std::abs(i - j); });
  EXPECT_FALSE(shortest_path_manager.HasAlternatives(0));
  EXPECT_TRUE(shortest_path_manager.HasAlternatives(1));
  EXPECT_TRUE(shortest_path_manager.HasAlternatives(2));
  EXPECT_TRUE(shortest_path_manager.HasAlternatives(3));
  EXPECT_TRUE(shortest_path_manager.HasAlternatives(4));
  EXPECT_FALSE(shortest_path_manager.HasAlternatives(5));
  EXPECT_THAT(shortest_path_manager.GetShortestPath(
                  /*source=*/0, /*sink=*/1, /*chain=*/{}),
              ::testing::ElementsAre());
  EXPECT_THAT(shortest_path_manager.GetShortestPath(
                  /*source=*/0, /*sink=*/5, /*chain=*/{1, 2}),
              ::testing::ElementsAre(3, 4));
  EXPECT_THAT(shortest_path_manager.GetShortestPath(
                  /*source=*/0, /*sink=*/5, /*chain=*/{3, 4}),
              ::testing::ElementsAre(3, 4));
  EXPECT_THAT(shortest_path_manager.GetShortestPath(
                  /*source=*/0, /*sink=*/5, /*chain=*/{1, 4}),
              ::testing::ElementsAre(3, 4));
  EXPECT_THAT(shortest_path_manager.GetShortestPath(
                  /*source=*/0, /*sink=*/5, /*chain=*/{3, 2}),
              ::testing::ElementsAre(3, 4));
}

}  // namespace
}  // namespace operations_research::routing
