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

#ifndef OR_TOOLS_UTIL_SORTED_INTERVAL_LIST_H_
#define OR_TOOLS_UTIL_SORTED_INTERVAL_LIST_H_

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"

namespace operations_research {

/**
 * Represents a closed interval [start, end]. We must have start <= end.
 */
struct ClosedInterval {
#if !defined(SWIG)
  /**
   * An iterator over the values of a ClosedInterval object.
   *
   * To iterate over the values, you can use either a range for loop:
   *
   * ClosedInterval interval = {0, 100};
   * for (const int64_t value : interval) { Work(value); }
   *
   * or a classical for loop:
   * for (auto it = begin(interval); it != end(interval); ++it) { Work(*it); }
   *
   * The iterator is designed to be very efficient, using just a single counter.
   * It works correctly for any combination of `start` and `end` except the full
   * int64_t range (start == INT64_MIN && end == INT64_MAX).
   */
  class Iterator;
#endif  // !defined(SWIG)

  ClosedInterval() {}
  explicit ClosedInterval(int64_t v) : start(v), end(v) {}
  ClosedInterval(int64_t s, int64_t e) : start(s), end(e) {
    DLOG_IF(DFATAL, s > e) << "Invalid ClosedInterval(" << s << ", " << e
                           << ")";
  }

  std::string DebugString() const;
  constexpr bool operator==(const ClosedInterval& other) const {
    return start == other.start && end == other.end;
  }

  // Because we mainly manipulate vector of disjoint intervals, we only need to
  // sort by the start. We do not care about the order in which interval with
  // the same start appear since they will always be merged into one interval.
  constexpr bool operator<(const ClosedInterval& other) const {
    return start < other.start;
  }

  template <typename H>
  friend H AbslHashValue(H h, const ClosedInterval& interval) {
    return H::combine(std::move(h), interval.start, interval.end);
  }

  int64_t start = 0;  // Inclusive.
  int64_t end = 0;    // Inclusive.
};

#if !defined(SWIG)
inline ClosedInterval::Iterator begin(ClosedInterval interval);
inline ClosedInterval::Iterator end(ClosedInterval interval);
#endif  // !defined(SWIG)

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
  Domain(Domain&& other) noexcept : intervals_(std::move(other.intervals_)) {}

  /// Move operator.
  Domain& operator=(Domain&& other) noexcept {
    intervals_ = std::move(other.intervals_);
    return *this;
  }
#endif  // !defined(SWIG)

  /// Constructor for the common case of a singleton domain.
  explicit Domain(int64_t value);

  /**
   * Constructor for the common case of a single interval [left, right].
   * If left > right, this will result in the empty domain.
   */
  Domain(int64_t left, int64_t right);

  /**
   * Returns the full domain Int64.
   */
  static Domain AllValues();

  /**
   * Creates a domain from the union of an unsorted list of integer values.
   * Input values may be repeated, with no consequence on the output
   */
  static Domain FromValues(std::vector<int64_t> values);

  /**
   * Creates a domain from the union of an unsorted list of intervals.
   *
   * Note that invalid intervals (start > end) will log a DFATAL error and will
   * be ignored.
   */
  static Domain FromIntervals(absl::Span<const ClosedInterval> intervals);

  /**
   * Same as FromIntervals() for a flattened representation (start, end,
   * start, end, ...).
   *
   * Note that invalid intervals (start > end) will log a DFATAL error and will
   * be ignored.
   */
  static Domain FromFlatSpanOfIntervals(
      absl::Span<const int64_t> flat_intervals);

  /**
   * This method is available in Python, Java and .NET. It allows
   * building a Domain object from a list of intervals (long[][] in Java and
   * .NET, [[0, 2], [5], [8, 10]] in python).
   *
   * Note that the intervals can be defined with a single value (i.e. [5]), or
   * two increasing values (i.e. [8, 10]).
   *
   * Invalid intervals (start > end) will log a DFATAL error and will be
   * ignored.
   */
  static Domain FromVectorIntervals(
      const std::vector<std::vector<int64_t> >& intervals);

  /**
   * This method is available in Python, Java and .NET. It allows
   * building a Domain object from a flattened list of intervals
   * (long[] in Java and .NET, [0, 2, 5, 5, 8, 10] in python).
   *
   * Note that invalid intervals (start > end) will log a DFATAL error and will
   * be ignored.
   */
  static Domain FromFlatIntervals(const std::vector<int64_t>& flat_intervals);

  /**
   * This method returns the flattened list of interval bounds of the domain.
   *
   * Thus the domain {0, 1, 2, 5, 8, 9, 10} will return [0, 2, 5, 5,
   * 8, 10] (as a C++ std::vector<int64_t>, as a java or C# long[], as
   * a python list of integers).
   */
  std::vector<int64_t> FlattenedIntervals() const;

#if !defined(SWIG)
  /**
   * Allows to iterate over all values of a domain in order with
   * for (const int64_t v : domain.Values()) { ... }
   *
   * Note that this shouldn't be used in another context !!
   * We don't implement full fledged iterator APIs.
   */
  class DomainIterator {
   public:
    explicit DomainIterator(
        const absl::InlinedVector<ClosedInterval, 1>& intervals)
        : value_(intervals.empty() ? int64_t{0} : intervals.front().start),
          it_(intervals.begin()),
          end_(intervals.end()) {}

