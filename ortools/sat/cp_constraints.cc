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
#include <functional>
#include <ostream>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

std::ostream& operator<<(std::ostream& os, const EnforcementStatus& e) {
  switch (e) {
    case EnforcementStatus::IS_FALSE:
      os << "IS_FALSE";
      break;
    case EnforcementStatus::CANNOT_PROPAGATE:
      os << "CANNOT_PROPAGATE";
      break;
    case EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT:
      os << "CAN_PROPAGATE_ENFORCEMENT";
      break;
    case EnforcementStatus::IS_ENFORCED:
      os << "IS_ENFORCED";
      break;
  }
  return os;
}

EnforcementPropagator::EnforcementPropagator(Model* model)
    : SatPropagator("EnforcementPropagator"),
      trail_(*model->GetOrCreate<Trail>()),
      assignment_(trail_.Assignment()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      rev_int_repository_(model->GetOrCreate<RevIntRepository>()) {
  // Note that this will be after the integer trail since rev_int_repository_
  // depends on IntegerTrail.
  model->GetOrCreate<SatSolver>()->AddPropagator(this);

  // Sentinel - also start of next Register().
  starts_.push_back(0);
}

bool EnforcementPropagator::Propagate(Trail* /*trail*/) {
  rev_int_repository_->SaveStateWithStamp(&rev_stack_size_, &rev_stamp_);
  while (propagation_trail_index_ < trail_.Index()) {
    const Literal literal = trail_[propagation_trail_index_++];
    if (literal.Index() >= static_cast<int>(watcher_.size())) continue;

    int new_size = 0;
    auto& watch_list = watcher_[literal.Index()];
    for (const EnforcementId id : watch_list) {
      const LiteralIndex index = ProcessIdOnTrue(literal, id);
      if (index == kNoLiteralIndex) {
        // We keep the same watcher.
        watch_list[new_size++] = id;
      } else {
        // Change the watcher.
        CHECK_NE(index, literal.Index());
        watcher_[index].push_back(id);
      }
    }
    watch_list.resize(new_size);

    // We also mark some constraint false.
    for (const EnforcementId id : watcher_[literal.NegatedIndex()]) {
      ChangeStatus(id, EnforcementStatus::IS_FALSE);
    }
  }
  rev_stack_size_ = static_cast<int>(untrail_stack_.size());

  // Compute the enforcement status of any constraint added at a positive level.
  // This is only needed until we are back to level zero.
  for (const EnforcementId id : ids_to_fix_until_next_root_level_) {
    ChangeStatus(id, DebugStatus(id));
  }
  if (trail_.CurrentDecisionLevel() == 0) {
    ids_to_fix_until_next_root_level_.clear();
  }

  return true;
}

void EnforcementPropagator::Untrail(const Trail& /*trail*/, int trail_index) {
  // Simply revert the status change.
  const int size = static_cast<int>(untrail_stack_.size());
  for (int i = size - 1; i >= rev_stack_size_; --i) {
    const auto [id, status] = untrail_stack_[i];
    statuses_[id] = status;
    if (callbacks_[id] != nullptr) callbacks_[id](id, status);
  }
  untrail_stack_.resize(rev_stack_size_);
  propagation_trail_index_ = trail_index;
}

// Adds a new constraint to the class and returns the constraint id.
//
// Note that we accept empty enforcement list so that client code can be used
// regardless of the presence of enforcement or not. A negative id means the
// constraint is never enforced, and should be ignored.
EnforcementId EnforcementPropagator::Register(
    absl::Span<const Literal> enforcement,
    std::function<void(EnforcementId, EnforcementStatus)> callback) {
  int num_true = 0;
  int num_false = 0;
  temp_literals_.clear();
  const int level = trail_.CurrentDecisionLevel();
  for (const Literal l : enforcement) {
    // Make sure we always have enough room for the literal and its negation.
    const int size = std::max(l.Index().value(), l.NegatedIndex().value()) + 1;
    if (size > static_cast<int>(watcher_.size())) {
      watcher_.resize(size);
    }
    if (assignment_.LiteralIsTrue(l)) {
      if (level == 0 || trail_.Info(l.Variable()).level == 0) continue;
      ++num_true;
    } else if (assignment_.LiteralIsFalse(l)) {
      ++num_false;
    }
    temp_literals_.push_back(l);
  }
  gtl::STLSortAndRemoveDuplicates(&temp_literals_);

  // Return special index if always enforced.
  if (temp_literals_.empty()) {
    if (callback != nullptr)
      callback(EnforcementId(-1), EnforcementStatus::IS_ENFORCED);
    return EnforcementId(-1);
  }

  const EnforcementId id(static_cast<int>(callbacks_.size()));
  callbacks_.push_back(std::move(callback));

  CHECK(!temp_literals_.empty());
  buffer_.insert(buffer_.end(), temp_literals_.begin(), temp_literals_.end());
  starts_.push_back(buffer_.size());  // Sentinel/next-start.

  // The default status at level zero.
  statuses_.push_back(temp_literals_.size() == 1
                          ? EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT
                          : EnforcementStatus::CANNOT_PROPAGATE);

  if (temp_literals_.size() == 1) {
    watcher_[temp_literals_[0].Index()].push_back(id);
  } else {
    // Make sure we watch correct literals.
    const auto span = GetSpan(id);
    int num_not_true = 0;
    for (int i = 0; i < span.size(); ++i) {
      if (assignment_.LiteralIsTrue(span[i])) continue;
      std::swap(span[num_not_true], span[i]);
      ++num_not_true;
      if (num_not_true == 2) break;
    }

    // We need to watch one of the literals at highest level.
    if (num_not_true == 1) {
      int max_level = trail_.Info(span[1].Variable()).level;
      for (int i = 2; i < span.size(); ++i) {
        const int level = trail_.Info(span[i].Variable()).level;
        if (level > max_level) {
          max_level = level;
          std::swap(span[1], span[i]);
        }
      }
    }

    watcher_[span[0].Index()].push_back(id);
    watcher_[span[1].Index()].push_back(id);
  }

  // Change status, call callback and set up untrail if the status is different
  // from EnforcementStatus::CANNOT_PROPAGATE.
  if (num_false > 0) {
    ChangeStatus(id, EnforcementStatus::IS_FALSE);
  } else if (num_true == temp_literals_.size()) {
    ChangeStatus(id, EnforcementStatus::IS_ENFORCED);
  } else if (num_true + 1 == temp_literals_.size()) {
    ChangeStatus(id, EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);
    // Because this is the default status, we still need to call the callback.
    if (temp_literals_.size() == 1) {
      if (callbacks_[id] != nullptr) {
        callbacks_[id](id, EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);
      }
    }
  }

  // Tricky: if we added something at a positive level, and its status is
  // not CANNOT_PROPAGATE, then we might need to fix it on backtrack.
  if (trail_.CurrentDecisionLevel() > 0 &&
      statuses_[id] != EnforcementStatus::CANNOT_PROPAGATE) {
    ids_to_fix_until_next_root_level_.push_back(id);
  }

  return id;
}

EnforcementId EnforcementPropagator::Register(
    absl::Span<const Literal> enforcement_literals,
    GenericLiteralWatcher* watcher, int literal_watcher_id) {
  return Register(
      enforcement_literals, [=](EnforcementId, EnforcementStatus status) {
        if (status == EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT ||
            status == EnforcementStatus::IS_ENFORCED) {
          watcher->CallOnNextPropagate(literal_watcher_id);
        }
      });
}

// Add the enforcement reason to the given vector.
void EnforcementPropagator::AddEnforcementReason(
    EnforcementId id, std::vector<Literal>* reason) const {
  for (const Literal l : GetSpan(id)) {
    reason->push_back(l.Negated());
  }
}

// Try to propagate when the enforced constraint is not satisfiable.
// This is currently in O(enforcement_size);
bool EnforcementPropagator::PropagateWhenFalse(
    EnforcementId id, absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason) {
  temp_reason_.clear();
  LiteralIndex unique_unassigned = kNoLiteralIndex;
  for (const Literal l : GetSpan(id)) {
    if (assignment_.LiteralIsFalse(l)) return true;
    if (assignment_.LiteralIsTrue(l)) {
      temp_reason_.push_back(l.Negated());
      continue;
    }
    if (unique_unassigned != kNoLiteralIndex) return true;
    unique_unassigned = l.Index();
  }

  temp_reason_.insert(temp_reason_.end(), literal_reason.begin(),
                      literal_reason.end());
  if (unique_unassigned == kNoLiteralIndex) {
    return integer_trail_->ReportConflict(temp_reason_, integer_reason);
  }

  // We also change the status right away.
  ChangeStatus(id, EnforcementStatus::IS_FALSE);
  integer_trail_->EnqueueLiteral(Literal(unique_unassigned).Negated(),
                                 temp_reason_, integer_reason);
  return true;
}

bool EnforcementPropagator::Enqueue(
    EnforcementId id, IntegerLiteral i_lit,
    absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason) {
  temp_reason_.clear();
  temp_reason_.insert(temp_reason_.end(), literal_reason.begin(),
                      literal_reason.end());
  AddEnforcementReason(id, &temp_reason_);
  return integer_trail_->Enqueue(i_lit, temp_reason_, integer_reason);
}

bool EnforcementPropagator::SafeEnqueue(
    EnforcementId id, IntegerLiteral i_lit,
    absl::Span<const IntegerLiteral> integer_reason) {
  temp_reason_.clear();
  AddEnforcementReason(id, &temp_reason_);
  return integer_trail_->SafeEnqueue(i_lit, temp_reason_, integer_reason);
}

bool EnforcementPropagator::ConditionalEnqueue(
    EnforcementId id, Literal lit, IntegerLiteral i_lit,
    absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason) {
  temp_reason_.clear();
  temp_reason_.insert(temp_reason_.end(), literal_reason.begin(),
                      literal_reason.end());
  AddEnforcementReason(id, &temp_reason_);
  temp_integer_reason_.clear();
  temp_integer_reason_.insert(temp_integer_reason_.end(),
                              integer_reason.begin(), integer_reason.end());
  return integer_trail_->ConditionalEnqueue(lit, i_lit, &temp_reason_,
                                            &temp_integer_reason_);
}

void EnforcementPropagator::EnqueueLiteral(
    EnforcementId id, Literal literal, absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason) {
  temp_reason_.clear();
  temp_reason_.insert(temp_reason_.end(), literal_reason.begin(),
                      literal_reason.end());
  AddEnforcementReason(id, &temp_reason_);
  return integer_trail_->EnqueueLiteral(literal, temp_reason_, integer_reason);
}

bool EnforcementPropagator::ReportConflict(
    EnforcementId id, absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason) {
  temp_reason_.clear();
  temp_reason_.insert(temp_reason_.end(), literal_reason.begin(),
                      literal_reason.end());
  AddEnforcementReason(id, &temp_reason_);
  return integer_trail_->ReportConflict(temp_reason_, integer_reason);
}

absl::Span<Literal> EnforcementPropagator::GetSpan(EnforcementId id) {
  if (id < 0) return {};
  DCHECK_LE(id + 1, starts_.size());
  const int size = starts_[id + 1] - starts_[id];
  DCHECK_NE(size, 0);
  return absl::MakeSpan(&buffer_[starts_[id]], size);
}

absl::Span<const Literal> EnforcementPropagator::GetSpan(
    EnforcementId id) const {
  if (id < 0) return {};
  DCHECK_LE(id + 1, starts_.size());
  const int size = starts_[id + 1] - starts_[id];
  DCHECK_NE(size, 0);
  return absl::MakeSpan(&buffer_[starts_[id]], size);
}

LiteralIndex EnforcementPropagator::ProcessIdOnTrue(Literal watched,
                                                    EnforcementId id) {
  const EnforcementStatus status = statuses_[id];
  if (status == EnforcementStatus::IS_FALSE) return kNoLiteralIndex;

  const auto span = GetSpan(id);
  if (span.size() == 1) {
    CHECK_EQ(status, EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);
    ChangeStatus(id, EnforcementStatus::IS_ENFORCED);
    return kNoLiteralIndex;
  }

  const int watched_pos = (span[0] == watched) ? 0 : 1;
  CHECK_EQ(span[watched_pos], watched);
  if (assignment_.LiteralIsFalse(span[watched_pos ^ 1])) {
    ChangeStatus(id, EnforcementStatus::IS_FALSE);
    return kNoLiteralIndex;
  }

  for (int i = 2; i < span.size(); ++i) {
    const Literal l = span[i];
    if (assignment_.LiteralIsFalse(l)) {
      ChangeStatus(id, EnforcementStatus::IS_FALSE);
      return kNoLiteralIndex;
    }
    if (!assignment_.LiteralIsAssigned(l)) {
      // Replace the watched literal. Note that if the other watched literal is
      // true, it should be processed afterwards. We do not change the status
      std::swap(span[watched_pos], span[i]);
      return span[watched_pos].Index();
    }
  }

  // All literal with index > 1 are true. Two case.
  if (assignment_.LiteralIsTrue(span[watched_pos ^ 1])) {
    // All literals are true.
    ChangeStatus(id, EnforcementStatus::IS_ENFORCED);
    return kNoLiteralIndex;
  } else {
    // The other watched literal is the last unassigned
    CHECK_EQ(status, EnforcementStatus::CANNOT_PROPAGATE);
    ChangeStatus(id, EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);
    return kNoLiteralIndex;
  }
}

void EnforcementPropagator::ChangeStatus(EnforcementId id,
                                         EnforcementStatus new_status) {
  const EnforcementStatus old_status = statuses_[id];
  if (old_status == new_status) return;
  if (trail_.CurrentDecisionLevel() != 0) {
    untrail_stack_.push_back({id, old_status});
  }
  statuses_[id] = new_status;
  if (callbacks_[id] != nullptr) callbacks_[id](id, new_status);
}

EnforcementStatus EnforcementPropagator::DebugStatus(EnforcementId id) {
  if (id < 0) return EnforcementStatus::IS_ENFORCED;

  int num_true = 0;
  for (const Literal l : GetSpan(id)) {
    if (assignment_.LiteralIsFalse(l)) {
      return EnforcementStatus::IS_FALSE;
    }
    if (assignment_.LiteralIsTrue(l)) ++num_true;
  }
  const int size = GetSpan(id).size();
  if (num_true == size) return EnforcementStatus::IS_ENFORCED;
  if (num_true + 1 == size) return EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT;
  return EnforcementStatus::CANNOT_PROPAGATE;
}

BooleanXorPropagator::BooleanXorPropagator(
    absl::Span<const Literal> enforcement_literals,
    const std::vector<Literal>& literals, bool value, Model* model)
    : literals_(literals),
      value_(value),
      trail_(*model->GetOrCreate<Trail>()),
      integer_trail_(*model->GetOrCreate<IntegerTrail>()),
      enforcement_propagator_(*model->GetOrCreate<EnforcementPropagator>()) {
  GenericLiteralWatcher* watcher = model->GetOrCreate<GenericLiteralWatcher>();
  enforcement_id_ = enforcement_propagator_.Register(
      enforcement_literals, watcher, RegisterWith(watcher));
}

bool BooleanXorPropagator::Propagate() {
  const EnforcementStatus status =
      enforcement_propagator_.Status(enforcement_id_);
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
    enforcement_propagator_.EnqueueLiteral(
        enforcement_id_, sum == value_ ? u.Negated() : u, literal_reason_,
        /*integer_reason=*/{});
    return true;
  }

  // Ok.
  if (sum == value_) return true;

  // Conflict.
  if (status == EnforcementStatus::IS_ENFORCED) {
    fill_literal_reason();
    return enforcement_propagator_.ReportConflict(
        enforcement_id_, literal_reason_, /*integer_reason=*/{});
  } else if (unassigned_index == -1) {
    fill_literal_reason();
    return enforcement_propagator_.PropagateWhenFalse(
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
