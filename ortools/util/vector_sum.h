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

// Fast summation of arrays (vectors, spans) of numbers.
//
// Speed: up to 2x faster than Eigen for float arrays with ~100 elements or more
//        (as of 2023-05).
// Precision: Better or comparable precision to std::accumulate<> on the same
//            value type. That said, the precision is inferior to precise sum
//            algorithm such as ::AccurateSum.

#ifndef OR_TOOLS_UTIL_VECTOR_SUM_H_
#define OR_TOOLS_UTIL_VECTOR_SUM_H_

#include "absl/types/span.h"
#include "ortools/util/vector_sum_internal.h"

namespace operations_research {

// Computes the sum of `values`, assuming that the first element of `values` is
// aligned to 16 bytes.
inline float AlignedVectorSum(absl::Span<const float> values) {
  return internal::VectorSum<4, 4, /*assume_aligned_at_start=*/true>(values);
}

// Computes the sum of `values` without assuming anything.
inline float VectorSum(absl::Span<const float> values) {
  return internal::VectorSum<4, 4, /*assume_aligned_at_start=*/false>(values);
}

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_VECTOR_SUM_H_
