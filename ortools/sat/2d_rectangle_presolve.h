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

#ifndef OR_TOOLS_SAT_2D_RECTANGLE_PRESOLVE_H_
#define OR_TOOLS_SAT_2D_RECTANGLE_PRESOLVE_H_

#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/inlined_vector.h"
#include "absl/types/span.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer.h"

namespace operations_research {
namespace sat {

// Given a set of fixed boxes and a set of boxes that are not yet
// fixed (but attributed a range), look for a more optimal set of fixed
// boxes that are equivalent to the initial set of fixed boxes. This
// uses "equivalent" in the sense that a placement of the non-fixed boxes will
// be non-overlapping with all other boxes if and only if it was with the
// original set of fixed boxes too.
bool PresolveFixed2dRectangles(
    absl::Span<const RectangleInRange> non_fixed_boxes,
    std::vector<Rectangle>* fixed_boxes);

// Given two vectors of non-overlapping rectangles defining two regions of the
// space: one mandatory region that must be occupied and one optional region
// that can be occupied, try to build a vector of as few non-overlapping
// rectangles as possible defining a region R that satisfy:
//   - R \subset (mandatory \union optional);
//   - mandatory \subset R.
//
// The function updates the vector of `mandatory_rectangles` with `R` and
// `optional_rectangles` with  `optional_rectangles \setdiff R`. It returns
// true if the `mandatory_rectangles` was updated.
//
// This function uses a greedy algorithm that merge rectangles that share an
// edge.
bool ReduceNumberofBoxesGreedy(std::vector<Rectangle>* mandatory_rectangles,
                               std::vector<Rectangle>* optional_rectangles);

// Same as above, but this implementation returns the optimal solution in
// minimizing the number of boxes if `optional_rectangles` is empty. On the
// other hand, its handling of optional boxes is rather limited. It simply fills
// the holes in the mandatory boxes with optional boxes, if possible.
bool ReduceNumberOfBoxesExactMandatory(
    std::vector<Rectangle>* mandatory_rectangles,
    std::vector<Rectangle>* optional_rectangles);

enum EdgePosition { TOP = 0, RIGHT = 1, BOTTOM = 2, LEFT = 3 };

template <typename Sink>
void AbslStringify(Sink& sink, EdgePosition e) {
  switch (e) {
    case EdgePosition::TOP:
      sink.Append("TOP");
      break;
    case EdgePosition::RIGHT:
      sink.Append("RIGHT");
      break;
    case EdgePosition::BOTTOM:
      sink.Append("BOTTOM");
      break;
    case EdgePosition::LEFT:
      sink.Append("LEFT");
      break;
  }
}

// Given a set of non-overlapping rectangles, precompute a data-structure that
// allow for each rectangle to find the adjacent rectangle along an edge.
//
// Note that it only consider adjacent rectangles whose segments has a
// intersection of non-zero size. In particular, rectangles as following are not
// considered adjacent:
//
// ********
// ********
// ********
// ********
//         +++++++++
//         +++++++++
//         +++++++++
//         +++++++++
//
// Precondition: All rectangles must be disjoint.
class Neighbours {
 public:
  class CompareClockwise {
   public:
    explicit CompareClockwise(EdgePosition edge) : edge_(edge) {}

    bool operator()(const Rectangle& a, const Rectangle& b) const {
      switch (edge_) {
        case EdgePosition::BOTTOM:
          return std::tie(a.x_min, a.x_max) > std::tie(b.x_min, b.x_max);
        case EdgePosition::TOP:
          return std::tie(a.x_min, a.x_max) < std::tie(b.x_min, b.x_max);
        case EdgePosition::LEFT:
          return std::tie(a.y_min, a.y_max) < std::tie(b.y_min, b.y_max);
        case EdgePosition::RIGHT:
          return std::tie(a.y_min, a.y_max) > std::tie(b.y_min, b.y_max);
      }
    }
    EdgePosition edge_;
  };

  explicit Neighbours(
      absl::Span<const Rectangle> rectangles,
      absl::Span<const std::tuple<int, EdgePosition, int>> neighbors)
      : size_(rectangles.size()) {
    for (const auto& [box_index, edge, neighbor] : neighbors) {
      neighbors_[edge][box_index].push_back(neighbor);
    }
    for (int edge = 0; edge < 4; ++edge) {
      for (auto& [box_index, neighbors] : neighbors_[edge]) {
        absl::c_sort(neighbors, [&rectangles, edge](int a, int b) {
          return CompareClockwise(static_cast<EdgePosition>(edge))(
              rectangles[a], rectangles[b]);
        });
      }
    }
  }

  int NumRectangles() const { return size_; }

  // Neighbors are sorted in the clockwise order.
  absl::Span<const int> GetSortedNeighbors(int rectangle_index,
                                           EdgePosition edge) const {
    if (auto it = neighbors_[edge].find(rectangle_index);
        it != neighbors_[edge].end()) {
      return it->second;
    } else {
      return {};
    }
  }

 private:
  absl::flat_hash_map<int, absl::InlinedVector<int, 3>> neighbors_[4];
  int size_;
};

Neighbours BuildNeighboursGraph(absl::Span<const Rectangle> rectangles);

std::vector<std::vector<int>> SplitInConnectedComponents(
    const Neighbours& neighbours);

// Generally, given a set of non-overlapping rectangles and a path that doesn't
// cross itself, the path can be cut into segments that touch only one single
// rectangle in the interior of the region delimited by the path. This struct
// holds a path cut into such segments. In particular, for the contour of an
// union of rectangles, the path is a subset of the union of all the rectangle's
// edges.
struct ShapePath {
  // The two vectors should have exactly the same size.
  std::vector<std::pair<IntegerValue, IntegerValue>> step_points;
  // touching_box_index[i] contains the index of the unique interior rectangle
  // touching the segment step_points[i]->step_points[(i+1)%size].
  std::vector<int> touching_box_index;
};

struct SingleShape {
  ShapePath boundary;
  std::vector<ShapePath> holes;
};

// Given a set of rectangles, split it into connected components and transform
// each individual set into a shape described by its boundary and holes paths.
std::vector<SingleShape> BoxesToShapes(absl::Span<const Rectangle> rectangles,
                                       const Neighbours& neighbours);

std::vector<Rectangle> CutShapeIntoRectangles(SingleShape shapes);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_2D_RECTANGLE_PRESOLVE_H_
