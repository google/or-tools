// Copyright 2010-2017 Google
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

#ifndef OR_TOOLS_SAT_CP_CONSTRAINTS_H_
#define OR_TOOLS_SAT_CP_CONSTRAINTS_H_

#include <functional>
#include <memory>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/int_type.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/rev.h"

namespace operations_research {
namespace sat {

// Propagate the fact that a XOR of literals is equal to the given value.
// The complexity is in O(n).
//
// TODO(user): By using a two watcher mechanism, we can propagate this a lot
// faster.
class BooleanXorPropagator : public PropagatorInterface {
 public:
  BooleanXorPropagator(const std::vector<Literal>& literals, bool value,
                       Trail* trail, IntegerTrail* integer_trail)
      : literals_(literals),
        value_(value),
        trail_(trail),
        integer_trail_(integer_trail) {}

  bool Propagate() final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  const std::vector<Literal> literals_;
  const bool value_;
  std::vector<Literal> literal_reason_;
  Trail* trail_;
  IntegerTrail* integer_trail_;

  DISALLOW_COPY_AND_ASSIGN(BooleanXorPropagator);
};

// Base class to help writing CP inspired constraints.
class CpPropagator : public PropagatorInterface {
 public:
  explicit CpPropagator(IntegerTrail* integer_trail);
  ~CpPropagator() override;

  // ----- Shortcuts to integer variables -----

  // Bound getters.
  IntegerValue Min(IntegerVariable v) const;
  IntegerValue Max(IntegerVariable v) const;
  IntegerValue Min(IntegerValue v) const { return v; }
  IntegerValue Max(IntegerValue v) const { return v; }

  // Bound setters. They expects integer_reason to be filled and it will be
  // used to create the conflicts. These setters are monotonic, i.e. setting a
  // min value lower or equal to the current min of the variable will result
  // in a no-op.
  bool SetMin(IntegerVariable v, IntegerValue value,
              const std::vector<IntegerLiteral>& reason);
  bool SetMax(IntegerVariable v, IntegerValue value,
              const std::vector<IntegerLiteral>& reason);
  bool SetMin(IntegerValue v, IntegerValue value,
              const std::vector<IntegerLiteral>& reason);
  bool SetMax(IntegerValue v, IntegerValue value,
              const std::vector<IntegerLiteral>& reason);

  // ----- Conflict management -----

  // Manage the list of integer bounds used to build the reason of propagation.
  void AddLowerBoundReason(IntegerVariable v,
                           std::vector<IntegerLiteral>* reason) const;
  void AddUpperBoundReason(IntegerVariable v,
                           std::vector<IntegerLiteral>* reason) const;
  void AddBoundsReason(IntegerVariable v,
                       std::vector<IntegerLiteral>* reason) const;

  void AddLowerBoundReason(IntegerValue v,
                           std::vector<IntegerLiteral>* reason) const {}
  void AddUpperBoundReason(IntegerValue v,
                           std::vector<IntegerLiteral>* reason) const {}
  void AddBoundsReason(IntegerValue v,
                       std::vector<IntegerLiteral>* reason) const {}

 protected:
  IntegerTrail* integer_trail_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CpPropagator);
};

// Non overlapping rectangles.
template <class S>
class NonOverlappingRectanglesPropagator : public CpPropagator {
 public:
  // The strict parameters indicates how to place zero width or zero height
  // boxes. If strict is true, these boxes must not 'cross' another box, and are
  // pushed by the other boxes.
  NonOverlappingRectanglesPropagator(const std::vector<IntegerVariable>& x,
                                     const std::vector<IntegerVariable>& y,
                                     const std::vector<S>& dx,
                                     const std::vector<S>& dy, bool strict,
                                     IntegerTrail* integer_trail);
  ~NonOverlappingRectanglesPropagator() override;

