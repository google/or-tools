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

#ifndef ORTOOLS_ROUTING_INDEX_MANAGER_H_
#define ORTOOLS_ROUTING_INDEX_MANAGER_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/base_export.h"
#include "ortools/base/strong_vector.h"
#include "ortools/routing/types.h"

namespace operations_research::routing {

/// @brief Manager for any NodeIndex <-> variable index conversion.
/// @details The routing solver uses variable indices internally and through
/// its API.
/// These variable indices are tricky to manage directly because one Node can
/// correspond to a multitude of variables, depending on the number of times
/// they appear in the model, and if they're used as start and/or end points.
/// This class aims to simplify variable index usage, allowing users to use
/// NodeIndex instead.
///
/// Usage:
///   \code{.cpp}
///   auto starts_ends = ...;  /// These are NodeIndex.
///   IndexManager manager(10, 4, starts_ends);  // 10 nodes, 4 vehicles.
///   Model model(manager);
///   \endcode
///
/// Then, use 'manager.NodeToIndex(node)' whenever model requires a variable
/// index.
///
/// Note: the mapping between node indices and variables indices is subject to
/// change so no assumption should be made on it. The only guarantee is that
/// indices range between 0 and n-1, where n = number of vehicles * 2 (for start
/// and end nodes) + number of non-start or end nodes.
///
class OR_DLL IndexManager {
 public:
  // For compatibility only.
#if !defined(SWIG)
  using NodeIndex = NodeIndex;
#endif  // !defined(SWIG)
  static const int64_t kUnassigned;

  /// @brief Creates a NodeIndex to variable index mapping for a problem
  /// containing 'num_nodes', 'num_vehicles' and the given starts and ends for
  /// each vehicle. If used, any start/end arrays have to have exactly
  /// 'num_vehicles' elements.
  /// @param num_nodes Number of nodes in the problem.
  /// @param num_vehicles Number of vehicles in the problem.
  /// @param depot @ref GetStartIndex "start" and @ref GetEndIndex "end"
  /// NodeIndex for all vehicles.
  IndexManager(int num_nodes, int num_vehicles, NodeIndex depot);
  /// @brief Creates a NodeIndex to variable index mapping.
  /// @param num_nodes Number of nodes in the problem.
  /// @param num_vehicles Number of vehicles in the problem.
  /// @param starts Array containing the start NodeIndex for each vehicle.
  /// @param ends Array containing the end NodeIndex for each vehicle.
  /// @note @b starts and @b ends arrays must have @b exactly @ref num_vehicles
  /// elements.
  IndexManager(int num_nodes, int num_vehicles,
               const std::vector<NodeIndex>& starts,
               const std::vector<NodeIndex>& ends);
  /// @brief Creates a NodeIndex to variable index mapping.
  /// @param num_nodes Number of nodes in the problem.
  /// @param num_vehicles Number of vehicles in the problem.
  /// @param starts_ends Array containing a pair [start,end] NodeIndex for each
  /// vehicle.
  /// @note @b starts_ends arrays must have @b exactly @ref num_vehicles
  /// elements.
  IndexManager(int num_nodes, int num_vehicles,
               const std::vector<std::pair<NodeIndex, NodeIndex>>& starts_ends);

  // Returns the number of nodes in the manager.
  int num_nodes() const { return num_nodes_; }
  // Returns the number of vehicles in the manager.
  int num_vehicles() const { return num_vehicles_; }
  // Returns the number of indices mapped to nodes.
  int num_indices() const { return index_to_node_.size(); }
  // Returns start and end indices of the given vehicle.
  int64_t GetStartIndex(int vehicle) const {
    return vehicle_to_start_[vehicle];
  }
  int64_t GetEndIndex(int vehicle) const { return vehicle_to_end_[vehicle]; }
  // Returns the index of a node. A node can correspond to multiple indices if
  // it's a start or end node. As of 03/2020, kUnassigned will be returned for
  // all end nodes. If a node appears more than once as a start node, the index
  // of the first node in the list of start nodes is returned.
  int64_t NodeToIndex(NodeIndex node) const {
    DCHECK_GE(node.value(), 0);
    DCHECK_LT(node.value(), node_to_index_.size());
    return node_to_index_[node];
  }
  // Same as NodeToIndex but for a given vector of nodes.
  std::vector<int64_t> NodesToIndices(
      const std::vector<NodeIndex>& nodes) const;
  // Returns the node corresponding to an index. A node may appear more than
  // once if it is used as the start or the end node of multiple vehicles.
  NodeIndex IndexToNode(int64_t index) const {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, index_to_node_.size());
    return index_to_node_[index];
  }
  // Same as IndexToNode but for a given vector of indices.
  std::vector<NodeIndex> IndicesToNodes(
      absl::Span<const int64_t> indices) const;
  // TODO(user) Add unit tests for NodesToIndices and IndicesToNodes.
  // TODO(user): Remove when removal of NodeIndex from Model is complete.
  int num_unique_depots() const { return num_unique_depots_; }
  std::vector<NodeIndex> GetIndexToNodeMap() const { return index_to_node_; }

 private:
  void Initialize(
      int num_nodes, int num_vehicles,
      const std::vector<std::pair<NodeIndex, NodeIndex>>& starts_ends);

  std::vector<NodeIndex> index_to_node_;
  util_intops::StrongVector<NodeIndex, int64_t> node_to_index_;
  std::vector<int64_t> vehicle_to_start_;
  std::vector<int64_t> vehicle_to_end_;
  int num_nodes_;
  int num_vehicles_;
  int num_unique_depots_;
};

/// For compatibility.
using RoutingIndexManager = IndexManager;

}  // namespace operations_research::routing

#endif  // ORTOOLS_ROUTING_INDEX_MANAGER_H_
