// Copyright 2010-2014 Google
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

// We use the notation std::min(arr, i, j) for the minimum arr[x] such that i <= x
// and x < j.
// Range Minimum Query (RMQ) is a data structure preprocessing an array arr so
// that querying std::min(arr, i, j) takes O(1) time. The preprocessing takes
// O(n*log(n)) time and memory.

// Note: There exists an O(n) preprocessing algorithm, but it is considerably
// more involved and the hidden constants behind it are much higher.
//
// The algorithms are well explained in Wikipedia:
// https://en.wikipedia.org/wiki/Range_minimum_query.
//
//
// Implementation: The idea is to cache every std::min(arr, i, j) where j - i is a
// power of two, i.e. j = i + 2^k for some k. Provided this information, we can
// answer all queries in O(1): given a pair (i, j) find the maximum k such that
// i + 2^k < j and note that
// std::min(std::min(arr, i, i+2^k), std::min(arr, j-2^k, j)) = std::min(arr, i, j).

#ifndef OR_TOOLS_UTIL_RANGE_MINIMUM_QUERY_H_
#define OR_TOOLS_UTIL_RANGE_MINIMUM_QUERY_H_

#include <algorithm>
#include <functional>
#include <numeric>
#include <utility>
#include <vector>

#include "util/bitset.h"

namespace operations_research {
template <typename T, typename Compare = std::less<T>>
class RangeMinimumQuery {
 public:
  explicit RangeMinimumQuery(std::vector<T> array);
  RangeMinimumQuery(std::vector<T> array, Compare cmp);

  // Returns the minimum (w.r.t. Compare) arr[x], where x is contained in
  // [from, to).
  T GetMinimumFromRange(int from, int to) const;

  const std::vector<T>& array() const;

 private:
  // cache_[k][i] = std::min(arr, i, i+2^k).
  std::vector<std::vector<T>> cache_;
  Compare cmp_;

  DISALLOW_COPY_AND_ASSIGN(RangeMinimumQuery);
};

// RangeMinimumIndexQuery is similar to RangeMinimumQuery, but
// GetMinimumIndexFromRange returns the index for which the minimum is attained.
template <typename T, typename Compare = std::less<T>>
class RangeMinimumIndexQuery {
 public:
  explicit RangeMinimumIndexQuery(std::vector<T> array);
  RangeMinimumIndexQuery(std::vector<T> array, Compare cmp);

  // Returns an index idx from [from, to) such that arr[idx] is the minimum
  // value of arr over the interval [from, to).
  int GetMinimumIndexFromRange(int from, int to) const;

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
  DISALLOW_COPY_AND_ASSIGN(RangeMinimumIndexQuery);
};

// RangeMinimumQuery implementation
template <typename T, typename Compare>
inline RangeMinimumQuery<T, Compare>::RangeMinimumQuery(std::vector<T> array)
    : RangeMinimumQuery(std::move(array), Compare()) {}

// Reminder: The task is to fill cache_ so that
// cache_[k][i] = std::min(arr, i, i+2^k) for every k <= Log2(n) and i <= n-2^k.
// Note that cache_[k+1][i] = std::min(cache_[k][i], cache_[k][i+2^k]), hence every
// row can be efficiently computed from the previous.
template <typename T, typename Compare>
RangeMinimumQuery<T, Compare>::RangeMinimumQuery(std::vector<T> array,
                                                 Compare cmp)
    : cache_(MostSignificantBitPosition32(array.size()) + 1),
      cmp_(std::move(cmp)) {
  const int array_size = array.size();
  cache_[0] = std::move(array);
  for (int row_idx = 1; row_idx < cache_.size(); ++row_idx) {
    const int row_length = array_size - (1 << row_idx) + 1;
    const int window = 1 << (row_idx - 1);
    cache_[row_idx].resize(row_length);
    for (int col_idx = 0; col_idx < row_length; ++col_idx) {
      cache_[row_idx][col_idx] =
          std::min(cache_[row_idx - 1][col_idx],
                   cache_[row_idx - 1][col_idx + window], cmp_);
    }
  }
}

template <typename T, typename Compare>
inline T RangeMinimumQuery<T, Compare>::GetMinimumFromRange(int from,
                                                            int to) const {
  DCHECK_LE(0, from);
  DCHECK_LT(from, to);
  DCHECK_LE(to, array().size());
  const int log_diff = MostSignificantBitPosition32(to - from);
  const int window = 1 << log_diff;
  const std::vector<T>& row = cache_[log_diff];
  return std::min(row[from], row[to - window], cmp_);
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
    int from, int to) const {
  return rmq_.GetMinimumFromRange(from, to);
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
