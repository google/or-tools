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

#include "ortools/constraint_solver/sequence_var.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/util/string_array.h"

namespace operations_research {
namespace {
int64_t ValueToIndex(int64_t value) { return value - 1; }

int64_t IndexToValue(int64_t index) { return index + 1; }
}  // namespace

// ----- SequenceVar -----

// TODO(user): Add better class invariants, in particular checks
// that ranked_first, ranked_last, and unperformed are truly disjoint.

SequenceVar::SequenceVar(Solver* const s,
                         const std::vector<IntervalVar*>& intervals,
                         const std::vector<IntVar*>& nexts,
                         const std::string& name)
    : PropagationBaseObject(s),
      intervals_(intervals),
      nexts_(nexts),
      previous_(nexts.size() + 1, -1) {
  set_name(name);
}

SequenceVar::~SequenceVar() {}

IntervalVar* SequenceVar::Interval(int index) const {
  return intervals_[index];
}

IntVar* SequenceVar::Next(int index) const { return nexts_[index]; }

std::string SequenceVar::DebugString() const {
  int64_t hmin, hmax, dmin, dmax;
  HorizonRange(&hmin, &hmax);
  DurationRange(&dmin, &dmax);
  int unperformed = 0;
  int ranked = 0;
  int not_ranked = 0;
  ComputeStatistics(&ranked, &not_ranked, &unperformed);
  return absl::StrFormat(
      "%s(horizon = %d..%d, duration = %d..%d, not ranked = %d, ranked = %d, "
      "nexts = [%s])",
      name(), hmin, hmax, dmin, dmax, not_ranked, ranked,
      JoinDebugStringPtr(nexts_));
}

void SequenceVar::Accept(ModelVisitor* const visitor) const {
  visitor->VisitSequenceVariable(this);
}

void SequenceVar::DurationRange(int64_t* const dmin,
                                int64_t* const dmax) const {
  int64_t dur_min = 0;
  int64_t dur_max = 0;
  for (int i = 0; i < intervals_.size(); ++i) {
    IntervalVar* const t = intervals_[i];
    if (t->MayBePerformed()) {
      if (t->MustBePerformed()) {
        dur_min += t->DurationMin();
      }
      dur_max += t->DurationMax();
    }
  }
  *dmin = dur_min;
  *dmax = dur_max;
}

void SequenceVar::HorizonRange(int64_t* const hmin, int64_t* const hmax) const {
  int64_t hor_min = std::numeric_limits<int64_t>::max();
  int64_t hor_max = std::numeric_limits<int64_t>::min();
  for (int i = 0; i < intervals_.size(); ++i) {
    IntervalVar* const t = intervals_[i];
    if (t->MayBePerformed()) {
      IntervalVar* const t = intervals_[i];
      hor_min = std::min(hor_min, t->StartMin());
      hor_max = std::max(hor_max, t->EndMax());
    }
  }
  *hmin = hor_min;
  *hmax = hor_max;
}

void SequenceVar::ActiveHorizonRange(int64_t* const hmin,
                                     int64_t* const hmax) const {
  absl::flat_hash_set<int> decided;
  for (int i = 0; i < intervals_.size(); ++i) {
    if (intervals_[i]->CannotBePerformed()) {
      decided.insert(i);
    }
  }
  int first = 0;
  while (nexts_[first]->Bound()) {
    first = nexts_[first]->Min();
    if (first < nexts_.size()) {
      decided.insert(ValueToIndex(first));
    } else {
      break;
    }
  }
  if (first != nexts_.size()) {
    UpdatePrevious();
    int last = nexts_.size();
    while (previous_[last] != -1) {
      last = previous_[last];
      decided.insert(ValueToIndex(last));
    }
  }
  int64_t hor_min = std::numeric_limits<int64_t>::max();
  int64_t hor_max = std::numeric_limits<int64_t>::min();
  for (int i = 0; i < intervals_.size(); ++i) {
    if (!decided.contains(i)) {
      IntervalVar* const t = intervals_[i];
      hor_min = std::min(hor_min, t->StartMin());
      hor_max = std::max(hor_max, t->EndMax());
    }
  }
  *hmin = hor_min;
  *hmax = hor_max;
}

void SequenceVar::ComputeStatistics(int* const ranked, int* const not_ranked,
                                    int* const unperformed) const {
  *unperformed = 0;
  for (int i = 0; i < intervals_.size(); ++i) {
    if (intervals_[i]->CannotBePerformed()) {
      (*unperformed)++;
    }
  }
  *ranked = 0;
  int first = 0;
  while (first < nexts_.size() && nexts_[first]->Bound()) {
    first = nexts_[first]->Min();
    (*ranked)++;
  }
  if (first != nexts_.size()) {
    UpdatePrevious();
    int last = nexts_.size();
    while (previous_[last] != -1) {
      last = previous_[last];
      (*ranked)++;
    }
  } else {  // We counted the sentinel.
    (*ranked)--;
  }
  *not_ranked = intervals_.size() - *ranked - *unperformed;
}

int SequenceVar::ComputeForwardFrontier() {
  int first = 0;
  while (first != nexts_.size() && nexts_[first]->Bound()) {
    first = nexts_[first]->Min();
  }
  return first;
}

int SequenceVar::ComputeBackwardFrontier() {
  UpdatePrevious();
  int last = nexts_.size();
  while (previous_[last] != -1) {
    last = previous_[last];
  }
  return last;
}

void SequenceVar::ComputePossibleFirstsAndLasts(
    std::vector<int>* const possible_firsts,
    std::vector<int>* const possible_lasts) {
  possible_firsts->clear();
  possible_lasts->clear();
  absl::flat_hash_set<int> to_check;
  for (int i = 0; i < intervals_.size(); ++i) {
    if (intervals_[i]->MayBePerformed()) {
      to_check.insert(i);
    }
  }
  int first = 0;
  while (nexts_[first]->Bound()) {
    first = nexts_[first]->Min();
    if (first == nexts_.size()) {
      return;
    }
    to_check.erase(ValueToIndex(first));
  }

  IntVar* const forward_var = nexts_[first];
  std::vector<int> candidates;
  int64_t smallest_start_max = std::numeric_limits<int64_t>::max();
  int ssm_support = -1;
  for (int64_t i = forward_var->Min(); i <= forward_var->Max(); ++i) {
    // TODO(user): use domain iterator.
    if (i != 0 && i < IndexToValue(intervals_.size()) &&
        intervals_[ValueToIndex(i)]->MayBePerformed() &&
        forward_var->Contains(i)) {
      const int candidate = ValueToIndex(i);
      candidates.push_back(candidate);
      if (intervals_[candidate]->MustBePerformed()) {
        if (smallest_start_max > intervals_[candidate]->StartMax()) {
          smallest_start_max = intervals_[candidate]->StartMax();
          ssm_support = candidate;
        }
      }
    }
  }
  for (int i = 0; i < candidates.size(); ++i) {
    const int candidate = candidates[i];
    if (candidate == ssm_support ||
        intervals_[candidate]->EndMin() <= smallest_start_max) {
      possible_firsts->push_back(candidate);
    }
  }

  UpdatePrevious();
  int last = nexts_.size();
  while (previous_[last] != -1) {
    last = previous_[last];
    to_check.erase(ValueToIndex(last));
  }

  candidates.clear();
  int64_t biggest_end_min = std::numeric_limits<int64_t>::min();
  int bem_support = -1;
  for (const int candidate : to_check) {
    if (nexts_[IndexToValue(candidate)]->Contains(last)) {
      candidates.push_back(candidate);
      if (intervals_[candidate]->MustBePerformed()) {
        if (biggest_end_min < intervals_[candidate]->EndMin()) {
          biggest_end_min = intervals_[candidate]->EndMin();
          bem_support = candidate;
        }
      }
    }
  }

  for (int i = 0; i < candidates.size(); ++i) {
    const int candidate = candidates[i];
    if (candidate == bem_support ||
        intervals_[candidate]->StartMax() >= biggest_end_min) {
      possible_lasts->push_back(candidate);
    }
  }
}

void SequenceVar::RankSequence(const std::vector<int>& rank_first,
                               const std::vector<int>& rank_last,
                               const std::vector<int>& unperformed) {
  solver()->GetPropagationMonitor()->RankSequence(this, rank_first, rank_last,
                                                  unperformed);
  // Mark unperformed.
  for (const int value : unperformed) {
    intervals_[value]->SetPerformed(false);
  }
  // Forward.
  int forward = 0;
  for (int i = 0; i < rank_first.size(); ++i) {
    const int next = 1 + rank_first[i];
    nexts_[forward]->SetValue(next);
    forward = next;
  }
  // Backward.
  int backward = IndexToValue(intervals_.size());
  for (int i = 0; i < rank_last.size(); ++i) {
    const int next = 1 + rank_last[i];
    nexts_[next]->SetValue(backward);
    backward = next;
  }
}

void SequenceVar::RankFirst(int index) {
  solver()->GetPropagationMonitor()->RankFirst(this, index);
  intervals_[index]->SetPerformed(true);
  int forward_frontier = 0;
  while (forward_frontier != nexts_.size() &&
         nexts_[forward_frontier]->Bound()) {
    forward_frontier = nexts_[forward_frontier]->Min();
    if (forward_frontier == IndexToValue(index)) {
      return;
    }
  }
  DCHECK_LT(forward_frontier, nexts_.size());
  nexts_[forward_frontier]->SetValue(IndexToValue(index));
}

void SequenceVar::RankNotFirst(int index) {
  solver()->GetPropagationMonitor()->RankNotFirst(this, index);
  const int forward_frontier = ComputeForwardFrontier();
  if (forward_frontier < nexts_.size()) {
    nexts_[forward_frontier]->RemoveValue(IndexToValue(index));
  }
}

void SequenceVar::RankLast(int index) {
  solver()->GetPropagationMonitor()->RankLast(this, index);
  intervals_[index]->SetPerformed(true);
  UpdatePrevious();
  int backward_frontier = nexts_.size();
  while (previous_[backward_frontier] != -1) {
    backward_frontier = previous_[backward_frontier];
    if (backward_frontier == IndexToValue(index)) {
      return;
    }
  }
  DCHECK_NE(backward_frontier, 0);
  nexts_[IndexToValue(index)]->SetValue(backward_frontier);
}

void SequenceVar::RankNotLast(int index) {
  solver()->GetPropagationMonitor()->RankNotLast(this, index);
  const int backward_frontier = ComputeBackwardFrontier();
  nexts_[IndexToValue(index)]->RemoveValue(backward_frontier);
}

void SequenceVar::UpdatePrevious() const {
  for (int i = 0; i < intervals_.size() + 2; ++i) {
    previous_[i] = -1;
  }
  for (int i = 0; i < nexts_.size(); ++i) {
    if (nexts_[i]->Bound()) {
      previous_[nexts_[i]->Min()] = i;
    }
  }
}

void SequenceVar::FillSequence(std::vector<int>* const rank_first,
                               std::vector<int>* const rank_last,
                               std::vector<int>* const unperformed) const {
  CHECK(rank_first != nullptr);
  CHECK(rank_last != nullptr);
  CHECK(unperformed != nullptr);
  rank_first->clear();
  rank_last->clear();
  unperformed->clear();
  for (int i = 0; i < intervals_.size(); ++i) {
    if (intervals_[i]->CannotBePerformed()) {
      unperformed->push_back(i);
    }
  }
  int first = 0;
  while (nexts_[first]->Bound()) {
    first = nexts_[first]->Min();
    if (first < nexts_.size()) {
      rank_first->push_back(ValueToIndex(first));
    } else {
      break;
    }
  }
  if (first != nexts_.size()) {
    UpdatePrevious();
    int last = nexts_.size();
    while (previous_[last] != -1) {
      last = previous_[last];
      rank_last->push_back(ValueToIndex(last));
    }
  }
}

}  // namespace operations_research
