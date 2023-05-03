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

#ifndef OR_TOOLS_ALGORITHMS_DUPLICATE_REMOVER_H_
#define OR_TOOLS_ALGORITHMS_DUPLICATE_REMOVER_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/log/check.h"
#include "absl/numeric/bits.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "google/protobuf/repeated_field.h"

namespace operations_research {

// This class offers an alternative to gtl::linked_hash_set<> which is:
// - stateless: it works directly on a vector<int> or any similar container,
//   without storing extra data anywhere;
// - faster when the number of unique values is 5K or above.
//
// The memory usage can be O(num_distinct_values) at any time if you use
// AppendAndLazilyRemoveDuplicates(). In fact, unit tests verify that the
// average number of elements kept is ≤ 1.5 * num_distinct_values, making
// it comparable to a flat_hash_set<int> (whose overhead factor is ~1.68).
//
// Usage pattern:
//
//   // One instance of this can handle many sets on the same [0, n) domain.
//   int N = 100'000;
//   DenseIntDuplicateRemover deduper(N);  // Uses N/8 bytes of memory.
//   std::vector<int> values;  // Your container. Could be RepeatedField<int>.
//   for (int x : ...) {
//     deduper.AppendAndLazilyRemoveDuplicates(x, &values);  // O(1) amortized.
//   }
//   deduper.RemoveDuplicates(&values);  // O(values.size())
//
class DenseIntDuplicateRemover {
 public:
  explicit DenseIntDuplicateRemover(int n)
      : n_(n),
        tmp_mask_storage_((n + 7) / 8, 0),
        tmp_mask_(tmp_mask_storage_) {}

  template <class IntContainer>
  void RemoveDuplicates(IntContainer* container);

  template <class IntContainer>
  void AppendAndLazilyRemoveDuplicates(int x, IntContainer* container);

 private:
  template <class IntContainer>
  void Append(int x, IntContainer* container);

  template <class IntContainer>
  void Truncate(size_t new_size, IntContainer* container);

  size_t RemoveDuplicatesInternal(absl::Span<int> span);

  absl::BitGen random_;
  const int n_;
  std::vector<uint8_t> tmp_mask_storage_;
  const absl::Span<uint8_t> tmp_mask_;
};

// _____________________________________________________________________________
// Implementation of the templates.

template <class IntContainer>
void DenseIntDuplicateRemover::RemoveDuplicates(IntContainer* container) {
  const size_t new_size = RemoveDuplicatesInternal(absl::MakeSpan(*container));
  Truncate(new_size, container);
}

template <class IntContainer>
void DenseIntDuplicateRemover::AppendAndLazilyRemoveDuplicates(
    int x, IntContainer* container) {
  DCHECK_GE(x, 0);
  DCHECK_LT(x, n_);
  Append(x, container);
  // ALGORITHM:
  // In order to remain stateless, yet call RemoveDuplicates() often enough
  // that the size of the container remains O(num_distinct_elements), but not
  // too often since we must remain O(1) time amortized, we randomize:
  // every time we append an element, we'll call RemoveDuplicates() with
  // probability 1/k, where k is the current size of the container.
  // That way, the added expected complexity is O(k)*1/k = O(1), yet we know
  // that we'll eventually call it. See the unit tests that verify the claims.
  // As an important optimization, since drawing the pseudo-random number is
  // expensive, we only perform it every kCheckPeriod, and to compensate we
  // multiply the probability by the same amount.
  constexpr int kCheckPeriod = 8;
  static_assert(absl::popcount(unsigned(kCheckPeriod)) == 1,
                "must be power of two");
  const size_t size = container->size();
  if (size & (kCheckPeriod - 1)) return;
  if (size >= 2 * n_ ||
      absl::Uniform<size_t>(random_, 0, container->size()) < kCheckPeriod) {
    RemoveDuplicates(container);
  }
}

template <>
inline void DenseIntDuplicateRemover::Append(int x,
                                             std::vector<int>* container) {
  container->push_back(x);
}

template <>
inline void DenseIntDuplicateRemover::Append(
    int x, google::protobuf::RepeatedField<int>* container) {
  container->Add(x);
}

template <>
inline void DenseIntDuplicateRemover::Truncate(size_t new_size,
                                               std::vector<int>* container) {
  container->resize(new_size);
}

template <>
inline void DenseIntDuplicateRemover::Truncate(
    size_t new_size, google::protobuf::RepeatedField<int>* container) {
  container->Truncate(new_size);
}

}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_DUPLICATE_REMOVER_H_
