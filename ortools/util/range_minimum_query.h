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

// The range minimum query problem is a range query problem where queries ask
// for the minimum of all elements in ranges of the array.
// The problem is divided into two phases:
// - precomputation: the data structure is given an array A of n elements.
// - query: the data structure must answer queries min(A, begin, end),
//   where min(A, begin, end) = min_{i in [begin, end)} A[i].
// This file has an implementation of the sparse table approach to solving the
// problem, for which the precomputation takes O(n*log(n)) time and memory,
// and further queries take O(1) time.
// Reference: https://en.wikipedia.org/wiki/Range_minimum_query.
//
// The data structure allows to have multiple arrays at the same time, and
// to reset the arrays.
//
// Usage, single range:
// RangeMinimumQuery rmq({10, 100, 30, 300, 70});
// rmq.GetMinimumFromRange(0, 5);  // Returns 10.
// rmq.GetMinimumFromRange(2, 4);  // Returns 30.
//
// Usage, multiple ranges:
// RangeMinimumQuery rmq({10, 100, 30, 300, 70});
// rmq.GetMinimumFromRange(0, 5);  // Returns 10.
// rmq.GetMinimumFromRange(2, 4);  // Returns 30.
//
// // We add another array {-3, 10, 5, 2, 15, 3}.
// const int begin2 = rmq.TablesSize();
// for (const int element : {-3, 10, 5, 2, 15, 3}) {
//   rmq.PushBack(element);
// }
// rmq.MakeSparseTableFromNewElements();
// rmq.GetMinimumFromRange(begin2 + 0, begin2 + 5);  // Returns -3.
// rmq.GetMinimumFromRange(begin2 + 2, begin2 + 4);  // Returns 2.
// rmq.GetMinimumFromRange(begin2 + 4, begin2 + 6);  // Returns 3.
// // The previous array can still be queried.
// rmq.GetMinimumFromRange(1, 3);  // Returns 30.
//
// // Forbidden, query ranges can only be within the same array.
// rmq.GetMinimumFromRange(3, 9);  // Undefined.
//
// rmq.Clear();
// // All arrays have been removed, so no range query can be made.
// rmq.GetMinimumFromRange(0, 5);  // Undefined.
//
// // Add a new range.
// for (const int element : {0, 3, 2}) {
//   rmq.PushBack(element);
// }
// rmq.MakeSparseTableFromNewElements();
// // Queries on the new array can be made.
//
// Note: There are other space/time tradeoffs for this problem, but they are
// generally worse in terms of the constants in the O(1) query time, moreover
// their implementation is generally more involved.
//
// Implementation: The idea is to cache every min(A, i, i+2^k).
// Provided this information, we can answer all queries in O(1): given a pair
// (i, j), first find the maximum k such that i + 2^k < j, then use
// min(A, i, j) = std::min(min(A, i, i+2^k), min(A, j-2^k, j)).

#ifndef OR_TOOLS_UTIL_RANGE_MINIMUM_QUERY_H_
#define OR_TOOLS_UTIL_RANGE_MINIMUM_QUERY_H_

#include <algorithm>
#include <functional>
#include <numeric>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "ortools/util/bitset.h"

namespace operations_research {
template <typename T, typename Compare = std::less<T>>
class RangeMinimumQuery {
 public:
  RangeMinimumQuery() {
    // This class uses the first two rows of cache_ to know the number of new
    // elements, which at any moment is cache_[1].size() - cache_[0].size().
    cache_.resize(2);
  };
  explicit RangeMinimumQuery(std::vector<T> array);
  RangeMinimumQuery(std::vector<T> array, Compare cmp);

  // This type is neither copyable nor movable.
  RangeMinimumQuery(const RangeMinimumQuery&) = delete;
  RangeMinimumQuery& operator=(const RangeMinimumQuery&) = delete;

  // Returns the minimum (w.r.t. Compare) arr[x], where x is contained in
  // [begin_index, end_index).
  // The range [begin_index, end_index) can only cover elements that were new
  // at the same call to MakeTableFromNewElements().
  // When calling this method, there must be no pending new elements, i.e. the
  // last method called apart from TableSize() must not have been PushBack().
  T RangeMinimum(int begin, int end) const;

  void PushBack(T element) { cache_[0].push_back(element); }

  // Generates the sparse table for all new elements, i.e. elements that were
  // added with PushBack() since the latest of these events: construction of
  // this object, a previous call to MakeTableFromNewElements(), or a call to
  // Clear().
  // The range of new elements [begin, end), with begin the Size() at the
  // latest event, and end the current Size().
  void MakeTableFromNewElements();

  // Returns the number of elements in sparse tables, excluding new elements.
  int TableSize() const { return cache_[1].size(); }

  // Clears all tables. This invalidates all further range queries on currently
  // existing tables. This does *not* release memory held by this object.
  void Clear() {
    for (auto& row : cache_) row.clear();
  }

  // Returns the concatenated sequence of all elements in all arrays.
  const std::vector<T>& array() const;

 private:
  // cache_[k][i] = min_{j in [i, i+2^k)} arr[j].
  std::vector<std::vector<T>> cache_;
  Compare cmp_;
};

// RangeMinimumIndexQuery is similar to RangeMinimumQuery, but
// GetMinimumIndexFromRange returns the index for which the minimum is attained.
template <typename T, typename Compare = std::less<T>>
class RangeMinimumIndexQuery {
 public:
  explicit RangeMinimumIndexQuery(std::vector<T> array);
  RangeMinimumIndexQuery(std::vector<T> array, Compare cmp);

