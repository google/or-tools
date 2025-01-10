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

#include "ortools/math_opt/core/invalid_indicators.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "ortools/base/status_builder.h"

namespace operations_research::math_opt {

absl::Status InvalidIndicators::ToStatus() const {
  if (invalid_indicators.empty()) {
    return absl::OkStatus();
  }
  std::vector<std::string> printed_pairs;
  for (int i = 0;
       i < std::min(kMaxNonBinaryIndicatorVariables, invalid_indicators.size());
       ++i) {
    printed_pairs.push_back(absl::StrCat("(", invalid_indicators[i].constraint,
                                         ", ", invalid_indicators[i].variable,
                                         ")"));
  }
  auto builder = util::InvalidArgumentErrorBuilder();
  builder
      << "the following (indicator constraint ID, indicator variable ID) pairs "
         "are invalid as the indicator variable is not binary: "
      << absl::StrJoin(printed_pairs, ", ");
  if (invalid_indicators.size() > kMaxNonBinaryIndicatorVariables) {
    builder << ", ...";
  }
  return builder;
}

void InvalidIndicators::Sort() {
  absl::c_sort(invalid_indicators, [](const VariableAndConstraint& lhs,
                                      const VariableAndConstraint& rhs) {
    return std::forward_as_tuple(lhs.constraint, lhs.variable) <
           std::forward_as_tuple(rhs.constraint, rhs.variable);
  });
}

}  // namespace operations_research::math_opt
