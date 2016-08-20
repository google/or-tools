// Copyright 2010-2014 Google
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

#include "sat/integer.h"

#include "base/stl_util.h"

namespace operations_research {
namespace sat {

bool IntegerTrail::Propagate(Trail* trail) {
  propagation_trail_index_ = trail->Index();

  // Make sure that our internal "integer_decision_levels_" size matches the
  // sat decision levels. At the level zero, integer_decision_levels_ should
  // be empty.
  if (trail->CurrentDecisionLevel() > integer_decision_levels_.size()) {
    integer_decision_levels_.push_back(integer_trail_.size());
    CHECK_EQ(trail->CurrentDecisionLevel(), integer_decision_levels_.size());
  }
  return true;
}

void IntegerTrail::Untrail(const Trail& trail, int literal_trail_index) {
  propagation_trail_index_ =
      std::min(propagation_trail_index_, literal_trail_index);

  // Note that if a conflict was detected before Propagate() of this class was
  // even called, it is possible that there is nothing to backtrack.
  const int decision_level = trail.CurrentDecisionLevel();
  if (decision_level >= integer_decision_levels_.size()) return;
  const int target = integer_decision_levels_[decision_level];
  integer_decision_levels_.resize(decision_level);
  CHECK_GE(target, vars_.size());

  // This is needed for the code below to work.
  if (target == integer_trail_.size()) return;

  for (int index = integer_trail_.size() - 1; index >= target; --index) {
    const TrailEntry& entry = integer_trail_[index];
    vars_[entry.var].current_trail_index = entry.prev_trail_index;
    vars_[entry.var].current_bound =
        integer_trail_[entry.prev_trail_index].bound;
  }

  // Resize vectors.
  literals_reason_buffer_.resize(
      integer_trail_[target].literals_reason_start_index);
  dependencies_buffer_.resize(integer_trail_[target].dependencies_start_index);
  integer_trail_.resize(target);
}

IntegerVariable IntegerTrail::AddIntegerVariable(int lower_bound,
                                                 int upper_bound) {
  CHECK(integer_decision_levels_.empty());
  CHECK_EQ(vars_.size(), integer_trail_.size());
  const IntegerVariable i(vars_.size() / 2);

  CHECK_EQ(LbVarOf(i).value(), vars_.size());
  vars_.push_back({lower_bound, static_cast<int>(integer_trail_.size())});
  integer_trail_.push_back({lower_bound, LbVarOf(i).value()});

  CHECK_EQ(MinusUbVarOf(i).value(), vars_.size());
  vars_.push_back({-upper_bound, static_cast<int>(integer_trail_.size())});
  integer_trail_.push_back({-upper_bound, MinusUbVarOf(i).value()});

  for (SparseBitset<LbVar>* w : watchers_) {
    w->Resize(LbVar(NumLbVars()));
  }
  return i;
}

int IntegerTrail::FindLowestTrailIndexThatExplainBound(
    IntegerLiteral i_lit) const {
  CHECK_LE(i_lit.bound, vars_[i_lit.var].current_bound);
  if (i_lit.bound <= LevelZeroBound(i_lit.var)) return -1;
  int prev_trail_index = vars_[i_lit.var].current_trail_index;
  int trail_index = prev_trail_index;
  while (i_lit.bound <= integer_trail_[trail_index].bound) {
    prev_trail_index = trail_index;
    trail_index = integer_trail_[trail_index].prev_trail_index;
    CHECK_GE(trail_index, 0);
  }
  return prev_trail_index;
}

void IntegerTrail::Enqueue(IntegerLiteral i_lit,
                           const std::vector<Literal>& literals_reason,
                           const std::vector<IntegerLiteral>& bounds_reason) {
  // Nothing to do if the bound is not better than the current one.
  if (i_lit.bound <= vars_[i_lit.var].current_bound) return;
  ++num_enqueues_;

  // Notify the watchers.
  for (SparseBitset<LbVar>* bitset : watchers_) bitset->Set(LbVar(i_lit.var));

  // Special case for level zero.
  if (integer_decision_levels_.empty()) {
    vars_[i_lit.var].current_bound = i_lit.bound;
    integer_trail_[i_lit.var].bound = i_lit.bound;
    return;
  }

  integer_trail_.push_back(
      {/*bound=*/i_lit.bound,
       /*var=*/i_lit.var,
       /*prev_trail_index=*/vars_[i_lit.var].current_trail_index,
       /*literals_reason_start_index=*/static_cast<int32>(
           literals_reason_buffer_.size()),
       /*dependencies_start_index=*/static_cast<int32>(
           dependencies_buffer_.size())});
  vars_[i_lit.var].current_bound = i_lit.bound;
  vars_[i_lit.var].current_trail_index = integer_trail_.size() - 1;

  // Copy literals_reason into our internal buffer.
  literals_reason_buffer_.insert(literals_reason_buffer_.end(),
                                 literals_reason.begin(),
                                 literals_reason.end());

  // Convert each IntegerLiteral reason to the index of an entry in the integer
  // trail.
  //
  // TODO(user): Do that lazily when the lazy reason are implemented.
  //
  // TODO(user): Check that the same LbVar never appear twice. If it does the
  // one with the lowest bound could be removed.
  const int size = vars_.size();
  for (IntegerLiteral i_lit : bounds_reason) {
    const int reason_tail_index = FindLowestTrailIndexThatExplainBound(i_lit);
    if (reason_tail_index >= size) {
      dependencies_buffer_.push_back(reason_tail_index);
    }
  }
}

BeginEndWrapper<std::vector<int>::const_iterator> IntegerTrail::Dependencies(
    int trail_index) const {
  return BeginEndRange(
      dependencies_buffer_.begin() +
          integer_trail_[trail_index].dependencies_start_index,
      trail_index + 1 < integer_trail_.size()
          ? dependencies_buffer_.begin() +
                integer_trail_[trail_index + 1].dependencies_start_index
          : dependencies_buffer_.end());
}

void IntegerTrail::AppendLiteralsReason(int trail_index,
                                        std::vector<Literal>* output) const {
  output->insert(
      output->end(),
      literals_reason_buffer_.begin() +
          integer_trail_[trail_index].literals_reason_start_index,
      trail_index + 1 < integer_trail_.size()
          ? literals_reason_buffer_.begin() +
                integer_trail_[trail_index + 1].literals_reason_start_index
          : literals_reason_buffer_.end());
}

std::vector<Literal> IntegerTrail::ReasonFor(IntegerLiteral literal) const {
  std::vector<Literal> reason;
  MergeReasonInto({literal}, &reason);
  return reason;
}

// TODO(user): If this is called many time on the same variables, it could be
// made faster by using some caching mecanism.
void IntegerTrail::MergeReasonInto(const std::vector<IntegerLiteral>& literals,
                                   std::vector<Literal>* output) const {
  DCHECK(tmp_queue_.empty());
  tmp_trail_indices_.clear();
  tmp_var_to_highest_explained_trail_index_.resize(vars_.size(), 0);
  DCHECK(std::all_of(tmp_var_to_highest_explained_trail_index_.begin(),
                     tmp_var_to_highest_explained_trail_index_.end(),
                     [](int v) { return v == 0; }));

  const int size = vars_.size();
  for (const IntegerLiteral& literal : literals) {
    const int trail_index = FindLowestTrailIndexThatExplainBound(literal);

    // Any indices lower than that means that there is no reason needed.
    // Note that it is important for size to be signed because of -1 indices.
    if (trail_index >= size) tmp_queue_.push_back(trail_index);
  }

  // This implement an iterative DFS on a DAG. Each time a node from the
  // tmp_queue_ is expanded, we change its sign, so that when we go back
  // (equivalent to the return of the recursive call), we can detect that this
  // node was already expanded.
  //
  // To detect nodes from which we already performed the full DFS exploration,
  // we use tmp_var_to_highest_explained_trail_index_.
  //
  // TODO(user): The order in which each trail_index is expanded will change
  // how much of the reason is "minimized". Investigate if some order are better
  // than other.
  while (!tmp_queue_.empty()) {
    const bool already_expored = tmp_queue_.back() < 0;
    const int trail_index = std::abs(tmp_queue_.back());
    const TrailEntry& entry = integer_trail_[trail_index];

    // Since we already have an explanation for a larger bound (ex: x>=4) we
    // don't need to add the explanation for a lower one (ex: x>=2).
    if (trail_index <=
        tmp_var_to_highest_explained_trail_index_[LbVar(entry.var)]) {
      tmp_queue_.pop_back();
      continue;
    }

    DCHECK_GT(trail_index, 0);
    if (already_expored) {
      // We are in the "return" of the DFS recursive call.
      DCHECK_GT(trail_index,
                tmp_var_to_highest_explained_trail_index_[LbVar(entry.var)]);
      tmp_var_to_highest_explained_trail_index_[LbVar(entry.var)] = trail_index;
      tmp_trail_indices_.push_back(trail_index);
      tmp_queue_.pop_back();
    } else {
      // We make "recursive calls" from this node.
      tmp_queue_.back() = -trail_index;
      for (const int next_trail_index : Dependencies(trail_index)) {
        const TrailEntry& next_entry = integer_trail_[next_trail_index];
        if (next_trail_index >
            tmp_var_to_highest_explained_trail_index_[LbVar(next_entry.var)]) {
          tmp_queue_.push_back(next_trail_index);
        }
      }
    }
  }

  // Cleanup + output the reason.
  for (const int trail_index : tmp_trail_indices_) {
    const TrailEntry& entry = integer_trail_[trail_index];
    tmp_var_to_highest_explained_trail_index_[LbVar(entry.var)] = 0;
    AppendLiteralsReason(trail_index, output);
  }
  STLSortAndRemoveDuplicates(output);
}

bool IntegerTrail::DomainIsEmpty(IntegerVariable i, Trail* trail) const {
  if (LowerBound(i) <= UpperBound(i)) return false;
  std::vector<Literal>* conflict = trail->MutableConflict();
  conflict->clear();

  // TODO(user): Avoid allocating memory here?
  MergeReasonInto({LowerBoundAsLiteral(i), UpperBoundAsLiteral(i)}, conflict);
  return true;
}

ClauseRef IntegerTrail::Reason(const Trail& trail, int trail_index) const {
  std::vector<Literal>* reason = trail.GetVectorToStoreReason(trail_index);
  *reason = literal_reasons_[trail_index];
  MergeReasonInto(integer_reasons_[trail_index], reason);
  return ClauseRef(*reason);
}

void IntegerTrail::EnqueueLiteral(Literal literal,
                                  std::vector<Literal>** literal_reason,
                                  std::vector<IntegerLiteral>** integer_reason,
                                  Trail* trail) {
  const int trail_index = trail->Index();
  if (trail_index >= literal_reasons_.size()) {
    literal_reasons_.resize(trail_index + 1);
    integer_reasons_.resize(trail_index + 1);
  }
  literal_reasons_[trail_index].clear();
  integer_reasons_[trail_index].clear();
  if (literal_reason != nullptr) {
    *literal_reason = &literal_reasons_[trail_index];
  }
  if (integer_reason != nullptr) {
    *integer_reason = &integer_reasons_[trail_index];
  }
  trail->Enqueue(literal, propagator_id_);
}

void IntegerTrail::EnqueueLiteral(Literal literal,
                                  const std::vector<Literal>& literal_reason,
                                  const std::vector<IntegerLiteral>& integer_reason,
                                  Trail* trail) {
  std::vector<Literal>* literal_reason_ptr;
  std::vector<IntegerLiteral>* integer_reason_ptr;
  EnqueueLiteral(literal, &literal_reason_ptr, &integer_reason_ptr, trail);
  *literal_reason_ptr = literal_reason;
  *integer_reason_ptr = integer_reason;
}

GenericLiteralWatcher::GenericLiteralWatcher(IntegerTrail* integer_trail)
    : Propagator("GenericLiteralWatcher"), integer_trail_(integer_trail) {
  integer_trail_->RegisterWatcher(&modified_vars_);
}

void GenericLiteralWatcher::UpdateCallingNeeds() {
  // Process the newly changed LbVars.
  for (const LbVar var : modified_vars_.PositionsSetAtLeastOnce()) {
    if (var.value() >= lb_var_to_watcher_ids_.size()) continue;
    for (const int id : lb_var_to_watcher_ids_[var]) {
      if (!in_queue_[id]) {
        in_queue_[id] = true;
        queue_.push_back(id);
      }
    }
  }
  const LbVar num_lb_vars(integer_trail_->NumLbVars());
  modified_vars_.ClearAndResize(num_lb_vars);
}

bool GenericLiteralWatcher::Propagate(Trail* trail) {
  while (propagation_trail_index_ < trail->Index()) {
    const Literal literal = (*trail)[propagation_trail_index_++];
    if (literal.Index() >= literal_to_watcher_ids_.size()) continue;
    for (const int id : literal_to_watcher_ids_[literal.Index()]) {
      if (!in_queue_[id]) {
        in_queue_[id] = true;
        queue_.push_back(id);
      }
    }
  }

  UpdateCallingNeeds();
  while (!queue_.empty()) {
    const int id = queue_.front();
    queue_.pop_front();

    if (!watchers_[id]->Propagate(trail)) {
      in_queue_[id] = false;
      return false;
    }
    UpdateCallingNeeds();

    // We mark the node afterwards because we assume that the Propagate() method
    // is idempotent and never need to be called twice in a row. If some
    // propagator don't have this property, we could add an option to call them
    // again until nothing changes.
    in_queue_[id] = false;
  }
  return true;
}

void GenericLiteralWatcher::Untrail(const Trail& trail, int trail_index) {
  if (propagation_trail_index_ > trail_index) {
    // This means that we already propagated all there is to propagate
    // at the level trail_index, so we can safely clear modified_vars_ in case
    // it wasn't already done.
    modified_vars_.ClearAndResize(LbVar(integer_trail_->NumLbVars()));
    in_queue_.assign(watchers_.size(), false);
    queue_.clear();
  }
  propagation_trail_index_ = trail_index;
}

// Registers a propagator and returns its unique ids.
int GenericLiteralWatcher::Register(PropagatorInterface* propagator) {
  const int id = watchers_.size();
  watchers_.push_back(propagator);

  // Initially call everything.
  in_queue_.resize(watchers_.size(), false);
  return id;
}

}  // namespace sat
}  // namespace operations_research
