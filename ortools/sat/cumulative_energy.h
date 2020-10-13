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

#ifndef OR_TOOLS_SAT_CUMULATIVE_ENERGY_H_
#define OR_TOOLS_SAT_CUMULATIVE_ENERGY_H_

#include <functional>
#include <vector>

#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/theta_tree.h"

namespace operations_research {
namespace sat {

// Enforces the existence of a preemptive schedule where every task is executed
// inside its interval, using energy units of the resource during execution.
//
// All energy expression are assumed to take a non-negative value;
// if the energy of a task is 0, the task can run anywhere.
// The schedule never uses more than capacity units of energy at a given time.
//
// This is mathematically equivalent to making a model with energy(task)
// different tasks with demand and size 1, but is much more efficient,
// since it uses O(|tasks|) variables instead of O(sum_{task} |energy(task)|).
void AddCumulativeEnergyConstraint(std::vector<AffineExpression> energies,
                                   AffineExpression capacity,
                                   SchedulingConstraintHelper* helper,
                                   Model* model);

// Creates a CumulativeEnergyConstraint where the energy of each interval is
// the product of the demands times its size.
void AddCumulativeOverloadChecker(const std::vector<AffineExpression>& demands,
                                  AffineExpression capacity,
                                  SchedulingConstraintHelper* helper,
                                  Model* model);

class CumulativeEnergyConstraint : public PropagatorInterface {
 public:
  CumulativeEnergyConstraint(std::vector<AffineExpression> energies,
                             AffineExpression capacity,
                             IntegerTrail* integer_trail,
                             SchedulingConstraintHelper* helper);
  bool Propagate() final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  const std::vector<AffineExpression> energies_;
  const AffineExpression capacity_;
  IntegerTrail* integer_trail_;
  SchedulingConstraintHelper* helper_;
  ThetaLambdaTree<IntegerValue> theta_tree_;

  // Task characteristics.
  std::vector<int> task_to_start_event_;

  // Start event characteristics, by nondecreasing start time.
  std::vector<TaskTime> start_event_task_time_;
  std::vector<bool> start_event_is_present_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CUMULATIVE_ENERGY_H_
