// Copyright 2010-2024 Google LLC
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

#include "absl/base/log_severity.h"
#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
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
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

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
  bool is_always_false = false;
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
      if (level == 0 || trail_.Info(l.Variable()).level == 0) {
        is_always_false = true;
        break;
      }
      ++num_false;
    }
    temp_literals_.push_back(l);
  }
  gtl::STLSortAndRemoveDuplicates(&temp_literals_);

  // Return special indices if never/always enforced.
  if (is_always_false) {
    if (callback != nullptr)
      callback(EnforcementId(-1), EnforcementStatus::IS_FALSE);
    return EnforcementId(-1);
  }
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
                          ? EnforcementStatus::CAN_PROPAGATE
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
    ChangeStatus(id, EnforcementStatus::CAN_PROPAGATE);
    // Because this is the default status, we still need to call the callback.
    if (temp_literals_.size() == 1) {
      if (callbacks_[id] != nullptr) {
        callbacks_[id](id, EnforcementStatus::CAN_PROPAGATE);
      }
    }
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

  // We also change the status right away.
  ChangeStatus(id, EnforcementStatus::IS_FALSE);
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
  if (num_true + 1 == size) return EnforcementStatus::CAN_PROPAGATE;
  return EnforcementStatus::CANNOT_PROPAGATE;
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
      precedences_(model->GetOrCreate<PrecedenceRelations>()),
      random_(model->GetOrCreate<ModelRandomGenerator>()),
      shared_stats_(model->GetOrCreate<SharedStatistics>()),
      watcher_id_(watcher_->Register(this)),
      order_(random_, [this](int id) { return GetVariables(infos_[id]); }) {
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

  stats.push_back({"linear_propag/num_cycles", num_cycles_});
  stats.push_back({"linear_propag/num_failed_cycles", num_failed_cycles_});

  stats.push_back({"linear_propag/num_short_reasons_", num_short_reasons_});
  stats.push_back({"linear_propag/num_long_reasons_", num_long_reasons_});

  stats.push_back({"linear_propag/num_scanned", num_scanned_});
  stats.push_back({"linear_propag/num_explored_in_disassemble",
                   num_explored_in_disassemble_});
  stats.push_back({"linear_propag/num_bool_aborts", num_bool_aborts_});
  stats.push_back({"linear_propag/num_loop_aborts", num_loop_aborts_});
  stats.push_back({"linear_propag/num_ignored", num_ignored_});
  stats.push_back({"linear_propag/num_delayed", num_delayed_});
  shared_stats_->AddStats(stats);
}

