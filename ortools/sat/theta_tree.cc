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

#include "ortools/sat/theta_tree.h"

#include <algorithm>
#include <memory>

#include "ortools/base/int_type.h"

namespace operations_research {
namespace sat {

template <typename IntegerType>
ThetaLambdaTree<IntegerType>::ThetaLambdaTree() {}

template <typename IntegerType>
void ThetaLambdaTree<IntegerType>::Reset(int num_events) {
  // Because the algorithm needs to access a node sibling (i.e. node_index ^ 1),
  // our tree will always have an even number of leaves, just large enough to
  // fit our number of events. And at least 2 for the empty tree case.
  num_events_ = num_events;
  num_leaves_ = std::max(2, num_events + (num_events & 1));

  const int num_nodes = 2 * num_leaves_;
  tree_.assign(num_nodes, TreeNode{IntegerTypeMinimumValue<IntegerType>(),
                                   IntegerTypeMinimumValue<IntegerType>(),
                                   IntegerType{0}, IntegerType{0}});

  // If num_leaves is not a power or two, the last depth of the tree will not be
  // full, and the array will looke like:
  //   [( num_leafs parents)(leaves at depth d - 1)(leaves at depth d)
  // The first leaves at depth p will have power_of_two_ as index.
  for (power_of_two_ = 2; power_of_two_ < num_leaves_; power_of_two_ <<= 1) {
  }
}

template <typename IntegerType>
int ThetaLambdaTree<IntegerType>::GetLeafFromEvent(int event) const {
  DCHECK_LE(0, event);
  DCHECK_LT(event, num_events_);
  // Keeping the ordering of events is important, so the first set of events
  // must be mapped to the set of leaves at depth d, and the second set of
  // events must be mapped to the set of leaves at depth d-1.
  const int r = power_of_two_ + event;
  return r < 2 * num_leaves_ ? r : r - num_leaves_;
}

template <typename IntegerType>
int ThetaLambdaTree<IntegerType>::GetEventFromLeaf(int leaf) const {
  DCHECK_GE(leaf, num_leaves_);
  DCHECK_LT(leaf, 2 * num_leaves_);
  const int r = leaf - power_of_two_;
  return r >= 0 ? r : r + num_leaves_;
}

template <typename IntegerType>
void ThetaLambdaTree<IntegerType>::AddOrUpdateEvent(
    int event, IntegerType initial_envelope, IntegerType energy_min,
    IntegerType energy_max) {
  DCHECK_LE(0, energy_min);
  DCHECK_LE(energy_min, energy_max);
  const int node = GetLeafFromEvent(event);
  tree_[node] = {initial_envelope + energy_min, initial_envelope + energy_max,
                 energy_min, energy_max - energy_min};
  RefreshNode(node);
}

template <typename IntegerType>
void ThetaLambdaTree<IntegerType>::AddOrUpdateOptionalEvent(
    int event, IntegerType initial_envelope_opt, IntegerType energy_max) {
  DCHECK_LE(0, energy_max);
  const int node = GetLeafFromEvent(event);
  tree_[node] = {IntegerTypeMinimumValue<IntegerType>(),
                 initial_envelope_opt + energy_max, IntegerType{0}, energy_max};
  RefreshNode(node);
}

template <typename IntegerType>
void ThetaLambdaTree<IntegerType>::RemoveEvent(int event) {
  const int node = GetLeafFromEvent(event);
  tree_[node] = {IntegerTypeMinimumValue<IntegerType>(),
                 IntegerTypeMinimumValue<IntegerType>(), IntegerType{0},
                 IntegerType{0}};
  RefreshNode(node);
}

template <typename IntegerType>
IntegerType ThetaLambdaTree<IntegerType>::GetEnvelope() const {
  return tree_[1].envelope;
}
template <typename IntegerType>
IntegerType ThetaLambdaTree<IntegerType>::GetOptionalEnvelope() const {
  return tree_[1].envelope_opt;
}

template <typename IntegerType>
int ThetaLambdaTree<IntegerType>::GetMaxEventWithEnvelopeGreaterThan(
    IntegerType target_envelope) const {
  DCHECK_LT(target_envelope, tree_[1].envelope);
  IntegerType unused;
  return GetEventFromLeaf(
      GetMaxLeafWithEnvelopeGreaterThan(1, target_envelope, &unused));
}

template <typename IntegerType>
void ThetaLambdaTree<IntegerType>::GetEventsWithOptionalEnvelopeGreaterThan(
    IntegerType target_envelope, int* critical_event, int* optional_event,
    IntegerType* available_energy) const {
  int critical_leaf;
  int optional_leaf;
  GetLeavesWithOptionalEnvelopeGreaterThan(target_envelope, &critical_leaf,
                                           &optional_leaf, available_energy);
  *critical_event = GetEventFromLeaf(critical_leaf);
  *optional_event = GetEventFromLeaf(optional_leaf);
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
  do {
    const int right = node | 1;
    const int left = right ^ 1;
    node >>= 1;
    const TreeNode lnode = tree_[left];
    const TreeNode rnode = tree_[right];
    tree_[node] = {
        std::max(rnode.envelope, lnode.envelope + rnode.sum_of_energy_min),
        std::max(rnode.envelope_opt,
                 rnode.sum_of_energy_min +
                     std::max(lnode.envelope_opt,
                              lnode.envelope + rnode.max_of_energy_delta)),
        lnode.sum_of_energy_min + rnode.sum_of_energy_min,
        std::max(rnode.max_of_energy_delta, lnode.max_of_energy_delta)};
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

template class ThetaLambdaTree<IntegerValue>;
template class ThetaLambdaTree<int64>;

}  // namespace sat
}  // namespace operations_research
