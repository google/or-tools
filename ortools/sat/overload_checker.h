// Copyright 2010-2017 Google
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

#ifndef OR_TOOLS_SAT_OVERLOAD_CHECKER_H_
#define OR_TOOLS_SAT_OVERLOAD_CHECKER_H_

#include <vector>

#include "ortools/base/macros.h"
#include "ortools/base/int_type.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {

// Overload Checker
//
// This propagator implements the overload checker filtering rule presented in
// Vilim Petr, "Max Energy Filtering Algorithm for Discrete Cumulative
// Constraint", CPAIOR 2009, http://vilim.eu/petr/cpaior2009.pdf.
//
// This propagator only increases the minimum of the capacity variable or fails
// if the minimum capacity cannot be increased. It has a time complexity of
// O(n log n).
//
// The propagator relies on a Theta-tree to maintain the energy and envelope of
// several set of tasks.
//
// The energy of a task can be seen as its surface and is the product of its
// minimum demand and minimum duration. The energy of a set of tasks is the sum
// of the energy of its tasks.
//
// The envelope of a task is the sum of the task energy and the total amount of
// energy available before the minimum starting time of the task, i.e., the
// product of its minimum start time by the maximum capacity of the resource.
//
// An overload, meaning that there is no solution, occurs when a set of tasks
// requires more energy than what is available between its starting and ending
// times.
//
// Be aware that overload checker is not enough to ensure that the cumulative
// constraint holds. This propagator should thus always be used with a
// timetabling propagator at least.
class OverloadChecker : public PropagatorInterface {
 public:
  OverloadChecker(const std::vector<IntervalVariable>& interval_vars,
                  const std::vector<IntegerVariable>& demand_vars,
                  IntegerVariable capacity, Trail* trail,
                  IntegerTrail* integer_trail,
                  IntervalsRepository* intervals_repository);

  bool Propagate() final;

  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  struct TaskTime {
    /* const */ int task_id;
    IntegerValue time;
    TaskTime(int task_id, IntegerValue time) : task_id(task_id), time(time) {}
    bool operator<(TaskTime other) const { return time < other.time; }
  };

  // Inserts the task at leaf_id with the given energy and envelope. The change
  // is propagated to the top of the Theta-tree by recomputing the energy and
  // the envelope of all the leaf's ancestors.
  void InsertTaskInThetaTree(int leaf_id, IntegerValue energy,
                             IntegerValue envelope);

  // Remove the task at leaf_id from the Theta-tree.
  void RemoveTaskFromThetaTree(int leaf_id);

  // Resets the theta-tree such that its deepest level is the first that can
  // contain at least num_tasks leaves. All nodes are resets to energy = 0 and
  // envelope = kMinIntegerValue.
  void ResetThetaTree(int num_tasks);

  // Searches for the leaf that contains the task that has the smallest minimum
  // start time and that is involved in the value of the root node envelope.
  int LeftMostInvolvedLeaf() const;

  IntegerValue StartMin(int task_id) const {
    return integer_trail_->LowerBound(start_vars_[task_id]);
  }

  IntegerValue EndMax(int task_id) const {
    return integer_trail_->UpperBound(end_vars_[task_id]);
  }

  IntegerValue DemandMin(int task_id) const {
    return integer_trail_->LowerBound(demand_vars_[task_id]);
  }

  IntegerValue DurationMin(int task_id) const {
    return intervals_repository_->MinSize(interval_vars_[task_id]);
  }

  // An optional task can be present, absent or its status still unknown. Normal
  // tasks are always present.
  bool IsPresent(int task_id) const;
  bool IsAbsent(int task_id) const;
  void AddPresenceReasonIfNeeded(int task_id);

  // Number of tasks.
  const int num_tasks_;

  // IntervalVariable and IntegerVariable of each tasks that must be considered
  // by this propagator.
  std::vector<IntervalVariable> interval_vars_;
  std::vector<IntegerVariable> start_vars_;
  std::vector<IntegerVariable> end_vars_;
  std::vector<IntegerVariable> demand_vars_;
  std::vector<IntegerVariable> duration_vars_;

  // Capacity of the resource.
  const IntegerVariable capacity_var_;

  // Reason vector.
  std::vector<Literal> literal_reason_;
  std::vector<IntegerLiteral> integer_reason_;

  Trail* trail_;
  IntegerTrail* integer_trail_;
  IntervalsRepository* intervals_repository_;

  std::vector<TaskTime> by_start_min_;
  std::vector<TaskTime> by_end_max_;
  std::vector<int> task_to_index_in_start_min_;

  // The Theta-tree is a complete binary tree that stores the tasks from left to
  // right in the leaves of its deepest level. We implement the Theta-tree in a
  // vector such that the root node is at position 1. The left and right
  // children of a node at position p are respectively stored at positions 2*p
  // and 2*p + 1.

  // Position of the first leaf.
  int first_leaf_;
  // Energy of each node in the Theta-tree.
  std::vector<IntegerValue> node_energies_;
  // Envelope of each node in the Theta-tree.
  std::vector<IntegerValue> node_envelopes_;

  DISALLOW_COPY_AND_ASSIGN(OverloadChecker);
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_OVERLOAD_CHECKER_H_
