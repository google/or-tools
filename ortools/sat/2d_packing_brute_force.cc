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

#include "ortools/sat/2d_packing_brute_force.h"

#include <algorithm>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/util.h"
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
    const CompactVectorVector<int, PotentialPositionForItem>&
        potential_item_positions,
    IntegerValue slack) {
  const auto add_position_if_valid =
      [&item_positions, bounding_box_size, &sizes_x, &sizes_y,
       &placed_item_indexes](
          CompactVectorVector<int, PotentialPositionForItem>& positions, int i,
          IntegerValue x, IntegerValue y) {
        const Rectangle rect = {.x_min = x,
                                .x_max = x + sizes_x[i],
                                .y_min = y,
                                .y_max = y + sizes_y[i]};
        if (ShouldPlaceItemAtPosition(i, rect, bounding_box_size,
                                      item_positions, placed_item_indexes)) {
          positions.AppendToLastVector({x, y, false});
        }
      };

  const int num_items = sizes_x.size();
  CompactVectorVector<int, PotentialPositionForItem> new_potential_positions;
  new_potential_positions.reserve(num_items,
                                  3 * potential_item_positions.num_entries());
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
      new_potential_positions.clear();
      bool is_unfeasible = false;
      for (int j = 0; j < num_items; ++j) {
        // We will find all potential positions for the j-th item.
        new_potential_positions.Add({});
        // No need to attribute potential positions to already placed items
        // (including the one we just placed).
        if (placed_item_indexes[j]) {
          continue;
        }
        DCHECK_NE(i, j);
        // Add the new "potential positions" derived from the new item.
        for (const int k : placed_item_indexes) {
          DCHECK_NE(j, k);
          if (k == i) {
            continue;
          }

          add_position_if_valid(new_potential_positions, j, item_position.x_max,
                                item_positions[k].y_max);
          add_position_if_valid(new_potential_positions, j,
                                item_positions[k].x_max, item_position.y_max);
        }

        // Then copy previously valid positions that remain valid.
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
            new_potential_positions.AppendToLastVector(position);
          } else {
            new_potential_positions.AppendToLastVector(original_position);
          }
        }
        add_position_if_valid(new_potential_positions, j,
                              item_positions[i].x_max, 0);
        add_position_if_valid(new_potential_positions, j, 0,
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

bool BruteForceOrthogonalPackingNoPreprocessing(
    const absl::Span<PermutableItem> items,
    const std::pair<IntegerValue, IntegerValue> bounding_box_size) {
  IntegerValue smallest_x = std::numeric_limits<IntegerValue>::max();
  IntegerValue smallest_y = std::numeric_limits<IntegerValue>::max();
  const int num_items = items.size();
  CHECK_LE(num_items, kMaxProblemSize);
  IntegerValue slack = bounding_box_size.first * bounding_box_size.second;

  for (const PermutableItem& item : items) {
    smallest_x = std::min(smallest_x, item.size_x);
    smallest_y = std::min(smallest_y, item.size_y);
    slack -= item.size_x * item.size_y;
    if (item.size_x > bounding_box_size.first ||
        item.size_y > bounding_box_size.second) {
      return false;
    }
  }
  if (slack < 0) {
    return false;
  }

  std::sort(items.begin(), items.end(),
            [](const PermutableItem& a, const PermutableItem& b) {
              return std::make_tuple(a.size_x * a.size_y, a.index) >
                     std::make_tuple(b.size_x * b.size_y, b.index);
            });

  std::vector<IntegerValue> new_sizes_x(num_items);
  std::vector<IntegerValue> new_sizes_y(num_items);
  CompactVectorVector<int, PotentialPositionForItem> potential_item_positions;
  potential_item_positions.reserve(num_items, num_items);
  for (int i = 0; i < num_items; ++i) {
    new_sizes_x[i] = items[i].size_x;
    new_sizes_y[i] = items[i].size_y;
    potential_item_positions.Add({PotentialPositionForItem{0, 0, false}});
  }
  std::vector<Rectangle> item_positions(num_items);
  Bitset64<int> placed_item_indexes(num_items);
  const bool found_solution = BruteForceOrthogonalPackingImpl(
      new_sizes_x, new_sizes_y, bounding_box_size, smallest_x, smallest_y,
      absl::MakeSpan(item_positions), placed_item_indexes,
      potential_item_positions, slack);
  if (!found_solution) {
    return false;
  }
  for (int i = 0; i < num_items; ++i) {
    items[i].position = item_positions[i];
  }
  return true;
}

// This function tries to pack a set of "tall" items with all the "shallow"
// items that can fit in the area around them. See discussion about
// the identically-feasible function v_1 in [1]. For example, for the packing:
//
// +----------------------------+
// |------+                     |
// |888888+----|                |
// |888888|&&&&|                |
// |------+&&&&|                |
// |####| |&&&&|                |
// |####| +----+                |
// |####|         +------+      |
// |####|         |......|      |
// |####|         |......|@@@   |
// |####+---------+......|@@@   |
// |####|OOOOOOOOO|......|@@@   |
// |####|OOOOOOOOO|......|@@@   |
// |####|OOOOOOOOO|......|@@@   |
// |####|OOOOOOOOO|......|@@@   |
// |####|OOOOOOOOO|......|@@@   |
// +----+---------+------+------+
//
// We can move all "tall" and "shallow" items to the left and pack all the
// shallow items on the space remaining on the top of the tall items:
//
// +----------------------------+
// |------+                     |
// |888888+----|                |
// |888888|&&&&|                |
// |------+&&&&|                |
// |####| |&&&&|                |
// |####| +----+                |
// |####+------+                |
// |####|......|                |
// |####|......|          @@@   |
// |####|......+---------+@@@   |
// |####|......|OOOOOOOOO|@@@   |
// |####|......|OOOOOOOOO|@@@   |
// |####|......|OOOOOOOOO|@@@   |
// |####|......|OOOOOOOOO|@@@   |
// |####|......|OOOOOOOOO|@@@   |
// +----+------+---------+------+
//
// If the packing is successful, we can remove both set from the problem:
// +----------------+
// |                |
// |                |
// |                |
// |                |
// |                |
// |                |
// |                |
// |                |
// |          @@@   |
// |---------+@@@   |
// |OOOOOOOOO|@@@   |
// |OOOOOOOOO|@@@   |
// |OOOOOOOOO|@@@   |
// |OOOOOOOOO|@@@   |
// |OOOOOOOOO|@@@   |
// +---------+------+
//
// [1] Carlier, Jacques, François Clautiaux, and Aziz Moukrim. "New reduction
// procedures and lower bounds for the two-dimensional bin packing problem with
// fixed orientation." Computers & Operations Research 34.8 (2007): 2223-2250.
//
// See Preprocess() for the API documentation.
bool PreprocessLargeWithSmallX(
    absl::Span<PermutableItem>& items,
    std::pair<IntegerValue, IntegerValue>& bounding_box_size,
    int max_complexity) {
  // The simplest way to implement this is to sort the shallow items alongside
  // their corresponding tall items. More precisely, the smallest and largest
  // items are at the end of the list. The reason we put the most interesting
  // values in the end even if this means we want to iterate on the list
  // backward is that if the heuristic is successful we will trim them from the
  // back of the list of unfixed items.
  std::sort(
      items.begin(), items.end(),
      [&bounding_box_size](const PermutableItem& a, const PermutableItem& b) {
        const bool a_is_small = 2 * a.size_x <= bounding_box_size.first;
        const bool b_is_small = 2 * b.size_x <= bounding_box_size.first;
        const IntegerValue a_size_for_comp =
            a_is_small ? a.size_x : bounding_box_size.first - a.size_x;
        const IntegerValue b_size_for_comp =
            b_is_small ? b.size_x : bounding_box_size.first - b.size_x;
        return std::make_tuple(a_size_for_comp, !a_is_small, a.index) >
               std::make_tuple(b_size_for_comp, !b_is_small, b.index);
      });
  IntegerValue total_large_item_width = 0;
  IntegerValue largest_small_item_height = 0;
  for (int i = items.size() - 1; i >= 1; --i) {
    if (2 * items[i].size_x <= bounding_box_size.first) {
      largest_small_item_height = items[i].size_x;
      continue;
    }
    DCHECK_LE(items[i].size_x + largest_small_item_height,
              bounding_box_size.first);
    // We found a big item. So all values that we visited before are either
    // taller than it or shallow enough to fit on top of it. Try to fit all that
    // together!
    if (items.subspan(i).size() > max_complexity) {
      return false;
    }
    total_large_item_width += items[i].size_y;
    if (BruteForceOrthogonalPackingNoPreprocessing(
            items.subspan(i),
            {bounding_box_size.first, total_large_item_width})) {
      bounding_box_size.second -= total_large_item_width;
      for (auto& item : items.subspan(i)) {
        item.position.y_min += bounding_box_size.second;
        item.position.y_max += bounding_box_size.second;
      }
      items = items.subspan(0, i);
      return true;
    }
  }

  return false;
}

// Same API as Preprocess().
bool PreprocessLargeWithSmallY(
    absl::Span<PermutableItem>& items,
    std::pair<IntegerValue, IntegerValue>& bounding_box_size,
    int max_complexity) {
  absl::Span<PermutableItem> orig_items = items;
  for (PermutableItem& item : orig_items) {
    std::swap(item.size_x, item.size_y);
    std::swap(item.position.x_min, item.position.y_min);
    std::swap(item.position.x_max, item.position.y_max);
  }
  std::swap(bounding_box_size.first, bounding_box_size.second);
  const bool ret =
      PreprocessLargeWithSmallX(items, bounding_box_size, max_complexity);
  std::swap(bounding_box_size.first, bounding_box_size.second);
  for (PermutableItem& item : orig_items) {
    std::swap(item.size_x, item.size_y);
    std::swap(item.position.x_min, item.position.y_min);
    std::swap(item.position.x_max, item.position.y_max);
  }
  return ret;
}

}  // namespace

