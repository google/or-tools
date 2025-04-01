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

#ifndef OR_TOOLS_UTIL_SCHEDULING_H_
#define OR_TOOLS_UTIL_SCHEDULING_H_

#include <algorithm>
#include <limits>
#include <vector>

#include "absl/log/check.h"

namespace operations_research {

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

template <typename IntegerType>
class ThetaLambdaTree {
 public:
  // Builds a reusable tree. Initialization is done with Reset().
  ThetaLambdaTree() = default;

  // Initializes this class for events in [0, num_events) and makes all of them
  // absent. Instead of allocating and de-allocating trees at every usage, i.e.
  // at every Propagate() of the scheduling algorithms that uses it, this class
  // allows to keep the same memory for each call.
  void Reset(int num_events);

  // Makes event present and updates its initial envelope and min/max energies.
  // This updates the tree in O(log n).
  void AddOrUpdateEvent(int event, IntegerType initial_envelope,
                        IntegerType energy_min, IntegerType energy_max) {
    DCHECK_LE(0, energy_min);
    DCHECK_LE(energy_min, energy_max);
    const int node = GetLeafFromEvent(event);
    tree_[node] = {.envelope = initial_envelope + energy_min,
                   .envelope_opt = initial_envelope + energy_max,
                   .sum_of_energy_min = energy_min,
                   .max_of_energy_delta = energy_max - energy_min};
    RefreshNode(node);
  }

  // Adds event to the lambda part of the tree only.
  // This will leave GetEnvelope() unchanged, only GetOptionalEnvelope() can
  // be affected, by setting envelope to std::numeric_limits<>::min(),
  // energy_min to 0, and initial_envelope_opt and energy_max to the parameters.
  // This updates the tree in O(log n).
  void AddOrUpdateOptionalEvent(int event, IntegerType initial_envelope_opt,
                                IntegerType energy_max) {
    DCHECK_LE(0, energy_max);
    const int node = GetLeafFromEvent(event);
    tree_[node] = {.envelope = std::numeric_limits<IntegerType>::min(),
                   .envelope_opt = initial_envelope_opt + energy_max,
                   .sum_of_energy_min = IntegerType{0},
                   .max_of_energy_delta = energy_max};
    RefreshNode(node);
  }

  // Makes event absent, compute the new envelope in O(log n).
  void RemoveEvent(int event) {
    const int node = GetLeafFromEvent(event);
    tree_[node] = {.envelope = std::numeric_limits<IntegerType>::min(),
                   .envelope_opt = std::numeric_limits<IntegerType>::min(),
                   .sum_of_energy_min = IntegerType{0},
                   .max_of_energy_delta = IntegerType{0}};
    RefreshNode(node);
  }

  // Returns the maximum envelope using all the energy_min in O(1).
  // If theta is empty, returns std::numeric_limits<>::min().
  IntegerType GetEnvelope() const { return tree_[1].envelope; }

  // Returns the maximum envelope using the energy min of all task but
  // one and the energy max of the last one in O(1).
  // If theta and lambda are empty, returns std::numeric_limits<>::min().
  IntegerType GetOptionalEnvelope() const { return tree_[1].envelope_opt; }

  // Computes the maximum event s.t. GetEnvelopeOf(event) > envelope_max.
  // There must be such an event, i.e. GetEnvelope() > envelope_max.
  // This finds the maximum event e such that
  // initial_envelope(e) + sum_{e' >= e} energy_min(e') > target_envelope.
  // This operation is O(log n).
  int GetMaxEventWithEnvelopeGreaterThan(IntegerType target_envelope) const {
    DCHECK_LT(target_envelope, tree_[1].envelope);
    IntegerType unused;
    return GetEventFromLeaf(
        GetMaxLeafWithEnvelopeGreaterThan(1, target_envelope, &unused));
  }

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
      IntegerType* available_energy) const {
    int critical_leaf;
    int optional_leaf;
    GetLeavesWithOptionalEnvelopeGreaterThan(target_envelope, &critical_leaf,
                                             &optional_leaf, available_energy);
    *critical_event = GetEventFromLeaf(critical_leaf);
    *optional_event = GetEventFromLeaf(optional_leaf);
  }

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

  TreeNode ComposeTreeNodes(const TreeNode& left, const TreeNode& right) {
    return {
        .envelope =
            std::max(right.envelope, left.envelope + right.sum_of_energy_min),
        .envelope_opt =
            std::max(right.envelope_opt,
                     right.sum_of_energy_min +
                         std::max(left.envelope_opt,
                                  left.envelope + right.max_of_energy_delta)),
        .sum_of_energy_min = left.sum_of_energy_min + right.sum_of_energy_min,
        .max_of_energy_delta =
            std::max(right.max_of_energy_delta, left.max_of_energy_delta)};
  }

  int GetLeafFromEvent(int event) const {
    DCHECK_LE(0, event);
    DCHECK_LT(event, num_events_);
    // Keeping the ordering of events is important, so the first set of events
    // must be mapped to the set of leaves at depth d, and the second set of
    // events must be mapped to the set of leaves at depth d-1.
    const int r = power_of_two_ + event;
    return r < 2 * num_leaves_ ? r : r - num_leaves_;
  }

