#!/usr/bin/env python3
# Copyright 2010-2025 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from absl.testing import absltest
from ortools.routing.python import routing


class IndexManagerTest(absltest.TestCase):

    def test_single_depot_manager(self):
        num_nodes = 10
        num_vehicles = 5
        depot_index = 1
        manager = routing.IndexManager(num_nodes, num_vehicles, depot_index)
        self.assertEqual(num_nodes, manager.num_nodes())
        self.assertEqual(num_vehicles, manager.num_vehicles())
        self.assertEqual(num_nodes + 2 * num_vehicles - 1, manager.num_indices())
        self.assertEqual(1, manager.num_unique_depots())

        expected_starts = [1, 10, 11, 12, 13]
        expected_ends = [14, 15, 16, 17, 18]
        for i in range(num_vehicles):
            self.assertEqual(expected_starts[i], manager.get_start_index(i))
            self.assertEqual(expected_ends[i], manager.get_end_index(i))

        for i in range(manager.num_indices()):
            if i >= num_nodes:  # start or end nodes
                self.assertEqual(depot_index, manager.index_to_node(i))
            else:
                self.assertEqual(i, manager.index_to_node(i))

        expected_nodes = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 1, 1, 1, 1, 1, 1, 1, 1]
        input_indices = [
            0,
            1,
            2,
            3,
            4,
            5,
            6,
            7,
            8,
            9,
            10,
            11,
            12,
            13,
            14,
            15,
            16,
            17,
            18,
        ]
        self.assertEqual(expected_nodes, manager.indices_to_nodes(input_indices))

        for i in range(manager.num_nodes()):
            self.assertEqual(i, manager.node_to_index(i))

        expected_indices_from_nodes = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
        input_nodes = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
        self.assertEqual(
            expected_indices_from_nodes, manager.nodes_to_indices(input_nodes)
        )

    def test_multiple_depot_manager(self):
        num_nodes = 10
        num_vehicles = 5
        starts = [0, 3, 9, 2, 2]
        ends = [0, 9, 3, 2, 1]
        manager = routing.IndexManager(num_nodes, num_vehicles, starts, ends)
        self.assertEqual(num_nodes, manager.num_nodes())
        self.assertEqual(num_vehicles, manager.num_vehicles())
        self.assertEqual(num_nodes + 2 * num_vehicles - 5, manager.num_indices())
        self.assertEqual(5, manager.num_unique_depots())

        expected_starts = [0, 2, 8, 1, 9]
        expected_ends = [10, 11, 12, 13, 14]
        for i in range(num_vehicles):
            self.assertEqual(expected_starts[i], manager.get_start_index(i))
            self.assertEqual(expected_ends[i], manager.get_end_index(i))

        expected_node_indices = [0, 2, 3, 4, 5, 6, 7, 8, 9, 2, 0, 9, 3, 2, 1]
        for i in range(manager.num_indices()):
            self.assertEqual(expected_node_indices[i], manager.index_to_node(i))

        self.assertEqual(
            expected_node_indices, manager.indices_to_nodes(list(range(15)))
        )

        unassigned = routing.IndexManager.k_unassigned
        expected_indices = [0, unassigned, 1, 2, 3, 4, 5, 6, 7, 8]
        for i in range(manager.num_nodes()):
            self.assertEqual(expected_indices[i], manager.node_to_index(i))

        expected_indices_from_nodes = [0, 1, 2, 3, 4, 5, 6, 7, 8]
        input_nodes = [0, 2, 3, 4, 5, 6, 7, 8, 9]
        self.assertEqual(
            expected_indices_from_nodes, manager.nodes_to_indices(input_nodes)
        )


if __name__ == "__main__":
    absltest.main()
