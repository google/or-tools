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
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/random.h"
#include "absl/strings/str_split.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/logging.h"
#include "ortools/sat/2d_orthogonal_packing_testing.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer.h"

namespace operations_research {
namespace sat {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

std::vector<Rectangle> BuildFromAsciiArt(std::string_view input) {
  std::vector<Rectangle> rectangles;
  std::vector<std::string_view> lines = absl::StrSplit(input, '\n');
  const int num_lines = lines.size();
  for (int i = 0; i < lines.size(); i++) {
    for (int j = 0; j < lines[i].size(); j++) {
      if (lines[i][j] != ' ') {
        rectangles.push_back({.x_min = j,
                              .x_max = j + 1,
                              .y_min = 2 * num_lines - 2 * i,
                              .y_max = 2 * num_lines - 2 * i + 2});
      }
    }
  }
  std::vector<Rectangle> empty;
  ReduceNumberofBoxesGreedy(&rectangles, &empty);
  return rectangles;
}

TEST(RectanglePresolve, Basic) {
  std::vector<Rectangle> input = BuildFromAsciiArt(R"(
        ***********   ***********
        ***********   ***********
        ***********   ***********


        ***********   ***********
        ***********   ***********
        ***********   ***********
  )");
  // Note that a single naive pass over the fixed rectangles' gaps would not
  // fill the middle region.
  std::vector<RectangleInRange> input_in_range;
  // Add a single object that is too large to fit between the fixed boxes.
  input_in_range.push_back(
      {.box_index = 0,
       .bounding_area = {.x_min = 0, .x_max = 80, .y_min = 0, .y_max = 80},
       .x_size = 5,
       .y_size = 5});

  EXPECT_TRUE(PresolveFixed2dRectangles(input_in_range, &input));
  EXPECT_EQ(input.size(), 1);
}

TEST(RectanglePresolve, Trim) {
  std::vector<Rectangle> input = {
      {.x_min = 0, .x_max = 5, .y_min = 0, .y_max = 5}};
  std::vector<RectangleInRange> input_in_range;
  input_in_range.push_back(
      {.box_index = 0,
       .bounding_area = {.x_min = 1, .x_max = 80, .y_min = 1, .y_max = 80},
       .x_size = 5,
       .y_size = 5});

  EXPECT_TRUE(PresolveFixed2dRectangles(input_in_range, &input));
  EXPECT_THAT(input, ElementsAre(Rectangle{
                         .x_min = 1, .x_max = 5, .y_min = 1, .y_max = 5}));
}

TEST(RectanglePresolve, FillBoundingBoxEdge) {
  std::vector<Rectangle> input = {
      {.x_min = 1, .x_max = 5, .y_min = 1, .y_max = 5}};
  std::vector<RectangleInRange> input_in_range;
  input_in_range.push_back(
      {.box_index = 0,
       .bounding_area = {.x_min = 0, .x_max = 80, .y_min = 0, .y_max = 80},
       .x_size = 5,
       .y_size = 5});

  EXPECT_TRUE(PresolveFixed2dRectangles(input_in_range, &input));
  EXPECT_THAT(input, ElementsAre(Rectangle{
                         .x_min = 0, .x_max = 5, .y_min = 0, .y_max = 5}));
}

TEST(RectanglePresolve, UseAreaNotOccupiable) {
  std::vector<Rectangle> input = {
      {.x_min = 20, .x_max = 25, .y_min = 0, .y_max = 5}};
  std::vector<RectangleInRange> input_in_range;
  input_in_range.push_back(
      {.box_index = 0,
       .bounding_area = {.x_min = 0, .x_max = 10, .y_min = 0, .y_max = 10},
       .x_size = 5,
       .y_size = 5});
  input_in_range.push_back(
      {.box_index = 1,
       .bounding_area = {.x_min = 0, .x_max = 15, .y_min = 0, .y_max = 10},
       .x_size = 5,
       .y_size = 5});
  input_in_range.push_back(
      {.box_index = 1,
       .bounding_area = {.x_min = 25, .x_max = 100, .y_min = 0, .y_max = 10},
       .x_size = 5,
       .y_size = 5});

  EXPECT_TRUE(PresolveFixed2dRectangles(input_in_range, &input));
  EXPECT_THAT(input, ElementsAre(Rectangle{
                         .x_min = 15, .x_max = 25, .y_min = 0, .y_max = 10}));
}

TEST(RectanglePresolve, RemoveOutsideBB) {
  std::vector<Rectangle> input = {
      {.x_min = 0, .x_max = 5, .y_min = 0, .y_max = 5}};
  std::vector<RectangleInRange> input_in_range;
  input_in_range.push_back(
      {.box_index = 0,
       .bounding_area = {.x_min = 5, .x_max = 80, .y_min = 5, .y_max = 80},
       .x_size = 5,
       .y_size = 5});

  EXPECT_TRUE(PresolveFixed2dRectangles(input_in_range, &input));
  EXPECT_THAT(input, IsEmpty());
}

TEST(RectanglePresolve, RandomTest) {
  constexpr int kFixedRectangleSize = 10;
  constexpr int kNumRuns = 1000;
  absl::BitGen bit_gen;

  for (int run = 0; run < kNumRuns; ++run) {
    // Start by generating a feasible problem that we know the solution with
    // some items fixed.
    std::vector<Rectangle> input =
        GenerateNonConflictingRectanglesWithPacking({100, 100}, 40, bit_gen);
    std::shuffle(input.begin(), input.end(), bit_gen);
    absl::Span<const Rectangle> fixed_rectangles =
        absl::MakeConstSpan(input).subspan(0, kFixedRectangleSize);
    absl::Span<const Rectangle> other_rectangles =
        absl::MakeSpan(input).subspan(kFixedRectangleSize);
    std::vector<Rectangle> new_fixed_rectangles(fixed_rectangles.begin(),
                                                fixed_rectangles.end());
    const std::vector<RectangleInRange> input_in_range =
        MakeItemsFromRectangles(other_rectangles, 0.6, bit_gen);

    // Presolve the fixed items.
    PresolveFixed2dRectangles(input_in_range, &new_fixed_rectangles);
    if (run == 0) {
      LOG(INFO) << "Presolved:\n"
                << RenderDot(std::nullopt, fixed_rectangles) << "To:\n"
                << RenderDot(std::nullopt, new_fixed_rectangles);
    }

    if (new_fixed_rectangles.size() > fixed_rectangles.size()) {
      LOG(FATAL) << "Presolved:\n"
                 << RenderDot(std::nullopt, fixed_rectangles) << "To:\n"
                 << RenderDot(std::nullopt, new_fixed_rectangles);
    }
    CHECK_LE(new_fixed_rectangles.size(), fixed_rectangles.size());

    // Check if the original solution is still a solution.
    std::vector<Rectangle> all_rectangles(new_fixed_rectangles.begin(),
                                          new_fixed_rectangles.end());
    all_rectangles.insert(all_rectangles.end(), other_rectangles.begin(),
                          other_rectangles.end());
    for (int i = 0; i < all_rectangles.size(); ++i) {
      for (int j = i + 1; j < all_rectangles.size(); ++j) {
        CHECK(all_rectangles[i].IsDisjoint(all_rectangles[j]))
            << RenderDot(std::nullopt, {all_rectangles[i], all_rectangles[j]});
      }
    }
  }
}

Neighbours NaiveBuildNeighboursGraph(absl::Span<const Rectangle> rectangles) {
  auto interval_intersect = [](IntegerValue begin1, IntegerValue end1,
                               IntegerValue begin2, IntegerValue end2) {
    return std::max(begin1, begin2) < std::min(end1, end2);
  };
  std::vector<std::tuple<int, EdgePosition, int>> neighbors;
  for (int i = 0; i < rectangles.size(); ++i) {
    for (int j = 0; j < rectangles.size(); ++j) {
      if (i == j) continue;
      const Rectangle& r1 = rectangles[i];
      const Rectangle& r2 = rectangles[j];
      if (r1.x_min == r2.x_max &&
          interval_intersect(r1.y_min, r1.y_max, r2.y_min, r2.y_max)) {
        neighbors.push_back({i, EdgePosition::LEFT, j});
        neighbors.push_back({j, EdgePosition::RIGHT, i});
      }
      if (r1.y_min == r2.y_max &&
          interval_intersect(r1.x_min, r1.x_max, r2.x_min, r2.x_max)) {
        neighbors.push_back({i, EdgePosition::BOTTOM, j});
        neighbors.push_back({j, EdgePosition::TOP, i});
      }
    }
  }
  return Neighbours(rectangles, neighbors);
}

std::string RenderNeighborsGraph(std::optional<Rectangle> bb,
                                 absl::Span<const Rectangle> rectangles,
                                 const Neighbours& neighbours) {
  const absl::flat_hash_map<EdgePosition, std::string> edge_colors = {
      {EdgePosition::TOP, "red"},
      {EdgePosition::BOTTOM, "green"},
      {EdgePosition::LEFT, "blue"},
      {EdgePosition::RIGHT, "cyan"}};
  std::stringstream ss;
  ss << "  edge[headclip=false, tailclip=false, penwidth=30];\n";
  for (int box_index = 0; box_index < neighbours.NumRectangles(); ++box_index) {
    for (int edge_int = 0; edge_int < 4; ++edge_int) {
      const EdgePosition edge = static_cast<EdgePosition>(edge_int);
      const auto edge_neighbors =
          neighbours.GetSortedNeighbors(box_index, edge);
      for (int neighbor : edge_neighbors) {
        ss << "  " << box_index << "->" << neighbor << " [color=\""
           << edge_colors.find(edge)->second << "\"];\n";
      }
    }
  }
  return RenderDot(bb, rectangles, ss.str());
}

std::string RenderContour(std::optional<Rectangle> bb,
                          absl::Span<const Rectangle> rectangles,
                          const ShapePath& path) {
  const std::vector<std::string> colors = {"red",  "green",  "blue",
                                           "cyan", "yellow", "purple"};
  std::stringstream ss;
  ss << "  edge[headclip=false, tailclip=false, penwidth=30];\n";
  for (int i = 0; i < path.step_points.size(); ++i) {
    std::pair<IntegerValue, IntegerValue> p = path.step_points[i];
    ss << "  p" << i << "[pos=\"" << 2 * p.first << "," << 2 * p.second
       << "!\" shape=point]\n";
    if (i != path.step_points.size() - 1) {
      ss << "  p" << i << "->p" << i + 1 << "\n";
    }
  }
  return RenderDot(bb, rectangles, ss.str());
}

TEST(BuildNeighboursGraphTest, Simple) {
  std::vector<Rectangle> rectangles = {
      {.x_min = 0, .x_max = 10, .y_min = 0, .y_max = 10},
      {.x_min = 10, .x_max = 20, .y_min = 0, .y_max = 10},
      {.x_min = 0, .x_max = 10, .y_min = 10, .y_max = 20}};
  const Neighbours neighbours = BuildNeighboursGraph(rectangles);
  EXPECT_THAT(neighbours.GetSortedNeighbors(0, EdgePosition::RIGHT),
              ElementsAre(1));
  EXPECT_THAT(neighbours.GetSortedNeighbors(0, EdgePosition::TOP),
              ElementsAre(2));
  EXPECT_THAT(neighbours.GetSortedNeighbors(1, EdgePosition::LEFT),
              ElementsAre(0));
  EXPECT_THAT(neighbours.GetSortedNeighbors(2, EdgePosition::BOTTOM),
              ElementsAre(0));
}

TEST(BuildNeighboursGraphTest, NeighborsAroundCorner) {
  std::vector<Rectangle> rectangles = {
      {.x_min = 0, .x_max = 10, .y_min = 0, .y_max = 10},
      {.x_min = 10, .x_max = 20, .y_min = 10, .y_max = 20}};
  const Neighbours neighbours = BuildNeighboursGraph(rectangles);
  for (int i = 0; i < 4; ++i) {
    const EdgePosition edge = static_cast<EdgePosition>(i);
    EXPECT_THAT(neighbours.GetSortedNeighbors(0, edge), IsEmpty());
    EXPECT_THAT(neighbours.GetSortedNeighbors(1, edge), IsEmpty());
  }
}

TEST(BuildNeighboursGraphTest, RandomTest) {
  constexpr int kNumRuns = 100;
  absl::BitGen bit_gen;

  for (int run = 0; run < kNumRuns; ++run) {
    // Start by generating a feasible problem that we know the solution with
    // some items fixed.
    std::vector<Rectangle> input =
        GenerateNonConflictingRectanglesWithPacking({100, 100}, 60, bit_gen);
    std::shuffle(input.begin(), input.end(), bit_gen);
    auto neighbours = BuildNeighboursGraph(input);
    auto expected_neighbours = NaiveBuildNeighboursGraph(input);
    for (int box_index = 0; box_index < neighbours.NumRectangles();
         ++box_index) {
      for (int edge_int = 0; edge_int < 4; ++edge_int) {
        const EdgePosition edge = static_cast<EdgePosition>(edge_int);
        if (neighbours.GetSortedNeighbors(box_index, edge) !=
            expected_neighbours.GetSortedNeighbors(box_index, edge)) {
          LOG(FATAL) << "Got:\n"
                     << RenderNeighborsGraph(std::nullopt, input, neighbours)
                     << "Expected:\n"
                     << RenderNeighborsGraph(std::nullopt, input,
                                             expected_neighbours);
        }
      }
    }
  }
}

struct ContourPoint {
  IntegerValue x;
  IntegerValue y;
  int next_box_index;
  EdgePosition next_direction;