  // This type is neither copyable nor movable.
  RangeMinimumIndexQuery(const RangeMinimumIndexQuery&) = delete;
  RangeMinimumIndexQuery& operator=(const RangeMinimumIndexQuery&) = delete;

  // Returns an index idx from [begin, end) such that arr[idx] is the minimum
  // value of arr over the interval [begin, end).
  int GetMinimumIndexFromRange(int begin, int end) const;

  // Returns the original array.
  const std::vector<T>& array() const;

 private:
  // Returns a vector with values 0, 1, ... n - 1 for a given n.
  static std::vector<int> CreateIndexVector(int n);
  struct IndexComparator {
    bool operator()(int lhs_idx, int rhs_idx) const;
    const std::vector<T> array;
    Compare cmp;
  } cmp_;
  const RangeMinimumQuery<int, IndexComparator> rmq_;
};

// RangeMinimumQuery implementation
template <typename T, typename Compare>
inline RangeMinimumQuery<T, Compare>::RangeMinimumQuery(std::vector<T> array)
    : RangeMinimumQuery(std::move(array), Compare()) {}

template <typename T, typename Compare>
RangeMinimumQuery<T, Compare>::RangeMinimumQuery(std::vector<T> array,
                                                 Compare cmp)
    : cache_(2), cmp_(std::move(cmp)) {
  // This class uses the first two rows of cache_ to know the number of new
  // elements.
  cache_[0] = std::move(array);
  MakeTableFromNewElements();
}

template <typename T, typename Compare>
inline T RangeMinimumQuery<T, Compare>::RangeMinimum(int begin, int end) const {
  DCHECK_LE(0, begin);
  DCHECK_LT(begin, end);
  DCHECK_LE(end, cache_[1].size());
  DCHECK_EQ(cache_[0].size(), cache_[1].size());
  const int layer = MostSignificantBitPosition32(end - begin);
  DCHECK_LT(layer, cache_.size());
  const int window = 1 << layer;
  const T* row = cache_[layer].data();
  DCHECK_LE(end - window, cache_[layer].size());
  return std::min(row[begin], row[end - window], cmp_);
}

// Reminder: The task is to fill cache_ so that for i in [begin, end),
// cache_[k][i] = min(arr, i, i+2^k) for every k <= Log2(n) and i <= n-2^k.
// Note that cache_[k+1][i] = min(cache_[k][i], cache_[k][i+2^k]), hence every
// row can be efficiently computed from the previous.
template <typename T, typename Compare>
void RangeMinimumQuery<T, Compare>::MakeTableFromNewElements() {
  const int new_size = cache_[0].size();
  const int old_size = cache_[1].size();
  if (old_size >= new_size) return;
  // This is the minimum number of rows needed to store the sequence of
  // new elements, there may be more rows in the cache.
  const int num_rows = 1 + MostSignificantBitPosition32(new_size - old_size);
  if (cache_.size() < num_rows) cache_.resize(num_rows);
  // Record the new number of elements, wastes just size(T) space.
  cache_[1].resize(new_size);

  for (int row = 1; row < num_rows; ++row) {
    const int half_window = 1 << (row - 1);
    const int last_col = new_size - 2 * half_window;
    if (cache_[row].size() <= last_col) cache_[row].resize(last_col + 1);
    for (int col = old_size; col <= last_col; ++col) {
      cache_[row][col] = std::min(cache_[row - 1][col],
                                  cache_[row - 1][col + half_window], cmp_);
    }
  }
}

template <typename T, typename Compare>
inline const std::vector<T>& RangeMinimumQuery<T, Compare>::array() const {
  return cache_[0];
}

// RangeMinimumIndexQuery implementation
template <typename T, typename Compare>
inline RangeMinimumIndexQuery<T, Compare>::RangeMinimumIndexQuery(
    std::vector<T> array)
    : RangeMinimumIndexQuery(std::move(array), Compare()) {}

template <typename T, typename Compare>
RangeMinimumIndexQuery<T, Compare>::RangeMinimumIndexQuery(std::vector<T> array,
                                                           Compare cmp)
    : cmp_({std::move(array), std::move(cmp)}),
      rmq_(CreateIndexVector(cmp_.array.size()), cmp_) {}

template <typename T, typename Compare>
inline int RangeMinimumIndexQuery<T, Compare>::GetMinimumIndexFromRange(
    int begin, int end) const {
  DCHECK_LT(begin, end);
  return rmq_.RangeMinimum(begin, end);
}

template <typename T, typename Compare>
inline bool RangeMinimumIndexQuery<T, Compare>::IndexComparator::operator()(
    int lhs_idx, int rhs_idx) const {
  return cmp(array[lhs_idx], array[rhs_idx]);
}

template <typename T, typename Compare>
std::vector<int> RangeMinimumIndexQuery<T, Compare>::CreateIndexVector(int n) {
  std::vector<int> result(n, 0);
  std::iota(result.begin(), result.end(), 0);
  return result;
}

template <typename T, typename Compare>
inline const std::vector<T>& RangeMinimumIndexQuery<T, Compare>::array() const {
  return cmp_.array;
}
}  // namespace operations_research
#endif  // OR_TOOLS_UTIL_RANGE_MINIMUM_QUERY_H_
