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

"""Test Routing API."""

import functools

from absl.testing import absltest
from ortools.constraint_solver import pywrapcp
from ortools.routing import enums_pb2
from ortools.routing import pywraprouting
from ortools.util import optional_boolean_pb2


def Distance(node_i, node_j):
    return node_i + node_j


def TransitDistance(manager, i, j):
    return Distance(manager.IndexToNode(i), manager.IndexToNode(j))


def UnaryTransitDistance(manager, i):
    return Distance(manager.IndexToNode(i), 0)


def One(unused_i, unused_j):
    return 1


def Two(unused_i, unused_j):
    return 1


def Three(unused_i, unused_j):
    return 1


class Callback:

    def __init__(self, model):
        self.model = model
        self.costs = []

    def __call__(self):
        self.costs.append(self.model.CostVar().Max())


class TestPyWrapRoutingIndexManager(absltest.TestCase):

    def testCtor(self):
        manager = pywraprouting.IndexManager(42, 3, 7)
        self.assertIsNotNone(manager)
        self.assertEqual(42, manager.GetNumberOfNodes())
        self.assertEqual(3, manager.GetNumberOfVehicles())
        self.assertEqual(42 + 3 * 2 - 1, manager.GetNumberOfIndices())
        for i in range(manager.GetNumberOfVehicles()):
            self.assertEqual(7, manager.IndexToNode(manager.GetStartIndex(i)))
            self.assertEqual(7, manager.IndexToNode(manager.GetEndIndex(i)))

    def testCtorMultiDepotSame(self):
        manager = pywraprouting.IndexManager(42, 3, [0, 0, 0], [0, 0, 0])
        self.assertIsNotNone(manager)
        self.assertEqual(42, manager.GetNumberOfNodes())
        self.assertEqual(3, manager.GetNumberOfVehicles())
        self.assertEqual(42 + 3 * 2 - 1, manager.GetNumberOfIndices())
        for i in range(manager.GetNumberOfVehicles()):
            self.assertEqual(0, manager.IndexToNode(manager.GetStartIndex(i)))
            self.assertEqual(0, manager.IndexToNode(manager.GetEndIndex(i)))

    def testCtorMultiDepotAllDiff(self):
        manager = pywraprouting.IndexManager(42, 3, [1, 2, 3], [4, 5, 6])
        self.assertIsNotNone(manager)
        self.assertEqual(42, manager.GetNumberOfNodes())
        self.assertEqual(3, manager.GetNumberOfVehicles())
        self.assertEqual(42, manager.GetNumberOfIndices())
        for i in range(manager.GetNumberOfVehicles()):
            self.assertEqual(i + 1, manager.IndexToNode(manager.GetStartIndex(i)))
            self.assertEqual(i + 4, manager.IndexToNode(manager.GetEndIndex(i)))


