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

#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/util/sorted_interval_list.h"

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

  bool Propagate() final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  // Fills integer_reason_ with the reason why we have the given hall interval.
  void FillHallReason(IntegerValue hall_lb, IntegerValue hall_ub);

  // Do half the job of Propagate().
  bool PropagateLowerBounds();

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

// Initial version of the circuit/sub-circuit constraint.
// It mainly report conflicts and do not propagate much.
class CircuitPropagator : PropagatorInterface, ReversibleInterface {
 public:
  // The constraints take a dense representation of a graph on [0, n). Each arc
  // being present when the given literal is true. The special values
  // kTrueLiteralIndex and kFalseLiteralIndex can be used for arcs that are
  // either always there or never there.
  CircuitPropagator(const std::vector<std::vector<LiteralIndex>>& graph,
                    bool allow_subcircuit, Trail* trail);

  void SetLevel(int level) final;
  bool Propagate() final;
  bool IncrementalPropagate(const std::vector<int>& watch_indices) final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  // Clears and fills trail_->MutableConflict() with the literals of the arcs
  // that form a cycle containing the given node.
  void FillConflictFromCircuitAt(int start);

  const int num_nodes_;
  const bool allow_subcircuit_;
  Trail* trail_;

  // Internal representation of the graph given at construction. Const.
  struct Arc {
    int tail;
    int head;
  };
  std::vector<LiteralIndex> self_arcs_;

  std::vector<Literal> watch_index_to_literal_;
  std::vector<std::vector<Arc>> watch_index_to_arcs_;

  // Index in trail_ up to which we propagated all the assigned Literals.
  int propagation_trail_index_;

  // Current partial chains of arc that are present.
  std::vector<int> next_;  // -1 if not assigned yet.
  std::vector<int> prev_;  // -1 if not assigned yet.
  std::vector<LiteralIndex> next_literal_;

  // Backtrack support for the partial chains of arcs, level_ends_[level] is an
  // index in added_arcs_;
  std::vector<int> level_ends_;
  std::vector<Arc> added_arcs_;

  // Temporary vector.
  std::vector<bool> in_circuit_;

  DISALLOW_COPY_AND_ASSIGN(CircuitPropagator);
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

// In an "int element" constraint, a target variable is equal to one of a given
// set of candidate variables, each selected by a given literal. This propagator
// is reponsible for updating the min of the target variable using the min of
// the candidate variables that can still be choosen.
class OneOfVarMinPropagator : public PropagatorInterface {
 public:
  OneOfVarMinPropagator(IntegerVariable target_var,
                        const std::vector<IntegerVariable>& vars,
                        const std::vector<Literal>& selectors, Trail* trail,
                        IntegerTrail* integer_trail);

  bool Propagate() final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  const IntegerVariable target_var_;
  const std::vector<IntegerVariable> vars_;
  const std::vector<Literal> selectors_;
  Trail* trail_;
  IntegerTrail* integer_trail_;

  std::vector<Literal> literal_reason_;
  std::vector<IntegerLiteral> integer_reason_;

  DISALLOW_COPY_AND_ASSIGN(OneOfVarMinPropagator);
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
    AllDifferentBoundsPropagator* constraint = new AllDifferentBoundsPropagator(
        vars, model->GetOrCreate<IntegerTrail>());
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

// Enforces that exactly one literal per rows and per columns is true.
// This only work for a square matrix (but could easily be generalized).
std::function<void(Model*)> ExactlyOnePerRowAndPerColumn(
    const std::vector<std::vector<LiteralIndex>>& square_matrix);

inline std::function<void(Model*)> CircuitConstraint(
    const std::vector<std::vector<LiteralIndex>>& graph) {
  return [=](Model* model) {
    if (graph.empty()) return;
    model->Add(ExactlyOnePerRowAndPerColumn(graph));
    CircuitPropagator* constraint = new CircuitPropagator(
        graph, /*allow_subcircuit=*/false, model->GetOrCreate<Trail>());
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

inline std::function<void(Model*)> SubcircuitConstraint(
    const std::vector<std::vector<LiteralIndex>>& graph) {
  return [=](Model* model) {
    if (graph.empty()) return;
    model->Add(ExactlyOnePerRowAndPerColumn(graph));
    CircuitPropagator* constraint = new CircuitPropagator(
        graph, /*allow_subcircuit=*/true, model->GetOrCreate<Trail>());
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
// Note(user): If there is just one or two candiates, this doesn't add anything.
inline std::function<void(Model*)> PartialIsOneOfVar(
    IntegerVariable target_var, const std::vector<IntegerVariable>& vars,
    const std::vector<Literal>& selectors) {
  CHECK_EQ(vars.size(), selectors.size());
  return [=](Model* model) {
    if (vars.size() > 2) {
      // Propagate the min.
      OneOfVarMinPropagator* constraint = new OneOfVarMinPropagator(
          target_var, vars, selectors, model->GetOrCreate<Trail>(),
          model->GetOrCreate<IntegerTrail>());
      constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
      model->TakeOwnership(constraint);
    }
    if (vars.size() > 2) {
      // Propagate the max.
      OneOfVarMinPropagator* constraint = new OneOfVarMinPropagator(
          NegationOf(target_var), NegationOf(vars), selectors,
          model->GetOrCreate<Trail>(), model->GetOrCreate<IntegerTrail>());
      constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
      model->TakeOwnership(constraint);
    }
  };
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_CONSTRAINTS_H_
