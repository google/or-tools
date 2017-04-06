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
  TimeTablingPerTask(const std::vector<IntegerVariable>& demand_vars,
                     IntegerVariable capacity, IntegerTrail* integer_trail,
                     SchedulingConstraintHelper* helper);

  bool Propagate() final;

  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  struct ProfileRectangle {
    /* const */ IntegerValue start;
    /* const */ IntegerValue end;
    /* const */ IntegerValue height;
    ProfileRectangle(IntegerValue start, IntegerValue end, IntegerValue height)
        : start(start), end(end), height(height) {}
  };

  // Builds the profile and increases the lower bound of the capacity
  // variable accordingly.
  bool BuildProfile();

  // Reverses the profile. This is needed to reuse a given profile to update
  // both the start and end times.
  void ReverseProfile();

  // Tries to increase the minimum start time of each task according to the
  // current profile. This function can be called after ReverseProfile() and
  // ReverseVariables to update the maximum end time of each task.
  bool SweepAllTasks();

  // Tries to increase the minimum start time of task_id.
  bool SweepTask(int task_id);

  // Updates the starting time of task_id to right and explain it. The reason is
  // all the mandatory parts contained in [left, right).
  bool UpdateStartingTime(int task_id, IntegerValue left, IntegerValue right);

  // Increases the minimum capacity to new_min and explain it. The reason is all
  // the mandatory parts that overlap time.
  bool IncreaseCapacity(IntegerValue time, IntegerValue new_min);

  // Explains the resource overload at time or removes task_id if it is
  // optional.
  bool OverloadOrRemove(int task_id, IntegerValue time);

  // Explains the state of the profile in the time interval [left, right). The
  // reason is all the mandatory parts that overlap the interval. The current
  // reason is not cleared when this method is called.
  void AddProfileReason(IntegerValue left, IntegerValue right);

  IntegerValue CapacityMin() const {
    return integer_trail_->LowerBound(capacity_var_);
  }

  IntegerValue CapacityMax() const {
    return integer_trail_->UpperBound(capacity_var_);
  }

  IntegerValue DemandMin(int task_id) const {
    return integer_trail_->LowerBound(demand_vars_[task_id]);
  }

  // Number of tasks.
  const int num_tasks_;

  // The demand variables of the tasks.
  std::vector<IntegerVariable> demand_vars_;

  // Capacity of the resource.
  const IntegerVariable capacity_var_;

  IntegerTrail* integer_trail_;
  SchedulingConstraintHelper* helper_;

  // Start (resp. end) of the compulsory parts used to build the profile.
  std::vector<SchedulingConstraintHelper::TaskTime> scp_;
  std::vector<SchedulingConstraintHelper::TaskTime> ecp_;

  // Optimistic profile of the resource consumption over time.
  std::vector<ProfileRectangle> profile_;
  IntegerValue profile_max_height_;

  // True if the corresponding task is part of the profile, i.e., it has a
  // mandatory part and is not optional.
  std::vector<bool> in_profile_;

  // True if the last call of the propagator has filtered the domain of a task
  // and changed the shape of the profile.
  bool profile_changed_;

  DISALLOW_COPY_AND_ASSIGN(TimeTablingPerTask);
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_TIMETABLE_H_
