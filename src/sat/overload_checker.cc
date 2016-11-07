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
  // Allocate space for the sorted tasks.
  by_start_min_.reserve(num_tasks_);
  by_end_max_.reserve(num_tasks_);
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
  const int last_leaf_ = first_leaf_ + num_tasks - 1;
  // Add a dummy leaf to simplify the algorithm if the last leaf is not the
  // right child of its parent. Left children are always at an even position.
  const int tree_size_ = (last_leaf_ | 1) + 1;
  // Reset all the tree nodes.
  energies_.assign(tree_size_, IntegerValue(0));
  envelopes_.assign(tree_size_, kMinIntegerValue);
  leaf_to_task_.resize(num_tasks_);
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

bool OverloadChecker::Propagate() {
  // Sort the tasks by start min and end max.
  by_start_min_.clear();
  by_end_max_.clear();
  for (int t = 0; t < num_tasks_; t++) {
    // Tasks with no energy have no impact in the algorithm and are not
    // considered in the remainder of this function.
    if (DurationMin(t) > 0 && DemandMin(t) > 0) {
      by_start_min_.push_back(TaskTime(t, StartMin(t)));
      by_end_max_.push_back(TaskTime(t, EndMax(t)));
    }
  }
  std::sort(by_start_min_.begin(), by_start_min_.end());
  std::sort(by_end_max_.begin(), by_end_max_.end());

  // Link each unskipped task to its position in by_start_min_.
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
    const int leaf_id = task_to_index_in_start_min_[task_id];

    // Compute the energy and envelope of the task.
    const IntegerValue energy = DurationMin(task_id) * DemandMin(task_id);
    const IntegerValue envelope = StartMin(task_id) * capacity_max + energy;

    DCHECK_GT(energy, kMinIntegerValue);
    DCHECK_GT(envelope, kMinIntegerValue);

    // Insert the task in the Theta-tree. This will compute the envelope of the
    // left-cut ending with task task_id where the left-cut of task_id is the
    // set of all tasks having a maximum ending time that is lower or equal than
    // the maximum ending time of task_id.
    InsertTaskInThetaTree(task_id, leaf_id, energy, envelope);

    // Compute the minimum capacity required to provide the left-cut with enough
    // energy. The minimum capacity is ceil(envelopes_[i] / EndMax(task_id)).
    IntegerValue new_capacity_min = envelopes_[1] / EndMax(task_id);
    if (envelopes_[1] % EndMax(task_id) != 0) new_capacity_min++;

    // Do not explain if the minimum capacity does not increase.
    if (new_capacity_min <= integer_trail_->LowerBound(capacity_var_)) continue;

    reason_.clear();

    // Collect the tasks responsible for the value of the envelope.
    ExplainEnvelope(1);

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
  const int leaf_node = first_leaf_ + leaf_id;
  DCHECK_LT(leaf_node, energies_.size());
  leaf_to_task_[leaf_id] = task_id;
  energies_[leaf_node] = energy;
  envelopes_[leaf_node] = envelope;
  int parent = leaf_node / 2;
  while (parent != 0) {
    DCHECK_LT(parent, first_leaf_);
    const int left = parent * 2;
    const int right = left + 1;
    energies_[parent] = energies_[left] + energies_[right];
    envelopes_[parent] =
        std::max(envelopes_[left] + energies_[right], envelopes_[right]);
    parent = parent / 2;
  }
}

void OverloadChecker::ExplainEnvelope(int parent) {
  if (parent >= first_leaf_) {
    const int t = leaf_to_task_[parent - first_leaf_];
    reason_.push_back(integer_trail_->UpperBoundAsLiteral(end_vars_[t]));
    reason_.push_back(integer_trail_->LowerBoundAsLiteral(start_vars_[t]));
    reason_.push_back(integer_trail_->LowerBoundAsLiteral(demand_vars_[t]));
    if (duration_vars_[t] != kNoIntegerVariable) {
      reason_.push_back(integer_trail_->LowerBoundAsLiteral(duration_vars_[t]));
    }
  } else {
    const int left = parent * 2;
    const int right = left + 1;
    if (envelopes_[right] == kMinIntegerValue) {
      ExplainEnvelope(left);
    } else if (envelopes_[right] == envelopes_[parent]) {
      ExplainEnvelope(right);
    } else {
      ExplainEnvelope(left);
      ExplainEnvelope(right);
    }
  }
}

}  // namespace sat
}  // namespace operations_research
