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

#ifndef OR_TOOLS_SAT_2D_ORTHOGONAL_PACKING_TESTING_H_
#define OR_TOOLS_SAT_2D_ORTHOGONAL_PACKING_TESTING_H_

#include <utility>
#include <vector>

#include "absl/random/bit_gen_ref.h"
#include "absl/types/span.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer_base.h"

namespace operations_research {
namespace sat {

std::vector<Rectangle> GenerateNonConflictingRectangles(int num_rectangles,
                                                        absl::BitGenRef random);

// Alternative way of generating random rectangles. This one generate random
// rectangles and try to pack them using the left-bottom-first order.
std::vector<Rectangle> GenerateNonConflictingRectanglesWithPacking(
    std::pair<IntegerValue, IntegerValue> bb, int average_num_boxes,
    absl::BitGenRef random);

std::vector<RectangleInRange> MakeItemsFromRectangles(
    absl::Span<const Rectangle> rectangles, double slack_factor,
    absl::BitGenRef random);

std::vector<ItemForPairwiseRestriction>
GenerateItemsRectanglesWithNoPairwiseConflict(
    absl::Span<const Rectangle> rectangles, double slack_factor,
    absl::BitGenRef random);

std::vector<ItemForPairwiseRestriction>
GenerateItemsRectanglesWithNoPairwisePropagation(int num_rectangles,
                                                 double slack_factor,
                                                 absl::BitGenRef random);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_2D_ORTHOGONAL_PACKING_TESTING_H_
