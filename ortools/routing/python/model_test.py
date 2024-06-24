#!/usr/bin/env python3
# Copyright 2010-2024 Google LLC
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

"""Test for routing pybind11 layer."""

from absl.testing import absltest
from ortools.routing import enums_pb2
from ortools.routing import parameters_pb2
from ortools.routing.python import model

FirstSolutionStrategy = enums_pb2.FirstSolutionStrategy
RoutingSearchStatus = enums_pb2.RoutingSearchStatus

def Distance(node_i, node_j):
    return node_i + node_j


def TransitDistance(manager, i, j):
    return Distance(manager.index_to_node(i), manager.index_to_node(j))


def UnaryTransitDistance(manager, i):
    return Distance(manager.index_to_node(i), 0)



class TestRoutingIndexManager(absltest.TestCase):

    def test_create_index_manager(self):
        print("test_create_index_manager")
        manager = model.RoutingIndexManager(42, 3, 7)
        self.assertIsNotNone(manager)
        print(manager)
        self.assertEqual(42, manager.num_nodes())
        self.assertEqual(3, manager.num_vehicles())
        self.assertEqual(42 + 3 * 2 - 1, manager.num_indices())
        for i in range(manager.num_vehicles()):
            self.assertEqual(7, manager.index_to_node(manager.get_start_index(i)))
            self.assertEqual(7, manager.index_to_node(manager.get_end_index(i)))


class ModelTest(absltest.TestCase):

    def test_create_model(self):
        print("test_create_model")
        manager = model.RoutingIndexManager(42, 3, 7)
        self.assertIsNotNone(manager)
        routing_model = model.RoutingModel(manager)
        self.assertIsNotNone(routing_model)
        print(routing_model)


if __name__ == "__main__":
    absltest.main()
