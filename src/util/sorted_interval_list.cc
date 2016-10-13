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

#include "util/sorted_interval_list.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "util/saturated_arithmetic.h"

namespace operations_research {

SortedDisjointIntervalList::SortedDisjointIntervalList() {}

SortedDisjointIntervalList::SortedDisjointIntervalList(
    const std::vector<int64>& starts, const std::vector<int64>& ends) {
  InsertIntervals(starts, ends);
}

SortedDisjointIntervalList::SortedDisjointIntervalList(
    const std::vector<int>& starts, const std::vector<int>& ends) {
  InsertIntervals(starts, ends);
}

SortedDisjointIntervalList
SortedDisjointIntervalList::BuildComplementOnInterval(int64 start, int64 end) {
  SortedDisjointIntervalList interval_list;
  int64 next_start = start;
  for (auto it = FirstIntervalGreaterOrEqual(start); it != this->end(); ++it) {
    const Interval& interval = *it;
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
    LOG(DFATAL) << "Invalid (start, end) given as Interval: " << start << " .. "
                << end;
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
  const_cast<Interval*>(&(*it))->start = new_start;
  const_cast<Interval*>(&(*it))->end = new_end;
  return it;
}

SortedDisjointIntervalList::Iterator SortedDisjointIntervalList::GrowRightByOne(
    int64 value, int64* newly_covered) {
  auto it = intervals_.upper_bound({value, kint64max});
  auto it_prev = it;

  // No interval containing or adjacent to "value" on the left (i.e. below).
  if (it == begin() || ((--it_prev)->end < value - 1 && value != kint64min)) {
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
      const_cast<Interval*>(&(*it))->start = value;
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
    const_cast<Interval*>(&(*it_prev))->end = it->end;
    intervals_.erase(it);
  } else {
    const_cast<Interval*>(&(*it_prev))->end = it_prev->end + 1;
  }
  return it_prev;
}

template <class T>
void SortedDisjointIntervalList::InsertAll(const std::vector<T>& starts,
                                           const std::vector<T>& ends) {
  CHECK_EQ(starts.size(), ends.size());
  for (int i = 0; i < starts.size(); ++i) InsertInterval(starts[i], ends[i]);
}

void SortedDisjointIntervalList::InsertIntervals(const std::vector<int64>& starts,
                                                 const std::vector<int64>& ends) {
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

std::string SortedDisjointIntervalList::Interval::DebugString() const {
  return StringPrintf("[%" GG_LL_FORMAT "d .. %" GG_LL_FORMAT "d]", start, end);
}

std::string SortedDisjointIntervalList::DebugString() const {
  std::string str = "[";
  for (const Interval& interval : intervals_) {
    if (str.size() > 1) str += ", ";
    str += interval.DebugString();
  }
  str += "]";
  return str;
}

std::ostream& operator<<(std::ostream& out,
                    const SortedDisjointIntervalList::Interval& interval) {
  return out << interval.DebugString();
}
}  // namespace operations_research