    int64_t operator*() const { return value_; }

    void operator++() {
      if (value_ == it_->end) {
        ++it_;
        if (it_ != end_) value_ = it_->start;
      } else {
        ++value_;
      }
    }

    bool operator!=(
        absl::InlinedVector<ClosedInterval, 1>::const_iterator end) const {
      return it_ != end;
    }

   private:
    int64_t value_;
    absl::InlinedVector<ClosedInterval, 1>::const_iterator it_;
    absl::InlinedVector<ClosedInterval, 1>::const_iterator end_;
  };
  struct DomainIteratorBeginEnd {
    DomainIterator begin() const { return DomainIterator(intervals); }
    absl::InlinedVector<ClosedInterval, 1>::const_iterator end() const {
      return intervals.end();
    }
    const absl::InlinedVector<ClosedInterval, 1>& intervals;
  };
  struct DomainIteratorBeginEndWithOwnership {
    DomainIterator begin() const { return DomainIterator(intervals); }
    absl::InlinedVector<ClosedInterval, 1>::const_iterator end() const {
      return intervals.end();
    }
    absl::InlinedVector<ClosedInterval, 1> intervals;
  };
  DomainIteratorBeginEnd Values() const& { return {this->intervals_}; }
  DomainIteratorBeginEndWithOwnership Values() const&& {
    return {std::move(this->intervals_)};
  }
#endif  // !defined(SWIG)

  /**
   * Returns true if this is the empty set.
   */
  bool IsEmpty() const;

  /**
   * Returns the number of elements in the domain. It is capped at kint64max
   */
  int64_t Size() const;

  /**
   * Returns true if the domain has just two values. This often mean a non-fixed
   * Boolean variable.
   */
  bool HasTwoValues() const {
    if (intervals_.size() == 1) {
      return intervals_[0].end == intervals_[0].start + 1;
    } else if (intervals_.size() == 2) {
      return intervals_[0].end == intervals_[0].start &&
             intervals_[1].end == intervals_[1].start;
    }
    return false;
  }

  /**
   * Returns the min value of the domain.
   * The domain must not be empty.
   */
  int64_t Min() const;

  /**
   * Returns the max value of the domain.
   * The domain must not be empty.
   */
  int64_t Max() const;

  /**
   * Returns the value closest to zero. If there is a tie, pick positive one.
   */
  int64_t SmallestValue() const;

  /**
   * Returns the value closest to the given point.
   * If there is a tie, pick larger one.
   */
  int64_t ClosestValue(int64_t wanted) const;

  /**
   * Returns the closest value in the domain that is <= (resp. >=) to the input.
   * Do not change the input if there is no such value.
   */
  int64_t ValueAtOrBefore(int64_t input) const;
  int64_t ValueAtOrAfter(int64_t input) const;

  /**
   * If the domain contains zero, this return the simple interval around it.
   * Otherwise, this returns an empty domain.
   */
  Domain PartAroundZero() const;

  /**
   * Returns true iff the domain is reduced to a single value.
   * The domain must not be empty.
   */
  bool IsFixed() const;

  /**
   * Returns the value of a fixed domain. IsFixed() must be true.
   * This is the same as Min() or Max() but allows for a more readable code and
   * also crash in debug mode if called on a non fixed domain.
   */
  int64_t FixedValue() const;

  /**
   * Returns true iff value is in Domain.
   */
  bool Contains(int64_t value) const;

  /**
   * Returns the distance from the value to the domain.
   */
  int64_t Distance(int64_t value) const;

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
  Domain MultiplicationBy(int64_t coeff, bool* exact = nullptr) const;

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
  Domain ContinuousMultiplicationBy(int64_t coeff) const;

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
  Domain DivisionBy(int64_t coeff) const;

  /**
   * Returns {x ∈ Int64, ∃ e ∈ D, x * coeff = e}.
   *
   * For instance Domain(1, 7).InverseMultiplicationBy(2) == Domain(1, 3).
   */
  Domain InverseMultiplicationBy(int64_t coeff) const;

  /**
   * Returns a superset of {x ∈ Int64, ∃ e ∈ D, ∃ m ∈ modulo, x = e % m }.
   *
   * We check that modulo is strictly positive.
   * The sign of the modulo depends on the sign of e.
   * We compute the exact min/max if the modulo is fixed, otherwise we will
   * just return a superset.
   */
  Domain PositiveModuloBySuperset(const Domain& modulo) const;

  /**
   * Returns a superset of {x ∈ Int64, ∃ e ∈ D, ∃ d ∈ divisor, x = e / d }.
   *
   * We check that divisor is strictly positive.
   * For now we just intersect with the min/max possible value.
   */
  Domain PositiveDivisionBySuperset(const Domain& divisor) const;

