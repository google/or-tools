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
  for (int i = 0; i < lines.size(); i++) {
    for (int j = 0; j < lines[i].size(); j++) {
      if (lines[i][j] != ' ') {
        rectangles.push_back(
            {.x_min = j, .x_max = j + 1, .y_min = i, .y_max = i + 1});
      }
    }
  }
  std::vector<Rectangle> empty;
  ReduceNumberofBoxes(&rectangles, &empty);
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
  constexpr int kTotalRectangles = 100;
  constexpr int kFixedRectangleSize = 60;
  constexpr int kNumRuns = 1000;
  absl::BitGen bit_gen;

  for (int run = 0; run < kNumRuns; ++run) {
    // Start by generating a feasible problem that we know the solution with
    // some items fixed.
    std::vector<Rectangle> input =
        GenerateNonConflictingRectangles(kTotalRectangles, bit_gen);
    std::shuffle(input.begin(), input.end(), bit_gen);
    CHECK_EQ(input.size(), kTotalRectangles);
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
    LOG(INFO) << "Presolved:\n"
              << RenderDot(std::nullopt, fixed_rectangles) << "To:\n"
              << RenderDot(std::nullopt, new_fixed_rectangles);

    CHECK_LE(new_fixed_rectangles.size(), kFixedRectangleSize);

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

Neighbours NaiveBuildNeighboursGraph(const std::vector<Rectangle>& rectangles) {
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

ShapePath TraceBoundaryNaive(
    std::pair<IntegerValue, IntegerValue> starting_corner,
    absl::Span<const Rectangle> rectangles) {
  // First build a grid that tells by which box each 1x1 rectangle is occupied
  // or -1 if empty.
  constexpr int kBoundingBoxSize = 100;
  std::vector<std::vector<int>> grid(
      kBoundingBoxSize + 1, std::vector<int>(kBoundingBoxSize + 1, -1));

  for (int n = 0; n < rectangles.size(); n++) {
    const Rectangle& r = rectangles[n];
    CHECK_GE(r.x_min, 0);
    CHECK_LE(r.x_max, kBoundingBoxSize);
    CHECK_GE(r.y_min, 0);
    CHECK_LE(r.y_max, kBoundingBoxSize);
    for (IntegerValue i = r.x_min; i < r.x_max; i++) {
      for (IntegerValue j = r.y_min; j < r.y_max; j++) {
        grid[i.value()][j.value()] = n;
      }
    }
  }

  // Now collect all the boundary edges: an occupied cell that touches an
  // unoccupied one.
  absl::flat_hash_map<std::pair<IntegerValue, IntegerValue>, int> x_edges;
  absl::flat_hash_map<std::pair<IntegerValue, IntegerValue>, int> y_edges;
  for (int i = -1; i < kBoundingBoxSize; i++) {
    for (int j = -1; j < kBoundingBoxSize; j++) {
      if (i != -1) {
        if ((j == -1 || grid[i][j] == -1) && grid[i][j + 1] != -1) {
          x_edges[{i, j + 1}] = grid[i][j + 1];
        }
        if (j != -1 && grid[i][j + 1] == -1 && grid[i][j] != -1) {
          x_edges[{i, j + 1}] = grid[i][j];
        }
      }
      if (j != -1) {
        if ((i == -1 || grid[i][j] == -1) && grid[i + 1][j] != -1) {
          y_edges[{i + 1, j}] = grid[i + 1][j];
        }
        if (i != -1 && grid[i + 1][j] == -1 && grid[i][j] != -1) {
          y_edges[{i + 1, j}] = grid[i][j];
        }
      }
    }
  }

  ShapePath path;
  std::pair<IntegerValue, IntegerValue> cur = starting_corner;
  int cur_index;
  if (x_edges.contains(starting_corner)) {
    cur_index = x_edges.at(starting_corner);
  } else if (y_edges.contains(starting_corner)) {
    cur_index = y_edges.at(starting_corner);
  } else {
    LOG(FATAL) << "Should not happen: {" << starting_corner.first << ","
               << starting_corner.second << "} "
               << RenderDot(std::nullopt, rectangles);
  }
  const int first_index = cur_index;

  auto is_aligned = [](const std::pair<IntegerValue, IntegerValue>& p1,
                       const std::pair<IntegerValue, IntegerValue>& p2,
                       const std::pair<IntegerValue, IntegerValue>& p3) {
    return ((p1.first == p2.first) == (p2.first == p3.first)) &&
           ((p1.second == p2.second) == (p2.second == p3.second));
  };

  // Grow the path by a segment of size one.
  const auto add_segment =
      [&path, &is_aligned](const std::pair<IntegerValue, IntegerValue>& segment,
                           int index) {
        if (path.step_points.size() > 1 &&
            is_aligned(path.step_points[path.step_points.size() - 1],
                       path.step_points[path.step_points.size() - 2],
                       segment) &&
            path.touching_box_index.back() == index) {
          path.step_points.back() = segment;
        } else {
          if (!path.step_points.empty()) {
            path.touching_box_index.push_back(index);
          }
          path.step_points.push_back(segment);
        }
      };

  // Now we navigate from one edge to the next. To avoid going back, we remove
  // used edges from the hash map.
  do {
    add_segment(cur, cur_index);

    // Find the next segment.
    if (x_edges.contains({cur.first, cur.second}) &&
        x_edges.contains({cur.first - 1, cur.second}) &&
        !path.touching_box_index.empty()) {
      // Corner case (literally):
      // ********
      // ********
      // ********
      // ********
      //         +++++++++
      //         +++++++++
      //         +++++++++
      //         +++++++++
      //
      // In this case we keep following the same box.
      auto it_x = x_edges.find({cur.first, cur.second});
      if (cur_index == it_x->second) {
        auto extract = x_edges.extract({cur.first, cur.second});
        cur = {cur.first + 1, cur.second};
        cur_index = extract.mapped();
      } else {
        auto extract = x_edges.extract({cur.first - 1, cur.second});
        cur = extract.key();
        cur_index = extract.mapped();
      }
    } else if (y_edges.contains({cur.first, cur.second}) &&
               y_edges.contains({cur.first, cur.second - 1}) &&
               !path.touching_box_index.empty()) {
      auto it_y = y_edges.find({cur.first, cur.second});
      if (cur_index == it_y->second) {
        auto extract = y_edges.extract({cur.first, cur.second});
        cur = {cur.first, cur.second + 1};
        cur_index = extract.mapped();
      } else {
        auto extract = y_edges.extract({cur.first, cur.second - 1});
        cur = extract.key();
        cur_index = extract.mapped();
      }
    } else if (auto extract = y_edges.extract({cur.first, cur.second});
               !extract.empty()) {
      cur = {cur.first, cur.second + 1};
      cur_index = extract.mapped();
    } else if (auto extract = x_edges.extract({cur.first - 1, cur.second});
               !extract.empty()) {
      cur = extract.key();
      cur_index = extract.mapped();
    } else if (auto extract = x_edges.extract({cur.first, cur.second});
               !extract.empty()) {
      cur = {cur.first + 1, cur.second};
      cur_index = extract.mapped();
    } else if (auto extract = y_edges.extract({cur.first, cur.second - 1});
               !extract.empty()) {
      cur = extract.key();
      cur_index = extract.mapped();
    } else {
      LOG(FATAL) << "Should not happen: {" << cur.first << "," << cur.second
                 << "} " << RenderContour(std::nullopt, rectangles, path);
    }
  } while (cur != starting_corner);

  add_segment(cur, cur_index);
  path.touching_box_index.push_back(first_index);
  return path;
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
    absl::Span<const Rectangle> fixed_rectangles =
        absl::MakeConstSpan(input).subspan(0, num_fixed_rectangles);
    absl::Span<const Rectangle> other_rectangles =
        absl::MakeSpan(input).subspan(num_fixed_rectangles);
    std::vector<Rectangle> new_fixed_rectangles(fixed_rectangles.begin(),
                                                fixed_rectangles.end());
    const std::vector<RectangleInRange> input_in_range =
        MakeItemsFromRectangles(other_rectangles, 0.6, bit_gen);

    auto neighbours = BuildNeighboursGraph(fixed_rectangles);
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

    const ShapePath shape =
        TraceBoundary(min_coord, min_index, fixed_rectangles, neighbours);
    absl::flat_hash_set<int> seen;
    std::vector<Rectangle> component;
    std::vector<int> index_map(input.size());
    for (const int box_index : components[0]) {
      component.push_back(fixed_rectangles[box_index]);
      index_map[box_index] = component.size() - 1;
    }

    const ShapePath expected_shape =
        TraceBoundaryNaive(shape.step_points[0], component);
    if (shape.step_points != expected_shape.step_points) {
      LOG(ERROR) << "Fast algo:\n"
                 << RenderContour(bb, fixed_rectangles, shape);
      LOG(ERROR) << "Naive algo:\n"
                 << RenderContour(bb, component, expected_shape);
      LOG(FATAL) << "Found different solutions between naive and fast algo!";
    }
    EXPECT_EQ(shape.step_points, expected_shape.step_points);
    for (int i = 0; i < shape.step_points.size(); ++i) {
      EXPECT_EQ(index_map[shape.touching_box_index[i]],
                expected_shape.touching_box_index[i]);
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

}  // namespace
}  // namespace sat
}  // namespace operations_research
