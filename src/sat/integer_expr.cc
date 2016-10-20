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

#include "sat/integer_expr.h"

namespace operations_research {
namespace sat {

IntegerSumLE::IntegerSumLE(LiteralIndex reified_literal,
                           const std::vector<IntegerVariable>& vars,
                           const std::vector<IntegerValue>& coeffs,
                           IntegerValue upper, IntegerTrail* integer_trail)
    : reified_literal_(reified_literal),
      upper_bound_(upper),
      vars_(vars),
      coeffs_(coeffs),
      integer_trail_(integer_trail) {
  // Handle negative coefficients.
  for (int i = 0; i < vars.size(); ++i) {
    if (coeffs_[i] < 0) {
      vars_[i] = NegationOf(vars_[i]);
      coeffs_[i] = -coeffs_[i];
    }
  }

  // Literal reason will either alway contains the negation of reified_literal
  // or be always empty.
  if (reified_literal_ != kNoLiteralIndex) {
    literal_reason_.push_back(Literal(reified_literal_).Negated());
  }
}

void IntegerSumLE::FillIntegerReason() {
  integer_reason_.clear();
  for (const IntegerVariable& var : vars_) {
    integer_reason_.push_back(integer_trail_->LowerBoundAsLiteral(var));
  }
}

bool IntegerSumLE::Propagate(Trail* trail) {
  CHECK(!vars_.empty());  // TODO(user): deal with this corner case.

  // Reified case: If the reified literal is false, we ignore the constraint.
  if (reified_literal_ != kNoLiteralIndex &&
      trail->Assignment().LiteralIsFalse(Literal(reified_literal_))) {
    return true;
  }

  IntegerValue new_lb(0);
  for (int i = 0; i < vars_.size(); ++i) {
    new_lb += integer_trail_->LowerBound(vars_[i]) * coeffs_[i];
  }

  // Conflict?
  if (new_lb > upper_bound_) {
    FillIntegerReason();

    // Reified case: If the reified literal is unassigned, we set it to false,
    // otherwise we have a conflict.
    if (reified_literal_ != kNoLiteralIndex &&
        !trail->Assignment().LiteralIsTrue(Literal(reified_literal_))) {
      integer_trail_->EnqueueLiteral(Literal(reified_literal_).Negated(), {},
                                     integer_reason_);
      return true;
    }

    // Conflict.
    std::vector<Literal>* conflict = trail->MutableConflict();
    *conflict = literal_reason_;
    integer_trail_->MergeReasonInto(integer_reason_, conflict);
    return false;
  }

  // Reified case: We can only propagate the actual constraint if the reified
  // literal is true.
  if (reified_literal_ != kNoLiteralIndex &&
      !trail->Assignment().LiteralIsTrue(Literal(reified_literal_))) {
    return true;
  }

  // Will only be filled on the first push.
  integer_reason_.clear();

  // The lower bound of all the variables minus one can be used to update the
  // upper bound of the last one.
  for (int i = 0; i < vars_.size(); ++i) {
    const IntegerValue new_lb_excluding_i =
        new_lb - integer_trail_->LowerBound(vars_[i]) * coeffs_[i];
    const IntegerValue new_term_ub = upper_bound_ - new_lb_excluding_i;
    const IntegerValue new_ub = new_term_ub / coeffs_[i];
    if (new_ub < integer_trail_->UpperBound(vars_[i])) {
      if (integer_reason_.empty()) FillIntegerReason();

      // We need to remove the entry i temporarily.
      const IntegerLiteral saved = integer_reason_[i];
      std::swap(integer_reason_[i], integer_reason_.back());
      integer_reason_.pop_back();

      if (!integer_trail_->Enqueue(
              IntegerLiteral::LowerOrEqual(vars_[i], new_ub), literal_reason_,
              integer_reason_)) {
        return false;
      }

      // Restore integer_reason_. Note that this is not needed if we returned
      // false above.
      integer_reason_.push_back(saved);
      std::swap(integer_reason_[i], integer_reason_.back());
    }
  }

  return true;
}

void IntegerSumLE::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (const IntegerVariable& var : vars_) {
    watcher->WatchLowerBound(var, id);
  }
  if (reified_literal_ != kNoLiteralIndex) {
    // We only watch the true direction.
    watcher->WatchLiteral(Literal(reified_literal_), id);
  }
}

MinPropagator::MinPropagator(const std::vector<IntegerVariable>& vars,
                             IntegerVariable min_var,
                             IntegerTrail* integer_trail)
    : vars_(vars), min_var_(min_var), integer_trail_(integer_trail) {}

bool MinPropagator::Propagate(Trail* trail) {
  if (vars_.empty()) return true;

  // Count the number of interval that are possible candidate for the min.
  // Only the intervals for which lb > current_min_ub cannot.
  const IntegerLiteral min_ub_literal =
      integer_trail_->UpperBoundAsLiteral(min_var_);
  const IntegerValue current_min_ub = integer_trail_->UpperBound(min_var_);
  int num_intervals_that_can_be_min = 0;
  int last_possible_min_interval = 0;

  IntegerValue min = kMaxIntegerValue;
  for (int i = 0; i < vars_.size(); ++i) {
    const IntegerValue lb = integer_trail_->LowerBound(vars_[i]);
    min = std::min(min, lb);
    if (lb <= current_min_ub) {
      ++num_intervals_that_can_be_min;
      last_possible_min_interval = i;
    }
  }

  // Propagation a)
  if (min > integer_trail_->LowerBound(min_var_)) {
    literal_reason_.clear();
    integer_reason_.clear();
    for (const IntegerVariable var : vars_) {
      integer_reason_.push_back(IntegerLiteral::GreaterOrEqual(var, min));
    }
    if (!integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(min_var_, min),
                                 literal_reason_, integer_reason_)) {
      return false;
    }
  }

