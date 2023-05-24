// Copyright 2010-2022 Google LLC
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

#include "ortools/sat/linear_propagation.h"

#include <stdint.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/numeric/int128.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/bitset.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

void CustomFifoQueue::IncreaseSize(int n) {
  CHECK_GE(n, pos_.size());
  CHECK_LE(left_, right_);
  pos_.resize(n, -1);
  tmp_positions_.resize(n, 0);

  // We need 1 more space since we can add at most n element.
  queue_.resize(n + 1, 0);
}

int CustomFifoQueue::Pop() {
  DCHECK(!empty());
  const int id = queue_[left_];
  pos_[id] = -1;
  ++left_;
  if (left_ == queue_.size()) left_ = 0;
  return id;
}

void CustomFifoQueue::Push(int id) {
  DCHECK_GE(id, 0);
  DCHECK_LE(id, pos_.size());
  DCHECK(!Contains(id));
  pos_[id] = right_;
  queue_[right_] = id;
  ++right_;
  if (right_ == queue_.size()) right_ = 0;
}

void CustomFifoQueue::Reorder(absl::Span<const int> order) {
  if (order.size() <= 1) return;

  const int capacity = queue_.size();
  const int size = left_ < right_ ? right_ - left_ : left_ + capacity - right_;
  if (order.size() > size / 8) {
    return ReorderDense(order);
  }

  int index = 0;
  for (const int id : order) {
    const int p = pos_[id];
    DCHECK_GE(p, 0);
    tmp_positions_[index++] = p >= left_ ? p : p + capacity;
  }
  std::sort(&tmp_positions_[0], &tmp_positions_[index]);
  DCHECK(std::unique(&tmp_positions_[0], &tmp_positions_[index]) ==
         &tmp_positions_[index]);

  index = 0;
  for (const int id : order) {
    int p = tmp_positions_[index++];
    if (p >= capacity) p -= capacity;
    pos_[id] = p;
    queue_[p] = id;
  }
}

void CustomFifoQueue::ReorderDense(absl::Span<const int> order) {
  for (const int id : order) {
    DCHECK_GE(pos_[id], 0);
    queue_[pos_[id]] = -1;
  }
  int order_index = 0;
  if (left_ <= right_) {
    for (int i = left_; i < right_; ++i) {
      if (queue_[i] == -1) {
        queue_[i] = order[order_index++];
        pos_[queue_[i]] = i;
      }
    }
  } else {
    const int size = queue_.size();
    for (int i = left_; i < size; ++i) {
      if (queue_[i] == -1) {
        queue_[i] = order[order_index++];
        pos_[queue_[i]] = i;
      }
    }
    for (int i = 0; i < left_; ++i) {
      if (queue_[i] == -1) {
        queue_[i] = order[order_index++];
        pos_[queue_[i]] = i;
      }
    }
  }
  DCHECK_EQ(order_index, order.size());
}

void CustomFifoQueue::SortByPos(absl::Span<int> elements) {
  std::sort(elements.begin(), elements.end(),
            [this](const int id1, const int id2) {
              const int p1 = pos_[id1];
              const int p2 = pos_[id2];
              if (p1 >= left_) {
                if (p2 >= left_) return p1 < p2;
                return true;
              } else {
                // p1 < left_.
                if (p2 < left_) return p1 < p2;
                return false;
              }
            });
}

