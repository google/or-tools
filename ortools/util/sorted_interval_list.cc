// Copyright 2010-2021 Google LLC
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

#include "ortools/util/sorted_interval_list.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {

std::string ClosedInterval::DebugString() const {
  if (start == end) return absl::StrFormat("[%d]", start);
  return absl::StrFormat("[%d,%d]", start, end);
}

bool IntervalsAreSortedAndNonAdjacent(
    absl::Span<const ClosedInterval> intervals) {
  for (int i = 1; i < intervals.size(); ++i) {
    if (intervals[i - 1].start > intervals[i - 1].end) return false;
    // First test make sure that intervals[i - 1].end + 1 will not overflow.
    if (intervals[i - 1].end >= intervals[i].start ||
        intervals[i - 1].end + 1 >= intervals[i].start) {
      return false;
    }
  }
  return intervals.empty() ? true
                           : intervals.back().start <= intervals.back().end;
}

namespace {

template <class Intervals>
std::string IntervalsAsString(const Intervals& intervals) {
  std::string result;
  for (ClosedInterval interval : intervals) {
    result += interval.DebugString();
  }
  if (result.empty()) result = "[]";
  return result;
}

// Transforms a sorted list of intervals in a sorted DISJOINT list for which
// IntervalsAreSortedAndNonAdjacent() would return true.
void UnionOfSortedIntervals(absl::InlinedVector<ClosedInterval, 1>* intervals) {
  DCHECK(std::is_sorted(intervals->begin(), intervals->end()));
  int new_size = 0;
  for (const ClosedInterval& i : *intervals) {
    if (new_size > 0 && i.start <= CapAdd((*intervals)[new_size - 1].end, 1)) {
      (*intervals)[new_size - 1].end =
          std::max(i.end, (*intervals)[new_size - 1].end);
    } else {
      (*intervals)[new_size++] = i;
    }
  }
  intervals->resize(new_size);

  // This is important for InlinedVector in the case the result is a single
  // intervals.
  intervals->shrink_to_fit();
  DCHECK(IntervalsAreSortedAndNonAdjacent(*intervals));
}

}  // namespace

// TODO(user): Use MathUtil::CeilOfRatio / FloorOfRatio instead.
int64_t CeilRatio(int64_t value, int64_t positive_coeff) {
  DCHECK_GT(positive_coeff, 0);
  const int64_t result = value / positive_coeff;
  const int64_t adjust = static_cast<int64_t>(result * positive_coeff < value);
  return result + adjust;
}

int64_t FloorRatio(int64_t value, int64_t positive_coeff) {
  DCHECK_GT(positive_coeff, 0);
  const int64_t result = value / positive_coeff;
  const int64_t adjust = static_cast<int64_t>(result * positive_coeff > value);
  return result - adjust;
}

std::ostream& operator<<(std::ostream& out, const ClosedInterval& interval) {
  return out << interval.DebugString();
}

std::ostream& operator<<(std::ostream& out,
                         const std::vector<ClosedInterval>& intervals) {
  return out << IntervalsAsString(intervals);
}

std::ostream& operator<<(std::ostream& out, const Domain& domain) {
  return out << IntervalsAsString(domain);
}

Domain::Domain(int64_t value) : intervals_({{value, value}}) {}

// HACK(user): We spare significant time if we use an initializer here, because
// InlineVector<1> is able to recognize the fast path or "exactly one element".
// I was unable to obtain the same performance with any other recipe, I always
// had at least 1 more cycle. See BM_SingleIntervalDomainConstructor.
// Since the constructor takes very few cycles (most likely less than 10),
// that's quite significant.
namespace {
inline ClosedInterval UncheckedClosedInterval(int64_t s, int64_t e) {
  ClosedInterval i;
  i.start = s;
  i.end = e;
  return i;
}
}  // namespace

Domain::Domain(int64_t left, int64_t right)
    : intervals_({UncheckedClosedInterval(left, right)}) {
  if (left > right) intervals_.clear();
}

