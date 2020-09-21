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

#ifndef OR_TOOLS_SAT_SCHEDULING_CONSTRAINTS_H_
#define OR_TOOLS_SAT_SCHEDULING_CONSTRAINTS_H_

#include <vector>

#include "ortools/base/int_type.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {

class ConvexHullPropagator : public PropagatorInterface {
 public:
  explicit ConvexHullPropagator(SchedulingConstraintHelper* helper,
                                Model* model)
      : helper_(helper),
        trail_(model->GetOrCreate<Trail>()),
        integer_trail_(model->GetOrCreate<IntegerTrail>()) {}
  bool Propagate() final;
  int RegisterWith(GenericLiteralWatcher* watcher);

 private:
  SchedulingConstraintHelper* helper_;
  Trail* trail_;
  IntegerTrail* integer_trail_;

  std::vector<Literal> literal_reason_;
  std::vector<IntegerLiteral> integer_reason_;

  DISALLOW_COPY_AND_ASSIGN(ConvexHullPropagator);
};

inline std::function<void(Model*)> ConvexHullConstraint(
    IntervalVariable span, const std::vector<IntervalVariable>& intervals) {
  return [=](Model* model) {
    std::vector<IntervalVariable> copy = intervals;
    copy.push_back(span);
    SchedulingConstraintHelper* helper =
        new SchedulingConstraintHelper(copy, model);
    model->TakeOwnership(helper);

    auto* watcher = model->GetOrCreate<GenericLiteralWatcher>();
    ConvexHullPropagator* propagator = new ConvexHullPropagator(helper, model);
    propagator->RegisterWith(watcher);
    model->TakeOwnership(propagator);
  };
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SCHEDULING_CONSTRAINTS_H_
