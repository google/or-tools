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

#include "ortools/sat/integer_expr.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/numeric/int128.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"
#include "ortools/sat/cp_constraints.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/util.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

template <bool use_int128>
LinearConstraintPropagator<use_int128>::LinearConstraintPropagator(
    absl::Span<const Literal> enforcement_literals,
    absl::Span<const IntegerVariable> vars,
    absl::Span<const IntegerValue> coeffs, IntegerValue upper, Model* model)
    : upper_bound_(upper),
      shared_(
          model->GetOrCreate<LinearConstraintPropagator<use_int128>::Shared>()),
      size_(vars.size()),
      vars_(new IntegerVariable[size_]),
      coeffs_(new IntegerValue[size_]),
      max_variations_(new IntegerValue[size_]) {
  // TODO(user): deal with this corner case.
  CHECK(!vars.empty());

  // Copy data.
  memcpy(vars_.get(), vars.data(), size_ * sizeof(IntegerVariable));
  memcpy(coeffs_.get(), coeffs.data(), size_ * sizeof(IntegerValue));

  // Handle negative coefficients.
  for (int i = 0; i < size_; ++i) {
    if (coeffs_[i] < 0) {
      vars_[i] = NegationOf(vars_[i]);
      coeffs_[i] = -coeffs_[i];
    }
  }

  // Literal reason will only be used with the negation of enforcement_literals.
  // It will stay constant. We also do not store enforcement_literals, but
  // retrieve them from there.
  literal_reason_.reserve(enforcement_literals.size());
  for (const Literal literal : enforcement_literals) {
    literal_reason_.push_back(literal.Negated());
  }

  // Initialize the reversible numbers.
  rev_num_fixed_vars_ = 0;
  rev_lb_fixed_vars_ = IntegerValue(0);
}

// TODO(user): Avoid duplication with other constructor.
template <bool use_int128>
LinearConstraintPropagator<use_int128>::LinearConstraintPropagator(
    LinearConstraint ct, Model* model)
    : upper_bound_(ct.ub),
      shared_(
          model->GetOrCreate<LinearConstraintPropagator<use_int128>::Shared>()),
      size_(ct.num_terms),
      vars_(std::move(ct.vars)),
      coeffs_(std::move(ct.coeffs)),
      max_variations_(new IntegerValue[size_]) {
  // TODO(user): deal with this corner case.
  CHECK_GT(size_, 0);

  // Handle negative coefficients.
  for (int i = 0; i < size_; ++i) {
    if (coeffs_[i] < 0) {
      vars_[i] = NegationOf(vars_[i]);
      coeffs_[i] = -coeffs_[i];
    }
  }

  // Initialize the reversible numbers.
  rev_num_fixed_vars_ = 0;
  rev_lb_fixed_vars_ = IntegerValue(0);
}

template <bool use_int128>
void LinearConstraintPropagator<use_int128>::FillIntegerReason() {
  shared_->integer_reason.clear();
  shared_->reason_coeffs.clear();
  for (int i = 0; i < size_; ++i) {
    const IntegerVariable var = vars_[i];
    if (!shared_->integer_trail->VariableLowerBoundIsFromLevelZero(var)) {
      shared_->integer_reason.push_back(
          shared_->integer_trail->LowerBoundAsLiteral(var));
      shared_->reason_coeffs.push_back(coeffs_[i]);
    }
  }
}

namespace {
IntegerValue CappedCast(absl::int128 input, IntegerValue cap) {
  if (input >= absl::int128(cap.value())) {
    return cap;
  }
  return IntegerValue(static_cast<int64_t>(input));
}

}  // namespace

// NOTE(user): This is only used with int128, so we code only a single version.
template <bool use_int128>
std::pair<IntegerValue, IntegerValue>
LinearConstraintPropagator<use_int128>::ConditionalLb(
    IntegerLiteral integer_literal, IntegerVariable target_var) const {
  // The code below is wrong if integer_literal and target_var are the same.
  // In this case we return the trivial bounds.
  if (PositiveVariable(integer_literal.var) == PositiveVariable(target_var)) {
    if (integer_literal.var == target_var) {
      return {kMinIntegerValue, integer_literal.bound};
    } else {
      return {integer_literal.Negated().bound, kMinIntegerValue};
    }
  }

  // Recall that all our coefficient are positive.
  bool literal_var_present = false;
  bool literal_var_present_positively = false;
  IntegerValue var_coeff;

  bool target_var_present_negatively = false;
  absl::int128 target_coeff;

  // Warning: It is important to do the computation like the propagation is
  // doing it to be sure we don't have overflow, since this is what we check
  // when creating constraints.
  absl::int128 lb_128 = 0;
  for (int i = 0; i < size_; ++i) {
    const IntegerVariable var = vars_[i];
    const IntegerValue coeff = coeffs_[i];
    if (var == NegationOf(target_var)) {
      target_coeff = absl::int128(coeff.value());
      target_var_present_negatively = true;
    }

    const IntegerValue lb = shared_->integer_trail->LowerBound(var);
    lb_128 += absl::int128(coeff.value()) * absl::int128(lb.value());
    if (PositiveVariable(var) == PositiveVariable(integer_literal.var)) {
      var_coeff = coeff;
      literal_var_present = true;
      literal_var_present_positively = (var == integer_literal.var);
    }
  }

  if (!literal_var_present || !target_var_present_negatively) {
    return {kMinIntegerValue, kMinIntegerValue};
  }

  // The upper bound on NegationOf(target_var) are lb(-target) + slack / coeff.
  // So the lower bound on target_var is ub - slack / coeff.
  const absl::int128 slack128 = absl::int128(upper_bound_.value()) - lb_128;
  const IntegerValue target_lb = shared_->integer_trail->LowerBound(target_var);
  const IntegerValue target_ub = shared_->integer_trail->UpperBound(target_var);
  if (slack128 <= 0) {
    // TODO(user): If there is a conflict (negative slack) we can be more
    // precise.
    return {target_ub, target_ub};
  }

  const IntegerValue target_diff = target_ub - target_lb;
  const IntegerValue delta = CappedCast(slack128 / target_coeff, target_diff);

  // A literal means var >= bound.
  if (literal_var_present_positively) {
    // We have var_coeff * var in the expression, the literal is var >= bound.
    // When it is false, it is not relevant as implied_lb used var >= lb.
    // When it is true, the diff is bound - lb.
    const IntegerValue diff =
        std::max(IntegerValue(0),
                 integer_literal.bound -
                     shared_->integer_trail->LowerBound(integer_literal.var));
    const absl::int128 tighter_slack =
        std::max(absl::int128(0), slack128 - absl::int128(var_coeff.value()) *
                                                 absl::int128(diff.value()));
    const IntegerValue tighter_delta =
        CappedCast(tighter_slack / target_coeff, target_diff);
    return {target_ub - delta, target_ub - tighter_delta};
  } else {
    // We have var_coeff * -var in the expression, the literal is var >= bound.
    // When it is true, it is not relevant as implied_lb used -var >= -ub.
    // And when it is false it means var < bound, so -var >= -bound + 1
    const IntegerValue diff =
        std::max(IntegerValue(0),
                 shared_->integer_trail->UpperBound(integer_literal.var) -
                     integer_literal.bound + 1);
    const absl::int128 tighter_slack =
        std::max(absl::int128(0), slack128 - absl::int128(var_coeff.value()) *
                                                 absl::int128(diff.value()));
    const IntegerValue tighter_delta =
        CappedCast(tighter_slack / target_coeff, target_diff);
    return {target_ub - tighter_delta, target_ub - delta};
  }
}

template <bool use_int128>
void LinearConstraintPropagator<use_int128>::Explain(
    int /*id*/, IntegerValue propagation_slack, IntegerVariable var_to_explain,
    int trail_index, std::vector<Literal>* literals_reason,
    std::vector<int>* trail_indices_reason) {
  *literals_reason = literal_reason_;
  trail_indices_reason->clear();
  shared_->reason_coeffs.clear();
  for (int i = 0; i < size_; ++i) {
    const IntegerVariable var = vars_[i];
    if (PositiveVariable(var) == PositiveVariable(var_to_explain)) {
      continue;
    }
    const int index =
        shared_->integer_trail->FindTrailIndexOfVarBefore(var, trail_index);
    if (index >= 0) {
      trail_indices_reason->push_back(index);
      if (propagation_slack > 0) {
        shared_->reason_coeffs.push_back(coeffs_[i]);
      }
    }
  }
  if (propagation_slack > 0) {
    shared_->integer_trail->RelaxLinearReason(
        propagation_slack, shared_->reason_coeffs, trail_indices_reason);
  }
}

