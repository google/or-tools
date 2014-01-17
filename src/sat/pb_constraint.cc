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

namespace operations_research {
namespace sat {

namespace {

// Returns false if the addition overflow/underflow. Otherwise returns true
// and performs the addition *b += a;
bool SafeAdd(Coefficient a, Coefficient* b) {
  if (a > 0) {
    if (*b > std::numeric_limits<Coefficient>::max() - a) return false;
  } else {
    if (*b < std::numeric_limits<Coefficient>::min() - a) return false;
  }
  *b += a;
  return true;
}

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

bool PbCannonicalForm(std::vector<LiteralWithCoeff>* cst, Coefficient* bound_shift,
                      Coefficient* max_value) {
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
        if (!SafeAdd(current.coefficient, &(representative->coefficient)))
          return false;
      } else {
        // Here current_literal is equal to (1 - representative).
        if (!SafeAdd(-current.coefficient, &(representative->coefficient)))
          return false;
        if (!SafeAdd(-current.coefficient, bound_shift)) return false;
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
      if (!SafeAdd(-current.coefficient, bound_shift)) return false;
      (*cst)[i].coefficient = -current.coefficient;
      (*cst)[i].literal = current.literal.Negated();
    }
    if (!SafeAdd((*cst)[i].coefficient, max_value)) return false;
  }

