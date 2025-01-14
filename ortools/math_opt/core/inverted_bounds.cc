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

#include "ortools/math_opt/core/inverted_bounds.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "ortools/base/status_builder.h"

namespace operations_research::math_opt {

absl::Status InvertedBounds::ToStatus() const {
  if (empty()) {
    return absl::OkStatus();
  }

  auto builder = util::InvalidArgumentErrorBuilder();
  const auto format_bounds_ids = [&builder](const std::string_view name,
                                            const std::vector<int64_t>& ids) {
    if (ids.empty()) {
      return;
    }

    builder << name << " with ids "
            << absl::StrJoin(absl::MakeSpan(ids).subspan(0, kMaxInvertedBounds),
                             ",");
    if (ids.size() > kMaxInvertedBounds) {
      builder << "...";
    }
  };

  format_bounds_ids("variables", variables);
  if (!variables.empty() && !linear_constraints.empty()) {
    builder << " and ";
  }
  format_bounds_ids("linear constraints", linear_constraints);
  builder << " have lower_bound > upper_bound";

  return builder;
}

}  // namespace operations_research::math_opt