Domain Domain::AllValues() { return Domain(kint64min, kint64max); }

Domain Domain::FromValues(std::vector<int64_t> values) {
  std::sort(values.begin(), values.end());
  Domain result;
  for (const int64_t v : values) {
    if (result.intervals_.empty() || v > result.intervals_.back().end + 1) {
      result.intervals_.push_back({v, v});
    } else {
      result.intervals_.back().end = v;
    }
  }
  return result;
}

Domain Domain::FromIntervals(absl::Span<const ClosedInterval> intervals) {
  Domain result;
  result.intervals_.assign(intervals.begin(), intervals.end());
  std::sort(result.intervals_.begin(), result.intervals_.end());
  UnionOfSortedIntervals(&result.intervals_);
  return result;
}

Domain Domain::FromFlatSpanOfIntervals(
    absl::Span<const int64_t> flat_intervals) {
  DCHECK(flat_intervals.size() % 2 == 0) << flat_intervals.size();
  Domain result;
  result.intervals_.reserve(flat_intervals.size() / 2);
  for (int i = 0; i < flat_intervals.size(); i += 2) {
    result.intervals_.push_back({flat_intervals[i], flat_intervals[i + 1]});
  }
  std::sort(result.intervals_.begin(), result.intervals_.end());
  UnionOfSortedIntervals(&result.intervals_);
  return result;
}

Domain Domain::FromFlatIntervals(const std::vector<int64_t>& flat_intervals) {
  return FromFlatSpanOfIntervals(absl::MakeSpan(flat_intervals));
}

Domain Domain::FromVectorIntervals(
    const std::vector<std::vector<int64_t>>& intervals) {
  Domain result;
  for (const std::vector<int64_t>& interval : intervals) {
    if (interval.size() == 1) {
      result.intervals_.push_back({interval[0], interval[0]});
    } else {
      DCHECK_EQ(interval.size(), 2);
      result.intervals_.push_back({interval[0], interval[1]});
    }
  }
  std::sort(result.intervals_.begin(), result.intervals_.end());
  UnionOfSortedIntervals(&result.intervals_);
  return result;
}

bool Domain::IsEmpty() const { return intervals_.empty(); }

bool Domain::IsFixed() const { return Min() == Max(); }

int64_t Domain::Size() const {
  int64_t size = 0;
  for (const ClosedInterval interval : intervals_) {
    size = operations_research::CapAdd(
        size, operations_research::CapSub(interval.end, interval.start));
  }
  // Because the intervals are closed on both side above, with miss 1 per
  // interval.
  size = operations_research::CapAdd(size, intervals_.size());
  return size;
}

int64_t Domain::Min() const {
  DCHECK(!IsEmpty());
  return intervals_.front().start;
}

int64_t Domain::Max() const {
  DCHECK(!IsEmpty());
  return intervals_.back().end;
}

int64_t Domain::SmallestValue() const {
  DCHECK(!IsEmpty());
  int64_t result = Min();
  for (const ClosedInterval interval : intervals_) {
    if (interval.start <= 0 && interval.end >= 0) return 0;
    for (const int64_t b : {interval.start, interval.end}) {
      if (b > 0 && b <= std::abs(result)) result = b;
      if (b < 0 && -b < std::abs(result)) result = b;
    }
  }
  return result;
}

int64_t Domain::FixedValue() const {
  DCHECK(IsFixed());
  return intervals_.front().start;
}

bool Domain::Contains(int64_t value) const {
  // Because we only compare by start and there is no duplicate starts, this
  // should be the next interval after the one that has a chance to contains
  // value.
  auto it = std::upper_bound(intervals_.begin(), intervals_.end(),
                             ClosedInterval(value, value));
  if (it == intervals_.begin()) return false;
  --it;
  return value <= it->end;
}