template <bool use_int128>
bool LinearConstraintPropagator<use_int128>::Propagate() {
  // Reified case: If any of the enforcement_literals are false, we ignore the
  // constraint.
  int num_unassigned_enforcement_literal = 0;
  LiteralIndex unique_unnasigned_literal = kNoLiteralIndex;
  for (const Literal negated_enforcement : literal_reason_) {
    const Literal literal = negated_enforcement.Negated();
    if (shared_->assignment.LiteralIsFalse(literal)) return true;
    if (!shared_->assignment.LiteralIsTrue(literal)) {
      ++num_unassigned_enforcement_literal;
      unique_unnasigned_literal = literal.Index();
    }
  }

  // Unfortunately, we can't propagate anything if we have more than one
  // unassigned enforcement literal.
  if (num_unassigned_enforcement_literal > 1) return true;

  int num_fixed_vars = rev_num_fixed_vars_;
  IntegerValue lb_fixed_vars = rev_lb_fixed_vars_;

  // Compute the new lower bound and update the reversible structures.
  absl::int128 lb_128 = 0;
  IntegerValue lb_unfixed_vars = IntegerValue(0);
  for (int i = num_fixed_vars; i < size_; ++i) {
    const IntegerVariable var = vars_[i];
    const IntegerValue coeff = coeffs_[i];
    const IntegerValue lb = shared_->integer_trail->LowerBound(var);
    const IntegerValue ub = shared_->integer_trail->UpperBound(var);
    if (use_int128) {
      lb_128 += absl::int128(lb.value()) * absl::int128(coeff.value());
      continue;
    }

    if (lb != ub) {
      max_variations_[i] = (ub - lb) * coeff;
      lb_unfixed_vars += lb * coeff;
    } else {
      // Update the set of fixed variables.
      std::swap(vars_[i], vars_[num_fixed_vars]);
      std::swap(coeffs_[i], coeffs_[num_fixed_vars]);
      std::swap(max_variations_[i], max_variations_[num_fixed_vars]);
      num_fixed_vars++;
      lb_fixed_vars += lb * coeff;
    }
  }
  shared_->time_limit->AdvanceDeterministicTime(
      static_cast<double>(size_ - num_fixed_vars) * 5e-9);

  // Save the current sum of fixed variables.
  if (is_registered_ && num_fixed_vars != rev_num_fixed_vars_) {
    CHECK(!use_int128);
    shared_->rev_integer_value_repository->SaveState(&rev_lb_fixed_vars_);
    shared_->rev_int_repository->SaveState(&rev_num_fixed_vars_);
    rev_num_fixed_vars_ = num_fixed_vars;
    rev_lb_fixed_vars_ = lb_fixed_vars;
  }

  // If use_int128 is true, the slack or propagation slack can be larger than
  // this. To detect overflow with capped arithmetic, it is important the slack
  // used in our algo never exceed this value.
  const absl::int128 max_slack = std::numeric_limits<int64_t>::max() - 1;

  // Conflict?
  IntegerValue slack;
  absl::int128 slack128;
  if (use_int128) {
    slack128 = absl::int128(upper_bound_.value()) - lb_128;
    if (slack128 < 0) {
      // It is fine if we don't relax as much as possible.
      // Note that RelaxLinearReason() is overflow safe.
      slack = static_cast<int64_t>(std::max(-max_slack, slack128));
    }
  } else {
    slack = upper_bound_ - (lb_fixed_vars + lb_unfixed_vars);
  }
  if (slack < 0) {
    FillIntegerReason();
    shared_->integer_trail->RelaxLinearReason(
        -slack - 1, shared_->reason_coeffs, &shared_->integer_reason);

    if (num_unassigned_enforcement_literal == 1) {
      // Propagate the only non-true literal to false.
      const Literal to_propagate = Literal(unique_unnasigned_literal).Negated();
      std::vector<Literal> tmp = literal_reason_;
      tmp.erase(std::find(tmp.begin(), tmp.end(), to_propagate));
      shared_->integer_trail->EnqueueLiteral(to_propagate, tmp,
                                             shared_->integer_reason);
      return true;
    }
    return shared_->integer_trail->ReportConflict(literal_reason_,
                                                  shared_->integer_reason);
  }

  // We can only propagate more if all the enforcement literals are true.
  if (num_unassigned_enforcement_literal > 0) return true;

  // The lower bound of all the variables except one can be used to update the
  // upper bound of the last one.
  for (int i = num_fixed_vars; i < size_; ++i) {
    if (!use_int128 && max_variations_[i] <= slack) continue;

    // TODO(user): If the new ub fall into an hole of the variable, we can
    // actually relax the reason more by computing a better slack.
    const IntegerVariable var = vars_[i];
    const IntegerValue coeff = coeffs_[i];
    const IntegerValue lb = shared_->integer_trail->LowerBound(var);

    IntegerValue new_ub;
    IntegerValue propagation_slack;
    if (use_int128) {
      const absl::int128 coeff128 = absl::int128(coeff.value());
      const absl::int128 div128 = slack128 / coeff128;
      const IntegerValue ub = shared_->integer_trail->UpperBound(var);
      if (absl::int128(lb.value()) + div128 >= absl::int128(ub.value())) {
        continue;
      }
      new_ub = lb + IntegerValue(static_cast<int64_t>(div128));
      propagation_slack = static_cast<int64_t>(
          std::min(max_slack, (div128 + 1) * coeff128 - slack128 - 1));
    } else {
      const IntegerValue div = slack / coeff;
      new_ub = lb + div;
      propagation_slack = (div + 1) * coeff - slack - 1;
    }
    if (!shared_->integer_trail->EnqueueWithLazyReason(
            IntegerLiteral::LowerOrEqual(var, new_ub), 0, propagation_slack,
            this)) {
      // TODO(user): this is never supposed to happen since if we didn't have a
      // conflict above, we should be able to reduce the upper bound. It might
      // indicate an issue with our Boolean <-> integer encoding.
      return false;
    }
  }

  return true;
}

template <bool use_int128>
bool LinearConstraintPropagator<use_int128>::PropagateAtLevelZero() {
  // TODO(user): Deal with enforcements. It is just a bit of code to read the
  // value of the literals at level zero.
  if (!literal_reason_.empty()) return true;

  // Compute the new lower bound and update the reversible structures.
  absl::int128 lb_128 = 0;
  IntegerValue min_activity = IntegerValue(0);
  for (int i = 0; i < size_; ++i) {
    const IntegerVariable var = vars_[i];
    const IntegerValue coeff = coeffs_[i];
    const IntegerValue lb = shared_->integer_trail->LevelZeroLowerBound(var);
    if (use_int128) {
      lb_128 += absl::int128(lb.value()) * absl::int128(coeff.value());
    } else {
      const IntegerValue ub = shared_->integer_trail->LevelZeroUpperBound(var);
      max_variations_[i] = (ub - lb) * coeff;
      min_activity += lb * coeff;
    }
  }
  shared_->time_limit->AdvanceDeterministicTime(
      static_cast<double>(size_ * 1e-9));

  // Conflict?
  IntegerValue slack;
  absl::int128 slack128;
  if (use_int128) {
    slack128 = absl::int128(upper_bound_.value()) - lb_128;
    if (slack128 < 0) {
      return shared_->integer_trail->ReportConflict({}, {});
    }
  } else {
    slack = upper_bound_ - min_activity;
    if (slack < 0) {
      return shared_->integer_trail->ReportConflict({}, {});
    }
  }

  // The lower bound of all the variables except one can be used to update the
  // upper bound of the last one.
  for (int i = 0; i < size_; ++i) {
    if (!use_int128 && max_variations_[i] <= slack) continue;

    const IntegerVariable var = vars_[i];
    const IntegerValue coeff = coeffs_[i];
    const IntegerValue lb = shared_->integer_trail->LevelZeroLowerBound(var);

    IntegerValue new_ub;
    if (use_int128) {
      const IntegerValue ub = shared_->integer_trail->LevelZeroUpperBound(var);
      if (absl::int128((ub - lb).value()) * absl::int128(coeff.value()) <=
          slack128) {
        continue;
      }
      const absl::int128 div128 = slack128 / absl::int128(coeff.value());
      new_ub = lb + IntegerValue(static_cast<int64_t>(div128));
    } else {
      const IntegerValue div = slack / coeff;
      new_ub = lb + div;
    }
    if (!shared_->integer_trail->Enqueue(
            IntegerLiteral::LowerOrEqual(var, new_ub), {}, {})) {
      return false;
    }
  }

  return true;
}

template <bool use_int128>
void LinearConstraintPropagator<use_int128>::RegisterWith(
    GenericLiteralWatcher* watcher) {
  is_registered_ = true;
  const int id = watcher->Register(this);
  for (int i = 0; i < size_; ++i) {
    watcher->WatchLowerBound(vars_[i], id);
  }
  for (const Literal negated_enforcement : literal_reason_) {
    // We only watch the true direction.
    //
    // TODO(user): if there is more than one, maybe we should watch more to
    // propagate a "conflict" as soon as only one is unassigned?
    watcher->WatchLiteral(negated_enforcement.Negated(), id);
  }
}

// Explicit declaration.
template class LinearConstraintPropagator<true>;
template class LinearConstraintPropagator<false>;

LevelZeroEquality::LevelZeroEquality(IntegerVariable target,
                                     const std::vector<IntegerVariable>& vars,
                                     const std::vector<IntegerValue>& coeffs,
                                     Model* model)
    : target_(target),
      vars_(vars),
      coeffs_(coeffs),
      trail_(model->GetOrCreate<Trail>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()) {
  auto* watcher = model->GetOrCreate<GenericLiteralWatcher>();
  const int id = watcher->Register(this);
  watcher->SetPropagatorPriority(id, 2);
  watcher->WatchIntegerVariable(target, id);
  for (const IntegerVariable& var : vars_) {
    watcher->WatchIntegerVariable(var, id);
  }
}

