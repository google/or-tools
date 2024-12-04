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

#include "ortools/sat/2d_orthogonal_packing_testing.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/types/span.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer_base.h"

namespace operations_research {
namespace sat {

std::vector<Rectangle> GenerateNonConflictingRectangles(
    int num_rectangles, absl::BitGenRef random) {
  static constexpr int kSizeMax = 1'000'000;
  std::vector<Rectangle> rectangles;
  rectangles.reserve(num_rectangles);
  rectangles.push_back(
      {.x_min = 0, .x_max = kSizeMax, .y_min = 0, .y_max = kSizeMax});
  while (rectangles.size() < num_rectangles) {
    std::swap(rectangles.back(),
              rectangles[absl::Uniform(random, 0ull, rectangles.size() - 1)]);
    const Rectangle& rec = rectangles.back();
    if (absl::Bernoulli(random, 0.5)) {
      const IntegerValue cut = IntegerValue(
          absl::Uniform(random, rec.x_min.value(), rec.x_max.value()));
      const Rectangle new_range = {.x_min = rec.x_min,
                                   .x_max = cut,
                                   .y_min = rec.y_min,
                                   .y_max = rec.y_max};
      const Rectangle new_range2 = {.x_min = cut,
                                    .x_max = rec.x_max,
                                    .y_min = rec.y_min,
                                    .y_max = rec.y_max};
      if (new_range.Area() == 0 || new_range2.Area() == 0) {
        continue;
      }
      rectangles.pop_back();
      rectangles.push_back(new_range);
      rectangles.push_back(new_range2);
    } else {
      const IntegerValue cut = IntegerValue(
          absl::Uniform(random, rec.y_min.value(), rec.y_max.value()));
      const Rectangle new_range = {.x_min = rec.x_min,
                                   .x_max = rec.x_max,
                                   .y_min = rec.y_min,
                                   .y_max = cut};
      const Rectangle new_range2 = {.x_min = rec.x_min,
                                    .x_max = rec.x_max,
                                    .y_min = cut,
                                    .y_max = rec.y_max};
      if (new_range.Area() == 0 || new_range2.Area() == 0) {
        continue;
      }
      rectangles.pop_back();
      rectangles.push_back(new_range);
      rectangles.push_back(new_range2);
    }
  }
  return rectangles;
}

std::vector<Rectangle> GenerateNonConflictingRectanglesWithPacking(
    std::pair<IntegerValue, IntegerValue> bb, int average_num_boxes,
    absl::BitGenRef random) {
  const double p = 0.01;
  std::vector<Rectangle> rectangles;
  int num_retries = 0;
  double average_size =
      std::sqrt(bb.first.value() * bb.second.value() / average_num_boxes);
  const int64_t n_x = static_cast<int64_t>(average_size / p);
  const int64_t n_y = static_cast<int64_t>(average_size / p);
  while (num_retries < 4) {
    num_retries++;

    std::pair<IntegerValue, IntegerValue> sizes;
    do {
      sizes.first = std::binomial_distribution<>(n_x, p)(random);
    } while (sizes.first == 0 || sizes.first > bb.first);
    do {
      sizes.second = std::binomial_distribution<>(n_y, p)(random);
    } while (sizes.second == 0 || sizes.second > bb.second);

    std::vector<IntegerValue> possible_x_positions = {0};
    std::vector<IntegerValue> possible_y_positions = {0};
    for (const Rectangle& rec : rectangles) {
      possible_x_positions.push_back(rec.x_max);
      possible_y_positions.push_back(rec.y_max);
    }
    std::sort(possible_x_positions.begin(), possible_x_positions.end());
    std::sort(possible_y_positions.begin(), possible_y_positions.end());
    bool found_position = false;
    for (const IntegerValue x : possible_x_positions) {
      for (const IntegerValue y : possible_y_positions) {
        if (x + sizes.first > bb.first || y + sizes.second > bb.second) {
          continue;
        }
        const Rectangle rec = {.x_min = x,
                               .x_max = x + sizes.first,
                               .y_min = y,
                               .y_max = y + sizes.second};
        bool conflict = false;
        for (const Rectangle r : rectangles) {
          if (!r.IsDisjoint(rec)) {
            conflict = true;
            break;
          }
        }
        if (conflict) {
          continue;
        } else {
          rectangles.push_back(rec);
          found_position = true;
          break;
        }
      }
      if (found_position) {
        break;
      }
    }
    if (found_position) {
      num_retries = 0;
    }
  }
  return rectangles;
}

std::vector<RectangleInRange> MakeItemsFromRectangles(
    absl::Span<const Rectangle> rectangles, double slack_factor,
    absl::BitGenRef random) {
  IntegerValue size_max_x = 0;
  IntegerValue size_max_y = 0;
  for (const Rectangle& rec : rectangles) {
    size_max_x = std::max(size_max_x, rec.SizeX());
    size_max_y = std::max(size_max_y, rec.SizeY());
  }
  std::vector<RectangleInRange> ranges;
  ranges.reserve(rectangles.size());
  const int max_slack_x = slack_factor * size_max_x.value();
  const int max_slack_y = slack_factor * size_max_y.value();
  int count = 0;
  for (const Rectangle& rec : rectangles) {
    RectangleInRange range;
    range.x_size = rec.x_max - rec.x_min;
    range.y_size = rec.y_max - rec.y_min;
    range.box_index = count++;
    range.bounding_area = {
        .x_min =
            rec.x_min - IntegerValue(absl::Uniform(random, 0, max_slack_x)),
        .x_max =
            rec.x_max + IntegerValue(absl::Uniform(random, 0, max_slack_x)),
        .y_min =
            rec.y_min - IntegerValue(absl::Uniform(random, 0, max_slack_y)),
        .y_max =
            rec.y_max + IntegerValue(absl::Uniform(random, 0, max_slack_y))};
    ranges.push_back(range);
  }
  return ranges;
}

std::vector<ItemForPairwiseRestriction>
GenerateItemsRectanglesWithNoPairwiseConflict(
    absl::Span<const Rectangle> rectangles, double slack_factor,
    absl::BitGenRef random) {
  const std::vector<RectangleInRange> range_items =
      MakeItemsFromRectangles(rectangles, slack_factor, random);
  std::vector<ItemForPairwiseRestriction> items;
  items.reserve(rectangles.size());
  for (int i = 0; i < range_items.size(); ++i) {
    const RectangleInRange& rec = range_items[i];
    items.push_back({.index = i,
                     .x = {.start_min = rec.bounding_area.x_min,
                           .start_max = rec.bounding_area.x_max - rec.x_size,
                           .end_min = rec.bounding_area.x_min + rec.x_size,
                           .end_max = rec.bounding_area.x_max},
                     .y = {.start_min = rec.bounding_area.y_min,
                           .start_max = rec.bounding_area.y_max - rec.y_size,
                           .end_min = rec.bounding_area.y_min + rec.y_size,
                           .end_max = rec.bounding_area.y_max}});
  }
  return items;
}

std::vector<ItemForPairwiseRestriction>
GenerateItemsRectanglesWithNoPairwisePropagation(int num_rectangles,
                                                 double slack_factor,
                                                 absl::BitGenRef random) {
  const std::vector<Rectangle> rectangles =
      GenerateNonConflictingRectangles(num_rectangles, random);
  std::vector<ItemForPairwiseRestriction> items =
      GenerateItemsRectanglesWithNoPairwiseConflict(rectangles, slack_factor,
                                                    random);
  bool done = false;
  // Now run the propagator until there is no more pairwise conditions.
  do {
    std::vector<PairwiseRestriction> results;
    AppendPairwiseRestrictions(items, &results);
    for (const PairwiseRestriction& restriction : results) {
      CHECK(restriction.type !=
            PairwiseRestriction::PairwiseRestrictionType::CONFLICT);
      // Remove the slack we added
      for (int index : {restriction.first_index, restriction.second_index}) {
        const auto& rec = rectangles[index];
        items[index] = {.index = index,
                        .x = {.start_min = rec.x_min,
                              .start_max = rec.x_min,
                              .end_min = rec.x_max,
                              .end_max = rec.x_max},
                        .y = {.start_min = rec.y_min,
                              .start_max = rec.y_min,
                              .end_min = rec.y_max,
                              .end_max = rec.y_max}};
      }
    }
    done = results.empty();
  } while (!done);
  return items;
}

}  // namespace sat
}  // namespace operations_research
