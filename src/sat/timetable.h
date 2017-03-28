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

#ifndef OR_TOOLS_SAT_TIMETABLE_H_
#define OR_TOOLS_SAT_TIMETABLE_H_

#include "sat/integer.h"
#include "sat/intervals.h"
#include "sat/model.h"
#include "sat/sat_base.h"

namespace operations_research {
namespace sat {

// A strongly quadratic version of Time Tabling filtering. This propagator
// is similar to the CumulativeTimeTable propagator of the constraint solver.
class TimeTablingPerTask : public PropagatorInterface {
 public:
  TimeTablingPerTask(const std::vector<IntervalVariable>& interval_vars,
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

  struct ProfileRectangle {
    /* const */ IntegerValue start;
    /* const */ IntegerValue end;
    /* const */ IntegerValue height;
    ProfileRectangle(IntegerValue start, IntegerValue end, IntegerValue height)
        : start(start), end(end), height(height) {}
  };

  // Builds the time table and increases the lower bound of the capacity
  // variable accordingly.
  bool BuildTimeTable();

  // Increases the start min of task task_id. This function may call
  // UpdateStartingTime().
  bool SweepTaskRight(int task_id);

  // Decreases the end max of task task_id. This function may call
  // UpdateEndingTime().
  bool SweepTaskLeft(int task_id);

  // Updates the starting time of task_id to new_start and fills the vector
  // reason_ with the corresponding reason.
  bool UpdateStartingTime(int task_id, IntegerValue new_start);

  // Updates the ending time of task_id to new_end and fills the vector
  // reason_ with the corresponding reason.
  bool UpdateEndingTime(int task_id, IntegerValue new_end);

  // Explains the resource overload at time or removes task_id if it is
  // optional.
  bool OverloadOrRemove(int task_id, IntegerValue time);

  // Fills reason_ with the reason that explains the height of the profile at
  // the given time point. Also return the height of the profile at time.
  IntegerValue ExplainProfileHeight(IntegerValue time);

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

  bool IsPresent(int task_id) const;
  bool IsAbsent(int task_id) const;
  void AddPresenceReasonIfNeeded(int task_id);

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

  // Tasks sorted by start max (resp. end min).
  std::vector<TaskTime> by_start_max_;
  std::vector<TaskTime> by_end_min_;

  // Start (resp. end) of the compulsory parts used to build the profile.
  std::vector<TaskTime> scp_;
  std::vector<TaskTime> ecp_;

  // Optimistic profile of the resource consumption over time.
  std::vector<ProfileRectangle> profile_;
  IntegerValue profile_max_height_;

  // True if the corresponding task is part of the profile.
  std::vector<bool> in_profile_;

  // True if the last call of the propagator has filtered the domain of a task
  // and changed the shape of the profile.
  bool profile_changed_;

  DISALLOW_COPY_AND_ASSIGN(TimeTablingPerTask);
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_TIMETABLE_H_