class TestPyWrapRoutingModel(absltest.TestCase):

    def testCtor(self):
        manager = pywraprouting.IndexManager(42, 3, 7)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        for i in range(manager.GetNumberOfVehicles()):
            self.assertEqual(7, manager.IndexToNode(model.Start(i)))
            self.assertEqual(7, manager.IndexToNode(model.End(i)))

    def testSolve(self):
        manager = pywraprouting.IndexManager(42, 3, 7)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        self.assertEqual(
            enums_pb2.RoutingSearchStatus.ROUTING_NOT_SOLVED, model.status()
        )
        assignment = model.Solve()
        self.assertEqual(enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, model.status())
        self.assertIsNotNone(assignment)
        self.assertEqual(0, assignment.ObjectiveValue())

    def testSolveMultiDepot(self):
        manager = pywraprouting.IndexManager(42, 3, [1, 2, 3], [4, 5, 6])
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        self.assertEqual(
            enums_pb2.RoutingSearchStatus.ROUTING_NOT_SOLVED, model.status()
        )
        assignment = model.Solve()
        self.assertEqual(enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, model.status())
        self.assertIsNotNone(assignment)
        self.assertEqual(0, assignment.ObjectiveValue())

    def testTransitCallback(self):
        manager = pywraprouting.IndexManager(5, 1, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        transit_idx = model.RegisterTransitCallback(
            functools.partial(TransitDistance, manager)
        )
        self.assertEqual(1, transit_idx)
        model.SetArcCostEvaluatorOfAllVehicles(transit_idx)
        self.assertEqual(
            enums_pb2.RoutingSearchStatus.ROUTING_NOT_SOLVED, model.status()
        )
        assignment = model.Solve()
        self.assertTrue(assignment)
        self.assertEqual(enums_pb2.RoutingSearchStatus.ROUTING_SUCCESS, model.status())
        self.assertEqual(20, assignment.ObjectiveValue())

    def testTransitLambda(self):
        manager = pywraprouting.IndexManager(5, 1, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        transit_id = model.RegisterTransitCallback(lambda from_index, to_index: 1)
        self.assertEqual(1, transit_id)
        model.SetArcCostEvaluatorOfAllVehicles(transit_id)
        self.assertEqual(
            enums_pb2.RoutingSearchStatus.ROUTING_NOT_SOLVED, model.status()
        )
        assignment = model.Solve()
        self.assertEqual(enums_pb2.RoutingSearchStatus.ROUTING_SUCCESS, model.status())
        self.assertIsNotNone(assignment)
        self.assertEqual(5, assignment.ObjectiveValue())

    def testTransitMatrix(self):
        manager = pywraprouting.IndexManager(5, 1, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        matrix = [[i + 1 for i in range(5)] for _ in range(5)]
        transit_idx = model.RegisterTransitMatrix(matrix)
        self.assertEqual(1, transit_idx)
        model.SetArcCostEvaluatorOfAllVehicles(transit_idx)
        self.assertEqual(
            enums_pb2.RoutingSearchStatus.ROUTING_NOT_SOLVED, model.status()
        )
        assignment = model.Solve()
        self.assertTrue(assignment)
        self.assertEqual(enums_pb2.RoutingSearchStatus.ROUTING_SUCCESS, model.status())
        self.assertEqual(15, assignment.ObjectiveValue())

    def testUnaryTransitCallback(self):
        manager = pywraprouting.IndexManager(5, 1, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        transit_idx = model.RegisterUnaryTransitCallback(
            functools.partial(UnaryTransitDistance, manager)
        )
        self.assertEqual(1, transit_idx)
        model.SetArcCostEvaluatorOfAllVehicles(transit_idx)
        self.assertEqual(
            enums_pb2.RoutingSearchStatus.ROUTING_NOT_SOLVED, model.status()
        )
        assignment = model.Solve()
        self.assertTrue(assignment)
        self.assertEqual(enums_pb2.RoutingSearchStatus.ROUTING_SUCCESS, model.status())
        self.assertEqual(10, assignment.ObjectiveValue())

    def testUnaryTransitLambda(self):
        manager = pywraprouting.IndexManager(5, 1, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        transit_id = model.RegisterUnaryTransitCallback(lambda from_index: 1)
        self.assertEqual(1, transit_id)
        model.SetArcCostEvaluatorOfAllVehicles(transit_id)
        self.assertEqual(
            enums_pb2.RoutingSearchStatus.ROUTING_NOT_SOLVED, model.status()
        )
        assignment = model.Solve()
        self.assertEqual(enums_pb2.RoutingSearchStatus.ROUTING_SUCCESS, model.status())
        self.assertIsNotNone(assignment)
        self.assertEqual(5, assignment.ObjectiveValue())

    def testUnaryTransitVector(self):
        manager = pywraprouting.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        vector = list(range(10))
        transit_idx = model.RegisterUnaryTransitVector(vector)
        self.assertEqual(1, transit_idx)
        model.SetArcCostEvaluatorOfAllVehicles(transit_idx)
        self.assertEqual(
            enums_pb2.RoutingSearchStatus.ROUTING_NOT_SOLVED, model.status()
        )
        assignment = model.Solve()
        self.assertTrue(assignment)
        self.assertEqual(enums_pb2.RoutingSearchStatus.ROUTING_SUCCESS, model.status())
        self.assertEqual(45, assignment.ObjectiveValue())

    def testTSP(self):
        # Create routing model
        manager = pywraprouting.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        # Add cost function
        transit_idx = model.RegisterTransitCallback(
            functools.partial(TransitDistance, manager)
        )
        model.SetArcCostEvaluatorOfAllVehicles(transit_idx)
        self.assertEqual(
            enums_pb2.RoutingSearchStatus.ROUTING_NOT_SOLVED, model.status()
        )
        # Solve
        search_parameters = pywraprouting.DefaultRoutingSearchParameters()
        search_parameters.first_solution_strategy = (
            enums_pb2.FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        assignment = model.SolveWithParameters(search_parameters)
        self.assertEqual(enums_pb2.RoutingSearchStatus.ROUTING_SUCCESS, model.status())
        self.assertEqual(90, assignment.ObjectiveValue())
        # Inspect solution
        index = model.Start(0)
        visited_nodes = []
        expected_visited_nodes = [1, 2, 3, 4, 5, 6, 7, 8, 9, 0]
        while not model.IsEnd(index):
            index = assignment.Value(model.NextVar(index))
            visited_nodes.append(manager.IndexToNode(index))
        self.assertEqual(expected_visited_nodes, visited_nodes)

    def testVRP(self):
        # Create routing model
        manager = pywraprouting.IndexManager(10, 2, [0, 1], [1, 0])
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        # Add cost function
        transit_idx = model.RegisterTransitCallback(
            functools.partial(TransitDistance, manager)
        )
        model.SetArcCostEvaluatorOfAllVehicles(transit_idx)
        # Solve
        search_parameters = pywraprouting.DefaultRoutingSearchParameters()
        search_parameters.first_solution_strategy = (
            enums_pb2.FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        assignment = model.SolveWithParameters(search_parameters)
        self.assertEqual(89, assignment.ObjectiveValue())
        # Inspect solution
        index = model.Start(1)
        visited_nodes = []
        expected_visited_nodes = [2, 4, 6, 8, 3, 5, 7, 9, 0]
        while not model.IsEnd(index):
            index = assignment.Value(model.NextVar(index))
            visited_nodes.append(manager.IndexToNode(index))
        self.assertEqual(expected_visited_nodes, visited_nodes)
        self.assertTrue(model.IsEnd(assignment.Value(model.NextVar(model.Start(0)))))

    def testDimensionTSP(self):
        # Create routing model
        manager = pywraprouting.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        # Add cost function
        transit_idx = model.RegisterTransitCallback(
            functools.partial(TransitDistance, manager)
        )
        model.SetArcCostEvaluatorOfAllVehicles(transit_idx)
        # Add generic dimension
        model.AddDimension(transit_idx, 90, 90, True, "distance")
        distance_dimension = model.GetDimensionOrDie("distance")
        # Solve
        search_parameters = pywraprouting.DefaultRoutingSearchParameters()
        search_parameters.first_solution_strategy = (
            enums_pb2.FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        assignment = model.SolveWithParameters(search_parameters)
        self.assertEqual(90, assignment.ObjectiveValue())
        # Inspect solution
        node = model.Start(0)
        cumul = 0
        while not model.IsEnd(node):
            self.assertEqual(cumul, assignment.Value(distance_dimension.CumulVar(node)))
            next_node = assignment.Value(model.NextVar(node))
            cumul += Distance(node, next_node)
            node = next_node

    def testDimensionWithVehicleCapacitiesTSP(self):
        # Create routing model
        manager = pywraprouting.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        # Add cost function
        transit_idx = model.RegisterTransitCallback(
            functools.partial(TransitDistance, manager)
        )
        model.SetArcCostEvaluatorOfAllVehicles(transit_idx)
        # Add generic dimension
        model.AddDimensionWithVehicleCapacity(transit_idx, 90, [90], True, "distance")
        distance_dimension = model.GetDimensionOrDie("distance")
        # Solve
        search_parameters = pywraprouting.DefaultRoutingSearchParameters()
        search_parameters.first_solution_strategy = (
            enums_pb2.FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        assignment = model.SolveWithParameters(search_parameters)
        self.assertEqual(90, assignment.ObjectiveValue())
        # Inspect solution
        node = model.Start(0)
        cumul = 0
        while not model.IsEnd(node):
            self.assertEqual(cumul, assignment.Value(distance_dimension.CumulVar(node)))
            next_node = assignment.Value(model.NextVar(node))
            cumul += Distance(node, next_node)
            node = next_node

    def testDimensionWithVehicleTransitsTSP(self):
        # Create routing model
        manager = pywraprouting.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        # Add cost function
        transit_idx = model.RegisterTransitCallback(
            functools.partial(TransitDistance, manager)
        )
        model.SetArcCostEvaluatorOfAllVehicles(transit_idx)
        # Add generic dimension
        model.AddDimensionWithVehicleTransits([transit_idx], 90, 90, True, "distance")
        distance_dimension = model.GetDimensionOrDie("distance")
        # Solve
        search_parameters = pywraprouting.DefaultRoutingSearchParameters()
        search_parameters.first_solution_strategy = (
            enums_pb2.FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        assignment = model.SolveWithParameters(search_parameters)
        self.assertEqual(90, assignment.ObjectiveValue())
        # Inspect solution
        node = model.Start(0)
        cumul = 0
        while not model.IsEnd(node):
            self.assertEqual(cumul, assignment.Value(distance_dimension.CumulVar(node)))
            next_node = assignment.Value(model.NextVar(node))
            cumul += Distance(node, next_node)
            node = next_node

    def testDimensionWithVehicleTransitsVRP(self):
        # Create routing model
        manager = pywraprouting.IndexManager(10, 3, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        # Add cost function
        transit_idx = model.RegisterTransitCallback(
            functools.partial(TransitDistance, manager)
        )
        model.SetArcCostEvaluatorOfAllVehicles(transit_idx)
        # Add generic dimension
        distances = [
            model.RegisterTransitCallback(One),
            model.RegisterTransitCallback(Two),
            model.RegisterTransitCallback(Three),
        ]
        model.AddDimensionWithVehicleTransits(distances, 90, 90, True, "distance")
        distance_dimension = model.GetDimensionOrDie("distance")
        # Solve
        search_parameters = pywraprouting.DefaultRoutingSearchParameters()
        search_parameters.first_solution_strategy = (
            enums_pb2.FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        assignment = model.SolveWithParameters(search_parameters)
        self.assertEqual(90, assignment.ObjectiveValue())
        # Inspect solution
        for vehicle in range(0, model.vehicles()):
            node = model.Start(vehicle)
            cumul = 0
            while not model.IsEnd(node):
                self.assertEqual(
                    cumul, assignment.Min(distance_dimension.CumulVar(node))
                )
                next_node = assignment.Value(model.NextVar(node))
                # Increment cumul by the vehicle distance which is equal to the vehicle
                # index + 1, cf. distances.
                cumul += vehicle + 1
                node = next_node

    def testConstantDimensionTSP(self):
        # Create routing model
        manager = pywraprouting.IndexManager(10, 3, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        # Add cost function
        transit_idx = model.RegisterTransitCallback(
            functools.partial(TransitDistance, manager)
        )
        model.SetArcCostEvaluatorOfAllVehicles(transit_idx)
        # Add constant dimension
        constant_id, success = model.AddConstantDimension(1, 100, True, "count")
        self.assertTrue(success)
        self.assertEqual(transit_idx + 1, constant_id)
        count_dimension = model.GetDimensionOrDie("count")
        # Solve
        search_parameters = pywraprouting.DefaultRoutingSearchParameters()
        search_parameters.first_solution_strategy = (
            enums_pb2.FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        assignment = model.SolveWithParameters(search_parameters)
        self.assertEqual(90, assignment.ObjectiveValue())
        # Inspect solution
        node = model.Start(0)
        count = 0
        while not model.IsEnd(node):
            self.assertEqual(count, assignment.Value(count_dimension.CumulVar(node)))
            count += 1
            node = assignment.Value(model.NextVar(node))
        self.assertEqual(10, count)

    def testVectorDimensionTSP(self):
        # Create routing model
        manager = pywraprouting.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        # Add cost function
        transit_idx = model.RegisterTransitCallback(
            functools.partial(TransitDistance, manager)
        )
        model.SetArcCostEvaluatorOfAllVehicles(transit_idx)
        # Add vector dimension
        values = list(range(10))
        unary_transit_id, success = model.AddVectorDimension(
            values, 100, True, "vector"
        )
        self.assertTrue(success)
        self.assertEqual(transit_idx + 1, unary_transit_id)
        vector_dimension = model.GetDimensionOrDie("vector")
        # Solve
        search_parameters = pywraprouting.DefaultRoutingSearchParameters()
        search_parameters.first_solution_strategy = (
            enums_pb2.FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        self.assertEqual(
            enums_pb2.RoutingSearchStatus.ROUTING_NOT_SOLVED, model.status()
        )
        assignment = model.SolveWithParameters(search_parameters)
        self.assertIsNotNone(assignment)
        self.assertEqual(enums_pb2.RoutingSearchStatus.ROUTING_SUCCESS, model.status())
        self.assertEqual(90, assignment.ObjectiveValue())
        # Inspect solution
        node = model.Start(0)
        cumul = 0
        while not model.IsEnd(node):
            self.assertEqual(cumul, assignment.Value(vector_dimension.CumulVar(node)))
            cumul += values[node]
            node = assignment.Value(model.NextVar(node))

    def testMatrixDimensionTSP(self):
        # Create routing model
        manager = pywraprouting.IndexManager(5, 1, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        # Add cost function
        cost = model.RegisterTransitCallback(
            functools.partial(TransitDistance, manager)
        )
        model.SetArcCostEvaluatorOfAllVehicles(cost)
        # Add matrix dimension
        values = [[j for _ in range(5)] for j in range(5)]
        transit_id, success = model.AddMatrixDimension(values, 100, True, "matrix")
        self.assertTrue(success)
        self.assertEqual(cost + 1, transit_id)
        dimension = model.GetDimensionOrDie("matrix")
        # Solve
        search_parameters = pywraprouting.DefaultRoutingSearchParameters()
        search_parameters.first_solution_strategy = (
            enums_pb2.FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        self.assertEqual(
            enums_pb2.RoutingSearchStatus.ROUTING_NOT_SOLVED, model.status()
        )
        assignment = model.SolveWithParameters(search_parameters)
        self.assertIsNotNone(assignment)
        self.assertEqual(enums_pb2.RoutingSearchStatus.ROUTING_SUCCESS, model.status())
        self.assertEqual(20, assignment.ObjectiveValue())
        # Inspect solution
        index = model.Start(0)
        cumul = 0
        while not model.IsEnd(index):
            self.assertEqual(cumul, assignment.Value(dimension.CumulVar(index)))
            cumul += values[manager.IndexToNode(index)][manager.IndexToNode(index)]
            index = assignment.Value(model.NextVar(index))

    def testMatrixDimensionVRP(self):
        manager = pywraprouting.IndexManager(5, 2, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        # Add cost function
        matrix = [[i + j for i in range(5)] for j in range(5)]
        transit_idx = model.RegisterTransitMatrix(matrix)
        model.SetArcCostEvaluatorOfAllVehicles(transit_idx)
        # Add matrix dimension
        matrix_transit_idx, success = model.AddMatrixDimension(
            matrix, 10, True, "matrix"  # capacity  # fix_start_cumul_to_zero
        )
        self.assertTrue(success)
        self.assertEqual(transit_idx + 1, matrix_transit_idx)
        dimension = model.GetDimensionOrDie("matrix")
        # Solve
        search_parameters = pywraprouting.DefaultRoutingSearchParameters()
        search_parameters.first_solution_strategy = (
            enums_pb2.FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        self.assertEqual(
            enums_pb2.RoutingSearchStatus.ROUTING_NOT_SOLVED, model.status()
        )
        assignment = model.SolveWithParameters(search_parameters)
        self.assertIsNotNone(assignment)
        self.assertEqual(enums_pb2.RoutingSearchStatus.ROUTING_SUCCESS, model.status())
        self.assertEqual(20, assignment.ObjectiveValue())
        # Inspect solution
        for v in range(manager.GetNumberOfVehicles()):
            index = model.Start(v)
            cumul = 0
            while not model.IsEnd(index):
                self.assertEqual(cumul, assignment.Value(dimension.CumulVar(index)))
                prev_index = index
                index = assignment.Value(model.NextVar(index))
                cumul += matrix[manager.IndexToNode(prev_index)][
                    manager.IndexToNode(index)
                ]

    def testAllowedVehicles(self):
        manager = pywraprouting.IndexManager(
            2, 2, 0  # num_nodes  # num_vehicles  # depot
        )
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        # out of range vehicle index is allowed.
        model.SetAllowedVehiclesForIndex(vehicles=[13], index=1)
        self.assertFalse(model.IsVehicleAllowedForIndex(vehicle=0, index=1))
        self.assertFalse(model.IsVehicleAllowedForIndex(1, 1))
        self.assertTrue(model.IsVehicleAllowedForIndex(13, 1))
        # empty list means any vehicles are allowed.
        model.SetAllowedVehiclesForIndex([], 1)
        self.assertTrue(model.IsVehicleAllowedForIndex(0, 1))
        self.assertTrue(model.IsVehicleAllowedForIndex(1, 1))
        self.assertTrue(model.IsVehicleAllowedForIndex(42, 1))
        # only allow first vehicle for node 1.
        model.SetAllowedVehiclesForIndex([0], 1)
        self.assertTrue(model.IsVehicleAllowedForIndex(0, 1))
        self.assertFalse(model.IsVehicleAllowedForIndex(1, 1))
        self.assertFalse(model.IsVehicleAllowedForIndex(2, 1))
        assignment = model.Solve()
        self.assertIsNotNone(assignment)
        self.assertEqual(1, assignment.Value(model.NextVar(model.Start(0))))
        self.assertEqual(model.GetVehicleClassesCount(), 2)

    def testDisjunctionTSP(self):
        # Create routing model
        manager = pywraprouting.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        # Add cost function
        transit_idx = model.RegisterTransitCallback(
            functools.partial(TransitDistance, manager)
        )
        model.SetArcCostEvaluatorOfAllVehicles(transit_idx)
        # Add disjunctions
        disjunctions = [
            [manager.NodeToIndex(1), manager.NodeToIndex(2)],
            [manager.NodeToIndex(3)],
            [manager.NodeToIndex(4)],
            [manager.NodeToIndex(5)],
            [manager.NodeToIndex(6)],
            [manager.NodeToIndex(7)],
            [manager.NodeToIndex(8)],
            [manager.NodeToIndex(9)],
        ]
        for disjunction in disjunctions:
            model.AddDisjunction(disjunction)
        # Solve
        search_parameters = pywraprouting.DefaultRoutingSearchParameters()
        search_parameters.first_solution_strategy = (
            enums_pb2.FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        assignment = model.SolveWithParameters(search_parameters)
        self.assertEqual(86, assignment.ObjectiveValue())
        # Inspect solution
        node = model.Start(0)
        count = 0
        while not model.IsEnd(node):
            count += 1
            node = assignment.Value(model.NextVar(node))
        self.assertEqual(9, count)

    def testDisjunctionPenaltyTSP(self):
        # Create routing model
        manager = pywraprouting.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        # Add cost function
        transit_idx = model.RegisterTransitCallback(
            functools.partial(TransitDistance, manager)
        )
        model.SetArcCostEvaluatorOfAllVehicles(transit_idx)
        # Add disjunctions
        disjunctions = [
            ([manager.NodeToIndex(1), manager.NodeToIndex(2)], 1000),
            ([manager.NodeToIndex(3)], 1000),
            ([manager.NodeToIndex(4)], 1000),
            ([manager.NodeToIndex(5)], 1000),
            ([manager.NodeToIndex(6)], 1000),
            ([manager.NodeToIndex(7)], 1000),
            ([manager.NodeToIndex(8)], 1000),
            ([manager.NodeToIndex(9)], 0),
        ]
        for disjunction, penalty in disjunctions:
            model.AddDisjunction(disjunction, penalty)
        # Solve
        search_parameters = pywraprouting.DefaultRoutingSearchParameters()
        search_parameters.first_solution_strategy = (
            enums_pb2.FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        assignment = model.SolveWithParameters(search_parameters)
        self.assertEqual(68, assignment.ObjectiveValue())
        # Inspect solution
        node = model.Start(0)
        count = 0
        while not model.IsEnd(node):
            count += 1
            node = assignment.Value(model.NextVar(node))
        self.assertEqual(8, count)

    def testRoutingModelParameters(self):
        # Create routing model with parameters
        parameters = pywraprouting.DefaultRoutingModelParameters()
        parameters.solver_parameters.CopyFrom(pywrapcp.Solver.DefaultSolverParameters())
        parameters.solver_parameters.trace_propagation = True
        manager = pywraprouting.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager, parameters)
        self.assertIsNotNone(model)
        self.assertEqual(1, model.vehicles())
        self.assertTrue(model.solver().Parameters().trace_propagation)

    def testRoutingLocalSearchFiltering(self):
        parameters = pywraprouting.DefaultRoutingModelParameters()
        parameters.solver_parameters.profile_local_search = True
        manager = pywraprouting.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager, parameters)
        self.assertIsNotNone(model)
        model.Solve()
        profile = model.solver().LocalSearchProfile()
        print(profile)
        self.assertIsInstance(profile, str)
        self.assertTrue(profile)  # Verify it's not empty.

    def testRoutingSearchParameters(self):
        # Create routing model
        manager = pywraprouting.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        # Add cost function
        transit_idx = model.RegisterTransitCallback(
            functools.partial(TransitDistance, manager)
        )
        model.SetArcCostEvaluatorOfAllVehicles(transit_idx)
        # Close with parameters
        search_parameters = pywraprouting.DefaultRoutingSearchParameters()
        search_parameters.first_solution_strategy = (
            enums_pb2.FirstSolutionStrategy.SAVINGS
        )
        search_parameters.local_search_metaheuristic = (
            enums_pb2.LocalSearchMetaheuristic.GUIDED_LOCAL_SEARCH
        )
        search_parameters.local_search_operators.use_two_opt = (
            optional_boolean_pb2.BOOL_FALSE
        )
        search_parameters.solution_limit = 20
        model.CloseModelWithParameters(search_parameters)
        # Solve with parameters
        assignment = model.SolveWithParameters(search_parameters)
        self.assertEqual(
            11, model.GetNumberOfDecisionsInFirstSolution(search_parameters)
        )
        self.assertEqual(0, model.GetNumberOfRejectsInFirstSolution(search_parameters))
        self.assertEqual(90, assignment.ObjectiveValue())
        assignment = model.SolveFromAssignmentWithParameters(
            assignment, search_parameters
        )
        self.assertEqual(90, assignment.ObjectiveValue())

    def testFindErrorInRoutingSearchParameters(self):
        params = pywraprouting.DefaultRoutingSearchParameters()
        params.local_search_operators.use_cross = optional_boolean_pb2.BOOL_UNSPECIFIED
        self.assertIn("cross", pywraprouting.FindErrorInRoutingSearchParameters(params))

    def testCallback(self):
        manager = pywraprouting.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        transit_idx = model.RegisterTransitCallback(
            functools.partial(TransitDistance, manager)
        )
        model.SetArcCostEvaluatorOfAllVehicles(transit_idx)
        callback = Callback(model)
        model.AddAtSolutionCallback(callback)
        search_parameters = pywraprouting.DefaultRoutingSearchParameters()
        search_parameters.first_solution_strategy = (
            enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC
        )
        assignment = model.SolveWithParameters(search_parameters)
        self.assertEqual(90, assignment.ObjectiveValue())
        self.assertEqual(len(callback.costs), 1)
        self.assertEqual(90, callback.costs[0])

    def testReadAssignment(self):
        manager = pywraprouting.IndexManager(10, 2, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        # TODO(user): porting this segfaults the tests.
        transit_idx = model.RegisterTransitCallback(
            functools.partial(TransitDistance, manager)
        )
        model.SetArcCostEvaluatorOfAllVehicles(transit_idx)
        routes = [
            [
                manager.NodeToIndex(1),
                manager.NodeToIndex(3),
                manager.NodeToIndex(5),
                manager.NodeToIndex(4),
                manager.NodeToIndex(2),
                manager.NodeToIndex(6),
            ],
            [
                manager.NodeToIndex(7),
                manager.NodeToIndex(9),
                manager.NodeToIndex(8),
            ],
        ]
        assignment = model.ReadAssignmentFromRoutes(routes, False)
        search_parameters = pywraprouting.DefaultRoutingSearchParameters()
        search_parameters.solution_limit = 1
        solution = model.SolveFromAssignmentWithParameters(
            assignment, search_parameters
        )
        self.assertEqual(90, solution.ObjectiveValue())
        for vehicle in range(0, model.vehicles()):
            node = model.Start(vehicle)
            count = 0
            while not model.IsEnd(node):
                node = solution.Value(model.NextVar(node))
                if not model.IsEnd(node):
                    self.assertEqual(routes[vehicle][count], manager.IndexToNode(node))
                    count += 1

    def testAutomaticFirstSolutionStrategy_simple(self):
        manager = pywraprouting.IndexManager(31, 7, 3)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        # Add cost function
        transit_idx = model.RegisterTransitCallback(
            functools.partial(TransitDistance, manager)
        )
        model.SetArcCostEvaluatorOfAllVehicles(transit_idx)
        # Solve
        search_parameters = pywraprouting.DefaultRoutingSearchParameters()
        self.assertIsNotNone(model.SolveWithParameters(search_parameters))
        self.assertEqual(
            enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC,
            model.GetAutomaticFirstSolutionStrategy(),
        )

    def testAutomaticFirstSolutionStrategy_pd(self):
        manager = pywraprouting.IndexManager(31, 7, 0)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        # Add cost function
        transit_idx = model.RegisterTransitCallback(
            functools.partial(TransitDistance, manager)
        )
        model.SetArcCostEvaluatorOfAllVehicles(transit_idx)
        self.assertTrue(model.AddDimension(transit_idx, 0, 1000, True, "distance"))
        dst_dimension = model.GetDimensionOrDie("distance")
        # Add few Pickup and Delivery
        for request in [[2 * i, 2 * i + 1] for i in range(1, 15)]:
            pickup_index = manager.NodeToIndex(request[0])
            delivery_index = manager.NodeToIndex(request[1])
            model.AddPickupAndDelivery(pickup_index, delivery_index)
            model.solver().Add(
                model.VehicleVar(pickup_index) == model.VehicleVar(delivery_index)
            )
            model.solver().Add(
                dst_dimension.CumulVar(pickup_index)
                <= dst_dimension.CumulVar(delivery_index)
            )
        # Solve
        search_parameters = pywraprouting.DefaultRoutingSearchParameters()
        self.assertIsNotNone(model.SolveWithParameters(search_parameters))
        self.assertEqual(
            enums_pb2.FirstSolutionStrategy.PARALLEL_CHEAPEST_INSERTION,
            model.GetAutomaticFirstSolutionStrategy(),
        )


class TestBoundCost(absltest.TestCase):

    def testCtor(self):
        bound_cost = pywraprouting.BoundCost()
        self.assertIsNotNone(bound_cost)
        self.assertEqual(0, bound_cost.bound)
        self.assertEqual(0, bound_cost.cost)

        bound_cost = pywraprouting.BoundCost(97, 43)
        self.assertIsNotNone(bound_cost)
        self.assertEqual(97, bound_cost.bound)
        self.assertEqual(43, bound_cost.cost)


class TestRoutingDimension(absltest.TestCase):

    def testCtor(self):
        manager = pywraprouting.IndexManager(31, 7, 3)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        transit_idx = model.RegisterTransitCallback(
            functools.partial(TransitDistance, manager)
        )
        self.assertTrue(model.AddDimension(transit_idx, 90, 90, True, "distance"))
        model.GetDimensionOrDie("distance")

    def testSoftSpanUpperBound(self):
        manager = pywraprouting.IndexManager(31, 7, 3)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        transit_idx = model.RegisterTransitCallback(
            functools.partial(TransitDistance, manager)
        )
        self.assertTrue(model.AddDimension(transit_idx, 100, 100, True, "distance"))
        dimension = model.GetDimensionOrDie("distance")

        bound_cost = pywraprouting.BoundCost(97, 43)
        self.assertIsNotNone(bound_cost)
        self.assertFalse(dimension.HasSoftSpanUpperBounds())
        for v in range(manager.GetNumberOfVehicles()):
            dimension.SetSoftSpanUpperBoundForVehicle(bound_cost, v)
            bc = dimension.GetSoftSpanUpperBoundForVehicle(v)
            self.assertIsNotNone(bc)
            self.assertEqual(97, bc.bound)
            self.assertEqual(43, bc.cost)
        self.assertTrue(dimension.HasSoftSpanUpperBounds())

    def testQuadraticCostSoftSpanUpperBound(self):
        manager = pywraprouting.IndexManager(31, 7, 3)
        self.assertIsNotNone(manager)
        model = pywraprouting.Model(manager)
        self.assertIsNotNone(model)
        transit_idx = model.RegisterTransitCallback(
            functools.partial(TransitDistance, manager)
        )
        self.assertTrue(model.AddDimension(transit_idx, 100, 100, True, "distance"))
        dimension = model.GetDimensionOrDie("distance")

        bound_cost = pywraprouting.BoundCost(97, 43)
        self.assertIsNotNone(bound_cost)
        self.assertFalse(dimension.HasQuadraticCostSoftSpanUpperBounds())
        for v in range(manager.GetNumberOfVehicles()):
            dimension.SetQuadraticCostSoftSpanUpperBoundForVehicle(bound_cost, v)
            bc = dimension.GetQuadraticCostSoftSpanUpperBoundForVehicle(v)
            self.assertIsNotNone(bc)
            self.assertEqual(97, bc.bound)
            self.assertEqual(43, bc.cost)
        self.assertTrue(dimension.HasQuadraticCostSoftSpanUpperBounds())


# TODO(user): Add tests for Routing[Cost|Vehicle|Resource]ClassIndex

if __name__ == "__main__":
    absltest.main()