bool Domain::IsIncludedIn(const Domain& domain) const {
  int i = 0;
  const auto& others = domain.intervals_;
  for (const ClosedInterval interval : intervals_) {
    // Find the unique interval in others that contains interval if any.
    for (; i < others.size() && interval.end > others[i].end; ++i) {
    }
    if (i == others.size()) return false;
    if (interval.start < others[i].start) return false;
  }
  return true;
}

Domain Domain::Complement() const {
  Domain result;
  int64_t next_start = kint64min;
  result.intervals_.reserve(intervals_.size() + 1);
  for (const ClosedInterval& interval : intervals_) {
    if (interval.start != kint64min) {
      result.intervals_.push_back({next_start, interval.start - 1});
    }
    if (interval.end == kint64max) return result;
    next_start = interval.end + 1;
  }
  result.intervals_.push_back({next_start, kint64max});
  DCHECK(IntervalsAreSortedAndNonAdjacent(result.intervals_));
  return result;
}

Domain Domain::Negation() const {
  Domain result = *this;
  result.NegateInPlace();
  return result;
}

void Domain::NegateInPlace() {
  if (intervals_.empty()) return;
  std::reverse(intervals_.begin(), intervals_.end());
  if (intervals_.back().end == kint64min) {
    // corner-case
    intervals_.pop_back();
  }
  for (ClosedInterval& ref : intervals_) {
    std::swap(ref.start, ref.end);
    ref.start = ref.start == kint64min ? kint64max : -ref.start;
    ref.end = ref.end == kint64min ? kint64max : -ref.end;
  }
  DCHECK(IntervalsAreSortedAndNonAdjacent(intervals_));
}

Domain Domain::IntersectionWith(const Domain& domain) const {
  Domain result;
  const auto& a = intervals_;
  const auto& b = domain.intervals_;
  for (int i = 0, j = 0; i < a.size() && j < b.size();) {
    if (a[i].start <= b[j].start) {
      if (a[i].end < b[j].start) {
        // Empty intersection. We advance past the first interval.
        ++i;
      } else {  // a[i].end >= b[j].start
        // Non-empty intersection: push back the intersection of these two, and
        // advance past the first interval to finish.
        if (a[i].end <= b[j].end) {
          result.intervals_.push_back({b[j].start, a[i].end});
          ++i;
        } else {  // a[i].end > b[j].end.
          result.intervals_.push_back({b[j].start, b[j].end});
          ++j;
        }
      }
    } else {  // a[i].start > b[i].start.
      // We do the exact same thing as above, but swapping a and b.
      if (b[j].end < a[i].start) {
        ++j;
      } else {  // b[j].end >= a[i].start
        if (b[j].end <= a[i].end) {
          result.intervals_.push_back({a[i].start, b[j].end});
          ++j;
        } else {  // a[i].end > b[j].end.
          result.intervals_.push_back({a[i].start, a[i].end});
          ++i;
        }
      }
    }
  }
  DCHECK(IntervalsAreSortedAndNonAdjacent(result.intervals_));
  return result;
}

Domain Domain::UnionWith(const Domain& domain) const {
  Domain result;
  const auto& a = intervals_;
  const auto& b = domain.intervals_;
  result.intervals_.resize(a.size() + b.size());
  std::merge(a.begin(), a.end(), b.begin(), b.end(), result.intervals_.begin());
  UnionOfSortedIntervals(&result.intervals_);
  return result;
}

// TODO(user): Use a better algorithm.
Domain Domain::AdditionWith(const Domain& domain) const {
  Domain result;

  const auto& a = intervals_;
  const auto& b = domain.intervals_;
  result.intervals_.reserve(a.size() * b.size());
  for (const ClosedInterval& i : a) {
    for (const ClosedInterval& j : b) {
      result.intervals_.push_back(
          {CapAdd(i.start, j.start), CapAdd(i.end, j.end)});
    }
  }

  // The sort is not needed if one of the list is of size 1.
  if (a.size() > 1 && b.size() > 1) {
    std::sort(result.intervals_.begin(), result.intervals_.end());
  }
  UnionOfSortedIntervals(&result.intervals_);
  return result;
}

