// Copyright 2010-2022 Google LLC
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
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/strong_integers.h"

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
  TimeTableEdgeFinding(AffineExpression capacity,
                       SchedulingConstraintHelper* helper,
                       SchedulingDemandHelper* demands, Model* model);

  bool Propagate() final;

  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  // Build the timetable and fills the mandatory_energy_before_start_min_ and
  // mandatory_energy_before_end_max_.
  //
  // TODO(user): Share the profile building code with TimeTablingPerTask ! we do
  // not really need the mandatory_energy_before_* vectors and can recompute the
  // profile integral in a window efficiently during TimeTableEdgeFindingPass().
  void BuildTimeTable();

  // Performs a single pass of the Timetable Edge Finding filtering rule to
  // updates the start time of the tasks. This same function can be used to
  // update the end times by calling the SwitchToMirrorProblem method first.
  bool TimeTableEdgeFindingPass();

  // Fills the reason for the energy in [window_min, window_max].
  // We exclude the given task_index mandatory energy and uses
  // tasks_contributing_to_free_energy_.
  void FillEnergyInWindowReason(IntegerValue window_min,
                                IntegerValue window_max, int task_index);

  IntegerValue CapacityMax() const {
    return integer_trail_->UpperBound(capacity_);
  }

  const int num_tasks_;
  const AffineExpression capacity_;
  SchedulingConstraintHelper* helper_;
  SchedulingDemandHelper* demands_;
  IntegerTrail* integer_trail_;

  // Start (resp. end) of the compulsory parts used to build the profile.
  std::vector<TaskTime> scp_;
  std::vector<TaskTime> ecp_;

  // Sizes and energy of the free parts.
  std::vector<IntegerValue> size_free_;
  std::vector<IntegerValue> energy_free_;

  // Energy contained in the time table before the start min (resp. end max)
  // of each task.
  std::vector<IntegerValue> mandatory_energy_before_start_min_;
  std::vector<IntegerValue> mandatory_energy_before_end_max_;

  // List of task that should participate in the reason.
  std::vector<int> reason_tasks_fully_included_in_window_;
  std::vector<int> reason_tasks_partially_included_in_window_;

  DISALLOW_COPY_AND_ASSIGN(TimeTableEdgeFinding);
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_TIMETABLE_EDGEFINDING_H_
