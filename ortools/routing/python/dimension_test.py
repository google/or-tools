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

"""Tests for routing dimension."""

import functools

from absl.testing import absltest

from ortools.routing import enums_pb2
from ortools.routing.python import routing

FirstSolutionStrategy = enums_pb2.FirstSolutionStrategy
RoutingSearchStatus = enums_pb2.RoutingSearchStatus


def distance(node_i: int, node_j: int) -> int:
    return node_i + node_j


def transit_distance(manager: routing.IndexManager, i: int, j: int) -> int:
    return distance(manager.index_to_node(i), manager.index_to_node(j))


def one(unused_i: int, unused_j: int) -> int:
    return 1


def two(unused_i: int, unused_j: int) -> int:
    return 1


def three(unused_i: int, unused_j: int) -> int:
    return 1


class DimensionTest(absltest.TestCase):

    def test_dimension_tsp(self) -> None:
        # Create routing model
        manager = routing.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        routing_model = routing.Model(manager)
        self.assertIsNotNone(routing_model)
        # Add cost function
        transit_idx = routing_model.register_transit_callback(
            functools.partial(transit_distance, manager)
        )
        routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_idx)
        # Add generic dimension
        routing_model.add_dimension(transit_idx, 90, 90, True, "distance")
        distance_dimension = routing_model.get_dimension_or_die("distance")
        # Solve
        search_parameters = routing.default_routing_search_parameters()
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
            cumul += distance(node, next_node)
            node = next_node

    def test_dimension_with_vehicle_capacities_tsp(self) -> None:
        # Create routing model
        manager = routing.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        routing_model = routing.Model(manager)
        self.assertIsNotNone(routing_model)
        # Add cost function
        transit_idx = routing_model.register_transit_callback(
            functools.partial(transit_distance, manager)
        )
        routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_idx)
        # Add generic dimension
        routing_model.add_dimension_with_vehicle_capacity(
            transit_idx, 90, [90], True, "distance"
        )
        distance_dimension = routing_model.get_dimension_or_die("distance")
        # Solve
        search_parameters = routing.default_routing_search_parameters()
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
            cumul += distance(node, next_node)
            node = next_node

    def test_dimension_with_vehicle_transits_tsp(self) -> None:
        # Create routing model
        manager = routing.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        routing_model = routing.Model(manager)
        self.assertIsNotNone(routing_model)
        # Add cost function
        transit_idx = routing_model.register_transit_callback(
            functools.partial(transit_distance, manager)
        )
        routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_idx)
        # Add generic dimension
        routing_model.add_dimension_with_vehicle_transits(
            [transit_idx], 90, 90, True, "distance"
        )
        distance_dimension = routing_model.get_dimension_or_die("distance")
        # Solve
        search_parameters = routing.default_routing_search_parameters()
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
            cumul += distance(node, next_node)
            node = next_node

    def test_dimension_with_vehicle_transits_vrp(self) -> None:
        # Create routing model
        manager = routing.IndexManager(10, 3, 0)
        self.assertIsNotNone(manager)
        routing_model = routing.Model(manager)
        self.assertIsNotNone(routing_model)
        # Add cost function
        transit_idx = routing_model.register_transit_callback(
            functools.partial(transit_distance, manager)
        )
        routing_model.set_arc_cost_evaluator_of_all_vehicles(transit_idx)
        # Add generic dimension
        distances = [
            routing_model.register_transit_callback(one),
            routing_model.register_transit_callback(two),
            routing_model.register_transit_callback(three),
        ]
        routing_model.add_dimension_with_vehicle_transits(
            distances, 90, 90, True, "distance"
        )
        distance_dimension = routing_model.get_dimension_or_die("distance")
        # Solve
        search_parameters = routing.default_routing_search_parameters()
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

    def test_constant_dimension_tsp(self) -> None:
        # Create routing model
        manager = routing.IndexManager(10, 3, 0)
        self.assertIsNotNone(manager)
        routing_model = routing.Model(manager)
        self.assertIsNotNone(routing_model)
        # Add cost function
        transit_idx = routing_model.register_transit_callback(
            functools.partial(transit_distance, manager)
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
        search_parameters = routing.default_routing_search_parameters()
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

    def test_vector_dimension_tsp(self) -> None:
        # Create routing model
        manager = routing.IndexManager(10, 1, 0)
        self.assertIsNotNone(manager)
        routing_model = routing.Model(manager)
        self.assertIsNotNone(routing_model)
        # Add cost function
        transit_idx = routing_model.register_transit_callback(
            functools.partial(transit_distance, manager)
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
        search_parameters = routing.default_routing_search_parameters()
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

    def test_matrix_dimension_tsp(self) -> None:
        # Create routing model
        manager = routing.IndexManager(5, 1, 0)
        self.assertIsNotNone(manager)
        routing_model = routing.Model(manager)
        self.assertIsNotNone(routing_model)
        # Add cost function
        cost = routing_model.register_transit_callback(
            functools.partial(transit_distance, manager)
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
        search_parameters = routing.default_routing_search_parameters()
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

    def test_matrix_dimension_vrp(self) -> None:
        manager = routing.IndexManager(5, 2, 0)
        self.assertIsNotNone(manager)
        routing_model = routing.Model(manager)
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
        search_parameters = routing.default_routing_search_parameters()
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

    def test_dimension_methods(self) -> None:
        manager = routing.IndexManager(num_nodes=10, num_vehicles=1, depot=0)
        routing_model = routing.Model(manager)
        transit_idx = routing_model.register_transit_callback(lambda i, j: 1)
        routing_model.add_dimension(transit_idx, 100, 100, True, "Dist")
        dim = routing_model.get_dimension_or_die("Dist")
        dim.set_span_cost_coefficient_for_vehicle(10, 0)
        dim.set_global_span_cost_coefficient(5)
        dim.set_slack_cost_coefficient_for_vehicle(2, 0)
        node = manager.node_to_index(1)
        dim.set_cumul_var_soft_upper_bound(node, 10, 100)
        dim.set_cumul_var_soft_lower_bound(node, 0, 100)
        self.assertEqual(1, dim.get_transit_value(0, 1, 0))
        self.assertIsNotNone(dim.cumul_var(0))
        self.assertIsNotNone(dim.transit_var(0))
        self.assertIsNotNone(dim.slack_var(0))

        # Test new methods
        dim.set_cumul_var_range(node, 0, 100)
        self.assertEqual(0, dim.get_cumul_var_min(node))
        self.assertEqual(100, dim.get_cumul_var_max(node))

        self.assertFalse(dim.has_cumul_var_soft_upper_bound(0))
        self.assertTrue(dim.has_cumul_var_soft_upper_bound(node))
        self.assertEqual(10, dim.get_cumul_var_soft_upper_bound(node))
        self.assertEqual(100, dim.get_cumul_var_soft_upper_bound_coefficient(node))

        self.assertFalse(dim.has_cumul_var_soft_lower_bound(0))
        self.assertTrue(dim.has_cumul_var_soft_lower_bound(node))
        self.assertEqual(0, dim.get_cumul_var_soft_lower_bound(node))
        self.assertEqual(100, dim.get_cumul_var_soft_lower_bound_coefficient(node))

        dim.set_soft_span_upper_bound_for_vehicle(routing.BoundCost(100, 10), 0)
        self.assertTrue(dim.has_soft_span_upper_bounds())
        bound = dim.get_soft_span_upper_bound_for_vehicle(0)
        self.assertEqual(100, bound.bound)
        self.assertEqual(10, bound.cost)

        dim.set_quadratic_cost_soft_span_upper_bound_for_vehicle(
            routing.BoundCost(200, 20), 0
        )
        self.assertTrue(dim.has_quadratic_cost_soft_span_upper_bounds())
        bound = dim.get_quadratic_cost_soft_span_upper_bound_for_vehicle(0)
        self.assertEqual(200, bound.bound)
        self.assertEqual(20, bound.cost)

        # Test piecewise linear cost
        pwl_cost = routing.PiecewiseLinearFunction(
            [0, 10],  # points_x
            [0, 10],  # points_y
            [1, 1],  # slopes
            [10, 20],  # other_points_x
        )
        dim.set_cumul_var_piecewise_linear_cost(node, pwl_cost)
        self.assertTrue(dim.has_cumul_var_piecewise_linear_cost(node))
        piecewise_cost = dim.get_cumul_var_piecewise_linear_cost(node)
        self.assertIsNotNone(piecewise_cost)
        self.assertEqual(5, piecewise_cost.value(5))

        # Test class transit
        # Assuming standard vehicles have class 0?
        self.assertEqual(1, dim.get_transit_value_from_class(0, 1, 0))

    def test_break_intervals(self) -> None:
        manager = routing.IndexManager(num_nodes=10, num_vehicles=1, depot=0)
        routing_model = routing.Model(manager)
        transit_idx = routing_model.register_transit_callback(lambda i, j: 1)
        routing_model.add_dimension(transit_idx, 100, 100, True, "Dist")
        dim = routing_model.get_dimension_or_die("Dist")

        solver = routing_model.solver
        interval = solver.new_fixed_duration_interval_var(0, 10, 5, False, "Break")

        # Test set_break_intervals_of_vehicle with evaluators
        dim.set_break_intervals_of_vehicle([interval], 0, -1, -1)

        # Test set_break_intervals_of_vehicle with node visits
        dim.set_break_intervals_of_vehicle([interval], 0, [0] * manager.num_nodes())

        # Test break distance duration
        dim.set_break_distance_duration_of_vehicle(10, 5, 0)
        break_distance_duration = dim.get_break_distance_duration_of_vehicle(0)
        self.assertLen(break_distance_duration, 1)
        self.assertEqual(10, break_distance_duration[0][0])
        self.assertEqual(5, break_distance_duration[0][1])

        # Test get_break_intervals_of_vehicle
        # Note: IntervalVars are not directly comparable in Python, so we check size
        breaks = dim.get_break_intervals_of_vehicle(0)
        self.assertLen(breaks, 1)
        # Check that it returns the interval we set earlier
        # (The name might be empty if not set, but we set it as "Break")
        self.assertEqual("Break", breaks[0].name)

        # Test InitializeBreaks and HasBreakConstraints
        self.assertTrue(dim.has_break_constraints())
        self.assertTrue(hasattr(dim, "initialize_breaks"))


if __name__ == "__main__":
    absltest.main()
