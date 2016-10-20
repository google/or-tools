// Copyright 2010-2014 Google
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

#include <unordered_map>

#include "sat/integer.h"
#include "sat/model.h"
#include "util/sorted_interval_list.h"

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
                       IntegerTrail* integer_trail)
      : literals_(literals), value_(value), integer_trail_(integer_trail) {}

  bool Propagate(Trail* trail) final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  const std::vector<Literal> literals_;
  const bool value_;
  std::vector<Literal> literal_reason_;
  IntegerTrail* integer_trail_;

  DISALLOW_COPY_AND_ASSIGN(BooleanXorPropagator);
};

// Implement the all different bound consistent propagator with explanation.
// That is, given n variables that must be all different, this propagates the
// bounds of each variables as much as possible. The key is to detect the so
// called Hall interval which are interval of size k that contains the domain
// of k variables. Because all the variables must take different values, we can
// deduce that the domain of the other variables cannot contains such Hall
// interval.
//
// We use a "simple" O(n log n) algorithm.
//
// TODO(user): implement the faster algorithm described in:
// https://cs.uwaterloo.ca/~vanbeek/Publications/ijcai03_TR.pdf
// Note that the algorithms are similar, the gain comes by replacing our
// SortedDisjointIntervalList with a more customized class for our operations.
// It is even possible to get an O(n) complexity if the values of the bounds are
// in a range of size O(n).
class AllDifferentBoundsPropagator : public PropagatorInterface {
 public:
  AllDifferentBoundsPropagator(const std::vector<IntegerVariable>& vars,
                               IntegerTrail* integer_trail);

  bool Propagate(Trail* trail) final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  // Fills integer_reason_ with the reason why we have the given hall interval.
  void FillHallReason(IntegerValue hall_lb, IntegerValue hall_ub);

  // Do half the job of Propagate().
  bool PropagateLowerBounds(Trail* trail);

  std::vector<IntegerVariable> vars_;
  std::vector<IntegerVariable> negated_vars_;
  IntegerTrail* integer_trail_;

  // The sets of "critical" intervals. This has the same meaning as in the
  // disjunctive constraint.
  SortedDisjointIntervalList critical_intervals_;

  // The list of Hall intervalls detected so far, sorted.
  std::vector<IntegerValue> hall_starts_;
  std::vector<IntegerValue> hall_ends_;

  // Members needed for explaining the propagation.
  //
  // The IntegerVariable in an hall interval [lb, ub] are the variables with key
  // in [lb, ub] in this map. Note(user): if the set of bounds is small, we
  // could use a std::vector here. The O(ub - lb) to create the reason is fine
  // since this is the size of the reason.
  //
  // Optimization: we only insert the entry in the map lazily when the reason
  // is needed.
  int64 num_calls_;
  std::vector<std::pair<int64, IntegerVariable>> to_insert_;
  std::unordered_map<int64, IntegerVariable> value_to_variable_;
  std::vector<IntegerLiteral> integer_reason_;

  DISALLOW_COPY_AND_ASSIGN(AllDifferentBoundsPropagator);
};

// Base class to help writing CP inspired constraints.
class CpPropagator : public PropagatorInterface {
 public:
  explicit CpPropagator(Trail* trail, IntegerTrail* integer_trail);
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

  // This method must be called before returning false during propagation.
  void FillConflictFromReason(const std::vector<IntegerLiteral>& reason);

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
  Trail* trail_;
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
                                     Trail* trail, IntegerTrail* integer_trail);
  ~NonOverlappingRectanglesPropagator() override;

  // TODO(user): Look at intervals to to store x and dx, y and dy.
  bool Propagate(Trail* trail) final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  bool CanBoxesOverlap(int i, int j) const;
  bool AreBoxesDisjoingHorizontallyForSure(int i, int j) const;
  bool AreBoxesDisjoingVerticallyForSure(int i, int j) const;
  void FillNeighbors(int box);
  bool FailWhenEnergyIsTooLarge(int box);
  bool PushOneBox(int box, int other);
  void AddBoxReason(int box);
  IntegerValue DistanceToBoundingBox(int box, int other);

  const std::vector<IntegerVariable> x_;
  const std::vector<IntegerVariable> y_;
  const std::vector<S> dx_;
  const std::vector<S> dy_;
  const bool strict_;

  std::vector<int> neighbors_;
  std::vector<IntegerLiteral> integer_reason_;

  DISALLOW_COPY_AND_ASSIGN(NonOverlappingRectanglesPropagator);
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
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    BooleanXorPropagator* constraint =
        new BooleanXorPropagator(literals, value, integer_trail);
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

// Enforces that the given tuple of variables takes different values.
std::function<void(Model*)> AllDifferent(
    const std::vector<IntegerVariable>& vars);

// Enforces that the given tuple of variables takes different values.
// Same as AllDifferent() but use a different propagator that only enforce
// the so called "bound consistency" on the variable domains.
//
// Compared to AllDifferent() this doesn't require fully encoding the variables
// and it is also quite fast. Note that the propagation is different, this will
// not remove already taken values from inside a domain, but it will propagates
// more the domain bounds.
inline std::function<void(Model*)> AllDifferentOnBounds(
    const std::vector<IntegerVariable>& vars) {
  return [=](Model* model) {
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    AllDifferentBoundsPropagator* constraint =
        new AllDifferentBoundsPropagator(vars, integer_trail);
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
    Trail* trail = model->GetOrCreate<Trail>();
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    NonOverlappingRectanglesPropagator<IntegerVariable>* constraint =
        new NonOverlappingRectanglesPropagator<IntegerVariable>(
            x, y, dx, dy, false, trail, integer_trail);
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
    Trail* trail = model->GetOrCreate<Trail>();
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    NonOverlappingRectanglesPropagator<IntegerValue>* constraint =
        new NonOverlappingRectanglesPropagator<IntegerValue>(
            x, y, ToIntegerValueVector(dx), ToIntegerValueVector(dy), false,
            trail, integer_trail);
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
    Trail* trail = model->GetOrCreate<Trail>();
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    NonOverlappingRectanglesPropagator<IntegerVariable>* constraint =
        new NonOverlappingRectanglesPropagator<IntegerVariable>(
            x, y, dx, dy, true, trail, integer_trail);
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
    Trail* trail = model->GetOrCreate<Trail>();
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    NonOverlappingRectanglesPropagator<IntegerValue>* constraint =
        new NonOverlappingRectanglesPropagator<IntegerValue>(
            x, y, ToIntegerValueVector(dx), ToIntegerValueVector(dy), true,
            trail, integer_trail);
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_CONSTRAINTS_H_