// TODO(user): We could go even further than just the GCD, and do more
// arithmetic to tighten the target bounds. See for instance a problem like
// ej.mps.gz that we don't solve easily, but has just 3 variables! the goal is
// to minimize X, given 31013 X - 41014 Y - 51015 Z = -31013 (all >=0, Y and Z
// bounded with high values). I know some MIP solvers have a basic linear
// diophantine equation support.
bool LevelZeroEquality::Propagate() {
  // TODO(user): Once the GCD is not 1, we could at any level make sure the
  // objective is of the correct form. For now, this only happen in a few
  // miplib problem that we close quickly, so I didn't add the extra code yet.
  if (trail_->CurrentDecisionLevel() != 0) return true;

  int64_t gcd = 0;
  IntegerValue sum(0);
  for (int i = 0; i < vars_.size(); ++i) {
    if (integer_trail_->IsFixed(vars_[i])) {
      sum += coeffs_[i] * integer_trail_->LowerBound(vars_[i]);
      continue;
    }
    gcd = MathUtil::GCD64(gcd, std::abs(coeffs_[i].value()));
    if (gcd == 1) break;
  }
  if (gcd == 0) return true;  // All fixed.

  if (gcd > gcd_) {
    VLOG(1) << "Objective gcd: " << gcd;
  }
  CHECK_GE(gcd, gcd_);
  gcd_ = IntegerValue(gcd);

  const IntegerValue lb = integer_trail_->LowerBound(target_);
  const IntegerValue lb_remainder = PositiveRemainder(lb - sum, gcd_);
  if (lb_remainder != 0) {
    if (!integer_trail_->Enqueue(
            IntegerLiteral::GreaterOrEqual(target_, lb + gcd_ - lb_remainder),
            {}, {}))
      return false;
  }

  const IntegerValue ub = integer_trail_->UpperBound(target_);
  const IntegerValue ub_remainder =
      PositiveRemainder(ub - sum, IntegerValue(gcd));
  if (ub_remainder != 0) {
    if (!integer_trail_->Enqueue(
            IntegerLiteral::LowerOrEqual(target_, ub - ub_remainder), {}, {}))
      return false;
  }

  return true;
}

MinPropagator::MinPropagator(std::vector<AffineExpression> vars,
                             AffineExpression min_var,
                             IntegerTrail* integer_trail)
    : vars_(std::move(vars)),
      min_var_(min_var),
      integer_trail_(integer_trail) {}

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
    for (const AffineExpression& var : vars_) {
      integer_reason_.push_back(var.GreaterOrEqual(min));
    }
    if (!integer_trail_->SafeEnqueue(min_var_.GreaterOrEqual(min),
                                     integer_reason_)) {
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
      for (const AffineExpression& var : vars_) {
        if (var == vars_[last_possible_min_interval]) continue;
        integer_reason_.push_back(var.GreaterOrEqual(current_min_ub + 1));
      }
      if (!integer_trail_->SafeEnqueue(
              vars_[last_possible_min_interval].LowerOrEqual(current_min_ub),
              integer_reason_)) {
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
    for (const AffineExpression& var : vars_) {
      IntegerLiteral lit = var.GreaterOrEqual(current_min_ub + 1);
      if (lit != IntegerLiteral::TrueLiteral()) {
        integer_reason_.push_back(lit);
      }
    }
    return integer_trail_->ReportConflict(integer_reason_);
  }

  return true;
}

void MinPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (const AffineExpression& var : vars_) {
    watcher->WatchLowerBound(var, id);
  }
  watcher->WatchUpperBound(min_var_, id);
}

LinMinPropagator::LinMinPropagator(std::vector<LinearExpression> exprs,
                                   IntegerVariable min_var, Model* model)
    : exprs_(std::move(exprs)),
      min_var_(min_var),
      model_(model),
      integer_trail_(model_->GetOrCreate<IntegerTrail>()) {}

void LinMinPropagator::Explain(int id, IntegerValue propagation_slack,
                               IntegerVariable var_to_explain, int trail_index,
                               std::vector<Literal>* literals_reason,
                               std::vector<int>* trail_indices_reason) {
  const auto& vars = exprs_[id].vars;
  const auto& coeffs = exprs_[id].coeffs;
  literals_reason->clear();
  trail_indices_reason->clear();
  std::vector<IntegerValue> reason_coeffs;
  const int size = vars.size();
  for (int i = 0; i < size; ++i) {
    const IntegerVariable var = vars[i];
    if (PositiveVariable(var) == PositiveVariable(var_to_explain)) {
      continue;
    }
    const int index =
        integer_trail_->FindTrailIndexOfVarBefore(var, trail_index);
    if (index >= 0) {
      trail_indices_reason->push_back(index);
      if (propagation_slack > 0) {
        reason_coeffs.push_back(coeffs[i]);
      }
    }
  }
  if (propagation_slack > 0) {
    integer_trail_->RelaxLinearReason(propagation_slack, reason_coeffs,
                                      trail_indices_reason);
  }
  // Now add the old integer_reason that triggered this propagation.
  for (IntegerLiteral reason_lit : integer_reason_for_unique_candidate_) {
    const int index =
        integer_trail_->FindTrailIndexOfVarBefore(reason_lit.var, trail_index);
    if (index >= 0) {
      trail_indices_reason->push_back(index);
    }
  }
}

bool LinMinPropagator::PropagateLinearUpperBound(
    int id, absl::Span<const IntegerVariable> vars,
    absl::Span<const IntegerValue> coeffs, const IntegerValue upper_bound) {
  IntegerValue sum_lb = IntegerValue(0);
  const int num_vars = vars.size();
  max_variations_.resize(num_vars);
  for (int i = 0; i < num_vars; ++i) {
    const IntegerVariable var = vars[i];
    const IntegerValue coeff = coeffs[i];
    // The coefficients are assumed to be positive for this to work properly.
    DCHECK_GE(coeff, 0);
    const IntegerValue lb = integer_trail_->LowerBound(var);
    const IntegerValue ub = integer_trail_->UpperBound(var);
    max_variations_[i] = (ub - lb) * coeff;
    sum_lb += lb * coeff;
  }

  model_->GetOrCreate<TimeLimit>()->AdvanceDeterministicTime(
      static_cast<double>(num_vars) * 1e-9);

  const IntegerValue slack = upper_bound - sum_lb;
  if (slack < 0) {
    // Conflict.
    local_reason_.clear();
    reason_coeffs_.clear();
    for (int i = 0; i < num_vars; ++i) {
      const IntegerVariable var = vars[i];
      if (!integer_trail_->VariableLowerBoundIsFromLevelZero(var)) {
        local_reason_.push_back(integer_trail_->LowerBoundAsLiteral(var));
        reason_coeffs_.push_back(coeffs[i]);
      }
    }
    integer_trail_->RelaxLinearReason(-slack - 1, reason_coeffs_,
                                      &local_reason_);
    local_reason_.insert(local_reason_.end(),
                         integer_reason_for_unique_candidate_.begin(),
                         integer_reason_for_unique_candidate_.end());
    return integer_trail_->ReportConflict({}, local_reason_);
  }

  // The lower bound of all the variables except one can be used to update the
  // upper bound of the last one.
  for (int i = 0; i < num_vars; ++i) {
    if (max_variations_[i] <= slack) continue;

    const IntegerVariable var = vars[i];
    const IntegerValue coeff = coeffs[i];
    const IntegerValue div = slack / coeff;
    const IntegerValue new_ub = integer_trail_->LowerBound(var) + div;
    const IntegerValue propagation_slack = (div + 1) * coeff - slack - 1;
    if (!integer_trail_->EnqueueWithLazyReason(
            IntegerLiteral::LowerOrEqual(var, new_ub), id, propagation_slack,
            this)) {
      return false;
    }
  }
  return true;
}

bool LinMinPropagator::Propagate() {
  if (exprs_.empty()) return true;

  // Count the number of interval that are possible candidate for the min.
  // Only the intervals for which lb > current_min_ub cannot.
  const IntegerValue current_min_ub = integer_trail_->UpperBound(min_var_);
  int num_intervals_that_can_be_min = 0;
  int last_possible_min_interval = 0;

  expr_lbs_.clear();
  IntegerValue min_of_linear_expression_lb = kMaxIntegerValue;
  for (int i = 0; i < exprs_.size(); ++i) {
    const IntegerValue lb = exprs_[i].Min(*integer_trail_);
    expr_lbs_.push_back(lb);
    min_of_linear_expression_lb = std::min(min_of_linear_expression_lb, lb);
    if (lb <= current_min_ub) {
      ++num_intervals_that_can_be_min;
      last_possible_min_interval = i;
    }
  }

  // Propagation a) lb(min) >= lb(MIN(exprs)) = MIN(lb(exprs));

  // Conflict will be detected by the fact that the [lb, ub] of the min is
  // empty. In case of conflict, we just need the reason for pushing UB + 1.
  if (min_of_linear_expression_lb > current_min_ub) {
    min_of_linear_expression_lb = current_min_ub + 1;
  }
  if (min_of_linear_expression_lb > integer_trail_->LowerBound(min_var_)) {
    local_reason_.clear();
    for (int i = 0; i < exprs_.size(); ++i) {
      const IntegerValue slack = expr_lbs_[i] - min_of_linear_expression_lb;
      integer_trail_->AppendRelaxedLinearReason(slack, exprs_[i].coeffs,
                                                exprs_[i].vars, &local_reason_);
    }
    if (!integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(
                                     min_var_, min_of_linear_expression_lb),
                                 {}, local_reason_)) {
      return false;
    }
  }

  // Propagation b) ub(min) >= ub(MIN(exprs)) and we can't propagate anything
  // here unless there is just one possible expression 'e' that can be the min:
  //   for all u != e, lb(u) > ub(min);
  // In this case, ub(min) >= ub(e).
  if (num_intervals_that_can_be_min == 1) {
    const IntegerValue ub_of_only_candidate =
        exprs_[last_possible_min_interval].Max(*integer_trail_);
    if (current_min_ub < ub_of_only_candidate) {
      // For this propagation, we only need to fill the integer reason once at
      // the lowest level. At higher levels this reason still remains valid.
      if (rev_unique_candidate_ == 0) {
        integer_reason_for_unique_candidate_.clear();

        // The reason is that all the other interval start after current_min_ub.
        // And that min_ub has its current value.
        integer_reason_for_unique_candidate_.push_back(
            integer_trail_->UpperBoundAsLiteral(min_var_));
        for (int i = 0; i < exprs_.size(); ++i) {
          if (i == last_possible_min_interval) continue;
          const IntegerValue slack = expr_lbs_[i] - (current_min_ub + 1);
          integer_trail_->AppendRelaxedLinearReason(
              slack, exprs_[i].coeffs, exprs_[i].vars,
              &integer_reason_for_unique_candidate_);
        }
        rev_unique_candidate_ = 1;
      }

      return PropagateLinearUpperBound(
          last_possible_min_interval, exprs_[last_possible_min_interval].vars,
          exprs_[last_possible_min_interval].coeffs,
          current_min_ub - exprs_[last_possible_min_interval].offset);
    }
  }

  return true;
}

void LinMinPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  bool has_var_also_in_exprs = false;
  for (const LinearExpression& expr : exprs_) {
    for (int i = 0; i < expr.vars.size(); ++i) {
      const IntegerVariable& var = expr.vars[i];
      const IntegerValue coeff = expr.coeffs[i];
      if (coeff > 0) {
        watcher->WatchLowerBound(var, id);
      } else {
        watcher->WatchUpperBound(var, id);
      }
      has_var_also_in_exprs |= (var == min_var_);
    }
  }
  watcher->WatchUpperBound(min_var_, id);
  watcher->RegisterReversibleInt(id, &rev_unique_candidate_);
  if (has_var_also_in_exprs) {
    watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
  }
}

ProductPropagator::ProductPropagator(
    absl::Span<const Literal> enforcement_literals, AffineExpression a,
    AffineExpression b, AffineExpression p, Model* model)
    : a_(a),
      b_(b),
      p_(p),
      integer_trail_(*model->GetOrCreate<IntegerTrail>()),
      enforcement_propagator_(*model->GetOrCreate<EnforcementPropagator>()) {
  GenericLiteralWatcher* watcher = model->GetOrCreate<GenericLiteralWatcher>();
  enforcement_id_ = enforcement_propagator_.Register(
      enforcement_literals, watcher, RegisterWith(watcher));
}

// We want all affine expression to be either non-negative or across zero.
bool ProductPropagator::CanonicalizeCases() {
  if (integer_trail_.UpperBound(a_) <= 0) {
    a_ = a_.Negated();
    p_ = p_.Negated();
  }
  if (integer_trail_.UpperBound(b_) <= 0) {
    b_ = b_.Negated();
    p_ = p_.Negated();
  }

  // If both a and b positive, p must be too.
  if (integer_trail_.LowerBound(a_) >= 0 &&
      integer_trail_.LowerBound(b_) >= 0) {
    return enforcement_propagator_.SafeEnqueue(
        enforcement_id_, p_.GreaterOrEqual(0),
        {a_.GreaterOrEqual(0), b_.GreaterOrEqual(0)});
  }

  // Otherwise, make sure p is non-negative or across zero.
  if (integer_trail_.UpperBound(p_) <= 0) {
    if (integer_trail_.LowerBound(a_) < 0) {
      DCHECK_GT(integer_trail_.UpperBound(a_), 0);
      a_ = a_.Negated();
      p_ = p_.Negated();
    } else {
      DCHECK_LT(integer_trail_.LowerBound(b_), 0);
      DCHECK_GT(integer_trail_.UpperBound(b_), 0);
      b_ = b_.Negated();
      p_ = p_.Negated();
    }
  }

  return true;
}

// Note that this propagation is exact, except on the domain of p as this
// involves more complex arithmetic.
//
// TODO(user): We could tighten the bounds on p by removing extreme value that
// do not contains divisor in the domains of a or b. There is an algo in O(
// smallest domain size between a or b).
bool ProductPropagator::PropagateWhenAllNonNegative() {
  {
    const IntegerValue max_a = integer_trail_.UpperBound(a_);
    const IntegerValue max_b = integer_trail_.UpperBound(b_);
    const IntegerValue new_max = CapProdI(max_a, max_b);
    if (new_max < integer_trail_.UpperBound(p_)) {
      if (!enforcement_propagator_.SafeEnqueue(
              enforcement_id_, p_.LowerOrEqual(new_max),
              {integer_trail_.UpperBoundAsLiteral(a_),
               integer_trail_.UpperBoundAsLiteral(b_), a_.GreaterOrEqual(0),
               b_.GreaterOrEqual(0)})) {
        return false;
      }
    }
  }

  {
    const IntegerValue min_a = integer_trail_.LowerBound(a_);
    const IntegerValue min_b = integer_trail_.LowerBound(b_);
    const IntegerValue new_min = CapProdI(min_a, min_b);

    // The conflict test is needed because when new_min is large, we could
    // have an overflow in p_.GreaterOrEqual(new_min);
    if (new_min > integer_trail_.UpperBound(p_)) {
      return enforcement_propagator_.ReportConflict(
          enforcement_id_, {integer_trail_.UpperBoundAsLiteral(p_),
                            integer_trail_.LowerBoundAsLiteral(a_),
                            integer_trail_.LowerBoundAsLiteral(b_)});
    }
    if (new_min > integer_trail_.LowerBound(p_)) {
      if (!enforcement_propagator_.SafeEnqueue(
              enforcement_id_, p_.GreaterOrEqual(new_min),
              {integer_trail_.LowerBoundAsLiteral(a_),
               integer_trail_.LowerBoundAsLiteral(b_)})) {
        return false;
      }
    }
  }

  for (int i = 0; i < 2; ++i) {
    const AffineExpression a = i == 0 ? a_ : b_;
    const AffineExpression b = i == 0 ? b_ : a_;
    const IntegerValue max_a = integer_trail_.UpperBound(a);
    const IntegerValue min_b = integer_trail_.LowerBound(b);
    const IntegerValue min_p = integer_trail_.LowerBound(p_);
    const IntegerValue max_p = integer_trail_.UpperBound(p_);
    const IntegerValue prod = CapProdI(max_a, min_b);
    if (prod > max_p) {
      if (!enforcement_propagator_.SafeEnqueue(
              enforcement_id_, a.LowerOrEqual(FloorRatio(max_p, min_b)),
              {integer_trail_.LowerBoundAsLiteral(b),
               integer_trail_.UpperBoundAsLiteral(p_), p_.GreaterOrEqual(0)})) {
        return false;
      }
    } else if (prod < min_p && max_a != 0) {
      if (!enforcement_propagator_.SafeEnqueue(
              enforcement_id_, b.GreaterOrEqual(CeilRatio(min_p, max_a)),
              {integer_trail_.UpperBoundAsLiteral(a),
               integer_trail_.LowerBoundAsLiteral(p_), a.GreaterOrEqual(0)})) {
        return false;
      }
    }
  }

  return true;
}

// This assumes p > 0, p = a * X, and X can take any value.
// We can propagate max of a by computing a bound on the min b when positive.
// The expression b is just used to detect when there is no solution given the
// upper bound of b.
bool ProductPropagator::PropagateMaxOnPositiveProduct(AffineExpression a,
                                                      AffineExpression b,
                                                      IntegerValue min_p,
                                                      IntegerValue max_p) {
  const IntegerValue max_a = integer_trail_.UpperBound(a);
  if (max_a <= 0) return true;
  DCHECK_GT(min_p, 0);

  if (max_a >= min_p) {
    if (max_p < max_a) {
      if (!enforcement_propagator_.SafeEnqueue(
              enforcement_id_, a.LowerOrEqual(max_p),
              {p_.LowerOrEqual(max_p), p_.GreaterOrEqual(1)})) {
        return false;
      }
    }
    return true;
  }

  const IntegerValue min_pos_b = CeilRatio(min_p, max_a);
  if (min_pos_b > integer_trail_.UpperBound(b)) {
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_, b.LowerOrEqual(0),
            {integer_trail_.LowerBoundAsLiteral(p_),
             integer_trail_.UpperBoundAsLiteral(a),
             integer_trail_.UpperBoundAsLiteral(b)})) {
      return false;
    }
    return true;
  }

  const IntegerValue new_max_a = FloorRatio(max_p, min_pos_b);
  if (new_max_a < integer_trail_.UpperBound(a)) {
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_, a.LowerOrEqual(new_max_a),
            {integer_trail_.LowerBoundAsLiteral(p_),
             integer_trail_.UpperBoundAsLiteral(a),
             integer_trail_.UpperBoundAsLiteral(p_)})) {
      return false;
    }
  }
  return true;
}

