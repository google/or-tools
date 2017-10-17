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

#ifndef OR_TOOLS_SAT_TIMETABLE_EDGEFINDING_H_
#define OR_TOOLS_SAT_TIMETABLE_EDGEFINDING_H_

#include <vector>

#include "ortools/base/macros.h"
#include "ortools/base/int_type.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {

// TimeTableEdgeFinding implements the timetable edge finding filtering rule
// presented in Vilim Petr, "Timetable edge finding filtering algorithm for
// discrete cumulative resources", CPAIOR 2011,
// http://vilim.eu/petr/cpaior2011.pdf.
//
// This propagator runs in O(n^2) where n is the number of tasks. It increases
// both the start times and decreases the ending times of the tasks.
//
// Note that this propagator does not ensure that the cumulative constraint
// holds. It should thus always be used with at least a timetable propagator.
//
// ALOGRITHM:
//
// The algorithm relies on free tasks. A free task is basically a task without
// its mandatory part. For instance:
//
//              s_min       s_max            e_min       e_max
//                v           v                v           v
//       task:    =============================
//                ^           ^                ^
//                | free part | Mandatory part |
//
// Obviously, the free part of a task that has no mandatory part is equal to the
// task itself. Also, a free part cannot have a mandatory part by definition. A
// fixed task thus have no free part.
//
// The idea of the algorithm is to use free and mandatory parts separately to
// have a better estimation of the energy contained in a task interval.
//
// If the sum of the energy of all the free parts and mandatory subparts
// contained in a task interval exceeds the amount of energy available, then the
// problem is unfeasible. A task thus cannot be scheduled at its minimum start
// time if this would cause an overload in one of the task intervals.
class TimeTableEdgeFinding : public PropagatorInterface {
 public:
  TimeTableEdgeFinding(const std::vector<IntervalVariable>& interval_vars,
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

  // Build the timetable and fills the mandatory_energy_before_start_min_ and
  // mandatory_energy_before_end_max_. This method assumes that by_start_max_
  // and by_end_min_ are sorted and up-to-date.
  void BuildTimeTable();

  // Performs a single pass of the Timetable Edge Finding filtering rule to
  // updates the start time of the tasks. This same function can be used to
  // update the end times by calling the SwitchToMirrorProblem method first.
  bool TimeTableEdgeFindingPass();

  // Increases the start min of task_id with the proper explanation.
  bool IncreaseStartMin(IntegerValue begin, IntegerValue end, int task_id,
                        IntegerValue new_start);

  // Adds the reason that explain the presence of the task to reason_.
  // This method does not reset the content of reason_.
  void AddPresenceReasonIfNeeded(int task_id);

  // Configures the propagator to update the start variables of the mirrored
  // tasks. This is needed to update the start and end times of the tasks.
  void SwitchToMirrorProblem();

  // Returns true if the task uses the resource.
  bool IsPresent(int task_id) const;

  // Returns true if the task does not use the resource.
  bool IsAbsent(int task_id) const;

  IntegerValue StartMin(int task_id) const {
    return integer_trail_->LowerBound(start_vars_[task_id]);
  }

  IntegerValue StartMax(int task_id) const {
    return integer_trail_->UpperBound(start_vars_[task_id]);
  }

  IntegerValue EndMin(int task_id) const {
    return integer_trail_->LowerBound(end_vars_[task_id]);
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

  IntegerValue CapacityMin() const {
    return integer_trail_->LowerBound(capacity_var_);
  }

  IntegerValue CapacityMax() const {
    return integer_trail_->UpperBound(capacity_var_);
  }

  // Number of tasks.
  const int num_tasks_;

  // IntervalVariable and IntegerVariable of each tasks that must be considered
  // in this constraint.
  std::vector<IntervalVariable> interval_vars_;
  std::vector<IntegerVariable> start_vars_;
  std::vector<IntegerVariable> end_vars_;
  std::vector<IntegerVariable> demand_vars_;
  std::vector<IntegerVariable> duration_vars_;

  // Mirror variables
  std::vector<IntegerVariable> mirror_start_vars_;
  std::vector<IntegerVariable> mirror_end_vars_;

  // Capacity of the resource.
  const IntegerVariable capacity_var_;

  // Reason vector.
  std::vector<Literal> literal_reason_;
  std::vector<IntegerLiteral> reason_;

  Trail* trail_;
  IntegerTrail* integer_trail_;
  IntervalsRepository* intervals_repository_;

  // Used for fast access and to maintain the actual value of end_min since
  // updating start_vars_[t] does not directly update end_vars_[t].
  std::vector<IntegerValue> start_min_;
  std::vector<IntegerValue> start_max_;
  std::vector<IntegerValue> end_min_;
  std::vector<IntegerValue> end_max_;
  std::vector<IntegerValue> duration_min_;
  std::vector<IntegerValue> demand_min_;

  // Tasks sorted by start (resp. end) min (resp. max).
  std::vector<TaskTime> by_start_min_;
  std::vector<TaskTime> by_start_max_;
  std::vector<TaskTime> by_end_min_;
  std::vector<TaskTime> by_end_max_;

  // Start (resp. end) of the compulsory parts used to build the profile.
  std::vector<TaskTime> scp_;
  std::vector<TaskTime> ecp_;

  // Energy of the free parts.
  std::vector<IntegerValue> energy_free_;

  // Energy contained in the time table before the start min (resp. end max)
  // of each task.
  std::vector<IntegerValue> mandatory_energy_before_start_min_;
  std::vector<IntegerValue> mandatory_energy_before_end_max_;

  DISALLOW_COPY_AND_ASSIGN(TimeTableEdgeFinding);
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_TIMETABLE_EDGEFINDING_H_
