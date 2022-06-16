// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_SAT_THETA_TREE_H_
#define OR_TOOLS_SAT_THETA_TREE_H_

#include <cstdint>
#include <vector>

#include "ortools/base/logging.h"
#include "ortools/sat/integer.h"

namespace operations_research {
namespace sat {

// The Theta-Lambda tree can be used to implement several scheduling algorithms.
//
// This template class is instantiated only for IntegerValue and int64_t.
//
// The tree structure itself is a binary tree coded in a vector, where node 0 is
// unused, node 1 is the root, node 2 is the left child of the root, node 3 its
// right child, etc.
//
// The API gives access to rightmost events that realize a given envelope.
//
// See:
// _ (0) Petr Vilim's PhD thesis "Global Constraints in Scheduling".
// _ (1) Petr Vilim "Edge Finding Filtering Algorithm for Discrete Cumulative
//   Resources in O(kn log n)"
// _ (2) Petr Vilim "Max energy filtering algorithm for discrete cumulative
//   resources".
// _ (3) Wolf & Schrader "O(n log n) Overload Checking for the Cumulative
//   Constraint and Its Application".
// _ (4) Kameugne & Fotso "A cumulative not-first/not-last filtering algorithm
//   in O(n^2 log n)".
// _ (5) Ouellet & Quimper "Time-table extended-edge-finding for the cumulative
//   constraint".
//
// Instead of providing one declination of the theta-tree per possible filtering
// algorithm, this generalization intends to provide a data structure that can
// fit several algorithms.
// This tree is based around the notion of events. It has events at its leaves
// that can be present or absent, and present events come with an
// initial_envelope, a minimal and a maximal energy.
// All nodes maintain values on the set of present events under them:
// _ sum_energy_min(node) = sum_{leaf \in leaves(node)} energy_min(leaf)
// _ envelope(node) =
//     max_{leaf \in leaves(node)}
//       initial_envelope(leaf) +
//       sum_{leaf' \in leaves(node), leaf' >= leaf} energy_min(leaf').
//
// Thus, the envelope of a leaf representing an event, when present, is
//   initial_envelope(event) + sum_energy_min(event).
//
// We also maintain envelope_opt with is the maximum envelope a node could take
// if at most one of the events were at its maximum energy.
// _ energy_delta(leaf) = energy_max(leaf) - energy_min(leaf)
// _ max_energy_delta(node) = max_{leaf \in leaves(node)} energy_delta(leaf)
// _ envelope_opt(node) =
//     max_{leaf \in leaves(node)}
//       initial_envelope(leaf) +
//       sum_{leaf' \in leaves(node), leaf' >= leaf} energy_min(leaf') +
//       max_{leaf' \in leaves(node), leaf' >= leaf} energy_delta(leaf');
//
// Most articles using theta-tree variants hack Vilim's original theta tree
// for the disjunctive resource constraint by manipulating envelope and
// energy:
// _ in (0), initial_envelope = start_min, energy = duration
// _ in (3), initial_envelope = C * start_min, energy = demand * duration
// _ in (5), there are several trees in parallel:
//           initial_envelope = C * start_min or (C - h) * start_min
//           energy = demand * duration, h * (Horizon - start_min),
//                    or h * (end_min).
// _ in (2), same as (3), but putting the max energy instead of min in lambda.
// _ in OscaR's TimeTableOverloadChecker,
//   initial_envelope = C * start_min -
//                      energy of mandatory profile before start_min,
//   energy = demand * duration
//
// There is hope to unify the variants of these algorithms by abstracting the
// tasks away to reason only on events.

// The minimal value of an envelope, for instance the envelope of the empty set.
template <typename IntegerType>
constexpr IntegerType IntegerTypeMinimumValue() {
  return std::numeric_limits<IntegerType>::min();
}
template <>
constexpr IntegerValue IntegerTypeMinimumValue() {
  return kMinIntegerValue;
}

template <typename IntegerType>
class ThetaLambdaTree {
 public:
  // Builds a reusable tree. Initialization is done with Reset().
  ThetaLambdaTree();

  // Initializes this class for events in [0, num_events) and makes all of them
  // absent. Instead of allocating and de-allocating trees at every usage, i.e.
  // at every Propagate() of the scheduling algorithms that uses it, this class
  // allows to keep the same memory for each call.
  void Reset(int num_events);

  // Recomputes the values of internal nodes of the tree from the values in the
  // leaves. We enable batching modifications to the tree by providing
  // DelayedXXX() methods that run in O(1), but those methods do not
  // update internal nodes. This breaks tree invariants, so that GetXXX()
  // methods will not reflect modifications made to events.
  // RecomputeTreeForDelayedOperations() restores those invariants in O(n).
  // Thus, batching operations can be done by first doing calls to DelayedXXX()
  // methods, then calling RecomputeTreeForDelayedOperations() once.
  void RecomputeTreeForDelayedOperations();

  // Makes event present and updates its initial envelope and min/max energies.
  // The initial_envelope must be >= ThetaLambdaTreeNegativeInfinity().
  // This updates the tree in O(log n).
  void AddOrUpdateEvent(int event, IntegerType initial_envelope,
                        IntegerType energy_min, IntegerType energy_max);

