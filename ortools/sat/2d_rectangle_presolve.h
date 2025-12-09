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

#ifndef ORTOOLS_SAT_2D_RECTANGLE_PRESOLVE_H_
#define ORTOOLS_SAT_2D_RECTANGLE_PRESOLVE_H_

#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/inlined_vector.h"
#include "absl/types/span.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer_base.h"

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

// Detect whether the fixed boxes of a no_overlap_2d constraint are splitting
// the space into separate components and thus can be replaced by one
// no_overlap_2d constraint per component. If this is not possible, return an
// empty result. Otherwise, return a struct containing what boxes (fixed and
// non-fixed) are needed in each new constraint.
//
// Note that for this to be correct, we need to introduce new boxes to "fill"
// the space occupied by the other components. For example, if we have a
// no_overlap_2d constraint with the fixed boxes and the bounding box of the
// non-fixed boxes as follows:
// +---------------------+
// |                     |
// |                     |
// |                     |
// |                     |
// |                     |
// |   ++++++++++++++++++|
// |   ++++++++++++++++++|
// |   ++++++++++++++++++|
// |*****                |
// |*****                |
// |*****                |
// |                     |
// |                     |
// |                     |
// +---------------------+
// and we are building the new no_overlap_2d constraint for the space below, we
// would need to add two new fixed boxes (the '.' and the 'o' one):
// +---------------------+
// |ooooooooooooooooooooo|
// |ooooooooooooooooooooo|
// |ooooooooooooooooooooo|
// |ooooooooooooooooooooo|
// |ooooooooooooooooooooo|
// |...++++++++++++++++++|
// |...++++++++++++++++++|
// |...++++++++++++++++++|
// |*****                |
// |*****                |
// |*****                |
// |                     |
// |                     |
// |                     |
// +---------------------+
// This ensures that the new no_overlap_2d constraint will impose the box to be
// in that component as it should. Note that in the example above, the number of
// boxes won't really increase after a presolve pass, since the presolve for
// fixed boxes will simplify it to two fixed boxes again.
struct Disjoint2dPackingResult {
  struct Bin {
    // Fixed boxes that the non-fixed boxes in this bin cannot overlap with.
    std::vector<Rectangle> fixed_boxes;
    // Non-fixed boxes on the original problem to copy to this new constraint.
    std::vector<int> non_fixed_box_indexes;
    // Area that is covered by the connected component bin represents encoded as
    // a non-overlapping set of rectangles.
    std::vector<Rectangle> bin_area;
  };
  std::vector<Bin> bins;
};
Disjoint2dPackingResult DetectDisjointRegionIn2dPacking(
    absl::Span<const RectangleInRange> non_fixed_boxes,
    absl::Span<const Rectangle> fixed_boxes, int max_num_components);

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

#endif  // ORTOOLS_SAT_2D_RECTANGLE_PRESOLVE_H_