  bool operator!=(const ContourPoint& other) const {
    return x != other.x || y != other.y ||
           next_box_index != other.next_box_index ||
           next_direction != other.next_direction;
  }
};

// This function runs in O(log N).
ContourPoint NextByClockwiseOrder(const ContourPoint& point,
                                  absl::Span<const Rectangle> rectangles,
                                  const Neighbours& neighbours) {
  // This algorithm is very verbose, but it is about handling four cases. In the
  // schema below,  "-->" is the current direction, "X" the next point and
  // the dashed arrow the next direction.
  //
  // Case 1:
  //              ++++++++
  //            ^ ++++++++
  //            : ++++++++
  //            : ++++++++
  //              ++++++++
  //     --->   X ++++++++
  // ******************
  // ******************
  // ******************
  // ******************
  //
  // Case 2:
  //            ^ ++++++++
  //            : ++++++++
  //            : ++++++++
  //              ++++++++
  //    --->    X ++++++++
  // *************++++++++
  // *************++++++++
  // *************
  // *************
  //
  // Case 3:
  //    --->    X   ...>
  // *************++++++++
  // *************++++++++
  // *************++++++++
  // *************++++++++
  //
  // Case 4:
  //     --->      X
  // ************* :
  // ************* :
  // ************* :
  // ************* \/
  ContourPoint result;
  const Rectangle& cur_rectangle = rectangles[point.next_box_index];

  EdgePosition cur_edge;
  bool clockwise;
  // Much of the code below need to know two things: in which direction we are
  // going and what edge of which rectangle we are touching. For example, in the
  // "Case 4" drawing above we are going RIGHT and touching the TOP edge of the
  // current rectangle. This switch statement finds this `cur_edge`.
  switch (point.next_direction) {
    case EdgePosition::TOP:
      if (cur_rectangle.x_max == point.x) {
        cur_edge = EdgePosition::RIGHT;
        clockwise = false;
      } else {
        cur_edge = EdgePosition::LEFT;
        clockwise = true;
      }
      break;
    case EdgePosition::BOTTOM:
      if (cur_rectangle.x_min == point.x) {
        cur_edge = EdgePosition::LEFT;
        clockwise = false;
      } else {
        cur_edge = EdgePosition::RIGHT;
        clockwise = true;
      }
      break;
    case EdgePosition::LEFT:
      if (cur_rectangle.y_max == point.y) {
        cur_edge = EdgePosition::TOP;
        clockwise = false;
      } else {
        cur_edge = EdgePosition::BOTTOM;
        clockwise = true;
      }
      break;
    case EdgePosition::RIGHT:
      if (cur_rectangle.y_min == point.y) {
        cur_edge = EdgePosition::BOTTOM;
        clockwise = false;
      } else {
        cur_edge = EdgePosition::TOP;
        clockwise = true;
      }
      break;
  }

  // Test case 1. We need to find the next box after the current point in the
  // edge we are following in the current direction.
  const auto cur_edge_neighbors =
      neighbours.GetSortedNeighbors(point.next_box_index, cur_edge);

  const Rectangle fake_box_for_lower_bound = {
      .x_min = point.x, .x_max = point.x, .y_min = point.y, .y_max = point.y};
  const auto clockwise_cmp = Neighbours::CompareClockwise(cur_edge);
  auto it = absl::c_lower_bound(
      cur_edge_neighbors, -1,
      [&fake_box_for_lower_bound, rectangles, clockwise_cmp, clockwise](int a,
                                                                        int b) {
        const Rectangle& rectangle_a =
            (a == -1 ? fake_box_for_lower_bound : rectangles[a]);
        const Rectangle& rectangle_b =
            (b == -1 ? fake_box_for_lower_bound : rectangles[b]);
        if (clockwise) {
          return clockwise_cmp(rectangle_a, rectangle_b);
        } else {
          return clockwise_cmp(rectangle_b, rectangle_a);
        }
      });

  if (it != cur_edge_neighbors.end()) {
    // We found box in the current edge. We are in case 1.
    result.next_box_index = *it;
    const Rectangle& next_rectangle = rectangles[*it];
    switch (point.next_direction) {
      case EdgePosition::TOP:
        result.x = point.x;
        result.y = next_rectangle.y_min;
        if (cur_edge == EdgePosition::LEFT) {
          result.next_direction = EdgePosition::LEFT;
        } else {
          result.next_direction = EdgePosition::RIGHT;
        }
        break;
      case EdgePosition::BOTTOM:
        result.x = point.x;
        result.y = next_rectangle.y_max;
        if (cur_edge == EdgePosition::LEFT) {
          result.next_direction = EdgePosition::LEFT;
        } else {
          result.next_direction = EdgePosition::RIGHT;
        }
        break;
      case EdgePosition::LEFT:
        result.y = point.y;
        result.x = next_rectangle.x_max;
        if (cur_edge == EdgePosition::TOP) {
          result.next_direction = EdgePosition::TOP;
        } else {
          result.next_direction = EdgePosition::BOTTOM;
        }
        break;
      case EdgePosition::RIGHT:
        result.y = point.y;
        result.x = next_rectangle.x_min;
        if (cur_edge == EdgePosition::TOP) {
          result.next_direction = EdgePosition::TOP;
        } else {
          result.next_direction = EdgePosition::BOTTOM;
        }
        break;
    }
    return result;
  }

  // We now know we are not in Case 1, so know the next (x, y) position: it is
  // the corner of the current rectangle in the direction we are going.
  switch (point.next_direction) {
    case EdgePosition::TOP:
      result.x = point.x;
      result.y = cur_rectangle.y_max;
      break;
    case EdgePosition::BOTTOM:
      result.x = point.x;
      result.y = cur_rectangle.y_min;
      break;
    case EdgePosition::LEFT:
      result.x = cur_rectangle.x_min;
      result.y = point.y;
      break;
    case EdgePosition::RIGHT:
      result.x = cur_rectangle.x_max;
      result.y = point.y;
      break;
  }

  // Case 2 and 3.
  const auto next_edge_neighbors =
      neighbours.GetSortedNeighbors(point.next_box_index, point.next_direction);
  if (!next_edge_neighbors.empty()) {
    // We are looking for the neighbor on the edge of the current box.
    const int candidate_index =
        clockwise ? next_edge_neighbors.front() : next_edge_neighbors.back();
    const Rectangle& next_rectangle = rectangles[candidate_index];
    switch (point.next_direction) {
      case EdgePosition::TOP:
      case EdgePosition::BOTTOM:
        if (next_rectangle.x_min < point.x && point.x < next_rectangle.x_max) {
          // Case 2
          result.next_box_index = candidate_index;
          if (cur_edge == EdgePosition::LEFT) {
            result.next_direction = EdgePosition::LEFT;
          } else {
            result.next_direction = EdgePosition::RIGHT;
          }
          return result;
        } else if (next_rectangle.x_min == point.x &&
                   cur_edge == EdgePosition::LEFT) {
          // Case 3
          result.next_box_index = candidate_index;
          result.next_direction = point.next_direction;
          return result;
        } else if (next_rectangle.x_max == point.x &&
                   cur_edge == EdgePosition::RIGHT) {
          // Case 3
          result.next_box_index = candidate_index;
          result.next_direction = point.next_direction;
          return result;
        }
        break;
      case EdgePosition::LEFT:
      case EdgePosition::RIGHT:
        if (next_rectangle.y_min < point.y && point.y < next_rectangle.y_max) {
          result.next_box_index = candidate_index;
          if (cur_edge == EdgePosition::TOP) {
            result.next_direction = EdgePosition::TOP;
          } else {
            result.next_direction = EdgePosition::BOTTOM;
          }
          return result;
        } else if (next_rectangle.y_max == point.y &&
                   cur_edge == EdgePosition::TOP) {
          result.next_box_index = candidate_index;
          result.next_direction = point.next_direction;
          return result;
        } else if (next_rectangle.y_min == point.y &&
                   cur_edge == EdgePosition::BOTTOM) {
          result.next_box_index = candidate_index;
          result.next_direction = point.next_direction;
          return result;
        }
        break;
    }
  }

  // Now we must be in the case 4.
  result.next_box_index = point.next_box_index;
  switch (point.next_direction) {
    case EdgePosition::TOP:
    case EdgePosition::BOTTOM:
      if (cur_edge == EdgePosition::LEFT) {
        result.next_direction = EdgePosition::RIGHT;
      } else {
        result.next_direction = EdgePosition::LEFT;
      }
      break;
    case EdgePosition::LEFT:
    case EdgePosition::RIGHT:
      if (cur_edge == EdgePosition::TOP) {
        result.next_direction = EdgePosition::BOTTOM;
      } else {
        result.next_direction = EdgePosition::TOP;
      }
      break;
  }
  return result;
}

// Returns a path delimiting a boundary of the union of a set of rectangles. It
// should work for both the exterior boundary and the boundaries of the holes
// inside the union. The path will start on `starting_point` and follow the
// boundary on clockwise order.
//
// `starting_point` should be a point in the boundary and `starting_box_index`
// the index of a rectangle with one edge containing `starting_point`.
//
// The resulting `path` satisfy:
// - path.step_points.front() == path.step_points.back() == starting_point
// - path.touching_box_index.front() == path.touching_box_index.back() ==
//                                   == starting_box_index
//
ShapePath TraceBoundary(
    const std::pair<IntegerValue, IntegerValue>& starting_step_point,
    int starting_box_index, absl::Span<const Rectangle> rectangles,
    const Neighbours& neighbours) {
  // First find which direction we need to go to follow the border in the
  // clockwise order.
  const Rectangle& initial_rec = rectangles[starting_box_index];
  bool touching_edge[4];
  touching_edge[EdgePosition::LEFT] =
      initial_rec.x_min == starting_step_point.first;
  touching_edge[EdgePosition::RIGHT] =
      initial_rec.x_max == starting_step_point.first;
  touching_edge[EdgePosition::TOP] =
      initial_rec.y_max == starting_step_point.second;
  touching_edge[EdgePosition::BOTTOM] =
      initial_rec.y_min == starting_step_point.second;

  EdgePosition next_direction;
  if (touching_edge[EdgePosition::LEFT]) {
    if (touching_edge[EdgePosition::TOP]) {
      next_direction = EdgePosition::RIGHT;
    } else {
      next_direction = EdgePosition::TOP;
    }
  } else if (touching_edge[EdgePosition::RIGHT]) {
    if (touching_edge[EdgePosition::BOTTOM]) {
      next_direction = EdgePosition::LEFT;
    } else {
      next_direction = EdgePosition::BOTTOM;
    }
  } else if (touching_edge[EdgePosition::TOP]) {
    next_direction = EdgePosition::LEFT;
  } else if (touching_edge[EdgePosition::BOTTOM]) {
    next_direction = EdgePosition::RIGHT;
  } else {
    LOG(FATAL)
        << "TraceBoundary() got a `starting_step_point` that is not in an edge "
           "of the rectangle of `starting_box_index`. This is not allowed.";
  }
  const ContourPoint starting_point = {.x = starting_step_point.first,
                                       .y = starting_step_point.second,
                                       .next_box_index = starting_box_index,
                                       .next_direction = next_direction};
  ShapePath result;
  for (ContourPoint point = starting_point; true;
       point = NextByClockwiseOrder(point, rectangles, neighbours)) {
    if (result.step_points.size() > 3 &&
        result.step_points.back() == result.step_points.front() &&
        point.x == result.step_points[1].first &&
        point.y == result.step_points[1].second) {
      break;
    }
    if (!result.step_points.empty() &&
        point.x == result.step_points.back().first &&
        point.y == result.step_points.back().second) {
      // There is a special corner-case of the algorithm using the neighbours.
      // Consider the following set-up:
      //
      // ******** |
      // ******** |
      // ******** +---->
      // ########++++++++
      // ########++++++++
      // ########++++++++
      //
      // In this case, the only way the algorithm could reach the "+" box is via
      // the "#" box, but which is doesn't contribute to the path. The algorithm
      // returns a technically correct zero-size interval, which might be useful
      // for callers that want to count the "#" box as visited, but this is not
      // our case.
      result.touching_box_index.back() = point.next_box_index;
    } else {
      result.touching_box_index.push_back(point.next_box_index);
      result.step_points.push_back({point.x, point.y});
    }
  }
  return result;
}

std::string RenderShapes(std::optional<Rectangle> bb,
                         absl::Span<const Rectangle> rectangles,
                         const std::vector<SingleShape>& shapes) {
  const std::vector<std::string> colors = {"black", "white",  "orange",
                                           "cyan",  "yellow", "purple"};
  std::stringstream ss;
  ss << "  edge[headclip=false, tailclip=false, penwidth=40];\n";
  int count = 0;
  for (int i = 0; i < shapes.size(); ++i) {
    std::string_view shape_color = colors[i % colors.size()];
    for (int j = 0; j < shapes[i].boundary.step_points.size(); ++j) {
      std::pair<IntegerValue, IntegerValue> p =
          shapes[i].boundary.step_points[j];
      ss << "  p" << count << "[pos=\"" << 2 * p.first << "," << 2 * p.second
         << "!\" shape=point]\n";
      if (j != shapes[i].boundary.step_points.size() - 1) {
        ss << "  p" << count << "->p" << count + 1 << " [color=\""
           << shape_color << "\"];\n";
      }
      ++count;
    }
    for (const ShapePath& hole : shapes[i].holes) {
      for (int j = 0; j < hole.step_points.size(); ++j) {
        std::pair<IntegerValue, IntegerValue> p = hole.step_points[j];
        ss << "  p" << count << "[pos=\"" << 2 * p.first << "," << 2 * p.second
           << "!\" shape=point]\n";
        if (j != hole.step_points.size() - 1) {
          ss << "  p" << count << "->p" << count + 1 << " [color=\""
             << shape_color << "\", penwidth=20];\n";
        }
        ++count;
      }
    }
  }
  return RenderDot(bb, rectangles, ss.str());
}

TEST(ContourTest, Random) {
  constexpr int kNumRuns = 100;
  absl::BitGen bit_gen;

  for (int run = 0; run < kNumRuns; ++run) {
    // Start by generating a feasible problem that we know the solution with
    // some items fixed.
    std::vector<Rectangle> input =
        GenerateNonConflictingRectanglesWithPacking({100, 100}, 60, bit_gen);
    std::shuffle(input.begin(), input.end(), bit_gen);
    const int num_fixed_rectangles = input.size() * 2 / 3;
    const absl::Span<const Rectangle> fixed_rectangles =
        absl::MakeConstSpan(input).subspan(0, num_fixed_rectangles);
    const absl::Span<const Rectangle> other_rectangles =
        absl::MakeSpan(input).subspan(num_fixed_rectangles);
    const std::vector<RectangleInRange> input_in_range =
        MakeItemsFromRectangles(other_rectangles, 0.6, bit_gen);

    const Neighbours neighbours = BuildNeighboursGraph(fixed_rectangles);
    const auto components = SplitInConnectedComponents(neighbours);
    const Rectangle bb = {.x_min = 0, .x_max = 100, .y_min = 0, .y_max = 100};
    int min_index = -1;
    std::pair<IntegerValue, IntegerValue> min_coord = {
        std::numeric_limits<IntegerValue>::max(),
        std::numeric_limits<IntegerValue>::max()};
    for (const int box_index : components[0]) {
      const Rectangle& rectangle = fixed_rectangles[box_index];
      if (std::make_pair(rectangle.x_min, rectangle.y_min) < min_coord) {
        min_coord = {rectangle.x_min, rectangle.y_min};
        min_index = box_index;
      }
    }

    const std::vector<SingleShape> shapes =
        BoxesToShapes(fixed_rectangles, neighbours);
    for (const SingleShape& shape : shapes) {
      const ShapePath& boundary = shape.boundary;
      const ShapePath expected_shape =
          TraceBoundary(boundary.step_points[0], boundary.touching_box_index[0],
                        fixed_rectangles, neighbours);
      if (boundary.step_points != expected_shape.step_points) {
        LOG(ERROR) << "Fast algo:\n"
                   << RenderContour(bb, fixed_rectangles, boundary);
        LOG(ERROR) << "Naive algo:\n"
                   << RenderContour(bb, fixed_rectangles, expected_shape);
        LOG(FATAL) << "Found different solutions between naive and fast algo!";
      }
      EXPECT_EQ(boundary.step_points, expected_shape.step_points);
      EXPECT_EQ(boundary.touching_box_index, expected_shape.touching_box_index);
    }

    if (run == 0) {
      LOG(INFO) << RenderShapes(bb, fixed_rectangles, shapes);
    }
  }
}

TEST(ContourTest, SimpleShapes) {
  std::vector<Rectangle> rectangles = {
      {.x_min = 0, .x_max = 10, .y_min = 10, .y_max = 20},
      {.x_min = 3, .x_max = 8, .y_min = 0, .y_max = 10}};
  ShapePath shape =
      TraceBoundary({0, 20}, 0, rectangles, BuildNeighboursGraph(rectangles));
  EXPECT_THAT(shape.touching_box_index, ElementsAre(0, 0, 0, 1, 1, 1, 0, 0, 0));
  EXPECT_THAT(shape.step_points,
              ElementsAre(std::make_pair(0, 20), std::make_pair(10, 20),
                          std::make_pair(10, 10), std::make_pair(8, 10),
                          std::make_pair(8, 0), std::make_pair(3, 0),
                          std::make_pair(3, 10), std::make_pair(0, 10),
                          std::make_pair(0, 20)));

  rectangles = {{.x_min = 0, .x_max = 10, .y_min = 10, .y_max = 20},
                {.x_min = 0, .x_max = 10, .y_min = 0, .y_max = 10}};
  shape =
      TraceBoundary({0, 20}, 0, rectangles, BuildNeighboursGraph(rectangles));
  EXPECT_THAT(shape.touching_box_index, ElementsAre(0, 0, 1, 1, 1, 0, 0));
  EXPECT_THAT(shape.step_points,
              ElementsAre(std::make_pair(0, 20), std::make_pair(10, 20),
                          std::make_pair(10, 10), std::make_pair(10, 0),
                          std::make_pair(0, 0), std::make_pair(0, 10),
                          std::make_pair(0, 20)));

  rectangles = {{.x_min = 0, .x_max = 10, .y_min = 10, .y_max = 20},
                {.x_min = 0, .x_max = 15, .y_min = 0, .y_max = 10}};
  shape =
      TraceBoundary({0, 20}, 0, rectangles, BuildNeighboursGraph(rectangles));
  EXPECT_THAT(shape.touching_box_index, ElementsAre(0, 0, 1, 1, 1, 1, 0, 0));
  EXPECT_THAT(shape.step_points,
              ElementsAre(std::make_pair(0, 20), std::make_pair(10, 20),
                          std::make_pair(10, 10), std::make_pair(15, 10),
                          std::make_pair(15, 0), std::make_pair(0, 0),
                          std::make_pair(0, 10), std::make_pair(0, 20)));

  rectangles = {{.x_min = 0, .x_max = 10, .y_min = 10, .y_max = 20},
                {.x_min = 0, .x_max = 10, .y_min = 0, .y_max = 10},
                {.x_min = 10, .x_max = 20, .y_min = 0, .y_max = 10}};
  shape =
      TraceBoundary({0, 20}, 0, rectangles, BuildNeighboursGraph(rectangles));
  EXPECT_THAT(shape.touching_box_index, ElementsAre(0, 0, 2, 2, 2, 1, 1, 0, 0));
  EXPECT_THAT(shape.step_points,
              ElementsAre(std::make_pair(0, 20), std::make_pair(10, 20),
                          std::make_pair(10, 10), std::make_pair(20, 10),
                          std::make_pair(20, 0), std::make_pair(10, 0),
                          std::make_pair(0, 0), std::make_pair(0, 10),
                          std::make_pair(0, 20)));
}

TEST(ContourTest, ExampleFromPaper) {
  const std::vector<Rectangle> input = BuildFromAsciiArt(R"(
                        *******************              
                        *******************              
    **********          *******************              
    **********          *******************              
    ***************************************              
    ***************************************              
    ***************************************              
    ***************************************              
    ***********     **************     ****              
    ***********     **************     ****              
    ***********     *******    ***     ****              
    ***********     *******    ***     ****              
    ***********     **************     ****              
    ***********     **************     ****              
    ***********     **************     ****              
    ***************************************              
    ***************************************              
    ***************************************              
        **************************************           
        **************************************           
        **************************************           
        *******************************                  
        ***************************************          
        ***************************************          
        ****************    ****************             
        ****************    ****************             
        ******              ***                          
        ******              ***                          
        ******              ***                          
        ******                                           
    )");
  const Neighbours neighbours = BuildNeighboursGraph(input);
  auto s = BoxesToShapes(input, neighbours);
  LOG(INFO) << RenderDot(std::nullopt, input);
  const std::vector<Rectangle> output = CutShapeIntoRectangles(s[0]);
  LOG(INFO) << RenderDot(std::nullopt, output);
  EXPECT_THAT(output.size(), 16);
}

bool RectanglesCoverSameArea(absl::Span<const Rectangle> a,
                             absl::Span<const Rectangle> b) {
  return RegionIncludesOther(a, b) && RegionIncludesOther(b, a);
}

TEST(ReduceNumberOfBoxes, RandomTestNoOptional) {
  constexpr int kNumRuns = 1000;
  absl::BitGen bit_gen;

  for (int run = 0; run < kNumRuns; ++run) {
    // Start by generating a feasible problem that we know the solution with
    // some items fixed.
    std::vector<Rectangle> input =
        GenerateNonConflictingRectanglesWithPacking({100, 100}, 60, bit_gen);
    std::shuffle(input.begin(), input.end(), bit_gen);

    std::vector<Rectangle> output = input;
    std::vector<Rectangle> optional_rectangles_empty;
    ReduceNumberOfBoxesExactMandatory(&output, &optional_rectangles_empty);
    if (run == 0) {
      LOG(INFO) << "Presolved:\n" << RenderDot(std::nullopt, input);
      LOG(INFO) << "To:\n" << RenderDot(std::nullopt, output);
    }

    if (output.size() > input.size()) {
      LOG(INFO) << "Presolved:\n" << RenderDot(std::nullopt, input);
      LOG(INFO) << "To:\n" << RenderDot(std::nullopt, output);
      ADD_FAILURE() << "ReduceNumberofBoxes() increased the number of boxes, "
                       "but it should be optimal in reducing them!";
    }
    CHECK(RectanglesCoverSameArea(output, input));
  }
}

TEST(ReduceNumberOfBoxes, Problematic) {
  // This example shows that we must consider diagonals that touches only on its
  // extremities as "intersecting" for the bipartite graph.
  const std::vector<Rectangle> input = {
      {.x_min = 26, .x_max = 51, .y_min = 54, .y_max = 81},
      {.x_min = 51, .x_max = 78, .y_min = 44, .y_max = 67},
      {.x_min = 51, .x_max = 62, .y_min = 67, .y_max = 92},
      {.x_min = 78, .x_max = 98, .y_min = 24, .y_max = 54},
  };
  std::vector<Rectangle> output = input;
  std::vector<Rectangle> optional_rectangles_empty;
  ReduceNumberOfBoxesExactMandatory(&output, &optional_rectangles_empty);
  LOG(INFO) << "Presolved:\n" << RenderDot(std::nullopt, input);
  LOG(INFO) << "To:\n" << RenderDot(std::nullopt, output);
}

// This example shows that sometimes the best solution with respect to minimum
// number of boxes requires *not* filling a hole. Actually this follows from the
// formula that the minimum number of rectangles in a partition of a polygon
// with n vertices and h holes is n/2 + h − g − 1, where g is the number of
// non-intersecting good diagonals. This test-case shows a polygon with 4
// internal vertices, 1 hole and 4 non-intersecting good diagonals that includes
// the hole. Removing the hole reduces the n/2 term by 2, decrease the h term by
// 1, but decrease the g term by 4.
//
//          ***********************
//          ***********************
//          ***********************.....................
//          ***********************.....................
//          ***********************.....................
//          ***********************.....................
//          ***********************.....................
// ++++++++++++++++++++++          .....................
// ++++++++++++++++++++++          .....................
// ++++++++++++++++++++++          .....................
// ++++++++++++++++++++++000000000000000000000000
// ++++++++++++++++++++++000000000000000000000000
// ++++++++++++++++++++++000000000000000000000000
//                       000000000000000000000000
//                       000000000000000000000000
//                       000000000000000000000000
//                       000000000000000000000000
//
TEST(ReduceNumberOfBoxes, Problematic2) {
  const std::vector<Rectangle> input = {
      {.x_min = 64, .x_max = 82, .y_min = 76, .y_max = 98},
      {.x_min = 39, .x_max = 59, .y_min = 63, .y_max = 82},
      {.x_min = 59, .x_max = 78, .y_min = 61, .y_max = 76},
      {.x_min = 44, .x_max = 64, .y_min = 82, .y_max = 100},
  };
  std::vector<Rectangle> output = input;
  std::vector<Rectangle> optional_rectangles = {
      {.x_min = 59, .x_max = 64, .y_min = 76, .y_max = 82},
  };
  ReduceNumberOfBoxesExactMandatory(&output, &optional_rectangles);
  LOG(INFO) << "Presolving:\n" << RenderDot(std::nullopt, input);

  // Presolve will refuse to do anything since removing the hole will increase
  // the number of boxes.
  CHECK(input == output);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
