// Copyright 2010-2013 Google
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
#include "sat/pb_constraint.h"

#include "util/saturated_arithmetic.h"

namespace operations_research {
namespace sat {

namespace {

bool LiteralComparator(const LiteralWithCoeff& a, const LiteralWithCoeff& b) {
  return a.literal.Index() < b.literal.Index();
}

bool CoeffComparator(const LiteralWithCoeff& a, const LiteralWithCoeff& b) {
  if (a.coefficient == b.coefficient) {
    return a.literal.Index() < b.literal.Index();
  }
  return a.coefficient < b.coefficient;
}

}  // namespace

bool ComputeBooleanLinearExpressionCanonicalForm(std::vector<LiteralWithCoeff>* cst,
                                                 Coefficient* bound_shift,
                                                 Coefficient* max_value) {
  // Note(user): For some reason, the IntType checking doesn't work here ?! that
  // is a bit worrying, but the code seems to behave correctly.
  *bound_shift = 0;
  *max_value = 0;

  // First, sort by literal to remove duplicate literals.
  // This also remove term with a zero coefficient.
  std::sort(cst->begin(), cst->end(), LiteralComparator);
  int index = 0;
  LiteralWithCoeff* representative = nullptr;
  for (int i = 0; i < cst->size(); ++i) {
    const LiteralWithCoeff current = (*cst)[i];
    if (current.coefficient == 0) continue;
    if (representative != nullptr &&
        current.literal.Variable() == representative->literal.Variable()) {
      if (current.literal == representative->literal) {
        if (!SafeAddInto(current.coefficient, &(representative->coefficient)))
          return false;
      } else {
        // Here current_literal is equal to (1 - representative).
        if (!SafeAddInto(-current.coefficient, &(representative->coefficient)))
          return false;
        if (!SafeAddInto(-current.coefficient, bound_shift)) return false;
      }
    } else {
      if (representative != nullptr && representative->coefficient == 0) {
        --index;
      }
      (*cst)[index] = current;
      representative = &((*cst)[index]);
      ++index;
    }
  }
  if (representative != nullptr && representative->coefficient == 0) {
    --index;
  }
  cst->resize(index);

  // Then, make all coefficients positive by replacing a term "-c x" into
  // "c(1-x) - c" which is the same as "c(not x) - c".
  for (int i = 0; i < cst->size(); ++i) {
    const LiteralWithCoeff current = (*cst)[i];
    if (current.coefficient < 0) {
      if (!SafeAddInto(-current.coefficient, bound_shift)) return false;
      (*cst)[i].coefficient = -current.coefficient;
      (*cst)[i].literal = current.literal.Negated();
    }
    if (!SafeAddInto((*cst)[i].coefficient, max_value)) return false;
  }

  // Finally sort by increasing coefficients.
  std::sort(cst->begin(), cst->end(), CoeffComparator);
  DCHECK_GE(*max_value, 0);
  return true;
}

// TODO(user): Also check for no duplicates literals + unit tests.
bool BooleanLinearExpressionIsCanonical(const std::vector<LiteralWithCoeff>& cst) {
  Coefficient previous(1);
  for (LiteralWithCoeff term : cst) {
    if (term.coefficient < previous) return false;
    previous = term.coefficient;
  }
  return true;
}

// TODO(user): Use more complex simplification like dividing by the gcd of
// everyone and using less different coefficients if possible.
void SimplifyCanonicalBooleanLinearConstraint(std::vector<LiteralWithCoeff>* cst,
                                              Coefficient* rhs) {
  // Replace all coefficient >= rhs by rhs + 1 (these literal must actually be
  // false). Note that the linear sum of literals remains canonical.
  //
  // TODO(user): It is probably better to remove these literals and have other
  // constraint setting them to false from the symmetry finder perspective.
  for (LiteralWithCoeff& x : *cst) {
    if (x.coefficient > *rhs) x.coefficient = *rhs + 1;
  }
}

Coefficient ComputeCanonicalRhs(Coefficient upper_bound,
                                Coefficient bound_shift,
                                Coefficient max_value) {
  Coefficient rhs = upper_bound;
  if (!SafeAddInto(bound_shift, &rhs)) {
    if (bound_shift > 0) {
      // Positive overflow. The constraint is trivially true.
      // This is because the canonical linear expression is in [0, max_value].
      return max_value;
    } else {
      // Negative overflow. The constraint is infeasible.
      return Coefficient(-1);
    }
  }
  if (rhs < 0) return Coefficient(-1);
  return std::min(max_value, rhs);
}

Coefficient ComputeNegatedCanonicalRhs(Coefficient lower_bound,
                                       Coefficient bound_shift,
                                       Coefficient max_value) {
  // The new bound is "max_value - (lower_bound + bound_shift)", but we must
  // pay attention to possible overflows.
  Coefficient shifted_lb = lower_bound;
  if (!SafeAddInto(bound_shift, &shifted_lb)) {
    if (bound_shift > 0) {
      // Positive overflow. The constraint is infeasible.
      return Coefficient(-1);
    } else {
      // Negative overflow. The constraint is trivialy satisfiable.
      return max_value;
    }
  }
  if (shifted_lb <= 0) {
    // If shifted_lb <= 0 then the constraint is trivialy satisfiable. We test
    // this so we are sure that max_value - shifted_lb doesn't overflow below.
    return max_value;
  }
  return max_value - shifted_lb;
}

bool CanonicalBooleanLinearProblem::AddLinearConstraint(
    bool use_lower_bound, Coefficient lower_bound, bool use_upper_bound,
    Coefficient upper_bound, std::vector<LiteralWithCoeff>* cst) {
  // Canonicalize the linear expression of the constraint.
  Coefficient bound_shift;
  Coefficient max_value;
  if (!ComputeBooleanLinearExpressionCanonicalForm(cst, &bound_shift,
                                                   &max_value)) {
    return false;
  }
  if (use_upper_bound) {
    const Coefficient rhs =
        ComputeCanonicalRhs(upper_bound, bound_shift, max_value);
    if (!AddConstraint(*cst, max_value, rhs)) return false;
  }
  if (use_lower_bound) {
    // We transform the constraint into an upper-bounded one.
    for (int i = 0; i < cst->size(); ++i) {
      (*cst)[i].literal = (*cst)[i].literal.Negated();
    }
    const Coefficient rhs =
        ComputeNegatedCanonicalRhs(lower_bound, bound_shift, max_value);
    if (!AddConstraint(*cst, max_value, rhs)) return false;
  }
  return true;
}

bool CanonicalBooleanLinearProblem::AddConstraint(
    const std::vector<LiteralWithCoeff>& cst, Coefficient max_value,
    Coefficient rhs) {
  if (rhs < 0) return false;          // Trivially unsatisfiable.
  if (rhs >= max_value) return true;  // Trivially satisfiable.
  constraints_.emplace_back(cst.begin(), cst.end());
  rhs_.push_back(rhs);
  SimplifyCanonicalBooleanLinearConstraint(&constraints_.back(), &rhs_.back());
  return true;
}

void MutableUpperBoundedLinearConstraint::ClearAndResize(int num_variables) {
  terms_.assign(num_variables, Coefficient(0));
  non_zeros_.ClearAndResize(VariableIndex(num_variables));
}

void MutableUpperBoundedLinearConstraint::ClearAll() {
  // TODO(user): We could be more efficient and have only one loop here.
  for (VariableIndex var : non_zeros_.PositionsSetAtLeastOnce()) {
    terms_[var] = Coefficient(0);
  }
  non_zeros_.ClearAll();
  rhs_ = 0;
}

void MutableUpperBoundedLinearConstraint::ReduceCoefficients() {
  // TODO(user): avoid the max_sum computation? we could maintain it
  // incrementally.
  Coefficient max_sum(0);
  for (VariableIndex var : PossibleNonZeros()) {
    CHECK(SafeAddInto(GetCoefficient(var), &max_sum));
  }
  CHECK_LT(rhs_, max_sum) << "Trivially sat.";
  const Coefficient bound = max_sum - rhs_;
  for (VariableIndex var : PossibleNonZeros()) {
    const Coefficient coeff = GetCoefficient(var);
    if (coeff > bound) {
      rhs_ -= coeff - bound;
      terms_[var] = (terms_[var] > 0) ? bound : -bound;
    }
  }

  // TODO(user): Also reduce the trivially false literal when coeff > rhs_ ?
}

std::string MutableUpperBoundedLinearConstraint::DebugString() {
  std::string result;
  for (VariableIndex var : PossibleNonZeros()) {
    if (!result.empty()) result += " + ";
    result += StringPrintf("%lld[%s]", GetCoefficient(var).value(),
                           GetLiteral(var).DebugString().c_str());
  }
  result += StringPrintf(" <= %lld", rhs_.value());
  return result;
}

// TODO(user): Keep this for DCHECK(), but maintain the slack incrementally
// instead of recomputing it.
Coefficient MutableUpperBoundedLinearConstraint::ComputeSlackForTrailPrefix(
    const Trail& trail, int trail_index) {
  Coefficient sum(0);
  for (VariableIndex var : PossibleNonZeros()) {
    if (GetCoefficient(var) == 0) continue;
    if (trail.Assignment().IsLiteralTrue(GetLiteral(var)) &&
        trail.Info(var).trail_index < trail_index) {
      sum += GetCoefficient(var);
      CHECK_GE(sum, 0) << "Overflow!";
    }
  }
  return rhs_ - sum;
}

void MutableUpperBoundedLinearConstraint::ReduceSlackTo(const Trail& trail,
                                                        int trail_index,
                                                        Coefficient target) {
  // Positive slack.
  const Coefficient slack = ComputeSlackForTrailPrefix(trail, trail_index);
  CHECK_LE(target, slack);
  CHECK_GE(target, 0);

  // This is not stricly needed, but true in our use case:
  // The variable assigned at trail_index was causing a conflict.
  const Coefficient coeff = GetCoefficient(trail[trail_index].Variable());
  CHECK_LT(slack, coeff);

  // Nothing to do if the slack is already target.
  if (slack == target) return;

  // Applies the algorithm described in the .h
  const Coefficient diff = slack - target;
  rhs_ -= diff;
  for (VariableIndex var : PossibleNonZeros()) {
    if (GetCoefficient(var) == 0) continue;
    if (trail.Assignment().IsLiteralTrue(GetLiteral(var)) &&
        trail.Info(var).trail_index < trail_index) {
      continue;
    }
    if (GetCoefficient(var) > diff) {
      terms_[var] = (terms_[var] > 0) ? terms_[var] - diff : terms_[var] + diff;
    } else {
      terms_[var] = 0;
    }
  }
}

void MutableUpperBoundedLinearConstraint::CopyIntoVector(
    std::vector<LiteralWithCoeff>* output) {
  output->clear();
  for (VariableIndex var : non_zeros_.PositionsSetAtLeastOnce()) {
    const Coefficient coeff = GetCoefficient(var);
    if (coeff != 0) {
      output->push_back(LiteralWithCoeff(GetLiteral(var), GetCoefficient(var)));
    }
  }
  std::sort(output->begin(), output->end(), CoeffComparator);
}

UpperBoundedLinearConstraint::UpperBoundedLinearConstraint(
    const std::vector<LiteralWithCoeff>& cst, ResolutionNode* node)
    : node_(node) {
  DCHECK(!cst.empty());
  DCHECK(std::is_sorted(cst.begin(), cst.end(), CoeffComparator));
  literals_.reserve(cst.size());
  for (LiteralWithCoeff term : cst) {
    if (term.coefficient == 0) continue;
    if (coeffs_.empty() || term.coefficient != coeffs_.back()) {
      coeffs_.push_back(term.coefficient);
      starts_.push_back(literals_.size());
    }
    literals_.push_back(term.literal);
  }

  // Sentinel.
  starts_.push_back(literals_.size());
}

void UpperBoundedLinearConstraint::AddToConflict(
    MutableUpperBoundedLinearConstraint* conflict) {
  int literal_index = 0;
  int coeff_index = 0;
  for (Literal literal : literals_) {
    conflict->AddTerm(literal, coeffs_[coeff_index]);
    ++literal_index;
    if (literal_index == starts_[coeff_index + 1]) ++coeff_index;
  }
  conflict->AddToRhs(rhs_);
}

bool UpperBoundedLinearConstraint::HasIdenticalTerms(
    const std::vector<LiteralWithCoeff>& cst) {
  if (cst.size() != literals_.size()) return false;
  int literal_index = 0;
  int coeff_index = 0;
  for (LiteralWithCoeff term : cst) {
    if (literals_[literal_index] != term.literal) return false;
    if (coeffs_[coeff_index] != term.coefficient) return false;
    ++literal_index;
    if (literal_index == starts_[coeff_index + 1]) {
      ++coeff_index;
    }
  }
  return true;
}

bool UpperBoundedLinearConstraint::InitializeRhs(Coefficient rhs,
                                                 int trail_index,
                                                 Coefficient* threshold,
                                                 Trail* trail,
                                                 std::vector<Literal>* conflict) {
  rhs_ = rhs;

  // Compute the slack from the assigned variables with a trail index
  // smaller than the given trail_index. The variable at trail_index has not
  // yet been propagated.
  Coefficient slack = rhs;
  if (trail_index > 0) {
    const int last_level = trail->CurrentDecisionLevel();

    // sum_at_previous_level[i] is the sum of assigned literals with a level <
    // i. Since we want the sums up to sum_at_previous_level[last_level + 1],
    // the size of the vector must be last_level + 2.
    std::vector<Coefficient> sum_at_previous_level(last_level + 2, Coefficient(0));

    int literal_index = 0;
    int coeff_index = 0;
    for (Literal literal : literals_) {
      const VariableIndex var = literal.Variable();
      const Coefficient coeff = coeffs_[coeff_index];
      if (trail->Assignment().IsLiteralTrue(literal) &&
          trail->Info(var).trail_index < trail_index) {
        slack -= coeff;
        sum_at_previous_level[trail->Info(var).level + 1] += coeff;
      }
      ++literal_index;
      if (literal_index == starts_[coeff_index + 1]) ++coeff_index;
    }

    // The constraint is infeasible provided the current propagated trail.
    if (slack < 0) return false;

    // Cummulative sum.
    for (int i = 1; i < sum_at_previous_level.size(); ++i) {
      sum_at_previous_level[i] += sum_at_previous_level[i - 1];
    }

    // Check the no-propagation at earlier level precondition.
    literal_index = 0;
    coeff_index = 0;
    for (Literal literal : literals_) {
      const VariableIndex var = literal.Variable();
      const int level = trail->Assignment().IsVariableAssigned(var)
                            ? trail->Info(var).level
                            : last_level;
      if (coeffs_[coeff_index] > rhs_ - sum_at_previous_level[level]) {
        // This variable should have been propagated at an earlier level.
        return false;
      }
      ++literal_index;
      if (literal_index == starts_[coeff_index + 1]) ++coeff_index;
    }
  }

  // Initial propagation.
  index_ = coeffs_.size() - 1;
  already_propagated_end_ = literals_.size();
  Update(slack, threshold);
  return *threshold < 0 ? Propagate(trail_index - 1, threshold, trail, conflict)
                        : true;
}

bool UpperBoundedLinearConstraint::Propagate(int trail_index,
                                             Coefficient* threshold,
                                             Trail* trail,
                                             std::vector<Literal>* conflict) {
  DCHECK_LT(*threshold, 0);
  const Coefficient slack = GetSlackFromThreshold(*threshold);
  DCHECK_GE(slack, 0) << "The constraint is already a conflict!";
  while (index_ >= 0 && coeffs_[index_] > slack) --index_;

  // Check propagation.
  VariableIndex first_propagated_variable(-1);
  for (int i = starts_[index_ + 1]; i < already_propagated_end_; ++i) {
    if (trail->Assignment().IsLiteralFalse(literals_[i])) continue;
    if (trail->Assignment().IsLiteralTrue(literals_[i])) {
      if (trail->Info(literals_[i].Variable()).trail_index > trail_index) {
        // Conflict.
        FillReason(*trail, trail_index, literals_[i].Variable(), conflict);
        conflict->push_back(literals_[i].Negated());
        Update(slack, threshold);
        return false;
      }
    } else {
      // Propagation.
      if (first_propagated_variable < 0) {
        trail->EnqueueWithPbReason(literals_[i].Negated(), trail_index, this);
        first_propagated_variable = literals_[i].Variable();
      } else {
        // Note that the reason for first_propagated_variable is always a
        // valid reason for literals_[i].Variable() because we process the
        // variable in increasing coefficient order.
        trail->EnqueueWithSameReasonAs(literals_[i].Negated(),
                                       first_propagated_variable);
      }
    }
  }
  Update(slack, threshold);
  DCHECK_GE(*threshold, 0);
  return true;
}

void UpperBoundedLinearConstraint::FillReason(const Trail& trail,
                                              int source_trail_index,
                                              VariableIndex propagated_variable,
                                              std::vector<Literal>* reason) {
  // Optimization for an "at most one" constraint.
  if (rhs_ == 1) {
    reason->clear();
    reason->push_back(trail[source_trail_index].Negated());
    return;
  }

  // This is needed for unsat proof.
  const bool include_level_zero = trail.NeedFixedLiteralsInReason();

  // Optimization: This will be set to the index of the last literal in the
  // reason, that is the one with smallest indices.
  int last_i = 0;
  int last_coeff_index = 0;

  // Compute the initial reason which is formed by all the literals of the
  // constraint that were assigned to true at the time of the propagation.
  // We remove literals with a level of 0 since they are not needed.
  // We also compute the slack at the time.
  reason->clear();
  Coefficient slack = rhs_;
  Coefficient propagated_variable_coefficient(0);
  int coeff_index = coeffs_.size() - 1;
  for (int i = literals_.size() - 1; i >= 0; --i) {
    const Literal literal = literals_[i];
    if (literal.Variable() == propagated_variable) {
      propagated_variable_coefficient = coeffs_[coeff_index];
    } else {
      if (trail.Assignment().IsLiteralTrue(literal) &&
          trail.Info(literal.Variable()).trail_index <= source_trail_index) {
        if (include_level_zero || trail.Info(literal.Variable()).level != 0) {
          reason->push_back(literal.Negated());
          last_i = i;
          last_coeff_index = coeff_index;
        }
        slack -= coeffs_[coeff_index];
      }
    }
    if (i == starts_[coeff_index]) {
      --coeff_index;
    }
  }
  DCHECK_GT(propagated_variable_coefficient, slack);
  DCHECK_GE(propagated_variable_coefficient, 0);

  // In both cases, we can't minimize the reason further.
  if (reason->size() <= 1 || coeffs_.size() == 1) return;

  Coefficient limit = propagated_variable_coefficient - slack;
  DCHECK_GE(limit, 1);

  // Remove literals with small coefficients from the reason as long as the
  // limit is still stricly positive.
  coeff_index = last_coeff_index;
  if (coeffs_[coeff_index] >= limit) return;
  for (int i = last_i; i < literals_.size(); ++i) {
    const Literal literal = literals_[i];
    if (i == starts_[coeff_index + 1]) {
      ++coeff_index;
      if (coeffs_[coeff_index] >= limit) break;
    }
    if (literal.Negated() != reason->back()) continue;
    limit -= coeffs_[coeff_index];
    reason->pop_back();
    if (coeffs_[coeff_index] >= limit) break;
  }
  DCHECK(!reason->empty());
  DCHECK_GE(limit, 1);
}

void UpperBoundedLinearConstraint::ResolvePBConflict(
    const Trail& trail, VariableIndex var,
    MutableUpperBoundedLinearConstraint* conflict) {
  const int limit_trail_index = trail.Info(var).trail_index;

  // Compute the constraint activity at the time and the coefficient of the
  // variable var.
  Coefficient activity(0);
  Coefficient var_coeff(0);
  int literal_index = 0;
  int coeff_index = 0;
  for (Literal literal : literals_) {
    if (literal.Variable() == var) {
      // The variable must be of the opposite sign in the current conflict.
      CHECK_NE(literal, conflict->GetLiteral(var));
      var_coeff = coeffs_[coeff_index];
    } else if (trail.Assignment().IsLiteralTrue(literal) &&
               trail.Info(literal.Variable()).trail_index < limit_trail_index) {
      activity += coeffs_[coeff_index];
    }
    ++literal_index;
    if (literal_index == starts_[coeff_index + 1]) ++coeff_index;
  }

  // Special case.
  if (activity > rhs_) {
    // This constraint is already a conflict.
    // Use this one instead to start the resolution.
    //
    // TODO(user): Investigate if this is a good idea. It doesn't happen often,
    // but does happend. Maybe we can detect this before in Propagate()? The
    // setup is:
    // - At a given trail_index, var is propagated and added on the trail.
    // - There is some constraint literals assigned to true with a trail index
    //   in (trail_index, var.trail_index).
    // - Their sum is high enough to cause a conflict.
    // - But individually, their coefficients are too small to be propagated, so
    //   the conflict is not yet detected. It will be when these variables are
    //   processed by PropagateNext().
    conflict->ClearAll();
    AddToConflict(conflict);
    return;
  }

  // This is the slack of *this for the trail prefix < limit_trail_index.
  const Coefficient slack = rhs_ - activity;
  CHECK_GE(slack, 0);

  // This is the slack of the conflict at the same level.
  // TODO(user): This is potentially called again in ReduceSlackTo(), fix.
  const Coefficient conflict_slack =
      conflict->ComputeSlackForTrailPrefix(trail, limit_trail_index);

  // When we add the two constraints together, the slack of the result for the
  // trail < limit_trail_index - 1 must be negative. We know that its value is
  // <= slack1 + slack2 - std::min(coeffs), so we have nothing to do if this bound is
  // already negative.
  //
  // TODO(user): If there is more "cancelation" (like it is the case for var)
  // when we add the two constraints, the resulting slack may be even lower.
  // Taking that into account is probably good.
  const Coefficient conflict_var_coeff = conflict->GetCoefficient(var);
  const Coefficient min_coeffs = std::min(var_coeff, conflict_var_coeff);
  const Coefficient new_slack_ub = slack + conflict_slack - min_coeffs;
  CHECK_LT(conflict_slack, conflict_var_coeff);
  CHECK_LT(slack, var_coeff);
  if (new_slack_ub < 0) {
    AddToConflict(conflict);
    return;
  }

  // We need to relax one or both of the constraints so the new slack is < 0.
  // Using the relaxation described in ReduceSlackTo(), we can have this new
  // slack bound:
  //
  //   (slack - diff) + (conflict_slack - conflict_diff)
  //      - std::min(var_coeff - diff, conflict_var_coeff - conflict_diff).
  //
  // For all diff in [0, slack)
  // For all conflict_diff in [0, conflict_slack)
  Coefficient diff(0);
  Coefficient conflict_diff(0);

  // Is relaxing the constraint with the highest coeff enough?
  if (new_slack_ub < std::max(var_coeff, conflict_var_coeff) - min_coeffs) {
    const Coefficient reduc = new_slack_ub + 1;
    if (var_coeff < conflict_var_coeff) {
      conflict_diff += reduc;
    } else {
      diff += reduc;
    }
  } else {
    // Just reduce the slack of both constraints to zero.
    //
    // TODO(user): The best will be to relax as little as possible.
    diff = slack;
    conflict_diff = conflict_slack;
  }

  // Relax the conflict.
  CHECK_GE(conflict_diff, 0);
  CHECK_LE(conflict_diff, conflict_slack);
  if (conflict_diff > 0) {
    conflict->ReduceSlackTo(trail, limit_trail_index,
                            conflict_slack - conflict_diff);
  }

  // We apply the same algorithm as the one in ReduceSlackTo() but on
  // the non-mutable representation and add it on the fly into conflict.
  CHECK_GE(diff, 0);
  CHECK_LE(diff, slack);
  if (diff == 0) {
    // Special case if there if no relaxation is needed.
    AddToConflict(conflict);
    return;
  }

  literal_index = 0;
  coeff_index = 0;
  for (Literal literal : literals_) {
    if (trail.Assignment().IsLiteralTrue(literal) &&
        trail.Info(literal.Variable()).trail_index < limit_trail_index) {
      conflict->AddTerm(literal, coeffs_[coeff_index]);
    } else {
      const Coefficient new_coeff = coeffs_[coeff_index] - diff;
      if (new_coeff > 0) {
        conflict->AddTerm(literal, new_coeff);
      }
    }
    ++literal_index;
    if (literal_index == starts_[coeff_index + 1]) ++coeff_index;
  }

  // And the rhs.
  conflict->AddToRhs(rhs_ - diff);
}

void UpperBoundedLinearConstraint::Untrail(Coefficient* threshold) {
  const Coefficient slack = GetSlackFromThreshold(*threshold);
  while (index_ + 1 < coeffs_.size() && coeffs_[index_ + 1] <= slack) ++index_;
  Update(slack, threshold);
}

// TODO(user): This is relatively slow. Take the "transpose" all at once, and
// maybe put small constraints first on the to_update_ lists.
bool PbConstraints::AddConstraint(const std::vector<LiteralWithCoeff>& cst,
                                  Coefficient rhs, ResolutionNode* node) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(!cst.empty());
  DCHECK(std::is_sorted(cst.begin(), cst.end(), CoeffComparator));

