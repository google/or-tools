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

#include "ortools/util/sorted_interval_list.h"

#include <algorithm>

#include "absl/strings/str_format.h"
#include "ortools/base/logging.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {

std::string ClosedInterval::DebugString() const {
  if (start == end) return absl::StrFormat("[%d]", start);
  return absl::StrFormat("[%d,%d]", start, end);
}

bool IntervalsAreSortedAndNonAdjacent(
    absl::Span<const ClosedInterval> intervals) {
  if (intervals.empty()) return true;
  int64 previous_end;
  bool is_first_interval = true;
  for (const ClosedInterval interval : intervals) {
    if (interval.start > interval.end) return false;
    if (!is_first_interval) {
      // First test make sure that previous_end + 1 will not overflow.
      if (interval.start <= previous_end) return false;
      if (interval.start <= previous_end + 1) return false;
    }
    is_first_interval = false;
    previous_end = interval.end;
  }
  return true;
}

namespace {

template <class Intervals>
std::string IntervalsAsString(const Intervals& intervals) {
  std::string result;
  for (ClosedInterval interval : intervals) {
    result += interval.DebugString();
  }
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

int64 CeilRatio(int64 value, int64 positive_coeff) {
  DCHECK_GT(positive_coeff, 0);
  const int64 result = value / positive_coeff;
  const int64 adjust = static_cast<int64>(result * positive_coeff < value);
  return result + adjust;
}

int64 FloorRatio(int64 value, int64 positive_coeff) {
  DCHECK_GT(positive_coeff, 0);
  const int64 result = value / positive_coeff;
  const int64 adjust = static_cast<int64>(result * positive_coeff > value);
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

Domain::Domain(int64 value) : intervals_({{value, value}}) {}

Domain::Domain(int64 left, int64 right) : intervals_({{left, right}}) {
  if (left > right) intervals_.clear();
}

Domain Domain::AllValues() { return Domain(kint64min, kint64max); }

Domain Domain::FromValues(std::vector<int64> values) {
  std::sort(values.begin(), values.end());
  Domain result;
  for (const int64 v : values) {
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

Domain Domain::FromVectorIntervals(
    const std::vector<std::vector<int64>>& intervals) {
  Domain result;
  for (const std::vector<int64>& interval : intervals) {
    if (interval.size() == 1) {
      result.intervals_.push_back({interval[0], interval[0]});
    } else {
      result.intervals_.push_back({interval[0], interval[1]});
    }
  }
  std::sort(result.intervals_.begin(), result.intervals_.end());
  UnionOfSortedIntervals(&result.intervals_);
  return result;
}

Domain Domain::FromFlatIntervals(const std::vector<int64>& flat_intervals) {
  Domain result;
  const int new_size = flat_intervals.size() / 2;
  for (int i = 0; i < new_size; ++i) {
    result.intervals_.push_back(
        {flat_intervals[2 * i], flat_intervals[2 * i + 1]});
  }
  std::sort(result.intervals_.begin(), result.intervals_.end());
  UnionOfSortedIntervals(&result.intervals_);
  return result;
}

bool Domain::IsEmpty() const { return intervals_.empty(); }

int64 Domain::Size() const {
  int64 size = 0;
  for (const ClosedInterval interval : intervals_) {
    size = operations_research::CapAdd(
        size, operations_research::CapSub(interval.end, interval.start));
  }
  // Because the intervals are closed on both side above, with miss 1 per
  // interval.
  size = operations_research::CapAdd(size, intervals_.size());
  return size;
}

int64 Domain::Min() const {
  CHECK(!IsEmpty());
  return intervals_.front().start;
}

int64 Domain::Max() const {
  CHECK(!IsEmpty());
  return intervals_.back().end;
}

// TODO(user): binary search if size is large?
bool Domain::Contains(int64 value) const {
  for (const ClosedInterval& interval : intervals_) {
    if (interval.start <= value && interval.end >= value) return true;
  }
  return false;
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
  int64 next_start = kint64min;
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
  if (intervals_.empty()) return result;
  std::reverse(result.intervals_.begin(), result.intervals_.end());
  if (result.intervals_.back().end == kint64min) {
    // corner-case
    result.intervals_.pop_back();
  }
  for (ClosedInterval& ref : result.intervals_) {
    std::swap(ref.start, ref.end);
    ref.start = ref.start == kint64min ? kint64max : -ref.start;
    ref.end = ref.end == kint64min ? kint64max : -ref.end;
  }
  DCHECK(IntervalsAreSortedAndNonAdjacent(result.intervals_));
  return result;
}

Domain Domain::IntersectionWith(const Domain& domain) const {
  Domain result;
  const auto& a = intervals_;
  const auto& b = domain.intervals_;
  for (int i = 0, j = 0; i < a.size() && j < b.size();) {
    const ClosedInterval intersection{std::max(a[i].start, b[j].start),
                                      std::min(a[i].end, b[j].end)};
    if (intersection.start > intersection.end) {
      // Intersection is empty, we advance past the first interval of the two.
      if (a[i].start < b[j].start) {
        i++;
      } else {
        j++;
      }
    } else {
      // Intersection is non-empty, we add it to the result and advance past
      // the first interval to finish.
      result.intervals_.push_back(intersection);
      if (a[i].end < b[j].end) {
        i++;
      } else {
        j++;
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

Domain Domain::AdditionWith(const Domain& domain) const {
  Domain result;

  const auto& a = intervals_;
  const auto& b = domain.intervals_;
  for (const ClosedInterval& i : a) {
    for (const ClosedInterval& j : b) {
      result.intervals_.push_back(
          {CapAdd(i.start, j.start), CapAdd(i.end, j.end)});
    }
  }

  std::sort(result.intervals_.begin(), result.intervals_.end());
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

Domain Domain::MultiplicationBy(int64 coeff, bool* exact) const {
  if (exact != nullptr) *exact = true;
  if (intervals_.empty() || coeff == 0) return {};

  const int64 abs_coeff = std::abs(coeff);
  if (abs_coeff > 1 && Size() > kDomainComplexityLimit) {
    if (exact != nullptr) *exact = false;
    return ContinuousMultiplicationBy(coeff);
  }

  Domain result;
  if (abs_coeff != 1) {
    std::vector<int64> individual_values;
    for (const ClosedInterval& i : intervals_) {
      for (int v = i.start; v <= i.end; ++v) {
        individual_values.push_back(CapProd(v, abs_coeff));
      }
    }
    result = Domain::FromValues(individual_values);
  } else {
    result = *this;
  }
  return coeff > 0 ? result : result.Negation();
}

Domain Domain::ContinuousMultiplicationBy(int64 coeff) const {
  Domain result = *this;
  const int64 abs_coeff = std::abs(coeff);
  for (ClosedInterval& i : result.intervals_) {
    i.start = CapProd(i.start, abs_coeff);
    i.end = CapProd(i.end, abs_coeff);
  }
  UnionOfSortedIntervals(&result.intervals_);
  return coeff > 0 ? result : result.Negation();
}

Domain Domain::DivisionBy(int64 coeff) const {
  CHECK_NE(coeff, 0);
  Domain result = *this;
  const int64 abs_coeff = std::abs(coeff);
  for (ClosedInterval& i : result.intervals_) {
    i.start = i.start / abs_coeff;
    i.end = i.end / abs_coeff;
  }
  UnionOfSortedIntervals(&result.intervals_);
  return coeff > 0 ? result : result.Negation();
}

Domain Domain::InverseMultiplicationBy(const int64 coeff) const {
  if (coeff == 0) {
    return Contains(0) ? Domain::AllValues() : Domain();
  }
  Domain result = *this;
  int new_size = 0;
  const int64 abs_coeff = std::abs(coeff);
  for (const ClosedInterval& i : result.intervals_) {
    const int64 start = CeilRatio(i.start, abs_coeff);
    const int64 end = FloorRatio(i.end, abs_coeff);
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
  return coeff > 0 ? result : result.Negation();
}

std::vector<int64> Domain::FlattenedIntervals() const {
  std::vector<int64> result;
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

SortedDisjointIntervalList::SortedDisjointIntervalList() {}

SortedDisjointIntervalList::SortedDisjointIntervalList(
    const std::vector<int64>& starts, const std::vector<int64>& ends) {
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
SortedDisjointIntervalList::BuildComplementOnInterval(int64 start, int64 end) {
  SortedDisjointIntervalList interval_list;
  int64 next_start = start;
  for (auto it = FirstIntervalGreaterOrEqual(start); it != this->end(); ++it) {
    const ClosedInterval& interval = *it;
    const int64 next_end = CapSub(interval.start, 1);
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
    int64 start, int64 end) {
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
    const int64 before_start = start - 1;
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
    const int64 after_end = end + 1;
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
  const int64 new_start = std::min(it1->start, start);
  const int64 new_end = std::max(it3->end, end);
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
    int64 value, int64* newly_covered) {
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
    const std::vector<int64>& starts, const std::vector<int64>& ends) {
  InsertAll(starts, ends);
}

void SortedDisjointIntervalList::InsertIntervals(const std::vector<int>& starts,
                                                 const std::vector<int>& ends) {
  // TODO(user): treat kint32min and kint32max as their kint64 variants.
  InsertAll(starts, ends);
}

SortedDisjointIntervalList::Iterator
SortedDisjointIntervalList::FirstIntervalGreaterOrEqual(int64 value) const {
  const auto it = intervals_.upper_bound({value, kint64max});
  if (it == begin()) return it;
  auto it_prev = it;
  it_prev--;
  DCHECK_LE(it_prev->start, value);
  return it_prev->end >= value ? it_prev : it;
}

SortedDisjointIntervalList::Iterator
SortedDisjointIntervalList::LastIntervalLessOrEqual(int64 value) const {
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
