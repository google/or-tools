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

#ifndef OR_TOOLS_SAT_2D_ORTHOGONAL_PACKING_H_
#define OR_TOOLS_SAT_2D_ORTHOGONAL_PACKING_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/synchronization.h"

namespace operations_research {
namespace sat {

// Class for solving the orthogonal packing problem when it can be done
// efficiently (ie., not applying any heuristic slower than O(N^2)).
class OrthogonalPackingInfeasibilityDetector {
 public:
  explicit OrthogonalPackingInfeasibilityDetector(
      SharedStatistics* shared_stats)
      : shared_stats_(shared_stats) {}

  ~OrthogonalPackingInfeasibilityDetector();

  enum class Status {
    INFEASIBLE,
    FEASIBLE,
    UNKNOWN,
  };
  struct Result {
    Status result;
    std::vector<int> minimum_unfeasible_subproblem;
  };

  Result TestFeasibility(
      absl::Span<const IntegerValue> sizes_x,
      absl::Span<const IntegerValue> sizes_y,
      std::pair<IntegerValue, IntegerValue> bounding_box_size);

 private:
  std::vector<int> index_by_decreasing_x_size_;
  std::vector<int> index_by_decreasing_y_size_;

  int64_t num_calls_ = 0;
  int64_t num_conflicts_ = 0;
  int64_t num_conflicts_two_items_ = 0;
  int64_t num_trivial_conflicts_ = 0;

  SharedStatistics* shared_stats_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_2D_ORTHOGONAL_PACKING_H_
