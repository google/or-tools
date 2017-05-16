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

#include "ortools/sat/theta_tree.h"

namespace operations_research {
namespace sat {

ThetaLambdaTree::ThetaLambdaTree() {}

// Make a tree using the first num_events events of the vectors.
void ThetaLambdaTree::Reset(int num_events) {
  // Use 2^k leaves in the tree, with 2^k >= std::max(num_events, 2).
  num_events_ = num_events;
  for (num_leaves_ = 2; num_leaves_ < num_events; num_leaves_ <<= 1) {
  }
  const int num_nodes = 2 * num_leaves_;
  tree_energy_min_.assign(num_nodes, IntegerValue(0));
  tree_energy_opt_.assign(num_nodes, IntegerValue(0));
  tree_envelope_.assign(num_nodes, kMinIntegerValue);
  tree_envelope_opt_.assign(num_nodes, kMinIntegerValue);
}

void ThetaLambdaTree::AddOrUpdateEvent(int event, IntegerValue initial_envelope,
                                       IntegerValue energy_min,
                                       IntegerValue energy_max) {
  DCHECK_LE(0, event);
  DCHECK_LT(event, num_events_);
  DCHECK_LE(0, energy_min);
  DCHECK_LE(energy_min, energy_max);
  const int node = num_leaves_ + event;
  tree_envelope_[node] = initial_envelope + energy_min;
  tree_energy_min_[node] = energy_min;
  tree_envelope_opt_[node] = initial_envelope + energy_max;
  tree_energy_opt_[node] = energy_max;
  RefreshNode(node);
}

void ThetaLambdaTree::RemoveEvent(int event) {
  DCHECK_LE(0, event);
  DCHECK_LT(event, num_events_);
  const int node = num_leaves_ + event;
  tree_envelope_[node] = kMinIntegerValue;
  tree_energy_min_[node] = IntegerValue(0);
  tree_envelope_opt_[node] = kMinIntegerValue;
  tree_energy_opt_[node] = IntegerValue(0);
  RefreshNode(node);
}

IntegerValue ThetaLambdaTree::GetEnvelope() const { return tree_envelope_[1]; }
IntegerValue ThetaLambdaTree::GetOptionalEnvelope() const {
  return tree_envelope_opt_[1];
}

int ThetaLambdaTree::GetMaxEventWithEnvelopeGreaterThan(
    IntegerValue target_envelope) const {
  DCHECK_LT(target_envelope, tree_envelope_[1]);
  return GetMaxLeafWithEnvelopeGreaterThan(1, target_envelope) - num_leaves_;
}

void ThetaLambdaTree::GetEventsWithOptionalEnvelopeGreaterThan(
    IntegerValue target_envelope, int* critical_event, int* optional_event,
    IntegerValue* available_energy) const {
  int critical_leaf;
  int optional_leaf;
  GetLeavesWithOptionalEnvelopeGreaterThan(target_envelope, &critical_leaf,
                                           &optional_leaf, available_energy);
  *critical_event = critical_leaf - num_leaves_;
  *optional_event = optional_leaf - num_leaves_;
}

IntegerValue ThetaLambdaTree::GetEnvelopeOf(int event) const {
  DCHECK_LE(0, event);
  DCHECK_LT(event, num_events_);
  IntegerValue env = tree_envelope_[event + num_leaves_];
  for (int node = event + num_leaves_; node > 1; node >>= 1) {
    const int right = node | 1;
    if (right != node) env += tree_energy_min_[right];
  }
  return env;
}

void ThetaLambdaTree::RefreshNode(int node) {
  do {
    const int right = node | 1;
    const int left = right ^ 1;
    node >>= 1;
    tree_energy_min_[node] = tree_energy_min_[left] + tree_energy_min_[right];
    tree_envelope_[node] = std::max(
        tree_envelope_[right], tree_envelope_[left] + tree_energy_min_[right]);
    tree_energy_opt_[node] =
        std::max(tree_energy_min_[left] + tree_energy_opt_[right],
                 tree_energy_min_[right] + tree_energy_opt_[left]);
    tree_envelope_opt_[node] =
        std::max(tree_envelope_opt_[left] + tree_energy_min_[right],
                 tree_envelope_[left] + tree_energy_opt_[right]);
    tree_envelope_opt_[node] =
        std::max(tree_envelope_opt_[node], tree_envelope_opt_[right]);
  } while (node > 1);
}

int ThetaLambdaTree::GetMaxLeafWithEnvelopeGreaterThan(
    int node, IntegerValue target_envelope) const {
  DCHECK_LT(target_envelope, tree_envelope_[node]);
  while (node < num_leaves_) {
    const int left = node << 1;
    const int right = left | 1;

    if (target_envelope < tree_envelope_[right]) {
      node = right;
    } else {
      target_envelope -= tree_energy_min_[right];
      node = left;
    }
  }
  return node;
}

int ThetaLambdaTree::GetMaxLeafWithOptionalEnergyGreaterThan(
    int node, IntegerValue node_available_energy,
    IntegerValue* available_energy) const {
  DCHECK_LT(node_available_energy, tree_energy_opt_[node]);
  while (node < num_leaves_) {
    const int left = node << 1;
    const int right = left | 1;

    const IntegerValue available_energy_right =
        node_available_energy - tree_energy_min_[left];
    if (available_energy_right < tree_energy_opt_[right]) {
      node_available_energy = available_energy_right;
      node = right;
    } else {  // available_energy_left < tree_energy_opt_[left]
      node_available_energy -= tree_energy_min_[right];
      node = left;
    }
  }
  *available_energy = node_available_energy;
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

    if (target_envelope < tree_envelope_opt_[right]) {
      node = right;
    } else if (target_envelope <
               tree_envelope_[left] + tree_energy_opt_[right]) {
      *critical_leaf =
          GetMaxLeafWithEnvelopeGreaterThan(left, tree_envelope_[left] - 1);
      *optional_leaf = GetMaxLeafWithOptionalEnergyGreaterThan(
          right, target_envelope - tree_envelope_[left], available_energy);
      return;
    } else {  // < tree_envelope_opt_[left] + tree_energy_min_[right]
      target_envelope -= tree_energy_min_[right];
      node = left;
    }
  }
  *critical_leaf = node;
  *optional_leaf = node;
  *available_energy =
      target_envelope - tree_envelope_[node] + tree_energy_min_[node];
}

}  // namespace sat
}  // namespace operations_research