  // Propagation b)
  if (num_intervals_that_can_be_min == 1) {
    const IntegerValue ub_of_only_candidate =
        integer_trail_->UpperBound(vars_[last_possible_min_interval]);
    if (current_min_ub < ub_of_only_candidate) {
      literal_reason_.clear();
      integer_reason_.clear();

      // The reason is that all the other interval start after current_min_ub.
      // And that min_ub has its current value.
      integer_reason_.push_back(min_ub_literal);
      for (const IntegerVariable var : vars_) {
        if (var == vars_[last_possible_min_interval]) continue;
        integer_reason_.push_back(
            IntegerLiteral::GreaterOrEqual(var, current_min_ub + 1));
      }
      if (!integer_trail_->Enqueue(
              IntegerLiteral::LowerOrEqual(vars_[last_possible_min_interval],
                                           current_min_ub),
              literal_reason_, integer_reason_)) {
        return false;
      }
    }
  }

  // Conflict.
  //
  // TODO(user): Not sure this code is useful since this will be detected
  // by the fact that the [lb, ub] of the min is empty. It depends on the
  // propagation order though, but probably the precedences propagator would
  // propagate before this one. So change this to a CHECK?
  if (num_intervals_that_can_be_min == 0) {
    integer_reason_.clear();

    // Almost the same as propagation b).
    integer_reason_.push_back(min_ub_literal);
    for (const IntegerVariable var : vars_) {
      integer_reason_.push_back(
          IntegerLiteral::GreaterOrEqual(var, current_min_ub + 1));
    }

    std::vector<Literal>* conflict = trail->MutableConflict();
    integer_trail_->MergeReasonInto(integer_reason_, conflict);
    return false;
  }

  return true;
}

void MinPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (const IntegerVariable& var : vars_) {
    watcher->WatchLowerBound(var, id);
  }
  watcher->WatchUpperBound(min_var_, id);
}

