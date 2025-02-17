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

#include "ortools/math_opt/validators/ids_validator.h"

#include <stddef.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/model_summary.h"

namespace operations_research::math_opt {

absl::Status CheckIdsRangeAndStrictlyIncreasing(absl::Span<const int64_t> ids) {
  int64_t previous{-1};
  for (int i = 0; i < ids.size(); previous = ids[i], ++i) {
    if (ids[i] < 0 || ids[i] == std::numeric_limits<int64_t>::max()) {
      return util::InvalidArgumentErrorBuilder()
             << "Expected ids to be nonnegative and not max(int64_t) but at "
                "index "
             << i << " found id: " << ids[i];
    }
    if (ids[i] <= previous) {
      return util::InvalidArgumentErrorBuilder()
             << "Expected ids to be strictly increasing, but at index " << i
             << " found id: " << ids[i] << " and at index " << i - 1
             << " found id: " << ids[i - 1];
    }
  }
  return absl::OkStatus();
}

absl::Status CheckIdsSubset(absl::Span<const int64_t> ids,
                            const IdNameBiMap& universe,
                            std::optional<int64_t> upper_bound) {
  for (const int64_t id : ids) {
    if (upper_bound.has_value() && id >= *upper_bound) {
      return util::InvalidArgumentErrorBuilder()
             << "id " << id
             << " should be less than upper bound: " << *upper_bound;
    }
    if (!universe.HasId(id)) {
      return util::InvalidArgumentErrorBuilder() << "id " << id << " not found";
    }
  }
  return absl::OkStatus();
}

absl::Status CheckIdsSubset(absl::Span<const int64_t> ids,
                            const IdNameBiMap& universe,
                            absl::string_view ids_description,
                            absl::string_view universe_description) {
  for (int i = 0; i < ids.size(); ++i) {
    const int64_t id = ids[i];
    if (!universe.HasId(id)) {
      return util::InvalidArgumentErrorBuilder()
             << "Id: " << id << " (at index: " << i << ") in "
             << ids_description << " is missing from " << universe_description;
    }
  }
  return absl::OkStatus();
}

absl::Status CheckIdsIdentical(absl::Span<const int64_t> first_ids,
                               const IdNameBiMap& second_ids,
                               absl::string_view first_description,
                               absl::string_view second_description) {
  if (first_ids.size() != second_ids.Size()) {
    return util::InvalidArgumentErrorBuilder()
           << first_description << " has size " << first_ids.size() << ", but "
           << second_description << " has size " << second_ids.Size();
  }
  RETURN_IF_ERROR(CheckIdsSubset(first_ids, second_ids, first_description,
                                 second_description));
  return absl::OkStatus();
}

}  // namespace operations_research::math_opt