bool ProductPropagator::Propagate() {
  const EnforcementStatus status =
      enforcement_propagator_.Status(enforcement_id_);
  if (status == EnforcementStatus::CAN_PROPAGATE) {
    const int64_t min_a = integer_trail_.LowerBound(a_).value();
    const int64_t max_a = integer_trail_.UpperBound(a_).value();
    const int64_t min_b = integer_trail_.LowerBound(b_).value();
    const int64_t max_b = integer_trail_.UpperBound(b_).value();
    const int64_t min_p = integer_trail_.LowerBound(p_).value();
    const int64_t max_p = integer_trail_.UpperBound(p_).value();
    const int64_t p1 = CapProdI(max_a, max_b).value();
    const int64_t p2 = CapProdI(max_a, min_b).value();
    const int64_t p3 = CapProdI(min_a, max_b).value();
    const int64_t p4 = CapProdI(min_a, min_b).value();
    const int64_t min_ab = std::min({p1, p2, p3, p4});
    const int64_t max_ab = std::max({p1, p2, p3, p4});
    // If the bounds of a * b and p are disjoint, the enforcement must be false.
    // TODO(user): relax the reason in a better way.
    if (min_ab > max_p) {
      return enforcement_propagator_.PropagateWhenFalse(
          enforcement_id_,
          /*literal_reason=*/{},
          {a_.GreaterOrEqual(min_a), a_.LowerOrEqual(max_a),
           b_.GreaterOrEqual(min_b), b_.LowerOrEqual(max_b),
           p_.LowerOrEqual(max_p)});
    }
    if (min_p > max_ab) {
      return enforcement_propagator_.PropagateWhenFalse(
          enforcement_id_,
          /*literal_reason=*/{},
          {a_.GreaterOrEqual(min_a), a_.LowerOrEqual(max_a),
           b_.GreaterOrEqual(min_b), b_.LowerOrEqual(max_b),
           p_.GreaterOrEqual(min_p)});
    }
    // Otherwise we cannot propagate anything since the enforcement is unknown.
    return true;
  }

  if (status != EnforcementStatus::IS_ENFORCED) return true;
  if (!CanonicalizeCases()) return false;

  // In the most common case, we use better reasons even though the code
  // below would propagate the same.
  const int64_t min_a = integer_trail_.LowerBound(a_).value();
  const int64_t min_b = integer_trail_.LowerBound(b_).value();
  if (min_a >= 0 && min_b >= 0) {
    // This was done by CanonicalizeCases().
    DCHECK_GE(integer_trail_.LowerBound(p_), 0);
    return PropagateWhenAllNonNegative();
  }

  // Lets propagate on p_ first, the max/min is given by one of: max_a * max_b,
  // max_a * min_b, min_a * max_b, min_a * min_b. This is true, because any
  // product x * y, depending on the sign, is dominated by one of these.
  //
  // TODO(user): In the reasons, including all 4 bounds is always correct, but
  // we might be able to relax some of them.
  const IntegerValue max_a = integer_trail_.UpperBound(a_);
  const IntegerValue max_b = integer_trail_.UpperBound(b_);
  const IntegerValue p1 = CapProdI(max_a, max_b);
  const IntegerValue p2 = CapProdI(max_a, min_b);
  const IntegerValue p3 = CapProdI(min_a, max_b);
  const IntegerValue p4 = CapProdI(min_a, min_b);
  const IntegerValue new_max_p = std::max({p1, p2, p3, p4});
  if (new_max_p < integer_trail_.UpperBound(p_)) {
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_, p_.LowerOrEqual(new_max_p),
            {integer_trail_.LowerBoundAsLiteral(a_),
             integer_trail_.LowerBoundAsLiteral(b_),
             integer_trail_.UpperBoundAsLiteral(a_),
             integer_trail_.UpperBoundAsLiteral(b_)})) {
      return false;
    }
  }
  const IntegerValue new_min_p = std::min({p1, p2, p3, p4});
  if (new_min_p > integer_trail_.LowerBound(p_)) {
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_, p_.GreaterOrEqual(new_min_p),
            {integer_trail_.LowerBoundAsLiteral(a_),
             integer_trail_.LowerBoundAsLiteral(b_),
             integer_trail_.UpperBoundAsLiteral(a_),
             integer_trail_.UpperBoundAsLiteral(b_)})) {
      return false;
    }
  }

  // Lets propagate on a and b.
  const IntegerValue min_p = integer_trail_.LowerBound(p_);
  const IntegerValue max_p = integer_trail_.UpperBound(p_);

  // We need a bit more propagation to avoid bad cases below.
  const bool zero_is_possible = min_p <= 0;
  if (!zero_is_possible) {
    if (integer_trail_.LowerBound(a_) == 0) {
      if (!enforcement_propagator_.SafeEnqueue(
              enforcement_id_, a_.GreaterOrEqual(1),
              {p_.GreaterOrEqual(1), a_.GreaterOrEqual(0)})) {
        return false;
      }
    }
    if (integer_trail_.LowerBound(b_) == 0) {
      if (!enforcement_propagator_.SafeEnqueue(
              enforcement_id_, b_.GreaterOrEqual(1),
              {p_.GreaterOrEqual(1), b_.GreaterOrEqual(0)})) {
        return false;
      }
    }
    if (integer_trail_.LowerBound(a_) >= 0 &&
        integer_trail_.LowerBound(b_) <= 0) {
      return enforcement_propagator_.SafeEnqueue(
          enforcement_id_, b_.GreaterOrEqual(1),
          {a_.GreaterOrEqual(0), p_.GreaterOrEqual(1)});
    }
    if (integer_trail_.LowerBound(b_) >= 0 &&
        integer_trail_.LowerBound(a_) <= 0) {
      return enforcement_propagator_.SafeEnqueue(
          enforcement_id_, a_.GreaterOrEqual(1),
          {b_.GreaterOrEqual(0), p_.GreaterOrEqual(1)});
    }
  }

  for (int i = 0; i < 2; ++i) {
    // p = a * b, what is the min/max of a?
    const AffineExpression a = i == 0 ? a_ : b_;
    const AffineExpression b = i == 0 ? b_ : a_;
    const IntegerValue max_b = integer_trail_.UpperBound(b);
    const IntegerValue min_b = integer_trail_.LowerBound(b);

    // If the domain of b contain zero, we can't propagate anything on a.
    // Because of CanonicalizeCases(), we just deal with min_b > 0 here.
    if (zero_is_possible && min_b <= 0) continue;

    // Here both a and b are across zero, but zero is not possible.
    if (min_b < 0 && max_b > 0) {
      CHECK_GT(min_p, 0);  // Because zero is not possible.

      // If a is not across zero, we will deal with this on the next
      // Propagate() call.
      if (!PropagateMaxOnPositiveProduct(a, b, min_p, max_p)) {
        return false;
      }
      if (!PropagateMaxOnPositiveProduct(a.Negated(), b.Negated(), min_p,
                                         max_p)) {
        return false;
      }
      continue;
    }

    // This shouldn't happen here.
    // If it does, we should reach the fixed point on the next iteration.
    if (min_b <= 0) continue;
    if (min_p >= 0) {
      return enforcement_propagator_.SafeEnqueue(
          enforcement_id_, a.GreaterOrEqual(0),
          {p_.GreaterOrEqual(0), b.GreaterOrEqual(1)});
    }
    if (max_p <= 0) {
      return enforcement_propagator_.SafeEnqueue(
          enforcement_id_, a.LowerOrEqual(0),
          {p_.LowerOrEqual(0), b.GreaterOrEqual(1)});
    }

    // So min_b > 0 and p is across zero: min_p < 0 and max_p > 0.
    const IntegerValue new_max_a = FloorRatio(max_p, min_b);
    if (new_max_a < integer_trail_.UpperBound(a)) {
      if (!enforcement_propagator_.SafeEnqueue(
              enforcement_id_, a.LowerOrEqual(new_max_a),
              {integer_trail_.UpperBoundAsLiteral(p_),
               integer_trail_.LowerBoundAsLiteral(b)})) {
        return false;
      }
    }
    const IntegerValue new_min_a = CeilRatio(min_p, min_b);
    if (new_min_a > integer_trail_.LowerBound(a)) {
      if (!enforcement_propagator_.SafeEnqueue(
              enforcement_id_, a.GreaterOrEqual(new_min_a),
              {integer_trail_.LowerBoundAsLiteral(p_),
               integer_trail_.LowerBoundAsLiteral(b)})) {
        return false;
      }
    }
  }

  return true;
}

int ProductPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->WatchAffineExpression(a_, id);
  watcher->WatchAffineExpression(b_, id);
  watcher->WatchAffineExpression(p_, id);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
  return id;
}

SquarePropagator::SquarePropagator(
    absl::Span<const Literal> enforcement_literals, AffineExpression x,
    AffineExpression s, Model* model)
    : x_(x),
      s_(s),
      integer_trail_(*model->GetOrCreate<IntegerTrail>()),
      enforcement_propagator_(*model->GetOrCreate<EnforcementPropagator>()) {
  GenericLiteralWatcher* watcher = model->GetOrCreate<GenericLiteralWatcher>();
  enforcement_id_ = enforcement_propagator_.Register(
      enforcement_literals, watcher, RegisterWith(watcher));
  CHECK_GE(integer_trail_.LevelZeroLowerBound(x), 0);
}

