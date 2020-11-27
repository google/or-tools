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

#ifndef OR_TOOLS_UTIL_SORTED_INTERVAL_LIST_H_
#define OR_TOOLS_UTIL_SORTED_INTERVAL_LIST_H_

#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/types/span.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"

namespace operations_research {

/**
 * Represents a closed interval [start, end]. We must have start <= end.
 */
struct ClosedInterval {
  ClosedInterval() {}
  ClosedInterval(int64 s, int64 e) : start(s), end(e) {
    DLOG_IF(DFATAL, s > e) << "Invalid ClosedInterval(" << s << ", " << e
                           << ")";
  }

  std::string DebugString() const;
  bool operator==(const ClosedInterval& other) const {
    return start == other.start && end == other.end;
  }

  // Because we mainly manipulate vector of disjoint intervals, we only need to
  // sort by the start. We do not care about the order in which interval with
  // the same start appear since they will always be merged into one interval.
  bool operator<(const ClosedInterval& other) const {
    return start < other.start;
  }

  int64 start = 0;  // Inclusive.
  int64 end = 0;    // Inclusive.
};

std::ostream& operator<<(std::ostream& out, const ClosedInterval& interval);
std::ostream& operator<<(std::ostream& out,
                         const std::vector<ClosedInterval>& intervals);

/**
 * Returns true iff we have:
 * - The intervals appear in increasing order.
 * - for all i: intervals[i].start <= intervals[i].end
 *   (should always be true, by construction, but bad intervals can in practice
 *    escape detection in opt mode).
 * - for all i but the last: intervals[i].end + 1 < intervals[i+1].start
 */
bool IntervalsAreSortedAndNonAdjacent(
    absl::Span<const ClosedInterval> intervals);

/**
 * We call \e domain any subset of Int64 = [kint64min, kint64max].
 *
 * This class can be used to represent such set efficiently as a sorted and
 * non-adjacent list of intervals. This is efficient as long as the size of such
 * list stays reasonable.
 *
 * In the comments below, the domain of *this will always be written 'D'.
 * Note that all the functions are safe with respect to integer overflow.
 */
class Domain {
 public:
  /// By default, Domain will be empty.
  Domain() {}

#if !defined(SWIG)
  /// Copy constructor (mandatory as we define the move constructor).
  Domain(const Domain& other) : intervals_(other.intervals_) {}

  /// Copy operator (mandatory as we define the move operator).
  Domain& operator=(const Domain& other) {
    intervals_ = other.intervals_;
    return *this;
  }

  /// Move constructor.
  Domain(Domain&& other) : intervals_(std::move(other.intervals_)) {}

  /// Move operator.
  Domain& operator=(Domain&& other) {
    intervals_ = std::move(other.intervals_);
    return *this;
  }
#endif  // !defined(SWIG)

  /// Constructor for the common case of a singleton domain.
  explicit Domain(int64 value);

  /**
   * Constructor for the common case of a single interval [left, right].
   * If left > right, this will result in the empty domain.
   */
  Domain(int64 left, int64 right);

  /**
   * Returns the full domain Int64.
   */
  static Domain AllValues();

  /**
   * Creates a domain from the union of an unsorted list of integer values.
   * Input values may be repeated, with no consequence on the output
   */
  static Domain FromValues(std::vector<int64> values);

  /**
   * Creates a domain from the union of an unsorted list of intervals.
   */
  static Domain FromIntervals(absl::Span<const ClosedInterval> intervals);

  /**
   * Same as FromIntervals() for a flattened representation (start, end,
   * start, end, ...).
   */
  static Domain FromFlatSpanOfIntervals(absl::Span<const int64> flat_intervals);

  /**
   * This method is available in Python, Java and .NET. It allows
   * building a Domain object from a list of intervals (long[][] in Java and
   * .NET, [[0, 2], [5, 5], [8, 10]] in python).
   */
  static Domain FromVectorIntervals(
      const std::vector<std::vector<int64> >& intervals);