  // Delayed version of AddOrUpdateEvent(),
  // see RecomputeTreeForDelayedOperations().
  void DelayedAddOrUpdateEvent(int event, IntegerType initial_envelope,
                               IntegerType energy_min, IntegerType energy_max);

  // Adds event to the lambda part of the tree only.
  // This will leave GetEnvelope() unchanged, only GetOptionalEnvelope() can
  // be affected. This is done by setting envelope to IntegerTypeMinimumValue(),
  // energy_min to 0, and initial_envelope_opt and energy_max to the parameters.
  // This updates the tree in O(log n).
  void AddOrUpdateOptionalEvent(int event, IntegerType initial_envelope_opt,
                                IntegerType energy_max);

  // Delayed version of AddOrUpdateOptionalEvent(),
  // see RecomputeTreeForDelayedOperations().
  void DelayedAddOrUpdateOptionalEvent(int event,
                                       IntegerType initial_envelope_opt,
                                       IntegerType energy_max);

  // Makes event absent, compute the new envelope in O(log n).
  void RemoveEvent(int event);

  // Delayed version of RemoveEvent(), see RecomputeTreeForDelayedOperations().
  void DelayedRemoveEvent(int event);

  // Returns the maximum envelope using all the energy_min in O(1).
  // If theta is empty, returns ThetaLambdaTreeNegativeInfinity().
  IntegerType GetEnvelope() const;

  // Returns the maximum envelope using the energy min of all task but
  // one and the energy max of the last one in O(1).
  // If theta and lambda are empty, returns ThetaLambdaTreeNegativeInfinity().
  IntegerType GetOptionalEnvelope() const;

  // Computes the maximum event s.t. GetEnvelopeOf(event) > envelope_max.
  // There must be such an event, i.e. GetEnvelope() > envelope_max.
  // This finds the maximum event e such that
  // initial_envelope(e) + sum_{e' >= e} energy_min(e') > target_envelope.
  // This operation is O(log n).
  int GetMaxEventWithEnvelopeGreaterThan(IntegerType target_envelope) const;

  // Returns initial_envelope(event) + sum_{event' >= event} energy_min(event'),
  // in time O(log n).
  IntegerType GetEnvelopeOf(int event) const;

  // Computes a pair of events (critical_event, optional_event) such that
  // if optional_event was at its maximum energy, the envelope of critical_event
  // would be greater than target_envelope.
  //
  // This assumes that such a pair exists, i.e. GetOptionalEnvelope() should be
  // greater than target_envelope. More formally, this finds events such that:
  //   initial_envelope(critical_event) +
  //   sum_{event' >= critical_event} energy_min(event') +
  //   max_{optional_event >= critical_event} energy_delta(optional_event)
  //     > target envelope.
  //
  // For efficiency reasons, this also fills available_energy with the maximum
  // energy the optional task can take such that the optional envelope of the
  // pair would be target_envelope, i.e.
  //   target_envelope - GetEnvelopeOf(event) + energy_min(optional_event).
  //
  // This operation is O(log n).
  void GetEventsWithOptionalEnvelopeGreaterThan(
      IntegerType target_envelope, int* critical_event, int* optional_event,
      IntegerType* available_energy) const;

  // Getters.
  IntegerType EnergyMin(int event) const {
    return tree_[GetLeafFromEvent(event)].sum_of_energy_min;
  }

 private:
  struct TreeNode {
    IntegerType envelope;
    IntegerType envelope_opt;
    IntegerType sum_of_energy_min;
    IntegerType max_of_energy_delta;
  };

  TreeNode ComposeTreeNodes(TreeNode left, TreeNode right);

  int GetLeafFromEvent(int event) const;
  int GetEventFromLeaf(int leaf) const;

  // Propagates the change of leaf energies and envelopes towards the root.
  void RefreshNode(int node);

  // Finds the maximum leaf under node such that
  // initial_envelope(leaf) + sum_{leaf' >= leaf} energy_min(leaf')
  //   > target_envelope.
  // Fills extra with the difference.
  int GetMaxLeafWithEnvelopeGreaterThan(int node, IntegerType target_envelope,
                                        IntegerType* extra) const;

  // Returns the leaf with maximum energy delta under node.
  int GetLeafWithMaxEnergyDelta(int node) const;

  // Finds the leaves and energy relevant for
  // GetEventsWithOptionalEnvelopeGreaterThan().
  void GetLeavesWithOptionalEnvelopeGreaterThan(
      IntegerType target_envelope, int* critical_leaf, int* optional_leaf,
      IntegerType* available_energy) const;

  // Number of events of the last Reset().
  int num_events_;
  int num_leaves_;
  int power_of_two_;

  // A bool used in debug mode, to check that sequences of delayed operations
  // are ended by Reset() or RecomputeTreeForDelayedOperations().
  bool leaf_nodes_have_delayed_operations_ = false;

  // Envelopes and energies of nodes.
  std::vector<TreeNode> tree_;
};

// Explicit instantiations in theta_Tree.cc.
extern template class ThetaLambdaTree<IntegerValue>;
extern template class ThetaLambdaTree<int64_t>;

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_THETA_TREE_H_