// Propagation from x to s: s in [min_x * min_x, max_x * max_x].
// Propagation from s to x: x in [ceil(sqrt(min_s)), floor(sqrt(max_s))].
bool SquarePropagator::Propagate() {
  const IntegerValue min_x = integer_trail_.LowerBound(x_);
  const IntegerValue min_s = integer_trail_.LowerBound(s_);
  const IntegerValue min_x_square = CapProdI(min_x, min_x);
  const IntegerValue max_x = integer_trail_.UpperBound(x_);
  const IntegerValue max_s = integer_trail_.UpperBound(s_);
  const IntegerValue max_x_square = CapProdI(max_x, max_x);

  const EnforcementStatus status =
      enforcement_propagator_.Status(enforcement_id_);
  if (status == EnforcementStatus::CAN_PROPAGATE) {
    // If the bounds of x * x and s are disjoint, the enforcement must be false.
    // TODO(user): relax the reason in a better way.
    if (min_x_square > max_s) {
      return enforcement_propagator_.PropagateWhenFalse(
          enforcement_id_,
          /*literal_reason=*/{},
          {x_.GreaterOrEqual(min_x), s_.LowerOrEqual(min_x - 1)});
    }
    if (min_s > max_x_square) {
      return enforcement_propagator_.PropagateWhenFalse(
          enforcement_id_,
          /*literal_reason=*/{},
          {s_.GreaterOrEqual(min_s), x_.LowerOrEqual(min_s - 1)});
    }
    // Otherwise we cannot propagate anything since the enforcement is unknown.
    return true;
  }

  if (status != EnforcementStatus::IS_ENFORCED) return true;
  if (min_x_square > min_s) {
    if (!enforcement_propagator_.SafeEnqueue(enforcement_id_,
                                             s_.GreaterOrEqual(min_x_square),
                                             {x_.GreaterOrEqual(min_x)})) {
      return false;
    }
  } else if (min_x_square < min_s) {
    const IntegerValue new_min(CeilSquareRoot(min_s.value()));
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_, x_.GreaterOrEqual(new_min),
            {s_.GreaterOrEqual((new_min - 1) * (new_min - 1) + 1)})) {
      return false;
    }
  }
  if (max_x_square < max_s) {
    if (!enforcement_propagator_.SafeEnqueue(enforcement_id_,
                                             s_.LowerOrEqual(max_x_square),
                                             {x_.LowerOrEqual(max_x)})) {
      return false;
    }
  } else if (max_x_square > max_s) {
    const IntegerValue new_max(FloorSquareRoot(max_s.value()));
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_, x_.LowerOrEqual(new_max),
            {s_.LowerOrEqual(CapProdI(new_max + 1, new_max + 1) - 1)})) {
      return false;
    }
  }

  return true;
}

int SquarePropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->WatchAffineExpression(x_, id);
  watcher->WatchAffineExpression(s_, id);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
  return id;
}

DivisionPropagator::DivisionPropagator(
    absl::Span<const Literal> enforcement_literals, AffineExpression num,
    AffineExpression denom, AffineExpression div, Model* model)
    : num_(num),
      denom_(denom),
      div_(div),
      negated_denom_(denom.Negated()),
      negated_num_(num.Negated()),
      negated_div_(div.Negated()),
      integer_trail_(*model->GetOrCreate<IntegerTrail>()),
      enforcement_propagator_(*model->GetOrCreate<EnforcementPropagator>()) {
  GenericLiteralWatcher* watcher = model->GetOrCreate<GenericLiteralWatcher>();
  enforcement_id_ = enforcement_propagator_.Register(
      enforcement_literals, watcher, RegisterWith(watcher));
}

// TODO(user): We can propagate more, especially in the case where denom
// spans across 0.
// TODO(user): We can propagate a bit more if min_div = 0:
//     (min_num > -min_denom).
bool DivisionPropagator::Propagate() {
  if (integer_trail_.LowerBound(denom_) < 0 &&
      integer_trail_.UpperBound(denom_) > 0) {
    return true;
  }

  AffineExpression num = num_;
  AffineExpression negated_num = negated_num_;
  AffineExpression denom = denom_;
  AffineExpression negated_denom = negated_denom_;

  if (integer_trail_.UpperBound(denom) < 0) {
    std::swap(num, negated_num);
    std::swap(denom, negated_denom);
  }

  const EnforcementStatus status =
      enforcement_propagator_.Status(enforcement_id_);
  if (status == EnforcementStatus::CAN_PROPAGATE) {
    const IntegerValue min_num = integer_trail_.LowerBound(num);
    const IntegerValue max_num = integer_trail_.UpperBound(num);
    const IntegerValue min_denom = integer_trail_.LowerBound(denom);
    const IntegerValue max_denom = integer_trail_.UpperBound(denom);
    const IntegerValue min_div = integer_trail_.LowerBound(div_);
    const IntegerValue max_div = integer_trail_.UpperBound(div_);
    // If the bounds of num / denom and div are disjoint, the enforcement must
    // be false. TODO(user): relax the reason in a better way.
    if (min_num / max_denom > max_div) {
      return enforcement_propagator_.PropagateWhenFalse(
          enforcement_id_,
          /*literal_reason=*/{},
          {num_.GreaterOrEqual(min_num), denom_.LowerOrEqual(max_denom),
           div_.LowerOrEqual(max_div)});
    }
    if (max_num / min_denom < min_div) {
      return enforcement_propagator_.PropagateWhenFalse(
          enforcement_id_,
          /*literal_reason=*/{},
          {num_.LowerOrEqual(max_num), denom_.GreaterOrEqual(min_denom),
           div_.GreaterOrEqual(min_div)});
    }
    // Otherwise we cannot propagate anything since the enforcement is unknown.
    return true;
  }

  if (status != EnforcementStatus::IS_ENFORCED) return true;
  if (!PropagateSigns(num, denom, div_)) return false;

  if (integer_trail_.UpperBound(num) >= 0 &&
      integer_trail_.UpperBound(div_) >= 0 &&
      !PropagateUpperBounds(num, denom, div_)) {
    return false;
  }

  if (integer_trail_.UpperBound(negated_num) >= 0 &&
      integer_trail_.UpperBound(negated_div_) >= 0 &&
      !PropagateUpperBounds(negated_num, denom, negated_div_)) {
    return false;
  }

  if (integer_trail_.LowerBound(num) >= 0 &&
      integer_trail_.LowerBound(div_) >= 0) {
    return PropagatePositiveDomains(num, denom, div_);
  }

  if (integer_trail_.LowerBound(negated_num) >= 0 &&
      integer_trail_.LowerBound(negated_div_) >= 0) {
    return PropagatePositiveDomains(negated_num, denom, negated_div_);
  }

  return true;
}

bool DivisionPropagator::PropagateSigns(AffineExpression num,
                                        AffineExpression denom,
                                        AffineExpression div) {
  const IntegerValue min_num = integer_trail_.LowerBound(num);
  const IntegerValue max_num = integer_trail_.UpperBound(num);
  const IntegerValue min_div = integer_trail_.LowerBound(div);
  const IntegerValue max_div = integer_trail_.UpperBound(div);

  // If num >= 0, as denom > 0, then div must be >= 0.
  if (min_num >= 0 && min_div < 0) {
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_, div.GreaterOrEqual(0),
            {num.GreaterOrEqual(0), denom.GreaterOrEqual(1)})) {
      return false;
    }
  }

  // If div > 0, as denom > 0, then num must be > 0.
  if (min_num <= 0 && min_div > 0) {
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_, num.GreaterOrEqual(1),
            {div.GreaterOrEqual(1), denom.GreaterOrEqual(1)})) {
      return false;
    }
  }

  // If num <= 0, as denom > 0, then div must be <= 0.
  if (max_num <= 0 && max_div > 0) {
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_, div.LowerOrEqual(0),
            {num.LowerOrEqual(0), denom.GreaterOrEqual(1)})) {
      return false;
    }
  }

  // If div < 0, as denom > 0, then num must be < 0.
  if (max_num >= 0 && max_div < 0) {
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_, num.LowerOrEqual(-1),
            {div.LowerOrEqual(-1), denom.GreaterOrEqual(1)})) {
      return false;
    }
  }

  return true;
}

bool DivisionPropagator::PropagateUpperBounds(AffineExpression num,
                                              AffineExpression denom,
                                              AffineExpression div) {
  const IntegerValue max_num = integer_trail_.UpperBound(num);
  const IntegerValue min_denom = integer_trail_.LowerBound(denom);
  const IntegerValue max_denom = integer_trail_.UpperBound(denom);
  const IntegerValue max_div = integer_trail_.UpperBound(div);

  const IntegerValue new_max_div = max_num / min_denom;
  if (max_div > new_max_div) {
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_, div.LowerOrEqual(new_max_div),
            {num.LowerOrEqual(max_num), denom.GreaterOrEqual(min_denom)})) {
      return false;
    }
  }

  // We start from num / denom <= max_div.
  // num < (max_div + 1) * denom
  // num + 1 <= (max_div + 1) * max_denom.
  const IntegerValue new_max_num =
      CapAddI(CapProdI(max_div + 1, max_denom), -1);
  if (max_num > new_max_num) {
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_, num.LowerOrEqual(new_max_num),
            {denom.LowerOrEqual(max_denom), denom.GreaterOrEqual(1),
             div.LowerOrEqual(max_div)})) {
      return false;
    }
  }

  return true;
}