Domain Domain::RelaxIfTooComplex() const {
  if (NumIntervals() > kDomainComplexityLimit) {
    return Domain(Min(), Max());
  } else {
    return *this;
  }
}

Domain Domain::MultiplicationBy(int64_t coeff, bool* exact) const {
  if (exact != nullptr) *exact = true;
  if (intervals_.empty()) return {};
  if (coeff == 0) return Domain(0);

  const int64_t abs_coeff = std::abs(coeff);
  const int64_t size_if_non_trivial = abs_coeff > 1 ? Size() : 0;
  if (size_if_non_trivial > kDomainComplexityLimit) {
    if (exact != nullptr) *exact = false;
    return ContinuousMultiplicationBy(coeff);
  }

  Domain result;
  if (abs_coeff > 1) {
    const int64_t max_value = kint64max / abs_coeff;
    const int64_t min_value = kint64min / abs_coeff;
    result.intervals_.reserve(size_if_non_trivial);
    for (const ClosedInterval& i : intervals_) {
      for (int64_t v = i.start;; ++v) {
        // We ignore anything that overflow.
        if (v >= min_value && v <= max_value) {
          // Because abs_coeff > 1, all new values are disjoint.
          const int64_t new_value = v * abs_coeff;
          result.intervals_.push_back({new_value, new_value});
        }

        // This is to avoid doing ++v when v is kint64max!
        if (v == i.end) break;
      }
    }
  } else {
    result = *this;
  }
  if (coeff < 0) result.NegateInPlace();
  return result;
}

Domain Domain::ContinuousMultiplicationBy(int64_t coeff) const {
  Domain result = *this;
  const int64_t abs_coeff = std::abs(coeff);
  for (ClosedInterval& i : result.intervals_) {
    i.start = CapProd(i.start, abs_coeff);
    i.end = CapProd(i.end, abs_coeff);
  }
  UnionOfSortedIntervals(&result.intervals_);
  if (coeff < 0) result.NegateInPlace();
  return result;
}

Domain Domain::ContinuousMultiplicationBy(const Domain& domain) const {
  Domain result;
  for (const ClosedInterval& i : this->intervals_) {
    for (const ClosedInterval& j : domain.intervals_) {
      ClosedInterval new_interval;
      const int64_t a = CapProd(i.start, j.start);
      const int64_t b = CapProd(i.end, j.end);
      const int64_t c = CapProd(i.start, j.end);
      const int64_t d = CapProd(i.end, j.start);
      new_interval.start = std::min({a, b, c, d});
      new_interval.end = std::max({a, b, c, d});
      result.intervals_.push_back(new_interval);
    }
  }
  std::sort(result.intervals_.begin(), result.intervals_.end());
  UnionOfSortedIntervals(&result.intervals_);
  return result;
}

Domain Domain::DivisionBy(int64_t coeff) const {
  CHECK_NE(coeff, 0);
  Domain result = *this;
  const int64_t abs_coeff = std::abs(coeff);
  for (ClosedInterval& i : result.intervals_) {
    i.start = i.start / abs_coeff;
    i.end = i.end / abs_coeff;
  }
  UnionOfSortedIntervals(&result.intervals_);
  if (coeff < 0) result.NegateInPlace();
  return result;
}

Domain Domain::InverseMultiplicationBy(const int64_t coeff) const {
  if (coeff == 0) {
    return Contains(0) ? Domain::AllValues() : Domain();
  }
  Domain result = *this;
  int new_size = 0;
  const int64_t abs_coeff = std::abs(coeff);
  for (const ClosedInterval& i : result.intervals_) {
    const int64_t start = CeilRatio(i.start, abs_coeff);
    const int64_t end = FloorRatio(i.end, abs_coeff);
    if (start > end) continue;
    if (new_size > 0 && start == result.intervals_[new_size - 1].end + 1) {
      result.intervals_[new_size - 1].end = end;
    } else {
      result.intervals_[new_size++] = {start, end};
    }
  }
  result.intervals_.resize(new_size);
  result.intervals_.shrink_to_fit();
  DCHECK(IntervalsAreSortedAndNonAdjacent(result.intervals_));
  if (coeff < 0) result.NegateInPlace();
  return result;
}

