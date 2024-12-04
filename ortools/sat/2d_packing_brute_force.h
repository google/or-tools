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
#include "ortools/sat/integer_base.h"

namespace operations_research {
namespace sat {

// Try to solve the Orthogonal Packing Problem by enumeration of all possible
// solutions. It will try to preprocess the problem into a smaller one and will
// only try to solve it if it the reduced problem has `max_complexity` or less
// items.
// Warning: do not call this with a too many items and a large value of
// `max_complexity` or it will run forever.
struct BruteForceResult {
  enum class Status {
    kFoundSolution,
    kNoSolutionExists,
    kTooBig,
  };

  Status status;
  // Only non-empty if status==kFoundSolution.
  std::vector<Rectangle> positions_for_solution;
};

BruteForceResult BruteForceOrthogonalPacking(
    absl::Span<const IntegerValue> sizes_x,
    absl::Span<const IntegerValue> sizes_y,
    std::pair<IntegerValue, IntegerValue> bounding_box_size,
    int max_complexity);

// Note that functions taking a Span<PermutableItems> are free to permute them
// as they see fit unless documented otherwise.
struct PermutableItem {
  IntegerValue size_x;
  IntegerValue size_y;
  // Position of the item in the original input.
  int index;
  Rectangle position;
};

// Exposed for testing
bool Preprocess(absl::Span<PermutableItem>& items,
                std::pair<IntegerValue, IntegerValue>& bounding_box_size,
                int max_complexity);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_2D_PACKING_BRUTE_FORCE_H_
