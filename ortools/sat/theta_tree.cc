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

#include "ortools/sat/theta_tree.h"

#include <algorithm>
#include <memory>

#include "ortools/base/int_type.h"

namespace operations_research {
namespace sat {

ThetaLambdaTree::ThetaLambdaTree() {}

void ThetaLambdaTree::Reset(int num_events) {
  // Because the algorithm needs to access a node sibling (i.e. node_index ^ 1),
  // our tree will always have an even number of leaves, just large enough to
  // fit our number of events. And at least 2 for the empty tree case.
  num_events_ = num_events;
  num_leaves_ = std::max(2, num_events + (num_events & 1));

  const int num_nodes = 2 * num_leaves_;
  tree_envelope_.assign(num_nodes, kMinIntegerValue);
  tree_envelope_opt_.assign(num_nodes, kMinIntegerValue);
  tree_sum_of_energy_min_.assign(num_nodes, IntegerValue(0));
  tree_max_of_energy_delta_.assign(num_nodes, IntegerValue(0));

  // If num_leaves is not a power or two, the last depth of the tree will not be
  // full, and the array will looke like:
  //   [( num_leafs parents)(leaves at depth d - 1)(leaves at depth d)
  // The first leaves at depth p will have power_of_two_ as index.
  for (power_of_two_ = 2; power_of_two_ < num_leaves_; power_of_two_ <<= 1) {
  }
}

int ThetaLambdaTree::GetLeafFromEvent(int event) const {
  DCHECK_LE(0, event);
  DCHECK_LT(event, num_events_);
  // Keeping the ordering of events is important, so the first set of events
  // must be mapped to the set of leaves at depth d, and the second set of
  // events must be mapped to the set of leaves at depth d-1.
  const int r = power_of_two_ + event;
  return r < 2 * num_leaves_ ? r : r - num_leaves_;
}

int ThetaLambdaTree::GetEventFromLeaf(int leaf) const {
  DCHECK_GE(leaf, num_leaves_);
  DCHECK_LT(leaf, 2 * num_leaves_);
  const int r = leaf - power_of_two_;
  return r >= 0 ? r : r + num_leaves_;
}

void ThetaLambdaTree::AddOrUpdateEvent(int event, IntegerValue initial_envelope,
                                       IntegerValue energy_min,
                                       IntegerValue energy_max) {
  DCHECK_LE(0, energy_min);
  DCHECK_LE(energy_min, energy_max);
  const int node = GetLeafFromEvent(event);
  tree_envelope_[node] = initial_envelope + energy_min;
  tree_envelope_opt_[node] = initial_envelope + energy_max;
  tree_sum_of_energy_min_[node] = energy_min;
  tree_max_of_energy_delta_[node] = energy_max - energy_min;
  RefreshNode(node);
}

void ThetaLambdaTree::RemoveEvent(int event) {
  const int node = GetLeafFromEvent(event);
  tree_envelope_[node] = kMinIntegerValue;
  tree_envelope_opt_[node] = kMinIntegerValue;
  tree_sum_of_energy_min_[node] = IntegerValue(0);
  tree_max_of_energy_delta_[node] = IntegerValue(0);
  RefreshNode(node);
}

IntegerValue ThetaLambdaTree::GetEnvelope() const { return tree_envelope_[1]; }
IntegerValue ThetaLambdaTree::GetOptionalEnvelope() const {
  return tree_envelope_opt_[1];
}

int ThetaLambdaTree::GetMaxEventWithEnvelopeGreaterThan(
    IntegerValue target_envelope) const {
  DCHECK_LT(target_envelope, tree_envelope_[1]);
  IntegerValue unused;
  return GetEventFromLeaf(
      GetMaxLeafWithEnvelopeGreaterThan(1, target_envelope, &unused));
}

void ThetaLambdaTree::GetEventsWithOptionalEnvelopeGreaterThan(
    IntegerValue target_envelope, int* critical_event, int* optional_event,
    IntegerValue* available_energy) const {
  int critical_leaf;
  int optional_leaf;
  GetLeavesWithOptionalEnvelopeGreaterThan(target_envelope, &critical_leaf,
                                           &optional_leaf, available_energy);
  *critical_event = GetEventFromLeaf(critical_leaf);
  *optional_event = GetEventFromLeaf(optional_leaf);
}

IntegerValue ThetaLambdaTree::GetEnvelopeOf(int event) const {
  const int leaf = GetLeafFromEvent(event);
  IntegerValue envelope = tree_envelope_[leaf];
  for (int node = leaf; node > 1; node >>= 1) {
    const int right = node | 1;
    if (node != right) envelope += tree_sum_of_energy_min_[right];
  }
  return envelope;
}

void ThetaLambdaTree::RefreshNode(int node) {
  do {
    const int right = node | 1;
    const int left = right ^ 1;
    node >>= 1;
    const IntegerValue energy_right = tree_sum_of_energy_min_[right];
    tree_sum_of_energy_min_[node] =
        tree_sum_of_energy_min_[left] + energy_right;
    tree_max_of_energy_delta_[node] = std::max(tree_max_of_energy_delta_[right],
                                               tree_max_of_energy_delta_[left]);
    tree_envelope_[node] =
        std::max(tree_envelope_[right], tree_envelope_[left] + energy_right);
    tree_envelope_opt_[node] = std::max(
        tree_envelope_opt_[right],
        energy_right +
            std::max(tree_envelope_opt_[left],
                     tree_envelope_[left] + tree_max_of_energy_delta_[right]));
  } while (node > 1);
}

int ThetaLambdaTree::GetMaxLeafWithEnvelopeGreaterThan(
    int node, IntegerValue target_envelope, IntegerValue* extra) const {
  DCHECK_LT(target_envelope, tree_envelope_[node]);
  while (node < num_leaves_) {
    const int left = node << 1;
    const int right = left | 1;
    DCHECK_LT(right, tree_envelope_.size());

    if (target_envelope < tree_envelope_[right]) {
      node = right;
    } else {
      target_envelope -= tree_sum_of_energy_min_[right];
      node = left;
    }
  }
  *extra = tree_envelope_[node] - target_envelope;
  return node;
}

int ThetaLambdaTree::GetLeafWithMaxEnergyDelta(int node) const {
  const IntegerValue delta_node = tree_max_of_energy_delta_[node];
  while (node < num_leaves_) {
    const int left = node << 1;
    const int right = left | 1;
    DCHECK_LT(right, tree_envelope_.size());
    if (tree_max_of_energy_delta_[right] == delta_node) {
      node = right;
    } else {
      DCHECK_EQ(tree_max_of_energy_delta_[left], delta_node);
      node = left;
    }
  }
  return node;
}

void ThetaLambdaTree::GetLeavesWithOptionalEnvelopeGreaterThan(
    IntegerValue target_envelope, int* critical_leaf, int* optional_leaf,
    IntegerValue* available_energy) const {
  DCHECK_LT(target_envelope, tree_envelope_opt_[1]);
  int node = 1;
  while (node < num_leaves_) {
    const int left = node << 1;
    const int right = left | 1;
    DCHECK_LT(right, tree_envelope_.size());

    if (target_envelope < tree_envelope_opt_[right]) {
      node = right;
    } else {
      const IntegerValue opt_energy_right =
          tree_sum_of_energy_min_[right] + tree_max_of_energy_delta_[right];
      if (target_envelope < tree_envelope_[left] + opt_energy_right) {
        *optional_leaf = GetLeafWithMaxEnergyDelta(right);
        IntegerValue extra;
        *critical_leaf = GetMaxLeafWithEnvelopeGreaterThan(
            left, target_envelope - opt_energy_right, &extra);
        *available_energy = tree_sum_of_energy_min_[*optional_leaf] +
                            tree_max_of_energy_delta_[*optional_leaf] - extra;
        return;
      } else {  // < tree_envelope_opt_[left] + tree_sum_of_energy_min_[right]
        target_envelope -= tree_sum_of_energy_min_[right];
        node = left;
      }
    }
  }
  *critical_leaf = node;
  *optional_leaf = node;
  *available_energy =
      target_envelope - tree_envelope_[node] + tree_sum_of_energy_min_[node];
}

}  // namespace sat
}  // namespace operations_research
