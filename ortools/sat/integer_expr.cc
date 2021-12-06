// Copyright 2010-2021 Google LLC
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
#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/integer.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/time_limit.h"

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
      time_limit_(model->GetOrCreate<TimeLimit>()),
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

std::pair<IntegerValue, IntegerValue> IntegerSumLE::ConditionalLb(
    IntegerLiteral integer_literal, IntegerVariable target_var) const {
  // Recall that all our coefficient are positive.
  bool literal_var_present = false;
  bool literal_var_present_positively = false;
  IntegerValue var_coeff;

  bool target_var_present_negatively = false;
  IntegerValue target_coeff;

  // Compute the implied_lb excluding  "- target_coeff * target".
  IntegerValue implied_lb(-upper_bound_);
  for (int i = 0; i < vars_.size(); ++i) {
    const IntegerVariable var = vars_[i];
    const IntegerValue coeff = coeffs_[i];
    if (var == NegationOf(target_var)) {
      target_coeff = coeff;
      target_var_present_negatively = true;
      continue;
    }

    const IntegerValue lb = integer_trail_->LowerBound(var);
    implied_lb += coeff * lb;
    if (PositiveVariable(var) == PositiveVariable(integer_literal.var)) {
      var_coeff = coeff;
      literal_var_present = true;
      literal_var_present_positively = (var == integer_literal.var);
    }
  }
  if (!literal_var_present || !target_var_present_negatively) {
    return {kMinIntegerValue, kMinIntegerValue};
  }

  // A literal means var >= bound.
  if (literal_var_present_positively) {
    // We have var_coeff * var in the expression, the literal is var >= bound.
    // When it is false, it is not relevant as implied_lb used var >= lb.
    // When it is true, the diff is bound - lb.
    const IntegerValue diff = std::max(
        IntegerValue(0), integer_literal.bound -
                             integer_trail_->LowerBound(integer_literal.var));
    return {CeilRatio(implied_lb, target_coeff),
            CeilRatio(implied_lb + var_coeff * diff, target_coeff)};
  } else {
    // We have var_coeff * -var in the expression, the literal is var >= bound.
    // When it is true, it is not relevant as implied_lb used -var >= -ub.
    // And when it is false it means var < bound, so -var >= -bound + 1
    const IntegerValue diff = std::max(
        IntegerValue(0), integer_trail_->UpperBound(integer_literal.var) -
                             integer_literal.bound + 1);
    return {CeilRatio(implied_lb + var_coeff * diff, target_coeff),
            CeilRatio(implied_lb, target_coeff)};
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
  } else {
    rev_num_fixed_vars_ = 0;
    rev_lb_fixed_vars_ = 0;
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
  time_limit_->AdvanceDeterministicTime(
      static_cast<double>(num_vars - rev_num_fixed_vars_) * 1e-9);

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

bool IntegerSumLE::PropagateAtLevelZero() {
  // TODO(user): Deal with enforcements. It is just a bit of code to read the
  // value of the literals at level zero.
  if (!enforcement_literals_.empty()) return true;

  // Compute the new lower bound and update the reversible structures.
  IntegerValue min_activity = IntegerValue(0);
  const int num_vars = vars_.size();
  for (int i = 0; i < num_vars; ++i) {
    const IntegerVariable var = vars_[i];
    const IntegerValue coeff = coeffs_[i];
    const IntegerValue lb = integer_trail_->LevelZeroLowerBound(var);
    const IntegerValue ub = integer_trail_->LevelZeroUpperBound(var);
    max_variations_[i] = (ub - lb) * coeff;
    min_activity += lb * coeff;
  }
  time_limit_->AdvanceDeterministicTime(static_cast<double>(num_vars * 1e-9));

  // Conflict?
  const IntegerValue slack = upper_bound_ - min_activity;
  if (slack < 0) {
    return integer_trail_->ReportConflict({}, {});
  }

  // The lower bound of all the variables except one can be used to update the
  // upper bound of the last one.
  for (int i = 0; i < num_vars; ++i) {
    if (max_variations_[i] <= slack) continue;

    const IntegerVariable var = vars_[i];
    const IntegerValue coeff = coeffs_[i];
    const IntegerValue div = slack / coeff;
    const IntegerValue new_ub = integer_trail_->LevelZeroLowerBound(var) + div;
    if (!integer_trail_->Enqueue(IntegerLiteral::LowerOrEqual(var, new_ub), {},
                                 {})) {
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

LinMinPropagator::LinMinPropagator(const std::vector<LinearExpression>& exprs,
                                   IntegerVariable min_var, Model* model)
    : exprs_(exprs),
      min_var_(min_var),
      model_(model),
      integer_trail_(model_->GetOrCreate<IntegerTrail>()) {}

bool LinMinPropagator::PropagateLinearUpperBound(
    const std::vector<IntegerVariable>& vars,
    const std::vector<IntegerValue>& coeffs, const IntegerValue upper_bound) {
  IntegerValue sum_lb = IntegerValue(0);
  const int num_vars = vars.size();
  std::vector<IntegerValue> max_variations;
  for (int i = 0; i < num_vars; ++i) {
    const IntegerVariable var = vars[i];
    const IntegerValue coeff = coeffs[i];
    // The coefficients are assumed to be positive for this to work properly.
    DCHECK_GE(coeff, 0);
    const IntegerValue lb = integer_trail_->LowerBound(var);
    const IntegerValue ub = integer_trail_->UpperBound(var);
    max_variations.push_back((ub - lb) * coeff);
    sum_lb += lb * coeff;
  }

  model_->GetOrCreate<TimeLimit>()->AdvanceDeterministicTime(
      static_cast<double>(num_vars) * 1e-9);

  const IntegerValue slack = upper_bound - sum_lb;

  std::vector<IntegerLiteral> linear_sum_reason;
  std::vector<IntegerValue> reason_coeffs;
  for (int i = 0; i < num_vars; ++i) {
    const IntegerVariable var = vars[i];
    if (!integer_trail_->VariableLowerBoundIsFromLevelZero(var)) {
      linear_sum_reason.push_back(integer_trail_->LowerBoundAsLiteral(var));
      reason_coeffs.push_back(coeffs[i]);
    }
  }
  if (slack < 0) {
    // Conflict.
    integer_trail_->RelaxLinearReason(-slack - 1, reason_coeffs,
                                      &linear_sum_reason);
    std::vector<IntegerLiteral> local_reason =
        integer_reason_for_unique_candidate_;
    local_reason.insert(local_reason.end(), linear_sum_reason.begin(),
                        linear_sum_reason.end());
    return integer_trail_->ReportConflict({}, local_reason);
  }

  // The lower bound of all the variables except one can be used to update the
  // upper bound of the last one.
  for (int i = 0; i < num_vars; ++i) {
    if (max_variations[i] <= slack) continue;

    const IntegerVariable var = vars[i];
    const IntegerValue coeff = coeffs[i];
    const IntegerValue div = slack / coeff;
    const IntegerValue new_ub = integer_trail_->LowerBound(var) + div;

    const IntegerValue propagation_slack = (div + 1) * coeff - slack - 1;
    if (!integer_trail_->Enqueue(
            IntegerLiteral::LowerOrEqual(var, new_ub),
            /*lazy_reason=*/[this, &vars, &coeffs, propagation_slack](
                                IntegerLiteral i_lit, int trail_index,
                                std::vector<Literal>* literal_reason,
                                std::vector<int>* trail_indices_reason) {
              literal_reason->clear();
              trail_indices_reason->clear();
              std::vector<IntegerValue> reason_coeffs;
              const int size = vars.size();
              for (int i = 0; i < size; ++i) {
                const IntegerVariable var = vars[i];
                if (PositiveVariable(var) == PositiveVariable(i_lit.var)) {
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
                integer_trail_->RelaxLinearReason(
                    propagation_slack, reason_coeffs, trail_indices_reason);
              }
              // Now add the old integer_reason that triggered this propatation.
              for (IntegerLiteral reason_lit :
                   integer_reason_for_unique_candidate_) {
                const int index = integer_trail_->FindTrailIndexOfVarBefore(
                    reason_lit.var, trail_index);
                if (index >= 0) {
                  trail_indices_reason->push_back(index);
                }
              }
            })) {
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
    const IntegerValue lb = LinExprLowerBound(exprs_[i], *integer_trail_);
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
    std::vector<IntegerLiteral> local_reason;
    for (int i = 0; i < exprs_.size(); ++i) {
      const IntegerValue slack = expr_lbs_[i] - min_of_linear_expression_lb;
      integer_trail_->AppendRelaxedLinearReason(slack, exprs_[i].coeffs,
                                                exprs_[i].vars, &local_reason);
    }
    if (!integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(
                                     min_var_, min_of_linear_expression_lb),
                                 {}, local_reason)) {
      return false;
    }
  }

  // Propagation b) ub(min) >= ub(MIN(exprs)) and we can't propagate anything
  // here unless there is just one possible expression 'e' that can be the min:
  //   for all u != e, lb(u) > ub(min);
  // In this case, ub(min) >= ub(e).
  if (num_intervals_that_can_be_min == 1) {
    const IntegerValue ub_of_only_candidate =
        LinExprUpperBound(exprs_[last_possible_min_interval], *integer_trail_);
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
          exprs_[last_possible_min_interval].vars,
          exprs_[last_possible_min_interval].coeffs,
          current_min_ub - exprs_[last_possible_min_interval].offset);
    }
  }

  return true;
}

void LinMinPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (const LinearExpression& expr : exprs_) {
    for (int i = 0; i < expr.vars.size(); ++i) {
      const IntegerVariable& var = expr.vars[i];
      const IntegerValue coeff = expr.coeffs[i];
      if (coeff > 0) {
        watcher->WatchLowerBound(var, id);
      } else {
        watcher->WatchUpperBound(var, id);
      }
    }
  }
  watcher->WatchUpperBound(min_var_, id);
  watcher->RegisterReversibleInt(id, &rev_unique_candidate_);
}

ProductPropagator::ProductPropagator(AffineExpression a, AffineExpression b,
                                     AffineExpression p,
                                     IntegerTrail* integer_trail)
    : a_(a), b_(b), p_(p), integer_trail_(integer_trail) {}

// We want all affine expression to be either non-negative or across zero.
bool ProductPropagator::CanonicalizeCases() {
  if (integer_trail_->UpperBound(a_) <= 0) {
    a_ = a_.Negated();
    p_ = p_.Negated();
  }
  if (integer_trail_->UpperBound(b_) <= 0) {
    b_ = b_.Negated();
    p_ = p_.Negated();
  }

  // If both a and b positive, p must be too.
  if (integer_trail_->LowerBound(a_) >= 0 &&
      integer_trail_->LowerBound(b_) >= 0) {
    return integer_trail_->SafeEnqueue(
        p_.GreaterOrEqual(0), {a_.GreaterOrEqual(0), b_.GreaterOrEqual(0)});
  }

  // Otherwise, make sure p is non-negative or accros zero.
  if (integer_trail_->UpperBound(p_) <= 0) {
    if (integer_trail_->LowerBound(a_) < 0) {
      DCHECK_GT(integer_trail_->UpperBound(a_), 0);
      a_ = a_.Negated();
      p_ = p_.Negated();
    } else {
      DCHECK_LT(integer_trail_->LowerBound(b_), 0);
      DCHECK_GT(integer_trail_->UpperBound(b_), 0);
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
  const IntegerValue max_a = integer_trail_->UpperBound(a_);
  const IntegerValue max_b = integer_trail_->UpperBound(b_);
  const IntegerValue new_max(CapProd(max_a.value(), max_b.value()));
  if (new_max < integer_trail_->UpperBound(p_)) {
    if (!integer_trail_->SafeEnqueue(
            p_.LowerOrEqual(new_max),
            {integer_trail_->UpperBoundAsLiteral(a_),
             integer_trail_->UpperBoundAsLiteral(b_), a_.GreaterOrEqual(0),
             b_.GreaterOrEqual(0)})) {
      return false;
    }
  }

  const IntegerValue min_a = integer_trail_->LowerBound(a_);
  const IntegerValue min_b = integer_trail_->LowerBound(b_);
  const IntegerValue new_min(CapProd(min_a.value(), min_b.value()));
  if (new_min > integer_trail_->LowerBound(p_)) {
    if (!integer_trail_->SafeEnqueue(
            p_.GreaterOrEqual(new_min),
            {integer_trail_->LowerBoundAsLiteral(a_),
             integer_trail_->LowerBoundAsLiteral(b_)})) {
      return false;
    }
  }

  for (int i = 0; i < 2; ++i) {
    const AffineExpression a = i == 0 ? a_ : b_;
    const AffineExpression b = i == 0 ? b_ : a_;
    const IntegerValue max_a = integer_trail_->UpperBound(a);
    const IntegerValue min_b = integer_trail_->LowerBound(b);
    const IntegerValue min_p = integer_trail_->LowerBound(p_);
    const IntegerValue max_p = integer_trail_->UpperBound(p_);
    const IntegerValue prod(CapProd(max_a.value(), min_b.value()));
    if (prod > max_p) {
      if (!integer_trail_->SafeEnqueue(a.LowerOrEqual(FloorRatio(max_p, min_b)),
                                       {integer_trail_->LowerBoundAsLiteral(b),
                                        integer_trail_->UpperBoundAsLiteral(p_),
                                        p_.GreaterOrEqual(0)})) {
        return false;
      }
    } else if (prod < min_p) {
      if (!integer_trail_->SafeEnqueue(
              b.GreaterOrEqual(CeilRatio(min_p, max_a)),
              {integer_trail_->UpperBoundAsLiteral(a),
               integer_trail_->LowerBoundAsLiteral(p_), a.GreaterOrEqual(0)})) {
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
  const IntegerValue max_a = integer_trail_->UpperBound(a);
  if (max_a <= 0) return true;
  DCHECK_GT(min_p, 0);

  if (max_a >= min_p) {
    if (max_p < max_a) {
      if (!integer_trail_->SafeEnqueue(
              a.LowerOrEqual(max_p),
              {p_.LowerOrEqual(max_p), p_.GreaterOrEqual(1)})) {
        return false;
      }
    }
    return true;
  }

  const IntegerValue min_pos_b = CeilRatio(min_p, max_a);
  if (min_pos_b > integer_trail_->UpperBound(b)) {
    if (!integer_trail_->SafeEnqueue(
            b.LowerOrEqual(0), {integer_trail_->LowerBoundAsLiteral(p_),
                                integer_trail_->UpperBoundAsLiteral(a),
                                integer_trail_->UpperBoundAsLiteral(b)})) {
      return false;
    }
    return true;
  }

  const IntegerValue new_max_a = FloorRatio(max_p, min_pos_b);
  if (new_max_a < integer_trail_->UpperBound(a)) {
    if (!integer_trail_->SafeEnqueue(
            a.LowerOrEqual(new_max_a),
            {integer_trail_->LowerBoundAsLiteral(p_),
             integer_trail_->UpperBoundAsLiteral(a),
             integer_trail_->UpperBoundAsLiteral(p_)})) {
      return false;
    }
  }
  return true;
}

bool ProductPropagator::Propagate() {
  if (!CanonicalizeCases()) return false;

  // In the most common case, we use better reasons even though the code
  // below would propagate the same.
  const int64_t min_a = integer_trail_->LowerBound(a_).value();
  const int64_t min_b = integer_trail_->LowerBound(b_).value();
  if (min_a >= 0 && min_b >= 0) {
    // This was done by CanonicalizeCases().
    DCHECK_GE(integer_trail_->LowerBound(p_), 0);
    return PropagateWhenAllNonNegative();
  }

  // Lets propagate on p_ first, the max/min is given by one of: max_a * max_b,
  // max_a * min_b, min_a * max_b, min_a * min_b. This is true, because any
  // product x * y, depending on the sign, is dominated by one of these.
  //
  // TODO(user): In the reasons, including all 4 bounds is always correct, but
  // we might be able to relax some of them.
  const int64_t max_a = integer_trail_->UpperBound(a_).value();
  const int64_t max_b = integer_trail_->UpperBound(b_).value();
  const IntegerValue p1(CapProd(max_a, max_b));
  const IntegerValue p2(CapProd(max_a, min_b));
  const IntegerValue p3(CapProd(min_a, max_b));
  const IntegerValue p4(CapProd(min_a, min_b));
  const IntegerValue new_max_p = std::max({p1, p2, p3, p4});
  if (new_max_p < integer_trail_->UpperBound(p_)) {
    if (!integer_trail_->SafeEnqueue(
            p_.LowerOrEqual(new_max_p),
            {integer_trail_->LowerBoundAsLiteral(a_),
             integer_trail_->LowerBoundAsLiteral(b_),
             integer_trail_->UpperBoundAsLiteral(a_),
             integer_trail_->UpperBoundAsLiteral(b_)})) {
      return false;
    }
  }
  const IntegerValue new_min_p = std::min({p1, p2, p3, p4});
  if (new_min_p > integer_trail_->LowerBound(p_)) {
    if (!integer_trail_->SafeEnqueue(
            p_.GreaterOrEqual(new_min_p),
            {integer_trail_->LowerBoundAsLiteral(a_),
             integer_trail_->LowerBoundAsLiteral(b_),
             integer_trail_->UpperBoundAsLiteral(a_),
             integer_trail_->UpperBoundAsLiteral(b_)})) {
      return false;
    }
  }

  // Lets propagate on a and b.
  const IntegerValue min_p = integer_trail_->LowerBound(p_);
  const IntegerValue max_p = integer_trail_->UpperBound(p_);

  // We need a bit more propagation to avoid bad cases below.
  const bool zero_is_possible = min_p <= 0;
  if (!zero_is_possible) {
    if (integer_trail_->LowerBound(a_) == 0) {
      if (!integer_trail_->SafeEnqueue(
              a_.GreaterOrEqual(1),
              {p_.GreaterOrEqual(1), a_.GreaterOrEqual(0)})) {
        return false;
      }
    }
    if (integer_trail_->LowerBound(b_) == 0) {
      if (!integer_trail_->SafeEnqueue(
              b_.GreaterOrEqual(1),
              {p_.GreaterOrEqual(1), b_.GreaterOrEqual(0)})) {
        return false;
      }
    }
    if (integer_trail_->LowerBound(a_) >= 0 &&
        integer_trail_->LowerBound(b_) <= 0) {
      return integer_trail_->SafeEnqueue(
          b_.GreaterOrEqual(1), {a_.GreaterOrEqual(0), p_.GreaterOrEqual(1)});
    }
    if (integer_trail_->LowerBound(b_) >= 0 &&
        integer_trail_->LowerBound(a_) <= 0) {
      return integer_trail_->SafeEnqueue(
          a_.GreaterOrEqual(1), {b_.GreaterOrEqual(0), p_.GreaterOrEqual(1)});
    }
  }

  for (int i = 0; i < 2; ++i) {
    // p = a * b, what is the min/max of a?
    const AffineExpression a = i == 0 ? a_ : b_;
    const AffineExpression b = i == 0 ? b_ : a_;
    const IntegerValue max_b = integer_trail_->UpperBound(b);
    const IntegerValue min_b = integer_trail_->LowerBound(b);

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
      return integer_trail_->SafeEnqueue(
          a.GreaterOrEqual(0), {p_.GreaterOrEqual(0), b.GreaterOrEqual(1)});
    }
    if (max_p <= 0) {
      return integer_trail_->SafeEnqueue(
          a.LowerOrEqual(0), {p_.LowerOrEqual(0), b.GreaterOrEqual(1)});
    }

    // So min_b > 0 and p is across zero: min_p < 0 and max_p > 0.
    const IntegerValue new_max_a = FloorRatio(max_p, min_b);
    if (new_max_a < integer_trail_->UpperBound(a)) {
      if (!integer_trail_->SafeEnqueue(
              a.LowerOrEqual(new_max_a),
              {integer_trail_->UpperBoundAsLiteral(p_),
               integer_trail_->LowerBoundAsLiteral(b)})) {
        return false;
      }
    }
    const IntegerValue new_min_a = CeilRatio(min_p, min_b);
    if (new_min_a > integer_trail_->LowerBound(a)) {
      if (!integer_trail_->SafeEnqueue(
              a.GreaterOrEqual(new_min_a),
              {integer_trail_->LowerBoundAsLiteral(p_),
               integer_trail_->LowerBoundAsLiteral(b)})) {
        return false;
      }
    }
  }

  return true;
}

void ProductPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->WatchAffineExpression(a_, id);
  watcher->WatchAffineExpression(b_, id);
  watcher->WatchAffineExpression(p_, id);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
}

namespace {

// TODO(user): Find better implementation? In pratice passing via double is
// almost always correct, but the CapProd() might be a bit slow. However this
// is only called when we do propagate something.
IntegerValue FloorSquareRoot(IntegerValue a) {
  IntegerValue result(static_cast<int64_t>(
      std::floor(std::sqrt(static_cast<double>(a.value())))));
  while (CapProd(result.value(), result.value()) > a) --result;
  while (CapProd(result.value() + 1, result.value() + 1) <= a) ++result;
  return result;
}

// TODO(user): Find better implementation?
IntegerValue CeilSquareRoot(IntegerValue a) {
  IntegerValue result(static_cast<int64_t>(
      std::ceil(std::sqrt(static_cast<double>(a.value())))));
  while (CapProd(result.value(), result.value()) < a) ++result;
  while ((result.value() - 1) * (result.value() - 1) >= a) --result;
  return result;
}

}  // namespace

SquarePropagator::SquarePropagator(AffineExpression x, AffineExpression s,
                                   IntegerTrail* integer_trail)
    : x_(x), s_(s), integer_trail_(integer_trail) {
  CHECK_GE(integer_trail->LevelZeroLowerBound(x), 0);
}

// Propagation from x to s: s in [min_x * min_x, max_x * max_x].
// Propagation from s to x: x in [ceil(sqrt(min_s)), floor(sqrt(max_s))].
bool SquarePropagator::Propagate() {
  const IntegerValue min_x = integer_trail_->LowerBound(x_);
  const IntegerValue min_s = integer_trail_->LowerBound(s_);
  const IntegerValue min_x_square(CapProd(min_x.value(), min_x.value()));
  if (min_x_square > min_s) {
    if (!integer_trail_->SafeEnqueue(s_.GreaterOrEqual(min_x_square),
                                     {x_.GreaterOrEqual(min_x)})) {
      return false;
    }
  } else if (min_x_square < min_s) {
    const IntegerValue new_min = CeilSquareRoot(min_s);
    if (!integer_trail_->SafeEnqueue(
            x_.GreaterOrEqual(new_min),
            {s_.GreaterOrEqual((new_min - 1) * (new_min - 1) + 1)})) {
      return false;
    }
  }

  const IntegerValue max_x = integer_trail_->UpperBound(x_);
  const IntegerValue max_s = integer_trail_->UpperBound(s_);
  const IntegerValue max_x_square(CapProd(max_x.value(), max_x.value()));
  if (max_x_square < max_s) {
    if (!integer_trail_->SafeEnqueue(s_.LowerOrEqual(max_x_square),
                                     {x_.LowerOrEqual(max_x)})) {
      return false;
    }
  } else if (max_x_square > max_s) {
    const IntegerValue new_max = FloorSquareRoot(max_s);
    if (!integer_trail_->SafeEnqueue(
            x_.LowerOrEqual(new_max),
            {s_.LowerOrEqual(IntegerValue(CapProd(new_max.value() + 1,
                                                  new_max.value() + 1)) -
                             1)})) {
      return false;
    }
  }

  return true;
}

void SquarePropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->WatchAffineExpression(x_, id);
  watcher->WatchAffineExpression(s_, id);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
}

DivisionPropagator::DivisionPropagator(AffineExpression num,
                                       AffineExpression denom,
                                       AffineExpression div,
                                       IntegerTrail* integer_trail)
    : num_(num),
      denom_(denom),
      div_(div),
      negated_num_(num.Negated()),
      negated_div_(div.Negated()),
      integer_trail_(integer_trail) {
  // The denominator can never be zero.
  CHECK_GT(integer_trail->LevelZeroLowerBound(denom), 0);
}

bool DivisionPropagator::Propagate() {
  if (!PropagateSigns()) return false;

  if (integer_trail_->UpperBound(num_) >= 0 &&
      integer_trail_->UpperBound(div_) >= 0 &&
      !PropagateUpperBounds(num_, denom_, div_)) {
    return false;
  }

  if (integer_trail_->UpperBound(negated_num_) >= 0 &&
      integer_trail_->UpperBound(negated_div_) >= 0 &&
      !PropagateUpperBounds(negated_num_, denom_, negated_div_)) {
    return false;
  }

  if (integer_trail_->LowerBound(num_) >= 0 &&
      integer_trail_->LowerBound(div_) >= 0) {
    return PropagatePositiveDomains(num_, denom_, div_);
  }

  if (integer_trail_->UpperBound(num_) <= 0 &&
      integer_trail_->UpperBound(div_) <= 0) {
    return PropagatePositiveDomains(negated_num_, denom_, negated_div_);
  }

  return true;
}

bool DivisionPropagator::PropagateSigns() {
  const IntegerValue min_num = integer_trail_->LowerBound(num_);
  const IntegerValue max_num = integer_trail_->UpperBound(num_);
  const IntegerValue min_div = integer_trail_->LowerBound(div_);
  const IntegerValue max_div = integer_trail_->UpperBound(div_);

  // If num >= 0, as denom > 0, then div must be >= 0.
  if (min_num >= 0 && min_div < 0) {
    if (!integer_trail_->SafeEnqueue(div_.GreaterOrEqual(0),
                                     {num_.GreaterOrEqual(0)})) {
      return false;
    }
  }

  // If div > 0, as denom > 0, then num must be > 0.
  if (min_num <= 0 && min_div > 0) {
    if (!integer_trail_->SafeEnqueue(num_.GreaterOrEqual(1),
                                     {div_.GreaterOrEqual(1)})) {
      return false;
    }
  }

  // If num <= 0, as denom > 0, then div must be <= 0.
  if (max_num <= 0 && max_div > 0) {
    if (!integer_trail_->SafeEnqueue(div_.LowerOrEqual(0),
                                     {num_.LowerOrEqual(0)})) {
      return false;
    }
  }

  // If div < 0, as denom > 0, then num must be < 0.
  if (max_num >= 0 && max_div < 0) {
    if (!integer_trail_->SafeEnqueue(num_.LowerOrEqual(-1),
                                     {div_.LowerOrEqual(-1)})) {
      return false;
    }
  }

  return true;
}

bool DivisionPropagator::PropagateUpperBounds(AffineExpression num,
                                              AffineExpression denom,
                                              AffineExpression div) {
  const IntegerValue max_num = integer_trail_->UpperBound(num);
  const IntegerValue min_denom = integer_trail_->LowerBound(denom);
  const IntegerValue max_denom = integer_trail_->UpperBound(denom);
  const IntegerValue max_div = integer_trail_->UpperBound(div);

  const IntegerValue new_max_div = max_num / min_denom;
  if (max_div > new_max_div) {
    if (!integer_trail_->SafeEnqueue(
            div.LowerOrEqual(new_max_div),
            {integer_trail_->UpperBoundAsLiteral(num),
             integer_trail_->LowerBoundAsLiteral(denom)})) {
      return false;
    }
  }

  // We start from num / denom <= max_div.
  // num < (max_div + 1) * denom
  // num + 1 <= (max_div + 1) * max_denom.
  const IntegerValue new_max_num =
      IntegerValue(CapAdd(CapProd(max_div.value() + 1, max_denom.value()), -1));
  if (max_num > new_max_num) {
    if (!integer_trail_->SafeEnqueue(
            num.LowerOrEqual(new_max_num),
            {integer_trail_->UpperBoundAsLiteral(denom),
             integer_trail_->UpperBoundAsLiteral(div)})) {
      return false;
    }
  }

  return true;
}

bool DivisionPropagator::PropagatePositiveDomains(AffineExpression num,
                                                  AffineExpression denom,
                                                  AffineExpression div) {
  const IntegerValue min_num = integer_trail_->LowerBound(num);
  const IntegerValue max_num = integer_trail_->UpperBound(num);
  const IntegerValue min_denom = integer_trail_->LowerBound(denom);
  const IntegerValue max_denom = integer_trail_->UpperBound(denom);
  const IntegerValue min_div = integer_trail_->LowerBound(div);
  const IntegerValue max_div = integer_trail_->UpperBound(div);

  const IntegerValue new_min_div = min_num / max_denom;
  if (min_div < new_min_div) {
    if (!integer_trail_->SafeEnqueue(
            div.GreaterOrEqual(new_min_div),
            {integer_trail_->LowerBoundAsLiteral(num),
             integer_trail_->UpperBoundAsLiteral(denom)})) {
      return false;
    }
  }

  // We start from num / denom >= min_div.
  // num >= min_div * denom.
  // num >= min_div * min_denom.
  const IntegerValue new_min_num =
      IntegerValue(CapProd(min_denom.value(), min_div.value()));
  if (min_num < new_min_num) {
    if (!integer_trail_->SafeEnqueue(
            num.GreaterOrEqual(new_min_num),
            {integer_trail_->LowerBoundAsLiteral(denom),
             integer_trail_->LowerBoundAsLiteral(div)})) {
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
      if (!integer_trail_->SafeEnqueue(
              denom.LowerOrEqual(new_max_denom),
              {integer_trail_->UpperBoundAsLiteral(num), num.GreaterOrEqual(0),
               integer_trail_->LowerBoundAsLiteral(div)})) {
        return false;
      }
    }
  }

  // denom >= CeilRatio(num + 1, max_div+1)
  //               >= CeilRatio(min_num + 1, max_div +).
  const IntegerValue new_min_denom = CeilRatio(min_num + 1, max_div + 1);
  if (min_denom < new_min_denom) {
    if (!integer_trail_->SafeEnqueue(denom.GreaterOrEqual(new_min_denom),
                                     {integer_trail_->LowerBoundAsLiteral(num),
                                      integer_trail_->UpperBoundAsLiteral(div),
                                      div.GreaterOrEqual(0)})) {
      return false;
    }
  }

  return true;
}

void DivisionPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->WatchAffineExpression(num_, id);
  watcher->WatchAffineExpression(denom_, id);
  watcher->WatchAffineExpression(div_, id);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
}

FixedDivisionPropagator::FixedDivisionPropagator(AffineExpression a,
                                                 IntegerValue b,
                                                 AffineExpression c,
                                                 IntegerTrail* integer_trail)
    : a_(a), b_(b), c_(c), integer_trail_(integer_trail) {
  CHECK_GT(b_, 0);
}

bool FixedDivisionPropagator::Propagate() {
  const IntegerValue min_a = integer_trail_->LowerBound(a_);
  const IntegerValue max_a = integer_trail_->UpperBound(a_);
  IntegerValue min_c = integer_trail_->LowerBound(c_);
  IntegerValue max_c = integer_trail_->UpperBound(c_);

  if (max_a / b_ < max_c) {
    max_c = max_a / b_;
    if (!integer_trail_->SafeEnqueue(
            c_.LowerOrEqual(max_c),
            {integer_trail_->UpperBoundAsLiteral(a_)})) {
      return false;
    }
  } else if (max_a / b_ > max_c) {
    const IntegerValue new_max_a =
        max_c >= 0 ? max_c * b_ + b_ - 1
                   : IntegerValue(CapProd(max_c.value(), b_.value()));
    CHECK_LT(new_max_a, max_a);
    if (!integer_trail_->SafeEnqueue(
            a_.LowerOrEqual(new_max_a),
            {integer_trail_->UpperBoundAsLiteral(c_)})) {
      return false;
    }
  }

  if (min_a / b_ > min_c) {
    min_c = min_a / b_;
    if (!integer_trail_->SafeEnqueue(
            c_.GreaterOrEqual(min_c),
            {integer_trail_->LowerBoundAsLiteral(a_)})) {
      return false;
    }
  } else if (min_a / b_ < min_c) {
    const IntegerValue new_min_a =
        min_c > 0 ? IntegerValue(CapProd(min_c.value(), b_.value()))
                  : min_c * b_ - b_ + 1;
    CHECK_GT(new_min_a, min_a);
    if (!integer_trail_->SafeEnqueue(
            a_.GreaterOrEqual(new_min_a),
            {integer_trail_->LowerBoundAsLiteral(c_)})) {
      return false;
    }
  }

  return true;
}

void FixedDivisionPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->WatchAffineExpression(a_, id);
  watcher->WatchAffineExpression(c_, id);
}

FixedModuloPropagator::FixedModuloPropagator(AffineExpression expr,
                                             IntegerValue mod,
                                             AffineExpression target,
                                             IntegerTrail* integer_trail)
    : expr_(expr), mod_(mod), target_(target), integer_trail_(integer_trail) {
  CHECK_GT(mod_, 0);
}

bool FixedModuloPropagator::Propagate() {
  if (!PropagateSignsAndTargetRange()) return false;
  if (!PropagateOuterBounds()) return false;

  if (integer_trail_->LowerBound(expr_) >= 0) {
    if (!PropagateBoundsWhenExprIsPositive(expr_, target_)) return false;
  } else if (integer_trail_->UpperBound(expr_) <= 0) {
    if (!PropagateBoundsWhenExprIsPositive(expr_.Negated(),
                                           target_.Negated())) {
      return false;
    }
  }

  return true;
}

bool FixedModuloPropagator::PropagateSignsAndTargetRange() {
  // Initial domain reduction on the target.
  if (integer_trail_->UpperBound(target_) >= mod_) {
    if (!integer_trail_->SafeEnqueue(target_.LowerOrEqual(mod_ - 1), {})) {
      return false;
    }
  }

  if (integer_trail_->LowerBound(target_) <= -mod_) {
    if (!integer_trail_->SafeEnqueue(target_.GreaterOrEqual(1 - mod_), {})) {
      return false;
    }
  }

  // The sign of target_ is fixed by the sign of expr_.
  if (integer_trail_->LowerBound(expr_) >= 0 &&
      integer_trail_->LowerBound(target_) < 0) {
    if (!integer_trail_->SafeEnqueue(target_.GreaterOrEqual(0),
                                     {expr_.GreaterOrEqual(0)})) {
      return false;
    }
  }

  if (integer_trail_->UpperBound(expr_) <= 0 &&
      integer_trail_->UpperBound(target_) > 0) {
    if (!integer_trail_->SafeEnqueue(target_.LowerOrEqual(0),
                                     {expr_.LowerOrEqual(0)})) {
      return false;
    }
  }

  return true;
}

bool FixedModuloPropagator::PropagateOuterBounds() {
  const IntegerValue min_expr = integer_trail_->LowerBound(expr_);
  const IntegerValue max_expr = integer_trail_->UpperBound(expr_);
  const IntegerValue min_target = integer_trail_->LowerBound(target_);
  const IntegerValue max_target = integer_trail_->UpperBound(target_);

  if (max_expr % mod_ > max_target) {
    if (!integer_trail_->SafeEnqueue(
            expr_.LowerOrEqual((max_expr / mod_) * mod_ + max_target),
            {integer_trail_->UpperBoundAsLiteral(target_),
             integer_trail_->UpperBoundAsLiteral(expr_)})) {
      return false;
    }
  }

  if (min_expr % mod_ < min_target) {
    if (!integer_trail_->SafeEnqueue(
            expr_.GreaterOrEqual((min_expr / mod_) * mod_ + min_target),
            {integer_trail_->LowerBoundAsLiteral(expr_),
             integer_trail_->LowerBoundAsLiteral(target_)})) {
      return false;
    }
  }

  if (min_expr / mod_ == max_expr / mod_) {
    if (min_target < min_expr % mod_) {
      if (!integer_trail_->SafeEnqueue(
              target_.GreaterOrEqual(min_expr - (min_expr / mod_) * mod_),
              {integer_trail_->LowerBoundAsLiteral(target_),
               integer_trail_->UpperBoundAsLiteral(target_),
               integer_trail_->LowerBoundAsLiteral(expr_),
               integer_trail_->UpperBoundAsLiteral(expr_)})) {
        return false;
      }
    }

    if (max_target > max_expr % mod_) {
      if (!integer_trail_->SafeEnqueue(
              target_.LowerOrEqual(max_expr - (max_expr / mod_) * mod_),
              {integer_trail_->LowerBoundAsLiteral(target_),
               integer_trail_->UpperBoundAsLiteral(target_),
               integer_trail_->LowerBoundAsLiteral(expr_),
               integer_trail_->UpperBoundAsLiteral(expr_)})) {
        return false;
      }
    }
  } else if (min_expr / mod_ == 0 && min_target < 0) {
    // expr == target when expr <= 0.
    if (min_target < min_expr) {
      if (!integer_trail_->SafeEnqueue(
              target_.GreaterOrEqual(min_expr),
              {integer_trail_->LowerBoundAsLiteral(target_),
               integer_trail_->LowerBoundAsLiteral(expr_)})) {
        return false;
      }
    }
  } else if (max_expr / mod_ == 0 && max_target > 0) {
    // expr == target when expr >= 0.
    if (max_target > max_expr) {
      if (!integer_trail_->SafeEnqueue(
              target_.LowerOrEqual(max_expr),
              {integer_trail_->UpperBoundAsLiteral(target_),
               integer_trail_->UpperBoundAsLiteral(expr_)})) {
        return false;
      }
    }
  }

  return true;
}

bool FixedModuloPropagator::PropagateBoundsWhenExprIsPositive(
    AffineExpression expr, AffineExpression target) {
  const IntegerValue min_target = integer_trail_->LowerBound(target);
  DCHECK_GE(min_target, 0);
  const IntegerValue max_target = integer_trail_->UpperBound(target);

  // The propagation rules below will not be triggered if the domain of target
  // covers [0..mod_ - 1].
  if (min_target == 0 && max_target == mod_ - 1) return true;

  const IntegerValue min_expr = integer_trail_->LowerBound(expr);
  const IntegerValue max_expr = integer_trail_->UpperBound(expr);

  if (max_expr % mod_ < min_target) {
    DCHECK_GE(max_expr, 0);
    if (!integer_trail_->SafeEnqueue(
            expr.LowerOrEqual((max_expr / mod_ - 1) * mod_ + max_target),
            {integer_trail_->UpperBoundAsLiteral(expr),
             integer_trail_->LowerBoundAsLiteral(target),
             integer_trail_->UpperBoundAsLiteral(target)})) {
      return false;
    }
  }

  if (min_expr % mod_ > max_target) {
    DCHECK_GE(min_expr, 0);
    if (!integer_trail_->SafeEnqueue(
            expr.GreaterOrEqual((min_expr / mod_ + 1) * mod_ + min_target),
            {integer_trail_->LowerBoundAsLiteral(target),
             integer_trail_->UpperBoundAsLiteral(target),
             integer_trail_->LowerBoundAsLiteral(expr)})) {
      return false;
    }
  }

  return true;
}

void FixedModuloPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->WatchAffineExpression(expr_, id);
  watcher->WatchAffineExpression(target_, id);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
}

std::function<void(Model*)> IsOneOf(IntegerVariable var,
                                    const std::vector<Literal>& selectors,
                                    const std::vector<IntegerValue>& values) {
  return [=](Model* model) {
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();

    CHECK(!values.empty());
    CHECK_EQ(values.size(), selectors.size());
    std::vector<int64_t> unique_values;
    absl::flat_hash_map<int64_t, std::vector<Literal>> value_to_selector;
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
    for (const int64_t v : unique_values) {
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
