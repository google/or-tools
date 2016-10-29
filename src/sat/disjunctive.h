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

#ifndef OR_TOOLS_SAT_DISJUNCTIVE_H_
#define OR_TOOLS_SAT_DISJUNCTIVE_H_

#include "sat/integer.h"
#include "sat/intervals.h"
#include "sat/model.h"
#include "sat/precedences.h"
#include "sat/sat_base.h"
#include "util/stats.h"

namespace operations_research {
namespace sat {

// Enforces a disjunctive (or no overlap) constraint on the given interval
// variables.
std::function<void(Model*)> Disjunctive(const std::vector<IntervalVariable>& vars);

// Same as Disjunctive() but also creates a Boolean variable for all the
// possible precedences of the form (task i is before task j).
std::function<void(Model*)> DisjunctiveWithBooleanPrecedences(
    const std::vector<IntervalVariable>& vars);

// Helper class to compute the min-end of a set of tasks given their min-start
// and min-duration. In Petr Vilim's PhD "Global Constraints in Scheduling",
// this corresponds to his Theta-tree except that we use a O(n) implementation
// for most of the function here, not a O(log(n)) one.
class TaskSet {
 public:
  TaskSet() : optimized_restart_(0) {}
  struct Entry {
    int task;
    IntegerValue min_start;
    IntegerValue min_duration;

    // Note that the tie-breaking is not important here.
    bool operator<(Entry other) const { return min_start < other.min_start; }
  };

  // Insertion and modification. These leave sorted_tasks_ sorted.
  void Clear() {
    sorted_tasks_.clear();
    optimized_restart_ = 0;
  }
  void AddEntry(const Entry& e);
  void NotifyEntryIsNowLastIfPresent(const Entry& e);
  void RemoveEntryWithIndex(int index);

  // Like AddEntry() when we already know that e.min_start will be the largest
  // one. This is checked in debug mode.
  void AddOrderedLastEntry(const Entry& e);

  // Advanced usage. Instead of calling many AddEntry(), it is more efficient to
  // call AddUnsortedEntry() instead, but then Sort() MUST be called just after
  // the insertions. Nothing is checked here, so it is up to the client to do
  // that properly.
  void AddUnsortedEntry(const Entry& e) { sorted_tasks_.push_back(e); }
  void Sort() { std::sort(sorted_tasks_.begin(), sorted_tasks_.end()); }

  // Returns the min-end for the task in the set. The time profile of the tasks
  // packed to the left will always be a set of contiguous tasks separated by
  // empty space:
  //
  //   [Bunch of tasks]   ...   [Bunch of tasks]     ...    [critical tasks].
  //
  // We call "critical tasks" the last group. These tasks will be solely
  // responsible for for the min-end of the whole set. The returned
  // critical_index will be the index of the first critical task in
  // SortedTasks().
  //
  // A reason for the min end is:
  // - The min-duration of all the critical tasks.
  // - The fact that all critical tasks have a min-start greater or equal to the
  //   first of them, that is SortedTasks()[critical_index].min_start.
  //
  // It is possible to behave like if one task was not in the set by setting
  // task_to_ignore to the id of this task. This returns 0 if the set is empty
  // in which case critical_index will be left unchanged.
  IntegerValue ComputeMinEnd(int task_to_ignore, int* critical_index) const;
  const std::vector<Entry>& SortedTasks() const { return sorted_tasks_; }

 private:
  std::vector<Entry> sorted_tasks_;
  mutable int optimized_restart_;
  DISALLOW_COPY_AND_ASSIGN(TaskSet);
};

// This class contains the propagation algorithms with explanation for a
// disjunctive constraint.
class DisjunctiveConstraint : public PropagatorInterface {
 public:
  // Creates a disjunctive constraint (or no overlap constraint) between the
  // given IntervalVariable.
  DisjunctiveConstraint(
      const std::vector<IntervalVariable>& non_overlapping_intervals, Trail* trail,
      IntegerTrail* integer_trail, IntervalsRepository* task_repository,
      PrecedencesPropagator* precedences);

  ~DisjunctiveConstraint() final {
    IF_STATS_ENABLED(LOG(INFO) << stats_.StatString());
  }

  // The algorithm is quadratic in the number of tasks.
  bool Propagate() final;

  // Registers this constraint with the GenericLiteralWatcher.
  void RegisterWith(GenericLiteralWatcher* watcher);

  void SetParameters(const SatParameters& parameters) {
    parameters_ = parameters;
  }

 private:
  // Reverses the time and all the relevant quantities. This is needed because
  // most passes will propagate different stuff in the reverse time. For
  // example, the not-last propagation will become the not-first one in the
  // reverse time direction.
  void SwitchToMirrorProblem();

