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

#include "ortools/math_opt/cpp/basis_status.h"

#include <optional>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/math_opt/cpp/enums.h"

namespace operations_research::math_opt {

std::optional<absl::string_view> Enum<BasisStatus>::ToOptString(
    BasisStatus value) {
  switch (value) {
    case BasisStatus::kFree:
      return "free";
    case BasisStatus::kAtLowerBound:
      return "at_lower_bound";
    case BasisStatus::kAtUpperBound:
      return "at_upper_bound";
    case BasisStatus::kFixedValue:
      return "fixed_value";
    case BasisStatus::kBasic:
      return "basic";
  }
  return std::nullopt;
}

absl::Span<const BasisStatus> Enum<BasisStatus>::AllValues() {
  static constexpr BasisStatus kBasisStatusValues[] = {
      BasisStatus::kFree,         BasisStatus::kAtLowerBound,
      BasisStatus::kAtUpperBound, BasisStatus::kFixedValue,
      BasisStatus::kBasic,
  };
  return absl::MakeConstSpan(kBasisStatusValues);
}

}  // namespace operations_research::math_opt
