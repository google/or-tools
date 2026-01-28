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

"""Test for routing pybind11 layer."""

import functools

from absl.testing import absltest

from ortools.constraint_solver.python import constraint_solver
from ortools.routing import enums_pb2
from ortools.routing import parameters_pb2
from ortools.routing.python import model

FirstSolutionStrategy = enums_pb2.FirstSolutionStrategy
RoutingSearchStatus = enums_pb2.RoutingSearchStatus
RoutingSearchParameters = parameters_pb2.RoutingSearchParameters


def Distance(node_i, node_j):
    return node_i + node_j


def TransitDistance(manager, i, j):
    return Distance(manager.index_to_node(i), manager.index_to_node(j))


def UnaryTransitDistance(manager, i):
    return Distance(manager.index_to_node(i), 0)


def One(unused_i, unused_j):
    return 1


def Two(unused_i, unused_j):
    return 1


def Three(unused_i, unused_j):
    return 1


class TestRoutingIndexManager(absltest.TestCase):

    def testCtor(self):
        manager = model.IndexManager(42, 3, 7)
        self.assertIsNotNone(manager)
        print(manager)
        self.assertEqual(42, manager.num_nodes())
        self.assertEqual(3, manager.num_vehicles())
        self.assertEqual(42 + 3 * 2 - 1, manager.num_indices())
        for i in range(manager.num_vehicles()):
            self.assertEqual(7, manager.index_to_node(manager.get_start_index(i)))
            self.assertEqual(7, manager.index_to_node(manager.get_end_index(i)))

    def testCtorMultiDepotSame(self):
        manager = model.IndexManager(42, 3, [0, 0, 0], [0, 0, 0])
        self.assertIsNotNone(manager)
        print(manager)
        self.assertEqual(42, manager.num_nodes())
        self.assertEqual(3, manager.num_vehicles())
        self.assertEqual(42 + 3 * 2 - 1, manager.num_indices())
        for i in range(manager.num_vehicles()):
            self.assertEqual(0, manager.index_to_node(manager.get_start_index(i)))
            self.assertEqual(0, manager.index_to_node(manager.get_end_index(i)))

    def testCtorMultiDepotAllDiff(self):
        manager = model.IndexManager(42, 3, [1, 2, 3], [4, 5, 6])
        self.assertIsNotNone(manager)
        print(manager)
        self.assertEqual(42, manager.num_nodes())
        self.assertEqual(3, manager.num_vehicles())
        self.assertEqual(42, manager.num_indices())
        for i in range(manager.num_vehicles()):
            self.assertEqual(i + 1, manager.index_to_node(manager.get_start_index(i)))
            self.assertEqual(i + 4, manager.index_to_node(manager.get_end_index(i)))


