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
#include <string>
#include <utility>
#include <vector>

namespace operations_research {
namespace sat {

void CustomFifoQueue::IncreaseSize(int n) {
  CHECK_GE(n, pos_.size());
  CHECK_LE(left_, right_);
  pos_.resize(n, -1);

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

  tmp_positions_.clear();
  for (const int id : order) {
    const int p = pos_[id];
    DCHECK_GE(p, 0);
    tmp_positions_.push_back(p >= left_ ? p : p + queue_.size());
  }
  std::sort(tmp_positions_.begin(), tmp_positions_.end());
  DCHECK(std::unique(tmp_positions_.begin(), tmp_positions_.end()) ==
         tmp_positions_.end());

  int index = 0;
  for (const int id : order) {
    int p = tmp_positions_[index++];
    if (p >= queue_.size()) p -= queue_.size();
    pos_[id] = p;
    queue_[p] = id;
  }
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

  // The special id of zero is used for all enforced constraint at level zero.
  starts_.push_back(0);
  is_enforced_.push_back(true);
  callbacks_.push_back(nullptr);

  starts_.push_back(0);  // Sentinel - also start of next Register().
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

  const EnforcementId id(is_enforced_.size());
  is_enforced_.push_back(false);
  callbacks_.push_back(nullptr);

  // TODO(user): No need to store literal fixed at level zero.
  // But then we could also filter the list as we fix stuff.
  buffer_.insert(buffer_.end(), enforcement.begin(), enforcement.end());
  starts_.push_back(buffer_.size());  // Sentinel.

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
  if (id == 0) return true;
  int num_true = 0;
  const auto span = GetSpan(id);
  for (const Literal l : span) {
    if (assignment_.LiteralIsFalse(l)) return false;
    if (assignment_.LiteralIsTrue(l)) ++num_true;
  }
  return num_true + 1 >= span.size();
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
  DCHECK_LE(id + 1, starts_.size());
  const int size = starts_[id + 1] - starts_[id];
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
  stats.push_back(
      {"linear_propag/num_explored_in_disassemble", num_explored_constraints_});
  stats.push_back({"linear_propag/num_bool_aborts", num_bool_aborts_});
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

  id_to_propagation_count_.push_back(0);
  variables_buffer_.insert(variables_buffer_.end(), vars.begin(), vars.end());
  coeffs_buffer_.insert(coeffs_buffer_.end(), coeffs.begin(), coeffs.end());
  CanonicalizeConstraint(id);

  enforcement_propagator_->CallWhenEnforced(enf_id, [this, id]() {
    AddToQueueIfNeeded(id);
    watcher_->CallOnNextPropagate(watcher_id_);
  });

  // Initialize watchers.
  // Initialy we want everything to be propagated at least once.
  in_queue_.push_back(false);
  propagation_queue_.IncreaseSize(in_queue_.size());
  AddToQueueIfNeeded(id);

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

bool LinearPropagator::PropagateOneConstraint(int id) {
  if (VLOG_IS_ON(1)) {
    ++num_scanned_;
    if (id_scanned_at_least_once_[id]) {
      ++num_extra_scans_;
    } else {
      id_scanned_at_least_once_.Set(id);
    }
  }

  // Skip constraint not enforced or that cannot propagate if false.
  //
  // TODO(user): this can be expensive, in most situation we have a single
  // literal, so the only bad case if a constraint is fixed to false.
  ConstraintInfo& info = infos_[id];
  if (!enforcement_propagator_->CanPropagateWhenFalse(info.enf_id)) {
    return true;
  }

  // Compute the slack and max_variations_ of each variables.
  // We also filter out fixed variables in a reversible way.
  IntegerValue implied_lb(0);
  auto coeffs = GetCoeffs(info);
  auto vars = GetVariables(info);
  IntegerValue max_variation(0);
  max_variations_.resize(info.rev_size);
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
  // We also do a quick check to see if any variable can be pushed.
  if (max_variation <= slack) return true;
  if (!enforcement_propagator_->IsEnforced(info.enf_id)) return true;

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

      // We reorder them first.
      std::swap(vars[i], vars[num_pushed]);
      std::swap(coeffs[i], coeffs[num_pushed]);
      ++num_pushed;
    }

    // Explore the subtree and detect cycles greedily.
    // Also postpone some propagation.
    if (num_pushed > 0) {
      if (!DisassembleSubtreeAndAddToQueue(id, num_pushed)) {
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
  absl::StrAppend(&result,
                  " enf=", enforcement_propagator_->IsEnforced(info.enf_id));
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
bool LinearPropagator::DisassembleSubtreeAndAddToQueue(int root_id,
                                                       int num_pushed) {
  disassemble_scanned_ids_.ClearAndResize(in_queue_.size());
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
    disassemble_scanned_ids_.Set(root_id);
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
      // TODO(user): Check previous slack vs var coeff?
      // TODO(user): Make sure there are none or detect cycle not going back to
      // the root.
      if (id == root_id) {
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

      if (disassemble_scanned_ids_[id]) continue;
      disassemble_scanned_ids_.Set(id);

      AddToQueueIfNeeded(id);
      if (id_to_propagation_count_[id] == 0) continue;  // Didn't push.

      // The constraint pushed some variable. Identify which ones will be pushed
      // further. Disassemble the whole info since we are about to propagate
      // this constraint again. Any pushed variable must be before the rev_size.
      const ConstraintInfo& info = infos_[id];
      auto coeffs = GetCoeffs(info);
      auto vars = GetVariables(info);
      IntegerValue var_coeff(0);
      disassemble_candidates_.clear();
      ++num_explored_constraints_;
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

  // TODO(user): For the ids not in the topo order, we could try to keep their
  // relative order as much as possible.
  tmp_to_reorder_.clear();
  std::reverse(disassemble_reverse_topo_order_.begin(),
               disassemble_reverse_topo_order_.end());  // !! not unique
  for (const int id : disassemble_reverse_topo_order_) {
    if (!disassemble_scanned_ids_[id]) continue;
    disassemble_scanned_ids_.Clear(id);
    if (!propagation_queue_.Contains(id)) continue;
    tmp_to_reorder_.push_back(id);
  }
  for (const int id : disassemble_scanned_ids_.PositionsSetAtLeastOnce()) {
    if (!disassemble_scanned_ids_[id]) continue;
    disassemble_scanned_ids_.Clear(id);
    if (!propagation_queue_.Contains(id)) continue;
    tmp_to_reorder_.push_back(id);
  }
  disassemble_scanned_ids_.NotifyAllClear();

  // TODO(user): This is too slow since require sort and can touch many entries.
  // Investigate alternatives.
  propagation_queue_.Reorder(tmp_to_reorder_);
  return true;
}

void LinearPropagator::AddToQueueIfNeeded(int id) {
  if (in_queue_[id]) return;
  DCHECK_LT(id, in_queue_.size());
  DCHECK_LT(id, infos_.size());
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
