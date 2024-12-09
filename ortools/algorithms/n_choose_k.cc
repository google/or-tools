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

#include "ortools/algorithms/n_choose_k.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "absl/log/check.h"
#include "absl/numeric/int128.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "ortools/algorithms/binary_search.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"

namespace operations_research {
namespace {
// This is the actual computation. It's in O(k).
template <typename Int>
Int InternalChoose(Int n, Int k) {
  DCHECK_LE(k, n - k);
  DCHECK_GT(k, 0);  // Having k>0 lets us start with i=2 (small optimization).
  //  We compute n * (n-1) * ... * (n-k+1) / k! in the best possible order to
  //  guarantee exact results, while trying to avoid overflows. It's not
  //  perfect: we finish with a division by k, which means that me may overflow
  //  even if the result doesn't (by a factor of up to k).
  Int result = n;
  for (Int i = 2; i <= k; ++i) {
    result *= n + 1 - i;
    result /= i;  // The product of i consecutive numbers is divisible by i!.
  }
  return result;
}

// This function precomputes the maximum N such that (N choose K) doesn't
// overflow, for all K.
// When `overflows_intermediate_computation` is true, "overflow" means
// "some overflow happens inside InternalChoose<int64_t>()", and when it's false
// it simply means "the result doesn't fit in an int64_t".
// This is only used in contexts where K ≤ N-K, which implies N ≥ 2K, thus we
// can stop when (2K Choose K) overflows, because at and beyond such K,
// (N Choose K) will always overflow. In practice that happens for K=31 or 34
// depending on `overflows_intermediate_computation`.
template <class Int>
std::vector<Int> LastNThatDoesNotOverflowForAllK(
    bool overflows_intermediate_computation) {
  absl::Time start_time = absl::Now();
  // Given the algorithm used in InternalChoose(), it's not hard to
  // find out when (N choose K) overflows an int64_t during its internal
  // computation: that's when (N choose K) > MAX_INT / k.

  // For K ≤ 2, we hardcode the values of the maximum N. That's because
  // the binary search done below uses MathUtil::LogCombinations, which only
  // works on int32_t, and that's problematic for the max N we get for K=2.
  //
  // For K=2, we want N(N-1) ≤ 2^num_digits, or N(N-1)/2 ≤ 2^num_digits if
  // !overflows_intermediate_computation, i.e. N(N-1) ≤ 2^(num_digits+1).
  // Then, when d is even, N(N-1) ≤ 2^d ⇔ N ≤ 2^(d/2), which is simple.
  // When d is odd, it's harder: N(N-1)≈(N-0.5)² and thus we get the bound
  // N ≤ pow(2.0, d/2)+0.5.
  const int bound_digits = std::numeric_limits<Int>::digits +
                           (overflows_intermediate_computation ? 0 : 1);
  std::vector<Int> result = {
      std::numeric_limits<Int>::max(),  // K=0
      std::numeric_limits<Int>::max(),  // K=1
      bound_digits % 2 == 0
          ? Int{1} << (bound_digits / 2)
          : static_cast<Int>(
                0.5 + std::pow(2.0, 0.5 * std::numeric_limits<Int>::digits)),
  };
  // We find the last N with binary search, for all K. We stop growing K
  // when (2*K Choose K) overflows.
  for (Int k = 3;; ++k) {
    const double max_log_comb =
        overflows_intermediate_computation
            ? std::numeric_limits<Int>::digits * std::log(2) - std::log(k)
            : std::numeric_limits<Int>::digits * std::log(2);
    result.push_back(BinarySearch<Int>(
        /*x_true*/ k,
        // x_false=X, X needs to be large enough so that X choose 3 overflows:
        // (X choose 3)≈(X-1)³/6, so we pick X = 2+6*2^(num_digits/3+1).
        /*x_false=*/
        (static_cast<Int>(
            2 + 6 * std::pow(2.0, std::numeric_limits<Int>::digits / 3 + 1))),
        [k, max_log_comb](Int n) {
          return MathUtil::LogCombinations(n, k) <= max_log_comb;
        }));
    if (result.back() < 2 * k) {
      result.pop_back();
      break;
    }
  }
  // Some DCHECKs for int64_t, which should validate the general formulaes.
  if constexpr (std::numeric_limits<Int>::digits == 63) {
    DCHECK_EQ(result.size(),
              overflows_intermediate_computation
                  ? 31    // 60 Choose 30 < 2^63/30 but 62 Choose 31 > 2^63/31.
                  : 34);  // 66 Choose 33 < 2^63 but 68 Choose 34 > 2^63.
  }
  VLOG(1) << "LastNThatDoesNotOverflowForAllK(): " << absl::Now() - start_time;
  return result;
}

template <typename Int>
bool NChooseKIntermediateComputationOverflowsInt(Int n, Int k) {
  DCHECK_LE(k, n - k);
  static const auto* const result =
      new std::vector<Int>(LastNThatDoesNotOverflowForAllK<Int>(
          /*overflows_intermediate_computation=*/true));
  return k < result->size() ? n > (*result)[k] : true;
}

template <typename Int>
bool NChooseKResultOverflowsInt(Int n, Int k) {
  DCHECK_LE(k, n - k);
  static const auto* const result =
      new std::vector<Int>(LastNThatDoesNotOverflowForAllK<Int>(
          /*overflows_intermediate_computation=*/false));
  return k < result->size() ? n > (*result)[k] : true;
}
}  // namespace

// NOTE(user): If performance ever matters, we could simply precompute and
// store all (N choose K) that don't overflow, there aren't that many of them:
// only a few tens of thousands, after removing simple cases like k ≤ 5.
absl::StatusOr<int64_t> NChooseK(int64_t n, int64_t k) {
  if (n < 0) {
    return absl::InvalidArgumentError(absl::StrFormat("n is negative (%d)", n));
  }
  if (k < 0) {
    return absl::InvalidArgumentError(absl::StrFormat("k is negative (%d)", k));
  }
  if (k > n) {
    return absl::InvalidArgumentError(
        absl::StrFormat("k=%d is greater than n=%d", k, n));
  }
  if (k > n / 2) k = n - k;
  if (k == 0) return 1;
  if (n < std::numeric_limits<uint32_t>::max() &&
      !NChooseKIntermediateComputationOverflowsInt<uint32_t>(n, k)) {
    return static_cast<int64_t>(InternalChoose<uint32_t>(n, k));
  }
  if (!NChooseKIntermediateComputationOverflowsInt<int64_t>(n, k)) {
    return InternalChoose<uint64_t>(n, k);
  }
  if (NChooseKResultOverflowsInt<int64_t>(n, k)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("(%d choose %d) overflows int64", n, k));
  }
  return static_cast<int64_t>(InternalChoose<absl::uint128>(n, k));
}

}  // namespace operations_research
