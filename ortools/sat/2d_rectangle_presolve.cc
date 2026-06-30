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
#include <deque>
#include <iterator>
#include <limits>
#include <optional>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/log_severity.h"
#include "ortools/graph/minimum_vertex_cover.h"
#include "ortools/graph_base/strongly_connected_components.h"
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
  std::vector<Rectangle> optional_boxes(fixed_boxes.begin(), fixed_boxes.end());
  // All items we added to `optional_boxes` at this point are only to be used by
  // the "gap between items" logic below. They are not actual optional boxes and
  // should be removed right after the logic is applied.
  const int num_optional_boxes_to_remove = optional_boxes.size();

  // Add a rectangle to `optional_boxes` while respecting that the set of
  // rectangles in `optional_boxes` must remain mutually disjoint.
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
  // using a sweep line algorithm. Note that supporting non-disjoint input is
  // necessary here, so we cannot use FindEmptySpacesHorizontally.
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

  // Look for horizontal gaps that are too narrow to place any non-fixed item.
  // We can write directly into `optional_boxes` instead of calling `add_box`
  // because FindEmptySpacesHorizontally is guaranteed to return disjoint
  // rectangles that are also disjoint from the input `dead_spaces`.
  for (const auto& space :
       FindEmptySpacesHorizontally(bounding_box, optional_boxes)) {
    if (space.rect.x_max - space.rect.x_min < min_x_size) {
      optional_boxes.push_back(space.rect);
    }
  }

  // Look for vertical gaps that are too short to place any non-fixed item.
  for (const auto& space :
       FindEmptySpacesVertically(bounding_box, optional_boxes)) {
    if (space.rect.y_max - space.rect.y_min < min_y_size) {
      optional_boxes.push_back(space.rect);
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
  if (fixed_boxes->size() > 1) {
    std::vector<Rectangle> empty_vec;
    if (ReduceNumberofBoxesGreedy(fixed_boxes, &empty_vec)) {
      changed = true;
    }
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
    LOG(FATAL) << "Invalid edge position: " << static_cast<int>(pos);
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
  std::deque<Rectangle> rectangle_storage;
  enum class OptionalEnum { OPTIONAL, MANDATORY };
  // bool for is_optional
  std::vector<std::pair<const Rectangle*, OptionalEnum>> rectangles_ptr;
  const int total_rectangles =
      mandatory_rectangles->size() + optional_rectangles->size();
  absl::flat_hash_map<Edge, int> top_edges_to_rectangle(total_rectangles);
  absl::flat_hash_map<Edge, int> bottom_edges_to_rectangle(total_rectangles);
  absl::flat_hash_map<Edge, int> left_edges_to_rectangle(total_rectangles);
  absl::flat_hash_map<Edge, int> right_edges_to_rectangle(total_rectangles);

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
      rectangle_storage.push_back(rectangle);
      Rectangle& new_rectangle = rectangle_storage.back();
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
  // the bottom of a rectangle touches the top of another one must be
  // consecutive in the vector.
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

  for (const bool bottom_vs_top : {true, false}) {
    //  bottom_vs_top: compare bottom of a box with top of another one.
    // !bottom_vs_top: compare left of a box with right of another one.
    const EdgePosition edge_position =
        bottom_vs_top ? EdgePosition::BOTTOM : EdgePosition::LEFT;
    const EdgePosition opposite =
        bottom_vs_top ? EdgePosition::TOP : EdgePosition::RIGHT;
    auto it = edges_to_rectangle[edge_position].begin();
    const auto end = edges_to_rectangle[edge_position].end();

    for (const auto& [edge, index] : edges_to_rectangle[opposite]) {
      // Advance the main iterator only past intervals that finish completely
      // before our current 'edge' starts.
      while (it != end) {
        if (bottom_vs_top) {  // BOTTOM vs TOP.
          if (it->first.y_start < edge.y_start ||
              (it->first.y_start == edge.y_start &&
               it->first.x_start + it->first.size <= edge.x_start)) {
            ++it;
            continue;
          }
        } else {  // LEFT vs RIGHT.
          if (it->first.x_start < edge.x_start ||
              (it->first.x_start == edge.x_start &&
               it->first.y_start + it->first.size <= edge.y_start)) {
            ++it;
            continue;
          }
        }
        break;  // Stop advancing main iterator.
      }

      // Use a temporary scan iterator to find all overlaps for this specific
      // edge.
      auto scan_it = it;
      if (bottom_vs_top) {  // Vertical edges.
        while (scan_it != end && scan_it->first.y_start == edge.y_start &&
               scan_it->first.x_start < edge.x_start + edge.size) {
          neighbours.push_back({index, opposite, scan_it->second});
          neighbours.push_back({scan_it->second, edge_position, index});
          ++scan_it;
        }
      } else {  // Horizontal edges.
        while (scan_it != end && scan_it->first.x_start == edge.x_start &&
               scan_it->first.y_start < edge.y_start + edge.size) {
          neighbours.push_back({index, opposite, scan_it->second});
          neighbours.push_back({scan_it->second, edge_position, index});
          ++scan_it;
        }
      }
    }
  }

  using EdgeTuple = std::tuple<int, EdgePosition, int>;
  // We don't expect any duplicate neighbours.
  DCHECK_EQ(neighbours.size(),
            absl::flat_hash_set<EdgeTuple>(neighbours.begin(), neighbours.end())
                .size());
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
    const Rectangle& rect = rectangles[i];

    auto process_edge = [&](EdgePosition pos, IntegerValue min_bound,
                            IntegerValue max_bound, IntegerValue fixed_pos) {
      absl::Span<const int> neighbors = neighbours.GetSortedNeighbors(i, pos);

      // GetSortedNeighbors() returns the neighbors in clockwise order.
      const bool is_descending =
          (pos == EdgePosition::BOTTOM || pos == EdgePosition::RIGHT);
      const bool is_vertical =
          (pos == EdgePosition::LEFT || pos == EdgePosition::RIGHT);
      IntegerValue current_min = min_bound;

      auto add_result = [&](IntegerValue start, IntegerValue end) {
        const Edge e = is_vertical ? Edge{.x_start = fixed_pos,
                                          .y_start = start,
                                          .size = end - start}
                                   : Edge{.x_start = start,
                                          .y_start = fixed_pos,
                                          .size = end - start};
        std::vector<std::pair<Edge, int>>& edges =
            is_vertical ? vertical_edges_on_boundary
                        : horizontal_edges_on_boundary;
        edges.push_back({e, i});
      };

      auto process_neighbor = [&](int neighbor_idx) {
        const Rectangle& neighbor = rectangles[neighbor_idx];
        const IntegerValue n_min =
            is_vertical ? neighbor.y_min : neighbor.x_min;
        const IntegerValue n_max =
            is_vertical ? neighbor.y_max : neighbor.x_max;

        if (n_min > current_min) {
          add_result(current_min, n_min);
        }
        current_min = std::max(current_min, n_max);
      };

      if (is_descending) {
        for (auto it = neighbors.rbegin(); it != neighbors.rend(); ++it) {
          process_neighbor(*it);
        }
      } else {
        for (const int neighbor_idx : neighbors) {
          process_neighbor(neighbor_idx);
        }
      }

      if (current_min < max_bound) {
        add_result(current_min, max_bound);
      }
    };

    process_edge(EdgePosition::LEFT, rect.y_min, rect.y_max, rect.x_min);
    process_edge(EdgePosition::RIGHT, rect.y_min, rect.y_max, rect.x_max);
    process_edge(EdgePosition::BOTTOM, rect.x_min, rect.x_max, rect.y_min);
    process_edge(EdgePosition::TOP, rect.x_min, rect.x_max, rect.y_max);
  }
}

using Point = std::pair<IntegerValue, IntegerValue>;

// A directed rectangle edge.
struct BoundarySegment {
  Point start;
  Point destination;
  int box_index;
};

// Unified transparent comparator for sorting and binary searching.
struct CompareBoundarySegment {
  using is_transparent = void;

  bool operator()(const BoundarySegment& a, const BoundarySegment& b) const {
    return std::tie(a.start, a.destination) < std::tie(b.start, b.destination);
  }
  bool operator()(const BoundarySegment& e, const Point& p) const {
    return e.start < p;
  }
  bool operator()(const Point& p, const BoundarySegment& e) const {
    return p < e.start;
  }
};

// Trace a boundary (interior or exterior) that contains the edge described by
// `initial_edge_idx`. This method sets to true in `edge_used` the indexes of
// the edges that were added to the boundary and skips edges that have been used
// before.
ShapePath TraceBoundary(int initial_edge_idx,
                        const std::vector<BoundarySegment>& sorted_edges,
                        std::vector<bool>& edge_used) {
  ShapePath path;
  int current_idx = initial_edge_idx;
  const Point start_point = sorted_edges[current_idx].start;
  Point prev = start_point;

  while (true) {
    const BoundarySegment& current_seg = sorted_edges[current_idx];
    path.step_points.push_back(prev);
    path.touching_box_index.push_back(current_seg.box_index);

    const Point cur = current_seg.destination;

    edge_used[current_idx] = true;

    if (cur == start_point) {
      break;
    }

    const auto lower_it =
        std::lower_bound(sorted_edges.begin(), sorted_edges.end(), cur,
                         CompareBoundarySegment());
    const int start_idx =
        static_cast<int>(std::distance(sorted_edges.begin(), lower_it));

    // We should have typically one outgoing edge, and at most two in case of a
    // pinch-point like the one in the example below:
    // ********
    // ********
    // ********
    // ********
    //       ^ +++++++++
    //       | +++++++++
    //       | +++++++++
    //         +++++++++
    //
    int outgoing_edges = 0;
    while (start_idx + outgoing_edges < sorted_edges.size() &&
           sorted_edges[start_idx + outgoing_edges].start == cur) {
      ++outgoing_edges;
    }

    DCHECK_LE(outgoing_edges, 2);

    // If we are in a pinch-point and have two options, prefer the one that
    // keeps following the same box.
    const int next_idx =
        (outgoing_edges == 2 &&
                 sorted_edges[start_idx].box_index != current_seg.box_index
             ? start_idx + 1
             : start_idx);

    DCHECK(!edge_used[next_idx]);

    prev = cur;
    current_idx = next_idx;
  }

  return path;
}

}  // namespace