  // TODO(user): Look at intervals to to store x and dx, y and dy.
  bool Propagate() final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  void UpdateNeighbors(int box);
  bool FailWhenEnergyIsTooLarge(int box);
  bool PushOneBox(int box, int other);
  void AddBoxReason(int box);
  void AddBoxReason(int box, IntegerValue xmin, IntegerValue xmax,
                    IntegerValue ymin, IntegerValue ymax);

  // Updates the boxes positions and size when the given box is before other in
  // the passed direction. This will fill integer_reason_ if it is empty,
  // otherwise, it will reuse its current value.
  bool FirstBoxIsBeforeSecondBox(const std::vector<IntegerVariable>& pos,
                                 const std::vector<S>& size, int box, int other,
                                 std::vector<IntegerValue>* min_end);

  const std::vector<IntegerVariable> x_;
  const std::vector<IntegerVariable> y_;
  const std::vector<S> dx_;
  const std::vector<S> dy_;
  const bool strict_;

  // The neighbors_ of a box will be in
  // [neighbors_[begin[box]], neighbors_[end[box]])
  std::vector<int> neighbors_;
  std::vector<int> neighbors_begins_;
  std::vector<int> neighbors_ends_;
  std::vector<int> tmp_removed_;

  std::vector<IntegerValue> cached_areas_;
  std::vector<IntegerValue> cached_min_end_x_;
  std::vector<IntegerValue> cached_min_end_y_;
  std::vector<IntegerValue> cached_distance_to_bounding_box_;
  std::vector<IntegerLiteral> integer_reason_;

  DISALLOW_COPY_AND_ASSIGN(NonOverlappingRectanglesPropagator);
};

// If we have:
//  - selectors[i] =>  (target_var >= vars[i] + offset[i])
//  - and we known that at least one selectors[i] must be true
// then we can propagate the fact that if no selectors is chosen yet, the lower
// bound of target_var is greater than the min of the still possible
// alternatives.
//
// This constraint take care of this case when no selectors[i] is chosen yet.
class GreaterThanAtLeastOneOfPropagator : public PropagatorInterface {
 public:
  GreaterThanAtLeastOneOfPropagator(IntegerVariable target_var,
                                    const std::vector<IntegerVariable>& vars,
                                    const std::vector<IntegerValue>& offsets,
                                    const std::vector<Literal>& selectors,
                                    Model* model);

  bool Propagate() final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  const IntegerVariable target_var_;
  const std::vector<IntegerVariable> vars_;
  const std::vector<IntegerValue> offsets_;
  const std::vector<Literal> selectors_;

  Trail* trail_;
  IntegerTrail* integer_trail_;

  std::vector<Literal> literal_reason_;
  std::vector<IntegerLiteral> integer_reason_;

