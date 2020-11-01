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

#include "ortools/constraint_solver/routing_index_manager.h"

#include <memory>

#include "absl/container/flat_hash_set.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"

namespace operations_research {

const int64 RoutingIndexManager::kUnassigned = -1;

RoutingIndexManager::RoutingIndexManager(int num_nodes, int num_vehicles,
                                         NodeIndex depot)
    : RoutingIndexManager(num_nodes, num_vehicles,
                          std::vector<std::pair<NodeIndex, NodeIndex>>(
                              num_vehicles, {depot, depot})) {}

RoutingIndexManager::RoutingIndexManager(int num_nodes, int num_vehicles,
                                         const std::vector<NodeIndex>& starts,
                                         const std::vector<NodeIndex>& ends) {
  CHECK_EQ(starts.size(), num_vehicles);
  CHECK_EQ(ends.size(), num_vehicles);
  std::vector<std::pair<NodeIndex, NodeIndex>> starts_ends(num_vehicles);
  for (int v = 0; v < num_vehicles; ++v) {
    starts_ends[v] = {starts[v], ends[v]};
  }
  Initialize(num_nodes, num_vehicles, starts_ends);
}

RoutingIndexManager::RoutingIndexManager(
    int num_nodes, int num_vehicles,
    const std::vector<std::pair<NodeIndex, NodeIndex>>& starts_ends) {
  Initialize(num_nodes, num_vehicles, starts_ends);
}

void RoutingIndexManager::Initialize(
    int num_nodes, int num_vehicles,
    const std::vector<std::pair<NodeIndex, NodeIndex>>& starts_ends) {
  static const NodeIndex kZeroNode(0);

  num_nodes_ = num_nodes;
  num_vehicles_ = num_vehicles;
  CHECK_EQ(num_vehicles_, starts_ends.size());
  absl::flat_hash_set<NodeIndex> starts;
  absl::flat_hash_set<NodeIndex> ends;
  absl::flat_hash_set<NodeIndex> unique_depots;
  for (const std::pair<NodeIndex, NodeIndex>& start_end : starts_ends) {
    const NodeIndex start = start_end.first;
    const NodeIndex end = start_end.second;
    CHECK_GE(start, 0);
    CHECK_GE(end, 0);
    CHECK_LE(start, num_nodes_);
    CHECK_LE(end, num_nodes_);
    starts.insert(start);
    ends.insert(end);
    unique_depots.insert(start);
    unique_depots.insert(end);
  }
  num_unique_depots_ = unique_depots.size();
  const int size = num_nodes_ + num_vehicles_ - num_unique_depots_;

  index_to_node_.resize(size + num_vehicles_);
  node_to_index_.resize(num_nodes_, kUnassigned);
  vehicle_to_start_.resize(num_vehicles_);
  vehicle_to_end_.resize(num_vehicles_);
  int64 index = 0;
  for (NodeIndex i = kZeroNode; i < num_nodes_; ++i) {
    if (gtl::ContainsKey(starts, i) || !gtl::ContainsKey(ends, i)) {
      index_to_node_[index] = i;
      node_to_index_[i] = index;
      ++index;
    }
  }
  absl::flat_hash_set<NodeIndex> seen_starts;
  for (int i = 0; i < num_vehicles_; ++i) {
    const NodeIndex start = starts_ends[i].first;
    if (!gtl::ContainsKey(seen_starts, start)) {
      seen_starts.insert(start);
      const int64 start_index = node_to_index_[start];
      vehicle_to_start_[i] = start_index;
      CHECK_NE(kUnassigned, start_index);
    } else {
      vehicle_to_start_[i] = index;
      index_to_node_[index] = start;
      ++index;
    }
  }
  for (int i = 0; i < num_vehicles_; ++i) {
    NodeIndex end = starts_ends[i].second;
    index_to_node_[index] = end;
    vehicle_to_end_[i] = index;
    CHECK_LE(size, index);
    ++index;
  }

  // Logging model information.
  VLOG(1) << "Number of nodes: " << num_nodes_;
  VLOG(1) << "Number of vehicles: " << num_vehicles_;
  for (int64 index = 0; index < index_to_node_.size(); ++index) {
    VLOG(2) << "Variable index " << index << " -> Node index "
            << index_to_node_[index];
  }
  for (NodeIndex node = kZeroNode; node < node_to_index_.size(); ++node) {
    VLOG(2) << "Node index " << node << " -> Variable index "
            << node_to_index_[node];
  }
}

std::vector<int64> RoutingIndexManager::NodesToIndices(
    const std::vector<NodeIndex>& nodes) const {
  std::vector<int64> indices;
  indices.reserve(nodes.size());
  for (const NodeIndex node : nodes) {
    const int64 index = NodeToIndex(node);
    CHECK_NE(kUnassigned, index);
    indices.push_back(index);
  }
  return indices;
}

std::vector<RoutingIndexManager::NodeIndex> RoutingIndexManager::IndicesToNodes(
    const std::vector<int64>& indices) const {
  std::vector<NodeIndex> nodes;
  nodes.reserve(indices.size());
  for (const int64 index : indices) {
    nodes.push_back(IndexToNode(index));
  }
  return nodes;
}

}  // namespace operations_research
