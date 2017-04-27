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

#include "ortools/base/integral_types.h"

namespace operations_research {

// Represents a closed interval [start, end]. We must have start <= end.
struct ClosedInterval {
  int64 start;  // Inclusive.
  int64 end;    // Inclusive.

  std::string DebugString() const;
  bool operator==(const ClosedInterval& other) const {
    return start == other.start && end == other.end;
  }
  bool operator<(const ClosedInterval& other) const {
    if (start == other.start) return end < other.end;
    return start < other.start;
  }
};

// Returns a compact std::string of a vector of intervals like "[1,4][6][10,20]".
std::string IntervalsAsString(const std::vector<ClosedInterval>& intervals);

std::ostream& operator<<(std::ostream& out, const ClosedInterval& interval);
std::ostream& operator<<(std::ostream& out,
                         const std::vector<ClosedInterval>& intervals);

// TODO(user): Regroup all the functions below in a SortedDisjointIntervalVector
// class, it will lead to shorter/easier to use names. This will also allow to
// use an inlined vector for the most common case of one or two intervals.

// Converts an unsorted list of integer values to the unique list of
// non-adjacent, disjoint ClosedInterval spanning exactly these values. Eg. for
// values (after sorting): {1, 2, 3, 5, 7, 8, 10, 11}, it returns the list of
// intervals: [1,3] [5] [7,8] [10,11]. Input values may be repeated, with no
// consequence on the output.
//
// The output will satisfy the criteria of IntervalsAreSortedAndDisjoint().
std::vector<ClosedInterval> SortedDisjointIntervalsFromValues(
    std::vector<int64> values);

// Returns true iff we have:
// - The intervals appear in increasing order.
// - for all i: intervals[i].start <= intervals[i].end
// - for all i but the last: intervals[i].end + 1 < intervals[i+1].start
bool IntervalsAreSortedAndDisjoint(
    const std::vector<ClosedInterval>& intervals);

// Returns true iff the given intervals contain the given value.
//
// TODO(user): This works in O(n), but could be made to work in O(log n) for
// long list of intervals.
bool SortedDisjointIntervalsContain(
    const std::vector<ClosedInterval>& intervals, int64 value);

// Returns the intersection of two lists of sorted disjoint intervals in a
// sorted disjoint interval form.
std::vector<ClosedInterval> IntersectionOfSortedDisjointIntervals(
    const std::vector<ClosedInterval>& a, const std::vector<ClosedInterval>& b);

// Returns the union of two lists of sorted disjoint intervals in a
// sorted disjoint interval form.
std::vector<ClosedInterval> UnionOfSortedDisjointIntervals(
    const std::vector<ClosedInterval>& a, const std::vector<ClosedInterval>& b);

// Returns the domain of x + y given that the domain of x is a and the one of y
// is b.
std::vector<ClosedInterval> AdditionOfSortedDisjointIntervals(
    const std::vector<ClosedInterval>& a, const std::vector<ClosedInterval>& b);

// Returns [kint64min, kint64max] minus the given intervals.
std::vector<ClosedInterval> ComplementOfSortedDisjointIntervals(
    const std::vector<ClosedInterval>& intervals);

// For an x in the given intervals, this returns the domain of -x.
//
// Tricky: because the negation of kint64min doesn't fit, we always remove
// kint64min from the given intervals.
std::vector<ClosedInterval> NegationOfSortedDisjointIntervals(
    std::vector<ClosedInterval> intervals);

// Returns the domain of x * coeff given the domain of x.
// To avoid an explosion in the size of the returned vector, the first function
// will actually return a super-set of the domain. For instance [1, 100] * 2
// will be transformed in [2, 200] not [2][4][6]...[200]. The second version
// will try to be exact as long as the result is not too large, and will set
// success to true when this is the case.
std::vector<ClosedInterval> MultiplicationOfSortedDisjointIntervals(
    std::vector<ClosedInterval> intervals, int64 coeff);
std::vector<ClosedInterval> PreciseMultiplicationOfSortedDisjointIntervals(
    std::vector<ClosedInterval> intervals, int64 coeff, bool* success);

// If x * coeff is in the given intervals, this returns the domain of x. Note
// that it is not the same as given the domains of x, return the domain of x /
// coeff because of how the integer division work.
std::vector<ClosedInterval> InverseMultiplicationOfSortedDisjointIntervals(
    std::vector<ClosedInterval> intervals, int64 coeff);

// This class represents a sorted list of disjoint, closed intervals.  When an
// interval is inserted, all intervals that overlap it or that are even adjacent
// to it are merged into one. I.e. [0,14] and [15,30] will be merged to [0,30].
//
// Iterators returned by this class are invalidated by non-const operations.
//
// TODO(user): Templatize the class on the type of the bounds.
class SortedDisjointIntervalList {
 public:
  struct IntervalComparator {
    bool operator()(const ClosedInterval& a, const ClosedInterval& b) const {
      return a.start != b.start ? a.start < b.start : a.end < b.end;
    }
  };
  typedef std::set<ClosedInterval, IntervalComparator> IntervalSet;
  typedef IntervalSet::iterator Iterator;

  SortedDisjointIntervalList();
  explicit SortedDisjointIntervalList(
      const std::vector<ClosedInterval>& intervals);

  // Creates a SortedDisjointIntervalList and fills it with intervals
  // [starts[i]..ends[i]]. All intervals must be consistent (starts[i] <=
  // ends[i]). There's two version, one for int64, one for int.
  //
  // TODO(user): Explain why we favored this API to the more natural
  // input std::vector<ClosedInterval> or std::vector<std::pair<int, int>>.
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

  // If value is in an interval, increase its end by one, otherwise insert the
  // interval [value, value]. In both cases, this returns an iterator to the
  // new/modified interval (possibly merged with others) and fills newly_covered
  // with the new value that was just added in the union of all the intervals.
  //
  // If this causes an interval ending at kint64max to grow, it will die with a
  // CHECK fail.
  Iterator GrowRightByOne(int64 value, int64* newly_covered);

  // Adds all intervals [starts[i]..ends[i]]. Same behavior as InsertInterval()
  // upon invalid intervals. There's a version with int64 and int32.
  void InsertIntervals(const std::vector<int64>& starts,
                       const std::vector<int64>& ends);
  void InsertIntervals(const std::vector<int>& starts,
                       const std::vector<int>& ends);

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
  // for (const ClosedInterval interval : list) {
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

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_SORTED_INTERVAL_LIST_H_