bool DivisionPropagator::PropagatePositiveDomains(AffineExpression num,
                                                  AffineExpression denom,
                                                  AffineExpression div) {
  const IntegerValue min_num = integer_trail_.LowerBound(num);
  const IntegerValue max_num = integer_trail_.UpperBound(num);
  const IntegerValue min_denom = integer_trail_.LowerBound(denom);
  const IntegerValue max_denom = integer_trail_.UpperBound(denom);
  const IntegerValue min_div = integer_trail_.LowerBound(div);
  const IntegerValue max_div = integer_trail_.UpperBound(div);

  const IntegerValue new_min_div = min_num / max_denom;
  if (min_div < new_min_div) {
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_, div.GreaterOrEqual(new_min_div),
            {num.GreaterOrEqual(min_num), denom.LowerOrEqual(max_denom),
             denom.GreaterOrEqual(1)})) {
      return false;
    }
  }

  // We start from num / denom >= min_div.
  // num >= min_div * denom.
  // num >= min_div * min_denom.
  const IntegerValue new_min_num = CapProdI(min_denom, min_div);
  if (min_num < new_min_num) {
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_, num.GreaterOrEqual(new_min_num),
            {denom.GreaterOrEqual(min_denom), div.GreaterOrEqual(min_div)})) {
      return false;
    }
  }

  // We start with num / denom >= min_div.
  // So num >= min_div * denom
  // If min_div == 0 we can't deduce anything.
  // Otherwise, denom <= num / min_div and denom <= max_num / min_div.
  if (min_div > 0) {
    const IntegerValue new_max_denom = max_num / min_div;
    if (max_denom > new_max_denom) {
      if (!enforcement_propagator_.SafeEnqueue(
              enforcement_id_, denom.LowerOrEqual(new_max_denom),
              {num.LowerOrEqual(max_num), num.GreaterOrEqual(0),
               div.GreaterOrEqual(min_div), denom.GreaterOrEqual(1)})) {
        return false;
      }
    }
  }

  // denom >= CeilRatio(num + 1, max_div + 1)
  //               >= CeilRatio(min_num + 1, max_div + 1).
  const IntegerValue new_min_denom = CeilRatio(min_num + 1, max_div + 1);
  if (min_denom < new_min_denom) {
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_, denom.GreaterOrEqual(new_min_denom),
            {num.GreaterOrEqual(min_num), div.LowerOrEqual(max_div),
             div.GreaterOrEqual(0), denom.GreaterOrEqual(1)})) {
      return false;
    }
  }

  return true;
}

int DivisionPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->WatchAffineExpression(num_, id);
  watcher->WatchAffineExpression(denom_, id);
  watcher->WatchAffineExpression(div_, id);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
  return id;
}

FixedDivisionPropagator::FixedDivisionPropagator(
    absl::Span<const Literal> enforcement_literals, AffineExpression a,
    IntegerValue b, AffineExpression c, Model* model)
    : a_(a),
      b_(b),
      c_(c),
      integer_trail_(*model->GetOrCreate<IntegerTrail>()),
      enforcement_propagator_(*model->GetOrCreate<EnforcementPropagator>()) {
  GenericLiteralWatcher* watcher = model->GetOrCreate<GenericLiteralWatcher>();
  enforcement_id_ = enforcement_propagator_.Register(
      enforcement_literals, watcher, RegisterWith(watcher));
  CHECK_GT(b_, 0);
}

bool FixedDivisionPropagator::Propagate() {
  const IntegerValue min_a = integer_trail_.LowerBound(a_);
  const IntegerValue max_a = integer_trail_.UpperBound(a_);
  IntegerValue min_c = integer_trail_.LowerBound(c_);
  IntegerValue max_c = integer_trail_.UpperBound(c_);

  const EnforcementStatus status =
      enforcement_propagator_.Status(enforcement_id_);
  if (status == EnforcementStatus::CAN_PROPAGATE) {
    // If the bounds of a / b and c are disjoint, the enforcement must be false.
    // TODO(user): relax the reason in a better way.
    if (min_a / b_ > max_c) {
      return enforcement_propagator_.PropagateWhenFalse(
          enforcement_id_,
          /*literal_reason=*/{},
          {a_.GreaterOrEqual(max_c * b_ + 1), c_.LowerOrEqual(max_c)});
    }
    if (max_a / b_ < min_c) {
      return enforcement_propagator_.PropagateWhenFalse(
          enforcement_id_,
          /*literal_reason=*/{},
          {a_.LowerOrEqual(min_c * b_ - 1), c_.GreaterOrEqual(min_c)});
    }
    // Otherwise we cannot propagate anything since the enforcement is unknown.
    return true;
  }

  if (status != EnforcementStatus::IS_ENFORCED) return true;
  if (max_a / b_ < max_c) {
    max_c = max_a / b_;
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_, c_.LowerOrEqual(max_c),
            {integer_trail_.UpperBoundAsLiteral(a_)})) {
      return false;
    }
  } else if (max_a / b_ > max_c) {
    const IntegerValue new_max_a =
        max_c >= 0 ? max_c * b_ + b_ - 1 : CapProdI(max_c, b_);
    CHECK_LT(new_max_a, max_a);
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_, a_.LowerOrEqual(new_max_a),
            {integer_trail_.UpperBoundAsLiteral(c_)})) {
      return false;
    }
  }

  if (min_a / b_ > min_c) {
    min_c = min_a / b_;
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_, c_.GreaterOrEqual(min_c),
            {integer_trail_.LowerBoundAsLiteral(a_)})) {
      return false;
    }
  } else if (min_a / b_ < min_c) {
    const IntegerValue new_min_a =
        min_c > 0 ? CapProdI(min_c, b_) : min_c * b_ - b_ + 1;
    CHECK_GT(new_min_a, min_a);
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_, a_.GreaterOrEqual(new_min_a),
            {integer_trail_.LowerBoundAsLiteral(c_)})) {
      return false;
    }
  }

  return true;
}

int FixedDivisionPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->WatchAffineExpression(a_, id);
  watcher->WatchAffineExpression(c_, id);
  return id;
}

FixedModuloPropagator::FixedModuloPropagator(
    absl::Span<const Literal> enforcement_literals, AffineExpression expr,
    IntegerValue mod, AffineExpression target, Model* model)
    : expr_(expr),
      mod_(mod),
      target_(target),
      negated_expr_(expr.Negated()),
      negated_target_(target.Negated()),
      integer_trail_(*model->GetOrCreate<IntegerTrail>()),
      enforcement_propagator_(*model->GetOrCreate<EnforcementPropagator>()) {
  CHECK_GT(mod_, 0);
  GenericLiteralWatcher* watcher = model->GetOrCreate<GenericLiteralWatcher>();
  enforcement_id_ = enforcement_propagator_.Register(
      enforcement_literals, watcher, RegisterWith(watcher));
}

bool FixedModuloPropagator::Propagate() {
  const EnforcementStatus status =
      enforcement_propagator_.Status(enforcement_id_);
  if (status == EnforcementStatus::CAN_PROPAGATE) {
    const IntegerValue min_target = integer_trail_.LowerBound(target_);
    const IntegerValue max_target = integer_trail_.UpperBound(target_);
    if (min_target >= mod_) {
      return enforcement_propagator_.PropagateWhenFalse(
          enforcement_id_, /*literal_reason=*/{},
          {target_.GreaterOrEqual(mod_)});
    } else if (max_target <= -mod_) {
      return enforcement_propagator_.PropagateWhenFalse(
          enforcement_id_, /*literal_reason=*/{},
          {target_.LowerOrEqual(-mod_)});
    }
    if (min_target > 0) {
      if (!PropagateWhenFalseAndTargetIsPositive(expr_, target_)) return false;
    } else if (max_target < 0) {
      if (!PropagateWhenFalseAndTargetIsPositive(negated_expr_,
                                                 negated_target_)) {
        return false;
      }
    } else if (!PropagateWhenFalseAndTargetDomainContainsZero()) {
      return false;
    }
    // Otherwise we cannot propagate anything since the enforcement is unknown.
    return true;
  }

  if (status != EnforcementStatus::IS_ENFORCED) return true;
  if (!PropagateSignsAndTargetRange()) return false;
  if (!PropagateOuterBounds()) return false;

  if (integer_trail_.LowerBound(expr_) >= 0) {
    if (!PropagateBoundsWhenExprIsNonNegative(expr_, target_)) return false;
  } else if (integer_trail_.UpperBound(expr_) <= 0) {
    if (!PropagateBoundsWhenExprIsNonNegative(negated_expr_, negated_target_)) {
      return false;
    }
  }

  return true;
}

bool FixedModuloPropagator::PropagateWhenFalseAndTargetIsPositive(
    AffineExpression expr, AffineExpression target) {
  const IntegerValue min_expr = integer_trail_.LowerBound(expr);
  const IntegerValue max_expr = integer_trail_.UpperBound(expr);
  // expr % mod_ must be in the target domain intersected with [0, mod_ - 1],
  // noted [min_expr_mod, max_expr_mod]. This interval is non-empty.
  const IntegerValue min_expr_mod =
      std::max(IntegerValue(0), integer_trail_.LowerBound(target));
  const IntegerValue max_expr_mod =
      std::min(mod_ - 1, integer_trail_.UpperBound(target));
  // expr must be in [min_expr_mod + k * mod_, max_expr_mod + k * mod_], for
  // some k >= 0. If the expr domain is in one of the following intervals, the
  // constraint is always false:
  // - ]-infinity, min_expr_mod[
  // - ]max_expr_mod + k * mod_ , min_expr_mod + (k + 1) * mod_[
  if (max_expr < min_expr_mod) {
    // TODO(user): relax the reason in a better way.
    return enforcement_propagator_.PropagateWhenFalse(
        enforcement_id_, /*literal_reason=*/{},
        {expr.LowerOrEqual(min_expr_mod - 1),
         target.GreaterOrEqual(min_expr_mod)});
  }
  // Compute the smallest k such that max_expr < min_expr_mod + (k + 1) * mod_.
  const IntegerValue k = MathUtil::FloorOfRatio(max_expr - min_expr_mod, mod_);
  if (min_expr > max_expr_mod + k * mod_) {
    // TODO(user): relax the reason in a better way.
    return enforcement_propagator_.PropagateWhenFalse(
        enforcement_id_, /*literal_reason=*/{},
        {expr.GreaterOrEqual(max_expr_mod + k * mod_ + 1),
         expr.LowerOrEqual(min_expr_mod + (k + 1) * mod_ - 1),
         target.GreaterOrEqual(min_expr_mod),
         target.LowerOrEqual(max_expr_mod)});
  }
  return true;
}

