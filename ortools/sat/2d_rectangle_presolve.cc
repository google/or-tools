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

#include "ortools/sat/2d_rectangle_presolve.h"

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/log_severity.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/graph/minimum_vertex_cover.h"
#include "ortools/graph/strongly_connected_components.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer_base.h"

namespace operations_research {
namespace sat {

namespace {
std::vector<Rectangle> FindSpacesThatCannotBeOccupied(
    absl::Span<const Rectangle> fixed_boxes,
    absl::Span<const RectangleInRange> non_fixed_boxes,
    const Rectangle& bounding_box, IntegerValue min_x_size,
    IntegerValue min_y_size) {
  std::vector<Rectangle> optional_boxes = {fixed_boxes.begin(),
                                           fixed_boxes.end()};

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
               new_box.RegionDifference(existing_box)) {
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
  // TODO(user): remove the limit of 1000 and reimplement FindEmptySpaces()
  // using a sweep line algorithm.
  if (non_fixed_boxes.size() < 1000) {
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
  return optional_boxes;
}

}  // namespace

bool PresolveFixed2dRectangles(
    absl::Span<const RectangleInRange> non_fixed_boxes,
    std::vector<Rectangle>* fixed_boxes) {
  // This implementation compiles a set of areas that cannot be occupied by any
  // item, then calls ReduceNumberofBoxes() to use these areas to minimize
  // `fixed_boxes`.
  bool changed = false;

  DCHECK(FindPartialRectangleIntersections(*fixed_boxes).empty());
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

  // The greedy algorithm is really fast. Run it first since it might greatly
  // reduce the size of large trivial instances.
  std::vector<Rectangle> empty_vec;
  if (ReduceNumberofBoxesGreedy(fixed_boxes, &empty_vec)) {
    changed = true;
  }

  IntegerValue min_x_size = std::numeric_limits<IntegerValue>::max();
  IntegerValue min_y_size = std::numeric_limits<IntegerValue>::max();

  CHECK(!non_fixed_boxes.empty());
  Rectangle bounding_box = non_fixed_boxes[0].bounding_area;

  for (const RectangleInRange& box : non_fixed_boxes) {
    bounding_box.GrowToInclude(box.bounding_area);
    min_x_size = std::min(min_x_size, box.x_size);
    min_y_size = std::min(min_y_size, box.y_size);
  }
  DCHECK_GT(min_x_size, 0);
  DCHECK_GT(min_y_size, 0);

  // Fixed items are only useful to constraint where the non-fixed items can be
  // placed. This means in particular that any part of a fixed item outside the
  // bounding box of the non-fixed items is useless. Clip them.
  int new_size = 0;
  while (new_size < fixed_boxes->size()) {
    Rectangle& rectangle = (*fixed_boxes)[new_size];
    DCHECK_GT(rectangle.SizeX(), 0);
    DCHECK_GT(rectangle.SizeY(), 0);
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
      changed = true;
      continue;
    } else {
      new_size++;
    }
  }

  std::vector<Rectangle> optional_boxes = FindSpacesThatCannotBeOccupied(
      *fixed_boxes, non_fixed_boxes, bounding_box, min_x_size, min_y_size);

