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

#ifndef OR_TOOLS_ALGORITHMS_N_CHOOSE_K_H_
#define OR_TOOLS_ALGORITHMS_N_CHOOSE_K_H_

#include <cstdint>

#include "absl/status/statusor.h"

namespace operations_research {
// Returns the number of ways to choose k elements among n, ignoring the order,
// i.e., the binomial coefficient (n, k).
// This is like std::exp(MathUtil::LogCombinations(n, k)), but faster, with
// perfect accuracy, and returning an error iff the result would overflow an
// int64_t or if an argument is invalid (i.e., n < 0, k < 0, or k > n).
//
// NOTE(user): If you need a variation of this, ask the authors: it's very easy
// to add. E.g., other int types, other behaviors (e.g., return 0 if k > n, or
// std::numeric_limits<int64_t>::max() on overflow, etc).
absl::StatusOr<int64_t> NChooseK(int64_t n, int64_t k);
}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_N_CHOOSE_K_H_