bool FixedModuloPropagator::PropagateWhenFalseAndTargetDomainContainsZero() {
  const IntegerValue neg_max_expr_mod =
      std::max(-mod_ + 1, integer_trail_.LowerBound(target_));
  const IntegerValue pos_max_expr_mod =
      std::min(mod_ - 1, integer_trail_.UpperBound(target_));
  // expr must be in [k * mod_, pos_max_expr_mod + k * mod_] or in
  // [neg_max_expr_mod - k * mod_, -k * mod_] for some k >= 0. If the expr
  // domain is in one of the following intervals, the constraint is always
  // false:
  // - ]-(k + 1) * mod_, neg_max_expr_mod - k * mod_[
  // - ]pos_max_expr_mod + k * mod_ , (k + 1) * mod_[
  const IntegerValue min_expr = integer_trail_.LowerBound(expr_);
  const IntegerValue max_expr = integer_trail_.UpperBound(expr_);
  // Compute the smallest k such that max_expr < (k + 1) * mod_.
  IntegerValue k = MathUtil::FloorOfRatio(max_expr, mod_);
  if (k >= 0 && min_expr > pos_max_expr_mod + k * mod_) {
    const IntegerValue min_target = integer_trail_.LowerBound(target_);
    // TODO(user): relax the reason in a better way.
    return enforcement_propagator_.PropagateWhenFalse(
        enforcement_id_, /*literal_reason=*/{},
        {expr_.GreaterOrEqual(pos_max_expr_mod + k * mod_ + 1),
         expr_.LowerOrEqual((k + 1) * mod_ - 1),
         target_.GreaterOrEqual(min_target),
         target_.LowerOrEqual(pos_max_expr_mod)});
  }
  // Compute the smallest k such that min_expr > -(k + 1) * mod_.
  k = MathUtil::FloorOfRatio(-min_expr, mod_);
  if (k >= 0 && max_expr < neg_max_expr_mod - k * mod_) {
    const IntegerValue max_target = integer_trail_.UpperBound(target_);
    // TODO(user): relax the reason in a better way.
    return enforcement_propagator_.PropagateWhenFalse(
        enforcement_id_, /*literal_reason=*/{},
        {expr_.GreaterOrEqual(-(k + 1) * mod_ + 1),
         expr_.LowerOrEqual(neg_max_expr_mod - k * mod_ - 1),
         target_.GreaterOrEqual(neg_max_expr_mod),
         target_.LowerOrEqual(max_target)});
  }
  return true;
}

bool FixedModuloPropagator::PropagateSignsAndTargetRange() {
  // Initial domain reduction on the target.
  if (integer_trail_.UpperBound(target_) >= mod_) {
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_, target_.LowerOrEqual(mod_ - 1), {})) {
      return false;
    }
  }

  if (integer_trail_.LowerBound(target_) <= -mod_) {
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_, target_.GreaterOrEqual(1 - mod_), {})) {
      return false;
    }
  }

  // The sign of target_ is fixed by the sign of expr_.
  if (integer_trail_.LowerBound(expr_) >= 0 &&
      integer_trail_.LowerBound(target_) < 0) {
    // expr >= 0 => target >= 0.
    if (!enforcement_propagator_.SafeEnqueue(enforcement_id_,
                                             target_.GreaterOrEqual(0),
                                             {expr_.GreaterOrEqual(0)})) {
      return false;
    }
  }

  if (integer_trail_.UpperBound(expr_) <= 0 &&
      integer_trail_.UpperBound(target_) > 0) {
    // expr <= 0 => target <= 0.
    if (!enforcement_propagator_.SafeEnqueue(enforcement_id_,
                                             target_.LowerOrEqual(0),
                                             {expr_.LowerOrEqual(0)})) {
      return false;
    }
  }

  if (integer_trail_.LowerBound(target_) > 0 &&
      integer_trail_.LowerBound(expr_) <= 0) {
    // target > 0 => expr > 0.
    if (!enforcement_propagator_.SafeEnqueue(enforcement_id_,
                                             expr_.GreaterOrEqual(1),
                                             {target_.GreaterOrEqual(1)})) {
      return false;
    }
  }

  if (integer_trail_.UpperBound(target_) < 0 &&
      integer_trail_.UpperBound(expr_) >= 0) {
    // target < 0 => expr < 0.
    if (!enforcement_propagator_.SafeEnqueue(enforcement_id_,
                                             expr_.LowerOrEqual(-1),
                                             {target_.LowerOrEqual(-1)})) {
      return false;
    }
  }

  return true;
}

bool FixedModuloPropagator::PropagateOuterBounds() {
  const IntegerValue min_expr = integer_trail_.LowerBound(expr_);
  const IntegerValue max_expr = integer_trail_.UpperBound(expr_);
  const IntegerValue min_target = integer_trail_.LowerBound(target_);
  const IntegerValue max_target = integer_trail_.UpperBound(target_);

  if (max_expr % mod_ > max_target) {
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_,
            expr_.LowerOrEqual((max_expr / mod_) * mod_ + max_target),
            {integer_trail_.UpperBoundAsLiteral(target_),
             integer_trail_.UpperBoundAsLiteral(expr_)})) {
      return false;
    }
  }

  if (min_expr % mod_ < min_target) {
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_,
            expr_.GreaterOrEqual((min_expr / mod_) * mod_ + min_target),
            {integer_trail_.LowerBoundAsLiteral(expr_),
             integer_trail_.LowerBoundAsLiteral(target_)})) {
      return false;
    }
  }

  if (min_expr / mod_ == max_expr / mod_) {
    if (min_target < min_expr % mod_) {
      if (!enforcement_propagator_.SafeEnqueue(
              enforcement_id_,
              target_.GreaterOrEqual(min_expr - (min_expr / mod_) * mod_),
              {integer_trail_.LowerBoundAsLiteral(target_),
               integer_trail_.UpperBoundAsLiteral(target_),
               integer_trail_.LowerBoundAsLiteral(expr_),
               integer_trail_.UpperBoundAsLiteral(expr_)})) {
        return false;
      }
    }

    if (max_target > max_expr % mod_) {
      if (!enforcement_propagator_.SafeEnqueue(
              enforcement_id_,
              target_.LowerOrEqual(max_expr - (max_expr / mod_) * mod_),
              {integer_trail_.LowerBoundAsLiteral(target_),
               integer_trail_.UpperBoundAsLiteral(target_),
               integer_trail_.LowerBoundAsLiteral(expr_),
               integer_trail_.UpperBoundAsLiteral(expr_)})) {
        return false;
      }
    }
  } else if (min_expr / mod_ == 0 && min_target < 0) {
    // expr == target when expr <= 0.
    if (min_target < min_expr) {
      if (!enforcement_propagator_.SafeEnqueue(
              enforcement_id_, target_.GreaterOrEqual(min_expr),
              {integer_trail_.LowerBoundAsLiteral(target_),
               integer_trail_.LowerBoundAsLiteral(expr_)})) {
        return false;
      }
    }
  } else if (max_expr / mod_ == 0 && max_target > 0) {
    // expr == target when expr >= 0.
    if (max_target > max_expr) {
      if (!enforcement_propagator_.SafeEnqueue(
              enforcement_id_, target_.LowerOrEqual(max_expr),
              {integer_trail_.UpperBoundAsLiteral(target_),
               integer_trail_.UpperBoundAsLiteral(expr_)})) {
        return false;
      }
    }
  }

  return true;
}

bool FixedModuloPropagator::PropagateBoundsWhenExprIsNonNegative(
    AffineExpression expr, AffineExpression target) {
  const IntegerValue min_target = integer_trail_.LowerBound(target);
  DCHECK_GE(min_target, 0);
  const IntegerValue max_target = integer_trail_.UpperBound(target);

  // The propagation rules below will not be triggered if the domain of target
  // covers [0..mod_ - 1].
  if (min_target == 0 && max_target == mod_ - 1) return true;

  const IntegerValue min_expr = integer_trail_.LowerBound(expr);
  const IntegerValue max_expr = integer_trail_.UpperBound(expr);

  if (max_expr % mod_ < min_target) {
    DCHECK_GE(max_expr, 0);
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_,
            expr.LowerOrEqual((max_expr / mod_ - 1) * mod_ + max_target),
            {integer_trail_.UpperBoundAsLiteral(expr),
             integer_trail_.LowerBoundAsLiteral(target),
             integer_trail_.UpperBoundAsLiteral(target)})) {
      return false;
    }
  }

  if (min_expr % mod_ > max_target) {
    DCHECK_GE(min_expr, 0);
    if (!enforcement_propagator_.SafeEnqueue(
            enforcement_id_,
            expr.GreaterOrEqual((min_expr / mod_ + 1) * mod_ + min_target),
            {integer_trail_.LowerBoundAsLiteral(target),
             integer_trail_.UpperBoundAsLiteral(target),
             integer_trail_.LowerBoundAsLiteral(expr)})) {
      return false;
    }
  }

  return true;
}

int FixedModuloPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->WatchAffineExpression(expr_, id);
  watcher->WatchAffineExpression(target_, id);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
  return id;
}

}  // namespace sat
}  // namespace operations_research
