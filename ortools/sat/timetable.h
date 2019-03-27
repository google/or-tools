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

#ifndef OR_TOOLS_SAT_TIMETABLE_H_
#define OR_TOOLS_SAT_TIMETABLE_H_

#include <vector>

#include "ortools/base/macros.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/util/rev.h"

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
  // The rectangle will be ordered by start, and the end of each rectangle
  // will be equal to the start of the next one.
  struct ProfileRectangle {
    /* const */ IntegerValue start;
    /* const */ IntegerValue height;

    ProfileRectangle(IntegerValue start, IntegerValue height)
        : start(start), height(height) {}

    bool operator<(const ProfileRectangle& other) const {
      return start < other.start;
    }
  };

  // Builds the profile and increases the lower bound of the capacity
  // variable accordingly.
  bool BuildProfile();

  // Reduces the set of tasks to be considered by BuildProfile(). This is done
  // by not considering parts of mandatory part that are overlaped by no task.
  // The set of tasks to consider is restored to its previous state each time a
  // backtrack occurs. This function must be called only if the by_start_max_
  // and by_end_min_ vectors are up to date and if the profile contains no
  // profile rectangle that can exceed the capacity of the resource.
  void ReduceProfile();

  // Reverses the profile. This is needed to reuse a given profile to update
  // both the start and end times.
  void ReverseProfile();

  // Tries to increase the minimum start time of each task according to the
  // current profile. This function can be called after ReverseProfile() and
  // ReverseVariables to update the maximum end time of each task.
  bool SweepAllTasks(bool is_forward);

  // Tries to increase the minimum start time of task_id.
  bool SweepTask(int task_id);

  // Updates the starting time of task_id to right and explain it. The reason is
  // all the mandatory parts contained in [left, right).
  bool UpdateStartingTime(int task_id, IntegerValue left, IntegerValue right);

  // Increases the minimum capacity to new_min and explain it. The reason is all
  // the mandatory parts that overlap time.
  bool IncreaseCapacity(IntegerValue time, IntegerValue new_min);

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

  IntegerValue DemandMax(int task_id) const {
    return integer_trail_->UpperBound(demand_vars_[task_id]);
  }

  // Returns true if the tasks is present and has a mantatory part.
  bool IsInProfile(int t) const {
    return positions_in_profile_tasks_[t] < num_profile_tasks_;
  }

  // Number of tasks.
  const int num_tasks_;

  // The demand variables of the tasks.
  std::vector<IntegerVariable> demand_vars_;

  // Capacity of the resource.
  const IntegerVariable capacity_var_;

  IntegerTrail* integer_trail_;
  SchedulingConstraintHelper* helper_;

  RevRepository<int> rev_repository_int_;
  RevRepository<IntegerValue> rev_repository_integer_value_;

  // Vector of tasks sorted by maximum starting (resp. minimum ending) time.
  std::vector<TaskTime> by_start_max_;
  std::vector<TaskTime> by_end_min_;

  // Tasks contained in the range [left_start_, right_start_) of by_start_max_
  // must be sorted and considered when building the profile. The state of these
  // bounds is restored when a backtrack occurs.
  int left_start_;
  int right_start_;

  // Tasks contained in the range [left_end_, right_end_) of by_end_min_ must be
  // sorted and considered when building the profile. The state of these bounds
  // is restored when a backtrack occurs.
  int left_end_;
  int right_end_;

  // Optimistic profile of the resource consumption over time.
  std::vector<ProfileRectangle> profile_;
  IntegerValue profile_max_height_;

  // Reversible starting height of the reduced profile. This corresponds to the
  // height of the leftmost profile rectangle that can be used for propagation.
  IntegerValue starting_profile_height_;

  // True if the last call of the propagator has filtered the domain of a task
  // and changed the shape of the profile.
  bool profile_changed_;

  // Reversible sets of tasks to consider for the forward (resp. backward)
  // propagation. A task with a fixed start do not need to be considered for the
  // forward pass, same for task with fixed end for the backward pass. It is why
  // we use two sets.
  std::vector<int> forward_tasks_to_sweep_;
  std::vector<int> backward_tasks_to_sweep_;
  int forward_num_tasks_to_sweep_;
  int backward_num_tasks_to_sweep_;

  // Reversible set of tasks to consider for reducing the profile. The set
  // contains the [0, num_active_tasks_) prefix of active_tasks_.
  std::vector<int> active_tasks_;
  int num_active_tasks_;

  // Reversible set (with random access) of tasks to consider for building the
  // profile. The set contains the tasks in the [0, num_profile_tasks_) prefix
  // of profile_tasks_. The positions of a task in profile_tasks_ is contained
  // in positions_in_profile_tasks_.
  std::vector<int> profile_tasks_;
  std::vector<int> positions_in_profile_tasks_;
  int num_profile_tasks_;

  DISALLOW_COPY_AND_ASSIGN(TimeTablingPerTask);
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_TIMETABLE_H_
