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

#ifndef OR_TOOLS_SAT_DISJUNCTIVE_H_
#define OR_TOOLS_SAT_DISJUNCTIVE_H_

#include <algorithm>
#include <functional>
#include <vector>

#include "ortools/base/int_type.h"
#include "ortools/base/macros.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/theta_tree.h"

namespace operations_research {
namespace sat {

// Enforces a disjunctive (or no overlap) constraint on the given interval
// variables. The intervals are interpreted as [start, end) and the constraint
// enforces that no time point belongs to two intervals.
//
// TODO(user): This is not completely true for empty intervals (start == end).
// Make sure such intervals are ignored by the constraint.
std::function<void(Model*)> Disjunctive(
    const std::vector<IntervalVariable>& vars);

// Creates Boolean variables for all the possible precedences of the form (task
// i is before task j) and forces that, for each couple of task (i,j), either i
// is before j or j is before i. Do not create any other propagators.
std::function<void(Model*)> DisjunctiveWithBooleanPrecedencesOnly(
    const std::vector<IntervalVariable>& vars);

// Same as Disjunctive() + DisjunctiveWithBooleanPrecedencesOnly().
std::function<void(Model*)> DisjunctiveWithBooleanPrecedences(
    const std::vector<IntervalVariable>& vars);

// Helper class to compute the end-min of a set of tasks given their start-min
// and size-min. In Petr Vilim's PhD "Global Constraints in Scheduling",
// this corresponds to his Theta-tree except that we use a O(n) implementation
// for most of the function here, not a O(log(n)) one.
class TaskSet {
 public:
  explicit TaskSet(int num_tasks) { sorted_tasks_.reserve(num_tasks); }

  struct Entry {
    int task;
    IntegerValue start_min;
    IntegerValue size_min;

    // Note that the tie-breaking is not important here.
    bool operator<(Entry other) const { return start_min < other.start_min; }
  };

  // Insertion and modification. These leave sorted_tasks_ sorted.
  void Clear() {
    sorted_tasks_.clear();
    optimized_restart_ = 0;
  }
  void AddEntry(const Entry& e);
  void RemoveEntryWithIndex(int index);

  // Same as AddEntry({t, helper->ShiftedStartMin(t), helper->SizeMin(t)}).
  // This is a minor optimization to not call SizeMin(t) twice.
  void AddShiftedStartMinEntry(const SchedulingConstraintHelper& helper, int t);

  // Advanced usage, if the entry is present, this assumes that its start_min is
  // >= the end min without it, and update the datastructure accordingly.
  void NotifyEntryIsNowLastIfPresent(const Entry& e);

  // Advanced usage. Instead of calling many AddEntry(), it is more efficient to
  // call AddUnsortedEntry() instead, but then Sort() MUST be called just after
  // the insertions. Nothing is checked here, so it is up to the client to do
  // that properly.
  void AddUnsortedEntry(const Entry& e) { sorted_tasks_.push_back(e); }
  void Sort() { std::sort(sorted_tasks_.begin(), sorted_tasks_.end()); }

  // Returns the end-min for the task in the set. The time profile of the tasks
  // packed to the left will always be a set of contiguous tasks separated by
  // empty space:
  //
  //   [Bunch of tasks]   ...   [Bunch of tasks]     ...    [critical tasks].
  //
  // We call "critical tasks" the last group. These tasks will be solely
  // responsible for for the end-min of the whole set. The returned
  // critical_index will be the index of the first critical task in
  // SortedTasks().
  //
  // A reason for the min end is:
  // - The size-min of all the critical tasks.
  // - The fact that all critical tasks have a start-min greater or equal to the
  //   first of them, that is SortedTasks()[critical_index].start_min.
  //
  // It is possible to behave like if one task was not in the set by setting
  // task_to_ignore to the id of this task. This returns 0 if the set is empty
  // in which case critical_index will be left unchanged.
  IntegerValue ComputeEndMin(int task_to_ignore, int* critical_index) const;
  IntegerValue ComputeEndMin() const;

  // Warning, this is only valid if ComputeEndMin() was just called. It is the
  // same index as if one called ComputeEndMin(-1, &critical_index), but saves
  // another unneeded loop.
  int GetCriticalIndex() const { return optimized_restart_; }

  const std::vector<Entry>& SortedTasks() const { return sorted_tasks_; }

 private:
  std::vector<Entry> sorted_tasks_;
  mutable int optimized_restart_ = 0;
};

// ============================================================================
// Below are many of the known propagation techniques for the disjunctive, each
// implemented in only one time direction and in its own propagator class. The
// Disjunctive() model function above will instantiate the used ones (according
// to the solver parameters) in both time directions.
//
// See Petr Vilim PhD "Global Constraints in Scheduling" for a description of
// some of the algorithm.
// ============================================================================

class DisjunctiveOverloadChecker : public PropagatorInterface {
 public:
  explicit DisjunctiveOverloadChecker(SchedulingConstraintHelper* helper)
      : helper_(helper) {
    // Resize this once and for all.
    task_to_event_.resize(helper_->NumTasks());
  }
  bool Propagate() final;
  int RegisterWith(GenericLiteralWatcher* watcher);

 private:
  bool PropagateSubwindow(IntegerValue global_window_end);

  SchedulingConstraintHelper* helper_;