  /**
   * Returns a superset of {x ∈ Int64, ∃ y ∈ D, x = y * y }.
   */
  Domain SquareSuperset() const;

  /**
   * Returns a superset of {x ∈ Int64, ∃ y ∈ D, x = (a*y + b)*(c*y + d) }.
   */
  Domain QuadraticSuperset(int64_t a, int64_t b, int64_t c, int64_t d) const;

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

  template <typename H>
  friend H AbslHashValue(H h, const Domain& domain) {
    return H::combine(std::move(h), domain.intervals_);
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
int64_t SumOfKMinValueInDomain(const Domain& domain, int k);

// Returns the sum of largest k values in the domain.
int64_t SumOfKMaxValueInDomain(const Domain& domain, int k);

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
   * ends[i]). There are two version, one for int64_t and one for int.
   */
  // TODO(user): Explain why we favored this API to the more natural
  // input std::vector<ClosedInterval> or std::vector<std::pair<int, int>>.
  SortedDisjointIntervalList(const std::vector<int64_t>& starts,
                             const std::vector<int64_t>& ends);
  SortedDisjointIntervalList(const std::vector<int>& starts,
                             const std::vector<int>& ends);

  /**
   * Builds the complement of the interval list on the interval [start, end].
   */
  SortedDisjointIntervalList BuildComplementOnInterval(int64_t start,
                                                       int64_t end);

  /**
   * Adds the interval [start..end] to the list, and merges overlapping or
   * immediately adjacent intervals ([2, 5] and [6, 7] are adjacent, but
   * [2, 5] and [7, 8] are not).
   *
   * Returns an iterator to the inserted interval (possibly merged with others).
   *
   * If start > end, it does LOG(DFATAL) and returns end() (no interval added).
   */
  Iterator InsertInterval(int64_t start, int64_t end);

  /**
   * If value is in an interval, increase its end by one, otherwise insert the
   * interval [value, value]. In both cases, this returns an iterator to the
   * new/modified interval (possibly merged with others) and fills newly_covered
   * with the new value that was just added in the union of all the intervals.
   *
   * If this causes an interval ending at kint64max to grow, it will die with a
   * CHECK fail.
   */
  Iterator GrowRightByOne(int64_t value, int64_t* newly_covered);

  /**
   * Adds all intervals [starts[i]..ends[i]].
   *
   * Same behavior as InsertInterval() upon invalid intervals. There's a version
   * with int64_t and int32_t.
   */
  void InsertIntervals(const std::vector<int64_t>& starts,
                       const std::vector<int64_t>& ends);
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
  Iterator FirstIntervalGreaterOrEqual(int64_t value) const;
  Iterator LastIntervalLessOrEqual(int64_t value) const;

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

// Implementation details.

#if !defined(SWIG)
class ClosedInterval::Iterator {
 public:
  using value_type = int64_t;
  using difference_type = std::ptrdiff_t;

  Iterator(const Iterator&) = default;

  int64_t operator*() const { return static_cast<int64_t>(current_); }

  Iterator& operator++() {
    ++current_;
    return *this;
  }
  void operator++(int) { ++current_; }

  bool operator==(Iterator other) const { return current_ == other.current_; }
  bool operator!=(Iterator other) const { return current_ != other.current_; }

  Iterator& operator=(const Iterator&) = default;

  static Iterator Begin(ClosedInterval interval) {
    AssertNotFullInt64Range(interval);
    return Iterator(static_cast<uint64_t>(interval.start));
  }
  static Iterator End(ClosedInterval interval) {
    AssertNotFullInt64Range(interval);
    return Iterator(static_cast<uint64_t>(interval.end) + 1);
  }

 private:
  explicit Iterator(uint64_t current) : current_(current) {}

  // Triggers a DCHECK-failure when `interval` represents the full int64_t
  // range.
  static void AssertNotFullInt64Range(ClosedInterval interval) {
    DCHECK_NE(static_cast<uint64_t>(interval.start),
              static_cast<uint64_t>(interval.end) + 1)
        << "Iteration over the full int64_t range is not supported.";
  }

  // Implementation note: In C++, integer overflow is well-defined only for
  // unsigned integers. To avoid any compilation issues or UBSan failures, the
  // iterator uses uint64_t internally and relies on the fact that since C++20
  // unsigned->signed conversion is well-defined for all values using modulo
  // arithmetic.
  uint64_t current_;
};

// begin()/end() are required for iteration over ClosedInterval in a range for
// loop.
inline ClosedInterval::Iterator begin(ClosedInterval interval) {
  return ClosedInterval::Iterator::Begin(interval);
}
inline ClosedInterval::Iterator end(ClosedInterval interval) {
  return ClosedInterval::Iterator::End(interval);
}
#endif  // !defined(SWIG)

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_SORTED_INTERVAL_LIST_H_
