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

IntegerSum::IntegerSum(const std::vector<IntegerVariable>& vars,
                       const std::vector<int>& coeffs, IntegerVariable sum,
                       IntegerTrail* integer_trail)
    : vars_(vars), coeffs_(coeffs), sum_(sum), integer_trail_(integer_trail) {
  // Handle negative coefficients.
  for (int i = 0; i < vars.size(); ++i) {
    if (coeffs_[i] < 0) {
      vars_[i] = NegationOf(vars_[i]);
      coeffs_[i] = -coeffs_[i];
    }
  }
}

bool IntegerSum::Propagate(Trail* trail) {
  if (vars_.empty()) return true;

  IntegerValue new_lb = integer_trail_->LowerBound(vars_[0]) * coeffs_[0];
  for (int i = 1; i < vars_.size(); ++i) {
    new_lb += integer_trail_->LowerBound(vars_[i]) * coeffs_[i];
  }

  // Update the sum lower-bound.
  if (new_lb > integer_trail_->LowerBound(sum_)) {
    integer_reason_.clear();
    for (const IntegerVariable& var : vars_) {
      integer_reason_.push_back(integer_trail_->LowerBoundAsLiteral(var));
    }
    if (!integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(sum_, new_lb),
                                 literal_reason_, integer_reason_)) {
      return false;
    }
  }

  // Update the variables upper-bound.
  const IntegerValue sum_upper_bound = integer_trail_->UpperBound(sum_);
  for (int i = 0; i < vars_.size(); ++i) {
    const IntegerValue new_term_ub =
        sum_upper_bound - new_lb +
        integer_trail_->LowerBound(vars_[i]) * coeffs_[i];
    const IntegerValue new_ub = new_term_ub / coeffs_[i];
    if (new_ub < integer_trail_->UpperBound(vars_[i])) {
      integer_reason_.clear();
      for (int j = 0; j < vars_.size(); ++j) {
        if (i == j) continue;
        integer_reason_.push_back(
            integer_trail_->LowerBoundAsLiteral(vars_[j]));
      }
      integer_reason_.push_back(integer_trail_->UpperBoundAsLiteral(sum_));
      if (!integer_trail_->Enqueue(
              IntegerLiteral::LowerOrEqual(vars_[i], new_ub), literal_reason_,
              integer_reason_)) {
        return false;
      }
    }
  }

  return true;
}

void IntegerSum::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (const IntegerVariable& var : vars_) {
    watcher->WatchIntegerVariable(var, id);
  }
  watcher->WatchIntegerVariable(sum_, id);
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
        std::vector<Literal>* literal_reason;
        std::vector<IntegerLiteral>* integer_reason;
        if (current_min > values_[i]) {
          integer_trail_->EnqueueLiteral(
              selectors_[i].Negated(), &literal_reason, &integer_reason, trail);
          integer_reason->push_back(integer_trail_->LowerBoundAsLiteral(var_));
        } else if (current_max < values_[i]) {
          integer_trail_->EnqueueLiteral(
              selectors_[i].Negated(), &literal_reason, &integer_reason, trail);
          integer_reason->push_back(integer_trail_->UpperBoundAsLiteral(var_));
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

}  // namespace sat
}  // namespace operations_research
