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

#ifndef OR_TOOLS_SAT_CUMULATIVE_H_
#define OR_TOOLS_SAT_CUMULATIVE_H_

#include <functional>
#include <vector>

#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

// Adds a cumulative constraint on the given intervals, the associated demands
// and the capacity expressions.
//
// Each interval represents a task to be scheduled in time such that the task
// consumes the resource during the time range [lb, ub) where lb and ub
// respectively represent the lower and upper bounds of the corresponding
// interval variable. The amount of resource consumed by the task is the value
// of its associated demand variable.
//
// The cumulative constraint forces the set of task to be scheduled such that
// the sum of the demands of all the tasks that overlap any time point cannot
// exceed the capacity of the resource.
//
// This constraint assumes that an interval can be optional or have a size
// of zero. The demands and the capacity can be any non-negative number.
//
// Optimization: If one already have an helper constructed from the interval
// variable, it can be passed as last argument.
std::function<void(Model*)> Cumulative(
    const std::vector<IntervalVariable>& vars,
    const std::vector<AffineExpression>& demands, AffineExpression capacity,
    SchedulingConstraintHelper* helper = nullptr);

// Adds a simple cumulative constraint. See the comment of Cumulative() above
// for a definition of the constraint. This is only used for testing.
//
// This constraint assumes that task demands and the resource capacity are fixed
// to non-negative number.
std::function<void(Model*)> CumulativeTimeDecomposition(
    const std::vector<IntervalVariable>& vars,
    const std::vector<AffineExpression>& demands, AffineExpression capacity,
    SchedulingConstraintHelper* helper = nullptr);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CUMULATIVE_H_