  std::vector<TaskTime> window_;
  std::vector<TaskTime> task_by_increasing_end_max_;

  ThetaLambdaTree<IntegerValue> theta_tree_;
  std::vector<int> task_to_event_;
};

class DisjunctiveDetectablePrecedences : public PropagatorInterface {
 public:
  DisjunctiveDetectablePrecedences(bool time_direction,
                                   SchedulingConstraintHelper* helper)
      : time_direction_(time_direction),
        helper_(helper),
        task_set_(helper->NumTasks()) {}
  bool Propagate() final;
  int RegisterWith(GenericLiteralWatcher* watcher);

 private:
  bool PropagateSubwindow();

  std::vector<TaskTime> task_by_increasing_end_min_;
  std::vector<TaskTime> task_by_increasing_start_max_;

  std::vector<bool> processed_;
  std::vector<int> to_propagate_;

  const bool time_direction_;
  SchedulingConstraintHelper* helper_;
  TaskSet task_set_;
};

// Singleton model class which is just a SchedulingConstraintHelper will all
// the intervals.
class AllIntervalsHelper : public SchedulingConstraintHelper {
 public:
  explicit AllIntervalsHelper(Model* model)
      : SchedulingConstraintHelper(
            model->GetOrCreate<IntervalsRepository>()->AllIntervals(), model) {}
};

// This propagates the same things as DisjunctiveDetectablePrecedences, except
// that it only sort the full set of intervals once and then work on a combined
// set of disjunctives.
template <bool time_direction>
class CombinedDisjunctive : public PropagatorInterface {
 public:
  explicit CombinedDisjunctive(Model* model);

  // After creation, this must be called for all the disjunctive constraints
  // in the model.
  void AddNoOverlap(const std::vector<IntervalVariable>& var);

  bool Propagate() final;

 private:
  AllIntervalsHelper* helper_;
  std::vector<std::vector<int>> task_to_disjunctives_;
  std::vector<bool> task_is_added_;
  std::vector<TaskSet> task_sets_;
  std::vector<IntegerValue> end_mins_;
};

class DisjunctiveNotLast : public PropagatorInterface {
 public:
  DisjunctiveNotLast(bool time_direction, SchedulingConstraintHelper* helper)
      : time_direction_(time_direction),
        helper_(helper),
        task_set_(helper->NumTasks()) {}
  bool Propagate() final;
  int RegisterWith(GenericLiteralWatcher* watcher);

 private:
  bool PropagateSubwindow();

  std::vector<TaskTime> start_min_window_;
  std::vector<TaskTime> start_max_window_;

  const bool time_direction_;
  SchedulingConstraintHelper* helper_;
  TaskSet task_set_;
};

class DisjunctiveEdgeFinding : public PropagatorInterface {
 public:
  DisjunctiveEdgeFinding(bool time_direction,
                         SchedulingConstraintHelper* helper)
      : time_direction_(time_direction), helper_(helper) {}
  bool Propagate() final;
  int RegisterWith(GenericLiteralWatcher* watcher);

 private:
  bool PropagateSubwindow(IntegerValue window_end_min);

  const bool time_direction_;
  SchedulingConstraintHelper* helper_;

  // This only contains non-gray tasks.
  std::vector<TaskTime> task_by_increasing_end_max_;

  // All these member are indexed in the same way.
  std::vector<TaskTime> window_;
  ThetaLambdaTree<IntegerValue> theta_tree_;
  std::vector<IntegerValue> event_size_;

  // Task indexed.
  std::vector<int> non_gray_task_to_event_;
  std::vector<bool> is_gray_;
};

// Exploits the precedences relations of the form "this set of disjoint
// IntervalVariables must be performed before a given IntegerVariable". The
// relations are computed with PrecedencesPropagator::ComputePrecedences().
class DisjunctivePrecedences : public PropagatorInterface {
 public:
  DisjunctivePrecedences(bool time_direction,
                         SchedulingConstraintHelper* helper,
                         IntegerTrail* integer_trail,
                         PrecedencesPropagator* precedences)
      : time_direction_(time_direction),
        helper_(helper),
        integer_trail_(integer_trail),
        precedences_(precedences),
        task_set_(helper->NumTasks()),
        task_to_arc_index_(helper->NumTasks()) {}
  bool Propagate() final;
  int RegisterWith(GenericLiteralWatcher* watcher);

 private:
  bool PropagateSubwindow();

  const bool time_direction_;
  SchedulingConstraintHelper* helper_;
  IntegerTrail* integer_trail_;
  PrecedencesPropagator* precedences_;

  std::vector<TaskTime> window_;
  std::vector<IntegerVariable> index_to_end_vars_;

  TaskSet task_set_;
  std::vector<int> task_to_arc_index_;
  std::vector<PrecedencesPropagator::IntegerPrecedences> before_;
};

// This is an optimization for the case when we have a big number of such
// pairwise constraints. This should be roughtly equivalent to what the general
// disjunctive case is doing, but it dealt with variable size better and has a
// lot less overhead.
class DisjunctiveWithTwoItems : public PropagatorInterface {
 public:
  explicit DisjunctiveWithTwoItems(SchedulingConstraintHelper* helper)
      : helper_(helper) {}
  bool Propagate() final;
  int RegisterWith(GenericLiteralWatcher* watcher);

 private:
  SchedulingConstraintHelper* helper_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_DISJUNCTIVE_H_