IsOneOfPropagator::IsOneOfPropagator(IntegerVariable var,
                                     const std::vector<Literal>& selectors,
                                     const std::vector<IntegerValue>& values,
                                     IntegerTrail* integer_trail)
    : var_(var),
      selectors_(selectors),
      values_(values),
      integer_trail_(integer_trail) {}

bool IsOneOfPropagator::Propagate(Trail* trail) {
  const IntegerValue current_min = integer_trail_->LowerBound(var_);
  const IntegerValue current_max = integer_trail_->UpperBound(var_);
  IntegerValue min = kMaxIntegerValue;
  IntegerValue max = kMinIntegerValue;
  literal_reason_.clear();
  for (int i = 0; i < selectors_.size(); ++i) {
    if (trail->Assignment().LiteralIsFalse(selectors_[i])) {
      literal_reason_.push_back(selectors_[i]);
    } else {
      min = std::min(min, values_[i]);
      max = std::max(max, values_[i]);

      if (!trail->Assignment().LiteralIsTrue(selectors_[i])) {
        // Propagate selector to false?
        if (current_min > values_[i]) {
          integer_trail_->EnqueueLiteral(
              selectors_[i].Negated(), {},
              {integer_trail_->LowerBoundAsLiteral(var_)});
        } else if (current_max < values_[i]) {
          integer_trail_->EnqueueLiteral(
              selectors_[i].Negated(), {},
              {integer_trail_->UpperBoundAsLiteral(var_)});
        }
      }
    }
  }

  // Propagate new min/max.
  if (min > current_min) {
    if (!integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(var_, min),
                                 literal_reason_, integer_reason_)) {
      return false;
    }
  }
  if (max < current_max) {
    if (!integer_trail_->Enqueue(IntegerLiteral::LowerOrEqual(var_, max),
                                 literal_reason_, integer_reason_)) {
      return false;
    }
  }

  return true;
}

void IsOneOfPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (const Literal& lit : selectors_) {
    watcher->WatchLiteral(lit.Negated(), id);
  }
  watcher->WatchIntegerVariable(var_, id);
}

ProductPropagator::ProductPropagator(IntegerVariable a, IntegerVariable b,
                                     IntegerVariable p,
                                     IntegerTrail* integer_trail)
    : a_(a), b_(b), p_(p), integer_trail_(integer_trail) {}

namespace {

// The maximum value of x such that x * b <= p with b > 0 and p >= 0;
IntegerValue MaxValue(IntegerValue b, IntegerValue p) {
  CHECK_GT(b, 0);
  CHECK_GE(p, 0);
  return p / b;
}

// The minimum value of x such that x * b >= p with b > 0 and p >= 0;
IntegerValue MinValue(IntegerValue b, IntegerValue p) {
  CHECK_GT(b, 0);
  CHECK_GE(p, 0);
  return (p + b - 1) / b;
}

}  // namespace