// Try to find an equivalent smaller OPP problem by fixing large items.
// The API is a bit unusual: it takes a reference to a mutable Span of sizes and
// rectangles. When this function finds an item that can be fixed, it sets
// the position of the PermutableItem, reorders `items` to put that item in the
// end of the span and then resizes the span so it contain only non-fixed items.
//
// Note that the position of input items is not used and the position of
// non-fixed items will not be modified by this function.
bool Preprocess(absl::Span<PermutableItem>& items,
                std::pair<IntegerValue, IntegerValue>& bounding_box_size,
                int max_complexity) {
  const int num_items = items.size();
  if (num_items == 1) {
    return false;
  }
  IntegerValue smallest_x = std::numeric_limits<IntegerValue>::max();
  IntegerValue largest_x = std::numeric_limits<IntegerValue>::min();
  IntegerValue smallest_y = std::numeric_limits<IntegerValue>::max();
  IntegerValue largest_y = std::numeric_limits<IntegerValue>::min();
  int largest_x_idx = -1;
  int largest_y_idx = -1;
  for (int i = 0; i < num_items; ++i) {
    if (items[i].size_x > largest_x) {
      largest_x = items[i].size_x;
      largest_x_idx = i;
    }
    if (items[i].size_y > largest_y) {
      largest_y = items[i].size_y;
      largest_y_idx = i;
    }
    smallest_x = std::min(smallest_x, items[i].size_x);
    smallest_y = std::min(smallest_y, items[i].size_y);
  }

  if (largest_x > bounding_box_size.first ||
      largest_y > bounding_box_size.second) {
    // No point in optimizing obviously infeasible instance.
    return false;
  }
  const auto remove_item = [&items](int index_to_remove) {
    std::swap(items[index_to_remove], items.back());
    items.remove_suffix(1);
  };
  if (largest_x + smallest_x > bounding_box_size.first) {
    // No item (not even the narrowest one) fit alongside the widest item. So we
    // care only about fitting the remaining items in the remaining space.
    bounding_box_size.second -= items[largest_x_idx].size_y;
    items[largest_x_idx].position = {
        .x_min = 0,
        .x_max = largest_x,
        .y_min = bounding_box_size.second,
        .y_max = bounding_box_size.second + items[largest_x_idx].size_y};
    remove_item(largest_x_idx);
    Preprocess(items, bounding_box_size, max_complexity);
    return true;
  }
  if (largest_y + smallest_y > bounding_box_size.second) {
    bounding_box_size.first -= items[largest_y_idx].size_x;
    items[largest_y_idx].position = {
        .x_min = bounding_box_size.first,
        .x_max = bounding_box_size.first + items[largest_y_idx].size_x,
        .y_min = 0,
        .y_max = largest_y};
    remove_item(largest_y_idx);
    Preprocess(items, bounding_box_size, max_complexity);
    return true;
  }

  if (PreprocessLargeWithSmallX(items, bounding_box_size, max_complexity)) {
    Preprocess(items, bounding_box_size, max_complexity);
    return true;
  }

  if (PreprocessLargeWithSmallY(items, bounding_box_size, max_complexity)) {
    Preprocess(items, bounding_box_size, max_complexity);
    return true;
  }

  return false;
}

