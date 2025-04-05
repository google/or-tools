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

#include "ortools/sat/2d_orthogonal_packing.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/algorithms/binary_search.h"
#include "ortools/base/gmock.h"
#include "ortools/sat/2d_orthogonal_packing_testing.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/synchronization.h"

namespace operations_research {
namespace sat {
namespace {

// Alternative way of computing Dff.LowestInverse().
template <typename DffClass>
IntegerValue ComputeDffInverse(const DffClass& dff, IntegerValue max,
                               IntegerValue value) {
  DCHECK_EQ(dff(0), 0);
  if (value <= 0) return 0;
  return BinarySearch<IntegerValue>(
      max, 0, [&dff, value](IntegerValue x) { return dff(x) >= value; });
}

template <typename DffClass, typename... Args>
void TestMaximalDff(absl::BitGenRef random, int64_t num_values_to_check,
                    int64_t max, Args&&... args) {
  DffClass dff(max, std::forward<Args>(args)...);

  num_values_to_check = std::min(num_values_to_check, max);
  const int64_t step_size = max / num_values_to_check;
  for (int64_t i = 0; i < max; i += step_size) {
    int64_t value_to_check;
    if (step_size > 1) {
      value_to_check =
          absl::Uniform(random, i, std::min(i + step_size - 1, max));
    } else {
      value_to_check = i;
    }

    for (int64_t j = 0; j < max; j += step_size) {
      int64_t value_to_check2;
      if (step_size > 1) {
        value_to_check2 =
            absl::Uniform(random, j, std::min(j + step_size - 1, max));
      } else {
        value_to_check2 = j;
      }

      const IntegerValue f = dff(value_to_check);
      const IntegerValue f2 = dff(value_to_check2);
      // f is nondecreasing
      if (f != f2) {
        CHECK_EQ(f < f2, value_to_check < value_to_check2);
      }
      // f is superadditive, i.e., f(x) + f(y) <= f(x + y)
      if (value_to_check + value_to_check2 <= max) {
        CHECK_LE(f + f2, dff(value_to_check + value_to_check2));
      }
      // f is symmetric, i.e., f(x) + f(C - x) = f(C)
      CHECK_EQ(dff(value_to_check) + dff(max - value_to_check), dff(max));
      CHECK_EQ(dff(value_to_check2) + dff(max - value_to_check2), dff(max));

      // Inverse works
      CHECK_EQ(dff(dff.LowestInverse(f)), f);
      // Lowest inverse is indeed the lowest
      CHECK_LE(dff.LowestInverse(f), value_to_check);
      if (dff.LowestInverse(f) > 0) {
        CHECK_NE(dff(dff.LowestInverse(f) - 1), f);
      }

      // Inverse works outside domain
      if (value_to_check <= dff(max)) {
        CHECK_GE(dff(dff.LowestInverse(value_to_check)), value_to_check);
        if (dff.LowestInverse(value_to_check) > 0) {
          CHECK_LT(dff(dff.LowestInverse(value_to_check) - 1), value_to_check);
        }
        CHECK_EQ(dff.LowestInverse(value_to_check),
                 ComputeDffInverse(dff, max, value_to_check));
      }
    }
  }
  // f(0) = 0
  CHECK_EQ(dff(0), 0);
}

TEST(DualFeasibilityFunctionTest, Dff2IsMaximal) {
  absl::BitGen random;
  for (int k = 1; k < 50; k++) {
    TestMaximalDff<RoundingDualFeasibleFunction>(
        random, 100, 100 + absl::Uniform(random, 0, 1), k);
  }
  for (int k = 100; 2 * k <= std::numeric_limits<uint16_t>::max(); k += 200) {
    TestMaximalDff<RoundingDualFeasibleFunction>(random, 200, 2 * k, k);
  }
}

TEST(DualFeasibilityFunctionTest, DffPowerOfTwo) {
  absl::BitGen random;
  for (int k = 0; k < 61; k++) {
    TestMaximalDff<RoundingDualFeasibleFunctionPowerOfTwo>(
        random, 100, std::numeric_limits<int64_t>::max() / 2, k);
  }
}

TEST(DualFeasibilityFunctionTest, Dff0IsMaximal) {
  absl::BitGen random;
  for (int k = 1; k < 50; k++) {
    TestMaximalDff<DualFeasibleFunctionF0>(random, 100, 100, k);
  }
  for (int k = 100; 2 * k <= std::numeric_limits<uint16_t>::max(); k += 200) {
    TestMaximalDff<DualFeasibleFunctionF0>(random, 200, 2 * k, k);
  }
}

TEST(DualFeasibilityFunctionTest, Composed) {
  absl::BitGen random;
  for (int k_f0 = 1; k_f0 < 50; k_f0++) {
    for (int k_f2 = 2; k_f2 < 50; k_f2++) {
      TestMaximalDff<DFFComposedF2F0>(random, 100, 100, k_f0, k_f2);
    }
  }
}

using DetectorStatus = OrthogonalPackingResult::Status;

struct OppProblem {
  std::vector<IntegerValue> items_x_sizes;
  std::vector<IntegerValue> items_y_sizes;
  std::pair<IntegerValue, IntegerValue> bb_sizes;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const OppProblem& p) {
    absl::Format(&sink,
                 "items_x_sizes={%s}, items_y_sizes={%s}, bb_sizes={%i, %i}",
                 absl::StrJoin(p.items_x_sizes, ", "),
                 absl::StrJoin(p.items_y_sizes, ", "), p.bb_sizes.first.value(),
                 p.bb_sizes.second.value());
  }
};

OppProblem CreateFeasibleOppProblem(absl::BitGenRef random, int max_size) {
  std::vector<RectangleInRange> problem = MakeItemsFromRectangles(
      GenerateNonConflictingRectangles(
          absl::Uniform(random, max_size - 1, max_size), random),
      0, random);

  OppProblem result;
  Rectangle bounding_box;
  bounding_box = {.x_min = std::numeric_limits<IntegerValue>::max(),
                  .x_max = std::numeric_limits<IntegerValue>::min(),
                  .y_min = std::numeric_limits<IntegerValue>::max(),
                  .y_max = std::numeric_limits<IntegerValue>::min()};
  std::vector<IntegerValue>& items_x_sizes = result.items_x_sizes;
  std::vector<IntegerValue>& items_y_sizes = result.items_y_sizes;
  for (const auto& item : problem) {
    items_x_sizes.push_back(item.x_size);
    items_y_sizes.push_back(item.y_size);

    bounding_box.x_min = std::min(bounding_box.x_min, item.bounding_area.x_min);
    bounding_box.x_max = std::max(bounding_box.x_max, item.bounding_area.x_max);
    bounding_box.y_min = std::min(bounding_box.y_min, item.bounding_area.y_min);
    bounding_box.y_max = std::max(bounding_box.y_max, item.bounding_area.y_max);
  }
  result.bb_sizes = {bounding_box.SizeX(), bounding_box.SizeY()};
  return result;
}

OppProblem CreateRandomOppProblem(absl::BitGenRef random, int size) {
  OppProblem result;
  std::vector<IntegerValue>& items_x_sizes = result.items_x_sizes;
  std::vector<IntegerValue>& items_y_sizes = result.items_y_sizes;
  const int num_items = absl::Uniform(random, 1, 20);
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
  const IntegerValue box_x_size = absl::Uniform(random, size, 4 * size);
  const IntegerValue box_y_size =
      std::max(IntegerValue(size), (area + box_x_size - 1) / box_x_size);
  result.bb_sizes = {box_x_size, box_y_size};
  return result;
}

TEST(DualFeasibilityTest, NoConflictWhenItDoesNotExist) {
  absl::BitGen random;
  SharedStatistics stats;
  OrthogonalPackingInfeasibilityDetector opp_solver(random, &stats);
  constexpr int num_runs = 400;
  for (int k = 0; k < num_runs; k++) {
    auto problem = CreateFeasibleOppProblem(random, 30);
    CHECK(opp_solver
              .TestFeasibility(problem.items_x_sizes, problem.items_y_sizes,
                               problem.bb_sizes)
              .GetResult() != DetectorStatus::INFEASIBLE)
        << "problem: " << absl::StrCat(problem);
  }
}

DetectorStatus NaiveCheckPairwiseFeasibility(
    absl::Span<const IntegerValue> sizes_x,
    absl::Span<const IntegerValue> sizes_y,
    const std::pair<IntegerValue, IntegerValue>& bounding_box_size) {
  for (int i = 0; i < sizes_x.size(); i++) {
    for (int j = i + 1; j < sizes_x.size(); j++) {
      if (sizes_x[i] + sizes_x[j] > bounding_box_size.first &&
          sizes_y[i] + sizes_y[j] > bounding_box_size.second) {
        return DetectorStatus::INFEASIBLE;
      }
    }
  }
  return DetectorStatus::UNKNOWN;
}

DetectorStatus NaiveCheckFeasibilityWithDualFunction(
    absl::Span<const IntegerValue> sizes_x,
    absl::Span<const IntegerValue> sizes_y,
    const std::pair<IntegerValue, IntegerValue>& bounding_box_size) {
  const IntegerValue bb_area =
      bounding_box_size.first * bounding_box_size.second;
  for (IntegerValue k = 0; 2 * k <= bounding_box_size.first; k++) {
    DualFeasibleFunctionF0 dff0_x(bounding_box_size.first, k);
    for (IntegerValue l = 0; 2 * l <= bounding_box_size.second; l++) {
      DualFeasibleFunctionF0 dff0_y(bounding_box_size.second, l);
      IntegerValue area = 0;
      for (int i = 0; i < sizes_x.size(); i++) {
        area += dff0_x(sizes_x[i]) * dff0_y(sizes_y[i]);
      }
      if (area > bb_area) {
        return DetectorStatus::INFEASIBLE;
      }
    }
  }
  return DetectorStatus::UNKNOWN;
}

TEST(DualFeasibilityTest, Random) {
  absl::BitGen random;
  SharedStatistics stats;
  OrthogonalPackingInfeasibilityDetector opp_solver(random, &stats);
  constexpr int num_runs = 400;
  constexpr int size = 20;
  for (int k = 0; k < num_runs; k++) {
    const OppProblem problem = CreateRandomOppProblem(random, size);
    const auto naive_result = NaiveCheckFeasibilityWithDualFunction(
        problem.items_x_sizes, problem.items_y_sizes, problem.bb_sizes);
    const auto naive_pairwise = NaiveCheckPairwiseFeasibility(
        problem.items_x_sizes, problem.items_y_sizes, problem.bb_sizes);

    const auto result = opp_solver.TestFeasibility(
        problem.items_x_sizes, problem.items_y_sizes, problem.bb_sizes,
        OrthogonalPackingOptions{.use_pairwise = true,
                                 .use_dff_f0 = true,
                                 .use_dff_f2 = false,
                                 .brute_force_threshold = 0});
    CHECK_EQ(result.GetResult() == DetectorStatus::INFEASIBLE,
             naive_result == DetectorStatus::INFEASIBLE ||
                 naive_pairwise == DetectorStatus::INFEASIBLE);
    CHECK_EQ(result.GetItemsParticipatingOnConflict().size() == 2,
             naive_pairwise == DetectorStatus::INFEASIBLE);
  }
}

// Try f_2^k(f_0^l(x)) for all values of k and l.
DetectorStatus NaiveFeasibilityWithDualFunction2(
    absl::Span<const IntegerValue> sizes_x,
    absl::Span<const IntegerValue> sizes_y,
    std::pair<IntegerValue, IntegerValue> bounding_box_size) {
  for (IntegerValue l = 0; 2 * l <= bounding_box_size.first; l++) {
    DualFeasibleFunctionF0 dff0(bounding_box_size.first, l);
    for (IntegerValue k = 1; 2 * k < bounding_box_size.first; k++) {
      const RoundingDualFeasibleFunction dff2(bounding_box_size.first, k);
      const IntegerValue c_k = bounding_box_size.first / k;
      const IntegerValue bb_area = 2 * c_k * bounding_box_size.second;
      IntegerValue area = 0;
      for (int i = 0; i < sizes_x.size(); i++) {
        // First apply f_0
        IntegerValue x_size = dff0(sizes_x[i]);
        // Now apply f_2
        if (2 * x_size > bounding_box_size.first) {
          x_size = 2 * (c_k - (bounding_box_size.first - x_size) / k);
        } else if (2 * x_size == bounding_box_size.first) {
          x_size = c_k;
        } else {
          x_size = 2 * (x_size / k);
        }
        CHECK_EQ(x_size, dff2(dff0(sizes_x[i])));
        IntegerValue y_size = sizes_y[i];
        area += x_size * y_size;
      }
      if (area > bb_area) {
        return DetectorStatus::INFEASIBLE;
      }
    }
  }
  return DetectorStatus::UNKNOWN;
}

TEST(DualFeasibility2Test, Random) {
  absl::BitGen random;
  SharedStatistics stats;
  OrthogonalPackingInfeasibilityDetector opp_solver(random, &stats);
  constexpr int num_runs = 40000;
  const int size = absl::Uniform(random, 10, 30);
  for (int k = 0; k < num_runs; k++) {
    const OppProblem problem = CreateRandomOppProblem(random, size);
    const DetectorStatus naive_result1 = NaiveFeasibilityWithDualFunction2(
        problem.items_x_sizes, problem.items_y_sizes, problem.bb_sizes);
    const DetectorStatus naive_result2 = NaiveFeasibilityWithDualFunction2(
        problem.items_y_sizes, problem.items_x_sizes,
        {problem.bb_sizes.second, problem.bb_sizes.first});

    const auto result = opp_solver.TestFeasibility(
        problem.items_x_sizes, problem.items_y_sizes, problem.bb_sizes,
        OrthogonalPackingOptions{.use_pairwise = false,
                                 .use_dff_f0 = true,
                                 .use_dff_f2 = true,
                                 .brute_force_threshold = 0});
    CHECK_EQ(naive_result1 == DetectorStatus::INFEASIBLE ||
                 naive_result2 == DetectorStatus::INFEASIBLE,
             result.GetResult() == DetectorStatus::INFEASIBLE)
        << " naive result x: " << (naive_result1 == DetectorStatus::INFEASIBLE)
        << " Naive result y: " << (naive_result2 == DetectorStatus::INFEASIBLE)
        << " Result: " << (result.GetResult() == DetectorStatus::INFEASIBLE)
        << " problem:" << absl::StrCat(problem);
  }
}

TEST(OrthogonalPackingTest, SlackDoesNotChangeFeasibility) {
  absl::BitGen random;
  SharedStatistics stats;
  OrthogonalPackingInfeasibilityDetector opp_solver(random, &stats);
  constexpr int num_runs = 400;
  constexpr int size = 20;
  for (int k = 0; k < num_runs; k++) {
    OppProblem problem = CreateRandomOppProblem(random, size);
    // Add one more item since `CreateRandomOppProblem()` does not generate
    // trivially infeasible problems.
    problem.items_x_sizes.push_back(absl::Uniform(random, 1, size));
    problem.items_y_sizes.push_back(absl::Uniform(random, 1, size));
    auto result = opp_solver.TestFeasibility(
        problem.items_x_sizes, problem.items_y_sizes, problem.bb_sizes);
    if (result.GetResult() != DetectorStatus::INFEASIBLE) {
      continue;
    }
    const std::vector<OrthogonalPackingResult::Item>&
        items_participating_on_conflict =
            result.GetItemsParticipatingOnConflict();
    for (int i = 0; i < items_participating_on_conflict.size(); ++i) {
      if (absl::Bernoulli(random, 0.5)) {
        result.TryUseSlackToReduceItemSize(
            i, OrthogonalPackingResult::Coord::kCoordX, 0);
        result.TryUseSlackToReduceItemSize(
            i, OrthogonalPackingResult::Coord::kCoordY, 0);
      } else {
        result.TryUseSlackToReduceItemSize(
            i, OrthogonalPackingResult::Coord::kCoordY, 0);
        result.TryUseSlackToReduceItemSize(
            i, OrthogonalPackingResult::Coord::kCoordX, 0);
      }
    }
    OppProblem modified_problem;
    modified_problem.bb_sizes = problem.bb_sizes;
    for (const auto& item : result.GetItemsParticipatingOnConflict()) {
      modified_problem.items_x_sizes.push_back(item.size_x);
      modified_problem.items_y_sizes.push_back(item.size_y);
    }
    EXPECT_TRUE(opp_solver
                    .TestFeasibility(modified_problem.items_x_sizes,
                                     modified_problem.items_y_sizes,
                                     modified_problem.bb_sizes)
                    .GetResult() == DetectorStatus::INFEASIBLE)
        << "problem: " << absl::StrCat(problem);
  }
}

void BM_OrthogonalPackingInfeasibilityDetector(benchmark::State& state) {
  absl::BitGen random;
  SharedStatistics stats;
  OrthogonalPackingInfeasibilityDetector opp_solver(random, &stats);
  std::vector<OppProblem> problems;
  for (int i = 0; i < 10; ++i) {
    problems.push_back(CreateFeasibleOppProblem(random, state.range(0)));
  }
  int index = 0;
  for (auto s : state) {
    const auto& problem = problems[index];
    CHECK(opp_solver
              .TestFeasibility(problem.items_x_sizes, problem.items_y_sizes,
                               problem.bb_sizes)
              .GetResult() != DetectorStatus::INFEASIBLE);
    ++index;
    if (index == 10) {
      index = 0;
    }
  }
}

BENCHMARK(BM_OrthogonalPackingInfeasibilityDetector)
    ->Arg(5)
    ->Arg(10)
    ->Arg(20)
    ->Arg(30)
    ->Arg(40)
    ->Arg(80)
    ->Arg(100)
    ->Arg(200)
    ->Arg(1000)
    ->Arg(10000);

MATCHER_P3(ItemIs, index, size_x, size_y, "") {
  return arg.index == index && arg.size_x == size_x && arg.size_y == size_y;
}

TEST(OrthogonalPacking, UseSlack) {
  using Item = OrthogonalPackingResult::Item;
  absl::BitGen random;
  SharedStatistics stats;
  OrthogonalPackingInfeasibilityDetector opp_solver(random, &stats);
  auto result = opp_solver.TestFeasibility(
      {10, 10, 10}, {10, 10, 10}, {11, 20},
      OrthogonalPackingOptions{// Detect only trivial conflicts
                               .use_pairwise = false,
                               .use_dff_f0 = false,
                               .use_dff_f2 = false,
                               .brute_force_threshold = 0});
  CHECK(result.GetResult() == DetectorStatus::INFEASIBLE);
  EXPECT_THAT(result.GetItemsParticipatingOnConflict(),
              testing::UnorderedElementsAre(
                  ItemIs(0, 10, 10), ItemIs(1, 10, 10), ItemIs(2, 10, 10)));

  int position_of_index_zero =
      std::find_if(result.GetItemsParticipatingOnConflict().begin(),
                   result.GetItemsParticipatingOnConflict().end(),
                   [](const Item& item) { return item.index == 0; }) -
      result.GetItemsParticipatingOnConflict().begin();

  result.TryUseSlackToReduceItemSize(
      position_of_index_zero, OrthogonalPackingResult::Coord::kCoordX, 9);
  EXPECT_THAT(result.GetItemsParticipatingOnConflict(),
              testing::Contains(ItemIs(0, 9, 10)));
  result.TryUseSlackToReduceItemSize(position_of_index_zero,
                                     OrthogonalPackingResult::Coord::kCoordX);
  // 2*10 + 10*10 + 10*10 = 220 = 11 * 20. So (2, 10) would fit.
  EXPECT_THAT(result.GetItemsParticipatingOnConflict(),
              testing::Contains(ItemIs(0, 3, 10)));

  auto result2 = opp_solver.TestFeasibility(
      {10, 10, 10}, {11, 10, 10}, {11, 20},
      OrthogonalPackingOptions{// Detect only trivial conflicts
                               .use_pairwise = false,
                               .use_dff_f0 = false,
                               .use_dff_f2 = false,
                               .brute_force_threshold = 0});
  position_of_index_zero =
      std::find_if(result2.GetItemsParticipatingOnConflict().begin(),
                   result2.GetItemsParticipatingOnConflict().end(),
                   [](const Item& item) { return item.index == 0; }) -
      result2.GetItemsParticipatingOnConflict().begin();
  result2.TryUseSlackToReduceItemSize(position_of_index_zero,
                                      OrthogonalPackingResult::Coord::kCoordX);
  // 2*11 + 10*10 + 10*10 = 222 > 11 * 20 = 220. So (2, 11) does not fit.
  EXPECT_THAT(result2.GetItemsParticipatingOnConflict(),
              testing::Contains(ItemIs(0, 2, 11)));
}

TEST(OrthogonalPacking, InvertDFF) {
  absl::BitGen random;
  SharedStatistics stats;
  OrthogonalPackingInfeasibilityDetector opp_solver(random, &stats);
  auto result = opp_solver.TestFeasibility(
      {4, 4, 8, 8}, {6, 6, 5, 5}, {13, 10},
      OrthogonalPackingOptions{.use_pairwise = false,
                               .use_dff_f0 = false,
                               .use_dff_f2 = true,
                               .brute_force_threshold = 0});
  CHECK(result.GetResult() == DetectorStatus::INFEASIBLE);
  EXPECT_THAT(result.GetItemsParticipatingOnConflict(),
              testing::UnorderedElementsAre(ItemIs(0, 3, 6), ItemIs(1, 3, 6),
                                            ItemIs(2, 8, 5), ItemIs(3, 8, 5)));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