  DISALLOW_COPY_AND_ASSIGN(GreaterThanAtLeastOneOfPropagator);
};

// ============================================================================
// Model based functions.
// ============================================================================

inline std::vector<IntegerValue> ToIntegerValueVector(
    const std::vector<int64>& input) {
  std::vector<IntegerValue> result(input.size());
  for (int i = 0; i < input.size(); ++i) {
    result[i] = IntegerValue(input[i]);
  }
  return result;
}

// Enforces the XOR of a set of literals to be equal to the given value.
inline std::function<void(Model*)> LiteralXorIs(
    const std::vector<Literal>& literals, bool value) {
  return [=](Model* model) {
    Trail* trail = model->GetOrCreate<Trail>();
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    BooleanXorPropagator* constraint =
        new BooleanXorPropagator(literals, value, trail, integer_trail);
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

// Enforces that the boxes with corners in (x, y), (x + dx, y), (x, y + dy)
// and (x + dx, y + dy) do not overlap.
// If one box has a zero dimension, then it can be placed anywhere.
inline std::function<void(Model*)> NonOverlappingRectangles(
    const std::vector<IntegerVariable>& x,
    const std::vector<IntegerVariable>& y,
    const std::vector<IntegerVariable>& dx,
    const std::vector<IntegerVariable>& dy) {
  return [=](Model* model) {
    NonOverlappingRectanglesPropagator<IntegerVariable>* constraint =
        new NonOverlappingRectanglesPropagator<IntegerVariable>(
            x, y, dx, dy, false, model->GetOrCreate<IntegerTrail>());
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

// Enforces that the boxes with corners in (x, y), (x + dx, y), (x, y + dy)
// and (x + dx, y + dy) do not overlap.
// If one box has a zero dimension, then it can be placed anywhere.
inline std::function<void(Model*)> NonOverlappingFixedSizeRectangles(
    const std::vector<IntegerVariable>& x,
    const std::vector<IntegerVariable>& y, const std::vector<int64>& dx,
    const std::vector<int64>& dy) {
  return [=](Model* model) {
    NonOverlappingRectanglesPropagator<IntegerValue>* constraint =
        new NonOverlappingRectanglesPropagator<IntegerValue>(
            x, y, ToIntegerValueVector(dx), ToIntegerValueVector(dy), false,
            model->GetOrCreate<IntegerTrail>());
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

// Enforces that the boxes with corners in (x, y), (x + dx, y), (x, y + dy)
// and (x + dx, y + dy) do not overlap.
// If one box has a zero dimension, it still cannot intersect another box.
inline std::function<void(Model*)> StrictNonOverlappingRectangles(
    const std::vector<IntegerVariable>& x,
    const std::vector<IntegerVariable>& y,
    const std::vector<IntegerVariable>& dx,
    const std::vector<IntegerVariable>& dy) {
  return [=](Model* model) {
    NonOverlappingRectanglesPropagator<IntegerVariable>* constraint =
        new NonOverlappingRectanglesPropagator<IntegerVariable>(
            x, y, dx, dy, true, model->GetOrCreate<IntegerTrail>());
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

// Enforces that the boxes with corners in (x, y), (x + dx, y), (x, y + dy)
// and (x + dx, y + dy) do not overlap.
// If one box has a zero dimension, it still cannot intersect another box.
inline std::function<void(Model*)> StrictNonOverlappingFixedSizeRectangles(
    const std::vector<IntegerVariable>& x,
    const std::vector<IntegerVariable>& y, const std::vector<int64>& dx,
    const std::vector<int64>& dy) {
  return [=](Model* model) {
    NonOverlappingRectanglesPropagator<IntegerValue>* constraint =
        new NonOverlappingRectanglesPropagator<IntegerValue>(
            x, y, ToIntegerValueVector(dx), ToIntegerValueVector(dy), true,
            model->GetOrCreate<IntegerTrail>());
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

inline std::function<void(Model*)> GreaterThanAtLeastOneOf(
    IntegerVariable target_var, const std::vector<IntegerVariable>& vars,
    const std::vector<IntegerValue>& offsets,
    const std::vector<Literal>& selectors) {
  return [=](Model* model) {
    GreaterThanAtLeastOneOfPropagator* constraint =
        new GreaterThanAtLeastOneOfPropagator(target_var, vars, offsets,
                                              selectors, model);
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

// The target variable is equal to exactly one of the candidate variable. The
// equality is controlled by the given "selector" literals.
//
// Note(user): This only propagate from the min/max of still possible candidates
// to the min/max of the target variable. The full constraint also requires
// to deal with the case when one of the literal is true.
//
// Note(user): If there is just one or two candidates, this doesn't add
// anything.
inline std::function<void(Model*)> PartialIsOneOfVar(
    IntegerVariable target_var, const std::vector<IntegerVariable>& vars,
    const std::vector<Literal>& selectors) {
  CHECK_EQ(vars.size(), selectors.size());
  return [=](Model* model) {
    const std::vector<IntegerValue> offsets(vars.size(), IntegerValue(0));
    if (vars.size() > 2) {
      // Propagate the min.
      model->Add(GreaterThanAtLeastOneOf(target_var, vars, offsets, selectors));
    }
    if (vars.size() > 2) {
      // Propagate the max.
      model->Add(GreaterThanAtLeastOneOf(NegationOf(target_var),
                                         NegationOf(vars), offsets, selectors));
    }
  };
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_CONSTRAINTS_H_