std::vector<SingleShape> BoxesToShapes(absl::Span<const Rectangle> rectangles,
                                       const Neighbours& neighbours) {
  std::vector<std::pair<Edge, int>> vertical_edges_on_boundary;
  std::vector<std::pair<Edge, int>> horizontal_edges_on_boundary;

  GetAllSegmentsTouchingVoid(rectangles, neighbours, vertical_edges_on_boundary,
                             horizontal_edges_on_boundary);

  std::vector<BoundarySegment> sorted_edges;
  sorted_edges.reserve(vertical_edges_on_boundary.size() +
                       horizontal_edges_on_boundary.size());

  // We pick the right orientation for each edge so that we only produce
  // paths in the clockwise direction.
  for (const auto& [edge, box_index] : vertical_edges_on_boundary) {
    const Rectangle& rect = rectangles[box_index];
    if (edge.x_start == rect.x_max) {
      sorted_edges.push_back({{edge.x_start, edge.y_start + edge.size},
                              {edge.x_start, edge.y_start},
                              box_index});
    } else {
      sorted_edges.push_back({{edge.x_start, edge.y_start},
                              {edge.x_start, edge.y_start + edge.size},
                              box_index});
    }
  }

  for (const auto& [edge, box_index] : horizontal_edges_on_boundary) {
    const Rectangle& rect = rectangles[box_index];
    if (edge.y_start == rect.y_max) {
      sorted_edges.push_back({{edge.x_start, edge.y_start},
                              {edge.x_start + edge.size, edge.y_start},
                              box_index});
    } else {
      sorted_edges.push_back({{edge.x_start + edge.size, edge.y_start},
                              {edge.x_start, edge.y_start},
                              box_index});
    }
  }

  absl::c_sort(sorted_edges, CompareBoundarySegment());
  std::vector<bool> edge_used(sorted_edges.size(), false);

  const auto components = SplitInConnectedComponents(neighbours);
  std::vector<SingleShape> result(components.size());
  std::vector<int> box_to_component(rectangles.size());
  for (int i = 0; i < components.size(); ++i) {
    for (const int box_index : components[i]) {
      box_to_component[box_index] = i;
    }
  }

  for (int i = 0; i < sorted_edges.size(); ++i) {
    if (edge_used[i]) continue;

    const int box_index = sorted_edges[i].box_index;
    const int component_index = box_to_component[box_index];

    // The lexicographical sort (min X, min Y) acts as a free topological test.
    // For any given component, the very first unvisited edge we encounter is
    // mathematically guaranteed to be its absolute bottom-left extremity.
    // Because an extreme outer point cannot exist inside a hole, the first
    // path we extract for a component is always its exterior boundary.
    // Any subsequent paths found for this component must therefore be holes.
    const bool is_hole = !result[component_index].boundary.step_points.empty();
    ShapePath& path = is_hole ? result[component_index].holes.emplace_back()
                              : result[component_index].boundary;

    // Note that by keeping the solid on the right we are getting the exterior
    // boundary with clockwise orientation and the interior holes with
    // counter-clockwise orientation naturally.
    path = TraceBoundary(i, sorted_edges, edge_used);
  }

  return result;
}