  // Helpers for the current bounds on a task time window.
  //      [(min-duration)       ...      (min-duration)]
  //      ^             ^                ^             ^
  //   min-start     min-end          max-start     max-end
  //
  // Note that for tasks with variable durations, we don't necessarily have
  // min-duration between the the min-XXX and max-XXX value.
  IntegerValue MinDuration(int t) const {
    return duration_vars_[t] == kNoIntegerVariable
               ? fixed_durations_[t]
               : integer_trail_->LowerBound(duration_vars_[t]);
  }
  IntegerValue MinStart(int t) const {
    return integer_trail_->LowerBound(start_vars_[t]);
  }
  IntegerValue MaxStart(int t) const {
    return integer_trail_->UpperBound(start_vars_[t]);
  }
  IntegerValue MinEnd(int t) const {
    return integer_trail_->LowerBound(end_vars_[t]);
  }
  IntegerValue MaxEnd(int t) const {
    return integer_trail_->UpperBound(end_vars_[t]);
  }

  // Helper functions to compute the reason of a propagation.
  // Append to literal_reason_ and integer_reason_ the corresponding reason.
  void AddPresenceAndDurationReason(int t);
  void AddMinDurationReason(int t);
  void AddMinStartReason(int t, IntegerValue lower_bound);
  void AddMinEndReason(int t, IntegerValue lower_bound);
  void AddMaxEndReason(int t, IntegerValue upper_bound);
  void AddMaxStartReason(int t, IntegerValue upper_bound);

  // Enqueues new bounds of an interval. The reasons (literal_reason_ and
  // integer_reason_) must already be filled. Note that we automatically push
  // min-end and max-start accordingly, so we maintain the invariants:
  // - min-end >= min-start + min-duration
  // - max-start <= max-end + min-duration
  bool IncreaseMinStart(int t, IntegerValue new_min_start);
  bool DecreaseMaxEnd(int t, IntegerValue new_max_end);

  // All these passes use the algorithms described in Petr Vilim PhD "Global
  // Constraints in Scheduling". Except that we don't use the O(log(n)) balanced
  // tree to compute the min end of a set of task but O(n) algorithm.
  //
  // This allows to integrate right away any changes we make to the bounds and
  // to have a simpler code. Since we also need to produces the reason of each
  // propagation, it is unclear if tree-based structure will be a lot better
  // here (especially because on the kind of problem we can solve n is
  // relatively small, like 30).
  bool OverloadCheckingPass();
  bool DetectablePrecedencePass();
  bool NotLastPass();
  bool EdgeFindingPass();

  // Exploits the precedences relations of the form "this set of disjoint
  // IntervalVariables must be performed before a given IntegerVariable". The
  // relations are computed with PrecedencesPropagator::ComputePrecedences().
  bool PrecedencePass();

  // Sorts the corresponding vectors of tasks. See below.
  void UpdateTaskByIncreasingMinStart();
  void UpdateTaskByIncreasingMinEnd();
  void UpdateTaskByDecreasingMaxStart();
  void UpdateTaskByDecreasingMaxEnd();

  // The IntegerVariable of each tasks that must be considered for this
  // constraint (note that the index is not an IntevalVariable but a new one
  // local to this constraint). For optional tasks, we don't list the one that
  // are already known to be absent.
  std::vector<IntegerVariable> start_vars_;
  std::vector<IntegerVariable> end_vars_;
  std::vector<IntegerVariable> duration_vars_;
  std::vector<IntegerValue> fixed_durations_;
  std::vector<LiteralIndex> reason_for_presence_;

  // The negation of the start/end variable so that SwitchToMirrorProblem()
  // can do its job in O(1) instead of calling NegationOf() on each entry.
  std::vector<IntegerVariable> minus_start_vars_;
  std::vector<IntegerVariable> minus_end_vars_;

  // This is used by PrecedencePass().
  std::vector<LiteralIndex> reason_for_beeing_before_;
  std::vector<PrecedencesPropagator::IntegerPrecedences> before_;
  std::vector<PrecedencesPropagator::IntegerPrecedences> after_;

  // True for the optional tasks that are known to be present and false for the
  // one we don't know yet. The one that are known to be absent are not listed.
  // The index is the same as for the *_vars vector.
  std::vector<bool> task_is_currently_present_;

  // Various task orders. The indices are index in the *_vars vectors. We keep
  // them separate so that in most cases they are almost sorted and std::sort()
  // should be faster. The increasing/decreasing order is choosen so that
  // SwitchToMirrorProblem() works in O(1) and permute these vectors
  // accordingly.
  //
  // TODO(user): Using sets or maintaining these vectors sorted at all time
  // may be faster.
  std::vector<int> task_by_increasing_min_start_;
  std::vector<int> task_by_increasing_min_end_;
  std::vector<int> task_by_decreasing_max_start_;
  std::vector<int> task_by_decreasing_max_end_;

  // Reason vectors. Declared here to avoid costly initialization.
  std::vector<Literal> literal_reason_;
  std::vector<IntegerLiteral> integer_reason_;

  const std::vector<IntervalVariable> non_overlapping_intervals_;
  Trail* trail_;
  IntegerTrail* integer_trail_;
  IntervalsRepository* intervals_;
  PrecedencesPropagator* precedences_;

  TaskSet task_set_;
  std::vector<bool> is_gray_;

  SatParameters parameters_;
  mutable StatsGroup stats_;

  DISALLOW_COPY_AND_ASSIGN(DisjunctiveConstraint);
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_DISJUNCTIVE_H_