std::ostream& operator<<(std::ostream& os, const EnforcementStatus& e) {
  switch (e) {
    case EnforcementStatus::IS_FALSE:
      os << "IS_FALSE";
      break;
    case EnforcementStatus::CANNOT_PROPAGATE:
      os << "CANNOT_PROPAGATE";
      break;
    case EnforcementStatus::CAN_PROPAGATE:
      os << "CAN_PROPAGATE";
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
  return true;
}

void EnforcementPropagator::Untrail(const Trail& /*trail*/, int trail_index) {
  // Simply revert the status change.
  const int size = static_cast<int>(untrail_stack_.size());
  for (int i = size - 1; i >= rev_stack_size_; --i) {
    const auto [id, status] = untrail_stack_[i];
    statuses_[id] = status;
    if (callbacks_[id] != nullptr) callbacks_[id](status);
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
    std::function<void(EnforcementStatus)> callback) {
  CHECK_EQ(trail_.CurrentDecisionLevel(), 0);
  int num_true = 0;
  int num_false = 0;
  temp_literals_.clear();
  for (const Literal l : enforcement) {
    // Make sure we always have enough room for the literal and its negation.
    const int size = std::max(l.Index().value(), l.NegatedIndex().value()) + 1;
    if (size > static_cast<int>(watcher_.size())) {
      watcher_.resize(size);
    }
    if (assignment_.LiteralIsTrue(l)) {
      ++num_true;
      continue;
    }
    if (assignment_.LiteralIsFalse(l)) {
      ++num_false;
      continue;
    }
    temp_literals_.push_back(l);
  }
  gtl::STLSortAndRemoveDuplicates(&temp_literals_);

  // Return special indices if never/always enforced.
  if (num_false > 0) {
    if (callback != nullptr) callback(EnforcementStatus::IS_FALSE);
    return EnforcementId(-1);
  }
  if (num_true == enforcement.size()) {
    if (callback != nullptr) callback(EnforcementStatus::IS_ENFORCED);
    return EnforcementId(-1);
  }

  const EnforcementId id(static_cast<int>(callbacks_.size()));
  callbacks_.push_back(std::move(callback));

  CHECK(!temp_literals_.empty());
  buffer_.insert(buffer_.end(), temp_literals_.begin(), temp_literals_.end());
  starts_.push_back(buffer_.size());  // Sentinel.
  statuses_.push_back(EnforcementStatus::CANNOT_PROPAGATE);

  if (temp_literals_.size() == 1) {
    watcher_[temp_literals_[0].Index()].push_back(id);
    ChangeStatus(id, EnforcementStatus::CAN_PROPAGATE);
  } else {
    watcher_[temp_literals_[0].Index()].push_back(id);
    watcher_[temp_literals_[1].Index()].push_back(id);
  }
  return id;
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

  integer_trail_->EnqueueLiteral(Literal(unique_unassigned).Negated(),
                                 temp_reason_, integer_reason);
  return true;
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
    CHECK_EQ(status, EnforcementStatus::CAN_PROPAGATE);
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
    ChangeStatus(id, EnforcementStatus::CAN_PROPAGATE);
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
  if (callbacks_[id] != nullptr) callbacks_[id](new_status);
}

LinearPropagator::LinearPropagator(Model* model)
    : trail_(model->GetOrCreate<Trail>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      enforcement_propagator_(model->GetOrCreate<EnforcementPropagator>()),
      watcher_(model->GetOrCreate<GenericLiteralWatcher>()),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      rev_int_repository_(model->GetOrCreate<RevIntRepository>()),
      rev_integer_value_repository_(
          model->GetOrCreate<RevIntegerValueRepository>()),
      shared_stats_(model->GetOrCreate<SharedStatistics>()),
      watcher_id_(watcher_->Register(this)) {
  // Note that we need this class always in sync.
  integer_trail_->RegisterWatcher(&modified_vars_);
  integer_trail_->RegisterReversibleClass(this);

  // TODO(user): When we start to push too much (Cycle?) we should see what
  // other propagator says before repropagating this one, system for call
  // later?
  watcher_->SetPropagatorPriority(watcher_id_, 0);
}

LinearPropagator::~LinearPropagator() {
  if (!VLOG_IS_ON(1)) return;
  if (shared_stats_ == nullptr) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back({"linear_propag/num_pushes", num_pushes_});
  stats.push_back(
      {"linear_propag/num_enforcement_pushes", num_enforcement_pushes_});
  stats.push_back({"linear_propag/num_simple_cycles", num_simple_cycles_});
  stats.push_back({"linear_propag/num_complex_cycles", num_complex_cycles_});
  stats.push_back({"linear_propag/num_scanned", num_scanned_});
  stats.push_back({"linear_propag/num_extra_scan", num_extra_scans_});
  stats.push_back({"linear_propag/num_explored_in_disassemble",
                   num_explored_in_disassemble_});
  stats.push_back({"linear_propag/num_bool_aborts", num_bool_aborts_});
  stats.push_back({"linear_propag/num_ignored", num_ignored_});
  stats.push_back({"linear_propag/num_reordered", num_reordered_});
  shared_stats_->AddStats(stats);
}

void LinearPropagator::SetLevel(int level) {
  if (level < previous_level_) {
    // If the solver backtracked at any point, we invalidate all our queue
    // and propagated_by information.
    ClearPropagatedBy();
    while (!propagation_queue_.empty()) {
      in_queue_[propagation_queue_.Pop()] = false;
    }
    for (int i = rev_at_false_size_; i < in_queue_and_at_false_.size(); ++i) {
      in_queue_[in_queue_and_at_false_[i]] = false;
    }
    in_queue_and_at_false_.resize(rev_at_false_size_);
  } else if (level > previous_level_) {
    rev_at_false_size_ = in_queue_and_at_false_.size();
    rev_int_repository_->SaveState(&rev_at_false_size_);
  }
  previous_level_ = level;

  // Tricky: if we aborted the current propagation because we pushed a Boolean,
  // by default, the GenericLiteralWatcher will only call Propagate() again if
  // one of the watched variable changed. With this, it is guaranteed to call
  // it again if it wasn't in the queue already.
  if (!propagation_queue_.empty()) {
    watcher_->CallOnNextPropagate(watcher_id_);
  }
}

bool LinearPropagator::Propagate() {
  id_scanned_at_least_once_.ClearAndResize(in_queue_.size());

  // Initial addition.
  // We will clear modified_vars_ on exit.
  for (const IntegerVariable var : modified_vars_.PositionsSetAtLeastOnce()) {
    if (var >= var_to_constraint_ids_.size()) continue;
    SetPropagatedBy(var, -1);
    AddWatchedToQueue(var);
  }

  // TODO(user): Abort this propagator as soon as a Boolean is propagated ? so
  // that we always finish the Boolean propagation first. This can happen when
  // we push a bound that has associated Booleans. The idea is to resume from
  // our current state when we are called again. Note however that we have to
  // clear the propagated_by_ info has other propagator might have pushed the
  // same variable further.
  //
  // Empty FIFO queue.
  const int saved_index = trail_->Index();
  while (!propagation_queue_.empty()) {
    const int id = propagation_queue_.Pop();
    in_queue_[id] = false;
    if (!PropagateOneConstraint(id)) {
      modified_vars_.ClearAndResize(integer_trail_->NumIntegerVariables());
      return false;
    }

    if (trail_->Index() > saved_index) {
      ++num_bool_aborts_;
      break;
    }
  }

  // Clean-up modified_vars_ to do as little as possible on the next call.
  modified_vars_.ClearAndResize(integer_trail_->NumIntegerVariables());
  return true;
}

// Adds a new constraint to the propagator.
void LinearPropagator::AddConstraint(
    absl::Span<const Literal> enforcement_literals,
    absl::Span<const IntegerVariable> vars,
    absl::Span<const IntegerValue> coeffs, IntegerValue upper_bound) {
  if (vars.empty()) return;
  for (const Literal l : enforcement_literals) {
    if (trail_->Assignment().LiteralIsFalse(l)) return;
  }

  // Make sure max_variations_ is of correct size.
  // Note that we also have a hard limit of 1 << 29 on the size.
  CHECK_LT(vars.size(), 1 << 29);
  if (vars.size() > max_variations_.size()) {
    max_variations_.resize(vars.size(), 0);
    buffer_of_ones_.resize(vars.size(), IntegerValue(1));
  }

  // Initialize constraint data.
  CHECK_EQ(vars.size(), coeffs.size());
  const int id = infos_.size();
  {
    ConstraintInfo info;
    info.all_coeffs_are_one = false;
    info.start = variables_buffer_.size();
    info.initial_size = vars.size();
    info.rev_rhs = upper_bound;
    info.rev_size = vars.size();
    infos_.push_back(std::move(info));
  }

  id_to_propagation_count_.push_back(0);
  variables_buffer_.insert(variables_buffer_.end(), vars.begin(), vars.end());
  coeffs_buffer_.insert(coeffs_buffer_.end(), coeffs.begin(), coeffs.end());
  CanonicalizeConstraint(id);

  bool all_at_one = true;
  for (const IntegerValue coeff : GetCoeffs(infos_.back())) {
    if (coeff != 1) {
      all_at_one = false;
      break;
    }
  }
  if (all_at_one) {
    // TODO(user): we still waste the space in coeffs_buffer_ so that the
    // start are aligned with the variables_buffer_.
    infos_.back().all_coeffs_are_one = true;
  }

  // Initialize watchers.
  // Initialy we want everything to be propagated at least once.
  in_queue_.push_back(false);
  propagation_queue_.IncreaseSize(in_queue_.size());

  if (!enforcement_literals.empty()) {
    infos_.back().enf_status = EnforcementStatus::CANNOT_PROPAGATE;
    infos_.back().enf_id = enforcement_propagator_->Register(
        enforcement_literals, [this, id](EnforcementStatus status) {
          infos_[id].enf_status = status;
          // TODO(user): With some care, when we cannot propagate or the
          // constraint is not enforced, we could live in_queue_[] at true but
          // not put the constraint in the queue.
          if (status == EnforcementStatus::CAN_PROPAGATE ||
              status == EnforcementStatus::IS_ENFORCED) {
            AddToQueueIfNeeded(id);
            watcher_->CallOnNextPropagate(watcher_id_);
          }
        });
  } else {
    AddToQueueIfNeeded(id);
    infos_.back().enf_id = -1;
    infos_.back().enf_status = EnforcementStatus::IS_ENFORCED;
  }

  for (const IntegerVariable var : GetVariables(infos_[id])) {
    // Transposed graph to know which constraint to wake up.
    if (var >= var_to_constraint_ids_.size()) {
      // We need both the var entry and its negation to be allocated.
      const int size = std::max(var, NegationOf(var)).value() + 1;
      var_to_constraint_ids_.resize(size);
      propagated_by_.resize(size, -1);
      propagated_by_was_set_.Resize(IntegerVariable(size));
      is_watched_.resize(size, false);
    }

    // TODO(user): Shall we decide on some ordering here? maybe big coeff first
    // so that we get the largest change in slack? the idea being to propagate
    // large change first in case of cycles.
    var_to_constraint_ids_[var].push_back(id);

    // We need to be registered to the watcher so Propagate() is called at
    // the proper priority. But then we rely on modified_vars_.
    if (!is_watched_[var]) {
      is_watched_[var] = true;
      watcher_->WatchLowerBound(var, watcher_id_);
    }
  }
}

absl::Span<IntegerValue> LinearPropagator::GetCoeffs(
    const ConstraintInfo& info) {
  if (info.all_coeffs_are_one) {
    return absl::MakeSpan(&buffer_of_ones_[0], info.initial_size);
  }
  return absl::MakeSpan(&coeffs_buffer_[info.start], info.initial_size);
}

absl::Span<IntegerVariable> LinearPropagator::GetVariables(
    const ConstraintInfo& info) {
  return absl::MakeSpan(&variables_buffer_[info.start], info.initial_size);
}

void LinearPropagator::CanonicalizeConstraint(int id) {
  const ConstraintInfo& info = infos_[id];
  auto coeffs = GetCoeffs(info);
  auto vars = GetVariables(info);
  for (int i = 0; i < vars.size(); ++i) {
    if (coeffs[i] < 0) {
      coeffs[i] = -coeffs[i];
      vars[i] = NegationOf(vars[i]);
    }
  }
}

// TODO(user): template everything for the case info.all_coeffs_are_one ?
bool LinearPropagator::PropagateOneConstraint(int id) {
  // This is here for development purpose, it is a bit too slow to check by
  // default though, even VLOG_IS_ON(1) so we disable it.
  if (/* DISABLES CODE */ (false)) {
    ++num_scanned_;
    if (id_scanned_at_least_once_[id]) {
      ++num_extra_scans_;
    } else {
      id_scanned_at_least_once_.Set(id);
    }
  }

  // Skip constraint not enforced or that cannot propagate if false.
  ConstraintInfo& info = infos_[id];
  if (info.enf_status == EnforcementStatus::IS_FALSE ||
      info.enf_status == EnforcementStatus::CANNOT_PROPAGATE) {
    DCHECK(!in_queue_[id]);
    if (info.enf_status == EnforcementStatus::IS_FALSE) {
      // We mark this constraint as in the queue but will never inspect it
      // again until we backtrack over this time.
      in_queue_[id] = true;
      in_queue_and_at_false_.push_back(id);
    }
    ++num_ignored_;
    return true;
  }

  // Compute the slack and max_variations_ of each variables.
  // We also filter out fixed variables in a reversible way.
  IntegerValue implied_lb(0);
  auto vars = GetVariables(info);
  auto coeffs = GetCoeffs(info);
  IntegerValue max_variation(0);
  bool first_change = true;
  time_limit_->AdvanceDeterministicTime(static_cast<double>(info.rev_size) *
                                        1e-9);
  for (int i = 0; i < info.rev_size;) {
    const IntegerVariable var = vars[i];
    const IntegerValue coeff = coeffs[i];
    const IntegerValue lb = integer_trail_->LowerBound(var);
    const IntegerValue ub = integer_trail_->UpperBound(var);
    if (lb == ub) {
      if (first_change) {
        // Note that we can save at most one state per fixed var. Also at
        // level zero we don't save anything.
        rev_int_repository_->SaveState(&info.rev_size);
        rev_integer_value_repository_->SaveState(&info.rev_rhs);
        first_change = false;
      }
      info.rev_size--;
      std::swap(vars[i], vars[info.rev_size]);
      std::swap(coeffs[i], coeffs[info.rev_size]);
      info.rev_rhs -= coeff * lb;
    } else {
      implied_lb += coeff * lb;
      max_variations_[i] = (ub - lb) * coeff;
      max_variation = std::max(max_variation, max_variations_[i]);
      ++i;
    }
  }
  const IntegerValue slack = info.rev_rhs - implied_lb;

  // Negative slack means the constraint is false.
  if (max_variation <= slack) return true;
  if (slack < 0) {
    // Fill integer reason.
    integer_reason_.clear();
    reason_coeffs_.clear();
    for (int i = 0; i < info.initial_size; ++i) {
      const IntegerVariable var = vars[i];
      if (!integer_trail_->VariableLowerBoundIsFromLevelZero(var)) {
        integer_reason_.push_back(integer_trail_->LowerBoundAsLiteral(var));
        reason_coeffs_.push_back(coeffs[i]);
      }
    }

    // Relax it.
    integer_trail_->RelaxLinearReason(-slack - 1, reason_coeffs_,
                                      &integer_reason_);
    ++num_enforcement_pushes_;
    return enforcement_propagator_->PropagateWhenFalse(info.enf_id, {},
                                                       integer_reason_);
  }

  // We can only propagate more if all the enforcement literals are true.
  if (info.enf_status != EnforcementStatus::IS_ENFORCED) return true;

  // The lower bound of all the variables except one can be used to update the
  // upper bound of the last one.
  int num_pushed = 0;
  for (int i = 0; i < info.rev_size; ++i) {
    if (max_variations_[i] <= slack) continue;

    // TODO(user): If the new ub fall into an hole of the variable, we can
    // actually relax the reason more by computing a better slack.
    ++num_pushes_;
    const IntegerVariable var = vars[i];
    const IntegerValue coeff = coeffs[i];
    const IntegerValue div = slack / coeff;
    const IntegerValue new_ub = integer_trail_->LowerBound(var) + div;
    const IntegerValue propagation_slack = (div + 1) * coeff - slack - 1;
    if (!integer_trail_->Enqueue(
            IntegerLiteral::LowerOrEqual(var, new_ub),
            /*lazy_reason=*/[this, info, propagation_slack](
                                IntegerLiteral i_lit, int trail_index,
                                std::vector<Literal>* literal_reason,
                                std::vector<int>* trail_indices_reason) {
              literal_reason->clear();
              trail_indices_reason->clear();
              enforcement_propagator_->AddEnforcementReason(info.enf_id,
                                                            literal_reason);
              reason_coeffs_.clear();

              auto coeffs = GetCoeffs(info);
              auto vars = GetVariables(info);
              for (int i = 0; i < info.initial_size; ++i) {
                const IntegerVariable var = vars[i];
                if (PositiveVariable(var) == PositiveVariable(i_lit.var)) {
                  continue;
                }
                const int index =
                    integer_trail_->FindTrailIndexOfVarBefore(var, trail_index);
                if (index >= 0) {
                  trail_indices_reason->push_back(index);
                  if (propagation_slack > 0) {
                    reason_coeffs_.push_back(coeffs[i]);
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

    // Add to the queue all touched constraint.
    const IntegerValue actual_ub = integer_trail_->UpperBound(var);
    const IntegerVariable next_var = NegationOf(var);
    if (actual_ub < new_ub) {
      // Was pushed further due to hole. We clear it.
      SetPropagatedBy(next_var, -1);
      AddWatchedToQueue(next_var);
    } else if (actual_ub == new_ub) {
      SetPropagatedBy(next_var, id);
      AddWatchedToQueue(next_var);

      // We reorder them first.
      std::swap(vars[i], vars[num_pushed]);
      std::swap(coeffs[i], coeffs[num_pushed]);
      ++num_pushed;
    }

    // Explore the subtree and detect cycles greedily.
    // Also postpone some propagation.
    if (num_pushed > 0) {
      if (!DisassembleSubtree(id, num_pushed)) {
        return false;
      }
    }
  }

  return true;
}

std::string LinearPropagator::ConstraintDebugString(int id) {
  std::string result;
  const ConstraintInfo& info = infos_[id];
  auto coeffs = GetCoeffs(info);
  auto vars = GetVariables(info);
  IntegerValue implied_lb(0);
  IntegerValue rhs_correction(0);
  for (int i = 0; i < info.initial_size; ++i) {
    const IntegerValue term = coeffs[i] * integer_trail_->LowerBound(vars[i]);
    if (i >= info.rev_size) {
      rhs_correction += term;
    }
    implied_lb += term;
    absl::StrAppend(&result, " +", coeffs[i].value(), "*X", vars[i].value());
  }
  const IntegerValue original_rhs = info.rev_rhs + rhs_correction;
  absl::StrAppend(&result, " <= ", original_rhs.value(),
                  " slack=", original_rhs.value() - implied_lb.value());
  absl::StrAppend(&result, " enf=", info.enf_status);
  return result;
}

bool LinearPropagator::ReportConflictingCycle() {
  // Often, all coefficients of the variable involved in the cycle are the same
  // and if we sum all constraint, we get an infeasible one. If this is the
  // case, we simplify the reason.
  //
  // TODO(user): We could relax if the coefficient of the sum do not overflow.
  // TODO(user): Sum constraints with eventual factor in more cases.
  {
    literal_reason_.clear();
    integer_reason_.clear();
    absl::int128 rhs_sum = 0;
    absl::flat_hash_map<IntegerVariable, absl::int128> map_sum;
    for (const auto [id, next_var] : disassemble_branch_) {
      const ConstraintInfo& info = infos_[id];
      enforcement_propagator_->AddEnforcementReason(info.enf_id,
                                                    &literal_reason_);
      auto coeffs = GetCoeffs(info);
      auto vars = GetVariables(info);
      IntegerValue rhs_correction(0);
      for (int i = 0; i < info.initial_size; ++i) {
        if (i >= info.rev_size) {
          rhs_correction += coeffs[i] * integer_trail_->LowerBound(vars[i]);
        }
        if (VariableIsPositive(vars[i])) {
          map_sum[vars[i]] += coeffs[i].value();
        } else {
          map_sum[PositiveVariable(vars[i])] -= coeffs[i].value();
        }
      }
      rhs_sum += (info.rev_rhs + rhs_correction).value();
    }

    // We shouldn't have overflow since each component do not overflow an
    // int64_t and we sum a small amount of them.
    absl::int128 implied_lb = 0;
    for (const auto [var, coeff] : map_sum) {
      if (coeff > 0) {
        if (!integer_trail_->VariableLowerBoundIsFromLevelZero(var)) {
          integer_reason_.push_back(integer_trail_->LowerBoundAsLiteral(var));
        }
        implied_lb +=
            coeff * absl::int128{integer_trail_->LowerBound(var).value()};
      } else if (coeff < 0) {
        if (!integer_trail_->VariableLowerBoundIsFromLevelZero(
                NegationOf(var))) {
          integer_reason_.push_back(integer_trail_->UpperBoundAsLiteral(var));
        }
        implied_lb +=
            coeff * absl::int128{integer_trail_->UpperBound(var).value()};
      }
    }
    if (implied_lb > rhs_sum) {
      // We sort for determinism.
      std::sort(integer_reason_.begin(), integer_reason_.end(),
                [](const IntegerLiteral& a, const IntegerLiteral& b) {
                  return a.var < b.var;
                });

      // Relax the linear reason if everything fit on an int64_t.
      const absl::int128 limit{std::numeric_limits<int64_t>::max()};
      const absl::int128 slack = implied_lb - rhs_sum;
      if (slack > 1) {
        reason_coeffs_.clear();
        bool abort = false;
        for (const IntegerLiteral i_lit : integer_reason_) {
          absl::int128 c = map_sum.at(PositiveVariable(i_lit.var));
          if (c < 0) c = -c;  // No std::abs() for int128.
          if (c >= limit) {
            abort = true;
            break;
          }
          reason_coeffs_.push_back(static_cast<int64_t>(c));
        }
        if (!abort) {
          const IntegerValue slack64(
              static_cast<int64_t>(std::min(limit, slack)));
          integer_trail_->RelaxLinearReason(slack64 - 1, reason_coeffs_,
                                            &integer_reason_);
        }
      }

      ++num_simple_cycles_;
      VLOG(2) << "Simplified " << integer_reason_.size() << " slack "
              << implied_lb - rhs_sum;
      return integer_trail_->ReportConflict(literal_reason_, integer_reason_);
    }
  }

  // For the complex reason, we just use the bound of every variable.
  // We do some basic simplification for the variable involved in the cycle.
  //
  // TODO(user): Can we simplify more?
  VLOG(2) << "Cycle";
  literal_reason_.clear();
  integer_reason_.clear();
  IntegerVariable previous_var = kNoIntegerVariable;
  for (const auto [id, next_var] : disassemble_branch_) {
    const ConstraintInfo& info = infos_[id];
    enforcement_propagator_->AddEnforcementReason(info.enf_id,
                                                  &literal_reason_);
    for (const IntegerVariable var : GetVariables(infos_[id])) {
      // The lower bound of this variable is implied by the previous constraint,
      // so we do not need to include it.
      if (var == previous_var) continue;

      // We do not need the lower bound of var to propagate its upper bound.
      if (var == NegationOf(next_var)) continue;

      if (!integer_trail_->VariableLowerBoundIsFromLevelZero(var)) {
        integer_reason_.push_back(integer_trail_->LowerBoundAsLiteral(var));
      }
    }
    previous_var = next_var;

    VLOG(2) << next_var << " [" << integer_trail_->LowerBound(next_var) << ","
            << integer_trail_->UpperBound(next_var)
            << "] : " << ConstraintDebugString(id);
  }
  ++num_complex_cycles_;
  return integer_trail_->ReportConflict(literal_reason_, integer_reason_);
}

// Note that if there is a loop in the propagated_by_ graph, it must be
// from root_id -> root_var, because each time we add an edge, we do
// disassemble.
//
// TODO(user): If one of the var coeff is > previous slack we push an id again,
// we can stop early with a conflict by propagating the ids in sequence.
bool LinearPropagator::DisassembleSubtree(int root_id, int num_pushed) {
  disassemble_to_reorder_.ClearAndResize(in_queue_.size());
  disassemble_reverse_topo_order_.clear();

  // The variable was just pushed, we explore the set of variable that will
  // be pushed further due to this push. Basically, if a constraint propagated
  // before and its slack will reduce due to the push, then any previously
  // propagated variable with a coefficient NOT GREATER than the one of the
  // variable reducing the slack will be pushed further.
  disassemble_queue_.clear();
  disassemble_branch_.clear();
  {
    const ConstraintInfo& info = infos_[root_id];
    auto vars = GetVariables(info);
    for (int i = 0; i < num_pushed; ++i) {
      disassemble_queue_.push_back({root_id, NegationOf(vars[i])});
    }
  }

  // Note that all var should be unique since there is only one propagated_by_
  // for each one. And each time we explore an id, we disassemble the tree.
  while (!disassemble_queue_.empty()) {
    const auto [prev_id, var] = disassemble_queue_.back();
    if (!disassemble_branch_.empty() &&
        disassemble_branch_.back().first == prev_id &&
        disassemble_branch_.back().second == var) {
      disassemble_branch_.pop_back();
      disassemble_reverse_topo_order_.push_back(prev_id);
      disassemble_queue_.pop_back();
      continue;
    }

    disassemble_branch_.push_back({prev_id, var});
    time_limit_->AdvanceDeterministicTime(
        static_cast<double>(var_to_constraint_ids_[var].size()) * 1e-9);
    for (const int id : var_to_constraint_ids_[var]) {
      if (prev_id == root_id) {
        // Root id was just propagated, so there is no need to reorder what
        // it pushes.
        DCHECK_NE(id, root_id);
        if (disassemble_to_reorder_[id]) continue;
        disassemble_to_reorder_.Set(id);
      } else if (id == root_id) {
        // TODO(user): Check previous slack vs var coeff?
        // TODO(user): Make sure there are none or detect cycle not going back
        // to the root.
        CHECK(!disassemble_branch_.empty());

        // This is a corner case in which there is actually no cycle.
        const IntegerVariable root_var = disassemble_branch_[0].second;
        CHECK_EQ(disassemble_branch_[0].first, root_id);
        CHECK_NE(var, root_var);
        if (var == NegationOf(root_var)) continue;

        // Tricky: We have a cycle here only if coeff of var >= root_coeff.
        // If there is no cycle, we will just finish the branch here.
        //
        // TODO(user): Can we be more precise? if one coeff is big, the
        // variation in slack might be big enough to push a variable twice and
        // thus push a lower coeff.
        const ConstraintInfo& info = infos_[id];
        auto coeffs = GetCoeffs(info);
        auto vars = GetVariables(info);
        IntegerValue root_coeff(0);
        IntegerValue var_coeff(0);
        for (int i = 0; i < info.initial_size; ++i) {
          if (vars[i] == var) var_coeff = coeffs[i];
          if (vars[i] == NegationOf(root_var)) root_coeff = coeffs[i];
        }
        CHECK_NE(root_coeff, 0);
        CHECK_NE(var_coeff, 0);
        if (var_coeff >= root_coeff) {
          return ReportConflictingCycle();
        } else {
          // We don't want to continue the search from root_id.
          continue;
        }
      }

      if (id_to_propagation_count_[id] == 0) continue;  // Didn't push.
      disassemble_to_reorder_.Set(id);

      // The constraint pushed some variable. Identify which ones will be pushed
      // further. Disassemble the whole info since we are about to propagate
      // this constraint again. Any pushed variable must be before the rev_size.
      const ConstraintInfo& info = infos_[id];
      auto coeffs = GetCoeffs(info);
      auto vars = GetVariables(info);
      IntegerValue var_coeff(0);
      disassemble_candidates_.clear();
      ++num_explored_in_disassemble_;
      time_limit_->AdvanceDeterministicTime(static_cast<double>(info.rev_size) *
                                            1e-9);
      for (int i = 0; i < info.rev_size; ++i) {
        if (vars[i] == var) {
          var_coeff = coeffs[i];
          continue;
        }
        const IntegerVariable next_var = NegationOf(vars[i]);
        if (propagated_by_[next_var] == id) {
          disassemble_candidates_.push_back({next_var, coeffs[i]});

          // We will propagate var again later, so clear all this for now.
          propagated_by_[next_var] = -1;
          id_to_propagation_count_[id]--;
        }
      }
      for (const auto [next_var, coeff] : disassemble_candidates_) {
        if (coeff <= var_coeff) {
          // We are guaranteed to push next_var only if var_coeff will move
          // the slack enough.
          //
          // TODO(user): Keep current delta in term of the DFS so we detect
          // cycle and depedendences in more cases.
          disassemble_queue_.push_back({id, next_var});
        }
      }
    }
  }

  CHECK(!disassemble_to_reorder_[root_id]);
  tmp_to_reorder_.clear();
  std::reverse(disassemble_reverse_topo_order_.begin(),
               disassemble_reverse_topo_order_.end());  // !! not unique
  for (const int id : disassemble_reverse_topo_order_) {
    if (!disassemble_to_reorder_[id]) continue;
    disassemble_to_reorder_.Clear(id);
    AddToQueueIfNeeded(id);
    if (!propagation_queue_.Contains(id)) continue;
    tmp_to_reorder_.push_back(id);
  }

  // TODO(user): Reordering can be sloe since require sort and can touch many
  // entries. Investigate alternatives. We could probably optimize this a bit
  // more.
  if (tmp_to_reorder_.empty()) return true;
  const int important_size = static_cast<int>(tmp_to_reorder_.size());

  for (const int id : disassemble_to_reorder_.PositionsSetAtLeastOnce()) {
    if (!disassemble_to_reorder_[id]) continue;
    disassemble_to_reorder_.Clear(id);
    if (!propagation_queue_.Contains(id)) continue;
    tmp_to_reorder_.push_back(id);
  }
  disassemble_to_reorder_.NotifyAllClear();

  // We try to keep the same order as before for the elements not in the
  // topological order.
  propagation_queue_.SortByPos(
      absl::MakeSpan(&tmp_to_reorder_[important_size],
                     tmp_to_reorder_.size() - important_size));

  num_reordered_ += tmp_to_reorder_.size();
  propagation_queue_.Reorder(tmp_to_reorder_);
  return true;
}

void LinearPropagator::AddToQueueIfNeeded(int id) {
  DCHECK_LT(id, in_queue_.size());
  DCHECK_LT(id, infos_.size());

  if (in_queue_[id]) return;
  in_queue_[id] = true;
  propagation_queue_.Push(id);
}

void LinearPropagator::AddWatchedToQueue(IntegerVariable var) {
  if (var >= static_cast<int>(var_to_constraint_ids_.size())) return;
  time_limit_->AdvanceDeterministicTime(
      static_cast<double>(var_to_constraint_ids_[var].size()) * 1e-9);
  for (const int id : var_to_constraint_ids_[var]) {
    AddToQueueIfNeeded(id);
  }
}

void LinearPropagator::SetPropagatedBy(IntegerVariable var, int id) {
  int& ref_id = propagated_by_[var];
  if (ref_id == id) return;

  propagated_by_was_set_.Set(var);

  DCHECK_GE(var, 0);
  DCHECK_LT(var, propagated_by_.size());
  if (ref_id != -1) {
    DCHECK_GE(ref_id, 0);
    DCHECK_LT(ref_id, id_to_propagation_count_.size());
    id_to_propagation_count_[ref_id]--;
  }
  ref_id = id;
  if (id != -1) id_to_propagation_count_[id]++;
}

void LinearPropagator::ClearPropagatedBy() {
  // To be sparse, we use the fact that each node with a parent must be in
  // modified_vars_.
  for (const IntegerVariable var :
       propagated_by_was_set_.PositionsSetAtLeastOnce()) {
    int& id = propagated_by_[var];
    if (id != -1) --id_to_propagation_count_[id];
    propagated_by_[var] = -1;
  }
  propagated_by_was_set_.ClearAndResize(propagated_by_was_set_.size());
  DCHECK(std::all_of(propagated_by_.begin(), propagated_by_.end(),
                     [](int id) { return id == -1; }));
  DCHECK(std::all_of(id_to_propagation_count_.begin(),
                     id_to_propagation_count_.end(),
                     [](int count) { return count == 0; }));
}

}  // namespace sat
}  // namespace operations_research
