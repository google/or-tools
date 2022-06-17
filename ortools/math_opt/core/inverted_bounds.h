// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_MATH_OPT_CORE_INVERTED_BOUNDS_H_
#define OR_TOOLS_MATH_OPT_CORE_INVERTED_BOUNDS_H_

#include <cstdint>
#include <vector>

#include "absl/status/status.h"

namespace operations_research::math_opt {

// The maximum number of variables/constraints with inverted bounds to report.
constexpr std::size_t kMaxInvertedBounds = 10;

// The ids of the variables and linear constraints with inverted bounds
// (lower_bounds > upper_bounds).
//
// This is used internally by solvers to return an error on Solve() when bounds
// are inverted.
struct InvertedBounds {
  // Returns true if this object contains no variable/constraint ids.
  bool empty() const { return variables.empty() && linear_constraints.empty(); }

  // Returns an error listing at most kMaxInvertedBounds variables and linear
  // constraints ids (kMaxInvertedBounds of each). Returns OK status if this
  // object is empty.
  absl::Status ToStatus() const;

  // Ids of the variables with inverted bounds.
  std::vector<int64_t> variables;

  // Ids of the linear constraints with inverted bounds.
  std::vector<int64_t> linear_constraints;
};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CORE_INVERTED_BOUNDS_H_