namespace {

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

struct WallEvent {
  IntegerValue min_y;
  IntegerValue max_y;
  int wall_start_idx;
};

struct RayStartEvent {
  IntegerValue y;
  int ray_start_idx;
};

struct SweepEvent {
  IntegerValue x;
  // Note the order: WallEvent is index 0, RayStartEvent is index 1.
  std::variant<WallEvent, RayStartEvent> payload;

  bool operator<(const SweepEvent& other) const {
    if (x != other.x) return x < other.x;
    // Process Walls (index 0) before RayStartEvents (index 1) at the same X.
    return payload.index() < other.payload.index();
  }
};

std::vector<PolygonCut> ExtractRightCuts(FlatShape& shape) {
  std::vector<PolygonCut> cuts;
  const int initial_points = shape.points.size();
  std::vector<SweepEvent> events;
  events.reserve(2 * initial_points);

  // Gather all events.
  for (int i = 0; i < initial_points; i++) {
    const auto previous = shape.points[i];
    const auto it = shape.points[shape.next[i]];
    const auto next_segment = shape.points[shape.next[shape.next[i]]];

    const EdgePosition previous_dir = GetSegmentDirection(previous, it);
    const EdgePosition next_dir = GetSegmentDirection(it, next_segment);

    // Identify walls (downward vertical segments).
    if (previous_dir == EdgePosition::BOTTOM) {
      events.push_back(
          {.x = previous.first,
           .payload = WallEvent{.min_y = std::min(previous.second, it.second),
                                .max_y = std::max(previous.second, it.second),
                                .wall_start_idx = i}});
    }

    // Identify ray starts.
    if (previous_dir != next_dir) {
      if ((previous_dir == EdgePosition::TOP &&
           next_dir == EdgePosition::LEFT) ||
          (previous_dir == EdgePosition::RIGHT &&
           next_dir == EdgePosition::TOP)) {
        events.push_back({.x = it.first,
                          .payload = RayStartEvent{
                              .y = it.second, .ray_start_idx = shape.next[i]}});
      }
    }
  }
  // Sort events left-to-right.
  absl::c_sort(events);

  // Maps Y-coordinate to the starting vertex index of the ray.
  absl::btree_map<IntegerValue, int> active_rays;

  // Sweep Line pass.
  for (const SweepEvent& event : events) {
    if (const auto* ray_event = std::get_if<RayStartEvent>(&event.payload)) {
      // A new ray starts shooting right.
      active_rays[ray_event->y] = ray_event->ray_start_idx;
    } else if (const auto* wall_event =
                   std::get_if<WallEvent>(&event.payload)) {
      // Find all active rays whose Y coordinate hits this wall.
      auto it_start = active_rays.lower_bound(wall_event->min_y);
      auto it_end = active_rays.upper_bound(wall_event->max_y);

      for (auto it = it_start; it != it_end; ++it) {
        PolygonCut cut;
        cut.start = shape.points[it->second];
        cut.end = {event.x, it->first};
        cut.start_index = it->second;
        cut.end_index = wall_event->wall_start_idx;
        cuts.push_back(cut);
      }
      active_rays.erase(it_start, it_end);
    }
  }

  // Cut the shape along the detected cuts.
  const auto apply_cut = [&shape](int seg,
                                  std::pair<IntegerValue, IntegerValue> pt) {
    while (true) {
      const auto cur = shape.points[seg];
      const auto next = shape.points[shape.next[seg]];
      const IntegerValue min_y = std::min(cur.second, next.second);
      const IntegerValue max_y = std::max(cur.second, next.second);

      if (min_y < pt.second && pt.second < max_y) {
        shape.points.push_back(pt);
        const int next_idx = shape.next[seg];
        shape.next[seg] = shape.points.size() - 1;
        shape.next.push_back(next_idx);
        return static_cast<int>(shape.points.size() - 1);
      }
      if (cur == pt) return seg;
      if (next == pt) return shape.next[seg];
      seg = shape.next[seg];
    }
  };

  for (PolygonCut& ray : cuts) {
    if (ray.end_index != -1) ray.end_index = apply_cut(ray.end_index, ray.end);
  }

  return cuts;
}

// Helper functions for rotating a single point.
std::pair<IntegerValue, IntegerValue> RotatePoint90CW(
    const std::pair<IntegerValue, IntegerValue>& p) {
  return {p.second, -p.first};
}

std::pair<IntegerValue, IntegerValue> RotatePoint90CCW(
    const std::pair<IntegerValue, IntegerValue>& p) {
  return {-p.second, p.first};
}

std::pair<IntegerValue, IntegerValue> RotatePoint180(
    const std::pair<IntegerValue, IntegerValue>& p) {
  return {-p.first, -p.second};
}

// Helper to rotate the entire shape in place.
void RotateShape90CW(FlatShape& shape) {
  for (auto& p : shape.points) {
    p = RotatePoint90CW(p);
  }
}

}  // namespace

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
  std::array<std::vector<PolygonCut>, 4> all_cuts;

  // We will simply call ExtractRightCuts() four times, doing the right
  // rotations before and after the call.
  all_cuts[EdgePosition::RIGHT] = ExtractRightCuts(shape);

  RotateShape90CW(shape);
  auto top_cuts = ExtractRightCuts(shape);
  for (auto& cut : top_cuts) {
    cut.start = RotatePoint90CCW(cut.start);
    cut.end = RotatePoint90CCW(cut.end);
  }
  all_cuts[EdgePosition::TOP] = top_cuts;

  RotateShape90CW(shape);
  auto left_cuts = ExtractRightCuts(shape);
  for (auto& cut : left_cuts) {
    cut.start = RotatePoint180(cut.start);
    cut.end = RotatePoint180(cut.end);
  }
  all_cuts[EdgePosition::LEFT] = left_cuts;

  RotateShape90CW(shape);
  auto bottom_cuts = ExtractRightCuts(shape);
  for (auto& cut : bottom_cuts) {
    cut.start = RotatePoint90CW(cut.start);
    cut.end = RotatePoint90CW(cut.end);
  }
  all_cuts[EdgePosition::BOTTOM] = bottom_cuts;

  // Restore shape to original orientation.
  RotateShape90CW(shape);

  return all_cuts;
}

