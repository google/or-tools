// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_UTIL_SORT_H_
#define OR_TOOLS_UTIL_SORT_H_

#include <algorithm>
#include <functional>
#include <iterator>

namespace operations_research {

template <class Iterator>
using value_type_t = typename std::iterator_traits<Iterator>::value_type;

// Sorts the elements in the range [begin, end) in ascending order using the
// comp predicate. The order of equal elements is guaranteed to be preserved
// only if is_stable is true.
//
// This function performs well if the elements in the range [begin, end) are
// almost sorted.
//
// The algorithm operates as follows:
// 1) Check that the range [begin, end) is already sorted by performing a
//    single iteration of bubble-sort.
// 2) Try to sort the range with insertion sort. Insertion sort will stop if it
//    uses the comp predicate more than max_comparisons. Note that the algorithm
//    may actually use the comp predicate more than max_comparisons in order
//    to complete its current insertion.
// 3) If insertion sort exceeds the maximum number of comparisons, the range is
//    sorted using std::stable_sort if is_stable is true or std::sort otherwise.
//
// The first two steps of this algorithm are inspired by the ones recommended
// in Algorithms, 4th Edition by Robert Sedgewick and Kevin Wayne.
template <class Iterator, class Compare = std::less<value_type_t<Iterator>>>
void IncrementalSort(int max_comparisons, Iterator begin, Iterator end,
                     Compare comp = Compare{}, bool is_stable = false) {
  // Ranges of at most one element are already sorted.
  if (std::distance(begin, end) <= 1) return;

  // Perform a single iteration of bubble-sort to place the smallest unsorted
  // element to its correct position.
  Iterator last_sorted = std::prev(end);
  for (auto it = last_sorted; it != begin; --it) {
    if (comp(*it, *std::prev(it))) {
      std::iter_swap(it, std::prev(it));
      last_sorted = it;
    }
  }

  // We know that the elements in the range [begin, last_sorted) are the
  // smallest elements of [begin, end) and are sorted.
  Iterator it = std::next(last_sorted);
  for (; it != end && max_comparisons > 0; ++it) {
    const auto inserted = *it;
    Iterator j = it;
    max_comparisons--;
    while (comp(inserted, *std::prev(j))) {
      max_comparisons--;
      *j = *std::prev(j);
      j--;
    }
    *j = inserted;
  }

  // Stop if insertion sort was able to sort the range.
  if (it == end) return;

  if (is_stable) {
    std::stable_sort(last_sorted, end, comp);
  } else {
    std::sort(last_sorted, end, comp);
  }
}

// Sorts the elements in the range [begin, end) in ascending order using the
// comp predicate. The order of equal elements is guaranteed to be preserved.
//
// This function performs well if the elements in the range [begin, end) are
// almost sorted.
//
// This algorithm is inspired by the ones recommended in Algorithms, 4th Edition
// by Robert Sedgewick and Kevin Wayne.
template <class Iterator, class Compare = std::less<value_type_t<Iterator>>>
void InsertionSort(Iterator begin, Iterator end, Compare comp = Compare{}) {
  // Ranges of at most one element are already sorted.
  if (std::distance(begin, end) <= 1) return;

  // Perform a single iteration of bubble-sort to place the smallest unsorted
  // element to its correct position.
  Iterator last_sorted = std::prev(end);
  for (auto it = last_sorted; it != begin; --it) {
    if (comp(*it, *std::prev(it))) {
      std::iter_swap(it, std::prev(it));
      last_sorted = it;
    }
  }

  // We know that the elements in the range [begin, last_sorted) are the
  // smallest elements of [begin, end) and are sorted.
  for (Iterator it = std::next(last_sorted); it != end; ++it) {
    const auto inserted = *it;
    Iterator j = it;
    while (comp(inserted, *std::prev(j))) {
      *j = *std::prev(j);
      j--;
    }
    *j = inserted;
  }
}

// Sorts the elements in the range [begin, end) in ascending order using the
// comp predicate. The order of equal elements is guaranteed to be preserved
// only if is_stable is true.
//
// This function performs well if the elements in the range [begin, end) are
// almost sorted.
template <class Iterator, class Compare = std::less<value_type_t<Iterator>>>
void IncrementalSort(Iterator begin, Iterator end, Compare comp = Compare{},
                     bool is_stable = false) {
  const int size = std::distance(begin, end);
  if (size <= 32) {
    InsertionSort(begin, end, comp);
  } else {
    IncrementalSort(size * 8, begin, end, comp, is_stable);
  }
}

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_SORT_H_
