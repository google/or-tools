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

#include "ortools/routing/index_manager.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/routing/types.h"

namespace operations_research::routing {
namespace {

TEST(IndexManagerTest, SingleDepotManager) {
  const int kNodes = 10;
  const int kVehicles = 5;
  const NodeIndex kDepot(1);
  IndexManager manager(/*num_nodes*/ kNodes, /* num_vehicles*/ kVehicles,
                       /*depot*/ kDepot);
  EXPECT_EQ(kNodes, manager.num_nodes());
  EXPECT_EQ(kVehicles, manager.num_vehicles());
  EXPECT_EQ(kNodes + 2 * kVehicles - 1, manager.num_indices());
  EXPECT_EQ(1, manager.num_unique_depots());
  const std::vector<int64_t> expected_starts = {1, 10, 11, 12, 13};
  const std::vector<int64_t> expected_ends = {14, 15, 16, 17, 18};
  for (int i = 0; i < kVehicles; ++i) {
    EXPECT_EQ(expected_starts[i], manager.GetStartIndex(i));
    EXPECT_EQ(expected_ends[i], manager.GetEndIndex(i));
  }
  for (int i = 0; i < manager.num_indices(); ++i) {
    if (i >= kNodes) {  // start or end nodes
      EXPECT_EQ(kDepot, manager.IndexToNode(i));
    } else {
      EXPECT_EQ(NodeIndex(i), manager.IndexToNode(i));
    }
  }
  EXPECT_THAT(
      std::vector<NodeIndex>(
          {NodeIndex(0), NodeIndex(1), NodeIndex(2), NodeIndex(3), NodeIndex(4),
           NodeIndex(5), NodeIndex(6), NodeIndex(7), NodeIndex(8), NodeIndex(9),
           NodeIndex(1), NodeIndex(1), NodeIndex(1), NodeIndex(1), NodeIndex(1),
           NodeIndex(1), NodeIndex(1), NodeIndex(1), NodeIndex(1)}),
      ::testing::ContainerEq(manager.IndicesToNodes(
          std::vector<int64_t>({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
                                14, 15, 16, 17, 18}))));
  for (int i = 0; i < manager.num_nodes(); ++i) {
    EXPECT_EQ(i, manager.NodeToIndex(NodeIndex(i)));
  }
  EXPECT_THAT(
      std::vector<int64_t>({0, 1, 2, 3, 4, 5, 6, 7, 8, 9}),
      ::testing::ContainerEq(manager.NodesToIndices(std::vector<NodeIndex>(
          {NodeIndex(0), NodeIndex(1), NodeIndex(2), NodeIndex(3), NodeIndex(4),
           NodeIndex(5), NodeIndex(6), NodeIndex(7), NodeIndex(8),
           NodeIndex(9)}))));
}

TEST(IndexManagerTest, MultipleDepotManager) {
  const int kNodes = 10;
  const int kVehicles = 5;
  const std::vector<NodeIndex> starts = {
      NodeIndex(0), NodeIndex(3), NodeIndex(9), NodeIndex(2), NodeIndex(2)};
  const std::vector<NodeIndex> ends = {NodeIndex(0), NodeIndex(9), NodeIndex(3),
                                       NodeIndex(2), NodeIndex(1)};
  IndexManager manager(/*num_nodes*/ kNodes, /* num_vehicles*/ kVehicles,
                       starts, ends);
  EXPECT_EQ(kNodes, manager.num_nodes());
  EXPECT_EQ(kVehicles, manager.num_vehicles());
  EXPECT_EQ(kNodes + 2 * kVehicles - 5, manager.num_indices());
  EXPECT_EQ(5, manager.num_unique_depots());
  const std::vector<int64_t> expected_starts = {0, 2, 8, 1, 9};
  const std::vector<int64_t> expected_ends = {10, 11, 12, 13, 14};
  for (int i = 0; i < kVehicles; ++i) {
    EXPECT_EQ(expected_starts[i], manager.GetStartIndex(i));
    EXPECT_EQ(expected_ends[i], manager.GetEndIndex(i));
  }
  const std::vector<NodeIndex> expected_node_indices = {
      NodeIndex(0), NodeIndex(2), NodeIndex(3), NodeIndex(4), NodeIndex(5),
      NodeIndex(6), NodeIndex(7), NodeIndex(8), NodeIndex(9), NodeIndex(2),
      NodeIndex(0), NodeIndex(9), NodeIndex(3), NodeIndex(2), NodeIndex(1)};
  for (int i = 0; i < manager.num_indices(); ++i) {
    EXPECT_EQ(expected_node_indices[i], manager.IndexToNode(i));
  }
  EXPECT_THAT(
      expected_node_indices,
      ::testing::ContainerEq(manager.IndicesToNodes(std::vector<int64_t>(
          {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14}))));
  const std::vector<int64_t> expected_indices = {
      0, IndexManager::kUnassigned, 1, 2, 3, 4, 5, 6, 7, 8};
  for (int i = 0; i < manager.num_nodes(); ++i) {
    EXPECT_EQ(expected_indices[i], manager.NodeToIndex(NodeIndex(i)));
  }

  EXPECT_THAT(
      std::vector<int64_t>({0, 1, 2, 3, 4, 5, 6, 7, 8}),
      ::testing::ContainerEq(manager.NodesToIndices(std::vector<NodeIndex>(
          {NodeIndex(0), NodeIndex(2), NodeIndex(3), NodeIndex(4), NodeIndex(5),
           NodeIndex(6), NodeIndex(7), NodeIndex(8), NodeIndex(9)}))));
}

}  // namespace
}  // namespace operations_research::routing