  // TODO(user): instead of doing the greedy algorithm first with optional
  // boxes, and then the one that is exact for mandatory boxes but weak for
  // optional ones, refactor the second algorithm. One possible way of doing
  // that would be to follow the shape boundary of optional+mandatory boxes and
  // look whether we can shave off some turns. For example, if we have a shape
  // like below, with the "+" representing area covered by optional boxes, we
  // can replace the turns by a straight line.
  //
  //          -->
  //       ^ ++++
  //       . ++++ .
  //       . ++++ .       =>
  //         ++++ \/
  //     --> ++++                   -->  -->
  //  ***********                ***********
  //  ***********                ***********
  //
  // Since less turns means less edges, this should be a good way to reduce the
  // number of boxes.
  if (ReduceNumberofBoxesGreedy(fixed_boxes, &optional_boxes)) {
    changed = true;
  }
  const int num_after_first_pass = fixed_boxes->size();
  if (ReduceNumberOfBoxesExactMandatory(fixed_boxes, &optional_boxes)) {
    changed = true;
  }
  if (changed && VLOG_IS_ON(1)) {
    IntegerValue area = 0;
    for (const Rectangle& r : *fixed_boxes) {
      area += r.Area();
    }
    VLOG_EVERY_N_SEC(1, 1) << "Presolved " << original_num_boxes
                           << " fixed rectangles (area=" << original_area
                           << ") into " << num_after_first_pass << " then "
                           << fixed_boxes->size() << " (area=" << area << ")";

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

  static bool CompareXThenY(const Edge& a, const Edge& b) {
    return std::tie(a.x_start, a.y_start, a.size) <
           std::tie(b.x_start, b.y_start, b.size);
  }
  static bool CompareYThenX(const Edge& a, const Edge& b) {
    return std::tie(a.y_start, a.x_start, a.size) <
           std::tie(b.y_start, b.x_start, b.size);
  }
};
}  // namespace

bool ReduceNumberofBoxesGreedy(std::vector<Rectangle>* mandatory_rectangles,
                               std::vector<Rectangle>* optional_rectangles) {
  // The current implementation just greedly merge rectangles that shares an
  // edge.
  std::vector<std::unique_ptr<Rectangle>> rectangle_storage;
  enum class OptionalEnum { OPTIONAL, MANDATORY };
  // bool for is_optional
  std::vector<std::pair<const Rectangle*, OptionalEnum>> rectangles_ptr;
  absl::flat_hash_map<Edge, int> top_edges_to_rectangle;
  absl::flat_hash_map<Edge, int> bottom_edges_to_rectangle;
  absl::flat_hash_map<Edge, int> left_edges_to_rectangle;
  absl::flat_hash_map<Edge, int> right_edges_to_rectangle;

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

Neighbours BuildNeighboursGraph(absl::Span<const Rectangle> rectangles) {
  // To build a graph of neighbours, we build a sorted vector for each one of
  // the edges (top, bottom, etc) of the rectangles. Then we merge the bottom
  // and top vectors and iterate on it. Due to the sorting order, segments where
  // the bottom of a rectangle touches the top of another one must consecutive.
  std::vector<std::pair<Edge, int>> edges_to_rectangle[4];
  std::vector<std::tuple<int, EdgePosition, int>> neighbours;
  neighbours.reserve(2 * rectangles.size());
  for (int edge_int = 0; edge_int < 4; ++edge_int) {
    const EdgePosition edge_position = static_cast<EdgePosition>(edge_int);
    edges_to_rectangle[edge_position].reserve(rectangles.size());
  }

  for (int i = 0; i < rectangles.size(); ++i) {
    const Rectangle& rectangle = rectangles[i];
    for (int edge_int = 0; edge_int < 4; ++edge_int) {
      const EdgePosition edge_position = static_cast<EdgePosition>(edge_int);
      const Edge edge = Edge::GetEdge(rectangle, edge_position);
      edges_to_rectangle[edge_position].push_back({edge, i});
    }
  }
  for (int edge_int = 0; edge_int < 4; ++edge_int) {
    const EdgePosition edge_position = static_cast<EdgePosition>(edge_int);
    const bool sort_x_then_y = edge_position == EdgePosition::LEFT ||
                               edge_position == EdgePosition::RIGHT;
    const auto cmp =
        sort_x_then_y
            ? [](const std::pair<Edge, int>& a,
                 const std::pair<Edge, int>&
                     b) { return Edge::CompareXThenY(a.first, b.first); }
            : [](const std::pair<Edge, int>& a, const std::pair<Edge, int>& b) {
                return Edge::CompareYThenX(a.first, b.first);
              };
    absl::c_sort(edges_to_rectangle[edge_position], cmp);
  }

  constexpr struct EdgeData {
    EdgePosition edge;
    EdgePosition opposite_edge;
    bool (*cmp)(const Edge&, const Edge&);
  } edge_data[4] = {{.edge = EdgePosition::BOTTOM,
                     .opposite_edge = EdgePosition::TOP,
                     .cmp = &Edge::CompareYThenX},
                    {.edge = EdgePosition::TOP,
                     .opposite_edge = EdgePosition::BOTTOM,
                     .cmp = &Edge::CompareYThenX},
                    {.edge = EdgePosition::LEFT,
                     .opposite_edge = EdgePosition::RIGHT,
                     .cmp = &Edge::CompareXThenY},
                    {.edge = EdgePosition::RIGHT,
                     .opposite_edge = EdgePosition::LEFT,
                     .cmp = &Edge::CompareXThenY}};

  for (int edge_int = 0; edge_int < 4; ++edge_int) {
    const EdgePosition edge_position = edge_data[edge_int].edge;
    const EdgePosition opposite_edge_position =
        edge_data[edge_int].opposite_edge;
    auto it = edges_to_rectangle[edge_position].begin();
    for (const auto& [edge, index] :
         edges_to_rectangle[opposite_edge_position]) {
      while (it != edges_to_rectangle[edge_position].end() &&
             edge_data[edge_int].cmp(it->first, edge)) {
        ++it;
      }
      if (it == edges_to_rectangle[edge_position].end()) {
        break;
      }
      if (edge_position == EdgePosition::BOTTOM ||
          edge_position == EdgePosition::TOP) {
        while (it != edges_to_rectangle[edge_position].end() &&
               it->first.y_start == edge.y_start &&
               it->first.x_start < edge.x_start + edge.size) {
          neighbours.push_back({index, opposite_edge_position, it->second});
          neighbours.push_back({it->second, edge_position, index});
          ++it;
        }
      } else {
        while (it != edges_to_rectangle[edge_position].end() &&
               it->first.x_start == edge.x_start &&
               it->first.y_start < edge.y_start + edge.size) {
          neighbours.push_back({index, opposite_edge_position, it->second});
          neighbours.push_back({it->second, edge_position, index});
          ++it;
        }
      }
    }
  }

  gtl::STLSortAndRemoveDuplicates(&neighbours);
  return Neighbours(rectangles, neighbours);
}

std::vector<std::vector<int>> SplitInConnectedComponents(
    const Neighbours& neighbours) {
  class GraphView {
   public:
    explicit GraphView(const Neighbours& neighbours)
        : neighbours_(neighbours) {}
    absl::Span<const int> operator[](int node) const {
      temp_.clear();
      for (int edge = 0; edge < 4; ++edge) {
        const auto edge_neighbors = neighbours_.GetSortedNeighbors(
            node, static_cast<EdgePosition>(edge));
        for (int neighbor : edge_neighbors) {
          temp_.push_back(neighbor);
        }
      }
      return temp_;
    }

   private:
    const Neighbours& neighbours_;
    mutable std::vector<int> temp_;
  };

  std::vector<std::vector<int>> components;
  FindStronglyConnectedComponents(neighbours.NumRectangles(),
                                  GraphView(neighbours), &components);
  return components;
}

namespace {
IntegerValue GetClockwiseStart(EdgePosition edge, const Rectangle& rectangle) {
  switch (edge) {
    case EdgePosition::LEFT:
      return rectangle.y_min;
    case EdgePosition::RIGHT:
      return rectangle.y_max;
    case EdgePosition::BOTTOM:
      return rectangle.x_max;
    case EdgePosition::TOP:
      return rectangle.x_min;
  }
}

IntegerValue GetClockwiseEnd(EdgePosition edge, const Rectangle& rectangle) {
  switch (edge) {
    case EdgePosition::LEFT:
      return rectangle.y_max;
    case EdgePosition::RIGHT:
      return rectangle.y_min;
    case EdgePosition::BOTTOM:
      return rectangle.x_min;
    case EdgePosition::TOP:
      return rectangle.x_max;
  }
}

// Given a list of rectangles and their neighbours graph, find the list of
// vertical and horizontal segments that touches a single rectangle edge. Or,
// view in another way, the pieces of an edge that is touching the empty space.
// For example, this corresponds to the "0" segments in the example below:
//
//   000000
//   0****0    000000
//   0****0    0****0
//   0****0    0****0
// 00******00000****00000
// 0********************0
// 0********************0
// 0000000000000000000000
void GetAllSegmentsTouchingVoid(
    absl::Span<const Rectangle> rectangles, const Neighbours& neighbours,
    std::vector<std::pair<Edge, int>>& vertical_edges_on_boundary,
    std::vector<std::pair<Edge, int>>& horizontal_edges_on_boundary) {
  for (int i = 0; i < rectangles.size(); ++i) {
    const Rectangle& rectangle = rectangles[i];
    for (int edge_int = 0; edge_int < 4; ++edge_int) {
      const EdgePosition edge = static_cast<EdgePosition>(edge_int);
      const auto box_neighbors = neighbours.GetSortedNeighbors(i, edge);
      if (box_neighbors.empty()) {
        if (edge == EdgePosition::LEFT || edge == EdgePosition::RIGHT) {
          vertical_edges_on_boundary.push_back(
              {Edge::GetEdge(rectangle, edge), i});
        } else {
          horizontal_edges_on_boundary.push_back(
              {Edge::GetEdge(rectangle, edge), i});
        }
        continue;
      }
      IntegerValue previous_pos = GetClockwiseStart(edge, rectangle);
      for (int n = 0; n <= box_neighbors.size(); ++n) {
        IntegerValue neighbor_start;
        const Rectangle* neighbor;
        if (n == box_neighbors.size()) {
          // On the last iteration we consider instead of the next neighbor the
          // end of the current box.
          neighbor_start = GetClockwiseEnd(edge, rectangle);
        } else {
          const int neighbor_idx = box_neighbors[n];
          neighbor = &rectangles[neighbor_idx];
          neighbor_start = GetClockwiseStart(edge, *neighbor);
        }
        switch (edge) {
          case EdgePosition::LEFT:
            if (neighbor_start > previous_pos) {
              vertical_edges_on_boundary.push_back(
                  {Edge{.x_start = rectangle.x_min,
                        .y_start = previous_pos,
                        .size = neighbor_start - previous_pos},
                   i});
            }
            break;
          case EdgePosition::RIGHT:
            if (neighbor_start < previous_pos) {
              vertical_edges_on_boundary.push_back(
                  {Edge{.x_start = rectangle.x_max,
                        .y_start = neighbor_start,
                        .size = previous_pos - neighbor_start},
                   i});
            }
            break;
          case EdgePosition::BOTTOM:
            if (neighbor_start < previous_pos) {
              horizontal_edges_on_boundary.push_back(
                  {Edge{.x_start = neighbor_start,
                        .y_start = rectangle.y_min,
                        .size = previous_pos - neighbor_start},
                   i});
            }
            break;
          case EdgePosition::TOP:
            if (neighbor_start > previous_pos) {
              horizontal_edges_on_boundary.push_back(
                  {Edge{.x_start = previous_pos,
                        .y_start = rectangle.y_max,
                        .size = neighbor_start - previous_pos},
                   i});
            }
            break;
        }
        if (n != box_neighbors.size()) {
          previous_pos = GetClockwiseEnd(edge, *neighbor);
        }
      }
    }
  }
}

// Trace a boundary (interior or exterior) that contains the edge described by
// starting_edge_position and starting_step_point. This method removes the edges
// that were added to the boundary from `segments_to_follow`.
ShapePath TraceBoundary(
    const EdgePosition& starting_edge_position,
    std::pair<IntegerValue, IntegerValue> starting_step_point,
    std::array<absl::btree_map<std::pair<IntegerValue, IntegerValue>,
                               std::pair<IntegerValue, int>>,
               4>& segments_to_follow) {
  // The boundary is composed of edges on the `segments_to_follow` map. So all
  // we need is to find and glue them together on the right order.
  ShapePath path;

  auto extracted =
      segments_to_follow[starting_edge_position].extract(starting_step_point);
  CHECK(!extracted.empty());
  const int first_index = extracted.mapped().second;

  std::pair<IntegerValue, IntegerValue> cur = starting_step_point;
  int cur_index = first_index;
  // Now we navigate from one edge to the next. To avoid going back, we remove
  // used edges from the hash map.
  while (true) {
    path.step_points.push_back(cur);

    bool can_go[4] = {false, false, false, false};
    EdgePosition direction_to_take = EdgePosition::LEFT;
    for (int edge_int = 0; edge_int < 4; ++edge_int) {
      const EdgePosition edge = static_cast<EdgePosition>(edge_int);
      if (segments_to_follow[edge].contains(cur)) {
        can_go[edge] = true;
        direction_to_take = edge;
      }
    }

    if (can_go == absl::Span<const bool>{false, false, false, false}) {
      // Cannot move anywhere, we closed the loop.
      break;
    }

    // Handle one pathological case.
    if (can_go[EdgePosition::LEFT] && can_go[EdgePosition::RIGHT]) {
      // Corner case (literally):
      // ********
      // ********
      // ********
      // ********
      //       ^ +++++++++
      //       | +++++++++
      //       | +++++++++
      //         +++++++++
      //
      // In this case we keep following the same box.
      auto it_x = segments_to_follow[EdgePosition::LEFT].find(cur);
      if (cur_index == it_x->second.second) {
        direction_to_take = EdgePosition::LEFT;
      } else {
        direction_to_take = EdgePosition::RIGHT;
      }
    } else if (can_go[EdgePosition::TOP] && can_go[EdgePosition::BOTTOM]) {
      auto it_y = segments_to_follow[EdgePosition::TOP].find(cur);
      if (cur_index == it_y->second.second) {
        direction_to_take = EdgePosition::TOP;
      } else {
        direction_to_take = EdgePosition::BOTTOM;
      }
    }

    auto extracted = segments_to_follow[direction_to_take].extract(cur);
    cur_index = extracted.mapped().second;
    switch (direction_to_take) {
      case EdgePosition::LEFT:
        cur.first -= extracted.mapped().first;
        segments_to_follow[EdgePosition::RIGHT].erase(
            cur);  // Forbid going back
        break;
      case EdgePosition::RIGHT:
        cur.first += extracted.mapped().first;
        segments_to_follow[EdgePosition::LEFT].erase(cur);  // Forbid going back
        break;
      case EdgePosition::TOP:
        cur.second += extracted.mapped().first;
        segments_to_follow[EdgePosition::BOTTOM].erase(
            cur);  // Forbid going back
        break;
      case EdgePosition::BOTTOM:
        cur.second -= extracted.mapped().first;
        segments_to_follow[EdgePosition::TOP].erase(cur);  // Forbid going back
        break;
    }
    path.touching_box_index.push_back(cur_index);
  }
  path.touching_box_index.push_back(cur_index);

  return path;
}
}  // namespace

std::vector<SingleShape> BoxesToShapes(absl::Span<const Rectangle> rectangles,
                                       const Neighbours& neighbours) {
  std::vector<std::pair<Edge, int>> vertical_edges_on_boundary;
  std::vector<std::pair<Edge, int>> horizontal_edges_on_boundary;
  GetAllSegmentsTouchingVoid(rectangles, neighbours, vertical_edges_on_boundary,
                             horizontal_edges_on_boundary);

  std::array<absl::btree_map<std::pair<IntegerValue, IntegerValue>,
                             std::pair<IntegerValue, int>>,
             4>
      segments_to_follow;

  for (const auto& [edge, box_index] : vertical_edges_on_boundary) {
    segments_to_follow[EdgePosition::TOP][{edge.x_start, edge.y_start}] = {
        edge.size, box_index};
    segments_to_follow[EdgePosition::BOTTOM][{
        edge.x_start, edge.y_start + edge.size}] = {edge.size, box_index};
  }
  for (const auto& [edge, box_index] : horizontal_edges_on_boundary) {
    segments_to_follow[EdgePosition::RIGHT][{edge.x_start, edge.y_start}] = {
        edge.size, box_index};
    segments_to_follow[EdgePosition::LEFT][{
        edge.x_start + edge.size, edge.y_start}] = {edge.size, box_index};
  }

  const auto components = SplitInConnectedComponents(neighbours);
  std::vector<SingleShape> result(components.size());
  std::vector<int> box_to_component(rectangles.size());
  for (int i = 0; i < components.size(); ++i) {
    for (const int box_index : components[i]) {
      box_to_component[box_index] = i;
    }
  }
  while (!segments_to_follow[EdgePosition::LEFT].empty()) {
    // Get edge most to the bottom left
    const int box_index =
        segments_to_follow[EdgePosition::RIGHT].begin()->second.second;
    const std::pair<IntegerValue, IntegerValue> starting_step_point =
        segments_to_follow[EdgePosition::RIGHT].begin()->first;
    const int component_index = box_to_component[box_index];

    // The left-most vertical edge of the connected component must be of its
    // exterior boundary. So we must always see the exterior boundary before
    // seeing any holes.
    const bool is_hole = !result[component_index].boundary.step_points.empty();
    ShapePath& path = is_hole ? result[component_index].holes.emplace_back()
                              : result[component_index].boundary;
    path = TraceBoundary(EdgePosition::RIGHT, starting_step_point,
                         segments_to_follow);
    if (is_hole) {
      // Follow the usual convention that holes are in the inverse orientation
      // of the external boundary.
      absl::c_reverse(path.step_points);
      absl::c_reverse(path.touching_box_index);
    }
  }
  return result;
}

namespace {
struct PolygonCut {
  std::pair<IntegerValue, IntegerValue> start;
  std::pair<IntegerValue, IntegerValue> end;
  int start_index;
  int end_index;

  struct CmpByStartY {
    bool operator()(const PolygonCut& a, const PolygonCut& b) const {
      return std::tie(a.start.second, a.start.first) <
             std::tie(b.start.second, b.start.first);
    }
  };

  struct CmpByEndY {
    bool operator()(const PolygonCut& a, const PolygonCut& b) const {
      return std::tie(a.end.second, a.end.first) <
             std::tie(b.end.second, b.end.first);
    }
  };

  struct CmpByStartX {
    bool operator()(const PolygonCut& a, const PolygonCut& b) const {
      return a.start < b.start;
    }
  };

  struct CmpByEndX {
    bool operator()(const PolygonCut& a, const PolygonCut& b) const {
      return a.end < b.end;
    }
  };

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const PolygonCut& diagonal) {
    absl::Format(&sink, "(%v,%v)-(%v,%v)", diagonal.start.first,
                 diagonal.start.second, diagonal.end.first,
                 diagonal.end.second);
  }
};

// A different representation of a shape. The two vectors must have the same
// size. The first one contains the points of the shape and the second one
// contains the index of the next point in the shape.
//
// Note that we code in this file is only correct for shapes with points
// connected only by horizontal or vertical lines.
struct FlatShape {
  std::vector<std::pair<IntegerValue, IntegerValue>> points;
  std::vector<int> next;
};

EdgePosition GetSegmentDirection(
    const std::pair<IntegerValue, IntegerValue>& curr_segment,
    const std::pair<IntegerValue, IntegerValue>& next_segment) {
  if (curr_segment.first == next_segment.first) {
    return next_segment.second > curr_segment.second ? EdgePosition::TOP
                                                     : EdgePosition::BOTTOM;
  } else {
    return next_segment.first > curr_segment.first ? EdgePosition::RIGHT
                                                   : EdgePosition::LEFT;
  }
}

// Given a polygon, this function returns all line segments that start on a
// concave vertex and follow horizontally or vertically until it reaches the
// border of the polygon. This function returns all such segments grouped on the
// direction the line takes after starting in the concave vertex. Some of those
// segments start and end on a convex vertex, so they will appear twice in the
// output. This function modifies the shape by splitting some of the path
// segments in two. This is needed to make sure that `PolygonCut.start_index`
// and `PolygonCut.end_index` always corresponds to points in the FlatShape,
// even if they are not edges.
std::array<std::vector<PolygonCut>, 4> GetPotentialPolygonCuts(
    FlatShape& shape) {
  std::array<std::vector<PolygonCut>, 4> cuts;

  // First, for each concave vertex we create a cut that starts at it and
  // crosses the polygon until infinite (in practice, int_max/int_min).
  for (int i = 0; i < shape.points.size(); i++) {
    const auto& it = &shape.points[shape.next[i]];
    const auto& previous = &shape.points[i];
    const auto& next_segment = &shape.points[shape.next[shape.next[i]]];
    const EdgePosition previous_dir = GetSegmentDirection(*previous, *it);
    const EdgePosition next_dir = GetSegmentDirection(*it, *next_segment);

    if ((previous_dir == EdgePosition::TOP && next_dir == EdgePosition::LEFT) ||
        (previous_dir == EdgePosition::RIGHT &&
         next_dir == EdgePosition::TOP)) {
      cuts[EdgePosition::RIGHT].push_back(
          {.start = *it,
           .end = {std::numeric_limits<IntegerValue>::max(), it->second},
           .start_index = shape.next[i]});
    }
    if ((previous_dir == EdgePosition::BOTTOM &&
         next_dir == EdgePosition::RIGHT) ||
        (previous_dir == EdgePosition::LEFT &&
         next_dir == EdgePosition::BOTTOM)) {
      cuts[EdgePosition::LEFT].push_back(
          {.start = {std::numeric_limits<IntegerValue>::min(), it->second},
           .end = *it,
           .end_index = shape.next[i]});
    }
    if ((previous_dir == EdgePosition::RIGHT &&
         next_dir == EdgePosition::TOP) ||
        (previous_dir == EdgePosition::BOTTOM &&
         next_dir == EdgePosition::RIGHT)) {
      cuts[EdgePosition::BOTTOM].push_back(
          {.start = {it->first, std::numeric_limits<IntegerValue>::min()},
           .end = *it,
           .end_index = shape.next[i]});
    }
    if ((previous_dir == EdgePosition::TOP && next_dir == EdgePosition::LEFT) ||
        (previous_dir == EdgePosition::LEFT &&
         next_dir == EdgePosition::BOTTOM)) {
      cuts[EdgePosition::TOP].push_back(
          {.start = *it,
           .end = {it->first, std::numeric_limits<IntegerValue>::max()},
           .start_index = shape.next[i]});
    }
  }

  // Now that we have one of the points of the segment (the one starting on a
  // vertex), we need to find the other point. This is basically finding the
  // first path segment that crosses each cut connecting edge->infinity we
  // collected above. We do a rather naive implementation of that below and its
  // complexity is O(N^2) even if it should be fast in most cases. If it
  // turns out to be costly on profiling we can use a more sophisticated
  // algorithm for finding the first intersection.

  // We need to sort the cuts so we can use binary search to quickly find cuts
  // that cross a segment.
  std::sort(cuts[EdgePosition::RIGHT].begin(), cuts[EdgePosition::RIGHT].end(),
            PolygonCut::CmpByStartY());
  std::sort(cuts[EdgePosition::LEFT].begin(), cuts[EdgePosition::LEFT].end(),
            PolygonCut::CmpByEndY());
  std::sort(cuts[EdgePosition::BOTTOM].begin(),
            cuts[EdgePosition::BOTTOM].end(), PolygonCut::CmpByEndX());
  std::sort(cuts[EdgePosition::TOP].begin(), cuts[EdgePosition::TOP].end(),
            PolygonCut::CmpByStartX());

  // This function cuts a segment in two if it crosses a cut. In any case, it
  // returns the index of a point `point_idx` so that `shape.points[point_idx]
  // == point_to_cut`.
  const auto cut_segment_if_necessary =
      [&shape](int segment_idx,
               std::pair<IntegerValue, IntegerValue> point_to_cut) {
        const auto& cur = shape.points[segment_idx];
        const auto& next = shape.points[shape.next[segment_idx]];
        if (cur.second == next.second) {
          DCHECK_EQ(point_to_cut.second, cur.second);
          // We have a horizontal segment
          const IntegerValue edge_start = std::min(cur.first, next.first);
          const IntegerValue edge_end = std::max(cur.first, next.first);

          if (edge_start < point_to_cut.first &&
              point_to_cut.first < edge_end) {
            shape.points.push_back(point_to_cut);
            const int next_idx = shape.next[segment_idx];
            shape.next[segment_idx] = shape.points.size() - 1;
            shape.next.push_back(next_idx);
            return static_cast<int>(shape.points.size() - 1);
          }
          return (shape.points[segment_idx] == point_to_cut)
                     ? segment_idx
                     : shape.next[segment_idx];
        } else {
          DCHECK_EQ(cur.first, next.first);
          DCHECK_EQ(point_to_cut.first, cur.first);
          // We have a vertical segment
          const IntegerValue edge_start = std::min(cur.second, next.second);
          const IntegerValue edge_end = std::max(cur.second, next.second);

          if (edge_start < point_to_cut.second &&
              point_to_cut.second < edge_end) {
            shape.points.push_back(point_to_cut);
            const int next_idx = shape.next[segment_idx];
            shape.next[segment_idx] = shape.points.size() - 1;
            shape.next.push_back(next_idx);
            return static_cast<int>(shape.points.size() - 1);
          }
          return (shape.points[segment_idx] == point_to_cut)
                     ? segment_idx
                     : shape.next[segment_idx];
        }
      };

  for (int i = 0; i < shape.points.size(); i++) {
    const auto* cur_point_ptr = &shape.points[shape.next[i]];
    const auto* previous = &shape.points[i];
    DCHECK(cur_point_ptr->first == previous->first ||
           cur_point_ptr->second == previous->second)
        << "found a segment that is neither horizontal nor vertical";
    const EdgePosition direction =
        GetSegmentDirection(*previous, *cur_point_ptr);

    if (direction == EdgePosition::BOTTOM) {
      const auto cut_start = absl::c_lower_bound(
          cuts[EdgePosition::RIGHT],
          PolygonCut{.start = {std::numeric_limits<IntegerValue>::min(),
                               cur_point_ptr->second}},
          PolygonCut::CmpByStartY());
      auto cut_end = absl::c_upper_bound(
          cuts[EdgePosition::RIGHT],
          PolygonCut{.start = {std::numeric_limits<IntegerValue>::max(),
                               previous->second}},
          PolygonCut::CmpByStartY());

      for (auto cut_it = cut_start; cut_it < cut_end; ++cut_it) {
        PolygonCut& diagonal = *cut_it;
        const IntegerValue diagonal_start_x = diagonal.start.first;
        const IntegerValue diagonal_cur_end_x = diagonal.end.first;
        // Our binary search guarantees those two conditions.
        DCHECK_LE(cur_point_ptr->second, diagonal.start.second);
        DCHECK_LE(diagonal.start.second, previous->second);

        // Let's test if the diagonal crosses the current boundary segment
        if (diagonal_start_x <= previous->first &&
            diagonal_cur_end_x > cur_point_ptr->first) {
          DCHECK_LT(diagonal_start_x, cur_point_ptr->first);
          DCHECK_LE(previous->first, diagonal_cur_end_x);

          diagonal.end.first = cur_point_ptr->first;

          diagonal.end_index = cut_segment_if_necessary(i, diagonal.end);
          DCHECK(shape.points[diagonal.end_index] == diagonal.end);

          // Subtle: cut_segment_if_necessary might add new points to the vector
          // of the shape, so the pointers computed from it might become
          // invalid. Moreover, the current segment now is shorter, so we need
          // to update our upper bound.
          cur_point_ptr = &shape.points[shape.next[i]];
          previous = &shape.points[i];
          cut_end = absl::c_upper_bound(
              cuts[EdgePosition::RIGHT],
              PolygonCut{.start = {std::numeric_limits<IntegerValue>::max(),
                                   previous->second}},
              PolygonCut::CmpByStartY());
        }
      }
    }

    if (direction == EdgePosition::TOP) {
      const auto cut_start = absl::c_lower_bound(
          cuts[EdgePosition::LEFT],
          PolygonCut{.end = {std::numeric_limits<IntegerValue>::min(),
                             previous->second}},
          PolygonCut::CmpByEndY());
      auto cut_end = absl::c_upper_bound(
          cuts[EdgePosition::LEFT],
          PolygonCut{.end = {std::numeric_limits<IntegerValue>::max(),
                             cur_point_ptr->second}},
          PolygonCut::CmpByEndY());
      for (auto cut_it = cut_start; cut_it < cut_end; ++cut_it) {
        PolygonCut& diagonal = *cut_it;
        const IntegerValue diagonal_start_x = diagonal.start.first;
        const IntegerValue diagonal_cur_end_x = diagonal.end.first;
        // Our binary search guarantees those two conditions.
        DCHECK_LE(diagonal.end.second, cur_point_ptr->second);
        DCHECK_LE(previous->second, diagonal.end.second);

        // Let's test if the diagonal crosses the current boundary segment
        if (diagonal_start_x < cur_point_ptr->first &&
            previous->first <= diagonal_cur_end_x) {
          DCHECK_LT(cur_point_ptr->first, diagonal_cur_end_x);
          DCHECK_LE(diagonal_start_x, previous->first);

          diagonal.start.first = cur_point_ptr->first;
          diagonal.start_index = cut_segment_if_necessary(i, diagonal.start);
          DCHECK(shape.points[diagonal.start_index] == diagonal.start);
          cur_point_ptr = &shape.points[shape.next[i]];
          previous = &shape.points[i];
          cut_end = absl::c_upper_bound(
              cuts[EdgePosition::LEFT],
              PolygonCut{.end = {std::numeric_limits<IntegerValue>::max(),
                                 cur_point_ptr->second}},
              PolygonCut::CmpByEndY());
        }
      }
    }

    if (direction == EdgePosition::LEFT) {
      const auto cut_start = absl::c_lower_bound(
          cuts[EdgePosition::BOTTOM],
          PolygonCut{.end = {cur_point_ptr->first,
                             std::numeric_limits<IntegerValue>::min()}},
          PolygonCut::CmpByEndX());
      auto cut_end = absl::c_upper_bound(
          cuts[EdgePosition::BOTTOM],
          PolygonCut{.end = {previous->first,
                             std::numeric_limits<IntegerValue>::max()}},
          PolygonCut::CmpByEndX());
      for (auto cut_it = cut_start; cut_it < cut_end; ++cut_it) {
        PolygonCut& diagonal = *cut_it;
        const IntegerValue diagonal_start_y = diagonal.start.second;
        const IntegerValue diagonal_cur_end_y = diagonal.end.second;

        // Our binary search guarantees those two conditions.
        DCHECK_LE(cur_point_ptr->first, diagonal.end.first);
        DCHECK_LE(diagonal.end.first, previous->first);

        // Let's test if the diagonal crosses the current boundary segment
        if (diagonal_start_y < cur_point_ptr->second &&
            cur_point_ptr->second <= diagonal_cur_end_y) {
          DCHECK_LE(diagonal_start_y, previous->second);
          DCHECK_LT(cur_point_ptr->second, diagonal_cur_end_y);

          diagonal.start.second = cur_point_ptr->second;
          diagonal.start_index = cut_segment_if_necessary(i, diagonal.start);
          DCHECK(shape.points[diagonal.start_index] == diagonal.start);
          cur_point_ptr = &shape.points[shape.next[i]];
          previous = &shape.points[i];
          cut_end = absl::c_upper_bound(
              cuts[EdgePosition::BOTTOM],
              PolygonCut{.end = {previous->first,
                                 std::numeric_limits<IntegerValue>::max()}},
              PolygonCut::CmpByEndX());
        }
      }
    }

    if (direction == EdgePosition::RIGHT) {
      const auto cut_start = absl::c_lower_bound(
          cuts[EdgePosition::TOP],
          PolygonCut{.start = {previous->first,
                               std::numeric_limits<IntegerValue>::min()}},
          PolygonCut::CmpByStartX());
      auto cut_end = absl::c_upper_bound(
          cuts[EdgePosition::TOP],
          PolygonCut{.start = {cur_point_ptr->first,
                               std::numeric_limits<IntegerValue>::max()}},
          PolygonCut::CmpByStartX());
      for (auto cut_it = cut_start; cut_it < cut_end; ++cut_it) {
        PolygonCut& diagonal = *cut_it;
        const IntegerValue diagonal_start_y = diagonal.start.second;
        const IntegerValue diagonal_cur_end_y = diagonal.end.second;

        // Our binary search guarantees those two conditions.
        DCHECK_LE(previous->first, diagonal.start.first);
        DCHECK_LE(diagonal.start.first, cur_point_ptr->first);

        // Let's test if the diagonal crosses the current boundary segment
        if (diagonal_start_y <= cur_point_ptr->second &&
            cur_point_ptr->second < diagonal_cur_end_y) {
          DCHECK_LT(diagonal_start_y, previous->second);
          DCHECK_LE(cur_point_ptr->second, diagonal_cur_end_y);

          diagonal.end.second = cur_point_ptr->second;
          diagonal.end_index = cut_segment_if_necessary(i, diagonal.end);
          DCHECK(shape.points[diagonal.end_index] == diagonal.end);
          cur_point_ptr = &shape.points[shape.next[i]];
          cut_end = absl::c_upper_bound(
              cuts[EdgePosition::TOP],
              PolygonCut{.start = {cur_point_ptr->first,
                                   std::numeric_limits<IntegerValue>::max()}},
              PolygonCut::CmpByStartX());
          previous = &shape.points[i];
        }
      }
    }
  }
  return cuts;
}

void CutShapeWithPolygonCuts(FlatShape& shape,
                             absl::Span<const PolygonCut> cuts) {
  std::vector<int> previous(shape.points.size(), -1);
  for (int i = 0; i < shape.points.size(); i++) {
    previous[shape.next[i]] = i;
  }

  std::vector<std::pair<int, int>> cut_previous_index(cuts.size(), {-1, -1});
  for (int i = 0; i < cuts.size(); i++) {
    DCHECK(cuts[i].start == shape.points[cuts[i].start_index]);
    DCHECK(cuts[i].end == shape.points[cuts[i].end_index]);

    cut_previous_index[i].first = previous[cuts[i].start_index];
    cut_previous_index[i].second = previous[cuts[i].end_index];
  }

  for (const auto& [i, j] : cut_previous_index) {
    const int prev_start_next = shape.next[i];
    const int prev_end_next = shape.next[j];
    const std::pair<IntegerValue, IntegerValue> start =
        shape.points[prev_start_next];
    const std::pair<IntegerValue, IntegerValue> end =
        shape.points[prev_end_next];

    shape.points.push_back(start);
    shape.next[i] = shape.points.size() - 1;
    shape.next.push_back(prev_end_next);

    shape.points.push_back(end);
    shape.next[j] = shape.points.size() - 1;
    shape.next.push_back(prev_start_next);
  }
}
}  // namespace

// This function applies the method described in page 3 of [1].
//
// [1] Eppstein, David. "Graph-theoretic solutions to computational geometry
// problems." International Workshop on Graph-Theoretic Concepts in Computer
// Science. Berlin, Heidelberg: Springer Berlin Heidelberg, 2009.
std::vector<Rectangle> CutShapeIntoRectangles(SingleShape shape) {
  auto is_aligned = [](const std::pair<IntegerValue, IntegerValue>& p1,
                       const std::pair<IntegerValue, IntegerValue>& p2,
                       const std::pair<IntegerValue, IntegerValue>& p3) {
    return ((p1.first == p2.first) == (p2.first == p3.first)) &&
           ((p1.second == p2.second) == (p2.second == p3.second));
  };
  const auto add_segment =
      [&is_aligned](const std::pair<IntegerValue, IntegerValue>& segment,
                    const int start_index,
                    std::vector<std::pair<IntegerValue, IntegerValue>>& points,
                    std::vector<int>& next) {
        if (points.size() > 1 + start_index &&
            is_aligned(points[points.size() - 1], points[points.size() - 2],
                       segment)) {
          points.back() = segment;
        } else {
          points.push_back(segment);
          next.push_back(points.size());
        }
      };

  // To cut our polygon into rectangles, we first put it into a data structure
  // that is easier to manipulate.
  FlatShape flat_shape;
  for (int i = 0; 1 + i < shape.boundary.step_points.size(); ++i) {
    const std::pair<IntegerValue, IntegerValue>& segment =
        shape.boundary.step_points[i];
    add_segment(segment, 0, flat_shape.points, flat_shape.next);
  }
  flat_shape.next.back() = 0;
  for (const ShapePath& hole : shape.holes) {
    const int start = flat_shape.next.size();
    if (hole.step_points.size() < 2) continue;
    for (int i = 0; i + 1 < hole.step_points.size(); ++i) {
      const std::pair<IntegerValue, IntegerValue>& segment =
          hole.step_points[i];
      add_segment(segment, start, flat_shape.points, flat_shape.next);
    }
    flat_shape.next.back() = start;
  }

  std::array<std::vector<PolygonCut>, 4> all_cuts =
      GetPotentialPolygonCuts(flat_shape);

  // Some cuts connect two concave edges and will be duplicated in all_cuts.
  // Those are important: since they "fix" two concavities with a single cut,
  // they are called "good diagonals" in the literature. Note that in
  // computational geometry jargon, a diagonal of a polygon is a line segment
  // that connects two non-adjacent vertices of a polygon, even in cases like
  // ours that we are only talking of diagonals that are not "diagonal" in the
  // usual meaning of the word: ie., horizontal or vertical segments connecting
  // two vertices of the polygon).
  std::array<std::vector<PolygonCut>, 2> good_diagonals;
  for (const auto& d : all_cuts[EdgePosition::BOTTOM]) {
    if (absl::c_binary_search(all_cuts[EdgePosition::TOP], d,
                              PolygonCut::CmpByStartX())) {
      good_diagonals[0].push_back(d);
    }
  }
  for (const auto& d : all_cuts[EdgePosition::LEFT]) {
    if (absl::c_binary_search(all_cuts[EdgePosition::RIGHT], d,
                              PolygonCut::CmpByStartY())) {
      good_diagonals[1].push_back(d);
    }
  }

  // The "good diagonals" are only more optimal that any cut if they are not
  // crossed by other cuts. To maximize their usefulness, we build a graph where
  // the good diagonals are the vertices and we add an edge every time a
  // vertical and horizontal diagonal cross. The minimum vertex cover of this
  // graph is the minimal set of good diagonals that are not crossed by other
  // cuts.
  std::vector<std::vector<int>> arcs(good_diagonals[0].size());
  for (int i = 0; i < good_diagonals[0].size(); ++i) {
    for (int j = 0; j < good_diagonals[1].size(); ++j) {
      const PolygonCut& vertical = good_diagonals[0][i];
      const PolygonCut& horizontal = good_diagonals[1][j];
      const IntegerValue vertical_x = vertical.start.first;
      const IntegerValue horizontal_y = horizontal.start.second;
      if (horizontal.start.first <= vertical_x &&
          vertical_x <= horizontal.end.first &&
          vertical.start.second <= horizontal_y &&
          horizontal_y <= vertical.end.second) {
        arcs[i].push_back(good_diagonals[0].size() + j);
      }
    }
  }

  const std::vector<bool> minimum_cover =
      BipartiteMinimumVertexCover(arcs, good_diagonals[1].size());

  std::vector<PolygonCut> minimum_cover_horizontal_diagonals;
  for (int i = good_diagonals[0].size();
       i < good_diagonals[0].size() + good_diagonals[1].size(); ++i) {
    if (minimum_cover[i]) continue;
    minimum_cover_horizontal_diagonals.push_back(
        good_diagonals[1][i - good_diagonals[0].size()]);
  }

  // Since our data structure only allow to cut the shape according to a list
  // of vertical or horizontal cuts, but not a list mixing both, we cut first
  // on the chosen horizontal good diagonals.
  CutShapeWithPolygonCuts(flat_shape, minimum_cover_horizontal_diagonals);

  // We need to recompute the cuts after we applied the good diagonals, since
  // the geometry has changed.
  all_cuts = GetPotentialPolygonCuts(flat_shape);

  // Now that we did all horizontal good diagonals, we need to cut on all
  // vertical good diagonals and then cut arbitrarily to remove all concave
  // edges. To make things simple, just apply all vertical cuts, since they
  // include all the vertical good diagonals and also fully slice the shape into
  // rectangles.

  // Remove duplicates coming from good diagonals first.
  std::vector<PolygonCut> cuts = all_cuts[EdgePosition::TOP];
  for (const auto& cut : all_cuts[EdgePosition::BOTTOM]) {
    if (!absl::c_binary_search(all_cuts[EdgePosition::TOP], cut,
                               PolygonCut::CmpByStartX())) {
      cuts.push_back(cut);
    }
  }

  CutShapeWithPolygonCuts(flat_shape, cuts);

  // Now every connected component of the shape is a rectangle. Build the final
  // result.
  std::vector<Rectangle> result;
  std::vector<bool> seen(flat_shape.points.size(), false);
  for (int i = 0; i < flat_shape.points.size(); ++i) {
    if (seen[i]) continue;
    Rectangle& rectangle = result.emplace_back(Rectangle{
        .x_min = std::numeric_limits<IntegerValue>::max(),
        .x_max = std::numeric_limits<IntegerValue>::min(),
        .y_min = std::numeric_limits<IntegerValue>::max(),
        .y_max = std::numeric_limits<IntegerValue>::min(),
    });
    int cur = i;
    do {
      seen[cur] = true;
      rectangle.GrowToInclude({.x_min = flat_shape.points[cur].first,
                               .x_max = flat_shape.points[cur].first,
                               .y_min = flat_shape.points[cur].second,
                               .y_max = flat_shape.points[cur].second});
      cur = flat_shape.next[cur];
      DCHECK_LT(cur, flat_shape.next.size());
    } while (cur != i);
  }

  return result;
}

bool ReduceNumberOfBoxesExactMandatory(
    std::vector<Rectangle>* mandatory_rectangles,
    std::vector<Rectangle>* optional_rectangles) {
  if (mandatory_rectangles->empty()) return false;
  std::vector<Rectangle> result = *mandatory_rectangles;
  std::vector<Rectangle> new_optional_rectangles = *optional_rectangles;

  // This heuristic can be slow for very large problems, so gate it with a
  // reasonable limit.
  if (mandatory_rectangles->size() < 1000) {
    Rectangle mandatory_bounding_box = (*mandatory_rectangles)[0];
    for (const Rectangle& box : *mandatory_rectangles) {
      mandatory_bounding_box.GrowToInclude(box);
    }
    const std::vector<Rectangle> mandatory_empty_holes =
        FindEmptySpaces(mandatory_bounding_box, *mandatory_rectangles);
    const std::vector<std::vector<int>> mandatory_holes_components =
        SplitInConnectedComponents(BuildNeighboursGraph(mandatory_empty_holes));

    // Now for every connected component of the holes in the mandatory area, see
    // if we can fill them with optional boxes.
    std::vector<Rectangle> holes_in_component;
    for (const std::vector<int>& component : mandatory_holes_components) {
      holes_in_component.clear();
      holes_in_component.reserve(component.size());
      for (const int index : component) {
        holes_in_component.push_back(mandatory_empty_holes[index]);
      }
      if (RegionIncludesOther(new_optional_rectangles, holes_in_component)) {
        // Fill the hole.
        result.insert(result.end(), holes_in_component.begin(),
                      holes_in_component.end());
        // We can modify `optional_rectangles` here since we know that if we
        // remove a hole this function will return true.
        new_optional_rectangles = PavedRegionDifference(
            new_optional_rectangles, std::move(holes_in_component));
      }
    }
  }
  const Neighbours neighbours = BuildNeighboursGraph(result);
  std::vector<SingleShape> shapes = BoxesToShapes(result, neighbours);

  std::vector<Rectangle> original_result;
  if (DEBUG_MODE) {
    original_result = result;
  }
  result.clear();
  for (SingleShape& shape : shapes) {
    // This is the function that applies the algorithm described in [1].
    const std::vector<Rectangle> cut_rectangles =
        CutShapeIntoRectangles(std::move(shape));
    result.insert(result.end(), cut_rectangles.begin(), cut_rectangles.end());
  }
  DCHECK(RegionIncludesOther(original_result, result) &&
         RegionIncludesOther(result, original_result));

  // It is possible that the algorithm actually increases the number of boxes.
  // See the "Problematic2" test.
  if (result.size() >= mandatory_rectangles->size()) return false;
  mandatory_rectangles->swap(result);
  optional_rectangles->swap(new_optional_rectangles);
  return true;
}

Disjoint2dPackingResult DetectDisjointRegionIn2dPacking(
    absl::Span<const RectangleInRange> non_fixed_boxes,
    absl::Span<const Rectangle> fixed_boxes, int max_num_components) {
  if (max_num_components <= 1) return {};

  IntegerValue min_x_size = std::numeric_limits<IntegerValue>::max();
  IntegerValue min_y_size = std::numeric_limits<IntegerValue>::max();

  CHECK(!non_fixed_boxes.empty());
  Rectangle bounding_box = non_fixed_boxes[0].bounding_area;

  for (const RectangleInRange& box : non_fixed_boxes) {
    bounding_box.GrowToInclude(box.bounding_area);
    min_x_size = std::min(min_x_size, box.x_size);
    min_y_size = std::min(min_y_size, box.y_size);
  }
  DCHECK_GT(min_x_size, 0);
  DCHECK_GT(min_y_size, 0);

  std::vector<Rectangle> optional_boxes = FindSpacesThatCannotBeOccupied(
      fixed_boxes, non_fixed_boxes, bounding_box, min_x_size, min_y_size);
  std::vector<Rectangle> unoccupiable_space = {fixed_boxes.begin(),
                                               fixed_boxes.end()};
  unoccupiable_space.insert(unoccupiable_space.end(), optional_boxes.begin(),
                            optional_boxes.end());

  std::vector<Rectangle> occupiable_space =
      FindEmptySpaces(bounding_box, unoccupiable_space);

  std::vector<Rectangle> empty;
  ReduceNumberofBoxesGreedy(&occupiable_space, &empty);
  std::vector<std::vector<int>> space_components =
      SplitInConnectedComponents(BuildNeighboursGraph(occupiable_space));

  if (space_components.size() == 1 ||
      space_components.size() > max_num_components) {
    return {};
  }

  // If we are here, that means that the space where boxes can be placed is not
  // connected.
  Disjoint2dPackingResult result;
  absl::flat_hash_set<int> component_set;
  for (const std::vector<int>& component : space_components) {
    Rectangle bin_bounding_box = occupiable_space[component[0]];
    for (int i = 1; i < component.size(); ++i) {
      bin_bounding_box.GrowToInclude(occupiable_space[component[i]]);
    }
    std::optional<Rectangle> reachable_area_bounding_box;
    Disjoint2dPackingResult::Bin& bin = result.bins.emplace_back();
    for (int idx : component) {
      bin.bin_area.push_back(occupiable_space[idx]);
    }
    for (int i = 0; i < non_fixed_boxes.size(); i++) {
      if (!non_fixed_boxes[i].bounding_area.IsDisjoint(bin_bounding_box)) {
        if (reachable_area_bounding_box.has_value()) {
          reachable_area_bounding_box->GrowToInclude(
              non_fixed_boxes[i].bounding_area);
        } else {
          reachable_area_bounding_box = non_fixed_boxes[i].bounding_area;
        }
        bin.non_fixed_box_indexes.push_back(i);
      }
    }
    if (bin.non_fixed_box_indexes.empty()) {
      result.bins.pop_back();
      continue;
    }
    bin.fixed_boxes =
        FindEmptySpaces(*reachable_area_bounding_box, bin.bin_area);
    ReduceNumberofBoxesGreedy(&bin.fixed_boxes, &empty);
  }
  VLOG_EVERY_N_SEC(1, 1) << "Detected a bin packing problem with "
                         << result.bins.size()
                         << " bins. Original problem sizes: "
                         << non_fixed_boxes.size() << " non-fixed boxes, "
                         << fixed_boxes.size() << " fixed boxes.";
  return result;
}

}  // namespace sat
}  // namespace operations_research