Domain Domain::PositiveModuloBySuperset(const Domain& modulo) const {
  if (IsEmpty()) return Domain();
  CHECK_GT(modulo.Min(), 0);
  const int64_t max_mod = modulo.Max() - 1;
  const int64_t max = std::min(Max(), max_mod);
  const int64_t min = Min() < 0 ? std::max(Min(), -max_mod) : 0;
  return Domain(min, max);
}

Domain Domain::PositiveDivisionBySuperset(const Domain& divisor) const {
  if (IsEmpty()) return Domain();
  CHECK_GT(divisor.Min(), 0);
  return Domain(std::min(Min() / divisor.Max(), Min() / divisor.Min()),
                std::max(Max() / divisor.Min(), Max() / divisor.Max()));
}

// It is a bit difficult to see, but this code is doing the same thing as
// for all interval in this.UnionWith(implied_domain.Complement())):
//  - Take the two extreme points (min and max) in interval \inter implied.
//  - Append to result [min, max] if these points exists.
Domain Domain::SimplifyUsingImpliedDomain(const Domain& implied_domain) const {
  Domain result;
  if (implied_domain.IsEmpty()) return result;

  int i = 0;
  int64_t min_point;
  int64_t max_point;
  bool started = false;
  for (const ClosedInterval interval : intervals_) {
    // We only "close" the new result interval if it cannot be extended by
    // implied_domain.Complement(). The only extension possible look like:
    // interval_:    ...]   [....
    // implied :   ...]       [...  i  ...]
    if (started && implied_domain.intervals_[i].start < interval.start) {
      result.intervals_.push_back({min_point, max_point});
      started = false;
    }

    // Find the two extreme points in interval \inter implied_domain.
    // Always stop the loop at the first interval with and end strictly greater
    // that interval.end.
    for (; i < implied_domain.intervals_.size(); ++i) {
      const ClosedInterval current = implied_domain.intervals_[i];
      if (current.end >= interval.start && current.start <= interval.end) {
        // Current and interval have a non-empty intersection.
        const int64_t inter_max = std::min(interval.end, current.end);
        if (!started) {
          started = true;
          min_point = std::max(interval.start, current.start);
          max_point = inter_max;
        } else {
          // No need to update the min_point here, and the new inter_max must
          // necessarily be > old one.
          DCHECK_GE(inter_max, max_point);
          max_point = inter_max;
        }
      }
      if (current.end > interval.end) break;
    }
    if (i == implied_domain.intervals_.size()) break;
  }
  if (started) {
    result.intervals_.push_back({min_point, max_point});
  }
  DCHECK(IntervalsAreSortedAndNonAdjacent(result.intervals_));
  return result;
}

std::vector<int64_t> Domain::FlattenedIntervals() const {
  std::vector<int64_t> result;
  for (const ClosedInterval& interval : intervals_) {
    result.push_back(interval.start);
    result.push_back(interval.end);
  }
  return result;
}

bool Domain::operator<(const Domain& other) const {
  const auto& d1 = intervals_;
  const auto& d2 = other.intervals_;
  const int common_size = std::min(d1.size(), d2.size());
  for (int i = 0; i < common_size; ++i) {
    const ClosedInterval& i1 = d1[i];
    const ClosedInterval& i2 = d2[i];
    if (i1.start < i2.start) return true;
    if (i1.start > i2.start) return false;
    if (i1.end < i2.end) return true;
    if (i1.end > i2.end) return false;
  }
  return d1.size() < d2.size();
}

std::string Domain::ToString() const { return IntervalsAsString(intervals_); }

