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

#include "ortools/sat/2d_packing_brute_force.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer_base.h"

namespace operations_research {
namespace sat {
namespace {

std::vector<Rectangle> SolveOrthogonalPacking(
    absl::Span<const IntegerValue> sizes_x,
    absl::Span<const IntegerValue> sizes_y,
    std::pair<IntegerValue, IntegerValue> bounding_box_size) {
  const int num_items = sizes_x.size();
  CHECK_EQ(num_items, sizes_y.size());
  CHECK_GT(bounding_box_size.first, 0);
  CHECK_GT(bounding_box_size.second, 0);
  CpModelBuilder cp_model;
  NoOverlap2DConstraint no_overlap_2d = cp_model.AddNoOverlap2D();
  std::vector<IntVar> start_x_vars;
  std::vector<IntVar> start_y_vars;
  for (int item = 0; item < num_items; ++item) {
    IntVar start_x = cp_model.NewIntVar(
        {0, bounding_box_size.first.value() - sizes_x[item].value()});
    IntVar start_y = cp_model.NewIntVar(
        {0, bounding_box_size.second.value() - sizes_y[item].value()});
    start_x_vars.push_back(start_x);
    start_y_vars.push_back(start_y);

    IntervalVar interval_x =
        cp_model.NewFixedSizeIntervalVar(start_x, sizes_x[item].value());
    IntervalVar interval_y =
        cp_model.NewFixedSizeIntervalVar(start_y, sizes_y[item].value());

    no_overlap_2d.AddRectangle(interval_x, interval_y);
  }
  SatParameters parameters;
  // Disable the propagator so we don't use the code we want to test.
  parameters.set_use_area_energetic_reasoning_in_no_overlap_2d(false);
  const CpSolverResponse response =
      SolveWithParameters(cp_model.Build(), parameters);
  if (response.status() != CpSolverStatus::OPTIMAL) {
    return {};
  }
  std::vector<Rectangle> solution;
  for (int i = 0; i < num_items; ++i) {
    const IntegerValue start_x =
        SolutionIntegerValue(response, start_x_vars[i]);
    const IntegerValue start_y =
        SolutionIntegerValue(response, start_y_vars[i]);
    solution.push_back({.x_min = start_x,
                        .x_max = start_x + sizes_x[i],
                        .y_min = start_y,
                        .y_max = start_y + sizes_y[i]});
  }
  return solution;
}

bool CumulativeIsFeasible(
    absl::Span<const IntegerValue> sizes_x,
    absl::Span<const IntegerValue> sizes_y,
    std::pair<IntegerValue, IntegerValue> bounding_box_size, bool both_sides) {
  const int num_items = sizes_x.size();
  CHECK_EQ(num_items, sizes_y.size());
  CHECK_GT(bounding_box_size.first, 0);
  CHECK_GT(bounding_box_size.second, 0);

  CpModelBuilder cp_model;
  CumulativeConstraint cumulative =
      cp_model.AddCumulative(bounding_box_size.second.value());

  for (int item = 0; item < num_items; ++item) {
    const IntVar start_time = cp_model.NewIntVar(
        {0, bounding_box_size.first.value() - sizes_x[item].value()});
    const IntervalVar start_time_interval_x =
        cp_model.NewFixedSizeIntervalVar(start_time, sizes_x[item].value());
    cumulative.AddDemand(start_time_interval_x, sizes_y[item].value());
  }
  if (both_sides) {
    CumulativeConstraint cumulative_y =
        cp_model.AddCumulative(bounding_box_size.first.value());
    for (int item = 0; item < num_items; ++item) {
      const IntVar start_time = cp_model.NewIntVar(
          {0, bounding_box_size.second.value() - sizes_y[item].value()});
      const IntervalVar start_time_interval_y =
          cp_model.NewFixedSizeIntervalVar(start_time, sizes_y[item].value());
      cumulative_y.AddDemand(start_time_interval_y, sizes_x[item].value());
    }
  }
  const CpSolverResponse response = Solve(cp_model.Build());
  return (response.status() == CpSolverStatus::OPTIMAL);
}

struct OppProblem {
  std::vector<IntegerValue> items_x_sizes;
  std::vector<IntegerValue> items_y_sizes;
  std::pair<IntegerValue, IntegerValue> bb_sizes;
};

OppProblem CreateRandomOppProblem(absl::BitGenRef random, int num_items) {
  OppProblem result;
  std::vector<IntegerValue>& items_x_sizes = result.items_x_sizes;
  std::vector<IntegerValue>& items_y_sizes = result.items_y_sizes;
  const int size = 300;
  items_x_sizes.clear();
  items_y_sizes.clear();
  IntegerValue area = 0;
  for (int i = 0; i < num_items; ++i) {
    const IntegerValue x_size = absl::Uniform(random, 1, size);
    const IntegerValue y_size = absl::Uniform(random, 1, size);
    items_x_sizes.push_back(x_size);
    items_y_sizes.push_back(y_size);
    area += x_size * y_size;
  }
  const IntegerValue box_x_size =
      absl::Uniform(random, size, static_cast<int64_t>(sqrt(num_items) * size));
  const IntegerValue box_y_size =
      std::max(IntegerValue(size), (area + box_x_size - 1) / box_x_size);
  result.bb_sizes = {box_x_size, box_y_size};
  return result;
}

void CheckSolution(const OppProblem& problem,
                   absl::Span<const Rectangle> solution) {
  CHECK_EQ(problem.items_x_sizes.size(), solution.size());
  for (const Rectangle& item : solution) {
    CHECK_GE(item.x_min, 0);
    CHECK_LE(item.x_max, problem.bb_sizes.first);
    CHECK_GE(item.y_min, 0);
    CHECK_LE(item.y_max, problem.bb_sizes.second);
  }

  for (int i = 0; i < problem.items_x_sizes.size(); ++i) {
    CHECK_EQ(problem.items_x_sizes[i], solution[i].SizeX());
    CHECK_EQ(problem.items_y_sizes[i], solution[i].SizeY());

    for (int j = i + 1; j < problem.items_x_sizes.size(); ++j) {
      CHECK(solution[i].IsDisjoint(solution[j]))
          << " for solution: "
          << RenderDot(Rectangle{.x_min = 0,
                                 .x_max = problem.bb_sizes.first,
                                 .y_min = 0,
                                 .y_max = problem.bb_sizes.second},
                       solution);
    }
  }
}

TEST(CheckSolutionTest, CheckWithCPSat) {
  EXPECT_TRUE(
      SolveOrthogonalPacking({4, 4, 8, 8}, {6, 6, 5, 5}, {13, 10}).empty());
  EXPECT_FALSE(
      SolveOrthogonalPacking({4, 4, 8, 8}, {6, 6, 5, 5}, {12, 12}).empty());

  absl::BitGen random;
  int feasible = 0;
  for (int i = 0; i < 1000; ++i) {
    const OppProblem problem = CreateRandomOppProblem(random, 6);
    const auto brute_force_solution = BruteForceOrthogonalPacking(
        problem.items_x_sizes, problem.items_y_sizes, problem.bb_sizes,
        problem.items_x_sizes.size());
    CHECK(brute_force_solution.status != BruteForceResult::Status::kTooBig);
    feasible +=
        brute_force_solution.status == BruteForceResult::Status::kFoundSolution;
    if (brute_force_solution.status ==
        BruteForceResult::Status::kFoundSolution) {
      CheckSolution(problem, brute_force_solution.positions_for_solution);
    }
    const auto solution = SolveOrthogonalPacking(
        problem.items_x_sizes, problem.items_y_sizes, problem.bb_sizes);
    if (!solution.empty()) {
      CheckSolution(problem, solution);
    }
    EXPECT_EQ(brute_force_solution.status ==
                  BruteForceResult::Status::kNoSolutionExists,
              solution.empty());
  }
  std::cout << "feasible: " << feasible << "\n";
}

// Example that is feasible for the cumulative in one dimension but not for 2d
// packing:
//
// digraph {
//   graph [ bgcolor=lightgray width=18 height=12]
//   node [style=filled]
//   bb [fillcolor="grey" pos="9,6!" shape=box width=18 height=12]
//   0 [fillcolor="green" pos="5,2!" shape=box width=10 height=4]
//   1 [fillcolor="purple" pos="13,3!" shape=box width=6 height=6]
//   2 [fillcolor="red" pos="17,4!" shape=box width=2 height=8]
//   3 [fillcolor="blue" pos="13,10!" shape=box width=10 height=4]
//   4 [fillcolor="yellow" pos="5,9!" shape=box width=6 height=6]
//   5 [fillcolor="cyan" pos="1,8!" shape=box width=2 height=8]
//   6 [fillcolor="red" pos="7,5!" shape=box width=10 height=2]
// }
//
// +--------------------------------------------------------+
// |****** @@@@@@@@@@@@@@@@@@ xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|
// |****** @@@@@@@@@@@@@@@@@@ xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|
// |****** @@@@@@@@@@@@@@@@@@ xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|
// |****** @@@@@@@@@@@@@@@@@@ xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|
// |****** @@@@@@@@@@@@@@@@@@ xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|
// |****** @@@@@@@@@@@@@@@@@@ xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|
// |****** @@@@@@@@@@@@@@@@@@      ++++++             ......|
// |****** @@@@@@@@@@@@@@@@@@      ++++++             ......|
// |****** @@@@@@@@@@@@@@@@@@      ++++++             ......|
// |****** ++++++++++++++++++++++++"""""""""""""""""" ......|  <-----
// |****** ++++++++++++++++++++++++"""""""""""""""""" ......|
// |****** ++++++++++++++++++++++++"""""""""""""""""" ......|
// |000000000000000000000000000000 """""""""""""""""" ......|
// |000000000000000000000000000000 """""""""""""""""" ......|
// |000000000000000000000000000000 """""""""""""""""" ......|
// |000000000000000000000000000000 """""""""""""""""" ......|
// |000000000000000000000000000000 """""""""""""""""" ......|
// |000000000000000000000000000000 """""""""""""""""" ......|
// +--------------------------------------------------------+
//                                 ^
//                                 |
TEST(CheckSolutionTest, CumulativeFeasiblePackingInfeasibleExample) {
  std::vector<IntegerValue> items_x_sizes = {5, 3, 1, 5, 3, 1, 5};
  std::vector<IntegerValue> items_y_sizes = {2, 3, 4, 2, 3, 4, 1};
  std::pair<IntegerValue, IntegerValue> bb_sizes = {9, 6};

  EXPECT_TRUE(
      SolveOrthogonalPacking(items_x_sizes, items_y_sizes, bb_sizes).empty());
  EXPECT_TRUE(
      CumulativeIsFeasible(items_x_sizes, items_y_sizes, bb_sizes, false));
  // Note that it is infeasible if we take the cumulative on the y though:
  EXPECT_FALSE(CumulativeIsFeasible(items_y_sizes, items_x_sizes,
                                    {bb_sizes.second, bb_sizes.first}, false));
}

// Example that is feasible for the cumulative in both dimensions but not for 2d
// packing.
//
// digraph {
//   graph [ bgcolor=lightgray width=30 height=30]
//   node [style=filled]
//   bb [fillcolor="grey" pos="15,15!" shape=box width=30 height=30]
//   0 [fillcolor="red" pos="29,17!" shape=box width=2 height=26]
//   1 [fillcolor="green" pos="22,10!" shape=box width=4 height=12]
//   2 [fillcolor="blue" pos="26,14!" shape=box width=4 height=20]
//   3 [fillcolor="cyan" pos="10,7!" shape=box width=20 height=14]
//   4 [fillcolor="yellow" pos="15,19!" shape=box width=14 height=10]
//   5 [fillcolor="purple" pos="26,2!" shape=box width=8 height=4]
//   6 [fillcolor="red" pos="4,23!" shape=box width=8 height=14]
//   7 [fillcolor="green" pos="18,27!" shape=box width=20 height=6]
// }
TEST(CheckSolutionTest, CumulativeFeasiblePackingInfeasibleBothExample) {
  std::vector<IntegerValue> items_x_sizes = {1, 2, 2, 10, 7, 4, 4, 10};
  std::vector<IntegerValue> items_y_sizes = {13, 6, 10, 7, 5, 2, 7, 3};
  std::pair<IntegerValue, IntegerValue> bb_sizes = {15, 15};
  EXPECT_TRUE(
      SolveOrthogonalPacking(items_x_sizes, items_y_sizes, bb_sizes).empty());
  EXPECT_TRUE(
      CumulativeIsFeasible(items_x_sizes, items_y_sizes, bb_sizes, true));
}

MATCHER_P(FieldEq, field, "") {
  return testing::Matches(testing::Field(field, ::testing::get<1>(arg)))(
      ::testing::get<0>(arg));
}

TEST(TestPreprocessing, Works) {
  std::vector<IntegerValue> bb_sizes = {100, 40};
  OppProblem problem = {.items_x_sizes = {95, 93, 90, 5, 7, 8, 20, 30, 20, 20},
                        .items_y_sizes = {5, 5, 6, 8, 2, 4, 10, 10, 10, 10},
                        .bb_sizes = {100, 40}};
  std::vector<PermutableItem> items;
  for (int i = 0; i < problem.items_x_sizes.size(); ++i) {
    items.push_back({.size_x = problem.items_x_sizes[i],
                     .size_y = problem.items_y_sizes[i]});
  }
  absl::Span<PermutableItem> preprocessed_items = absl::MakeSpan(items);
  EXPECT_TRUE(Preprocess(preprocessed_items, problem.bb_sizes, 10));
  // We expect that 95x5, 93x5, 90x6, 5x8, 7x2 and 8x4 be removed.
  EXPECT_EQ(preprocessed_items.size(), 4);
  EXPECT_THAT(preprocessed_items,
              testing::UnorderedPointwise(FieldEq(&PermutableItem::size_x),
                                          {20, 30, 20, 20}));
  // Original items don't disappeared, but the order might have been changed.
  EXPECT_THAT(items,
              testing::UnorderedPointwise(FieldEq(&PermutableItem::size_x),
                                          problem.items_x_sizes));
}

void BM_BruteForceOrthogonalPacking(benchmark::State& state) {
  absl::BitGen random;
  static constexpr int kNumProblems = 100;
  std::vector<OppProblem> problems;
  const bool feasible = state.range(1);
  while (problems.size() < kNumProblems) {
    OppProblem problem = CreateRandomOppProblem(random, state.range(0));
    if ((BruteForceOrthogonalPacking(problem.items_x_sizes,
                                     problem.items_y_sizes, problem.bb_sizes,
                                     problem.items_x_sizes.size())
             .status == BruteForceResult::Status::kFoundSolution) == feasible) {
      problems.push_back(problem);
    }
  }
  int index = 0;
  for (auto s : state) {
    const auto& problem = problems[index];
    BruteForceOrthogonalPacking(problem.items_x_sizes, problem.items_y_sizes,
                                problem.bb_sizes, problem.items_x_sizes.size());
    ++index;
    if (index == problems.size()) {
      index = 0;
    }
  }
}

BENCHMARK(BM_BruteForceOrthogonalPacking)
    ->ArgPair(3, false)
    ->ArgPair(4, false)
    ->ArgPair(5, false)
    ->ArgPair(6, false)
    ->ArgPair(7, false)
    ->ArgPair(8, false)
    ->ArgPair(9, false)
    ->ArgPair(3, true)
    ->ArgPair(4, true)
    ->ArgPair(5, true)
    ->ArgPair(6, true)
    ->ArgPair(7, true)
    ->ArgPair(8, true)
    ->ArgPair(9, true);

}  // namespace
}  // namespace sat
}  // namespace operations_research
