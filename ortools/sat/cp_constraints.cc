// Copyright 2010-2024 Google LLC
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
#include <vector>

#include "absl/types/span.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/strong_integers.h"

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
    IntegerVariable target_var, const absl::Span<const AffineExpression> exprs,
    const absl::Span<const Literal> selectors,
    const absl::Span<const Literal> enforcements, Model* model)
    : target_var_(target_var),
      enforcements_(enforcements.begin(), enforcements.end()),
      selectors_(selectors.begin(), selectors.end()),
      exprs_(exprs.begin(), exprs.end()),
      trail_(model->GetOrCreate<Trail>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()) {}

void GreaterThanAtLeastOneOfPropagator::Explain(
    int id, IntegerValue propagation_slack, IntegerVariable /*var_to_explain*/,
    int /*trail_index*/, std::vector<Literal>* literals_reason,
    std::vector<int>* trail_indices_reason) {
  literals_reason->clear();
  trail_indices_reason->clear();

  const int first_non_false = id;
  const IntegerValue target_min = propagation_slack;

  for (const Literal l : enforcements_) {
    literals_reason->push_back(l.Negated());
  }
  for (int i = 0; i < first_non_false; ++i) {
    // If the level zero bounds is good enough, no reason needed.
    //
    // TODO(user): We could also skip this if we already have the reason for
    // the expression being high enough in the current conflict.
    if (integer_trail_->LevelZeroLowerBound(exprs_[i]) >= target_min) {
      continue;
    }

    literals_reason->push_back(selectors_[i]);
  }
  integer_trail_->AddAllGreaterThanConstantReason(
      absl::MakeSpan(exprs_).subspan(first_non_false), target_min,
      trail_indices_reason);
}

bool GreaterThanAtLeastOneOfPropagator::Propagate() {
  // TODO(user): In case of a conflict, we could push one of them to false if
  // it is the only one.
  for (const Literal l : enforcements_) {
    if (!trail_->Assignment().LiteralIsTrue(l)) return true;
  }

  // Compute the min of the lower-bound for the still possible variables.
  // TODO(user): This could be optimized by keeping more info from the last
  // Propagate() calls.
  IntegerValue target_min = kMaxIntegerValue;
  const IntegerValue current_min = integer_trail_->LowerBound(target_var_);
  const AssignmentView assignment(trail_->Assignment());

  int first_non_false = 0;
  const int size = exprs_.size();
  for (int i = 0; i < size; ++i) {
    if (assignment.LiteralIsTrue(selectors_[i])) return true;

    // The permutation is needed to have proper lazy reason.
    if (assignment.LiteralIsFalse(selectors_[i])) {
      if (i != first_non_false) {
        std::swap(selectors_[i], selectors_[first_non_false]);
        std::swap(exprs_[i], exprs_[first_non_false]);
      }
      ++first_non_false;
      continue;
    }

    const IntegerValue min = integer_trail_->LowerBound(exprs_[i]);
    if (min < target_min) {
      target_min = min;

      // Abort if we can't get a better bound.
      if (target_min <= current_min) return true;
    }
  }

  if (target_min == kMaxIntegerValue) {
    // All false, conflit.
    *(trail_->MutableConflict()) = selectors_;
    return false;
  }

  // Note that we use id/propagation_slack for other purpose.
  return integer_trail_->EnqueueWithLazyReason(
      IntegerLiteral::GreaterOrEqual(target_var_, target_min),
      /*id=*/first_non_false, /*propagation_slack=*/target_min, this);
}

void GreaterThanAtLeastOneOfPropagator::RegisterWith(
    GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (const Literal l : selectors_) watcher->WatchLiteral(l.Negated(), id);
  for (const Literal l : enforcements_) watcher->WatchLiteral(l, id);
  for (const AffineExpression e : exprs_) {
    if (!e.IsConstant()) {
      watcher->WatchLowerBound(e, id);
    }
  }
}

}  // namespace sat
}  // namespace operations_research
