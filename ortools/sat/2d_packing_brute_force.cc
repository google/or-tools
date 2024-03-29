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
#include <array>
#include <limits>
#include <utility>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer.h"
#include "ortools/util/bitset.h"

namespace operations_research {
namespace sat {

static constexpr int kMaxProblemSize = 16;

namespace {

enum class RectangleRelationship {
  TOUCHING_NEITHER_LEFT_OR_BOTTOM,
  TOUCHING_BOTTOM,
  TOUCHING_LEFT,
  OVERLAP,
};

RectangleRelationship GetRectangleRelationship(const Rectangle& rectangle,
                                               const Rectangle& other) {
  if (rectangle.x_min < other.x_max && other.x_min < rectangle.x_max &&
      rectangle.y_min < other.y_max && other.y_min < rectangle.y_max) {
    return RectangleRelationship::OVERLAP;
  }

  if (rectangle.x_min == other.x_max && rectangle.y_min < other.y_max &&
      other.y_min < rectangle.y_max) {
    return RectangleRelationship::TOUCHING_LEFT;
  }
  if (rectangle.x_min < other.x_max && other.x_min < rectangle.x_max &&
      rectangle.y_min == other.y_max) {
    return RectangleRelationship::TOUCHING_BOTTOM;
  }
  return RectangleRelationship::TOUCHING_NEITHER_LEFT_OR_BOTTOM;
}

bool ShouldPlaceItemAtPosition(
    int i, const Rectangle& item_position,
    std::pair<IntegerValue, IntegerValue> bounding_box_size,
    absl::Span<const Rectangle> item_positions,
    const Bitset64<int>& placed_item_indexes) {
  // Check if it fits in the BB.
  if (item_position.x_max > bounding_box_size.first ||
      item_position.y_max > bounding_box_size.second) {
    return false;
  }

  // Break symmetry: force 0th item to be in the bottom left quarter.
  if (i == 0 && (2 * item_position.x_min >
                     bounding_box_size.first - item_position.SizeX() ||
                 2 * item_position.y_min >
                     bounding_box_size.second - item_position.SizeY())) {
    return false;
  }

  // Check if it is conflicting with another item.
  bool touches_something_on_left = item_position.x_min == 0;
  bool touches_something_on_bottom = item_position.y_min == 0;
  for (const int j : placed_item_indexes) {
    DCHECK_NE(i, j);
    const RectangleRelationship pos =
        GetRectangleRelationship(item_position, item_positions[j]);
    if (pos == RectangleRelationship::OVERLAP) {
      return false;
    }
    touches_something_on_left = touches_something_on_left ||
                                pos == RectangleRelationship::TOUCHING_LEFT;
    touches_something_on_bottom = touches_something_on_bottom ||
                                  pos == RectangleRelationship::TOUCHING_BOTTOM;
  }

  // Finally, check if it touching something both on the bottom and to the left.
  if (!touches_something_on_left || !touches_something_on_bottom) {
    return false;
  }
  return true;
}

struct PotentialPositionForItem {
  IntegerValue x;
  IntegerValue y;
  bool already_explored;

