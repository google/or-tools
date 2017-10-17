// Copyright 2010-2017 Google
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

#include "ortools/sat/integer_expr.h"

#include <algorithm>
#include <memory>
#include <unordered_map>

#include "ortools/base/stl_util.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

IntegerSumLE::IntegerSumLE(LiteralIndex reified_literal,
                           const std::vector<IntegerVariable>& vars,
                           const std::vector<IntegerValue>& coeffs,
                           IntegerValue upper, Model* model)
    : reified_literal_(reified_literal),
      upper_bound_(upper),
      trail_(model->GetOrCreate<Trail>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      rev_integer_value_repository_(
          model->GetOrCreate<RevIntegerValueRepository>()),
      vars_(vars),
      coeffs_(coeffs) {
  // TODO(user): deal with this corner case.
  CHECK(!vars_.empty());

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

  index_in_integer_reason_.resize(vars_.size());

  // Initialize the reversible numbers.
  rev_num_fixed_vars_ = 0;
  rev_lb_fixed_vars_ = IntegerValue(0);
}

void IntegerSumLE::FillIntegerReason() {
  integer_reason_.clear();
  for (int i = 0; i < vars_.size(); ++i) {
    const IntegerVariable var = vars_[i];
    if (integer_trail_->VariableLowerBoundIsFromLevelZero(var)) {
      index_in_integer_reason_[i] = -1;
    } else {
      index_in_integer_reason_[i] = integer_reason_.size();
      integer_reason_.push_back(integer_trail_->LowerBoundAsLiteral(var));
    }
  }
}

bool IntegerSumLE::Propagate() {
  // Reified case: If the reified literal is false, we ignore the constraint.
  if (reified_literal_ != kNoLiteralIndex &&
      trail_->Assignment().LiteralIsFalse(Literal(reified_literal_))) {
    return true;
  }

  // Save the current number of fixed variables.
  rev_integer_value_repository_->SaveState(&rev_lb_fixed_vars_);

  // Compute the new lower bound and update the reversible structures.
  IntegerValue lb_unfixed_vars = IntegerValue(0);
  for (int i = rev_num_fixed_vars_; i < vars_.size(); ++i) {
    const IntegerVariable var = vars_[i];
    const IntegerValue coeff = coeffs_[i];
    const IntegerValue lb = integer_trail_->LowerBound(var);
    if (lb != integer_trail_->UpperBound(var)) {
      lb_unfixed_vars += lb * coeff;
    } else {
      // Update the set of fixed variables.
      std::swap(vars_[i], vars_[rev_num_fixed_vars_]);
      std::swap(coeffs_[i], coeffs_[rev_num_fixed_vars_]);
      rev_num_fixed_vars_++;
      rev_lb_fixed_vars_ += lb * coeff;
    }
  }

  const IntegerValue new_lb = rev_lb_fixed_vars_ + lb_unfixed_vars;

  // Conflict?
  IntegerValue slack = upper_bound_ - new_lb;
  if (slack < 0) {
    // Like FillIntegerReason() but try to relax the reason a bit.
    //
    // TODO(user): if not all the slack is consumed, we could relax it even
    // more. It might also be advantageous to relax first the variable with the
    // highest "trail index".
    integer_reason_.clear();
    for (int i = 0; i < vars_.size(); ++i) {
      const IntegerVariable var = vars_[i];
      const IntegerValue lb = integer_trail_->LowerBound(var);
      const IntegerValue prev_lb = integer_trail_->PreviousLowerBound(var);
      if (lb == prev_lb) continue;  // level zero.
      const IntegerValue diff = (lb - prev_lb) * coeffs_[i];
      if (slack + diff < 0) {
        integer_reason_.push_back(IntegerLiteral::GreaterOrEqual(var, prev_lb));
        slack += diff;
      } else {
        integer_reason_.push_back(IntegerLiteral::GreaterOrEqual(var, lb));
      }
    }

    // Reified case: If the reified literal is unassigned, we set it to false,
    // otherwise we have a conflict.
    if (reified_literal_ != kNoLiteralIndex &&
        !trail_->Assignment().LiteralIsTrue(Literal(reified_literal_))) {
      integer_trail_->EnqueueLiteral(Literal(reified_literal_).Negated(), {},
                                     integer_reason_);
      return true;
    }
    return integer_trail_->ReportConflict(literal_reason_, integer_reason_);
  }

  // Reified case: We can only propagate the actual constraint if the reified
  // literal is true.
  if (reified_literal_ != kNoLiteralIndex &&
      !trail_->Assignment().LiteralIsTrue(Literal(reified_literal_))) {
    return true;
  }

  // The integer_reason_ will only be filled on the first push.
  bool first_push = true;

  // The lower bound of all the variables minus one can be used to update the
  // upper bound of the last one.
  int trail_index_with_same_reason = -1;
  for (int i = rev_num_fixed_vars_; i < vars_.size(); ++i) {
    const IntegerVariable var = vars_[i];
    const IntegerValue coeff = coeffs_[i];
    const IntegerValue var_slack =
        integer_trail_->UpperBound(var) - integer_trail_->LowerBound(var);
    if (var_slack * coeff > slack) {
      if (first_push) {
        first_push = false;
        FillIntegerReason();
      }

      // We need to remove the entry index from the reason temporarily.
      IntegerLiteral saved;
      const int index = index_in_integer_reason_[i];
      if (index >= 0) {
        saved = integer_reason_[index];
        integer_reason_[index] = integer_reason_.back();
        integer_reason_.pop_back();
      } else if (trail_index_with_same_reason == -1) {
        // All the push for which index < 0 share the same reason, so we save
        // the index of the first push so that we do not need to copy the reason
        // of the next ones.
        trail_index_with_same_reason = integer_trail_->Index();
      }

      const IntegerValue new_ub =
          integer_trail_->LowerBound(var) + slack / coeff;
      if (!integer_trail_->Enqueue(IntegerLiteral::LowerOrEqual(var, new_ub),
                                   literal_reason_, integer_reason_,
                                   index >= 0 ? integer_trail_->Index()
                                              : trail_index_with_same_reason)) {
        return false;
      }

      // Restore integer_reason_. Note that this is not needed if we returned
      // false above.
      if (index >= 0) {
        integer_reason_.push_back(saved);
        std::swap(integer_reason_[index], integer_reason_.back());
      }
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
  watcher->RegisterReversibleInt(id, &rev_num_fixed_vars_);
}

MinPropagator::MinPropagator(const std::vector<IntegerVariable>& vars,
                             IntegerVariable min_var,
                             IntegerTrail* integer_trail)
    : vars_(vars), min_var_(min_var), integer_trail_(integer_trail) {}

bool MinPropagator::Propagate() {
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
    integer_reason_.clear();
    for (const IntegerVariable var : vars_) {
      integer_reason_.push_back(IntegerLiteral::GreaterOrEqual(var, min));
    }
    if (!integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(min_var_, min),
                                 {}, integer_reason_)) {
      return false;
    }
  }

  // Propagation b)
  if (num_intervals_that_can_be_min == 1) {
    const IntegerValue ub_of_only_candidate =
        integer_trail_->UpperBound(vars_[last_possible_min_interval]);
    if (current_min_ub < ub_of_only_candidate) {
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
              {}, integer_reason_)) {
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
    return integer_trail_->ReportConflict(integer_reason_);
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

PositiveProductPropagator::PositiveProductPropagator(
    IntegerVariable a, IntegerVariable b, IntegerVariable p,
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

bool PositiveProductPropagator::Propagate() {
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

void PositiveProductPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->WatchIntegerVariable(a_, id);
  watcher->WatchIntegerVariable(b_, id);
  watcher->WatchIntegerVariable(p_, id);
}

SquarePropagator::SquarePropagator(IntegerVariable x, IntegerVariable s,
                                   IntegerTrail* integer_trail)
    : x_(x), s_(s), integer_trail_(integer_trail) {}

bool SquarePropagator::Propagate() {
  bool may_propagate = true;
  while (may_propagate) {
    may_propagate = false;
    IntegerValue min_x = integer_trail_->LowerBound(x_);
    IntegerValue max_x = integer_trail_->UpperBound(x_);
    IntegerValue min_s = integer_trail_->LowerBound(s_);
    IntegerValue max_s = integer_trail_->UpperBound(s_);

    // TODO(user): support this case.
    CHECK_GE(min_x, 0);

    // Propagation from x to s: s in [min_x*min_x, max_x*max_x].
    if (min_x * min_x > min_s) {
      may_propagate = true;
      min_s = min_x * min_x;
      if (!integer_trail_->Enqueue(
              IntegerLiteral::GreaterOrEqual(s_, min_s), {},
              {IntegerLiteral::GreaterOrEqual(x_, min_x)})) {
        return false;
      }
    }
    if (max_x * max_x < max_s) {
      may_propagate = true;
      max_s = max_x * max_x;
      if (!integer_trail_->Enqueue(IntegerLiteral::LowerOrEqual(s_, max_s), {},
                                   {IntegerLiteral::LowerOrEqual(x_, max_x)})) {
        return false;
      }
    }

    // Propagation from s to x: x in [ceil(sqrt(min_s)), floor(sqrt(max_s))].
    if (max_x * max_x > max_s) {
      may_propagate = true;
      // TODO(user): O(log(max_x)) version or someone will be unhappy.
      while (max_x * max_x > max_s) max_x--;
      if (!integer_trail_->Enqueue(IntegerLiteral::LowerOrEqual(x_, max_x), {},
                                   {IntegerLiteral::LowerOrEqual(
                                       s_, (max_x + 1) * (max_x + 1) - 1)})) {
        return false;
      }
    }
    if (min_x * min_x < min_s) {
      may_propagate = true;
      // TODO(user): O(log(min_x)) version or someone will be unhappy.
      while (min_x * min_x < min_s) min_x++;
      if (!integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(x_, min_x),
                                   {},
                                   {IntegerLiteral::GreaterOrEqual(
                                       s_, (min_x - 1) * (min_x - 1) + 1)})) {
        return false;
      }
    }
  }

  return true;
}

void SquarePropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->WatchIntegerVariable(x_, id);
  watcher->WatchIntegerVariable(s_, id);
}

DivisionPropagator::DivisionPropagator(IntegerVariable a, IntegerVariable b,
                                       IntegerVariable c,
                                       IntegerTrail* integer_trail)
    : a_(a), b_(b), c_(c), integer_trail_(integer_trail) {}

bool DivisionPropagator::Propagate() {
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

std::function<void(Model*)> IsOneOf(IntegerVariable var,
                                    const std::vector<Literal>& selectors,
                                    const std::vector<IntegerValue>& values) {
  return [=](Model* model) {
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();

    CHECK(!values.empty());
    CHECK_EQ(values.size(), selectors.size());
    std::vector<int64> unique_values;
    std::unordered_map<int64, std::vector<Literal>> value_to_selector;
    for (int i = 0; i < values.size(); ++i) {
      unique_values.push_back(values[i].value());
      value_to_selector[values[i].value()].push_back(selectors[i]);
    }
    STLSortAndRemoveDuplicates(&unique_values);

    integer_trail->UpdateInitialDomain(
        var, SortedDisjointIntervalsFromValues(unique_values));
    if (unique_values.size() == 1) {
      model->Add(ClauseConstraint(selectors));
      return;
    }

    // Note that it is more efficient to call AssociateToIntegerEqualValue()
    // with the values ordered, like we do here.
    for (const int64 v : unique_values) {
      const std::vector<Literal>& selectors = value_to_selector[v];
      if (selectors.size() == 1) {
        encoder->AssociateToIntegerEqualValue(selectors[0], var,
                                              IntegerValue(v));
      } else {
        const Literal l(model->Add(NewBooleanVariable()), true);
        model->Add(ReifiedBoolOr(selectors, l));
        encoder->AssociateToIntegerEqualValue(l, var, IntegerValue(v));
      }
    }
  };
}

}  // namespace sat
}  // namespace operations_research
