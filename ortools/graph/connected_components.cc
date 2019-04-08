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

//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// The following uses disjoint-sets algorithms, see:
// https://en.wikipedia.org/wiki/Disjoint-set_data_structure#Disjoint-set_forests

#include "ortools/graph/connected_components.h"

#include <numeric>

void DenseConnectedComponentsFinder::SetNumberOfNodes(int num_nodes) {
  const int old_num_nodes = GetNumberOfNodes();
  if (num_nodes == old_num_nodes) {
    return;
  }
  CHECK_GT(num_nodes, old_num_nodes);
  // Each new node starts as an isolated component:
  // It has itself as root.
  parent_.resize(num_nodes);
  std::iota(parent_.begin() + old_num_nodes, parent_.end(), old_num_nodes);
  //  It's in an isolated component of size 1.
  component_size_.resize(num_nodes, 1);
  //  Its rank is 0.
  rank_.resize(num_nodes);
  // This introduces one extra component per added node.
  num_components_ += num_nodes - old_num_nodes;
}

int DenseConnectedComponentsFinder::FindRoot(int node) {
  DCHECK_GE(node, 0);
  DCHECK_LT(node, GetNumberOfNodes());

  // Search the root.
  int root = parent_[node];
  while (parent_[root] != root) {
    root = parent_[root];
  }

  // Apply path compression.
  while (node != root) {
    const int prev_parent = parent_[node];
    parent_[node] = root;
    node = prev_parent;
  }
  return root;
}

void DenseConnectedComponentsFinder::AddEdge(int node1, int node2) {
  // Grow if needed.
  const int min_num_nodes = std::max(node1, node2) + 1;
  if (min_num_nodes > GetNumberOfNodes()) {
    SetNumberOfNodes(min_num_nodes);
  }

  // Just union the sets for node1 and node2.
  int root1 = FindRoot(node1);
  int root2 = FindRoot(node2);

  // Already the same set.
  if (root1 == root2) {
    return;
  }

  DCHECK_GE(num_components_, 2);
  --num_components_;

  const int component_size = component_size_[root1] + component_size_[root2];

  // Attach the shallowest tree to root of the deepest one. Note that this
  // operation grows the rank of the new common root by at most one (if the two
  // trees originally have the same rank).
  if (rank_[root1] > rank_[root2]) {
    parent_[root2] = root1;
    component_size_[root1] = component_size;
  } else {
    parent_[root1] = root2;
    component_size_[root2] = component_size;
    // If the ranks were the same then attaching just grew the rank by one.
    if (rank_[root1] == rank_[root2]) {
      ++rank_[root2];
    }
  }
}

bool DenseConnectedComponentsFinder::Connected(int node1, int node2) {
  if (node1 < 0 || node1 >= GetNumberOfNodes() || node2 < 0 ||
      node2 >= GetNumberOfNodes()) {
    return false;
  }
  return FindRoot(node1) == FindRoot(node2);
}

int DenseConnectedComponentsFinder::GetSize(int node) {
  if (node < 0 || node >= GetNumberOfNodes()) {
    return 0;
  }
  return component_size_[FindRoot(node)];
}

std::vector<int> DenseConnectedComponentsFinder::GetComponentIds() {
  std::vector<int> component_ids(GetNumberOfNodes(), -1);
  int current_component = 0;
  for (int node = 0; node < GetNumberOfNodes(); ++node) {
    int& root_component = component_ids[FindRoot(node)];
    if (root_component < 0) {
      // This is the first node in a yet unseen component.
      root_component = current_component;
      ++current_component;
    }
    component_ids[node] = root_component;
  }
  return component_ids;
}