int64_t SumOfKMinValueInDomain(const Domain& domain, int k) {
  int64_t current_sum = 0.0;
  int current_index = 0;
  for (const ClosedInterval interval : domain) {
    if (current_index >= k) break;
    for (int v(interval.start); v <= interval.end; ++v) {
      if (current_index >= k) break;
      current_index++;
      current_sum += v;
    }
  }
  return current_sum;
}

int64_t SumOfKMaxValueInDomain(const Domain& domain, int k) {
  return -SumOfKMinValueInDomain(domain.Negation(), k);
}

SortedDisjointIntervalList::SortedDisjointIntervalList() {}

SortedDisjointIntervalList::SortedDisjointIntervalList(
    const std::vector<int64_t>& starts, const std::vector<int64_t>& ends) {
  InsertIntervals(starts, ends);
}

SortedDisjointIntervalList::SortedDisjointIntervalList(
    const std::vector<int>& starts, const std::vector<int>& ends) {
  InsertIntervals(starts, ends);
}

SortedDisjointIntervalList::SortedDisjointIntervalList(
    const std::vector<ClosedInterval>& intervals) {
  for (ClosedInterval interval : intervals) {
    InsertInterval(interval.start, interval.end);
  }
}

SortedDisjointIntervalList
SortedDisjointIntervalList::BuildComplementOnInterval(int64_t start,
                                                      int64_t end) {
  SortedDisjointIntervalList interval_list;
  int64_t next_start = start;
  for (auto it = FirstIntervalGreaterOrEqual(start); it != this->end(); ++it) {
    const ClosedInterval& interval = *it;
    const int64_t next_end = CapSub(interval.start, 1);
    if (next_end > end) break;
    if (next_start <= next_end) {
      interval_list.InsertInterval(next_start, next_end);
    }
    next_start = CapAdd(interval.end, 1);
  }
  if (next_start <= end) {
    interval_list.InsertInterval(next_start, end);
  }
  return interval_list;
}

SortedDisjointIntervalList::Iterator SortedDisjointIntervalList::InsertInterval(
    int64_t start, int64_t end) {
  // start > end could mean an empty interval, but we prefer to LOG(DFATAL)
  // anyway. Really, the user should never give us that.
  if (start > end) {
    LOG(DFATAL) << "Invalid interval: " << ClosedInterval({start, end});
    return intervals_.end();
  }

  auto result = intervals_.insert({start, end});
  if (!result.second) return result.first;  // Duplicate: exit immediately.

  // TODO(user): tune the algorithm below if it proves to be a bottleneck.
  // For example, one could try to avoid an insertion if it's not needed
  // (when the interval merges with a single existing interval or is fully
  // contained by one).

  // Iterate over the previous iterators whose end is after (or almost at) our
  // start. After this, "it1" will point to the first interval that needs to be
  // merged with the current interval (possibly pointing to the current interval
  // itself, if no "earlier" interval should be merged).
  auto it1 = result.first;
  if (start == kint64min) {  // Catch underflows
    it1 = intervals_.begin();
  } else {
    const int64_t before_start = start - 1;
    while (it1 != intervals_.begin()) {
      auto prev_it = it1;
      --prev_it;
      if (prev_it->end < before_start) break;
      it1 = prev_it;
    }
  }

  // Ditto, on the other side: "it2" will point to the interval *after* the last
  // one that should be merged with the current interval.
  auto it2 = result.first;
  if (end == kint64max) {
    it2 = intervals_.end();
  } else {
    const int64_t after_end = end + 1;
    do {
      ++it2;
    } while (it2 != intervals_.end() && it2->start <= after_end);
  }

  // [it1..it2) is the range (inclusive on it1, exclusive on it2) of intervals
  // that should be merged together. We'll set it3 = it2-1 and erase [it1..it3)
  // and set *it3 to the merged interval.
  auto it3 = it2;
  it3--;
  if (it1 == it3) return it3;  // Nothing was merged.
  const int64_t new_start = std::min(it1->start, start);
  const int64_t new_end = std::max(it3->end, end);
  auto it = intervals_.erase(it1, it3);
  // HACK(user): set iterators point to *const* values. Which is expected,
  // because if one alters a set element's value, then it collapses the set
  // property! But in this very special case, we know that we can just overwrite
  // it->start, so we do it.
  const_cast<ClosedInterval*>(&(*it))->start = new_start;
  const_cast<ClosedInterval*>(&(*it))->end = new_end;
  return it;
}