void LinearPropagator::SetLevel(int level) {
  if (level < previous_level_) {
    order_.Clear();
    modified_vars_.ClearAll();

    // If the solver backtracked at any point, we invalidate all our queue
    // and propagated_by information.
    ClearPropagatedBy();
    for (const int id : propagation_queue_) in_queue_.Clear(id);
    propagation_queue_.clear();
    for (int i = rev_unenforced_size_; i < unenforced_constraints_.size();
         ++i) {
      in_queue_.Clear(unenforced_constraints_[i]);
    }
    unenforced_constraints_.resize(rev_unenforced_size_);
  } else if (level > previous_level_) {
    rev_unenforced_size_ = unenforced_constraints_.size();
    rev_int_repository_->SaveState(&rev_unenforced_size_);
  }

  // Tricky: if we aborted the current propagation because we pushed a Boolean,
  // by default, the GenericLiteralWatcher will only call Propagate() again if
  // one of the watched variable changed. With this, it is guaranteed to call
  // it again if it wasn't in the queue already.
  if (!propagation_queue_.empty() ||
      !modified_vars_.PositionsSetAtLeastOnce().empty() || !order_.IsEmpty()) {
    watcher_->CallOnNextPropagate(watcher_id_);
  }

  previous_level_ = level;
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

void LinearPropagator::OnVariableChange(IntegerVariable var, IntegerValue lb,
                                        int id) {
  // If no constraint use this var, we just ignore it.
  const int size = var_to_constraint_ids_[var].size();
  if (size == 0) return;

  SetPropagatedBy(var, id);
  order_.UpdateBound(var, lb);
  Bitset64<int>::View in_queue = in_queue_.view();
  time_limit_->AdvanceDeterministicTime(static_cast<double>(size) * 1e-9);
  for (const int id : var_to_constraint_ids_[var]) {
    if (in_queue[id]) continue;
    in_queue.Set(id);
    propagation_queue_.push_back(id);
  }
}

bool LinearPropagator::Propagate() {
  // Initial addition.
  //
  // We will clear modified_vars_ on exit since everything we propagate here
  // is handled by PropagateOneConstraint().
  for (const IntegerVariable var : modified_vars_.PositionsSetAtLeastOnce()) {
    if (var >= var_to_constraint_ids_.size()) continue;
    OnVariableChange(var, integer_trail_->LowerBound(var), -1);
  }

  // Cleanup.
  num_terms_for_dtime_update_ = 0;
  const auto cleanup = ::absl::MakeCleanup([this]() {
    time_limit_->AdvanceDeterministicTime(
        static_cast<double>(num_terms_for_dtime_update_) * 1e-9);
    modified_vars_.ClearAndResize(integer_trail_->NumIntegerVariables());
  });

  // We abort this propagator as soon as a Boolean is propagated, so that we
  // always finish the Boolean propagation first. This can happen when we push a
  // bound that has associated Booleans or push enforcement to false. The idea
  // is to resume from our current state when we are called again. Note however
  // that we have to clear the propagated_by_ info has other propagator might
  // have pushed the same variable further.
  //
  // TODO(user): More than the propagation speed, I think it is important to
  // have proper explanation, so if A pushes B, but later on the queue we have C
  // that push A that push B again, that might be bad? We can try to avoid this
  // even further, by organizing the queue in passes:
  //  - Scan all relevant constraints, remember who pushes but DO NOT push yet!
  //  - If no cycle, do not pushes constraint whose slack will changes due to
  //    other pushes.
  //  - consider the new constraint that need to be scanned and repeat.
  // I think it is okay to scan twice the constraints that push something in
  // order to get better explanation. We tend to diverge from the class shortest
  // path algo in this regard.
  //
  // TODO(user): If we push the idea further, can we first compute the fix point
  // without pushing anything, then compute a good order of constraints for the
  // explanations? what is tricky is that we might need to "scan" more than once
  // a constraint I think. ex: Y, Z, T >=0
  //  - 2 * Y + Z + T <= 11   ==>   Y <= 5, Z <= 11, T <= 11  (1)
  //  - Z + Y >= 6            ==>   Z >= 1
  //  - (1) again to push T <= 10  and reach the propagation fixed point.
  Bitset64<int>::View in_queue = in_queue_.view();
  while (true) {
    // We always process the whole queue in FIFO order.
    // Note that the order really only matter for infeasible constraint so it
    // shouldn't have a big impact.
    const int saved_index = trail_->Index();
    while (!propagation_queue_.empty()) {
      const int id = propagation_queue_.front();
      propagation_queue_.pop_front();
      in_queue.Clear(id);
      const auto [slack, num_to_push] = AnalyzeConstraint(id);
      if (slack < 0) {
        // This is either a conflict or an enforcement propagation.
        // We do it right away.
        if (!PropagateInfeasibleConstraint(id, slack)) return false;

        // We abort after the first pushed boolean. We could abort later too,
        // it  is unclear what works best.
        //
        // TODO(user): Maybe we should "update" explanation if we have a shorter
        // one to be less reliant on the propagation order.
        if (trail_->Index() > saved_index) {
          ++num_bool_aborts_;
          return true;
        }
      } else if (num_to_push > 0) {
        // Note that the last constraint added in to_propagate_ should never
        // be "skipped" and have any variable marked as var_will_change_.
        const auto vars = GetVariables(infos_[id]);
        const auto coeffs = GetCoeffs(infos_[id]);
        for (int i = 0; i < num_to_push; ++i) {
          const IntegerVariable var = vars[i];
          const IntegerValue coeff = coeffs[i];
          const IntegerValue div = slack / coeff;
          const IntegerValue new_ub = integer_trail_->LowerBound(var) + div;
          order_.Register(id, NegationOf(var), -new_ub);
        }
      }
    }

    const int next_id = order_.NextId();
    if (next_id == -1) break;

    // We can probably save the AnalyzeConstraint() cost, but then we only do
    // that when the constraint propagate, and the context might have change
    // since we computed it above.
    if (!PropagateOneConstraint(next_id)) return false;

    // TODO(user): This do not seems always good, especially since we pushed
    // Boolean with a really small explanation, maybe we want to push more of
    // these rather than go back to pure-binary propagation.
    if (trail_->Index() > saved_index) {
      ++num_bool_aborts_;
      return true;
    }
  }
  return true;
}

// Adds a new constraint to the propagator.
bool LinearPropagator::AddConstraint(
    absl::Span<const Literal> enforcement_literals,
    absl::Span<const IntegerVariable> vars,
    absl::Span<const IntegerValue> coeffs, IntegerValue upper_bound) {
  if (vars.empty()) return true;
  if (trail_->CurrentDecisionLevel() == 0) {
    for (const Literal l : enforcement_literals) {
      if (trail_->Assignment().LiteralIsFalse(l)) return true;
    }
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
  in_queue_.resize(in_queue_.size() + 1);

  if (!enforcement_literals.empty()) {
    infos_.back().enf_status =
        static_cast<int>(EnforcementStatus::CANNOT_PROPAGATE);
    infos_.back().enf_id = enforcement_propagator_->Register(
        enforcement_literals,
        [this, id](EnforcementId enf_id, EnforcementStatus status) {
          infos_[id].enf_status = static_cast<int>(status);
          // TODO(user): With some care, when we cannot propagate or the
          // constraint is not enforced, we could leave in_queue_[] at true but
          // not put the constraint in the queue.
          if (status == EnforcementStatus::CAN_PROPAGATE ||
              status == EnforcementStatus::IS_ENFORCED) {
            AddToQueueIfNeeded(id);
            watcher_->CallOnNextPropagate(watcher_id_);
          }

          // When a conditional precedence becomes enforced, add it. Note that
          // we cannot just use rev_size == 2 since we might miss some
          // explanation if a longer constraint only have 2 non-fixed variable
          // now.. It is however okay not to push precedence involving a fixed
          // variable, since these should be reflected in the variable domain
          // anyway.
          if (status == EnforcementStatus::IS_ENFORCED) {
            const auto info = infos_[id];
            if (info.initial_size == 2 && info.rev_size == 2 &&
                info.all_coeffs_are_one) {
              const auto vars = GetVariables(info);
              precedences_->PushConditionalRelation(
                  enforcement_propagator_->GetEnforcementLiterals(enf_id),
                  vars[0], vars[1], info.rev_rhs);
            }
          }
        });
  } else {
    // TODO(user): Shall we register root level precedence from here rather than
    // separately?
    AddToQueueIfNeeded(id);
    infos_.back().enf_id = -1;
    infos_.back().enf_status = static_cast<int>(EnforcementStatus::IS_ENFORCED);
  }

  order_.Resize(var_to_constraint_ids_.size(), in_queue_.size());
  for (const IntegerVariable var : GetVariables(infos_[id])) {
    // Transposed graph to know which constraint to wake up.
    if (var >= var_to_constraint_ids_.size()) {
      // We need both the var entry and its negation to be allocated.
      const int size = std::max(var, NegationOf(var)).value() + 1;
      var_to_constraint_ids_.resize(size);
      propagated_by_.resize(size, -1);
      propagated_by_was_set_.Resize(IntegerVariable(size));
      is_watched_.resize(size, false);

      order_.Resize(size, in_queue_.size());
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

  // Propagate this new constraint.
  // TODO(user): Do we want to do that?
  num_terms_for_dtime_update_ = 0;
  const auto cleanup = ::absl::MakeCleanup([this]() {
    time_limit_->AdvanceDeterministicTime(
        static_cast<double>(num_terms_for_dtime_update_) * 1e-9);
  });
  if (!PropagateOneConstraint(id)) return false;
  return true;
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
  const auto coeffs = GetCoeffs(info);
  const auto vars = GetVariables(info);
  for (int i = 0; i < vars.size(); ++i) {
    if (coeffs[i] < 0) {
      coeffs[i] = -coeffs[i];
      vars[i] = NegationOf(vars[i]);
    }
  }

  // Note that we DO NOT support having both var and NegationOf(var) in a
  // constraint, that would break the algo.
  if (DEBUG_MODE) {
    absl::flat_hash_set<IntegerVariable> no_dup;
    for (const IntegerVariable var : GetVariables(info)) {
      auto [_, inserted] = no_dup.insert(PositiveVariable(var));
      CHECK(inserted);
    }
  }
}

// TODO(user): template everything for the case info.all_coeffs_are_one ?
std::pair<IntegerValue, int> LinearPropagator::AnalyzeConstraint(int id) {
  ++num_scanned_;

  // Skip constraint not enforced or that cannot propagate if false.
  ConstraintInfo& info = infos_[id];
  const EnforcementStatus enf_status = EnforcementStatus(info.enf_status);
  if (DEBUG_MODE && enforcement_propagator_->PropagationIsDone(*trail_)) {
    const EnforcementStatus debug_status =
        enforcement_propagator_->DebugStatus(info.enf_id);
    if (enf_status != debug_status) {
      if (enf_status == EnforcementStatus::CANNOT_PROPAGATE &&
          debug_status == EnforcementStatus::IS_FALSE) {
        // This case might happen because in our two watched literals scheme,
        // we might watch two unassigned literal without knowing another one is
        // already false.
      } else {
        LOG(FATAL) << "Enforcement status not up to date: " << enf_status
                   << " vs debug: " << debug_status;
      }
    }
  }

  if (enf_status == EnforcementStatus::IS_FALSE ||
      enf_status == EnforcementStatus::CANNOT_PROPAGATE) {
    DCHECK(!in_queue_[id]);
    if (enf_status == EnforcementStatus::IS_FALSE) {
      // We mark this constraint as in the queue but will never inspect it
      // again until we backtrack over this time.
      in_queue_.Set(id);
      unenforced_constraints_.push_back(id);
    }
    ++num_ignored_;
    return {0, 0};
  }

  // Compute the slack and max_variations_ of each variables.
  // We also filter out fixed variables in a reversible way.
  IntegerValue implied_lb(0);
  const auto vars = GetVariables(info);
  IntegerValue max_variation(0);
  bool first_change = true;
  num_terms_for_dtime_update_ += info.rev_size;
  IntegerValue* max_variations = max_variations_.data();
  const IntegerValue* lower_bounds = integer_trail_->LowerBoundsData();
  if (info.all_coeffs_are_one) {
    // TODO(user): Avoid duplication?
    for (int i = 0; i < info.rev_size;) {
      const IntegerVariable var = vars[i];
      const IntegerValue lb = lower_bounds[var.value()];
      const IntegerValue diff = -lower_bounds[NegationOf(var).value()] - lb;
      if (diff == 0) {
        if (first_change) {
          // Note that we can save at most one state per fixed var. Also at
          // level zero we don't save anything.
          rev_int_repository_->SaveState(&info.rev_size);
          rev_integer_value_repository_->SaveState(&info.rev_rhs);
          first_change = false;
        }
        info.rev_size--;
        std::swap(vars[i], vars[info.rev_size]);
        info.rev_rhs -= lb;
      } else {
        implied_lb += lb;
        max_variations[i] = diff;
        max_variation = std::max(max_variation, diff);
        ++i;
      }
    }
  } else {
    const auto coeffs = GetCoeffs(info);
    for (int i = 0; i < info.rev_size;) {
      const IntegerVariable var = vars[i];
      const IntegerValue coeff = coeffs[i];
      const IntegerValue lb = lower_bounds[var.value()];
      const IntegerValue diff = -lower_bounds[NegationOf(var).value()] - lb;
      if (diff == 0) {
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
        max_variations[i] = diff * coeff;
        max_variation = std::max(max_variation, max_variations[i]);
        ++i;
      }
    }
  }

  // What we call slack here is the "room" between the implied_lb and the rhs.
  // Note that we use slack in other context in this file too.
  const IntegerValue slack = info.rev_rhs - implied_lb;

  // Negative slack means the constraint is false.
  // Note that if max_variation > slack, we are sure to propagate something
  // except if the constraint is enforced and the slack is non-negative.
  if (slack < 0 || max_variation <= slack) return {slack, 0};
  if (enf_status == EnforcementStatus::IS_ENFORCED) {
    // Swap the variable(s) that will be pushed at the beginning.
    int num_to_push = 0;
    const auto coeffs = GetCoeffs(info);
    for (int i = 0; i < info.rev_size; ++i) {
      if (max_variations[i] <= slack) continue;
      std::swap(vars[i], vars[num_to_push]);
      std::swap(coeffs[i], coeffs[num_to_push]);
      ++num_to_push;
    }
    return {slack, num_to_push};
  }
  return {slack, 0};
}

bool LinearPropagator::PropagateInfeasibleConstraint(int id,
                                                     IntegerValue slack) {
  DCHECK_LT(slack, 0);
  const ConstraintInfo& info = infos_[id];
  const auto vars = GetVariables(info);
  const auto coeffs = GetCoeffs(info);

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

bool LinearPropagator::PropagateOneConstraint(int id) {
  const auto [slack, num_to_push] = AnalyzeConstraint(id);
  if (slack < 0) return PropagateInfeasibleConstraint(id, slack);
  if (num_to_push == 0) return true;

  DCHECK_GT(num_to_push, 0);
  DCHECK_GE(slack, 0);
  const ConstraintInfo& info = infos_[id];
  const auto vars = GetVariables(info);
  const auto coeffs = GetCoeffs(info);

  // We can only propagate more if all the enforcement literals are true.
  // But this should have been checked by SkipConstraint().
  CHECK_EQ(info.enf_status, static_cast<int>(EnforcementStatus::IS_ENFORCED));

  // We can look for disasemble before the actual push.
  // This should lead to slighly better reason.
  // Explore the subtree and detect cycles greedily.
  // Also postpone some propagation.
  if (!DisassembleSubtree(id, num_to_push)) {
    return false;
  }

  // The lower bound of all the variables except one can be used to update the
  // upper bound of the last one.
  int num_pushed = 0;
  for (int i = 0; i < num_to_push; ++i) {
    if (!order_.VarShouldBePushedById(NegationOf(vars[i]), id)) {
      ++num_delayed_;
      continue;
    }

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

              const auto coeffs = GetCoeffs(info);
              const auto vars = GetVariables(info);
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
      OnVariableChange(next_var, -actual_ub, -1);
    } else if (actual_ub == new_ub) {
      OnVariableChange(next_var, -actual_ub, id);

      // We reorder them first.
      std::swap(vars[i], vars[num_pushed]);
      std::swap(coeffs[i], coeffs[num_pushed]);
      ++num_pushed;
    } else {
      // The bound was not pushed because we think we are in a propagation loop.
      ++num_loop_aborts_;
    }
  }

  return true;
}

std::string LinearPropagator::ConstraintDebugString(int id) {
  std::string result;
  const ConstraintInfo& info = infos_[id];
  const auto coeffs = GetCoeffs(info);
  const auto vars = GetVariables(info);
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
    for (const auto [id, next_var, increase] : disassemble_branch_) {
      const ConstraintInfo& info = infos_[id];
      enforcement_propagator_->AddEnforcementReason(info.enf_id,
                                                    &literal_reason_);
      const auto coeffs = GetCoeffs(info);
      const auto vars = GetVariables(info);
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

      ++num_short_reasons_;
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
  for (const auto [id, next_var, increase] : disassemble_branch_) {
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
  ++num_long_reasons_;
  return integer_trail_->ReportConflict(literal_reason_, integer_reason_);
}

std::pair<IntegerValue, IntegerValue> LinearPropagator::GetCycleCoefficients(
    int id, IntegerVariable var, IntegerVariable next_var) {
  const ConstraintInfo& info = infos_[id];
  const auto coeffs = GetCoeffs(info);
  const auto vars = GetVariables(info);
  IntegerValue var_coeff(0);
  IntegerValue next_coeff(0);
  for (int i = 0; i < info.initial_size; ++i) {
    if (vars[i] == var) var_coeff = coeffs[i];
    if (vars[i] == NegationOf(next_var)) next_coeff = coeffs[i];
  }
  DCHECK_NE(var_coeff, 0);
  DCHECK_NE(next_coeff, 0);
  return {var_coeff, next_coeff};
}

// Note that if there is a loop in the propagated_by_ graph, it must be
// from root_id -> root_var, because each time we add an edge, we do
// disassemble.
//
// TODO(user): If one of the var coeff is > previous slack we push an id again,
// we can stop early with a conflict by propagating the ids in sequence.
//
// TODO(user): Revisit the algo, no point exploring twice the same var, also
// the queue reordering heuristic might not be the best.
bool LinearPropagator::DisassembleSubtree(int root_id, int num_tight) {
  // The variable was just pushed, we explore the set of variable that will
  // be pushed further due to this push. Basically, if a constraint propagated
  // before and its slack will reduce due to the push, then any previously
  // propagated variable with a coefficient NOT GREATER than the one of the
  // variable reducing the slack will be pushed further.
  disassemble_queue_.clear();
  disassemble_branch_.clear();
  {
    const ConstraintInfo& info = infos_[root_id];
    const auto vars = GetVariables(info);
    for (int i = 0; i < num_tight; ++i) {
      disassemble_queue_.push_back({root_id, NegationOf(vars[i]), 1});
    }
  }

  // Note that all var should be unique since there is only one propagated_by_
  // for each one. And each time we explore an id, we disassemble the tree.
  absl::Span<int> id_to_count = absl::MakeSpan(id_to_propagation_count_);
  while (!disassemble_queue_.empty()) {
    const auto [prev_id, var, increase] = disassemble_queue_.back();
    if (!disassemble_branch_.empty() &&
        disassemble_branch_.back().id == prev_id &&
        disassemble_branch_.back().var == var) {
      disassemble_branch_.pop_back();
      disassemble_queue_.pop_back();
      continue;
    }

    disassemble_branch_.push_back({prev_id, var, increase});

    time_limit_->AdvanceDeterministicTime(
        static_cast<double>(var_to_constraint_ids_[var].size()) * 1e-9);
    for (const int id : var_to_constraint_ids_[var]) {
      if (id == root_id) {
        // TODO(user): Check previous slack vs var coeff?
        // TODO(user): Make sure there are none or detect cycle not going back
        // to the root.
        CHECK(!disassemble_branch_.empty());

        // This is a corner case in which there is actually no cycle.
        const auto [test_id, root_var, var_increase] = disassemble_branch_[0];
        CHECK_EQ(test_id, root_id);
        CHECK_NE(var, root_var);
        if (var == NegationOf(root_var)) continue;

        // Simple case, we have a cycle var -> root_var -> ... -> var where
        // all coefficient are non-increasing.
        const auto [var_coeff, root_coeff] =
            GetCycleCoefficients(id, var, root_var);
        if (CapProdI(var_increase, var_coeff) >= root_coeff) {
          ++num_cycles_;
          return ReportConflictingCycle();
        }

        // We don't want to continue the search from root_id.
        // TODO(user): We could still try the simple reason, it might be a
        // conflict.
        ++num_failed_cycles_;
        continue;
      }

      if (id_to_count[id] == 0) continue;  // Didn't push or was desassembled.

      // The constraint pushed some variable. Identify which ones will be pushed
      // further. Disassemble the whole info since we are about to propagate
      // this constraint again. Any pushed variable must be before the rev_size.
      const ConstraintInfo& info = infos_[id];
      const auto coeffs = GetCoeffs(info);
      const auto vars = GetVariables(info);
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
          id_to_count[id]--;
        }
      }

      for (const auto [next_var, next_var_coeff] : disassemble_candidates_) {
        // If var was pushed by increase, next_var is pushed by
        // (var_coeff * increase) / next_var_coeff.
        //
        // Note that it is okay to underevalute the increase in case of
        // overflow.
        const IntegerValue next_increase =
            FloorRatio(CapProdI(var_coeff, increase), next_var_coeff);
        if (next_increase > 0) {
          disassemble_queue_.push_back({id, next_var, next_increase});

          // We know this will push later, so we register hit with a sentinel
          // value so that it do not block any earlier propagation. Hopefully,
          // adding this "dependency" should help find a better propagation
          // order.
          order_.Register(id, next_var, kMinIntegerValue);
        }
      }
    }
  }

  return true;
}

void LinearPropagator::AddToQueueIfNeeded(int id) {
  DCHECK_LT(id, in_queue_.size());
  DCHECK_LT(id, infos_.size());

  if (in_queue_[id]) return;
  in_queue_.Set(id);
  propagation_queue_.push_back(id);
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