class ModelTest(absltest.TestCase):

    def testCtor(self):
        manager = model.IndexManager(42, 3, 7)
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager)
        self.assertIsNotNone(routing_model)
        print(routing_model)
        for i in range(manager.num_vehicles()):
            self.assertEqual(7, manager.index_to_node(routing_model.start(i)))
            self.assertEqual(7, manager.index_to_node(routing_model.end(i)))

    def testSolve(self):
        manager = model.IndexManager(42, 3, 7)
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager)
        self.assertIsNotNone(routing_model)
        self.assertEqual(RoutingSearchStatus.ROUTING_NOT_SOLVED, routing_model.status())
        assignment = routing_model.solve()
        self.assertEqual(RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status())
        self.assertIsNotNone(assignment)
        self.assertEqual(0, assignment.objective_value())

    def testSolveMultiDepot(self):
        manager = model.IndexManager(42, 3, [1, 2, 3], [4, 5, 6])
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager)
        self.assertIsNotNone(routing_model)
        self.assertEqual(RoutingSearchStatus.ROUTING_NOT_SOLVED, routing_model.status())
        assignment = routing_model.solve()
        self.assertEqual(RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status())
        self.assertIsNotNone(assignment)
        self.assertEqual(0, assignment.objective_value())

    def testTransitCallback(self):
        manager = model.IndexManager(5, 1, 0)
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager)
        self.assertIsNotNone(routing_model)
        transit_idx = routing_model.register_transit_callback(
            functools.partial(TransitDistance, manager)
        )
        self.assertEqual(1, transit_idx)
        routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_idx)
        self.assertEqual(RoutingSearchStatus.ROUTING_NOT_SOLVED, routing_model.status())
        assignment = routing_model.solve()
        self.assertTrue(assignment)
        self.assertEqual(RoutingSearchStatus.ROUTING_SUCCESS, routing_model.status())
        self.assertEqual(20, assignment.objective_value())

    def testTransitLambda(self):
        manager = model.IndexManager(5, 1, 0)
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager)
        self.assertIsNotNone(routing_model)
        transit_id = routing_model.register_transit_callback(
            lambda from_index, to_index: 1
        )
        self.assertEqual(1, transit_id)
        routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_id)
        self.assertEqual(RoutingSearchStatus.ROUTING_NOT_SOLVED, routing_model.status())
        assignment = routing_model.solve()
        self.assertEqual(RoutingSearchStatus.ROUTING_SUCCESS, routing_model.status())
        self.assertIsNotNone(assignment)
        self.assertEqual(5, assignment.objective_value())

    def testTransitMatrix(self):
        manager = model.IndexManager(5, 1, 0)
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager)
        self.assertIsNotNone(routing_model)
        matrix = [[i + 1 for i in range(5)] for _ in range(5)]
        transit_idx = routing_model.register_transit_matrix(matrix)
        self.assertEqual(1, transit_idx)
        routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_idx)
        self.assertEqual(RoutingSearchStatus.ROUTING_NOT_SOLVED, routing_model.status())
        assignment = routing_model.solve()
        self.assertTrue(assignment)
        self.assertEqual(RoutingSearchStatus.ROUTING_SUCCESS, routing_model.status())
        self.assertEqual(15, assignment.objective_value())

    def testUnaryTransitCallback(self):
        manager = model.IndexManager(5, 1, 0)
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager)
        self.assertIsNotNone(routing_model)
        transit_idx = routing_model.register_unary_transit_callback(
            functools.partial(UnaryTransitDistance, manager)
        )
        self.assertEqual(1, transit_idx)
        routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_idx)
        self.assertEqual(RoutingSearchStatus.ROUTING_NOT_SOLVED, routing_model.status())
        assignment = routing_model.solve()
        self.assertTrue(assignment)
        self.assertEqual(RoutingSearchStatus.ROUTING_SUCCESS, routing_model.status())
        self.assertEqual(10, assignment.objective_value())

    def testUnaryTransitLambda(self):
        manager = model.IndexManager(5, 1, 0)
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager)
        self.assertIsNotNone(routing_model)
        transit_id = routing_model.register_unary_transit_callback(lambda from_index: 1)
        self.assertEqual(1, transit_id)
        routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_id)
        self.assertEqual(RoutingSearchStatus.ROUTING_NOT_SOLVED, routing_model.status())
        assignment = routing_model.solve()
        self.assertEqual(RoutingSearchStatus.ROUTING_SUCCESS, routing_model.status())
        self.assertIsNotNone(assignment)
        self.assertEqual(5, assignment.objective_value())

    def testUnaryTransitVector(self):
        manager = model.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager)
        self.assertIsNotNone(routing_model)
        vector = list(range(10))
        transit_idx = routing_model.register_unary_transit_vector(vector)
        self.assertEqual(1, transit_idx)
        routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_idx)
        self.assertEqual(RoutingSearchStatus.ROUTING_NOT_SOLVED, routing_model.status())
        assignment = routing_model.solve()
        self.assertTrue(assignment)
        self.assertEqual(RoutingSearchStatus.ROUTING_SUCCESS, routing_model.status())
        self.assertEqual(45, assignment.objective_value())

    def testTSP(self):
        # Create routing model
        manager = model.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager)
        self.assertIsNotNone(routing_model)
        # Add cost function
        transit_idx = routing_model.register_transit_callback(
            functools.partial(TransitDistance, manager)
        )
        routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_idx)
        self.assertEqual(RoutingSearchStatus.ROUTING_NOT_SOLVED, routing_model.status())
        # Solve
        search_parameters = model.default_routing_search_parameters()
        search_parameters.first_solution_strategy = (
            FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        assignment = routing_model.solve_with_parameters(search_parameters)
        self.assertEqual(RoutingSearchStatus.ROUTING_SUCCESS, routing_model.status())
        self.assertEqual(90, assignment.objective_value())
        # Inspect solution
        index = routing_model.start(0)
        visited_nodes = []
        expected_visited_nodes = [1, 2, 3, 4, 5, 6, 7, 8, 9, 0]
        while not routing_model.is_end(index):
            index = assignment.value(routing_model.next_var(index))
            visited_nodes.append(manager.index_to_node(index))
        self.assertEqual(expected_visited_nodes, visited_nodes)

    def testVRP(self):
        # Create routing model
        manager = model.IndexManager(10, 2, [0, 1], [1, 0])
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager)
        self.assertIsNotNone(routing_model)
        # Add cost function
        transit_idx = routing_model.register_transit_callback(
            functools.partial(TransitDistance, manager)
        )
        routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_idx)
        # Solve
        search_parameters = model.default_routing_search_parameters()
        search_parameters.first_solution_strategy = (
            FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        assignment = routing_model.solve_with_parameters(search_parameters)
        self.assertEqual(89, assignment.objective_value())
        # Inspect solution
        index = routing_model.start(1)
        visited_nodes = []
        expected_visited_nodes = [2, 4, 6, 8, 3, 5, 7, 9, 0]
        while not routing_model.is_end(index):
            index = assignment.value(routing_model.next_var(index))
            visited_nodes.append(manager.index_to_node(index))
        self.assertEqual(expected_visited_nodes, visited_nodes)
        self.assertTrue(
            routing_model.is_end(
                assignment.value(routing_model.next_var(routing_model.start(0)))
            )
        )

    def testDimensionTSP(self):
        # Create routing model
        manager = model.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager)
        self.assertIsNotNone(routing_model)
        # Add cost function
        transit_idx = routing_model.register_transit_callback(
            functools.partial(TransitDistance, manager)
        )
        routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_idx)
        # Add generic dimension
        routing_model.add_dimension(transit_idx, 90, 90, True, "distance")
        distance_dimension = routing_model.get_dimension_or_die("distance")
        # Solve
        search_parameters = model.default_routing_search_parameters()
        search_parameters.first_solution_strategy = (
            FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        assignment = routing_model.solve_with_parameters(search_parameters)
        self.assertEqual(90, assignment.objective_value())
        # Inspect solution
        node = routing_model.start(0)
        cumul = 0
        while not routing_model.is_end(node):
            self.assertEqual(
                cumul, assignment.value(distance_dimension.cumul_var(node))
            )
            next_node = assignment.value(routing_model.next_var(node))
            cumul += Distance(node, next_node)
            node = next_node

    def testDimensionWithVehicleCapacitiesTSP(self):
        # Create routing model
        manager = model.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager)
        self.assertIsNotNone(routing_model)
        # Add cost function
        transit_idx = routing_model.register_transit_callback(
            functools.partial(TransitDistance, manager)
        )
        routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_idx)
        # Add generic dimension
        routing_model.add_dimension_with_vehicle_capacity(
            transit_idx, 90, [90], True, "distance"
        )
        distance_dimension = routing_model.get_dimension_or_die("distance")
        # Solve
        search_parameters = model.default_routing_search_parameters()
        search_parameters.first_solution_strategy = (
            FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        assignment = routing_model.solve_with_parameters(search_parameters)
        self.assertEqual(90, assignment.objective_value())
        # Inspect solution
        node = routing_model.start(0)
        cumul = 0
        while not routing_model.is_end(node):
            self.assertEqual(
                cumul, assignment.value(distance_dimension.cumul_var(node))
            )
            next_node = assignment.value(routing_model.next_var(node))
            cumul += Distance(node, next_node)
            node = next_node

    def testDimensionWithVehicleTransitsTSP(self):
        # Create routing model
        manager = model.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager)
        self.assertIsNotNone(routing_model)
        # Add cost function
        transit_idx = routing_model.register_transit_callback(
            functools.partial(TransitDistance, manager)
        )
        routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_idx)
        # Add generic dimension
        routing_model.add_dimension_with_vehicle_transits(
            [transit_idx], 90, 90, True, "distance"
        )
        distance_dimension = routing_model.get_dimension_or_die("distance")
        # Solve
        search_parameters = model.default_routing_search_parameters()
        search_parameters.first_solution_strategy = (
            FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        assignment = routing_model.solve_with_parameters(search_parameters)
        self.assertEqual(90, assignment.objective_value())
        # Inspect solution
        node = routing_model.start(0)
        cumul = 0
        while not routing_model.is_end(node):
            self.assertEqual(
                cumul, assignment.value(distance_dimension.cumul_var(node))
            )
            next_node = assignment.value(routing_model.next_var(node))
            cumul += Distance(node, next_node)
            node = next_node

    def testDimensionWithVehicleTransitsVRP(self):
        # Create routing model
        manager = model.IndexManager(10, 3, 0)
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager)
        self.assertIsNotNone(routing_model)
        # Add cost function
        transit_idx = routing_model.register_transit_callback(
            functools.partial(TransitDistance, manager)
        )
        routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_idx)
        # Add generic dimension
        distances = [
            routing_model.register_transit_callback(One),
            routing_model.register_transit_callback(Two),
            routing_model.register_transit_callback(Three),
        ]
        routing_model.add_dimension_with_vehicle_transits(
            distances, 90, 90, True, "distance"
        )
        distance_dimension = routing_model.get_dimension_or_die("distance")
        # Solve
        search_parameters = model.default_routing_search_parameters()
        search_parameters.first_solution_strategy = (
            FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        assignment = routing_model.solve_with_parameters(search_parameters)
        self.assertEqual(90, assignment.objective_value())
        # Inspect solution
        for vehicle in range(0, routing_model.vehicles()):
            node = routing_model.start(vehicle)
            cumul = 0
            while not routing_model.is_end(node):
                self.assertEqual(
                    cumul, assignment.min(distance_dimension.cumul_var(node))
                )
                next_node = assignment.value(routing_model.next_var(node))
                # Increment cumul by the vehicle distance which is equal to the vehicle
                # index + 1, cf. distances.
                cumul += vehicle + 1
                node = next_node

    def testConstantDimensionTSP(self):
        # Create routing model
        manager = model.IndexManager(10, 3, 0)
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager)
        self.assertIsNotNone(routing_model)
        # Add cost function
        transit_idx = routing_model.register_transit_callback(
            functools.partial(TransitDistance, manager)
        )
        routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_idx)
        # Add constant dimension
        constant_id, success = routing_model.add_constant_dimension(
            1, 100, True, "count"
        )
        self.assertTrue(success)
        self.assertEqual(transit_idx + 1, constant_id)
        count_dimension = routing_model.get_dimension_or_die("count")
        # Solve
        search_parameters = model.default_routing_search_parameters()
        search_parameters.first_solution_strategy = (
            FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        assignment = routing_model.solve_with_parameters(search_parameters)
        self.assertEqual(90, assignment.objective_value())
        # Inspect solution
        node = routing_model.start(0)
        count = 0
        while not routing_model.is_end(node):
            self.assertEqual(count, assignment.value(count_dimension.cumul_var(node)))
            count += 1
            node = assignment.value(routing_model.next_var(node))
        self.assertEqual(10, count)

    def testVectorDimensionTSP(self):
        # Create routing model
        manager = model.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager)
        self.assertIsNotNone(routing_model)
        # Add cost function
        transit_idx = routing_model.register_transit_callback(
            functools.partial(TransitDistance, manager)
        )
        routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_idx)
        # Add vector dimension
        values = list(range(10))
        unary_transit_id, success = routing_model.add_vector_dimension(
            values, 100, True, "vector"
        )
        self.assertTrue(success)
        self.assertEqual(transit_idx + 1, unary_transit_id)
        vector_dimension = routing_model.get_dimension_or_die("vector")
        # Solve
        search_parameters: RoutingSearchParameters = (
            model.default_routing_search_parameters()
        )
        self.assertIsNotNone(search_parameters)
        search_parameters.first_solution_strategy = (
            FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        self.assertEqual(RoutingSearchStatus.ROUTING_NOT_SOLVED, routing_model.status())
        assignment = routing_model.solve_with_parameters(search_parameters)
        self.assertIsNotNone(assignment)
        self.assertEqual(RoutingSearchStatus.ROUTING_SUCCESS, routing_model.status())
        self.assertEqual(90, assignment.objective_value())
        # Inspect solution
        node = routing_model.start(0)
        cumul = 0
        while not routing_model.is_end(node):
            self.assertEqual(cumul, assignment.value(vector_dimension.cumul_var(node)))
            cumul += values[node]
            node = assignment.value(routing_model.next_var(node))

    def testMatrixDimensionTSP(self):
        # Create routing model
        manager = model.IndexManager(5, 1, 0)
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager)
        self.assertIsNotNone(routing_model)
        # Add cost function
        cost = routing_model.register_transit_callback(
            functools.partial(TransitDistance, manager)
        )
        routing_model.set_arc_cost_evaluator_of_all_vehicles(cost)
        # Add matrix dimension
        values = [[j for _ in range(5)] for j in range(5)]
        transit_id, success = routing_model.add_matrix_dimension(
            values, 100, True, "matrix"
        )
        self.assertTrue(success)
        self.assertEqual(cost + 1, transit_id)
        dimension = routing_model.get_dimension_or_die("matrix")
        # Solve
        search_parameters = model.default_routing_search_parameters()
        search_parameters.first_solution_strategy = (
            FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        self.assertEqual(RoutingSearchStatus.ROUTING_NOT_SOLVED, routing_model.status())
        assignment = routing_model.solve_with_parameters(search_parameters)
        self.assertIsNotNone(assignment)
        self.assertEqual(RoutingSearchStatus.ROUTING_SUCCESS, routing_model.status())
        self.assertEqual(20, assignment.objective_value())
        # Inspect solution
        index = routing_model.start(0)
        cumul = 0
        while not routing_model.is_end(index):
            self.assertEqual(cumul, assignment.value(dimension.cumul_var(index)))
            cumul += values[manager.index_to_node(index)][manager.index_to_node(index)]
            index = assignment.value(routing_model.next_var(index))

    def testMatrixDimensionVRP(self):
        manager = model.IndexManager(5, 2, 0)
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager)
        self.assertIsNotNone(routing_model)
        # Add cost function
        matrix = [[i + j for i in range(5)] for j in range(5)]
        transit_idx = routing_model.register_transit_matrix(matrix)
        routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_idx)
        # Add matrix dimension
        matrix_transit_idx, success = routing_model.add_matrix_dimension(
            matrix, 10, True, "matrix"  # capacity  # fix_start_cumul_to_zero
        )
        self.assertTrue(success)
        self.assertEqual(transit_idx + 1, matrix_transit_idx)
        dimension = routing_model.get_dimension_or_die("matrix")
        # Solve
        search_parameters = model.default_routing_search_parameters()
        search_parameters.first_solution_strategy = (
            FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        self.assertEqual(RoutingSearchStatus.ROUTING_NOT_SOLVED, routing_model.status())
        assignment = routing_model.solve_with_parameters(search_parameters)
        self.assertIsNotNone(assignment)
        self.assertEqual(RoutingSearchStatus.ROUTING_SUCCESS, routing_model.status())
        self.assertEqual(20, assignment.objective_value())
        # Inspect solution
        for v in range(manager.num_vehicles()):
            index = routing_model.start(v)
            cumul = 0
            while not routing_model.is_end(index):
                self.assertEqual(cumul, assignment.value(dimension.cumul_var(index)))
                prev_index = index
                index = assignment.value(routing_model.next_var(index))
                cumul += matrix[manager.index_to_node(prev_index)][
                    manager.index_to_node(index)
                ]

    def testDisjunctionTSP(self):
        # Create routing model
        manager = model.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager)
        self.assertIsNotNone(routing_model)
        # Add cost function
        transit_idx = routing_model.register_transit_callback(
            functools.partial(TransitDistance, manager)
        )
        routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_idx)
        # Add disjunctions
        disjunctions = [
            [manager.node_to_index(1), manager.node_to_index(2)],
            [manager.node_to_index(3)],
            [manager.node_to_index(4)],
            [manager.node_to_index(5)],
            [manager.node_to_index(6)],
            [manager.node_to_index(7)],
            [manager.node_to_index(8)],
            [manager.node_to_index(9)],
        ]
        for disjunction in disjunctions:
            routing_model.add_disjunction(disjunction)
        # Solve
        search_parameters = model.default_routing_search_parameters()
        search_parameters.first_solution_strategy = (
            FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        assignment = routing_model.solve_with_parameters(search_parameters)
        self.assertEqual(86, assignment.objective_value())
        # Inspect solution
        node = routing_model.start(0)
        count = 0
        while not routing_model.is_end(node):
            count += 1
            node = assignment.value(routing_model.next_var(node))
        self.assertEqual(9, count)

    def testDisjunctionPenaltyTSP(self):
        # Create routing model
        manager = model.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager)
        self.assertIsNotNone(routing_model)
        # Add cost function
        transit_idx = routing_model.register_transit_callback(
            functools.partial(TransitDistance, manager)
        )
        routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_idx)
        # Add disjunctions
        disjunctions = [
            ([manager.node_to_index(1), manager.node_to_index(2)], 1000),
            ([manager.node_to_index(3)], 1000),
            ([manager.node_to_index(4)], 1000),
            ([manager.node_to_index(5)], 1000),
            ([manager.node_to_index(6)], 1000),
            ([manager.node_to_index(7)], 1000),
            ([manager.node_to_index(8)], 1000),
            ([manager.node_to_index(9)], 0),
        ]
        for disjunction, penalty in disjunctions:
            routing_model.add_disjunction(disjunction, penalty)
        # Solve
        search_parameters = model.default_routing_search_parameters()
        search_parameters.first_solution_strategy = (
            FirstSolutionStrategy.FIRST_UNBOUND_MIN_VALUE
        )
        assignment = routing_model.solve_with_parameters(search_parameters)
        self.assertEqual(68, assignment.objective_value())
        # Inspect solution
        node = routing_model.start(0)
        count = 0
        while not routing_model.is_end(node):
            count += 1
            node = assignment.value(routing_model.next_var(node))
        self.assertEqual(8, count)

    def testRoutingModelParameters(self):
        # Create routing model with parameters
        parameters = model.default_routing_model_parameters()
        parameters.solver_parameters.CopyFrom(
            constraint_solver.Solver.default_solver_parameters()
        )
        parameters.solver_parameters.trace_propagation = True
        manager = model.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager, parameters)
        self.assertIsNotNone(routing_model)
        self.assertEqual(1, routing_model.vehicles())
        self.assertTrue(routing_model.solver.parameters.trace_propagation)

    def testRoutingLocalSearchFiltering(self):
        parameters = model.default_routing_model_parameters()
        parameters.solver_parameters.profile_local_search = True
        manager = model.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        routing_model = model.Model(manager, parameters)
        self.assertIsNotNone(routing_model)
        routing_model.solve()
        profile = routing_model.solver.local_search_profile()
        print(profile)
        self.assertIsInstance(profile, str)
        self.assertTrue(profile)  # Verify it's not empty.


if __name__ == "__main__":
    absltest.main()
