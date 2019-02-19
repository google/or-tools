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

#ifndef OR_TOOLS_SAT_DIFFN_H_
#define OR_TOOLS_SAT_DIFFN_H_

#include <vector>

#include "ortools/base/int_type.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {

// Non overlapping rectangles.
class NonOverlappingRectanglesPropagator : public PropagatorInterface {
 public:
  // The strict parameters indicates how to place zero width or zero height
  // boxes. If strict is true, these boxes must not 'cross' another box, and are
  // pushed by the other boxes.
  NonOverlappingRectanglesPropagator(const std::vector<IntervalVariable>& x,
                                     const std::vector<IntervalVariable>& y,
                                     bool strict, Model* model,
                                     IntegerTrail* integer_trail);
  ~NonOverlappingRectanglesPropagator() override;

  // TODO(user): Look at intervals to to store x and dx, y and dy.
  bool Propagate() final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  IntegerValue Min(IntegerVariable v) const;
  IntegerValue Max(IntegerVariable v) const;
  bool SetMin(IntegerVariable v, IntegerValue value,
              const std::vector<IntegerLiteral>& reason);
  bool SetMax(IntegerVariable v, IntegerValue value,
              const std::vector<IntegerLiteral>& reason);

  void UpdateNeighbors(int box);
  bool FailWhenEnergyIsTooLarge(int box);
  bool PushOneBox(int box, int other);

  // Updates the boxes positions and size when the given box is before other in
  // the passed direction. This will fill integer_reason_ if it is empty,
  // otherwise, it will reuse its current value.
  bool FirstBoxIsBeforeSecondBox(const std::vector<IntegerVariable>& starts,
                                 const std::vector<IntegerVariable>& sizes,
                                 const std::vector<IntegerVariable>& ends,
                                 const std::vector<IntegerValue>& fixed_sizes,
                                 int box, int other);

  const int num_boxes_;
  const std::vector<IntervalVariable> x_;
  const std::vector<IntervalVariable> y_;
  const bool strict_;
  IntegerTrail* integer_trail_;

  // The neighbors_ of a box will be in
  // [neighbors_[begin[box]], neighbors_[end[box]])
  std::vector<int> neighbors_;
  std::vector<int> neighbors_begins_;
  std::vector<int> neighbors_ends_;
  std::vector<int> tmp_removed_;

  std::vector<IntegerValue> cached_areas_;
  std::vector<IntegerValue> cached_distance_to_bounding_box_;
  std::vector<IntegerLiteral> integer_reason_;

  std::vector<IntegerVariable> start_x_vars_;
  std::vector<IntegerVariable> end_x_vars_;
  std::vector<IntegerVariable> duration_x_vars_;
  std::vector<IntegerValue> fixed_duration_x_;
  std::vector<IntegerVariable> start_y_vars_;
  std::vector<IntegerVariable> end_y_vars_;
  std::vector<IntegerVariable> duration_y_vars_;
  std::vector<IntegerValue> fixed_duration_y_;

  DISALLOW_COPY_AND_ASSIGN(NonOverlappingRectanglesPropagator);
};

// Enforces that the boxes with corners in (x, y), (x + dx, y), (x, y + dy)
// and (x + dx, y + dy) do not overlap.
// If one box has a zero dimension, then it can be placed anywhere.
inline std::function<void(Model*)> NonOverlappingRectangles(
    const std::vector<IntervalVariable>& x,
    const std::vector<IntervalVariable>& y) {
  return [=](Model* model) {
    NonOverlappingRectanglesPropagator* constraint =
        new NonOverlappingRectanglesPropagator(
            x, y, false, model, model->GetOrCreate<IntegerTrail>());
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

// Enforces that the boxes with corners in (x, y), (x + dx, y), (x, y + dy)
// and (x + dx, y + dy) do not overlap.
// If one box has a zero dimension, it still cannot intersect another box.
inline std::function<void(Model*)> StrictNonOverlappingRectangles(
    const std::vector<IntervalVariable>& x,
    const std::vector<IntervalVariable>& y) {
  return [=](Model* model) {
    NonOverlappingRectanglesPropagator* constraint =
        new NonOverlappingRectanglesPropagator(
            x, y, true, model, model->GetOrCreate<IntegerTrail>());
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_DIFFN_H_