  int GetEventFromLeaf(int leaf) const {
    DCHECK_GE(leaf, num_leaves_);
    DCHECK_LT(leaf, 2 * num_leaves_);
    const int r = leaf - power_of_two_;
    return r >= 0 ? r : r + num_leaves_;
  }

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

  // Envelopes and energies of nodes.
  std::vector<TreeNode> tree_;
};

template <typename IntegerType>
void ThetaLambdaTree<IntegerType>::Reset(int num_events) {
  // Because the algorithm needs to access a node sibling (i.e. node_index ^ 1),
  // our tree will always have an even number of leaves, just large enough to
  // fit our number of events. And at least 2 for the empty tree case.
  num_events_ = num_events;
  num_leaves_ = std::max(2, num_events + (num_events & 1));

  const int num_nodes = 2 * num_leaves_;
  tree_.assign(num_nodes,
               TreeNode{.envelope = std::numeric_limits<IntegerType>::min(),
                        .envelope_opt = std::numeric_limits<IntegerType>::min(),
                        .sum_of_energy_min = IntegerType{0},
                        .max_of_energy_delta = IntegerType{0}});

  // If num_leaves is not a power or two, the last depth of the tree will not be
  // full, and the array will look like:
  //   [( num_leafs parents)(leaves at depth d - 1)(leaves at depth d)
  // The first leaves at depth p will have power_of_two_ as index.
  for (power_of_two_ = 2; power_of_two_ < num_leaves_; power_of_two_ <<= 1) {
  }
}

template <typename IntegerType>
IntegerType ThetaLambdaTree<IntegerType>::GetEnvelopeOf(int event) const {
  const int leaf = GetLeafFromEvent(event);
  IntegerType envelope = tree_[leaf].envelope;
  for (int node = leaf; node > 1; node >>= 1) {
    const int right = node | 1;
    if (node != right) envelope += tree_[right].sum_of_energy_min;
  }
  return envelope;
}

template <typename IntegerType>
void ThetaLambdaTree<IntegerType>::RefreshNode(int node) {
  TreeNode* tree = tree_.data();
  do {
    const int right = node | 1;
    const int left = right ^ 1;
    node >>= 1;
    tree[node] = ComposeTreeNodes(tree[left], tree[right]);
  } while (node > 1);
}

template <typename IntegerType>
int ThetaLambdaTree<IntegerType>::GetMaxLeafWithEnvelopeGreaterThan(
    int node, IntegerType target_envelope, IntegerType* extra) const {
  DCHECK_LT(target_envelope, tree_[node].envelope);
  while (node < num_leaves_) {
    const int left = node << 1;
    const int right = left | 1;
    DCHECK_LT(right, tree_.size());

    if (target_envelope < tree_[right].envelope) {
      node = right;
    } else {
      target_envelope -= tree_[right].sum_of_energy_min;
      node = left;
    }
  }
  *extra = tree_[node].envelope - target_envelope;
  return node;
}

template <typename IntegerType>
int ThetaLambdaTree<IntegerType>::GetLeafWithMaxEnergyDelta(int node) const {
  const IntegerType delta_node = tree_[node].max_of_energy_delta;
  while (node < num_leaves_) {
    const int left = node << 1;
    const int right = left | 1;
    DCHECK_LT(right, tree_.size());
    if (tree_[right].max_of_energy_delta == delta_node) {
      node = right;
    } else {
      DCHECK_EQ(tree_[left].max_of_energy_delta, delta_node);
      node = left;
    }
  }
  return node;
}

template <typename IntegerType>
void ThetaLambdaTree<IntegerType>::GetLeavesWithOptionalEnvelopeGreaterThan(
    IntegerType target_envelope, int* critical_leaf, int* optional_leaf,
    IntegerType* available_energy) const {
  DCHECK_LT(target_envelope, tree_[1].envelope_opt);
  int node = 1;
  while (node < num_leaves_) {
    const int left = node << 1;
    const int right = left | 1;
    DCHECK_LT(right, tree_.size());

    if (target_envelope < tree_[right].envelope_opt) {
      node = right;
    } else {
      const IntegerType opt_energy_right =
          tree_[right].sum_of_energy_min + tree_[right].max_of_energy_delta;
      if (target_envelope < tree_[left].envelope + opt_energy_right) {
        *optional_leaf = GetLeafWithMaxEnergyDelta(right);
        IntegerType extra;
        *critical_leaf = GetMaxLeafWithEnvelopeGreaterThan(
            left, target_envelope - opt_energy_right, &extra);
        *available_energy = tree_[*optional_leaf].sum_of_energy_min +
                            tree_[*optional_leaf].max_of_energy_delta - extra;
        return;
      } else {  // < tree_[left].envelope_opt + tree_[right].sum_of_energy_min
        target_envelope -= tree_[right].sum_of_energy_min;
        node = left;
      }
    }
  }
  *critical_leaf = node;
  *optional_leaf = node;
  *available_energy = target_envelope - (tree_[node].envelope_opt -
                                         tree_[node].sum_of_energy_min -
                                         tree_[node].max_of_energy_delta);
}

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_SCHEDULING_H_
