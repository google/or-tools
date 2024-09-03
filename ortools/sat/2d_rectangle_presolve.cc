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

#include "ortools/sat/2d_rectangle_presolve.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer.h"

namespace operations_research {
namespace sat {

bool PresolveFixed2dRectangles(
    absl::Span<const RectangleInRange> non_fixed_boxes,
    std::vector<Rectangle>* fixed_boxes) {
  // This implementation compiles a set of areas that cannot be occupied by any
  // item, then calls ReduceNumberofBoxes() to use these areas to minimize
  // `fixed_boxes`.
  bool changed = false;

  IntegerValue original_area = 0;
  std::vector<Rectangle> fixed_boxes_copy;
  if (VLOG_IS_ON(1)) {
    for (const Rectangle& r : *fixed_boxes) {
      original_area += r.Area();
    }
  }
  if (VLOG_IS_ON(2)) {
    fixed_boxes_copy = *fixed_boxes;
  }

  const int original_num_boxes = fixed_boxes->size();
  IntegerValue min_x_size = std::numeric_limits<IntegerValue>::max();
  IntegerValue min_y_size = std::numeric_limits<IntegerValue>::max();

  CHECK(!non_fixed_boxes.empty());
  Rectangle bounding_box = non_fixed_boxes[0].bounding_area;

  for (const RectangleInRange& box : non_fixed_boxes) {
    bounding_box.GrowToInclude(box.bounding_area);
    min_x_size = std::min(min_x_size, box.x_size);
    min_y_size = std::min(min_y_size, box.y_size);
  }

  // Fixed items are only useful to constraint where the non-fixed items can be
  // placed. This means in particular that any part of a fixed item outside the
  // bounding box of the non-fixed items is useless. Clip them.
  int new_size = 0;
  while (new_size < fixed_boxes->size()) {
    Rectangle& rectangle = (*fixed_boxes)[new_size];
    if (rectangle.x_min < bounding_box.x_min) {
      rectangle.x_min = bounding_box.x_min;
      changed = true;
    }
    if (rectangle.x_max > bounding_box.x_max) {
      rectangle.x_max = bounding_box.x_max;
      changed = true;
    }
    if (rectangle.y_min < bounding_box.y_min) {
      rectangle.y_min = bounding_box.y_min;
      changed = true;
    }
    if (rectangle.y_max > bounding_box.y_max) {
      rectangle.y_max = bounding_box.y_max;
      changed = true;
    }
    if (rectangle.SizeX() <= 0 || rectangle.SizeY() <= 0) {
      // The whole rectangle was outside of the domain, remove it.
      std::swap(rectangle, (*fixed_boxes)[fixed_boxes->size() - 1]);
      fixed_boxes->resize(fixed_boxes->size() - 1);
      continue;
    } else {
      new_size++;
    }
  }

  std::vector<Rectangle> optional_boxes = *fixed_boxes;

  if (bounding_box.x_min > std::numeric_limits<IntegerValue>::min() &&
      bounding_box.y_min > std::numeric_limits<IntegerValue>::min() &&
      bounding_box.x_max < std::numeric_limits<IntegerValue>::max() &&
      bounding_box.y_max < std::numeric_limits<IntegerValue>::max()) {
    // Add fake rectangles to build a frame around the bounding box. This allows
    // to find more areas that must be empty. The frame is as follows:
    //  +************
    //  +...........+
    //  +...........+
    //  +...........+
    //  ************+
    optional_boxes.push_back({.x_min = bounding_box.x_min - 1,
                              .x_max = bounding_box.x_max,
                              .y_min = bounding_box.y_min - 1,
                              .y_max = bounding_box.y_min});
    optional_boxes.push_back({.x_min = bounding_box.x_max,
                              .x_max = bounding_box.x_max + 1,
                              .y_min = bounding_box.y_min - 1,
                              .y_max = bounding_box.y_max});
    optional_boxes.push_back({.x_min = bounding_box.x_min,
                              .x_max = bounding_box.x_max + 1,
                              .y_min = bounding_box.y_max,
                              .y_max = bounding_box.y_max + 1});
    optional_boxes.push_back({.x_min = bounding_box.x_min - 1,
                              .x_max = bounding_box.x_min,
                              .y_min = bounding_box.y_min,
                              .y_max = bounding_box.y_max + 1});
  }

  // All items we added to `optional_boxes` at this point are only to be used by
  // the "gap between items" logic below. They are not actual optional boxes and
  // should be removed right after the logic is applied.
  const int num_optional_boxes_to_remove = optional_boxes.size();

  // Add a rectangle to `optional_boxes` but respecting that rectangles must
  // remain disjoint.
  const auto add_box = [&optional_boxes](Rectangle new_box) {
    std::vector<Rectangle> to_add = {std::move(new_box)};
    for (int i = 0; i < to_add.size(); ++i) {
      Rectangle new_box = to_add[i];
      bool is_disjoint = true;
      for (const Rectangle& existing_box : optional_boxes) {
        if (!new_box.IsDisjoint(existing_box)) {
          is_disjoint = false;
          for (const Rectangle& disjoint_box :
               new_box.SetDifference(existing_box)) {
            to_add.push_back(disjoint_box);
          }
          break;
        }
      }
      if (is_disjoint) {
        optional_boxes.push_back(std::move(new_box));
      }
    }
  };

  // Now check if there is any space that cannot be occupied by any non-fixed
  // item.
  std::vector<Rectangle> bounding_boxes;
  bounding_boxes.reserve(non_fixed_boxes.size());
  for (const RectangleInRange& box : non_fixed_boxes) {
    bounding_boxes.push_back(box.bounding_area);
  }
  std::vector<Rectangle> empty_spaces =
      FindEmptySpaces(bounding_box, std::move(bounding_boxes));
  for (const Rectangle& r : empty_spaces) {
    add_box(r);
  }

  // Now look for gaps between objects that are too small to place anything.
  for (int i = 1; i < optional_boxes.size(); ++i) {
    const Rectangle cur_box = optional_boxes[i];
    for (int j = 0; j < i; ++j) {
      const Rectangle& other_box = optional_boxes[j];
      const IntegerValue lower_top = std::min(cur_box.y_max, other_box.y_max);
      const IntegerValue higher_bottom =
          std::max(other_box.y_min, cur_box.y_min);
      const IntegerValue rightmost_left_edge =
          std::max(other_box.x_min, cur_box.x_min);
      const IntegerValue leftmost_right_edge =
          std::min(other_box.x_max, cur_box.x_max);
      if (rightmost_left_edge < leftmost_right_edge) {
        if (lower_top < higher_bottom &&
            higher_bottom - lower_top < min_y_size) {
          add_box({.x_min = rightmost_left_edge,
                   .x_max = leftmost_right_edge,
                   .y_min = lower_top,
                   .y_max = higher_bottom});
        }
      }
      if (higher_bottom < lower_top) {
        if (leftmost_right_edge < rightmost_left_edge &&
            rightmost_left_edge - leftmost_right_edge < min_x_size) {
          add_box({.x_min = leftmost_right_edge,
                   .x_max = rightmost_left_edge,
                   .y_min = higher_bottom,
                   .y_max = lower_top});
        }
      }
    }
  }
  optional_boxes.erase(optional_boxes.begin(),
                       optional_boxes.begin() + num_optional_boxes_to_remove);

  if (ReduceNumberofBoxes(fixed_boxes, &optional_boxes)) {
    changed = true;
  }
  if (changed && VLOG_IS_ON(1)) {
    IntegerValue area = 0;
    for (const Rectangle& r : *fixed_boxes) {
      area += r.Area();
    }
    VLOG_EVERY_N_SEC(1, 1) << "Presolved " << original_num_boxes
                           << " fixed rectangles (area=" << original_area
                           << ") into " << fixed_boxes->size()
                           << " (area=" << area << ")";

    VLOG_EVERY_N_SEC(2, 2) << "Presolved rectangles:\n"
                           << RenderDot(bounding_box, fixed_boxes_copy)
                           << "Into:\n"
                           << RenderDot(bounding_box, *fixed_boxes)
                           << (optional_boxes.empty()
                                   ? ""
                                   : absl::StrCat("Unused optional rects:\n",
                                                  RenderDot(bounding_box,
                                                            optional_boxes)));
  }
  return changed;
}

namespace {
struct Edge {
  IntegerValue x_start;
  IntegerValue y_start;
  IntegerValue size;