  /**
   * This method is available in Python, Java and .NET. It allows
   * building a Domain object from a flattened list of intervals
   * (long[] in Java and .NET, [0, 2, 5, 5, 8, 10] in python).
   */
  static Domain FromFlatIntervals(const std::vector<int64>& flat_intervals);

  /**
   * This method returns the flattened list of interval bounds of the domain.
   *
   * Thus the domain {0, 1, 2, 5, 8, 9, 10} will return [0, 2, 5, 5,
   * 8, 10] (as a C++ std::vector<int64>, as a java or C# long[], as
   * a python list of integers).
   */
  std::vector<int64> FlattenedIntervals() const;

  /**
   * Returns true if this is the empty set.
   */
  bool IsEmpty() const;

  /**
   * Returns the number of elements in the domain. It is capped at kint64max
   */
  int64 Size() const;

  /**
   * Returns the min value of the domain.
   * The domain must not be empty.
   */
  int64 Min() const;

  /**
   * Returns the max value of the domain.
   * The domain must not be empty.
   */
  int64 Max() const;

  /**
   * Returns true iff the domain is reduced to a single value.
   * The domain must not be empty.
   */
  bool IsFixed() const;

  /**
   * Returns the value of a fixed domain. IsFixed() must be true.
   * This is the same as Min() or Max() but allows for amore readable code and
   * also crash in debug mode if called on a non fixed domain.
   */
  int64 FixedValue() const;

  /**
   * Returns true iff value is in Domain.
   */
  bool Contains(int64 value) const;

  /**
   * Returns true iff D is included in the given domain.
   */
  bool IsIncludedIn(const Domain& domain) const;

  /**
   * Returns the set Int64 ∖ D.
   */
  Domain Complement() const;

  /**
   * Returns {x ∈ Int64, ∃ e ∈ D, x = -e}.
   *
   * Note in particular that if the negation of Int64 is not Int64 but
   * Int64 \ {kint64min} !!
   */
  Domain Negation() const;

  /**
   * Returns the intersection of D and domain.
   */
  Domain IntersectionWith(const Domain& domain) const;

  /**
   * Returns the union of D and domain.
   */
  Domain UnionWith(const Domain& domain) const;

  /**
   * Returns {x ∈ Int64, ∃ a ∈ D, ∃ b ∈ domain, x = a + b}.
   */
  Domain AdditionWith(const Domain& domain) const;

  /**
   * Returns {x ∈ Int64, ∃ e ∈ D, x = e * coeff}.
   *
   * Note that because the resulting domain will only contains multiple of
   * coeff, the size of intervals.size() can become really large. If it is
   * larger than a fixed constant, exact will be set to false and the result
   * will be set to ContinuousMultiplicationBy(coeff).
   *
   * Note that if you multiply by a negative coeff, kint64min will be dropped
   * from the result even if it was here due to how this is implemented.
   */
  Domain MultiplicationBy(int64 coeff, bool* exact = nullptr) const;

  /**
   * If NumIntervals() is too large, this return a superset of the domain.
   */
  Domain RelaxIfTooComplex() const;

  /**
   * Returns a superset of MultiplicationBy() to avoid the explosion in the
   * representation size. This behaves as if we replace the set D of
   * non-adjacent integer intervals by the set of floating-point elements in the
   * same intervals.
   *
   * For instance, [1, 100] * 2 will be transformed in [2, 200] and not in
   * [2][4][6]...[200] like in MultiplicationBy(). Note that this would be
   * similar to a InverseDivisionBy(), but not quite the same because if we
   * look for {x ∈ Int64, ∃ e ∈ D, x / coeff = e}, then we will get [2, 201] in
   * the case above.
   */
  Domain ContinuousMultiplicationBy(int64 coeff) const;

