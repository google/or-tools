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

#include "ortools/sat/cp_constraints.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/sat/enforcement.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

BooleanXorPropagator::BooleanXorPropagator(
    absl::Span<const Literal> enforcement_literals,
    const std::vector<Literal>& literals, bool value, Model* model)
    : literals_(literals),
      value_(value),
      trail_(*model->GetOrCreate<Trail>()),
      integer_trail_(*model->GetOrCreate<IntegerTrail>()),
      enforcement_helper_(*model->GetOrCreate<EnforcementHelper>()) {
  GenericLiteralWatcher* watcher = model->GetOrCreate<GenericLiteralWatcher>();
  enforcement_id_ = enforcement_helper_.Register(enforcement_literals, watcher,
                                                 RegisterWith(watcher));
}

bool BooleanXorPropagator::Propagate() {
  const EnforcementStatus status = enforcement_helper_.Status(enforcement_id_);
  if (status == EnforcementStatus::IS_FALSE ||
      status == EnforcementStatus::CANNOT_PROPAGATE) {
    return true;
  }

  bool sum = false;
  int unassigned_index = -1;
  for (int i = 0; i < literals_.size(); ++i) {
    const Literal l = literals_[i];
    if (trail_.Assignment().LiteralIsFalse(l)) {
      sum ^= false;
    } else if (trail_.Assignment().LiteralIsTrue(l)) {
      sum ^= true;
    } else {
      // If we have more than one unassigned literal, we can't deduce anything.
      if (unassigned_index != -1) return true;
      unassigned_index = i;
    }
  }

  auto fill_literal_reason = [&]() {
    literal_reason_.clear();
    for (int i = 0; i < literals_.size(); ++i) {
      if (i == unassigned_index) continue;
      const Literal l = literals_[i];
      literal_reason_.push_back(
          trail_.Assignment().LiteralIsFalse(l) ? l : l.Negated());
    }
  };

  // Propagates?
  if (status == EnforcementStatus::IS_ENFORCED && unassigned_index != -1) {
    fill_literal_reason();
    const Literal u = literals_[unassigned_index];
    return enforcement_helper_.EnqueueLiteral(
        enforcement_id_, sum == value_ ? u.Negated() : u, literal_reason_,
        /*integer_reason=*/{});
  }

  // Ok.
  if (sum == value_) return true;

  // Conflict.
  if (status == EnforcementStatus::IS_ENFORCED) {
    fill_literal_reason();
    return enforcement_helper_.ReportConflict(enforcement_id_, literal_reason_,
                                              /*integer_reason=*/{});
  } else if (unassigned_index == -1) {
    fill_literal_reason();
    return enforcement_helper_.PropagateWhenFalse(
        enforcement_id_, literal_reason_, /*integer_reason=*/{});
  } else {
    return true;
  }
}

int BooleanXorPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (const Literal& l : literals_) {
    watcher->WatchLiteral(l, id);
    watcher->WatchLiteral(l.Negated(), id);
  }
  return id;
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

void GreaterThanAtLeastOneOfPropagator::Explain(int id,
                                                IntegerLiteral to_explain,
                                                IntegerReason* reason) {
  const int first_non_false = id;

  // Note that even if we propagated more, we only need the reason for
  // to_explain.bound, this is easy for that propagator.
  CHECK_EQ(to_explain.var, target_var_);
  const IntegerValue target_min = to_explain.bound;

  reason->boolean_literals_at_true = enforcements_;

  auto& literals_at_false = integer_trail_->ClearedMutableTmpLiterals();
  for (int i = 0; i < first_non_false; ++i) {
    // If the level zero bounds is good enough, no reason needed.
    //
    // TODO(user): We could also skip this if we already have the reason for
    // the expression being high enough in the current conflict.
    if (integer_trail_->LevelZeroLowerBound(exprs_[i]) >= target_min) {
      continue;
    }

    literals_at_false.push_back(selectors_[i]);
  }
  reason->boolean_literals_at_false = literals_at_false;

  // Add all greater than target_min reason.
  auto& integer_literals = integer_trail_->ClearedMutableTmpIntegerLiterals();
  for (const AffineExpression& expr :
       absl::MakeSpan(exprs_).subspan(first_non_false)) {
    if (expr.IsConstant()) {
      DCHECK_GE(expr.constant, target_min);
      continue;
    }
    DCHECK_NE(expr.var, kNoIntegerVariable);
    const IntegerLiteral to_explain = expr.GreaterOrEqual(target_min);
    if (integer_trail_->IsTrueAtLevelZero(to_explain)) continue;
    integer_literals.push_back(to_explain);
  }
  reason->integer_literals = integer_literals;
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
  Literal* const selectors = selectors_.data();
  AffineExpression* const exprs = exprs_.data();
  for (int i = 0; i < size; ++i) {
    if (assignment.LiteralIsTrue(selectors[i])) return true;

    // The permutation is needed to have proper lazy reason.
    if (assignment.LiteralIsFalse(selectors[i])) {
      if (i != first_non_false) {
        std::swap(selectors[i], selectors[first_non_false]);
        std::swap(exprs[i], exprs[first_non_false]);
      }
      ++first_non_false;
      continue;
    }

    const IntegerValue min = integer_trail_->LowerBound(exprs[i]);
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