bool ProductPropagator::Propagate(Trail* trail) {
  // Copy because we will swap them.
  IntegerVariable a = a_;
  IntegerVariable b = b_;
  IntegerValue min_a = integer_trail_->LowerBound(a);
  IntegerValue max_a = integer_trail_->UpperBound(a);
  IntegerValue min_b = integer_trail_->LowerBound(b);
  IntegerValue max_b = integer_trail_->UpperBound(b);
  IntegerValue min_p = integer_trail_->LowerBound(p_);
  IntegerValue max_p = integer_trail_->UpperBound(p_);

  // TODO(user): support these cases.
  CHECK_GE(min_a, 0);
  CHECK_GE(min_b, 0);

  const IntegerValue zero(0);  // For convenience.
  bool may_propagate = true;
  while (may_propagate) {
    may_propagate = false;
    if (max_a * max_b < max_p) {
      may_propagate = true;
      max_p = max_a * max_b;
      if (!integer_trail_->Enqueue(IntegerLiteral::LowerOrEqual(p_, max_p), {},
                                   {integer_trail_->UpperBoundAsLiteral(a),
                                    integer_trail_->UpperBoundAsLiteral(b),
                                    IntegerLiteral::GreaterOrEqual(a, zero),
                                    IntegerLiteral::GreaterOrEqual(b, zero)})) {
        return false;
      }
    }
    if (min_a * min_b > min_p) {
      may_propagate = true;
      min_p = min_a * min_b;
      if (!integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(p_, min_p),
                                   {},
                                   {integer_trail_->LowerBoundAsLiteral(a),
                                    integer_trail_->LowerBoundAsLiteral(b)})) {
        return false;
      }
    }

    // This helps to check the validity of the test below.
    CHECK_GE(min_p, 0);
    CHECK_GE(max_p, min_p);

    for (int i = 0; i < 2; ++i) {
      if (max_a * min_b > max_p) {
        may_propagate = true;
        max_a = MaxValue(min_b, max_p);
        if (!integer_trail_->Enqueue(
                IntegerLiteral::LowerOrEqual(a, max_a), {},
                {integer_trail_->LowerBoundAsLiteral(b),
                 integer_trail_->UpperBoundAsLiteral(p_)})) {
          return false;
        }
      } else if (max_a * min_b < min_p) {
        may_propagate = true;
        min_b = MinValue(max_a, min_p);
        if (!integer_trail_->Enqueue(
                IntegerLiteral::GreaterOrEqual(b, min_b), {},
                {integer_trail_->UpperBoundAsLiteral(a),
                 IntegerLiteral::GreaterOrEqual(b, zero),
                 integer_trail_->LowerBoundAsLiteral(p_)})) {
          return false;
        }
      }

      // Same thing with a and b swapped.
      std::swap(a, b);
      std::swap(min_a, min_b);
      std::swap(max_a, max_b);
    }
  }
  return true;
}

void ProductPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->WatchIntegerVariable(a_, id);
  watcher->WatchIntegerVariable(b_, id);
  watcher->WatchIntegerVariable(p_, id);
}

DivisionPropagator::DivisionPropagator(IntegerVariable a, IntegerVariable b,
                                       IntegerVariable c,
                                       IntegerTrail* integer_trail)
    : a_(a), b_(b), c_(c), integer_trail_(integer_trail) {}

bool DivisionPropagator::Propagate(Trail* trail) {
  const IntegerValue min_a = integer_trail_->LowerBound(a_);
  const IntegerValue max_a = integer_trail_->UpperBound(a_);
  const IntegerValue min_b = integer_trail_->LowerBound(b_);
  const IntegerValue max_b = integer_trail_->UpperBound(b_);
  IntegerValue min_c = integer_trail_->LowerBound(c_);
  IntegerValue max_c = integer_trail_->UpperBound(c_);

  // TODO(user): support these cases.
  CHECK_GE(min_a, 0);
  CHECK_GT(min_b, 0);  // b can never be zero.

  bool may_propagate = true;
  while (may_propagate) {
    may_propagate = false;
    if (max_a / min_b < max_c) {
      may_propagate = true;
      max_c = max_a / min_b;
      if (!integer_trail_->Enqueue(IntegerLiteral::LowerOrEqual(c_, max_c), {},
                                   {integer_trail_->UpperBoundAsLiteral(a_),
                                    integer_trail_->LowerBoundAsLiteral(b_)})) {
        return false;
      }
    }
    if (min_a / max_b > min_c) {
      may_propagate = true;
      min_c = min_a / max_b;
      if (!integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(c_, min_c),
                                   {},
                                   {integer_trail_->LowerBoundAsLiteral(a_),
                                    integer_trail_->UpperBoundAsLiteral(b_)})) {
        return false;
      }
    }

    // TODO(user): propagate the bounds on a and b from the ones of c.
    // Note however that what we did is enough to enforce the constraint when
    // all the values are fixed.
  }
  return true;
}

void DivisionPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->WatchIntegerVariable(a_, id);
  watcher->WatchIntegerVariable(b_, id);
  watcher->WatchIntegerVariable(c_, id);
}

}  // namespace sat
}  // namespace operations_research