  enum class EdgePosition { TOP, BOTTOM, LEFT, RIGHT };

  static Edge GetEdge(const Rectangle& rectangle, EdgePosition pos) {
    switch (pos) {
      case EdgePosition::TOP:
        return {.x_start = rectangle.x_min,
                .y_start = rectangle.y_max,
                .size = rectangle.SizeX()};
      case EdgePosition::BOTTOM:
        return {.x_start = rectangle.x_min,
                .y_start = rectangle.y_min,
                .size = rectangle.SizeX()};
      case EdgePosition::LEFT:
        return {.x_start = rectangle.x_min,
                .y_start = rectangle.y_min,
                .size = rectangle.SizeY()};
      case EdgePosition::RIGHT:
        return {.x_start = rectangle.x_max,
                .y_start = rectangle.y_min,
                .size = rectangle.SizeY()};
    }
  }

  template <typename H>
  friend H AbslHashValue(H h, const Edge& e) {
    return H::combine(std::move(h), e.x_start, e.y_start, e.size);
  }

  bool operator==(const Edge& other) const {
    return x_start == other.x_start && y_start == other.y_start &&
           size == other.size;
  }
};
}  // namespace

bool ReduceNumberofBoxes(std::vector<Rectangle>* mandatory_rectangles,
                         std::vector<Rectangle>* optional_rectangles) {
  // The current implementation just greedly merge rectangles that shares an
  // edge. This is far from optimal, and it exists a polynomial optimal
  // algorithm (see page 3 of [1]) for this problem at least for the case where
  // optional_rectangles is empty.
  //
  // TODO(user): improve
  //
  // [1] Eppstein, David. "Graph-theoretic solutions to computational geometry
  // problems." International Workshop on Graph-Theoretic Concepts in Computer
  // Science. Berlin, Heidelberg: Springer Berlin Heidelberg, 2009.
  std::vector<std::unique_ptr<Rectangle>> rectangle_storage;
  enum class OptionalEnum { OPTIONAL, MANDATORY };
  // bool for is_optional
  std::vector<std::pair<const Rectangle*, OptionalEnum>> rectangles_ptr;
  absl::flat_hash_map<Edge, int> top_edges_to_rectangle;
  absl::flat_hash_map<Edge, int> bottom_edges_to_rectangle;
  absl::flat_hash_map<Edge, int> left_edges_to_rectangle;
  absl::flat_hash_map<Edge, int> right_edges_to_rectangle;

  using EdgePosition = Edge::EdgePosition;

  bool changed_optional = false;
  bool changed_mandatory = false;

  auto add_rectangle = [&](const Rectangle* rectangle_ptr,
                           OptionalEnum optional) {
    const int index = rectangles_ptr.size();
    rectangles_ptr.push_back({rectangle_ptr, optional});
    const Rectangle& rectangle = *rectangles_ptr[index].first;
    top_edges_to_rectangle[Edge::GetEdge(rectangle, EdgePosition::TOP)] = index;
    bottom_edges_to_rectangle[Edge::GetEdge(rectangle, EdgePosition::BOTTOM)] =
        index;
    left_edges_to_rectangle[Edge::GetEdge(rectangle, EdgePosition::LEFT)] =
        index;
    right_edges_to_rectangle[Edge::GetEdge(rectangle, EdgePosition::RIGHT)] =
        index;
  };
  for (const Rectangle& rectangle : *mandatory_rectangles) {
    add_rectangle(&rectangle, OptionalEnum::MANDATORY);
  }
  for (const Rectangle& rectangle : *optional_rectangles) {
    add_rectangle(&rectangle, OptionalEnum::OPTIONAL);
  }

  auto remove_rectangle = [&](const int index) {
    const Rectangle& rectangle = *rectangles_ptr[index].first;
    const Edge top_edge = Edge::GetEdge(rectangle, EdgePosition::TOP);
    const Edge bottom_edge = Edge::GetEdge(rectangle, EdgePosition::BOTTOM);
    const Edge left_edge = Edge::GetEdge(rectangle, EdgePosition::LEFT);
    const Edge right_edge = Edge::GetEdge(rectangle, EdgePosition::RIGHT);
    top_edges_to_rectangle.erase(top_edge);
    bottom_edges_to_rectangle.erase(bottom_edge);
    left_edges_to_rectangle.erase(left_edge);
    right_edges_to_rectangle.erase(right_edge);
    rectangles_ptr[index].first = nullptr;
  };

  bool iteration_did_merge = true;
  while (iteration_did_merge) {
    iteration_did_merge = false;
    for (int i = 0; i < rectangles_ptr.size(); ++i) {
      if (!rectangles_ptr[i].first) {
        continue;
      }
      const Rectangle& rectangle = *rectangles_ptr[i].first;

      const Edge top_edge = Edge::GetEdge(rectangle, EdgePosition::TOP);
      const Edge bottom_edge = Edge::GetEdge(rectangle, EdgePosition::BOTTOM);
      const Edge left_edge = Edge::GetEdge(rectangle, EdgePosition::LEFT);
      const Edge right_edge = Edge::GetEdge(rectangle, EdgePosition::RIGHT);

      int index = -1;
      if (const auto it = right_edges_to_rectangle.find(left_edge);
          it != right_edges_to_rectangle.end()) {
        index = it->second;
      } else if (const auto it = left_edges_to_rectangle.find(right_edge);
                 it != left_edges_to_rectangle.end()) {
        index = it->second;
      } else if (const auto it = bottom_edges_to_rectangle.find(top_edge);
                 it != bottom_edges_to_rectangle.end()) {
        index = it->second;
      } else if (const auto it = top_edges_to_rectangle.find(bottom_edge);
                 it != top_edges_to_rectangle.end()) {
        index = it->second;
      }
      if (index == -1) {
        continue;
      }
      iteration_did_merge = true;

      // Merge two rectangles!
      const OptionalEnum new_optional =
          (rectangles_ptr[i].second == OptionalEnum::MANDATORY ||
           rectangles_ptr[index].second == OptionalEnum::MANDATORY)
              ? OptionalEnum::MANDATORY
              : OptionalEnum::OPTIONAL;
      changed_mandatory =
          changed_mandatory || (new_optional == OptionalEnum::MANDATORY);
      changed_optional =
          changed_optional ||
          (rectangles_ptr[i].second == OptionalEnum::OPTIONAL ||
           rectangles_ptr[index].second == OptionalEnum::OPTIONAL);
      rectangle_storage.push_back(std::make_unique<Rectangle>(rectangle));
      Rectangle& new_rectangle = *rectangle_storage.back();
      new_rectangle.GrowToInclude(*rectangles_ptr[index].first);
      remove_rectangle(i);
      remove_rectangle(index);
      add_rectangle(&new_rectangle, new_optional);
    }
  }

  if (changed_mandatory) {
    std::vector<Rectangle> new_rectangles;
    for (auto [rectangle, optional] : rectangles_ptr) {
      if (rectangle && optional == OptionalEnum::MANDATORY) {
        new_rectangles.push_back(*rectangle);
      }
    }
    *mandatory_rectangles = std::move(new_rectangles);
  }
  if (changed_optional) {
    std::vector<Rectangle> new_rectangles;
    for (auto [rectangle, optional] : rectangles_ptr) {
      if (rectangle && optional == OptionalEnum::OPTIONAL) {
        new_rectangles.push_back(*rectangle);
      }
    }
    *optional_rectangles = std::move(new_rectangles);
  }
  return changed_mandatory;
}

}  // namespace sat
}  // namespace operations_research