  /**
   * Returns a superset of MultiplicationBy() to avoid the explosion in the
   * representation size. This behaves as if we replace the set D of
   * non-adjacent integer intervals by the set of floating-point elements in the
   * same intervals.
   *
   * For instance, [1, 100] * 2 will be transformed in [2, 200] and not in
   * [2][4][6]...[200] like in MultiplicationBy(). Note that this would be
   * similar to a InverseDivisionBy(), but not quite the same because if we
   * look for {x ∈ Int64, ∃ e ∈ D, x / coeff = e}, then we will get [2, 201] in
   * the case above.
   */
  Domain ContinuousMultiplicationBy(const Domain& domain) const;

  /**
   * Returns {x ∈ Int64, ∃ e ∈ D, x = e / coeff}.
   *
   * For instance Domain(1, 7).DivisionBy(2) == Domain(0, 3).
   */
  Domain DivisionBy(int64 coeff) const;

  /**
   * Returns {x ∈ Int64, ∃ e ∈ D, x * coeff = e}.
   *
   * For instance Domain(1, 7).InverseMultiplicationBy(2) == Domain(1, 3).
   */
  Domain InverseMultiplicationBy(const int64 coeff) const;

  /**
   * Advanced usage. Given some \e implied information on this domain that is
   * assumed to be always true (i.e. only values in the intersection with
   * implied domain matter), this function will simplify the current domain
   * without changing the set of "possible values".
   *
   * More precisely, this will:
   *  - Take the intersection with implied_domain.
   *  - Minimize the number of intervals. For example, if the
   *    domain is [1,2][4] and implied is [1][4], then the domain can be
   *    relaxed to [1, 4] to simplify its complexity without changing the set
   *    of admissible value assuming only implied values can be seen.
   *  - Restrict as much as possible the bounds of the remaining intervals.
   *    For example, if the input is [1,2] and implied is [0,4], then the domain
   * will not be changed.
   *
   * Note that \b domain.SimplifyUsingImpliedDomain(domain) will just return
   * [domain.Min(), domain.Max()]. This is meant to be applied to the right-hand
   * side of a constraint to make its propagation more efficient.
   */
  Domain SimplifyUsingImpliedDomain(const Domain& implied_domain) const;

  /**
   * Returns a compact string of a vector of intervals like "[1,4][6][10,20]".
   */
  std::string ToString() const;

  /**
   * Lexicographic order on the intervals() representation.
   */
  bool operator<(const Domain& other) const;

  bool operator==(const Domain& other) const {
    return intervals_ == other.intervals_;
  }

  bool operator!=(const Domain& other) const {
    return intervals_ != other.intervals_;
  }

  /**
   * Basic read-only std::vector<> wrapping to view a Domain as a sorted list of
   * non-adjacent intervals. Note that we don't expose size() which might be
   * confused with the number of values in the domain.
   */
  int NumIntervals() const { return intervals_.size(); }
  ClosedInterval front() const { return intervals_.front(); }
  ClosedInterval back() const { return intervals_.back(); }
  ClosedInterval operator[](int i) const { return intervals_[i]; }
  absl::InlinedVector<ClosedInterval, 1>::const_iterator begin() const {
    return intervals_.begin();
  }
  absl::InlinedVector<ClosedInterval, 1>::const_iterator end() const {
    return intervals_.end();
  }

  // Deprecated.
  //
  // TODO(user): remove, this makes a copy and is of a different type that our
  // internal InlinedVector() anyway.
  std::vector<ClosedInterval> intervals() const {
    return {intervals_.begin(), intervals_.end()};
  }

 private:
  // Same as Negation() but modify the current domain.
  void NegateInPlace();

  // Some functions relax the domain when its "complexity" (i.e NumIntervals())
  // become too large.
  static constexpr int kDomainComplexityLimit = 100;

