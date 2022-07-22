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

#ifndef OR_TOOLS_MATH_OPT_VALIDATORS_IDS_VALIDATOR_H_
#define OR_TOOLS_MATH_OPT_VALIDATORS_IDS_VALIDATOR_H_

#include <stdint.h>

#include <optional>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/math_opt/core/model_summary.h"

namespace operations_research {
namespace math_opt {

// Checks that the input ids are in [0, max(int64_t)) range and that they are
// strictly increasing.
absl::Status CheckIdsRangeAndStrictlyIncreasing(absl::Span<const int64_t> ids);

// Checks that the elements of ids are a subset of universe. Elements of ids
// do not need to be sorted or distinct. If upper_bound is set, elements must be
// strictly less than upper_bound.
//
// TODO(b/232526223): try merge this with the CheckIdsSubset overload below, or
// at least have one call the other.
absl::Status CheckIdsSubset(absl::Span<const int64_t> ids,
                            const IdNameBiMap& universe,
                            std::optional<int64_t> upper_bound = std::nullopt);

// Checks that the elements of ids are a subset of universe. Elements of ids
// do not need to be sorted or distinct.
absl::Status CheckIdsSubset(absl::Span<const int64_t> ids,
                            const IdNameBiMap& universe,
                            absl::string_view ids_description,
                            absl::string_view universe_description);

// first_ids and second_ids must include distinct ids.
absl::Status CheckIdsIdentical(absl::Span<const int64_t> first_ids,
                               const IdNameBiMap& second_ids,
                               absl::string_view first_description,
                               absl::string_view second_description);

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_VALIDATORS_IDS_VALIDATOR_H_