  // Finally sort by increasing coefficients.
  std::sort(cst->begin(), cst->end(), CoeffComparator);
  return true;
}

// TODO(user): Also check for no duplicates literals + unit tests.
bool LinearConstraintIsCannonical(const std::vector<LiteralWithCoeff>& cst) {
  Coefficient previous = 1;
  for (LiteralWithCoeff term : cst) {
    if (term.coefficient < previous) return false;
    previous = term.coefficient;
  }
  return true;
}

UpperBoundedLinearConstraint::UpperBoundedLinearConstraint(
    const std::vector<LiteralWithCoeff>& cst) {
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
                                                 Coefficient* slack,
                                                 Trail* trail,
                                                 std::vector<Literal>* conflict) {
  rhs_ = rhs;

  // Compute the current_rhs from the assigned variables with a trail index
  // smaller than the given trail_index.
  Coefficient current_rhs = rhs;
  int literal_index = 0;
  int coeff_index = 0;
  for (Literal literal : literals_) {
    if (trail->Assignment().IsLiteralTrue(literal) &&
        trail->Info(literal.Variable()).trail_index < trail_index) {
      current_rhs -= coeffs_[coeff_index];
    }
    ++literal_index;
    if (literal_index == starts_[coeff_index + 1]) {
      ++coeff_index;
    }
  }

  // Initial propagation.
  index_ = coeffs_.size() - 1;
  already_propagated_end_ = literals_.size();
  Update(current_rhs, slack);
  return *slack < 0 ? Propagate(trail_index, slack, trail, conflict) : true;
}

bool UpperBoundedLinearConstraint::Propagate(int trail_index,
                                             Coefficient* slack, Trail* trail,
                                             std::vector<Literal>* conflict) {
  DCHECK_LT(*slack, 0);
  const Coefficient current_rhs = GetCurrentRhsFromSlack(*slack);
  while (index_ >= 0 && coeffs_[index_] > current_rhs) --index_;

  // Check propagation.
  for (int i = starts_[index_ + 1]; i < already_propagated_end_; ++i) {
    if (trail->Assignment().IsLiteralFalse(literals_[i])) continue;
    if (trail->Assignment().IsLiteralTrue(literals_[i])) {
      if (trail->Info(literals_[i].Variable()).trail_index > trail_index) {
        // Conflict.
        FillReason(*trail, trail_index, conflict);
        conflict->push_back(literals_[i].Negated());
        Update(current_rhs, slack);
        return false;
      }
    } else {
      // Propagation.
      trail->EnqueueWithPbReason(literals_[i].Negated(), trail_index, this);
    }
  }
  Update(current_rhs, slack);
  return *slack >= 0;
}

void UpperBoundedLinearConstraint::FillReason(const Trail& trail,
                                              int source_trail_index,
                                              std::vector<Literal>* reason) {
  // Optimization for an "at most one" constraint.
  if (rhs_ == 1) {
    reason->clear();
    reason->push_back(trail[source_trail_index].Negated());
    return;
  }

  // Compute the initial reason which is formed by all the literals of the
  // constraint that were assigned to true at the time of the propagation.
  // We remove literals with a level of 0 since they are not needed.
  // We also compute the current_rhs at the time.
  reason->clear();
  Coefficient current_rhs = rhs_;
  int coeff_index = coeffs_.size() - 1;
  for (int i = literals_.size() - 1; i >= 0; --i) {
    const Literal literal = literals_[i];
    if (trail.Assignment().IsLiteralTrue(literal) &&
        trail.Info(literal.Variable()).trail_index <= source_trail_index) {
      if (trail.Info(literal.Variable()).level != 0) {
        reason->push_back(literal.Negated());
      }
      current_rhs -= coeffs_[coeff_index];

      // If current_rhs reaches zero, then we already have a minimal reason for
      // the constraint to be unsat. We can stop there.
      if (current_rhs == 0) break;
    }
    if (i == starts_[coeff_index]) {
      --coeff_index;
    }
  }

  // In both cases, we can't minimize the reason further.
  if (reason->size() <= 1 || coeffs_.size() == 1) return;

  // Find the smallest coefficient greater than current_rhs.
  // We want a coefficient of a literal that wasn't assigned at the time.
  coeff_index = coeffs_.size() - 1;
  Coefficient coeff = coeffs_[coeff_index];
  for (int i = literals_.size() - 1; i >= 0; --i) {
    if (coeffs_[coeff_index] <= current_rhs) break;
    const Literal literal = literals_[i];
    if (!trail.Assignment().IsVariableAssigned(literal.Variable()) ||
        trail.Info(literal.Variable()).trail_index > source_trail_index) {
      coeff = coeffs_[coeff_index];
      if (coeff == current_rhs + 1) break;
    }
    if (i == starts_[coeff_index]) --coeff_index;
  }
  int limit = coeff - current_rhs;
  DCHECK_GE(limit, 1);

  // Remove literals with small coefficients from the reason as long as the
  // limit is still stricly positive.
  coeff_index = 0;
  if (coeffs_[coeff_index] >= limit) return;
  for (int i = 0; i < literals_.size(); ++i) {
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

void UpperBoundedLinearConstraint::Untrail(Coefficient* slack) {
  const Coefficient current_rhs = GetCurrentRhsFromSlack(*slack);
  while (index_ + 1 < coeffs_.size() && coeffs_[index_ + 1] <= current_rhs)
    ++index_;
  Update(current_rhs, slack);
}

// TODO(user): This is relatively slow. Take the "transpose" all at once, and
// maybe put small constraints first on the to_update_ lists.
bool PbConstraints::AddConstraint(const std::vector<LiteralWithCoeff>& cst,
                                  Coefficient rhs) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(!cst.empty());
  DCHECK(std::is_sorted(cst.begin(), cst.end(), CoeffComparator));

  // Optimization if the constraint terms are the same as the one of the last
  // added constraint.
  if (!constraints_.empty() && constraints_.back().HasIdenticalTerms(cst)) {
    if (rhs < constraints_.back().Rhs()) {
      return constraints_.back().InitializeRhs(rhs, propagation_trail_index_,
                                               &slacks_.back(), trail_,
                                               &reason_scratchpad_);
    } else {
      // The constraint is redundant, so there is nothing to do.
      return true;
    }
  }

  const ConstraintIndex cst_index(constraints_.size());
  constraints_.emplace_back(UpperBoundedLinearConstraint(cst));
  slacks_.push_back(0);
  if (!constraints_.back().InitializeRhs(rhs, propagation_trail_index_,
                                         &slacks_.back(), trail_,
                                         &reason_scratchpad_)) {
    return false;
  }
  for (LiteralWithCoeff term : cst) {
    to_update_[term.literal.Index()]
        .push_back(ConstraintIndexWithCoeff(cst_index, term.coefficient));
  }
  return true;
}

bool PbConstraints::PropagateNext() {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(PropagationNeeded());
  const int order = propagation_trail_index_;
  const Literal true_literal = (*trail_)[propagation_trail_index_];
  ++propagation_trail_index_;

  // We need to upate ALL slack, otherwise the Untrail() will not be
  // synchronized.
  //
  // TODO(user): An alternative that sound slightly more efficient is to store
  // an index for this special case so that Untrail() know what to do.
  bool conflict = false;
  num_slack_updates_ += to_update_[true_literal.Index()].size();
  for (ConstraintIndexWithCoeff& update : to_update_[true_literal.Index()]) {
    const Coefficient slack = slacks_[update.index] - update.coefficient;
    slacks_[update.index] = slack;
    if (slack < 0 && !conflict) {
      update.need_untrail_inspection = true;
      ++num_constraint_lookups_;
      if (!constraints_[update.index.value()].Propagate(
               order, &slacks_[update.index], trail_, &reason_scratchpad_)) {
        trail_->SetFailingClause(ClauseRef(reason_scratchpad_));
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
      slacks_[update.index] += update.coefficient;

      // Only the constraints which where inspected during Propagate() need
      // inspection during Untrail().
      if (update.need_untrail_inspection) {
        update.need_untrail_inspection = false;
        to_untrail_.Set(update.index);
      }
    }
  }
  for (ConstraintIndex cst_index : to_untrail_.PositionsSetAtLeastOnce()) {
    constraints_[cst_index.value()].Untrail(&(slacks_[cst_index]));
  }
}

}  // namespace sat
}  // namespace operations_research
