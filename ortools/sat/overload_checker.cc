// Copyright 2010-2018 Google LLC
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

#include <algorithm>
#include <functional>
#include <memory>

#include "ortools/base/logging.h"
#include "ortools/util/sort.h"

namespace operations_research {
namespace sat {

OverloadChecker::OverloadChecker(const std::vector<AffineExpression>& demands,
                                 AffineExpression capacity,
                                 SchedulingConstraintHelper* helper,
                                 IntegerTrail* integer_trail)
    : num_tasks_(helper->NumTasks()),
      demands_(demands),
      capacity_(capacity),
      helper_(helper),
      integer_trail_(integer_trail) {
  CHECK_GT(num_tasks_, 1);
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
  helper_->WatchAllTasks(id, watcher);
  watcher->WatchUpperBound(capacity_.var, id);
  for (int t = 0; t < num_tasks_; ++t) {
    watcher->WatchLowerBound(demands_[t].var, id);
  }
}

bool OverloadChecker::Propagate() {
  // Sort the tasks by start-min and end-max. Note that we reuse the current
  // order because it is often already sorted.
  helper_->SetTimeDirection(true);
  const std::vector<TaskTime>& by_increasing_smin =
      helper_->TaskByIncreasingStartMin();
  const std::vector<TaskTime>& by_decreasing_emax =
      helper_->TaskByDecreasingEndMax();
  CHECK_EQ(by_increasing_smin.size(), num_tasks_);
  CHECK_EQ(by_decreasing_emax.size(), num_tasks_);

  // Link each task to its position in by_start_min_.
  for (int i = 0; i < by_increasing_smin.size(); ++i) {
    task_to_index_in_start_min_[by_increasing_smin[i].task_index] = i;
  }

  // Resize the theta-tree and reset all its nodes.
  ResetThetaTree(num_tasks_);

  // Maximum capacity to not exceed.
  const IntegerValue capacity_max = integer_trail_->UpperBound(capacity_);

  // Build the left cuts and check for a possible overload.
  for (int i = num_tasks_ - 1; i >= 0; --i) {
    const int task_index = by_decreasing_emax[i].task_index;
    const bool is_present = helper_->IsPresent(task_index);

    // Tasks with no energy have no impact in the algorithm, we skip them. Note
    // that we will temporarily add an optional task whose presence is not yet
    // decided to the Theta-tree to try to prove that it cannot be present.
    if (helper_->DurationMin(task_index) == 0 || DemandMin(task_index) == 0 ||
        helper_->IsAbsent(task_index)) {
      continue;
    }

    // Insert the task in the Theta-tree. This will compute the envelope of the
    // left-cut ending with task task_index where the left-cut of task_index is
    // the set of all tasks having a maximum ending time that is lower or equal
    // than the maximum ending time of task_index.
    const int leaf_id = task_to_index_in_start_min_[task_index];
    {
      // Compute the energy and envelope of the task.
      // TODO(user): Deal with integer overflow.
      const IntegerValue energy =
          helper_->DurationMin(task_index) * DemandMin(task_index);
      const IntegerValue envelope =
          helper_->StartMin(task_index) * capacity_max + energy;
      InsertTaskInThetaTree(leaf_id, energy, envelope);
    }

    // The interval with the maximum energy per unit of time.
    const int interval_start_leaf = LeftMostInvolvedLeaf();
    const IntegerValue interval_start =
        by_increasing_smin[interval_start_leaf].time;
    const IntegerValue interval_end = by_decreasing_emax[i].time;
    const IntegerValue interval_size = interval_end - interval_start;

    // Compute the minimum capacity required to provide the interval above with
    // enough energy.
    CHECK_LE(interval_start * capacity_max, node_envelopes_[1]);
    const IntegerValue new_capacity_min = CeilRatio(
        node_envelopes_[1] - interval_start * capacity_max, interval_size);

    // Continue if we can't propagate anything, there is two cases.
    if (is_present) {
      if (new_capacity_min <= integer_trail_->LowerBound(capacity_)) {
        continue;
      }
    } else {
      if (new_capacity_min <= integer_trail_->UpperBound(capacity_)) {
        RemoveTaskFromThetaTree(leaf_id);
        continue;
      }
    }

    helper_->ClearReason();

    // Compute the bounds of the task interval responsible for the value of the
    // root envelope.
    for (int j = num_tasks_ - 1; j >= i; --j) {
      const int t = by_decreasing_emax[j].task_index;

      // Do not consider tasks that are not contained in the task interval.
      if (task_to_index_in_start_min_[t] < interval_start_leaf) continue;
      if (helper_->DurationMin(t) == 0 || DemandMin(t) == 0) continue;
      if (!helper_->IsPresent(t) && j != i) continue;

      // Add the task to the explanation.
      helper_->AddStartMinReason(t, interval_start);
      helper_->AddEndMaxReason(t, interval_end);
      helper_->AddDurationMinReason(t);
      if (demands_[t].var != kNoIntegerVariable) {
        helper_->MutableIntegerReason()->push_back(
            integer_trail_->LowerBoundAsLiteral(demands_[t].var));
      }
      if (j != i || is_present) helper_->AddPresenceReason(t);
    }

    // Current capacity of the resource.
    if (capacity_.var != kNoIntegerVariable) {
      helper_->MutableIntegerReason()->push_back(
          integer_trail_->UpperBoundAsLiteral(capacity_.var));
    }

    if (is_present) {
      if (capacity_.var == kNoIntegerVariable) {
        if (capacity_.constant >= new_capacity_min) return true;
        return helper_->ReportConflict();
      }

      // Increase the minimum capacity.
      if (!helper_->PushIntegerLiteral(
              capacity_.GreaterOrEqual(new_capacity_min))) {
        return false;
      }
    } else {
      // The task must be absent.
      if (!helper_->PushTaskAbsence(task_index)) return false;
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