  // Special case if this is the first constraint.
  if (constraints_.empty()) {
    propagation_trail_index_ = trail_->Index();
  }

  // Optimization if the constraint terms are the same as the one of the last
  // added constraint.
  if (!constraints_.empty() && constraints_.back().HasIdenticalTerms(cst)) {
    if (rhs < constraints_.back().Rhs()) {
      // The new constraint is tighther, so we also replace the ResolutionNode.
      // TODO(user): The old one could be unlocked at this point.
      constraints_.back().ChangeResolutionNode(node);
      return constraints_.back().InitializeRhs(rhs, propagation_trail_index_,
                                               &thresholds_.back(), trail_,
                                               &conflict_scratchpad_);
    } else {
      // The constraint is redundant, so there is nothing to do.
      return true;
    }
  }

  const ConstraintIndex cst_index(constraints_.size());
  constraints_.emplace_back(UpperBoundedLinearConstraint(cst, node));
  thresholds_.push_back(Coefficient(0));
  if (!constraints_.back().InitializeRhs(rhs, propagation_trail_index_,
                                         &thresholds_.back(), trail_,
                                         &conflict_scratchpad_)) {
    thresholds_.pop_back();
    constraints_.pop_back();
    return false;
  }
  for (LiteralWithCoeff term : cst) {
    to_update_[term.literal.Index()].push_back(ConstraintIndexWithCoeff(
        trail_->Assignment().IsVariableAssigned(term.literal.Variable()),
        cst_index, term.coefficient));
  }
  return true;
}

bool PbConstraints::PropagateNext() {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(PropagationNeeded());
  const int order = propagation_trail_index_;
  const Literal true_literal = (*trail_)[propagation_trail_index_];
  ++propagation_trail_index_;

  // We need to upate ALL threshold, otherwise the Untrail() will not be
  // synchronized.
  bool conflict = false;
  num_threshold_updates_ += to_update_[true_literal.Index()].size();
  for (ConstraintIndexWithCoeff& update : to_update_[true_literal.Index()]) {
    const Coefficient threshold =
        thresholds_[update.index] - update.coefficient;
    thresholds_[update.index] = threshold;
    if (threshold < 0 && !conflict) {
      update.need_untrail_inspection = true;
      ++num_constraint_lookups_;
      if (!constraints_[update.index.value()].Propagate(
              order, &thresholds_[update.index], trail_,
              &conflict_scratchpad_)) {
        trail_->SetFailingClause(ClauseRef(conflict_scratchpad_));
        trail_->SetFailingResolutionNode(
            constraints_[update.index.value()].ResolutionNodePointer());
        conflict = true;
      }
    }
  }
  return !conflict;
}

void PbConstraints::Untrail(int trail_index) {
  SCOPED_TIME_STAT(&stats_);
  to_untrail_.ClearAndResize(ConstraintIndex(constraints_.size()));
  while (propagation_trail_index_ > trail_index) {
    --propagation_trail_index_;
    const Literal literal = (*trail_)[propagation_trail_index_];
    for (ConstraintIndexWithCoeff& update : to_update_[literal.Index()]) {
      thresholds_[update.index] += update.coefficient;

      // Only the constraints which where inspected during Propagate() need
      // inspection during Untrail().
      if (update.need_untrail_inspection) {
        update.need_untrail_inspection = false;
        to_untrail_.Set(update.index);
      }
    }
  }
  for (ConstraintIndex cst_index : to_untrail_.PositionsSetAtLeastOnce()) {
    constraints_[cst_index.value()].Untrail(&(thresholds_[cst_index]));
  }
}

}  // namespace sat
}  // namespace operations_research