  Rectangle GetRectangle(IntegerValue x_size, IntegerValue y_size) const {
    return {.x_min = x, .x_max = x + x_size, .y_min = y, .y_max = y + y_size};
  }
};

// This implementation search for a solution in the following order:
// - first place the 0-th item in the bottom left corner;
// - then place the 1-th item either on the bottom of the bounding box to the
//   right of the 0-th item, or on the left of the bounding box on top of it;
// - keep placing items, while respecting that each item should touch something
//   on both its bottom and left sides until either all items are placed (in
//   this case a solution is found and return) or we found an item that cannot
//   be placed on any possible solution.
// - if an item cannot be placed, backtrack: try to place the last successfully
//   placed item in another position.
//
// This is a recursive implementation, each call will place the first non placed
// item in a fixed order. Backtrack occur when we return from a recursive call.
//
// This return false iff it is infeasible to place the other items given the
// already placed ones.
//
// This implementation is very similar to the "Left-Most Active Only" method
// described in Clautiaux, François, Jacques Carlier, and Aziz Moukrim. "A new
// exact method for the two-dimensional orthogonal packing problem." European
// Journal of Operational Research 183.3 (2007): 1196-1211.
//
// TODO(user): try the graph-based algorithm by S. Fekete, J. Shepers, and
// J. Van Der Ween, https://arxiv.org/abs/cs/0604045.
bool BruteForceOrthogonalPackingImpl(
    absl::Span<const IntegerValue> sizes_x,
    absl::Span<const IntegerValue> sizes_y,
    std::pair<IntegerValue, IntegerValue> bounding_box_size,
    IntegerValue smallest_x, IntegerValue smallest_y,
    absl::Span<Rectangle> item_positions, Bitset64<int>& placed_item_indexes,
    absl::Span<const absl::InlinedVector<PotentialPositionForItem, 16>>
        potential_item_positions,
    IntegerValue slack) {
  const auto add_position_if_valid =
      [&item_positions, bounding_box_size, &sizes_x, &sizes_y,
       &placed_item_indexes](
          absl::InlinedVector<PotentialPositionForItem, 16>& positions, int i,
          IntegerValue x, IntegerValue y) {
        const Rectangle rect = {.x_min = x,
                                .x_max = x + sizes_x[i],
                                .y_min = y,
                                .y_max = y + sizes_y[i]};
        if (ShouldPlaceItemAtPosition(i, rect, bounding_box_size,
                                      item_positions, placed_item_indexes)) {
          positions.push_back({x, y, false});
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
    placed_item_indexes.Set(i);
    for (const PotentialPositionForItem& potential_position :
         potential_item_positions[i]) {
      if (potential_position.already_explored) {
        continue;
      }
      // Place the item on its candidate position.
      item_positions[i] =
          potential_position.GetRectangle(sizes_x[i], sizes_y[i]);
      const Rectangle& item_position = item_positions[i];

      IntegerValue slack_loss = 0;
      if (bounding_box_size.first - item_position.x_max < smallest_x) {
        // After placing this item, nothing will fit between it and the top of
        // the bounding box. Thus we have some space that will remain empty and
        // we can deduce it from our budget.
        slack_loss += item_position.SizeY() *
                      (bounding_box_size.first - item_position.x_max);
      }
      if (bounding_box_size.second - item_position.y_max < smallest_y) {
        // Same as above but with the right edge.
        slack_loss += item_position.SizeX() *
                      (bounding_box_size.second - item_position.y_max);
      }
      if (slack < slack_loss) {
        continue;
      }

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
      // We consider that every item must be touching something (other item or
      // the box boundaries) to the left and to the bottom. Thus, when we add a
      // new item, it is enough to consider at all positions where it would
      // touch the new item on the bottom and something else on the left or
      // touch the new item on the left and something else on the bottom. So we
      // consider the following points:
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
      // (like the point in the left boundary in the example above) but we will
      // detect that by testing each item one by one. Importantly, we only pass
      // valid positions down to the next search level.
      std::array<absl::InlinedVector<PotentialPositionForItem, 16>,
                 kMaxProblemSize>
          new_potential_positions_storage;
      absl::Span<absl::InlinedVector<PotentialPositionForItem, 16>>
          new_potential_positions(new_potential_positions_storage.data(),
                                  num_items);
      for (const int k : placed_item_indexes) {
        if (k == i) {
          continue;
        }

        const bool add_below =
            // We only add points below this one...
            item_positions[k].y_max <= item_position.y_max &&
            // ...and where we can fit at least the smallest element.
            item_position.x_max + smallest_x <= bounding_box_size.first &&
            item_positions[k].y_max + smallest_y <= bounding_box_size.second;
        const bool add_left =
            item_positions[k].x_max <= item_position.x_max &&
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
        for (const PotentialPositionForItem& original_position :
             potential_item_positions[j]) {
          if (!original_position.GetRectangle(sizes_x[j], sizes_y[j])
                   .IsDisjoint(item_position)) {
            // That was a valid position for item j, but now it is in conflict
            // with newly added item i.
            continue;
          }
          if (j < i) {
            // We already explored all items of index less than i in all their
            // current possible positions and they are all unfeasible. We still
            // keep track of whether it fit there or not, since having any item
            // that don't fit anywhere is a good stopping criteria. But we don't
            // have to retest those positions down in the search tree.
            PotentialPositionForItem position = original_position;
            position.already_explored = true;
            new_potential_positions[j].push_back(position);
          } else {
            new_potential_positions[j].push_back(original_position);
          }
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
              item_positions, placed_item_indexes, new_potential_positions,
              slack - slack_loss)) {
        return true;
      }
    }
    // Placing this item at the current bottom-left positions level failed.
    // Restore placed_item_indexes to its original value and try another one.
    placed_item_indexes.Set(i, false);
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
  CHECK_LE(num_items, kMaxProblemSize);
  std::vector<int> item_index_sorted_by_area_desc(num_items);
  std::array<absl::InlinedVector<PotentialPositionForItem, 16>, kMaxProblemSize>
      potential_item_positions_storage;
  absl::Span<absl::InlinedVector<PotentialPositionForItem, 16>>
      potential_item_positions(potential_item_positions_storage.data(),
                               num_items);
  for (int i = 0; i < num_items; ++i) {
    smallest_x = std::min(smallest_x, sizes_x[i]);
    smallest_y = std::min(smallest_y, sizes_y[i]);
    item_index_sorted_by_area_desc[i] = i;
    potential_item_positions[i].push_back({0, 0, false});
  }
  std::sort(item_index_sorted_by_area_desc.begin(),
            item_index_sorted_by_area_desc.end(),
            [sizes_x, sizes_y](int a, int b) {
              return sizes_x[a] * sizes_y[a] > sizes_x[b] * sizes_y[b];
            });
  std::array<IntegerValue, kMaxProblemSize> new_sizes_x_storage,
      new_sizes_y_storage;
  absl::Span<IntegerValue> new_sizes_x(new_sizes_x_storage.data(), num_items);
  absl::Span<IntegerValue> new_sizes_y(new_sizes_y_storage.data(), num_items);
  IntegerValue slack = bounding_box_size.first * bounding_box_size.second;
  for (int i = 0; i < num_items; ++i) {
    new_sizes_x[i] = sizes_x[item_index_sorted_by_area_desc[i]];
    new_sizes_y[i] = sizes_y[item_index_sorted_by_area_desc[i]];
    slack -= sizes_x[i] * sizes_y[i];
  }
  if (slack < 0) {
    return {};
  }
  std::array<Rectangle, kMaxProblemSize> item_positions_storage;
  absl::Span<Rectangle> item_positions(item_positions_storage.data(),
                                       num_items);
  Bitset64<int> placed_item_indexes(num_items);
  const bool found_solution = BruteForceOrthogonalPackingImpl(
      new_sizes_x, new_sizes_y, bounding_box_size, smallest_x, smallest_y,
      item_positions, placed_item_indexes, potential_item_positions, slack);
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
