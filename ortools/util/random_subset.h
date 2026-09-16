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

#ifndef ORTOOLS_UTIL_RANDOM_SUBSET_H_
#define ORTOOLS_UTIL_RANDOM_SUBSET_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/types/span.h"
#include "ortools/base/types.h"

namespace operations_research {

// Returns a random subset of k integers in [0..n). Works in O(k).
//
// ORDER: Like RandomSubsetOf(): Deterministically dependent on `random`, but
// neither sorted nor a uniformly random order.
//
// NOTE: If you need to call this many times with the same n, you should use
// the faster RandomSubsetOf().
template <typename IntType>
std::vector<IntType> RandomSubset(IntType n, IntType k, absl::BitGenRef random);

// Returns a Span of size `k` of random elements from `shufflable_elements`.
// Mutates `shufflable_elements` by partially shuffling it, only if necessary:
// in the particular cases k=0 and k=shufflable_elements->size(), for example,
// no shuffling is done.
// The returned Span points to the first k elements of `shufflable_elements`.
// As such, it is invalidated by the next call to RandomSubsetOf().
//
// ORDER: The order of elements in the returned Span is neither sorted nor
// a uniformly random shuffle, but it is deterministically tied to `random`.
//
// COMPLEXITY: O(min(k, N-k)) where N = shufflable_elements->size().
template <class T>
absl::Span<T> RandomSubsetOf(int64_t k, std::vector<T>* shufflable_elements,
                             absl::BitGenRef random);

// Returns k sorted numbers, distinct or not, from the [0..n) interval. The
// returned sequence will be picked uniformly at random.
// Base constraints: n ≥ 0, k ≥ 0.
// COMPLEXITY: O(k log k). Can be improved to O(min(n, k log k)) if necessary.
template <typename IntType>
std::vector<IntType> RandomSortedElements(
    IntType n, IntType k, absl::BitGenRef random,
    // If true, the same number may appear multiple times in the output.
    bool allow_repetition = false);

// Like RandomSortedElements(n+1, k), but with an arbitrary offset. The first
// element is always "mini" and the last element is always "maxi".
// Preconditions: k ≥ 2 and maxi ≥ mini.
template <typename IntType>
std::vector<IntType> RandomValuesSpanning(
    IntType mini, IntType maxi, IntType num_values, absl::BitGenRef random,
    // If true, the same number may appear multiple times in the output.
    bool allow_repetition = false);

// Returns `k` nonnegative numbers whose sum is equal to `sum`,
// picked uniformly at random. Same complexity as RandomSortedElements().
template <typename IntType>
std::vector<IntType> RandomSplit(IntType sum, IntType k,
                                 absl::BitGenRef random);

// _____________________________________________________________________________
// Implementation of the templates.

template <typename IntType>
std::vector<IntType> RandomSubset(IntType n, IntType k,
                                  absl::BitGenRef random) {
  CHECK_GE(k, 0);
  CHECK_LE(k, n);
  // Performance optimizations for small k.
  if (k <= 2) {
    if (k == 0) return {};
    if (k == 1) return {absl::Uniform<IntType>(random, 0, n)};
    // k = 2. We pick `a` uniformly, then pick `b-a` uniformly in [1, n).
    // We obtain b from b-a by shifting it by `a`, circling back over n, and
    // avoiding overflows.
    const IntType a = absl::Uniform<IntType>(random, 0, n);
    IntType b = absl::Uniform<IntType>(random, 1, n);  // This is b-a so far.
    if (b < n - a) {
      b += a;
      DCHECK_LT(b, n);
    } else {
      b -= n - a;  // Like b += a [mod n], but without overflow.
      DCHECK_GE(b, 0);
    }
    return {a, b};
  }
  // This is a weird formula, based on data: go/random-subset-benchmark
  const int kThresholdRatio = n < 30000      ? 100
                              : n < 3000000  ? 50
                              : n < 30000000 ? 16
                                             : 25;
  if (k > n / kThresholdRatio) {
    // It's faster to build the full vector and then use RandomSubsetOf().
    std::vector<IntType> values(static_cast<size_t>(n), 0);
    absl::c_iota(values, 0);
    // Instead of copying the returned Span, we directly resize the "values"
    // vector. This spares a reallocation of memory.
    CHECK_LE(k, kint64max);
    RandomSubsetOf(static_cast<int64_t>(k), &values, random);
    values.resize(static_cast<size_t>(k));
    return values;
  }
  // Else, it's faster to build a sparse set of the drawn values.
  std::vector<IntType> values;
  values.reserve(static_cast<size_t>(k));
  absl::flat_hash_set<IntType> values_set;
  values_set.reserve(static_cast<size_t>(k));
  while (values.size() < k) {
    const IntType v = absl::Uniform<IntType>(random, 0, n);
    if (values_set.insert(v).second) values.push_back(v);
  }
  return values;
}

template <class T>
absl::Span<T> RandomSubsetOf(int64_t k, std::vector<T>* shufflable_elements,
                             absl::BitGenRef random) {
  DCHECK_GE(k, 0);
  const int64_t n = shufflable_elements->size();
  if (k <= n / 2) {
    for (int64_t i = 0; i < k; i++) {
      using std::swap;  // go/using-std-swap
      swap((*shufflable_elements)[i],
           (*shufflable_elements)[absl::Uniform(random, i, n)]);
    }
  } else {  // k > n/2: we shuffle only n-k elements in the back.
    for (int i = n - 1; i >= k; i--) {
      using std::swap;
      swap((*shufflable_elements)[i],
           (*shufflable_elements)[absl::Uniform(random, 0, i + 1)]);
    }
  }
  return {shufflable_elements->data(), static_cast<size_t>(k)};
}

template <typename IntType>
std::vector<IntType> RandomSortedElements(IntType n, IntType k,
                                          absl::BitGenRef random,
                                          bool allow_repetition) {
  if (k == 0) return {};
  if (allow_repetition) {
    // Drawing a sorted list of k elements of [0..n) with repetitions is not
    // trivial: the simple  method of drawing k numbers independently and
    // sorting them is biased, because (0, 0, 0) is 3 times less likely to be
    // generated than (0, 0, 1) for example: the latter can be generated by 3
    // unsorted draws (0,0,1), (0,1,0) and (1,0,0).
    // The trick is to fall back to the perfectly unbiased RandomSubset(), like
    // the case with allow_repetition=false, by drawing in the [0, n+k-1) space
    // and then applying out[i] -= i to each element, thus potentially getting
    // duplicates. It's correct because it's bijective (the inverse is [i]+=i).
    n += k - IntType{1};
    CHECK_GE(n, k - (IntType{1}));  // Detect potential overflows.
  }
  std::vector<IntType> out = RandomSubset(n, k, random);
  std::sort(out.begin(), out.end());
  if (allow_repetition) {
    for (size_t i = 1; i < k; ++i) out[i] -= static_cast<IntType>(i);
  }
  return out;
}

template <typename IntType>
std::vector<IntType> RandomValuesSpanning(IntType mini, IntType maxi,
                                          IntType num_values,
                                          absl::BitGenRef random,
                                          bool allow_repetition) {
  CHECK_GE(maxi, mini);
  CHECK_GE(num_values, 2);
  if (num_values == 2) return {mini, maxi};
  // We generate the inner portion, increment it by mini[+1], then manually
  // prepend mini and append maxi at both ends.
  const IntType n =
      allow_repetition ? maxi - mini + IntType{1} : maxi - mini - IntType{1};
  // Detect overflows here. Note that we could completely avoid overflows by:
  // - making the `n` argument of RandomSortedElements() inclusive;
  // - and using Bits::UnsignedType<IntType>::Type here, to make sure that the
  //   difference (maxi - mini) fits.
  // But it would make the code more complex, so we didn't do it yet.
  CHECK_GE(n, 1);
  std::vector<IntType> inner = RandomSortedElements<IntType>(
      n, num_values - IntType{2}, random, allow_repetition);
  const IntType inner_offset = allow_repetition ? mini : mini + IntType{1};
  if (!allow_repetition) {
    for (IntType& x : inner) x += inner_offset;
  }
  inner.insert(inner.begin(), mini);
  inner.push_back(maxi);
  return inner;
}

template <typename IntType>
std::vector<IntType> RandomSplit(IntType sum, IntType k,
                                 absl::BitGenRef random) {
  if (k == 0) return {};
  // Catch overflows before they happen inside RandomSortedElements().
  CHECK_LE(sum, std::numeric_limits<IntType>::max() - k - 2);
  std::vector<IntType> out =
      RandomValuesSpanning<IntType>(0, sum, k + IntType{1}, random,
                                    /*allow_repetition=*/true);
  for (size_t i = 0; i < k; ++i) {
    out[i] = out[i + 1] - out[i];
  }
  out.pop_back();
  return out;
}

}  // namespace operations_research

#endif  // ORTOOLS_UTIL_RANDOM_SUBSET_H_
