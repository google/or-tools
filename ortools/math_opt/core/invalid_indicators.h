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

#ifndef OR_TOOLS_MATH_OPT_CORE_INVALID_INDICATORS_H_
#define OR_TOOLS_MATH_OPT_CORE_INVALID_INDICATORS_H_

#include <cstdint>
#include <vector>

#include "absl/status/status.h"

namespace operations_research::math_opt {

// The maximum number of non-binary indicator variables to report.
constexpr std::size_t kMaxNonBinaryIndicatorVariables = 10;

// Indicator constraints which are invalid because their associated indicator
// variables are not binary.
//
// This is used internally by solvers to return an error on Solve().
struct InvalidIndicators {
  struct VariableAndConstraint {
    int64_t variable;
    int64_t constraint;
  };

  // Returns an error listing at most kMaxNonBinaryIndicatorVariables indicator
  // constraints whose indicator variables are not binary. Returns OK status if
  // this object is empty.
  absl::Status ToStatus() const;

  // Sort the elements lexicographically by (constraint ID, variable ID).
  void Sort();

  // The variable and constraint pairs associated with indicator constraints
  // whose indicator variables are not binary.
  std::vector<VariableAndConstraint> invalid_indicators;
};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CORE_INVALID_INDICATORS_H_