SortedDisjointIntervalList::Iterator SortedDisjointIntervalList::GrowRightByOne(
    int64_t value, int64_t* newly_covered) {
  auto it = intervals_.upper_bound({value, kint64max});
  auto it_prev = it;

  // No interval containing or adjacent to "value" on the left (i.e. below).
  if (it != begin()) {
    --it_prev;
  }
  if (it == begin() || ((value != kint64min) && it_prev->end < value - 1)) {
    *newly_covered = value;
    if (it == end() || it->start != value + 1) {
      // No interval adjacent to "value" on the right: insert a singleton.
      return intervals_.insert(it, {value, value});
    } else {
      // There is an interval adjacent to "value" on the right. Extend it by
      // one. Note that we already know that there won't be a merge with another
      // interval on the left, since there were no interval adjacent to "value"
      // on the left.
      DCHECK_EQ(it->start, value + 1);
      const_cast<ClosedInterval*>(&(*it))->start = value;
      return it;
    }
  }

  // At this point, "it_prev" points to an interval containing or adjacent to
  // "value" on the left: grow it by one, and if it now touches the next
  // interval, merge with it.
  CHECK_NE(kint64max, it_prev->end) << "Cannot grow right by one: the interval "
                                       "that would grow already ends at "
                                       "kint64max";
  *newly_covered = it_prev->end + 1;
  if (it != end() && it_prev->end + 2 == it->start) {
    // We need to merge it_prev with 'it'.
    const_cast<ClosedInterval*>(&(*it_prev))->end = it->end;
    intervals_.erase(it);
  } else {
    const_cast<ClosedInterval*>(&(*it_prev))->end = it_prev->end + 1;
  }
  return it_prev;
}

template <class T>
void SortedDisjointIntervalList::InsertAll(const std::vector<T>& starts,
                                           const std::vector<T>& ends) {
  CHECK_EQ(starts.size(), ends.size());
  for (int i = 0; i < starts.size(); ++i) InsertInterval(starts[i], ends[i]);
}

void SortedDisjointIntervalList::InsertIntervals(
    const std::vector<int64_t>& starts, const std::vector<int64_t>& ends) {
  InsertAll(starts, ends);
}

void SortedDisjointIntervalList::InsertIntervals(const std::vector<int>& starts,
                                                 const std::vector<int>& ends) {
  // TODO(user): treat kint32min and kint32max as their kint64 variants.
  InsertAll(starts, ends);
}

SortedDisjointIntervalList::Iterator
SortedDisjointIntervalList::FirstIntervalGreaterOrEqual(int64_t value) const {
  const auto it = intervals_.upper_bound({value, kint64max});
  if (it == begin()) return it;
  auto it_prev = it;
  it_prev--;
  DCHECK_LE(it_prev->start, value);
  return it_prev->end >= value ? it_prev : it;
}

SortedDisjointIntervalList::Iterator
SortedDisjointIntervalList::LastIntervalLessOrEqual(int64_t value) const {
  const auto it = intervals_.upper_bound({value, kint64max});
  if (it == begin()) return end();
  auto it_prev = it;
  it_prev--;
  return it_prev;
}

std::string SortedDisjointIntervalList::DebugString() const {
  std::string str;
  for (const ClosedInterval& interval : intervals_) {
    str += interval.DebugString();
  }
  return str;
}

}  // namespace operations_research