  // Invariant: will always satisfy IntervalsAreSortedAndNonAdjacent().
  //
  // Note that we use InlinedVector for the common case of single internal
  // interval. This provide a good performance boost when working with a
  // std::vector<Domain>.
  absl::InlinedVector<ClosedInterval, 1> intervals_;
};

std::ostream& operator<<(std::ostream& out, const Domain& domain);

// Returns the sum of smallest k values in the domain.
int64 SumOfKMinValueInDomain(const Domain& domain, int k);

// Returns the sum of largest k values in the domain.
int64 SumOfKMaxValueInDomain(const Domain& domain, int k);

/**
 * This class represents a sorted list of disjoint, closed intervals.  When an
 * interval is inserted, all intervals that overlap it or are adjacent to it are
 * merged into one. I.e. [0,14] and [15,30] will be merged to [0,30].
 *
 * Iterators returned by this class are invalidated by non-const operations.
 */
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
  typedef IntervalSet::const_iterator ConstIterator;

  SortedDisjointIntervalList();
  explicit SortedDisjointIntervalList(
      const std::vector<ClosedInterval>& intervals);

  /**
   * Creates a SortedDisjointIntervalList and fills it with intervals
   * [starts[i]..ends[i]]. All intervals must be consistent (starts[i] <=
   * ends[i]). There are two version, one for int64 and one for int.
   */
  // TODO(user): Explain why we favored this API to the more natural
  // input std::vector<ClosedInterval> or std::vector<std::pair<int, int>>.
  SortedDisjointIntervalList(const std::vector<int64>& starts,
                             const std::vector<int64>& ends);
  SortedDisjointIntervalList(const std::vector<int>& starts,
                             const std::vector<int>& ends);

  /**
   * Builds the complement of the interval list on the interval [start, end].
   */
  SortedDisjointIntervalList BuildComplementOnInterval(int64 start, int64 end);

  /**
   * Adds the interval [start..end] to the list, and merges overlapping or
   * immediately adjacent intervals ([2, 5] and [6, 7] are adjacent, but
   * [2, 5] and [7, 8] are not).
   *
   * Returns an iterator to the inserted interval (possibly merged with others).
   *
   * If start > end, it does LOG(DFATAL) and returns end() (no interval added).
   */
  Iterator InsertInterval(int64 start, int64 end);

  /**
   * If value is in an interval, increase its end by one, otherwise insert the
   * interval [value, value]. In both cases, this returns an iterator to the
   * new/modified interval (possibly merged with others) and fills newly_covered
   * with the new value that was just added in the union of all the intervals.
   *
   * If this causes an interval ending at kint64max to grow, it will die with a
   * CHECK fail.
   */
  Iterator GrowRightByOne(int64 value, int64* newly_covered);

  /**
   * Adds all intervals [starts[i]..ends[i]].
   *
   * Same behavior as InsertInterval() upon invalid intervals. There's a version
   * with int64 and int32.
   */
  void InsertIntervals(const std::vector<int64>& starts,
                       const std::vector<int64>& ends);
  void InsertIntervals(const std::vector<int>& starts,
                       const std::vector<int>& ends);

  /**
   * Returns the number of disjoint intervals in the list.
   */
  int NumIntervals() const { return intervals_.size(); }

  /**
   * Returns an iterator to either:
   * - the first interval containing or above the given value, or
   * - the last interval containing or below the given value.
   * Returns end() if no interval fulfills that condition.
   *
   * If the value is within an interval, both functions will return it.
   */
  Iterator FirstIntervalGreaterOrEqual(int64 value) const;
  Iterator LastIntervalLessOrEqual(int64 value) const;

  std::string DebugString() const;

  /**
   * Const iterators for SortedDisjoinIntervalList.
   *
   * One example usage is to use range loops in C++:
   * SortedDisjointIntervalList list;
   * ...
   * for (const ClosedInterval& interval : list) {
   *    ...
   * }
   */
  ConstIterator begin() const { return intervals_.begin(); }
  ConstIterator end() const { return intervals_.end(); }

  /**
   * Returns a const& to the last interval. The list must not be empty.
   */
  const ClosedInterval& last() const { return *intervals_.rbegin(); }

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
