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
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "absl/random/random.h"
#include "absl/types/span.h"
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
  TryEdgeRectanglePropagatorForTest(bool x_is_forward, bool y_is_forward,
                                    SchedulingConstraintHelper* x,
                                    SchedulingConstraintHelper* y, Model* model)
      : TryEdgeRectanglePropagator(x_is_forward, y_is_forward, x, y, model) {}

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

std::pair<SchedulingConstraintHelper*, SchedulingConstraintHelper*>
CreateHelper(Model* model,
             absl::Span<const RectangleInRange> active_box_ranges) {
  std::vector<IntervalVariable> x_intervals;
  std::vector<IntervalVariable> y_intervals;
  for (const RectangleInRange& active_box_range : active_box_ranges) {
    const int64_t x_start_min = active_box_range.bounding_area.x_min.value();
    const int64_t x_end_max = active_box_range.bounding_area.x_max.value();
    const int64_t x_size = active_box_range.x_size.value();

    const int64_t y_start_min = active_box_range.bounding_area.y_min.value();
    const int64_t y_end_max = active_box_range.bounding_area.y_max.value();
    const int64_t y_size = active_box_range.y_size.value();

    const IntervalVariable x_interval =
        model->Add(NewInterval(x_start_min, x_end_max, x_size));

    const IntervalVariable y_interval =
        model->Add(NewInterval(y_start_min, y_end_max, y_size));

    x_intervals.push_back(x_interval);
    y_intervals.push_back(y_interval);
  }
  return {
      model->GetOrCreate<IntervalsRepository>()->GetOrCreateHelper(x_intervals),
      model->GetOrCreate<IntervalsRepository>()->GetOrCreateHelper(
          y_intervals)};
}

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

  {
    Model model;
    auto [x_helper, y_helper] = CreateHelper(&model, active_box_ranges);

    TryEdgeRectanglePropagatorForTest propagator(true, true, x_helper, y_helper,
                                                 &model);
    propagator.Propagate();
    EXPECT_THAT(propagator.propagations(),
                UnorderedElementsAre(Pair(2, IntegerValue(5))));
  }

  // Now the same thing, but makes it a conflict
  {
    active_box_ranges[2].bounding_area.x_min = 0;
    active_box_ranges[2].bounding_area.x_max = 5;
    Model model;
    auto [x_helper, y_helper] = CreateHelper(&model, active_box_ranges);

    TryEdgeRectanglePropagatorForTest propagator(true, true, x_helper, y_helper,
                                                 &model);
    propagator.Propagate();
    EXPECT_THAT(propagator.propagations(),
                UnorderedElementsAre(Pair(2, std::nullopt)));
  }
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
    Model model;
    auto [x_helper, y_helper] = CreateHelper(&model, input_in_range);

    TryEdgeRectanglePropagatorForTest propagator(true, true, x_helper, y_helper,
                                                 &model);
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
