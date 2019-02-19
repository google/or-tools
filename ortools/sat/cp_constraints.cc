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

#include "ortools/sat/cp_constraints.h"

#include <algorithm>

#include "absl/container/flat_hash_map.h"
#include "ortools/base/map_util.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/sort.h"

namespace operations_research {
namespace sat {

bool BooleanXorPropagator::Propagate() {
  bool sum = false;
  int unassigned_index = -1;
  for (int i = 0; i < literals_.size(); ++i) {
    const Literal l = literals_[i];
    if (trail_->Assignment().LiteralIsFalse(l)) {
      sum ^= false;
    } else if (trail_->Assignment().LiteralIsTrue(l)) {
      sum ^= true;
    } else {
      // If we have more than one unassigned literal, we can't deduce anything.
      if (unassigned_index != -1) return true;
      unassigned_index = i;
    }
  }

  // Propagates?
  if (unassigned_index != -1) {
    literal_reason_.clear();
    for (int i = 0; i < literals_.size(); ++i) {
      if (i == unassigned_index) continue;
      const Literal l = literals_[i];
      literal_reason_.push_back(
          trail_->Assignment().LiteralIsFalse(l) ? l : l.Negated());
    }
    const Literal u = literals_[unassigned_index];
    integer_trail_->EnqueueLiteral(sum == value_ ? u.Negated() : u,
                                   literal_reason_, {});
    return true;
  }

  // Ok.
  if (sum == value_) return true;

  // Conflict.
  std::vector<Literal>* conflict = trail_->MutableConflict();
  conflict->clear();
  for (int i = 0; i < literals_.size(); ++i) {
    const Literal l = literals_[i];
    conflict->push_back(trail_->Assignment().LiteralIsFalse(l) ? l
                                                               : l.Negated());
  }
  return false;
}

void BooleanXorPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (const Literal& l : literals_) {
    watcher->WatchLiteral(l, id);
    watcher->WatchLiteral(l.Negated(), id);
  }
}

GreaterThanAtLeastOneOfPropagator::GreaterThanAtLeastOneOfPropagator(
    IntegerVariable target_var, const std::vector<IntegerVariable>& vars,
    const std::vector<IntegerValue>& offsets,
    const std::vector<Literal>& selectors, Model* model)
    : target_var_(target_var),
      vars_(vars),
      offsets_(offsets),
      selectors_(selectors),
      trail_(model->GetOrCreate<Trail>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()) {}

bool GreaterThanAtLeastOneOfPropagator::Propagate() {
  // Compute the min of the lower-bound for the still possible variables.
  // TODO(user): This could be optimized by keeping more info from the last
  // Propagate() calls.
  IntegerValue target_min = kMaxIntegerValue;
  const IntegerValue current_min = integer_trail_->LowerBound(target_var_);
  for (int i = 0; i < vars_.size(); ++i) {
    if (trail_->Assignment().LiteralIsTrue(selectors_[i])) return true;
    if (trail_->Assignment().LiteralIsFalse(selectors_[i])) continue;
    target_min = std::min(target_min,
                          integer_trail_->LowerBound(vars_[i]) + offsets_[i]);

    // Abort if we can't get a better bound.
    if (target_min <= current_min) return true;
  }
  if (target_min == kMaxIntegerValue) {
    // All false, conflit.
    *(trail_->MutableConflict()) = selectors_;
    return false;
  }

  literal_reason_.clear();
  integer_reason_.clear();
  for (int i = 0; i < vars_.size(); ++i) {
    if (trail_->Assignment().LiteralIsFalse(selectors_[i])) {
      literal_reason_.push_back(selectors_[i]);
    } else {
      integer_reason_.push_back(
          IntegerLiteral::GreaterOrEqual(vars_[i], target_min - offsets_[i]));
    }
  }
  return integer_trail_->Enqueue(
      IntegerLiteral::GreaterOrEqual(target_var_, target_min), literal_reason_,
      integer_reason_);
}

void GreaterThanAtLeastOneOfPropagator::RegisterWith(
    GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (const Literal l : selectors_) watcher->WatchLiteral(l.Negated(), id);
  for (const IntegerVariable v : vars_) watcher->WatchLowerBound(v, id);
}

}  // namespace sat
}  // namespace operations_research