namespace {

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

FlatShape BuildFlatShape(const SingleShape& shape) {
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
  for (int i = 0; i < shape.boundary.step_points.size(); ++i) {
    const std::pair<IntegerValue, IntegerValue>& segment =
        shape.boundary.step_points[i];
    add_segment(segment, 0, flat_shape.points, flat_shape.next);
  }
  flat_shape.next.back() = 0;
  for (const ShapePath& hole : shape.holes) {
    const int start = flat_shape.next.size();
    if (hole.step_points.empty()) continue;
    for (int i = 0; i < hole.step_points.size(); ++i) {
      const std::pair<IntegerValue, IntegerValue>& segment =
          hole.step_points[i];
      add_segment(segment, start, flat_shape.points, flat_shape.next);
    }
    flat_shape.next.back() = start;
  }

  return flat_shape;
}

// This function applies the method described in page 3 of [1].
//
// [1] Eppstein, David. "Graph-theoretic solutions to computational geometry
// problems." International Workshop on Graph-Theoretic Concepts in Computer
// Science. Berlin, Heidelberg: Springer Berlin Heidelberg, 2009.
std::vector<Rectangle> CutShapeIntoRectangles(const SingleShape& shape) {
  FlatShape flat_shape = BuildFlatShape(shape);
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

  absl::flat_hash_set<
      std::tuple<IntegerValue, IntegerValue, IntegerValue, IntegerValue>>
      seen_diagonals;
  std::array<std::vector<PolygonCut>, 2> good_diagonals;
  for (const auto& d : all_cuts[EdgePosition::BOTTOM]) {
    seen_diagonals.insert(
        {d.start.first, d.start.second, d.end.first, d.end.second});
  }
  for (const auto& d : all_cuts[EdgePosition::TOP]) {
    // Good horizontal diagonals appear twice: once in BOTTOM and once,
    // reversed, in TOP.
    if (seen_diagonals.contains(
            {d.end.first, d.end.second, d.start.first, d.start.second})) {
      good_diagonals[0].push_back(d);
    }
  }
  seen_diagonals.clear();
  for (const auto& d : all_cuts[EdgePosition::LEFT]) {
    seen_diagonals.insert(
        {d.start.first, d.start.second, d.end.first, d.end.second});
  }
  for (const auto& d : all_cuts[EdgePosition::RIGHT]) {
    if (seen_diagonals.contains(
            {d.end.first, d.end.second, d.start.first, d.start.second})) {
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
  for (const auto& d : all_cuts[EdgePosition::TOP]) {
    seen_diagonals.insert(
        {d.start.first, d.start.second, d.end.first, d.end.second});
  }
  for (const auto& d : all_cuts[EdgePosition::BOTTOM]) {
    if (!seen_diagonals.contains(
            {d.end.first, d.end.second, d.start.first, d.start.second})) {
      cuts.push_back(d);
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
    const std::vector<EmptySpace> empty_spaces = FindEmptySpacesHorizontally(
        mandatory_bounding_box, *mandatory_rectangles);
    std::vector<Rectangle> mandatory_empty_holes;
    mandatory_empty_holes.reserve(empty_spaces.size());
    for (const auto& space : empty_spaces) {
      mandatory_empty_holes.push_back(space.rect);
    }
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
