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

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace operations_research {
namespace sat {

EnforcementPropagator::EnforcementPropagator(Model* model)
    : SatPropagator("EnforcementPropagator"),
      trail_(*model->GetOrCreate<Trail>()),
      assignment_(trail_.Assignment()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      rev_int_repository_(model->GetOrCreate<RevIntRepository>()) {
  // Note that this will be after the integer trail since rev_int_repository_
  // depends on IntegerTrail.
  model->GetOrCreate<SatSolver>()->AddPropagator(this);

  // The special id of zero is used for all enforced constraint at level zero.
  starts_.push_back(0);
  is_enforced_.push_back(true);
  callbacks_.push_back(nullptr);
}

bool EnforcementPropagator::Propagate(Trail* /*trail*/) {
  rev_int_repository_->SaveStateWithStamp(&rev_num_enforced_, &rev_stamp_);
  while (propagation_trail_index_ < trail_.Index()) {
    const Literal literal = trail_[propagation_trail_index_++];
    if (literal.Index() >= static_cast<int>(one_watcher_.size())) continue;

    int new_size = 0;
    auto& watch_list = one_watcher_[literal.Index()];
    for (const EnforcementId id : watch_list) {
      const LiteralIndex index = NewLiteralToWatchOrEnforced(id);
      if (index == kNoLiteralIndex) {
        // The constraint is now enforced, keep current watched literal.
        CHECK(!is_enforced_[id]);
        is_enforced_[id] = true;
        enforced_.push_back(id);
        watch_list[new_size++] = id;
        if (callbacks_[id] != nullptr) callbacks_[id]();
      } else {
        // Change one watcher.
        CHECK_NE(index, literal.Index());
        one_watcher_[index].push_back(id);
      }
    }
    watch_list.resize(new_size);
  }
  rev_num_enforced_ = static_cast<int>(enforced_.size());
  return true;
}

void EnforcementPropagator::Untrail(const Trail& /*trail*/, int trail_index) {
  // Simply unmark constraint no longer enforced.
  for (int i = rev_num_enforced_; i < enforced_.size(); ++i) {
    is_enforced_[enforced_[i]] = false;
  }
  enforced_.resize(rev_num_enforced_);
  propagation_trail_index_ = trail_index;
}

// Adds a new constraint to the class and returns the constraint id.
//
// Note that we accept empty enforcement list so that client code can be used
// regardless of the presence of enforcement or not. A negative id means the
// constraint is never enforced, and should be ignored.
EnforcementId EnforcementPropagator::Register(
    absl::Span<const Literal> enforcement) {
  CHECK_EQ(trail_.CurrentDecisionLevel(), 0);
  int num_true = 0;
  int num_false = 0;
  for (const Literal l : enforcement) {
    if (l.Index() >= static_cast<int>(one_watcher_.size())) {
      // Make sure we always have enough room for the watcher.
      one_watcher_.resize(l.Index() + 1);
    }
    if (assignment_.LiteralIsTrue(l)) ++num_true;
    if (assignment_.LiteralIsFalse(l)) ++num_false;
  }

  // Return special indices if never/always enforced.
  if (num_false > 0) return EnforcementId(-1);
  if (num_true == enforcement.size()) return EnforcementId(0);

  const EnforcementId id(starts_.size());
  starts_.push_back(buffer_.size());
  is_enforced_.push_back(false);
  callbacks_.push_back(nullptr);

  // TODO(user): No need to store literal fixed at level zero.
  // But then we could also filter the list as we fix stuff.
  buffer_.insert(buffer_.end(), enforcement.begin(), enforcement.end());
  for (const Literal l : enforcement) {
    if (!assignment_.LiteralIsAssigned(l)) {
      one_watcher_[l.Index()].push_back(id);
      break;
    }
  }

  return id;
}

// Note that the callback should just mark a constraint for future
// propagation, not propagate anything.
void EnforcementPropagator::CallWhenEnforced(EnforcementId id,
                                             std::function<void()> callback) {
  // Id zero is always enforced, no need to register anything
  if (id == 0) return;
  callbacks_[id] = std::move(callback);
}

// Returns true iff the constraint with given id is currently enforced.
bool EnforcementPropagator::IsEnforced(EnforcementId id) const {
  if (propagation_trail_index_ == trail_.Index()) return is_enforced_[id];

  // TODO(user): In some corner case, when LinearPropagator pushes a literal
  // we don't repropagate this right away. Maybe we should just simplify this
  // class and just pay the O(num_enforcement_literal) cost.
  //
  // For now we pay the cost to catch bugs.
  for (const Literal l : GetSpan(id)) {
    if (!assignment_.LiteralIsTrue(l)) return false;
  }
  return true;
}

// Add the enforcement reason to the given vector.
void EnforcementPropagator::AddEnforcementReason(
    EnforcementId id, std::vector<Literal>* reason) const {
  for (const Literal l : GetSpan(id)) {
    reason->push_back(l.Negated());
  }
}

// Returns true if IsEnforced(id) or if all literal are true but one.
// This is currently in O(enforcement_size);
bool EnforcementPropagator::CanPropagateWhenFalse(EnforcementId id) const {
  int num_true = 0;
  int num_false = 0;
  const auto span = GetSpan(id);
  for (const Literal l : span) {
    if (assignment_.LiteralIsTrue(l)) ++num_true;
    if (assignment_.LiteralIsFalse(l)) ++num_false;
  }
  return num_false == 0 && num_true + 1 >= span.size();
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

absl::Span<const Literal> EnforcementPropagator::GetSpan(
    EnforcementId id) const {
  const int size = id + 1 < static_cast<int>(starts_.size())
                       ? starts_[id + 1] - starts_[id]
                       : static_cast<int>(buffer_.size()) - starts_[id];
  if (size == 0) return {};
  return absl::MakeSpan(&buffer_[starts_[id]], size);
}

LiteralIndex EnforcementPropagator::NewLiteralToWatchOrEnforced(
    EnforcementId id) {
  // TODO(user): We could do some reordering to speed this up.
  for (const Literal l : GetSpan(id)) {
    if (assignment_.LiteralIsTrue(l)) continue;
    return l.Index();
  }
  return kNoLiteralIndex;
}

LinearPropagator::LinearPropagator(Model* model)
    : integer_trail_(model->GetOrCreate<IntegerTrail>()),
      enforcement_propagator_(model->GetOrCreate<EnforcementPropagator>()),
      watcher_(model->GetOrCreate<GenericLiteralWatcher>()),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      rev_int_repository_(model->GetOrCreate<RevIntRepository>()),
      rev_integer_value_repository_(
          model->GetOrCreate<RevIntegerValueRepository>()),
      watcher_id_(watcher_->Register(this)) {
  integer_trail_->RegisterWatcher(&modified_vars_);
  watcher_->SetPropagatorPriority(watcher_id_, 0);
}

bool LinearPropagator::Propagate() {
  // Initial addition.
  for (const IntegerVariable var : modified_vars_.PositionsSetAtLeastOnce()) {
    if (var >= var_to_constraint_ids_.size()) continue;
    AddWatchedToQueue(var);
  }

  // Sort by size first.
  // TODO(user): analyze impact of this better.
  std::sort(propagation_queue_.begin(), propagation_queue_.end(),
            [this](const int id1, const int id2) {
              return infos_[id1].rev_size < infos_[id2].rev_size;
            });

  // Empty FIFO queues.
  while (true) {
    // First the propagation one.
    while (!propagation_queue_.empty()) {
      const int id = propagation_queue_.front();
      propagation_queue_.pop_front();
      in_queue_[id] = false;
      if (!PropagateOneConstraint(id)) return ClearQueuesAndReturnFalse();
    }

    // Then the not_enforced_queue_.
    if (not_enforced_queue_.empty()) break;
    const int id = not_enforced_queue_.front();
    not_enforced_queue_.pop_front();
    in_queue_[id] = false;
    if (!PropagateOneConstraint(id)) return ClearQueuesAndReturnFalse();
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
  const EnforcementId enf_id =
      enforcement_propagator_->Register(enforcement_literals);
  if (enf_id < 0) return;  // Nothing to do.

  // Initialize constraint data.
  CHECK_EQ(vars.size(), coeffs.size());
  const int id = infos_.size();
  {
    ConstraintInfo info;
    info.enf_id = enf_id;
    info.start = variables_buffer_.size();
    info.initial_size = vars.size();
    info.rev_rhs = upper_bound;
    info.rev_size = vars.size();
    infos_.push_back(std::move(info));
  }

  variables_buffer_.insert(variables_buffer_.end(), vars.begin(), vars.end());
  coeffs_buffer_.insert(coeffs_buffer_.end(), coeffs.begin(), coeffs.end());
  CanonicalizeConstraint(id);

  // TODO(user): How Propagate() will be called on that event?
  // Add a function to LiteralWatcher?
  enforcement_propagator_->CallWhenEnforced(enf_id, [this, id]() {
    AddToQueueIfNeeded(id);
    watcher_->CallOnNextPropagate(watcher_id_);
  });

  // Initialize watchers.
  // Initialy we want everything to be propagated at least once.
  in_queue_.push_back(true);
  propagation_queue_.push_back(id);

  for (const IntegerVariable var : GetVariables(infos_[id])) {
    // Transposed graph to know which constraint to wake up.
    if (var >= var_to_constraint_ids_.size()) {
      var_to_constraint_ids_.resize(var + 1);
    }
    var_to_constraint_ids_[var].push_back(id);

    // We need to be registered to the watcher so Propagate() is called at
    // the proper priority. But then we rely on modified_vars_.
    if (var >= is_watched_.size()) {
      is_watched_.resize(var + 1);
    }
    if (!is_watched_[var]) {
      is_watched_[var] = true;
      watcher_->WatchLowerBound(var, watcher_id_);
    }
  }
}

absl::Span<IntegerValue> LinearPropagator::GetCoeffs(
    const ConstraintInfo& info) {
  return absl::MakeSpan(&coeffs_buffer_[info.start], info.initial_size);
}
absl::Span<IntegerVariable> LinearPropagator::GetVariables(
    const ConstraintInfo& info) {
  return absl::MakeSpan(&variables_buffer_[info.start], info.initial_size);
}

bool LinearPropagator::ClearQueuesAndReturnFalse() {
  for (const int id : propagation_queue_) in_queue_[id] = false;
  for (const int id : not_enforced_queue_) in_queue_[id] = false;
  propagation_queue_.clear();
  not_enforced_queue_.clear();
  return false;
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

bool LinearPropagator::PropagateOneConstraint(int id) {
  // Skip constraint not enforced or that cannot propagate if false.
  ConstraintInfo& info = infos_[id];
  if (!enforcement_propagator_->CanPropagateWhenFalse(info.enf_id)) {
    return true;
  }

  // Compute the slack and max_variations_ of each variables.
  // We also filter out fixed variables in a reversible way.
  IntegerValue implied_lb(0);
  auto coeffs = GetCoeffs(info);
  auto vars = GetVariables(info);
  max_variations_.resize(info.rev_size);
  bool first_change = true;
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
      ++i;
    }
  }
  const IntegerValue slack = info.rev_rhs - implied_lb;
  time_limit_->AdvanceDeterministicTime(static_cast<double>(vars.size()) *
                                        1e-9);

  // Negative slack means the constraint is false.
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

    return enforcement_propagator_->PropagateWhenFalse(info.enf_id, {},
                                                       integer_reason_);
  }

  // We can only propagate more if all the enforcement literals are true.
  if (!enforcement_propagator_->IsEnforced(info.enf_id)) return true;

  // The lower bound of all the variables except one can be used to update the
  // upper bound of the last one.
  for (int i = 0; i < info.rev_size; ++i) {
    if (max_variations_[i] <= slack) continue;

    // TODO(user): If the new ub fall into an hole of the variable, we can
    // actually relax the reason more by computing a better slack.
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
    if (integer_trail_->UpperBound(var) <= new_ub) {
      AddWatchedToQueue(NegationOf(var));
    }
  }

  return true;
}

void LinearPropagator::AddToQueueIfNeeded(int id) {
  if (in_queue_[id]) return;
  in_queue_[id] = true;
  if (enforcement_propagator_->IsEnforced(infos_[id].enf_id)) {
    propagation_queue_.push_back(id);
  } else {
    not_enforced_queue_.push_back(id);
  }
}

void LinearPropagator::AddWatchedToQueue(IntegerVariable var) {
  if (var >= static_cast<int>(var_to_constraint_ids_.size())) return;
  for (const int id : var_to_constraint_ids_[var]) {
    AddToQueueIfNeeded(id);
  }
}

}  // namespace sat
}  // namespace operations_research
