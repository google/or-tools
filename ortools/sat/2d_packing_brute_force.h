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

#ifndef OR_TOOLS_SAT_2D_PACKING_BRUTE_FORCE_H_
#define OR_TOOLS_SAT_2D_PACKING_BRUTE_FORCE_H_

#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer.h"

namespace operations_research {
namespace sat {

// Try to solve the Orthogonal Packing Problem by enumeration of all possible
// solutions. Returns an empty vector if the problem is infeasible, otherwise
// returns the items in the positions they appear in the solution in the same
// order as the input arguments.
// Warning: do not call this with too many item as it will run forever.
std::vector<Rectangle> BruteForceOrthogonalPacking(
    absl::Span<const IntegerValue> sizes_x,
    absl::Span<const IntegerValue> sizes_y,
    std::pair<IntegerValue, IntegerValue> bounding_box_size);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_2D_PACKING_BRUTE_FORCE_H_
