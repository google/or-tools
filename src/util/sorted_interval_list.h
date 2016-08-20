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

#ifndef OR_TOOLS_UTIL_SORTED_INTERVAL_LIST_H_
#define OR_TOOLS_UTIL_SORTED_INTERVAL_LIST_H_

#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "base/integral_types.h"

namespace operations_research {

// This class represents a sorted list of disjoint, closed intervals.  When an
// interval is inserted, all intervals that overlap it or that are even adjacent
// to it are merged into one. I.e. [0, 14] and [15, 30] will be merged to
// [0, 30].
//
// Iterators returned by this class are invalidated by non-const operations.
//
// TODO(user): Templatize the class on the type of the bounds.
class SortedDisjointIntervalList {
 public:
  struct Interval {
    int64 start;  // Inclusive.
    int64 end;    // Inclusive.
    std::string DebugString() const;
  };

  struct IntervalComparator {
    bool operator()(const Interval& a, const Interval& b) const {
      return a.start != b.start ? a.start < b.start : a.end < b.end;
    }
  };
  typedef std::set<Interval, IntervalComparator> IntervalSet;
  typedef IntervalSet::iterator Iterator;

  SortedDisjointIntervalList();

  // Creates a SortedDisjointIntervalList and fills it with intervals
  // [starts[i]..ends[i]]. All intervals must be consistent (starts[i] <=
  // ends[i]). There's two version, one for int64, one for int.
  //
  // TODO(user): Explain why we favored this API to the more natural
  // input std::vector<Interval> or std::vector<std::pair<int, int>>.
  SortedDisjointIntervalList(const std::vector<int64>& starts,
                             const std::vector<int64>& ends);
  SortedDisjointIntervalList(const std::vector<int>& starts,
                             const std::vector<int>& ends);

  // Builds the complement of the interval list on the interval [start, end].
  SortedDisjointIntervalList BuildComplementOnInterval(int64 start, int64 end);

  // Adds the interval [start..end] to the list, and merges overlapping or
  // immediately adjacent intervals ([2, 5] and [6, 7] are adjacent, but
  // [2, 5] and [7, 8] are not).
  //
  // Returns an iterator to the inserted interval (possibly merged with others).
  //
  // If start > end, it does LOG(DFATAL) and returns end() (no interval added).
  Iterator InsertInterval(int64 start, int64 end);

  // Adds all intervals [starts[i]..ends[i]]. Same behavior as InsertInterval()
  // upon invalid intervals. There's a version with int64 and int32.
  void InsertIntervals(const std::vector<int64>& starts, const std::vector<int64>& ends);
  void InsertIntervals(const std::vector<int>& starts, const std::vector<int>& ends);

  // Returns the number of disjoint intervals in the list.
  int NumIntervals() const { return intervals_.size(); }

  // Returns an iterator to either:
  // - the first interval containing or above the given value, or
  // - the last interval containing or below the given value.
  // Returns end() if no interval fulfils that condition.
  //
  // If the value is within an interval, both functions will return it.
  Iterator FirstIntervalGreaterOrEqual(int64 value) const;
  Iterator LastIntervalLessOrEqual(int64 value) const;

  std::string DebugString() const;

  // This is to use range loops in C++:
  // SortedDisjointIntervalList list;
  // ...
  // for (const Interval interval : list) {
  //    ...
  // }
  const Iterator begin() const { return intervals_.begin(); }
  const Iterator end() const { return intervals_.end(); }

  void clear() { intervals_.clear(); }
  void swap(SortedDisjointIntervalList& other) {
    intervals_.swap(other.intervals_);
  }

 private:
  template <class T>
  void InsertAll(const std::vector<T>& starts, const std::vector<T>& ends);

  IntervalSet intervals_;
};

std::ostream& operator<<(std::ostream& out,
                    const SortedDisjointIntervalList::Interval& interval);

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_SORTED_INTERVAL_LIST_H_
