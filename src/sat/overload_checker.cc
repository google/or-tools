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

#include "sat/overload_checker.h"
#include "sat/sat_solver.h"

namespace operations_research {
namespace sat {

OverloadChecker::OverloadChecker(
    const std::vector<IntervalVariable>& interval_vars,
    const std::vector<IntegerVariable>& demand_vars, IntegerVariable capacity,
    IntegerTrail* integer_trail, IntervalsRepository* intervals_repository)
    : num_tasks_(interval_vars.size()),
      interval_vars_(interval_vars),
      demand_vars_(demand_vars),
      capacity_var_(capacity),
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
  }
}

namespace {
IntegerValue CeilOfDivision(IntegerValue a, IntegerValue b) {
  return (a + b - 1) / b;
}
}  // namespace

bool OverloadChecker::Propagate() {
  // Sort the tasks by start-min and end-max. Note that we reuse the current
  // order because it is often already sorted.
  for (const bool start_min_or_end_max : {true, false}) {
    bool already_sorted = true;
    IntegerValue prev = kMinIntegerValue;
    std::vector<TaskTime>& to_sort =
        start_min_or_end_max ? by_start_min_ : by_end_max_;
    for (TaskTime& ref : to_sort) {
      const IntegerValue value =
          start_min_or_end_max ? StartMin(ref.task_id) : EndMax(ref.task_id);
      ref.time = value;
      if (already_sorted) {
        if (value < prev) already_sorted = false;
        prev = value;
      }
    }
    if (!already_sorted) std::sort(to_sort.begin(), to_sort.end());
  }

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

    // Tasks with no energy have no impact in the algorithm. Skip them.
    if (DurationMin(task_id) == 0 || DemandMin(task_id) == 0) continue;

    // Insert the task in the Theta-tree. This will compute the envelope of the
    // left-cut ending with task task_id where the left-cut of task_id is the
    // set of all tasks having a maximum ending time that is lower or equal than
    // the maximum ending time of task_id.
    {
      // Compute the energy and envelope of the task.
      // TODO(user): This code will not work for negative start_min.
      // TODO(user): Deal with integer overflow.
      const int leaf_id = task_to_index_in_start_min_[task_id];
      const IntegerValue energy = DurationMin(task_id) * DemandMin(task_id);
      const IntegerValue envelope = StartMin(task_id) * capacity_max + energy;
      InsertTaskInThetaTree(task_id, leaf_id, energy, envelope);
    }

    // Compute the minimum capacity required to provide the left-cut with enough
    // energy. The minimum capacity is ceil(envelopes_[i] / EndMax(task_id)).
    const IntegerValue new_capacity_min =
        CeilOfDivision(node_envelopes_[1], by_end_max_[i].time);

    // Do not explain if the minimum capacity does not increase.
    if (new_capacity_min <= integer_trail_->LowerBound(capacity_var_)) continue;

    reason_.clear();

    // Compute the bounds of the task interval responsible for the value of the
    // root envelope.
    const IntegerValue interval_end = by_end_max_[i].time;
    const int interval_start_leaf = LeftMostInvolvedLeaf();
    const IntegerValue interval_start = by_start_min_[interval_start_leaf].time;
    for (int j = 0; j <= i; ++j) {
      const int t = by_end_max_[j].task_id;

      // Do not consider tasks that are not contained in the task interval.
      if (task_to_index_in_start_min_[t] < interval_start_leaf) continue;
      if (DurationMin(t) == 0 || DemandMin(t) == 0) continue;

      // Add the task to the explanation.
      reason_.push_back(
          IntegerLiteral::GreaterOrEqual(start_vars_[t], interval_start));
      reason_.push_back(
          IntegerLiteral::LowerOrEqual(end_vars_[t], interval_end));
      reason_.push_back(integer_trail_->LowerBoundAsLiteral(demand_vars_[t]));
      if (duration_vars_[t] != kNoIntegerVariable) {
        reason_.push_back(
            integer_trail_->LowerBoundAsLiteral(duration_vars_[t]));
      }
    }

    // Current capacity of the resource.
    reason_.push_back(integer_trail_->UpperBoundAsLiteral(capacity_var_));

    // Explain the increase of minimum capacity.
    if (!integer_trail_->Enqueue(
            IntegerLiteral::GreaterOrEqual(capacity_var_, new_capacity_min),
            /*literal_reason=*/{}, reason_)) {
      return false;
    }
  }
  return true;
}

void OverloadChecker::InsertTaskInThetaTree(int task_id, int leaf_id,
                                            IntegerValue energy,
                                            IntegerValue envelope) {
  DCHECK_GT(energy, kMinIntegerValue);
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
