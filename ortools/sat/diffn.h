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

#include <functional>
#include <memory>
#include <vector>

#include "ortools/base/int_type.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/sat/cp_constraints.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/rev.h"

namespace operations_research {
namespace sat {

// Non overlapping rectangles.
class NonOverlappingRectanglesPropagator : public CpPropagator {
 public:
  // The strict parameters indicates how to place zero width or zero height
  // boxes. If strict is true, these boxes must not 'cross' another box, and are
  // pushed by the other boxes.
  NonOverlappingRectanglesPropagator(
      const std::vector<IntegerVariable>& start_x,
      const std::vector<IntegerVariable>& size_x,
      const std::vector<IntegerVariable>& end_x,
      const std::vector<IntegerVariable>& start_y,
      const std::vector<IntegerVariable>& size_y,
      const std::vector<IntegerVariable>& end_y, bool strict,
      IntegerTrail* integer_trail);
  ~NonOverlappingRectanglesPropagator() override;

  // TODO(user): Look at intervals to to store x and dx, y and dy.
  bool Propagate() final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  void UpdateNeighbors(int box);
  bool FailWhenEnergyIsTooLarge(int box);
  bool CheckEnergyOnProjections();
  bool CheckEnergyOnOneDimension(
      const std::vector<IntegerVariable>& starts,
      const std::vector<IntegerVariable>& sizes,
      const std::vector<IntegerVariable>& ends,
      const std::vector<IntegerVariable>& other_starts,
      const std::vector<IntegerVariable>& other_sizes,
      const std::vector<IntegerVariable>& other_ends);
  bool PushOneBox(int box, int other);
  void AddBoxReason(int box);
  void AddBoxInRectangleReason(int box, IntegerValue xmin, IntegerValue xmax,
                               IntegerValue ymin, IntegerValue ymax);

  // Updates the boxes positions and size when the given box is before other in
  // the passed direction. This will fill integer_reason_ if it is empty,
  // otherwise, it will reuse its current value.
  bool FirstBoxIsBeforeSecondBox(const std::vector<IntegerVariable>& starts,
                                 const std::vector<IntegerVariable>& sizes,
                                 const std::vector<IntegerVariable>& ends,
                                 const std::vector<bool>& fixed, int box,
                                 int other);

  const std::vector<IntegerVariable> start_x_;
  const std::vector<IntegerVariable> size_x_;
  const std::vector<IntegerVariable> end_x_;
  const std::vector<IntegerVariable> start_y_;
  const std::vector<IntegerVariable> size_y_;
  const std::vector<IntegerVariable> end_y_;
  std::vector<bool> fixed_x_;
  std::vector<bool> fixed_y_;
  const bool strict_;

  // The neighbors_ of a box will be in
  // [neighbors_[begin[box]], neighbors_[end[box]])
  std::vector<int> neighbors_;
  std::vector<int> neighbors_begins_;
  std::vector<int> neighbors_ends_;
  std::vector<int> tmp_removed_;

  std::vector<IntegerValue> cached_areas_;
  std::vector<IntegerValue> cached_distance_to_bounding_box_;
  std::vector<IntegerLiteral> integer_reason_;

  DISALLOW_COPY_AND_ASSIGN(NonOverlappingRectanglesPropagator);
};


// Non overlapping rectangles.
class FixedDiff2 : public CpPropagator {
 public:
  // The strict parameters indicates how to place zero width or zero height
  // boxes. If strict is true, these boxes must not 'cross' another box, and are
  // pushed by the other boxes.
  FixedDiff2(
      const std::vector<IntegerVariable>& start_x,
      const std::vector<IntegerValue>& size_x,
      const std::vector<IntegerVariable>& start_y,
      const std::vector<IntegerValue>& size_y,
      IntegerTrail* integer_trail);
  ~FixedDiff2() override;

