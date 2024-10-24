// Copyright 2010-2024 Google LLC
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

#ifndef OR_TOOLS_SAT_TIMETABLE_HORTIZONTAL_EDGEFINDING_H_
#define OR_TOOLS_SAT_TIMETABLE_HORTIZONTAL_EDGEFINDING_H_

#include <vector>

#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

// HorizontallyElasticOverloadChecker implements the improved quadratic 
// horizonally elastic + timetable overload checker filtering rule presented 
// in Roger Kameugne et al., "Improved timetable edge finder rule for 
// cumulative constraint with profile".
class HorizontallyElasticOverloadChecker : public PropagatorInterface {
 public:
  HorizontallyElasticOverloadChecker(AffineExpression capacity,
                       SchedulingConstraintHelper* helper,
                       SchedulingDemandHelper* demands, Model* model);

  // This type is neither copyable nor movable.
  HorizontallyElasticOverloadChecker(const HorizontallyElasticOverloadChecker&) = delete;
  HorizontallyElasticOverloadChecker& operator=(const HorizontallyElasticOverloadChecker&) = delete;

  bool Propagate() final;

  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  enum class ProfileEventType {
    kStartMin, 
    kStartMax, 
    kEndMin,
    kEndMax, 
    kSentinel,
  };

  enum class TaskType {
    kFull,
    kFixedPart,
    kIgnore,
  };

  // ProfileEvent represents an event to construct the horizontal elastic 
  // profile.
  struct ProfileEvent {
    /* const */ int task_id;
    /* const */ ProfileEventType event_type;
    IntegerValue time;
    IntegerValue height;

    ProfileEvent(int task_id, ProfileEventType event_type, IntegerValue time, IntegerValue height)
        : task_id(task_id), event_type(event_type), time(time), height(height) {}

    bool operator<(const ProfileEvent& other) const {
      return time < other.time;
    }
  };

  // Performs a single pass of the Horizontal Elastic Overload Checker rule
  // to detect potential conflicts. This same function can be used forward
  // and backward by calling the SynchronizeAndSetTimeDirection method first.
  bool OverloadCheckerPass();

  // Returns the end min for the task group in the given window accordingly 
  // to the horizontally elastic rule + timetable. 
  IntegerValue ScheduleTasks(IntegerValue window_end, IntegerValue capacity);

  void AddScheduleTaskReason(IntegerValue window_end); 

  const int num_tasks_;
  const AffineExpression capacity_;
  SchedulingConstraintHelper* helper_;
  SchedulingDemandHelper* demands_;
  IntegerTrail* integer_trail_;

  // Pre-allocated vector indicating how tasks should be processed by 
  // ScheduleTasks and AddScheduleTaskReason.
  std::vector<TaskType> task_types_;

  // Pre-allocated vector to contain the profile. The profile cannot 
  // contain more than 4*n + 1 events: one for each start/end min/max
  // event + one sentinel.
  std::vector<ProfileEvent> profile_events_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_TIMETABLE_HORTIZONTAL_EDGEFINDING_H_
