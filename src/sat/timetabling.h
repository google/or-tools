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

#ifndef OR_TOOLS_SAT_TIMETABLING_H_
#define OR_TOOLS_SAT_TIMETABLING_H_

#include "sat/integer.h"
#include "sat/intervals.h"
#include "sat/model.h"
#include "sat/sat_base.h"

namespace operations_research {
namespace sat {

// Enforces a cumulative constraint on the given interval variables.
std::function<void(Model*)> Cumulative(
    const std::vector<IntervalVariable>& vars,
    const std::vector<IntegerVariable>& demands,
    const IntegerVariable& capacity);

// A strongly quadratic version of Time Tabling filtering. This propagator
// is similar to the CumulativeTimeTable propagator of the constraint solver.
class TimeTablingPerTask : public PropagatorInterface {
 public:
  TimeTablingPerTask(const std::vector<IntervalVariable>& interval_vars,
                     const std::vector<IntegerVariable>& demand_vars,
                     IntegerVariable capacity, IntegerTrail* integer_trail,
                     IntervalsRepository* intervals_repository);

  bool Propagate() final;

  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  // Increase the start min of task task_id. This function may call
  // UpdateStartingTime().
  bool SweepTaskRight(int task_id);

  // Decrease the end max of task task_id. This function may call
  // UpdateEndingTime().
  bool SweepTaskLeft(int task_id);

  // Updates the starting time of task_id to new_start and fills the vector
  // reason_ with the corresponding reason.
  bool UpdateStartingTime(int task_id, IntegerValue new_start);

  // Updates the ending time of task_id to new_end and fills the vector
  // reason_ with the corresponding reason.
  bool UpdateEndingTime(int task_id, IntegerValue new_end);

  // Fills reason_ with the reason why the given task cannot overlap the given
  // time point. It is because there is not enough capacity to schedule the task
  // due to the mandatory parts of other tasks that already overlap this time
  // point. Note that reason_ is not cleared by this function.
  void ExplainWhyTaskCannotOverlapTimePoint(IntegerValue time, int task_id);

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
    if (duration_vars_[task_id] != kNoIntegerVariable)
      return integer_trail_->LowerBound(duration_vars_[task_id]);
    return intervals_repository_->FixedSize(interval_vars_[task_id]);
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

  // Capacity of the resource.
  const IntegerVariable capacity_var_;

  // Reason vector.
  std::vector<IntegerLiteral> reason_;

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

  struct Event {
    /* const */ IntegerValue time;
    /* const */ int task_id;
    Event(IntegerValue time, int task_id) : time(time), task_id(task_id) {}
    bool operator<(Event other) const { return time < other.time; }
    bool operator>(Event other) const { return time > other.time; }
  };

  // Events that represent the start of a compulsory part.
  std::vector<Event> scp_;
  // Events that represent the end of a compulsory part.
  std::vector<Event> ecp_;

  struct ProfileRectangle {
    /* const */ IntegerValue start;
    /* const */ IntegerValue end;
    /* const */ IntegerValue height;
    ProfileRectangle(IntegerValue start, IntegerValue end, IntegerValue height)
        : start(start), end(end), height(height) {}
  };

  // Optimistic profile of the resource consumption over time.
  std::vector<ProfileRectangle> profile_;

  // True if the last call of the propagator has filtered the domain of a task
  // and changed the shape of the profile.
  bool profile_changed_;

  DISALLOW_COPY_AND_ASSIGN(TimeTablingPerTask);
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_TIMETABLING_H_
