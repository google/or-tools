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

#ifndef ORTOOLS_UTIL_DENSE_SET_H_
#define ORTOOLS_UTIL_DENSE_SET_H_

#include <cstddef>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"

namespace operations_research {
// A set of dense non-negative integer values stored in a dense vector.
//
// This is useful when we want to iterate over a small subset of the possible
// values and reuse the memory, or if we want to randomly sample from the set.
//
// If the set is usually small but occasionally very large, iterating over a
// regular hash_set would be less efficient as you would (internal to the hash
// table iterator) have have to iterate over all the buckets in the hash
// table even if empty. If you clear the set frequently to avoid this, you would
// grow and rehash when you have a larger set.
//
// If resize=false, users *must* call reserve(K) where K > any key before
// calling any other method.
template <typename T, bool auto_resize = true>
class DenseSet {
 public:
  using iterator = typename std::vector<T>::const_iterator;
  using const_iterator = typename std::vector<T>::const_iterator;
  using value_type = T;
  static constexpr bool kAutoResize = auto_resize;

  const_iterator begin() const { return values_.begin(); }
  const_iterator end() const { return values_.end(); }
  size_t size() const { return values_.size(); }
  bool empty() const { return values_.empty(); }
  void reserve(size_t size) {
    values_.reserve(size);
    if (size >= positions_.size()) positions_.resize(size, -1);
  }
  size_t capacity() const { return positions_.size(); }

  std::pair<iterator, bool> insert(T value) {
    const int pos = Position(value);
    if (pos == -1) {
      DCHECK_GT(positions_.size(), ToInt(value));
      positions_[ToInt(value)] = values_.size();
      values_.push_back(value);
      return {values_.begin() + positions_[ToInt(value)], true};
    }
    return {values_.begin() + pos, false};
  }

  iterator find(T value) {
    const int pos = Position(value);
    DCHECK_GT(positions_.size(), ToInt(value));
    if (pos < 0) return values_.end();
    return values_.begin() + pos;
  }

  bool contains(T value) const {
    if (kAutoResize && ToInt(value) >= positions_.size()) return false;
    return positions_[ToInt(value)] >= 0;
  }

  void erase(iterator it) {
    const T value = *it;
    DCHECK_GT(positions_.size(), ToInt(value));
    positions_[ToInt(values_.back())] = it - values_.begin();
    positions_[ToInt(value)] = -1;
    // This is a hack to allow erase to work with a const iterator.
    values_[it - begin()] = values_.back();
    values_.pop_back();
  }

  int erase(T value) {
    const int pos = Position(value);
    if (pos < 0) return 0;
    DCHECK_GT(positions_.size(), ToInt(value));
    positions_[ToInt(values_.back())] = pos;
    values_[pos] = values_.back();
    values_.pop_back();
    positions_[ToInt(value)] = -1;
    return 1;
  }

  // The ordering is deterministic given the same sequence of inserts and
  // erases but is arbitrary and should not be relied upon.
  absl::Span<const T> values() const { return values_; }

  void clear() {
    // We expect values_ to be much smaller than the total number of possible
    // values, so just clear entries in the set.
    for (const T value : values_) {
      DCHECK_GT(positions_.size(), ToInt(value));
      positions_[ToInt(value)] = -1;
    }
    values_.clear();
  }

 private:
  static int ToInt(T);
  inline int Position(T value) {
    int int_value = ToInt(value);
    DCHECK_GE(int_value, 0);
    // Automatic Resize increases the CPU time of microbenchmarks by ~30%, but
    // even with kAutoResize=true, DenseSet is still 25x faster than a
    // flat_hash_set<int>.
    if (kAutoResize && int_value >= positions_.size()) {
      positions_.resize(ToInt(value) + 1, -1);
    }
    DCHECK_GT(positions_.size(), int_value);
    return positions_[int_value];
  }
  std::vector<int> positions_;
  std::vector<T> values_;
};

// Like DenseSet, but does not automatically resize the internal position
// vector, which is ~30% faster.
template <typename T>
using UnsafeDenseSet = DenseSet<T, false>;

template <typename T, bool resize>
inline int DenseSet<T, resize>::ToInt(T value) {
  return value.value();
}
template <>
inline int DenseSet<int, true>::ToInt(int value) {
  return value;
}
template <>
inline int DenseSet<int, false>::ToInt(int value) {
  return value;
}

}  // namespace operations_research
#endif  // ORTOOLS_UTIL_DENSE_SET_H_
