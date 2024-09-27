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

#include "ortools/sat/2d_try_edge_propagator.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/sat/2d_orthogonal_packing_testing.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {
namespace {

using ::testing::_;
using ::testing::Each;
using ::testing::Eq;
using ::testing::Not;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

class TryEdgeRectanglePropagatorForTest : public TryEdgeRectanglePropagator {
 public:
  explicit TryEdgeRectanglePropagatorForTest(
      Model* model, std::vector<RectangleInRange> active_box_ranges)
      : TryEdgeRectanglePropagator(true, true, GetHelperFromModel(model),
                                   GetHelperFromModel(model), model) {
    active_box_ranges_ = std::move(active_box_ranges);
  }

  void PopulateActiveBoxRanges() override {
    max_box_index_ = 0;
    for (const RectangleInRange& range : active_box_ranges_) {
      if (range.box_index > max_box_index_) {
        max_box_index_ = range.box_index;
      }
    }
  }

  bool ExplainAndPropagate(
      const std::vector<std::pair<int, std::optional<IntegerValue>>>&
          found_propagations) override {
    propagations_ = found_propagations;
    return false;
  }

  const std::vector<std::pair<int, std::optional<IntegerValue>>>& propagations()
      const {
    return propagations_;
  }

 private:
  static SchedulingConstraintHelper* GetHelperFromModel(Model* model) {
    return model->GetOrCreate<IntervalsRepository>()->GetOrCreateHelper({});
  }

  Model model_;
  IntervalsRepository* repository_ = model_.GetOrCreate<IntervalsRepository>();

  std::vector<std::pair<int, std::optional<IntegerValue>>> propagations_;
};

TEST(TryEdgeRectanglePropagatorTest, Simple) {
  //  **********
  //  **********            To place:
  //  **********            ++++++++
  //  **********            ++++++++
  //                        ++++++++
  //  ++++++++++            ++++++++
  //  ++++++++++
  //  ++++++++++
  //  ++++++++++
  //
  // The object to place can only be on the right of the two placed ones.
  std::vector<RectangleInRange> active_box_ranges = {
      {.box_index = 0,
       .bounding_area = {.x_min = 0, .x_max = 5, .y_min = 0, .y_max = 5},
       .x_size = 5,
       .y_size = 5},
      {.box_index = 1,
       .bounding_area = {.x_min = 0, .x_max = 5, .y_min = 6, .y_max = 11},
       .x_size = 5,
       .y_size = 5},
      {.box_index = 2,
       .bounding_area = {.x_min = 0, .x_max = 10, .y_min = 0, .y_max = 10},
       .x_size = 5,
       .y_size = 5},
  };
  Model model;
  TryEdgeRectanglePropagatorForTest propagator(&model, active_box_ranges);
  propagator.Propagate();
  EXPECT_THAT(propagator.propagations(),
              UnorderedElementsAre(Pair(2, IntegerValue(5))));

  // Now the same thing, but makes it a conflict
  active_box_ranges[2].bounding_area.x_min = 0;
  active_box_ranges[2].bounding_area.x_max = 5;
  TryEdgeRectanglePropagatorForTest propagator2(&model, active_box_ranges);
  propagator2.Propagate();
  EXPECT_THAT(propagator2.propagations(),
              UnorderedElementsAre(Pair(2, std::nullopt)));
}

TEST(TryEdgeRectanglePropagatorTest, NoConflictForFeasible) {
  constexpr int kNumRuns = 100;
  absl::BitGen bit_gen;
  Model model;

  for (int run = 0; run < kNumRuns; ++run) {
    // Start by generating a feasible problem that we know the solution with
    // some items fixed.
    std::vector<Rectangle> rectangles =
        GenerateNonConflictingRectanglesWithPacking({100, 100}, 60, bit_gen);
    std::shuffle(rectangles.begin(), rectangles.end(), bit_gen);
    const std::vector<RectangleInRange> input_in_range =
        MakeItemsFromRectangles(rectangles, 0.6, bit_gen);

    TryEdgeRectanglePropagatorForTest propagator(&model, input_in_range);
    propagator.Propagate();
    EXPECT_THAT(propagator.propagations(),
                Each(Pair(_, Not(Eq(std::nullopt)))));

    // Now check that the propagations are not in conflict with the initial
    // solution.
    for (const auto& [box_index, new_x_min] : propagator.propagations()) {
      EXPECT_LE(*new_x_min, rectangles[box_index].x_min);
    }
  }
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
