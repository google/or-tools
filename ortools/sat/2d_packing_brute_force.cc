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
#include <limits>
#include <utility>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer.h"

namespace operations_research {
namespace sat {

namespace {

enum class RectangleRelationship {
  TOUCHING_NEITHER_LEFT_OR_BOTTOM,
  TOUCHING_BOTTOM,
  TOUCHING_LEFT,
  OVERLAP,
};

// TODO(user): write faster and less hacky implementation
RectangleRelationship GetRectangleRelationship(const Rectangle& rectangle,
                                               const Rectangle& other) {
  if (!rectangle.IsDisjoint(other)) {
    return RectangleRelationship::OVERLAP;
  }
  const Rectangle item_position_left = {.x_min = rectangle.x_min - 1,
                                        .x_max = rectangle.x_max - 1,
                                        .y_min = rectangle.y_min,
                                        .y_max = rectangle.y_max};
  const Rectangle item_position_bottom = {.x_min = rectangle.x_min,
                                          .x_max = rectangle.x_max,
                                          .y_min = rectangle.y_min - 1,
                                          .y_max = rectangle.y_max - 1};
  if (!item_position_left.IsDisjoint(other)) {
    return RectangleRelationship::TOUCHING_LEFT;
  }
  if (!item_position_bottom.IsDisjoint(other)) {
    return RectangleRelationship::TOUCHING_BOTTOM;
  }
  return RectangleRelationship::TOUCHING_NEITHER_LEFT_OR_BOTTOM;
}

bool ShouldPlaceItemAtPosition(
    int i, IntegerValue x, IntegerValue y,
    absl::Span<const IntegerValue> sizes_x,
    absl::Span<const IntegerValue> sizes_y,
    std::pair<IntegerValue, IntegerValue> bounding_box_size,
    absl::InlinedVector<Rectangle, 16>& item_positions,
    absl::InlinedVector<bool, 16>& placed_item_indexes) {
  const int num_items = sizes_x.size();
  const Rectangle item_position = {
      .x_min = x, .x_max = x + sizes_x[i], .y_min = y, .y_max = y + sizes_y[i]};

  // Check if it fits in the BB.
  if (item_position.x_max > bounding_box_size.first ||
      item_position.y_max > bounding_box_size.second) {
    return false;
  }

  // Break symmetry: force 0th item to be in the bottom left quarter.
  if (i == 0 &&
      (2 * item_position.x_min > bounding_box_size.first - sizes_x[i] ||
       2 * item_position.y_min > bounding_box_size.second - sizes_y[i])) {
    return false;
  }

  // Check if it is conflicting with another item.
  bool is_conflicting_left = x == 0;
  bool is_conflicting_bottom = y == 0;
  for (int j = 0; j < num_items; ++j) {
    if (i != j && placed_item_indexes[j]) {
      const RectangleRelationship pos =
          GetRectangleRelationship(item_position, item_positions[j]);
      if (pos == RectangleRelationship::OVERLAP) {
        return false;
      }
      is_conflicting_left =
          is_conflicting_left || pos == RectangleRelationship::TOUCHING_LEFT;
      is_conflicting_bottom = is_conflicting_bottom ||
                              pos == RectangleRelationship::TOUCHING_BOTTOM;
    }
  }

  // Finally, check if it touching something both on the bottom and to the left.
  if (!is_conflicting_left || !is_conflicting_bottom) {
    return false;
  }
  return true;
}

// TODO(user): try the graph-based algorithm by S. Fekete, J. Shepers, and
// J. Van Der Ween, https://arxiv.org/abs/cs/0604045.
bool BruteForceOrthogonalPackingImpl(
    absl::Span<const IntegerValue> sizes_x,
    absl::Span<const IntegerValue> sizes_y,
    std::pair<IntegerValue, IntegerValue> bounding_box_size,
    IntegerValue smallest_x, IntegerValue smallest_y,
    absl::InlinedVector<Rectangle, 16>& item_positions,
    absl::InlinedVector<bool, 16>& placed_item_indexes,
    const absl::InlinedVector<
        absl::InlinedVector<std::pair<IntegerValue, IntegerValue>, 16>, 16>&
        potential_item_positions) {
  const auto add_position_if_valid =
      [&item_positions, bounding_box_size, &sizes_x, &sizes_y,
       &placed_item_indexes](
          absl::InlinedVector<std::pair<IntegerValue, IntegerValue>, 16>&
              positions,
          int i, IntegerValue x, IntegerValue y) {
        if (ShouldPlaceItemAtPosition(i, x, y, sizes_x, sizes_y,
                                      bounding_box_size, item_positions,
                                      placed_item_indexes)) {
          positions.push_back({x, y});
        }
      };

  const int num_items = sizes_x.size();
  bool has_unplaced_item = false;
  for (int i = 0; i < num_items; ++i) {
    if (placed_item_indexes[i]) {
      continue;
    }
    if (potential_item_positions[i].empty()) {
      return false;
    }

    has_unplaced_item = true;
    placed_item_indexes[i] = true;
    for (std::pair<IntegerValue, IntegerValue> potential_position :
         potential_item_positions[i]) {
      // Place the item on its candidate position.
      item_positions[i] = {.x_min = potential_position.first,
                           .x_max = potential_position.first + sizes_x[i],
                           .y_min = potential_position.second,
                           .y_max = potential_position.second + sizes_y[i]};
      const Rectangle& item_position = item_positions[i];

      // Now the hard part of the algorithm: create the new "potential
      // positions" vector after placing this item. Describing the actual set of
      // acceptable places to put consider for the next item in the search would
      // be pretty complex. For example:
      // +----------------------------+
      // |                            |
      // |x                           |
      // |--------+                   |
      // |88888888|                   |
      // |88888888|                   |
      // |--------+                   |
      // |####|                       |
      // |####|x   x                  |
      // |####|         +------+      |
      // |####|         |......|      |
      // |####|         |......|      |
      // |####|         |......|      |
      // |####|x   x    |......|      |
      // |####+---------+......|      |
      // |####|OOOOOOOOO|......|      |
      // |####|OOOOOOOOO|......|      |
      // |####|OOOOOOOOO|......|x     |
      // +----+---------+------+------+
      //
      // To make things simpler, we just consider:
      // - all previous positions if they didn't got invalid due to the new
      //   item;
      // - new position are derived getting the right-top most corner of the
      //   added item and connecting it to the bottom and the left with a line.
      //   New potential positions are the intersection of this line with either
      //   the current items or the box. For example, if we add a box to the
      //   example above (representing the two lines by '*'):
      // +----------------------------+
      // |                            |
      // |                            |
      // |--------+                   |
      // |88888888|                   |
      // |88888888|                   |
      // |--------+                   |
      // |####|                       |
      // |####|                       |
      // |####|         +------+      |
      // |x###|x        |......|x     |
      // |**************************  |
      // |####|         |......|@@@*  |
      // |####|         |......|@@@*  |
      // |####+---------+......|@@@*  |
      // |####|OOOOOOOOO|......|@@@*  |
      // |####|OOOOOOOOO|......|@@@*  |
      // |####|OOOOOOOOO|......|@@@*x |
      // +----+---------+------+------+
      //
      // This method finds potential locations that are not useful for any item,
      // but we will detect that by testing each item one by one.
      absl::InlinedVector<
          absl::InlinedVector<std::pair<IntegerValue, IntegerValue>, 16>, 16>
          new_potential_positions(num_items);
      for (int k = 0; k < num_items; ++k) {
        if (k == i || !placed_item_indexes[k]) {
          continue;
        }

        bool add_below =
            // We only add points below this one...
            item_positions[k].y_max <= item_position.y_min &&
            // ...and where we can fit at least the smallest element.
            item_position.x_max + smallest_x <= bounding_box_size.first &&
            item_positions[k].y_max + smallest_y <= bounding_box_size.second;
        bool add_left =
            item_positions[k].x_max <= item_position.x_min &&
            item_positions[k].x_max + smallest_x <= bounding_box_size.first &&
            item_position.y_max + smallest_y <= bounding_box_size.second;
        for (int j = 0; j < num_items; ++j) {
          if (k == j || placed_item_indexes[j]) {
            continue;
          }
          if (add_below) {
            add_position_if_valid(new_potential_positions[j], j,
                                  item_position.x_max, item_positions[k].y_max);
          }
          if (add_left) {
            add_position_if_valid(new_potential_positions[j], j,
                                  item_positions[k].x_max, item_position.y_max);
          }
        }
      }
      bool is_unfeasible = false;
      for (int j = 0; j < num_items; ++j) {
        // No positions to attribute to the item we just placed.
        if (i == j || placed_item_indexes[j]) {
          continue;
        }
        // First copy previously valid positions that remain valid.
        for (const std::pair<IntegerValue, IntegerValue>& original_position :
             potential_item_positions[j]) {
          const Rectangle item_in_pos = {
              .x_min = original_position.first,
              .x_max = original_position.first + sizes_x[j],
              .y_min = original_position.second,
              .y_max = original_position.second + sizes_y[j]};

          if (!item_in_pos.IsDisjoint(item_position)) {
            // That was a valid position for item j, but now it is in conflict
            // with newly added item i.
            continue;
          }
          new_potential_positions[j].push_back(original_position);
        }
        add_position_if_valid(new_potential_positions[j], j,
                              item_positions[i].x_max, 0);
        add_position_if_valid(new_potential_positions[j], j, 0,
                              item_positions[i].y_max);
        if (new_potential_positions[j].empty()) {
          // After placing the item i, there is no valid place to choose for the
          // item j. We must pick another placement for i.
          is_unfeasible = true;
          break;
        }
      }
      if (is_unfeasible) {
        continue;
      }
      if (BruteForceOrthogonalPackingImpl(
              sizes_x, sizes_y, bounding_box_size, smallest_x, smallest_y,
              item_positions, placed_item_indexes, new_potential_positions)) {
        return true;
      }
    }
    // Placing this item at the current bottom-left positions level failed.
    // Restore placed_item_indexes to its original value and try another one.
    placed_item_indexes[i] = false;
  }
  return !has_unplaced_item;
}

}  // namespace

std::vector<Rectangle> BruteForceOrthogonalPacking(
    absl::Span<const IntegerValue> sizes_x,
    absl::Span<const IntegerValue> sizes_y,
    std::pair<IntegerValue, IntegerValue> bounding_box_size) {
  IntegerValue smallest_x = std::numeric_limits<IntegerValue>::max();
  IntegerValue smallest_y = std::numeric_limits<IntegerValue>::max();
  int num_items = sizes_x.size();
  absl::InlinedVector<int, 16> item_index_sorted_by_area_desc(num_items);
  absl::InlinedVector<
      absl::InlinedVector<std::pair<IntegerValue, IntegerValue>, 16>, 16>
      potential_item_positions(num_items);
  for (int i = 0; i < num_items; ++i) {
    smallest_x = std::min(smallest_x, sizes_x[i]);
    smallest_y = std::min(smallest_y, sizes_y[i]);
    item_index_sorted_by_area_desc[i] = i;
    potential_item_positions[i].push_back({0, 0});
  }
  std::sort(item_index_sorted_by_area_desc.begin(),
            item_index_sorted_by_area_desc.end(),
            [sizes_x, sizes_y](int a, int b) {
              return sizes_x[a] * sizes_y[a] > sizes_x[b] * sizes_y[b];
            });
  absl::InlinedVector<IntegerValue, 16> new_sizes_x(num_items);
  absl::InlinedVector<IntegerValue, 16> new_sizes_y(num_items);
  for (int i = 0; i < num_items; ++i) {
    new_sizes_x[i] = sizes_x[item_index_sorted_by_area_desc[i]];
    new_sizes_y[i] = sizes_y[item_index_sorted_by_area_desc[i]];
  }
  absl::InlinedVector<Rectangle, 16> item_positions(num_items);
  absl::InlinedVector<bool, 16> placed_item_indexes(num_items);
  const bool found_solution = BruteForceOrthogonalPackingImpl(
      new_sizes_x, new_sizes_y, bounding_box_size, smallest_x, smallest_y,
      item_positions, placed_item_indexes, potential_item_positions);
  if (!found_solution) {
    return {};
  }
  std::vector<Rectangle> result(num_items);
  for (int i = 0; i < num_items; ++i) {
    result[item_index_sorted_by_area_desc[i]] = item_positions[i];
  }
  VLOG_EVERY_N_SEC(2, 3) << "Found a feasible packing by brute force. Dot:\n "
                         << RenderDot(bounding_box_size, result);
  return result;
}

}  // namespace sat
}  // namespace operations_research