BruteForceResult BruteForceOrthogonalPacking(
    absl::Span<const IntegerValue> sizes_x,
    absl::Span<const IntegerValue> sizes_y,
    std::pair<IntegerValue, IntegerValue> bounding_box_size,
    int max_complexity) {
  const int num_items = sizes_x.size();

  if (num_items > 2 * max_complexity) {
    // It is unlikely that preprocessing will remove half of the items, so don't
    // lose time trying.
    return {.status = BruteForceResult::Status::kTooBig};
  }
  CHECK_LE(num_items, kMaxProblemSize);

  std::vector<PermutableItem> items(num_items);
  for (int i = 0; i < num_items; ++i) {
    items[i] = {
        .size_x = sizes_x[i], .size_y = sizes_y[i], .index = i, .position = {}};
  }
  absl::Span<PermutableItem> post_processed_items = absl::MakeSpan(items);
  std::pair<IntegerValue, IntegerValue> post_processed_bounding_box_size =
      bounding_box_size;
  const bool post_processed =
      Preprocess(post_processed_items, post_processed_bounding_box_size,
                 max_complexity - 1);
  if (post_processed_items.size() > max_complexity) {
    return {.status = BruteForceResult::Status::kTooBig};
  }
  const bool is_feasible = BruteForceOrthogonalPackingNoPreprocessing(
      post_processed_items, post_processed_bounding_box_size);
  VLOG_EVERY_N_SEC(2, 3)
      << "Solved by brute force a problem of " << num_items << " items"
      << (post_processed ? absl::StrCat(" (", post_processed_items.size(),
                                        " after preprocessing)")
                         : "")
      << ": solution " << (is_feasible ? "found" : "not found") << ".";
  if (!is_feasible) {
    return {.status = BruteForceResult::Status::kNoSolutionExists};
  }
  std::vector<Rectangle> result(num_items);
  for (const PermutableItem& item : items) {
    result[item.index] = item.position;
  }
  VLOG_EVERY_N_SEC(3, 3) << "Found a feasible packing by brute force. Dot:\n "
                         << RenderDot(
                                Rectangle{.x_min = 0,
                                          .x_max = bounding_box_size.first,
                                          .y_min = 0,
                                          .y_max = bounding_box_size.second},
                                result);
  return {.status = BruteForceResult::Status::kFoundSolution,
          .positions_for_solution = result};
}

}  // namespace sat
}  // namespace operations_research
