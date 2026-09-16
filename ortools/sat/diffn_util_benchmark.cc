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

#include <cstddef>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "ortools/sat/2d_orthogonal_packing_testing.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer_base.h"

namespace operations_research {
namespace sat {
namespace {

void BM_FindRectangles(benchmark::State& state) {
  absl::BitGen random;
  std::vector<std::vector<RectangleInRange>> problems;
  static constexpr int kNumProblems = 20;
  for (int i = 0; i < kNumProblems; i++) {
    problems.push_back(MakeItemsFromRectangles(
        GenerateNonConflictingRectangles(state.range(0), random),
        state.range(1) / 100.0, random));
  }
  int idx = 0;
  for (auto s : state) {
    CHECK(FindRectanglesWithEnergyConflictMC(problems[idx], random, 1.0, 0.8)
              .conflicts.empty());
    ++idx;
    if (idx == kNumProblems) idx = 0;
  }
}

BENCHMARK(BM_FindRectangles)
    ->ArgPair(5, 1)
    ->ArgPair(10, 1)
    ->ArgPair(20, 1)
    ->ArgPair(30, 1)
    ->ArgPair(40, 1)
    ->ArgPair(80, 1)
    ->ArgPair(100, 1)
    ->ArgPair(200, 1)
    ->ArgPair(1000, 1)
    ->ArgPair(10000, 1)
    ->ArgPair(5, 100)
    ->ArgPair(10, 100)
    ->ArgPair(20, 100)
    ->ArgPair(30, 100)
    ->ArgPair(40, 100)
    ->ArgPair(80, 100)
    ->ArgPair(100, 100)
    ->ArgPair(200, 100)
    ->ArgPair(1000, 100)
    ->ArgPair(10000, 100);

void BM_FindPairwiseRestrictions(benchmark::State& state) {
  absl::BitGen random;
  // In the vast majority of the cases the propagator doesn't find any pairwise
  // condition to propagate. Thus we choose to benchmark for this particular
  // case.
  const std::vector<ItemWithVariableSize> items =
      GenerateItemsRectanglesWithNoPairwisePropagation(
          state.range(0), state.range(1) / 100.0, random);
  std::vector<PairwiseRestriction> results;
  for (auto s : state) {
    AppendPairwiseRestrictions(items, &results);
    CHECK(results.empty());
  }
}

BENCHMARK(BM_FindPairwiseRestrictions)
    ->ArgPair(5, 1)
    ->ArgPair(10, 1)
    ->ArgPair(20, 1)
    ->ArgPair(30, 1)
    ->ArgPair(40, 1)
    ->ArgPair(80, 1)
    ->ArgPair(100, 1)
    ->ArgPair(200, 1)
    ->ArgPair(1000, 1)
    ->ArgPair(10000, 1)
    ->ArgPair(5, 100)
    ->ArgPair(10, 100)
    ->ArgPair(20, 100)
    ->ArgPair(30, 100)
    ->ArgPair(40, 100)
    ->ArgPair(80, 100)
    ->ArgPair(100, 100)
    ->ArgPair(200, 100)
    ->ArgPair(1000, 100)
    ->ArgPair(10000, 100);

void BM_FindPartialIntersectionsSparse(benchmark::State& state) {
  absl::BitGen random;
  std::vector<std::vector<Rectangle>> problems;
  static constexpr int kNumProblems = 10;
  for (int i = 0; i < kNumProblems; i++) {
    std::vector<Rectangle>& rectangles = problems.emplace_back(
        GenerateNonConflictingRectangles(state.range(0), random));
    const int num_to_grow = absl::Uniform(random, 0, 20);
    for (int i = 0; i < num_to_grow; ++i) {
      Rectangle& rec =
          rectangles[absl::Uniform(random, size_t{0}, rectangles.size())];
      rec = {.x_min = rec.x_min - IntegerValue(absl::Uniform(random, 0, 4)),
             .x_max = rec.x_max + IntegerValue(absl::Uniform(random, 0, 4)),
             .y_min = rec.y_min - IntegerValue(absl::Uniform(random, 0, 4)),
             .y_max = rec.y_max + IntegerValue(absl::Uniform(random, 0, 4))};
    }
  }
  int idx = 0;
  for (auto s : state) {
    const std::vector<std::pair<int, int>> result =
        FindPartialRectangleIntersections(problems[idx]);
    CHECK_LT(result.size(), state.range(0) * state.range(0));
    ++idx;
    if (idx == kNumProblems) idx = 0;
  }
}

BENCHMARK(BM_FindPartialIntersectionsSparse)
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

std::vector<Rectangle> GeneratePathologicalCase(int num_rectangles) {
  std::vector<Rectangle> rectangles;
  for (int i = 0; i < num_rectangles / 2; ++i) {
    rectangles.push_back({.x_min = 2 * i,
                          .x_max = 2 * i + 1,
                          .y_min = 0,
                          .y_max = 2 * num_rectangles});
    rectangles.push_back({
        .x_min = 0,
        .x_max = 2 * num_rectangles,
        .y_min = 2 * i,
        .y_max = 2 * i + 1,
    });
  }
  return rectangles;
}

void BM_FindPartialIntersectionsPathological(benchmark::State& state) {
  const std::vector<Rectangle> rectangles =
      GeneratePathologicalCase(state.range(0));
  for (auto s : state) {
    const std::vector<std::pair<int, int>> result =
        FindPartialRectangleIntersections(rectangles);
    CHECK_LT(result.size(), state.range(0) * state.range(0));
  }
}

BENCHMARK(BM_FindPartialIntersectionsPathological)
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

std::vector<Rectangle> GenerateDenseCase(int num_rectangles) {
  absl::BitGen random;
  std::vector<Rectangle> rectangles;
  for (int i = 0; i < num_rectangles; ++i) {
    const IntegerValue x_min = absl::Uniform(random, 0, num_rectangles);
    const IntegerValue y_min = absl::Uniform(random, 0, num_rectangles);
    rectangles.push_back(
        {.x_min = x_min,
         .x_max = x_min + absl::Uniform(random, 1, num_rectangles),
         .y_min = y_min,
         .y_max = y_min + absl::Uniform(random, 1, num_rectangles)});
  }
  return rectangles;
}

void BM_FindPartialIntersectionsDense(benchmark::State& state) {
  absl::BitGen random;
  std::vector<std::vector<Rectangle>> problems;
  static constexpr int kNumProblems = 10;
  for (int i = 0; i < kNumProblems; i++) {
    problems.push_back(GenerateDenseCase(state.range(0)));
  }
  int idx = 0;
  for (auto s : state) {
    const std::vector<std::pair<int, int>> result =
        FindPartialRectangleIntersections(problems[idx]);
    CHECK_LT(result.size(), state.range(0) * state.range(0));
    ++idx;
    if (idx == kNumProblems) idx = 0;
  }
}

BENCHMARK(BM_FindPartialIntersectionsDense)
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

}  // namespace
}  // namespace sat
}  // namespace operations_research
