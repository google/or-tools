// Copyright 2010-2025 Google LLC
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

#ifndef ORTOOLS_SAT_CUMULATIVE_ENERGY_H_
#define ORTOOLS_SAT_CUMULATIVE_ENERGY_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "ortools/sat/2d_orthogonal_packing.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/scheduling_helpers.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/scheduling.h"

namespace operations_research {
namespace sat {

// Enforces the existence of a preemptive schedule where every task is executed
// inside its interval, using energy units of the resource during execution.
//
// Important: This only uses the energies min/max and not the actual demand
// of a task. It can thus be used in some non-conventional situation.
//
// All energy expression are assumed to take a non-negative value;
// if the energy of a task is 0, the task can run anywhere.
// The schedule never uses more than capacity units of energy at a given time.
//
// This is mathematically equivalent to making a model with energy(task)
// different tasks with demand and size 1, but is much more efficient,
// since it uses O(|tasks|) variables instead of O(sum_{task} |energy(task)|).
void AddCumulativeOverloadChecker(AffineExpression capacity,
                                  SchedulingConstraintHelper* helper,
                                  SchedulingDemandHelper* demands,
                                  Model* model);

// Same as above, but applying a Dual Feasible Function (also known as a
// conservative scale) before looking for overload.
void AddCumulativeOverloadCheckerDff(AffineExpression capacity,
                                     SchedulingConstraintHelper* helper,
                                     SchedulingDemandHelper* demands,
                                     Model* model);

// Implementation of AddCumulativeOverloadChecker().
class CumulativeEnergyConstraint : public PropagatorInterface {
 public:
  CumulativeEnergyConstraint(AffineExpression capacity,
                             SchedulingConstraintHelper* helper,
                             SchedulingDemandHelper* demands, Model* model);

  bool Propagate() final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  const AffineExpression capacity_;
  IntegerTrail* integer_trail_;
  SchedulingConstraintHelper* helper_;
  SchedulingDemandHelper* demands_;

  ThetaLambdaTree<IntegerValue> theta_tree_;

  // Task characteristics.
  std::vector<int> task_to_start_event_;

  // Start event characteristics, by nondecreasing start time.
  std::vector<TaskTime> start_event_task_time_;
  std::vector<bool> start_event_is_present_;
};

// Given that the "tasks" are part of a cumulative constraint, this adds a
// constraint that propagate the fact that: var >= max(end of subtasks) +
// offset.
//
// TODO(user): I am not sure this is the best way, but it does at least push
// the level zero bound on the large cumulative instances.
class CumulativeIsAfterSubsetConstraint : public PropagatorInterface {
 public:
  CumulativeIsAfterSubsetConstraint(IntegerVariable var,
                                    AffineExpression capacity,
                                    const std::vector<int>& subtasks,
                                    absl::Span<const IntegerValue> offsets,
                                    SchedulingConstraintHelper* helper,
                                    SchedulingDemandHelper* demands,
                                    Model* model);

  bool Propagate() final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  const IntegerVariable var_to_push_;
  const AffineExpression capacity_;
  const std::vector<int> subtasks_;

  // Computed at construction time, this is const.
  std::vector<bool> is_in_subtasks_;
  std::vector<IntegerValue> task_offsets_;

  // Temporary data used by the algorithm.
  MaxBoundedSubsetSum dp_;
  std::vector<std::pair<IntegerValue, IntegerValue>> energy_changes_;

  IntegerTrail* integer_trail_;
  SchedulingConstraintHelper* helper_;
  SchedulingDemandHelper* demands_;
};

// Implementation of AddCumulativeOverloadCheckerDff().
class CumulativeDualFeasibleEnergyConstraint : public PropagatorInterface {
 public:
  CumulativeDualFeasibleEnergyConstraint(AffineExpression capacity,
                                         SchedulingConstraintHelper* helper,
                                         SchedulingDemandHelper* demands,
                                         Model* model);

  ~CumulativeDualFeasibleEnergyConstraint() override;

  bool Propagate() final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  bool FindAndPropagateConflict(IntegerValue window_start,
                                IntegerValue window_end);

  ModelRandomGenerator* random_;
  SharedStatistics* shared_stats_;
  OrthogonalPackingInfeasibilityDetector opp_infeasibility_detector_;
  const AffineExpression capacity_;
  IntegerTrail* integer_trail_;
  SchedulingConstraintHelper* helper_;
  SchedulingDemandHelper* demands_;

  ThetaLambdaTree<IntegerValue> theta_tree_;

  // Task characteristics.
  std::vector<int> task_to_start_event_;

  // Start event characteristics, by nondecreasing start time.
  std::vector<TaskTime> start_event_task_time_;

  int64_t num_calls_ = 0;
  int64_t num_conflicts_ = 0;
  int64_t num_no_potential_window_ = 0;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_CUMULATIVE_ENERGY_H_
