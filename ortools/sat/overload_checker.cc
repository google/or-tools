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

#include "ortools/sat/overload_checker.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/sort.h"

namespace operations_research {
namespace sat {

OverloadChecker::OverloadChecker(
    const std::vector<IntervalVariable>& interval_vars,
    const std::vector<IntegerVariable>& demand_vars, IntegerVariable capacity,
    Trail* trail, IntegerTrail* integer_trail,
    IntervalsRepository* intervals_repository)
    : num_tasks_(interval_vars.size()),
      interval_vars_(interval_vars),
      demand_vars_(demand_vars),
      capacity_var_(capacity),
      trail_(trail),
      integer_trail_(integer_trail),
      intervals_repository_(intervals_repository) {
  CHECK_GT(num_tasks_, 1);
  // Collect the variables.
  start_vars_.resize(num_tasks_);
  end_vars_.resize(num_tasks_);
  duration_vars_.resize(num_tasks_);
  for (int t = 0; t < num_tasks_; ++t) {
    const IntervalVariable i = interval_vars[t];
    start_vars_[t] = intervals_repository->StartVar(i);
    end_vars_[t] = intervals_repository->EndVar(i);
    duration_vars_[t] = intervals_repository->SizeVar(i);
  }
  // Initialize the data for the sorted tasks.
  by_start_min_.reserve(num_tasks_);
  by_end_max_.reserve(num_tasks_);
  for (int t = 0; t < num_tasks_; ++t) {
    by_start_min_.push_back(TaskTime(t, IntegerValue(0)));
    by_end_max_.push_back(TaskTime(t, IntegerValue(0)));
  }
  task_to_index_in_start_min_.resize(num_tasks_);
}

void OverloadChecker::ResetThetaTree(int num_tasks) {
  // Compute the position of the first and last leaves. The position of the
  // first leaf is the smallest power of two that is greater or equal to the
  // number of tasks, i.e., the first node of the last level of the tree.
  // Remember that the root node starts at position 1.
  first_leaf_ = num_tasks - 1;
  first_leaf_ |= first_leaf_ >> 16;
  first_leaf_ |= first_leaf_ >> 8;
  first_leaf_ |= first_leaf_ >> 4;
  first_leaf_ |= first_leaf_ >> 2;
  first_leaf_ |= first_leaf_ >> 1;
  first_leaf_ += 1;
  const int last_leaf = first_leaf_ + num_tasks - 1;
  // Add a dummy leaf to simplify the algorithm if the last leaf is not the
  // right child of its parent. Left children are always at an even position.
  const int tree_size = (last_leaf | 1) + 1;
  // Reset all the tree nodes.
  node_energies_.assign(tree_size, IntegerValue(0));
  node_envelopes_.assign(tree_size, kMinIntegerValue);
}

void OverloadChecker::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->WatchUpperBound(capacity_var_, id);
  for (int t = 0; t < num_tasks_; ++t) {
    watcher->WatchIntegerVariable(start_vars_[t], id);
    watcher->WatchIntegerVariable(end_vars_[t], id);
    watcher->WatchLowerBound(demand_vars_[t], id);
    if (duration_vars_[t] != kNoIntegerVariable) {
      watcher->WatchLowerBound(duration_vars_[t], id);
    }
    if (!IsPresent(t) && !IsAbsent(t)) {
      const Literal is_present =
          intervals_repository_->IsPresentLiteral(interval_vars_[t]);
      watcher->WatchLiteral(is_present, id);
    }
  }
}

namespace {
IntegerValue CeilOfDivision(IntegerValue a, IntegerValue b) {
  return (a + b - 1) / b;
}
}  // namespace

void OverloadChecker::AddPresenceReasonIfNeeded(int task_id) {
  if (intervals_repository_->IsOptional(interval_vars_[task_id])) {
    literal_reason_.push_back(
        intervals_repository_->IsPresentLiteral(interval_vars_[task_id])
            .Negated());
  }
}

bool OverloadChecker::IsPresent(int task_id) const {
  if (intervals_repository_->IsOptional(interval_vars_[task_id])) {
    const Literal is_present =
        intervals_repository_->IsPresentLiteral(interval_vars_[task_id]);
    return trail_->Assignment().LiteralIsTrue(is_present);
  }
  return true;
}

bool OverloadChecker::IsAbsent(int task_id) const {
  if (intervals_repository_->IsOptional(interval_vars_[task_id])) {
    const Literal is_present =
        intervals_repository_->IsPresentLiteral(interval_vars_[task_id]);
    return trail_->Assignment().LiteralIsFalse(is_present);
  }
  return false;
}

bool OverloadChecker::Propagate() {
  // Sort the tasks by start-min and end-max. Note that we reuse the current
  // order because it is often already sorted.
  for (int t = 0; t < num_tasks_; ++t) {
    by_start_min_[t].time = StartMin(by_start_min_[t].task_id);
    by_end_max_[t].time = EndMax(by_end_max_[t].task_id);
  }
  IncrementalSort(by_start_min_.begin(), by_start_min_.end());
  IncrementalSort(by_end_max_.begin(), by_end_max_.end());

  // Link each task to its position in by_start_min_.
  for (int i = 0; i < by_start_min_.size(); ++i) {
    task_to_index_in_start_min_[by_start_min_[i].task_id] = i;
  }

  // Resize the theta-tree and reset all its nodes.
  ResetThetaTree(by_start_min_.size());

  // Maximum capacity to not exceed.
  const IntegerValue capacity_max = integer_trail_->UpperBound(capacity_var_);

  // Build the left cuts and check for a possible overload.
  for (int i = 0; i < by_end_max_.size(); ++i) {
    const int task_id = by_end_max_[i].task_id;
    const bool is_present = IsPresent(task_id);

    // Tasks with no energy have no impact in the algorithm, we skip them. Note
    // that we will temporarily add an optional task whose presence is not yet
    // decided to the Theta-tree to try to prove that it cannot be present.
    if (DurationMin(task_id) == 0 || DemandMin(task_id) == 0 ||
        IsAbsent(task_id)) {
      continue;
    }

    // Insert the task in the Theta-tree. This will compute the envelope of the
    // left-cut ending with task task_id where the left-cut of task_id is the
    // set of all tasks having a maximum ending time that is lower or equal than
    // the maximum ending time of task_id.
    const int leaf_id = task_to_index_in_start_min_[task_id];
    {
      // Compute the energy and envelope of the task.
      // TODO(user): Deal with integer overflow.
      const IntegerValue energy = DurationMin(task_id) * DemandMin(task_id);
      const IntegerValue envelope = StartMin(task_id) * capacity_max + energy;
      InsertTaskInThetaTree(leaf_id, energy, envelope);
    }

    // The interval with the maximum energy per unit of time.
    const int interval_start_leaf = LeftMostInvolvedLeaf();
    const IntegerValue interval_start = by_start_min_[interval_start_leaf].time;
    const IntegerValue interval_end = by_end_max_[i].time;
    const IntegerValue interval_size = interval_end - interval_start;

    // Compute the minimum capacity required to provide the interval above with
    // enough energy.
    CHECK_LE(interval_start * capacity_max, node_envelopes_[1]);
    const IntegerValue new_capacity_min = CeilOfDivision(
        node_envelopes_[1] - interval_start * capacity_max, interval_size);

    // Continue if we can't propagate anything, there is two cases.
    if (is_present) {
      if (new_capacity_min <= integer_trail_->LowerBound(capacity_var_)) {
        continue;
      }
    } else {
      if (new_capacity_min <= integer_trail_->UpperBound(capacity_var_)) {
        RemoveTaskFromThetaTree(leaf_id);
        continue;
      }
    }

    integer_reason_.clear();
    literal_reason_.clear();

    // Compute the bounds of the task interval responsible for the value of the
    // root envelope.
    for (int j = 0; j <= i; ++j) {
      const int t = by_end_max_[j].task_id;

      // Do not consider tasks that are not contained in the task interval.
      if (task_to_index_in_start_min_[t] < interval_start_leaf) continue;
      if (DurationMin(t) == 0 || DemandMin(t) == 0) continue;
      if (!IsPresent(t) && j != i) continue;

      // Add the task to the explanation.
      integer_reason_.push_back(
          IntegerLiteral::GreaterOrEqual(start_vars_[t], interval_start));
      integer_reason_.push_back(
          IntegerLiteral::LowerOrEqual(end_vars_[t], interval_end));
      integer_reason_.push_back(
          integer_trail_->LowerBoundAsLiteral(demand_vars_[t]));
      if (duration_vars_[t] != kNoIntegerVariable) {
        integer_reason_.push_back(
            integer_trail_->LowerBoundAsLiteral(duration_vars_[t]));
      }
      if (j != i || is_present) AddPresenceReasonIfNeeded(t);
    }

    // Current capacity of the resource.
    integer_reason_.push_back(
        integer_trail_->UpperBoundAsLiteral(capacity_var_));

    if (is_present) {
      // Increase the minimum capacity.
      if (!integer_trail_->Enqueue(
              IntegerLiteral::GreaterOrEqual(capacity_var_, new_capacity_min),
              literal_reason_, integer_reason_)) {
        return false;
      }
    } else {
      // The task must be absent.
      integer_trail_->EnqueueLiteral(
          intervals_repository_->IsPresentLiteral(interval_vars_[task_id])
              .Negated(),
          literal_reason_, integer_reason_);
      RemoveTaskFromThetaTree(leaf_id);
    }
  }
  return true;
}

void OverloadChecker::InsertTaskInThetaTree(int leaf_id, IntegerValue energy,
                                            IntegerValue envelope) {
  DCHECK_GT(energy, 0);
  DCHECK_GT(envelope, kMinIntegerValue);
  const int leaf_node = first_leaf_ + leaf_id;
  DCHECK_LT(leaf_node, node_energies_.size());
  node_energies_[leaf_node] = energy;
  node_envelopes_[leaf_node] = envelope;
  int parent = leaf_node / 2;
  while (parent != 0) {
    DCHECK_LT(parent, first_leaf_);
    const int left = parent * 2;
    const int right = left + 1;
    node_energies_[parent] += energy;
    node_envelopes_[parent] = std::max(
        node_envelopes_[left] + node_energies_[right], node_envelopes_[right]);
    parent = parent / 2;
  }
}

void OverloadChecker::RemoveTaskFromThetaTree(int leaf_id) {
  const int leaf_node = first_leaf_ + leaf_id;
  DCHECK_LT(leaf_node, node_energies_.size());
  node_energies_[leaf_node] = IntegerValue(0);
  node_envelopes_[leaf_node] = kMinIntegerValue;
  int parent = leaf_node / 2;
  while (parent != 0) {
    DCHECK_LT(parent, first_leaf_);
    const int left = parent * 2;
    const int right = left + 1;
    node_energies_[parent] = node_energies_[left] + node_energies_[right];
    node_envelopes_[parent] = std::max(
        node_envelopes_[left] + node_energies_[right], node_envelopes_[right]);
    parent = parent / 2;
  }
}

int OverloadChecker::LeftMostInvolvedLeaf() const {
  int parent = 1;
  while (parent < first_leaf_) {
    const int left = parent * 2;
    const int right = left + 1;
    if (node_envelopes_[parent] == node_envelopes_[right]) {
      parent = right;
    } else {
      parent = left;
    }
  }
  return parent - first_leaf_;
}

}  // namespace sat
}  // namespace operations_research