  // TODO(user): Look at intervals to to store x and dx, y and dy.
  bool Propagate() final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  IntegerValue MinEndX(int box);
  IntegerValue MaxEndX(int box);
  void UpdateNeighbors(int box);
  bool FailWhenEnergyIsTooLarge(int box);
  bool CheckEnergyOnProjections();
  bool CheckEnergyOnOneDimension(
      const std::vector<IntegerVariable>& starts,
      const std::vector<IntegerValue>& sizes,
      const std::vector<IntegerVariable>& other_starts,
      const std::vector<IntegerValue>& other_sizes);
  bool PushOneBox(int box, int other);
  void AddBoxReason(int box);
  void AddBoxInRectangleReason(int box, IntegerValue xmin, IntegerValue xmax,
                               IntegerValue ymin, IntegerValue ymax);

  // Updates the boxes positions and size when the given box is before other in
  // the passed direction. This will fill integer_reason_ if it is empty,
  // otherwise, it will reuse its current value.
  bool FirstBoxIsBeforeSecondBox(const std::vector<IntegerVariable>& starts,
                                 const std::vector<IntegerValue>& sizes,
                                 int box, int other);

  const std::vector<IntegerVariable> start_x_;
  const std::vector<IntegerValue> size_x_;
  const std::vector<IntegerVariable> start_y_;
  const std::vector<IntegerValue> size_y_;

  // The neighbors_ of a box will be in
  // [neighbors_[begin[box]], neighbors_[end[box]])
  std::vector<int> neighbors_;
  std::vector<int> neighbors_begins_;
  std::vector<int> neighbors_ends_;
  std::vector<int> tmp_removed_;

  std::vector<IntegerValue> cached_areas_;
  std::vector<IntegerValue> cached_distance_to_bounding_box_;
  std::vector<IntegerLiteral> integer_reason_;

  DISALLOW_COPY_AND_ASSIGN(FixedDiff2);
};


// Enforces that the boxes with corners in (x, y), (x + dx, y), (x, y + dy)
// and (x + dx, y + dy) do not overlap.
// If one box has a zero dimension, then it can be placed anywhere.
inline std::function<void(Model*)> NonOverlappingRectangles(
    const std::vector<IntegerVariable>& start_x,
    const std::vector<IntegerVariable>& size_x,
    const std::vector<IntegerVariable>& end_x,
    const std::vector<IntegerVariable>& start_y,
    const std::vector<IntegerVariable>& size_y,
    const std::vector<IntegerVariable>& end_y) {
  return [=](Model* model) {
    NonOverlappingRectanglesPropagator* constraint =
        new NonOverlappingRectanglesPropagator(
            start_x, size_x, end_x, start_y, size_y, end_y, false,
            model->GetOrCreate<IntegerTrail>());
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

// Enforces that the boxes with corners in (x, y), (x + dx, y), (x, y + dy)
// and (x + dx, y + dy) do not overlap.
// If one box has a zero dimension, it still cannot intersect another box.
inline std::function<void(Model*)> StrictNonOverlappingRectangles(
    const std::vector<IntegerVariable>& start_x,
    const std::vector<IntegerVariable>& size_x,
    const std::vector<IntegerVariable>& end_x,
    const std::vector<IntegerVariable>& start_y,
    const std::vector<IntegerVariable>& size_y,
    const std::vector<IntegerVariable>& end_y) {
  return [=](Model* model) {
    NonOverlappingRectanglesPropagator* constraint =
        new NonOverlappingRectanglesPropagator(
            start_x, size_x, end_x, start_y, size_y, end_y, true,
            model->GetOrCreate<IntegerTrail>());
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

// Enforces that the boxes with corners in (x, y), (x + dx, y), (x, y + dy)
// and (x + dx, y + dy) do not overlap.
// If one box has a zero dimension, then it can be placed anywhere.
inline std::function<void(Model*)> FixedNonOverlappingRectangles(
    const std::vector<IntegerVariable>& start_x,
    const std::vector<IntegerValue>& size_x,
    const std::vector<IntegerVariable>& start_y,
    const std::vector<IntegerValue>& size_y) {
  return [=](Model* model) {
    FixedDiff2* constraint = new FixedDiff2(start_x, size_x, start_y, size_y,
                                            model->GetOrCreate<IntegerTrail>());
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_DIFFN_H_