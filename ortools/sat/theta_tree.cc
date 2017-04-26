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
void ThetaLambdaTree::Reset(std::vector<IntegerValue> event_initial_envelope,
                            std::vector<IntegerValue> event_energy) {
  const int num_events = event_initial_envelope.size();
  DCHECK_EQ(num_events, event_energy.size());
  DCHECK(std::is_sorted(event_initial_envelope.begin(),
                        event_initial_envelope.end()));
  event_initial_envelope_ = std::move(event_initial_envelope);
  event_energy_ = std::move(event_energy);

  // Use 2^k leaves in the tree, with 2^k >= std::max(num_events, 2).
  num_events_ = num_events;
  for (num_leaves_ = 2; num_leaves_ < num_events; num_leaves_ <<= 1) {
  }
  const int num_nodes = 2 * num_leaves_;
  tree_energy_.assign(num_nodes, IntegerValue(0));
  tree_energy_opt_.assign(num_nodes, IntegerValue(0));
  tree_envelope_.assign(num_nodes, kMinIntegerValue);
  tree_envelope_opt_.assign(num_nodes, kMinIntegerValue);
  is_present_.assign(num_events, false);
  is_optional_.assign(num_events, false);
}

void ThetaLambdaTree::AddEvent(int event) {
  DCHECK_LE(0, event);
  DCHECK_LT(event, num_events_);
  DCHECK(!IsPresent(event) && !IsOptional(event));
  is_present_[event] = true;
  is_optional_[event] = false;
  const int node = num_leaves_ + event;
  tree_envelope_[node] = event_initial_envelope_[event] + event_energy_[event];
  tree_energy_[node] = event_energy_[event];
  tree_envelope_opt_[node] = tree_envelope_[node];
  tree_energy_opt_[node] = tree_energy_[node];
  RefreshNode(node);
}

void ThetaLambdaTree::AddOptionalEvent(int event) {
  DCHECK_LE(0, event);
  DCHECK_LT(event, num_events_);
  DCHECK(!IsPresent(event) && !IsOptional(event));
  is_present_[event] = false;
  is_optional_[event] = true;
  const int node = num_leaves_ + event;
  tree_envelope_[node] = kMinIntegerValue;
  tree_energy_[node] = IntegerValue(0);
  tree_envelope_opt_[node] =
      event_initial_envelope_[event] + event_energy_[event];
  tree_energy_opt_[node] = event_energy_[event];
  RefreshNode(node);
}

void ThetaLambdaTree::RemoveEvent(int event) {
  DCHECK_LE(0, event);
  DCHECK_LT(event, num_events_);
  DCHECK(IsPresent(event) || IsOptional(event));
  is_present_[event] = false;
  is_optional_[event] = false;
  const int node = num_leaves_ + event;
  tree_envelope_[node] = kMinIntegerValue;
  tree_energy_[node] = IntegerValue(0);
  tree_envelope_opt_[node] = kMinIntegerValue;
  tree_energy_opt_[node] = IntegerValue(0);
  RefreshNode(node);
}

bool ThetaLambdaTree::IsPresent(int event) const {
  DCHECK_LE(0, event);
  DCHECK_LT(event, num_events_);
  return is_present_[event];
}
bool ThetaLambdaTree::IsOptional(int event) const {
  DCHECK_LE(0, event);
  DCHECK_LT(event, num_events_);
  return is_optional_[event];
}

IntegerValue ThetaLambdaTree::GetEnvelope() const { return tree_envelope_[1]; }
IntegerValue ThetaLambdaTree::GetOptionalEnvelope() const {
  return tree_envelope_opt_[1];
}

int ThetaLambdaTree::GetRightmostEventWithEnvelopeGreaterOrEqualTo(
    IntegerValue target_envelope) const {
  DCHECK_LE(target_envelope, tree_envelope_[1]);
  const int event = GetLeafRealizingEnvelope(1, target_envelope) - num_leaves_;
  DCHECK(IsPresent(event));
  return event;
}

IntegerValue ThetaLambdaTree::GetEnvelopeOf(int event) const {
  DCHECK(IsPresent(event));
  DCHECK_LE(0, event);
  DCHECK_LT(event, num_events_);
  IntegerValue env = tree_envelope_[event + num_leaves_];
  for (int node = event + num_leaves_; node > 1; node >>= 1) {
    const int right = node | 1;
    if (right != node) env += tree_energy_[right];
  }
  return env;
}

int ThetaLambdaTree::GetOptionalEventRealizingOptionalEnvelope() const {
  const int event = GetLeafRealizingOptionalEnvelope<false>(1) - num_leaves_;
  DCHECK(IsOptional(event));
  return event;
}

int ThetaLambdaTree::GetEventRealizingOptionalEnvelope() const {
  const int event = GetLeafRealizingOptionalEnvelope<true>(1) - num_leaves_;
  DCHECK(IsPresent(event) || IsOptional(event));
  return event;
}

void ThetaLambdaTree::RefreshNode(int node) {
  do {
    const int right = node | 1;
    const int left = right ^ 1;
    node >>= 1;
    tree_energy_[node] = tree_energy_[left] + tree_energy_[right];
    tree_envelope_[node] = std::max(tree_envelope_[right],
                                    tree_envelope_[left] + tree_energy_[right]);
    tree_energy_opt_[node] =
        std::max(tree_energy_[left] + tree_energy_opt_[right],
                 tree_energy_[right] + tree_energy_opt_[left]);
    tree_envelope_opt_[node] =
        std::max(tree_envelope_opt_[left] + tree_energy_[right],
                 tree_envelope_[left] + tree_energy_opt_[right]);
    tree_envelope_opt_[node] =
        std::max(tree_envelope_opt_[node], tree_envelope_opt_[right]);
  } while (node > 1);
}

int ThetaLambdaTree::GetLeafRealizingEnvelope(int node,
                                              IntegerValue envelope) const {
  DCHECK_LE(envelope, tree_envelope_[node]);
  while (node < num_leaves_) {
    const int left = node << 1;
    const int right = left | 1;

    if (envelope <= tree_envelope_[right]) {
      node = right;
    } else {
      envelope -= tree_energy_[right];
      node = left;
    }
  }
  return node;
}

int ThetaLambdaTree::GetOptionalLeafOfMaximalEnergy(int node) const {
  while (node < num_leaves_) {
    const int left = node << 1;
    const int right = left | 1;

    if (tree_energy_opt_[node] ==
        tree_energy_[left] + tree_energy_opt_[right]) {
      node = right;
    } else {  // tree_energy_opt_[left] + tree_energy_[right]
      node = left;
    }
  }
  return node;
}

template <bool initial_envelope_leaf_wanted>
int ThetaLambdaTree::GetLeafRealizingOptionalEnvelope(int node) const {
  while (node < num_leaves_) {
    const int left = node << 1;
    const int right = left | 1;

    if (tree_envelope_opt_[node] == tree_envelope_opt_[right]) {
      node = right;
    } else if (tree_envelope_opt_[node] ==
               tree_envelope_[left] + tree_energy_opt_[right]) {
      return initial_envelope_leaf_wanted
                 ? GetLeafRealizingEnvelope(left, tree_envelope_[left])
                 : GetOptionalLeafOfMaximalEnergy(right);
    } else {  // tree_envelope_opt_[left] + tree_energy_[right]
      node = left;
    }
  }
  return node;
}

}  // namespace sat
}  // namespace operations_research
