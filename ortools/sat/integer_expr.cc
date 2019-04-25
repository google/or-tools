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

#include "ortools/sat/integer_expr.h"

#include <algorithm>
#include <memory>

#include "absl/container/flat_hash_map.h"
#include "ortools/base/stl_util.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

IntegerSumLE::IntegerSumLE(const std::vector<Literal>& enforcement_literals,
                           const std::vector<IntegerVariable>& vars,
                           const std::vector<IntegerValue>& coeffs,
                           IntegerValue upper, Model* model)
    : enforcement_literals_(enforcement_literals),
      upper_bound_(upper),
      trail_(model->GetOrCreate<Trail>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      rev_integer_value_repository_(
          model->GetOrCreate<RevIntegerValueRepository>()),
      vars_(vars),
      coeffs_(coeffs) {
  // TODO(user): deal with this corner case.
  CHECK(!vars_.empty());
  max_variations_.resize(vars_.size());

  // Handle negative coefficients.
  for (int i = 0; i < vars.size(); ++i) {
    if (coeffs_[i] < 0) {
      vars_[i] = NegationOf(vars_[i]);
      coeffs_[i] = -coeffs_[i];
    }
  }

  // Literal reason will only be used with the negation of enforcement_literals.
  for (const Literal literal : enforcement_literals) {
    literal_reason_.push_back(literal.Negated());
  }

  // Initialize the reversible numbers.
  rev_num_fixed_vars_ = 0;
  rev_lb_fixed_vars_ = IntegerValue(0);
}

void IntegerSumLE::FillIntegerReason() {
  integer_reason_.clear();
  reason_coeffs_.clear();
  const int num_vars = vars_.size();
  for (int i = 0; i < num_vars; ++i) {
    const IntegerVariable var = vars_[i];
    if (!integer_trail_->VariableLowerBoundIsFromLevelZero(var)) {
      integer_reason_.push_back(integer_trail_->LowerBoundAsLiteral(var));
      reason_coeffs_.push_back(coeffs_[i]);
    }
  }
}

bool IntegerSumLE::Propagate() {
  // Reified case: If any of the enforcement_literals are false, we ignore the
  // constraint.
  int num_unassigned_enforcement_literal = 0;
  LiteralIndex unique_unnasigned_literal = kNoLiteralIndex;
  for (const Literal literal : enforcement_literals_) {
    if (trail_->Assignment().LiteralIsFalse(literal)) return true;
    if (!trail_->Assignment().LiteralIsTrue(literal)) {
      ++num_unassigned_enforcement_literal;
      unique_unnasigned_literal = literal.Index();
    }
  }

  // Unfortunately, we can't propagate anything if we have more than one
  // unassigned enforcement literal.
  if (num_unassigned_enforcement_literal > 1) return true;

  // Save the current sum of fixed variables.
  if (is_registered_) {
    rev_integer_value_repository_->SaveState(&rev_lb_fixed_vars_);
  }

  // Compute the new lower bound and update the reversible structures.
  IntegerValue lb_unfixed_vars = IntegerValue(0);
  const int num_vars = vars_.size();
  for (int i = rev_num_fixed_vars_; i < num_vars; ++i) {
    const IntegerVariable var = vars_[i];
    const IntegerValue coeff = coeffs_[i];
    const IntegerValue lb = integer_trail_->LowerBound(var);
    const IntegerValue ub = integer_trail_->UpperBound(var);
    if (lb != ub) {
      max_variations_[i] = (ub - lb) * coeff;
      lb_unfixed_vars += lb * coeff;
    } else {
      // Update the set of fixed variables.
      std::swap(vars_[i], vars_[rev_num_fixed_vars_]);
      std::swap(coeffs_[i], coeffs_[rev_num_fixed_vars_]);
      std::swap(max_variations_[i], max_variations_[rev_num_fixed_vars_]);
      rev_num_fixed_vars_++;
      rev_lb_fixed_vars_ += lb * coeff;
    }
  }

  // Conflict?
  const IntegerValue slack =
      upper_bound_ - (rev_lb_fixed_vars_ + lb_unfixed_vars);
  if (slack < 0) {
    FillIntegerReason();
    integer_trail_->RelaxLinearReason(-slack - 1, reason_coeffs_,
                                      &integer_reason_);

    if (num_unassigned_enforcement_literal == 1) {
      // Propagate the only non-true literal to false.
      const Literal to_propagate = Literal(unique_unnasigned_literal).Negated();
      std::vector<Literal> tmp = literal_reason_;
      tmp.erase(std::find(tmp.begin(), tmp.end(), to_propagate));
      integer_trail_->EnqueueLiteral(to_propagate, tmp, integer_reason_);
      return true;
    }
    return integer_trail_->ReportConflict(literal_reason_, integer_reason_);
  }

  // We can only propagate more if all the enforcement literals are true.
  if (num_unassigned_enforcement_literal > 0) return true;

  // The lower bound of all the variables except one can be used to update the
  // upper bound of the last one.
  for (int i = rev_num_fixed_vars_; i < num_vars; ++i) {
    if (max_variations_[i] <= slack) continue;

    const IntegerVariable var = vars_[i];
    const IntegerValue coeff = coeffs_[i];
    const IntegerValue div = slack / coeff;
    const IntegerValue new_ub = integer_trail_->LowerBound(var) + div;
    const IntegerValue propagation_slack = (div + 1) * coeff - slack - 1;
    if (!integer_trail_->Enqueue(
            IntegerLiteral::LowerOrEqual(var, new_ub),
            /*lazy_reason=*/[this, propagation_slack](
                                IntegerLiteral i_lit, int trail_index,
                                std::vector<Literal>* literal_reason,
                                std::vector<int>* trail_indices_reason) {
              *literal_reason = literal_reason_;
              trail_indices_reason->clear();
              reason_coeffs_.clear();
              const int size = vars_.size();
              for (int i = 0; i < size; ++i) {
                const IntegerVariable var = vars_[i];
                if (PositiveVariable(var) == PositiveVariable(i_lit.var)) {
                  continue;
                }
                const int index =
                    integer_trail_->FindTrailIndexOfVarBefore(var, trail_index);
                if (index >= 0) {
                  trail_indices_reason->push_back(index);
                  if (propagation_slack > 0) {
                    reason_coeffs_.push_back(coeffs_[i]);
                  }
                }
              }
              if (propagation_slack > 0) {
                integer_trail_->RelaxLinearReason(
                    propagation_slack, reason_coeffs_, trail_indices_reason);
              }
            })) {
      return false;
    }
  }

  return true;
}

void IntegerSumLE::RegisterWith(GenericLiteralWatcher* watcher) {
  is_registered_ = true;
  const int id = watcher->Register(this);
  for (const IntegerVariable& var : vars_) {
    watcher->WatchLowerBound(var, id);
  }
  for (const Literal literal : enforcement_literals_) {
    // We only watch the true direction.
    //
    // TODO(user): if there is more than one, maybe we should watch more to
    // propagate a "conflict" as soon as only one is unassigned?
    watcher->WatchLiteral(Literal(literal), id);
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

AbsPropagator::AbsPropagator(IntegerVariable var, IntegerVariable abs_var,
                             IntegerTrail* integer_trail)
    : var_(var), abs_var_(abs_var), integer_trail_(integer_trail) {}

#define RETURN_IF_FALSE(x) \
  if (!(x)) return false;
bool AbsPropagator::Propagate() {
  const IntegerValue var_lb = integer_trail_->LowerBound(var_);
  const IntegerValue var_ub = integer_trail_->UpperBound(var_);
  const IntegerValue abs_ub = integer_trail_->UpperBound(abs_var_);

  if (abs_ub < var_ub) {
    integer_reason_.clear();
    integer_reason_.push_back(IntegerLiteral::LowerOrEqual(abs_var_, abs_ub));
    RETURN_IF_FALSE(integer_trail_->Enqueue(
        IntegerLiteral::LowerOrEqual(var_, abs_ub), {}, integer_reason_));
  }
  if (-abs_ub > var_lb) {
    integer_reason_.clear();
    integer_reason_.push_back(IntegerLiteral::LowerOrEqual(abs_var_, abs_ub));
    RETURN_IF_FALSE(integer_trail_->Enqueue(
        IntegerLiteral::GreaterOrEqual(var_, -abs_ub), {}, integer_reason_));
  }
  const IntegerValue max_abs_value = std::max(var_ub, -var_lb);
  if (max_abs_value < abs_ub) {
    integer_reason_.clear();
    integer_reason_.push_back(
        IntegerLiteral::LowerOrEqual(var_, max_abs_value));
    integer_reason_.push_back(
        IntegerLiteral::GreaterOrEqual(var_, -max_abs_value));
    RETURN_IF_FALSE(integer_trail_->Enqueue(
        IntegerLiteral::LowerOrEqual(abs_var_, max_abs_value), {},
        integer_reason_));
  }

  const IntegerValue abs_lb = integer_trail_->LowerBound(abs_var_);
  if (var_lb > abs_lb) {  // var > 0.
    integer_reason_.clear();
    integer_reason_.push_back(IntegerLiteral::GreaterOrEqual(var_, var_lb));
    RETURN_IF_FALSE(integer_trail_->Enqueue(
        IntegerLiteral::GreaterOrEqual(abs_var_, var_lb), {}, integer_reason_));
  } else if (var_ub < -abs_lb) {  // var < 0.
    integer_reason_.clear();
    integer_reason_.push_back(IntegerLiteral::LowerOrEqual(var_, var_ub));
    RETURN_IF_FALSE(integer_trail_->Enqueue(
        IntegerLiteral::GreaterOrEqual(abs_var_, -var_ub), {},
        integer_reason_));
  }

  if (abs_lb == 0) return true;

  if (var_lb > -abs_lb && abs_lb > var_lb) {  // forces abs_var == var.
    integer_reason_.clear();
    integer_reason_.push_back(
        IntegerLiteral::GreaterOrEqual(var_, -abs_lb + 1));
    integer_reason_.push_back(IntegerLiteral::GreaterOrEqual(abs_var_, abs_lb));
    RETURN_IF_FALSE(integer_trail_->Enqueue(
        IntegerLiteral::GreaterOrEqual(var_, abs_lb), {}, integer_reason_));
  } else if (var_ub < abs_lb && var_ub > -abs_lb) {  // abs_var == -var.
    integer_reason_.clear();
    integer_reason_.push_back(IntegerLiteral::LowerOrEqual(var_, abs_lb - 1));
    integer_reason_.push_back(IntegerLiteral::GreaterOrEqual(abs_var_, abs_lb));
    RETURN_IF_FALSE(integer_trail_->Enqueue(
        IntegerLiteral::LowerOrEqual(var_, -abs_lb), {}, integer_reason_))
  }
  return true;
}

#undef RETURN_IF_FALSE

void AbsPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->WatchLowerBound(var_, id);
  watcher->WatchUpperBound(var_, id);
  watcher->WatchLowerBound(abs_var_, id);
  watcher->WatchUpperBound(abs_var_, id);
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

FixedDivisionPropagator::FixedDivisionPropagator(IntegerVariable a,
                                                 IntegerValue b,
                                                 IntegerVariable c,
                                                 IntegerTrail* integer_trail)
    : a_(a), b_(b), c_(c), integer_trail_(integer_trail) {}

bool FixedDivisionPropagator::Propagate() {
  const IntegerValue min_a = integer_trail_->LowerBound(a_);
  const IntegerValue max_a = integer_trail_->UpperBound(a_);
  IntegerValue min_c = integer_trail_->LowerBound(c_);
  IntegerValue max_c = integer_trail_->UpperBound(c_);

  CHECK_GT(b_, 0);

  if (max_a / b_ < max_c) {
    max_c = max_a / b_;
    if (!integer_trail_->Enqueue(IntegerLiteral::LowerOrEqual(c_, max_c), {},
                                 {integer_trail_->UpperBoundAsLiteral(a_)})) {
      return false;
    }
  } else if (max_a / b_ > max_c) {
    const IntegerValue new_max_a =
        max_c >= 0 ? max_c * b_ + b_ - 1
                   : IntegerValue(CapProd(max_c.value(), b_.value()));
    CHECK_LT(new_max_a, max_a);
    if (!integer_trail_->Enqueue(IntegerLiteral::LowerOrEqual(a_, new_max_a),
                                 {},
                                 {integer_trail_->UpperBoundAsLiteral(c_)})) {
      return false;
    }
  }

  if (min_a / b_ > min_c) {
    min_c = min_a / b_;
    if (!integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(c_, min_c), {},
                                 {integer_trail_->LowerBoundAsLiteral(a_)})) {
      return false;
    }
  } else if (min_a / b_ < min_c) {
    const IntegerValue new_min_a =
        min_c > 0 ? IntegerValue(CapProd(min_c.value(), b_.value()))
                  : min_c * b_ - b_ + 1;
    CHECK_GT(new_min_a, min_a);
    if (!integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(a_, new_min_a),
                                 {},
                                 {integer_trail_->LowerBoundAsLiteral(c_)})) {
      return false;
    }
  }

  return true;
}

void FixedDivisionPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->WatchIntegerVariable(a_, id);
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
    absl::flat_hash_map<int64, std::vector<Literal>> value_to_selector;
    for (int i = 0; i < values.size(); ++i) {
      unique_values.push_back(values[i].value());
      value_to_selector[values[i].value()].push_back(selectors[i]);
    }
    gtl::STLSortAndRemoveDuplicates(&unique_values);

    integer_trail->UpdateInitialDomain(var, Domain::FromValues(unique_values));
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
