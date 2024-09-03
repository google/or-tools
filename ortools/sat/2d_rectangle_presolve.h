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

#include <vector>

#include "absl/types/span.h"
#include "ortools/sat/diffn_util.h"

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

// Given a set of non-overlapping rectangles split in two groups, mandatory and
// optional, try to build a set of as few non-overlapping rectangles as
// possible defining a region R that satisfy:
//   - R \subset (mandatory \union optional);
//   - mandatory \subset R.
//
// The function updates the set of `mandatory_rectangles` with `R` and
// `optional_rectangles` with  `optional_rectangles \setdiff R`. It returns
// true if the `mandatory_rectangles` was updated.
bool ReduceNumberofBoxes(std::vector<Rectangle>* mandatory_rectangles,
                         std::vector<Rectangle>* optional_rectangles);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_2D_RECTANGLE_PRESOLVE_H_
