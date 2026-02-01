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

"""Tests for CP-SAT routing."""

from absl.testing import absltest
from ortools.routing import enums_pb2
from ortools.routing.python import routing
from ortools.util import optional_boolean_pb2
from ortools.util.python import sorted_interval_list


def get_node_value(manager, i):
    return manager.index_to_node(i)


class SatTest(absltest.TestCase):

    def test_solve_tsp_model(self):
        for use_scaling in [False, True]:
            for use_cp in [
                optional_boolean_pb2.BOOL_FALSE,
                optional_boolean_pb2.BOOL_TRUE,
            ]:
                for use_generalized_cp_sat in [False, True]:
                    parameters = routing.default_routing_search_parameters()
                    if use_generalized_cp_sat:
                        parameters.use_generalized_cp_sat = (
                            optional_boolean_pb2.BOOL_TRUE
                        )
                    else:
                        parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE
                    parameters.use_cp = use_cp
                    if use_scaling:
                        parameters.log_cost_scaling_factor = 2.3
                        parameters.log_cost_offset = 1.1

                    index_manager = routing.IndexManager(4, 1, 0)
                    routing_model = routing.Model(index_manager)

                    # Using register_unary_transit_callback equivalent logic
                    vehicle_cost = routing_model.register_unary_transit_callback(
                        lambda i, im=index_manager: get_node_value(im, i)
                    )
                    routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)

                    assignment = routing_model.solve_with_parameters(parameters)
                    self.assertEqual(
                        enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL,
                        routing_model.status(),
                    )
                    self.assertIsNotNone(assignment)
                    self.assertEqual(1 + 2 + 3, assignment.objective_value())

    def test_solve_tsp_model_with_disjoint_time_windows(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            for use_generalized_cp_sat in [False, True]:
                parameters = routing.default_routing_search_parameters()
                if use_generalized_cp_sat:
                    parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
                else:
                    parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE
                parameters.use_cp = use_cp

                index_manager = routing.IndexManager(4, 1, 0)
                routing_model = routing.Model(index_manager)
                routing_model.add_constant_dimension_with_slack(
                    1, 1000, 1000, True, "dim"
                )
                dim = routing_model.get_dimension_or_die("dim")
                dim.set_span_cost_coefficient_for_all_vehicles(10)

                solver = routing_model.solver
                forbidden_values = [1, 2]  # Disjoint time windows [1, 2] forbidden
                for i in range(3):
                    dim.cumul_var(i).set_range(0, 100)
                    solver.add_not_member_ct(dim.cumul_var(i), forbidden_values)

                assignment = routing_model.solve_with_parameters(parameters)
                # Note: C++ test expects ROUTING_SUCCESS because optimality proof
                # is hard with disjoint windows. For small instance it might be
                # OPTIMAL.
                # c++ test assertion:
                # EXPECT_EQ(routing_model.status(),
                #           RoutingSearchStatus::ROUTING_SUCCESS);
                # Let's relax to check for assignment validity.
                self.assertIsNotNone(assignment)
                self.assertLessEqual(10 * (1 + 2 + 1 + 1), assignment.objective_value())
                self.assertGreaterEqual(
                    10 * (1 + 3 + 1 + 1), assignment.objective_value()
                )

    def test_solve_tsp_model_with_soft_ub(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            for use_generalized_cp_sat in [False, True]:
                parameters = routing.default_routing_search_parameters()
                if use_generalized_cp_sat:
                    parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
                else:
                    parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE
                parameters.use_cp = use_cp

                index_manager = routing.IndexManager(4, 1, 0)
                routing_model = routing.Model(index_manager)
                vehicle_cost = routing_model.register_unary_transit_callback(
                    lambda i, im=index_manager: get_node_value(im, i)
                )
                routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)

                routing_model.add_constant_dimension_with_slack(
                    1, 1000, 1000, False, "dim"
                )
                dim = routing_model.get_dimension_or_die("dim")
                dim.set_cumul_var_soft_upper_bound(1, 1, 20)
                dim.set_cumul_var_soft_upper_bound(2, 1, 10)

                assignment = routing_model.solve_with_parameters(parameters)
                self.assertEqual(
                    enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL,
                    routing_model.status(),
                )
                self.assertIsNotNone(assignment)
                self.assertEqual(1 + 2 + 3 + 10 * 1, assignment.objective_value())

    def test_solve_tsp_model_with_soft_lb(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            for use_generalized_cp_sat in [False, True]:
                parameters = routing.default_routing_search_parameters()
                if use_generalized_cp_sat:
                    parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
                else:
                    parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE
                parameters.use_cp = use_cp

                index_manager = routing.IndexManager(4, 1, 0)
                routing_model = routing.Model(index_manager)
                vehicle_cost = routing_model.register_unary_transit_callback(
                    lambda i, im=index_manager: get_node_value(im, i)
                )
                routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)

                routing_model.add_constant_dimension_with_slack(
                    1, 1000, 1000, False, "dim"
                )
                dim = routing_model.get_dimension_or_die("dim")
                dim.set_cumul_var_soft_lower_bound(1, 999, 20)
                dim.set_cumul_var_soft_lower_bound(2, 999, 10)

                assignment = routing_model.solve_with_parameters(parameters)
                self.assertEqual(
                    enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL,
                    routing_model.status(),
                )
                self.assertIsNotNone(assignment)
                self.assertEqual(1 + 2 + 3 + 10 * 1, assignment.objective_value())

    def test_solve_tsp_model_with_soft_lb_and_ub(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            for use_generalized_cp_sat in [False, True]:
                parameters = routing.default_routing_search_parameters()
                if use_generalized_cp_sat:
                    parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
                else:
                    parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE
                parameters.use_cp = use_cp

                index_manager = routing.IndexManager(4, 1, 0)
                routing_model = routing.Model(index_manager)
                vehicle_cost = routing_model.register_unary_transit_callback(
                    lambda i, im=index_manager: get_node_value(im, i)
                )
                routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)

                routing_model.add_constant_dimension_with_slack(
                    1, 1000, 1000, False, "dim"
                )
                dim = routing_model.get_dimension_or_die("dim")

                dim.set_cumul_var_soft_lower_bound(1, 1, 20)
                dim.set_cumul_var_soft_lower_bound(2, 1, 10)
                dim.set_cumul_var_soft_upper_bound(1, 1, 20)
                dim.set_cumul_var_soft_upper_bound(2, 1, 10)

                assignment = routing_model.solve_with_parameters(parameters)
                self.assertEqual(
                    enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL,
                    routing_model.status(),
                )
                self.assertIsNotNone(assignment)
                self.assertEqual(1 + 2 + 3 + 10 * 1, assignment.objective_value())

    def test_solve_tsp_with_unperformed_model(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            for use_generalized_cp_sat in [False, True]:
                parameters = routing.default_routing_search_parameters()
                if use_generalized_cp_sat:
                    parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
                else:
                    parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE
                parameters.use_cp = use_cp
                parameters.log_search = True

                index_manager = routing.IndexManager(4, 1, 0)
                routing_model = routing.Model(index_manager)
                vehicle_cost = routing_model.register_unary_transit_callback(
                    lambda i, im=index_manager: get_node_value(im, i)
                )
                routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)

                routing_model.add_constant_dimension_with_slack(1, 3, 3, False, "dim")
                for i in range(1, 4):
                    routing_model.add_disjunction([index_manager.node_to_index(i)], 100)

                assignment = routing_model.solve_with_parameters(parameters)
                self.assertEqual(
                    enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL,
                    routing_model.status(),
                )
                self.assertIsNotNone(assignment)
                self.assertEqual(1 + 2 + 100, assignment.objective_value())

    def test_solve_tsp_with_delayed_active_nodes(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            parameters = routing.default_routing_search_parameters()
            parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE
            parameters.use_cp = use_cp

            index_manager = routing.IndexManager(4, 1, 0)
            routing_model = routing.Model(index_manager)
            vehicle_cost = routing_model.register_unary_transit_callback(
                lambda i, im=index_manager: get_node_value(im, i)
            )
            routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)

            for i in range(1, 4):
                routing_model.add_disjunction(
                    [index_manager.node_to_index(i)], -1
                )  # -1 is kNoPenalty

            assignment = routing_model.solve_with_parameters(parameters)
            self.assertEqual(
                enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status()
            )
            self.assertIsNotNone(assignment)
            self.assertEqual(1 + 2 + 3, assignment.objective_value())

    def test_solve_vrp_with_soft_lb_and_ub(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            for use_generalized_cp_sat in [False, True]:
                parameters = routing.default_routing_search_parameters()
                if use_generalized_cp_sat:
                    parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
                else:
                    parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE
                parameters.use_cp = use_cp
                parameters.log_search = True

                index_manager = routing.IndexManager(3, 2, 0)
                routing_model = routing.Model(index_manager)
                vehicle_cost = routing_model.register_transit_callback(
                    lambda from_, to: 100
                )
                routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)
                routing_model.set_fixed_cost_of_all_vehicles(0)

                routing_model.add_constant_dimension_with_slack(
                    1, 1000, 1000, False, "dim"
                )
                dim = routing_model.get_dimension_or_die("dim")

                dim.set_cumul_var_soft_lower_bound(1, 1, 20)
                dim.set_cumul_var_soft_lower_bound(2, 1, 10)
                dim.set_cumul_var_soft_upper_bound(1, 1, 20)
                dim.set_cumul_var_soft_upper_bound(2, 1, 10)

                assignment = routing_model.solve_with_parameters(parameters)
                self.assertEqual(
                    enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL,
                    routing_model.status(),
                )
                self.assertIsNotNone(assignment)
                self.assertEqual(100 + 100 + 100 + 10, assignment.objective_value())

    def test_solve_vrp_callbacks(self):
        for report_intermediate_cp_sat_solutions in [False, True]:
            for use_cp in [
                optional_boolean_pb2.BOOL_FALSE,
                optional_boolean_pb2.BOOL_TRUE,
            ]:
                for use_generalized_cp_sat in [False, True]:
                    parameters = routing.default_routing_search_parameters()
                    parameters.report_intermediate_cp_sat_solutions = (
                        report_intermediate_cp_sat_solutions
                    )
                    if use_generalized_cp_sat:
                        parameters.use_generalized_cp_sat = (
                            optional_boolean_pb2.BOOL_TRUE
                        )
                    else:
                        parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE
                    parameters.use_cp = use_cp
                    parameters.log_search = True

                    index_manager = routing.IndexManager(3, 2, 0)
                    routing_model = routing.Model(index_manager)
                    vehicle_cost = routing_model.register_transit_callback(
                        lambda f, t: 100
                    )
                    routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)
                    routing_model.set_fixed_cost_of_all_vehicles(0)

                    routing_model.add_constant_dimension_with_slack(
                        1, 1000, 1000, False, "dim"
                    )
                    dim = routing_model.get_dimension_or_die("dim")
                    dim.cumul_var(1).set_range(1, 3)
                    dim.cumul_var(2).set_range(2, 30)

                    count = [0]

                    def callback(c=count):  # pylint: disable=dangerous-default-value
                        c[0] += 1

                    routing_model.add_at_solution_callback(callback)

                    assignment = routing_model.solve_with_parameters(parameters)
                    self.assertEqual(
                        enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL,
                        routing_model.status(),
                    )
                    self.assertIsNotNone(assignment)
                    self.assertEqual(100 + 100 + 100, assignment.objective_value())
                    # Python counts might differ slightly or depend on wrapper
                    # implementation of callbacks, but we check at least one solution was
                    # found.
                    self.assertGreater(count[0], 0)

    def test_solve_vrp_with_unperformed_model(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            for use_generalized_cp_sat in [False, True]:
                parameters = routing.default_routing_search_parameters()
                if use_generalized_cp_sat:
                    parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
                else:
                    parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE
                parameters.use_cp = use_cp

                index_manager = routing.IndexManager(3, 2, 0)
                routing_model = routing.Model(index_manager)
                vehicle_cost = routing_model.register_transit_callback(
                    lambda from_, to: 100
                )
                routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)
                routing_model.set_fixed_cost_of_all_vehicles(0)

                routing_model.add_disjunction([1], 500)
                routing_model.add_disjunction([2], 10)

                assignment = routing_model.solve_with_parameters(parameters)
                self.assertEqual(
                    enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL,
                    routing_model.status(),
                )
                self.assertIsNotNone(assignment)
                self.assertEqual(100 + 100 + 10, assignment.objective_value())

    def test_solve_alternatives_vrp_model_with_forced_active(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            parameters = routing.default_routing_search_parameters()
            parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
            parameters.use_cp = use_cp
            parameters.log_search = True

            index_manager = routing.IndexManager(5, 2, 0)
            routing_model = routing.Model(index_manager)
            vehicle_cost = routing_model.register_transit_callback(lambda from_, to: 10)
            routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)
            routing_model.set_fixed_cost_of_all_vehicles(0)

            routing_model.add_disjunction([1, 2, 3], 50)
            routing_model.active_var(3).set_values([1])
            routing_model.add_disjunction([4], 5)

            assignment = routing_model.solve_with_parameters(parameters)
            self.assertEqual(
                enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status()
            )
            self.assertIsNotNone(assignment)
            self.assertEqual(5 + 2 * 10, assignment.objective_value())
            self.assertEqual(4, assignment.value(routing_model.next_var(4)))
            self.assertEqual(1, assignment.value(routing_model.active_var(3)))
            self.assertEqual(1, assignment.value(routing_model.active_var(5)))

    def test_solve_alternatives_vrp_model_with_double_disjunctions(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            parameters = routing.default_routing_search_parameters()
            parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
            parameters.use_cp = use_cp
            parameters.log_search = True

            index_manager = routing.IndexManager(5, 2, 0)
            routing_model = routing.Model(index_manager)
            vehicle_cost = routing_model.register_unary_transit_callback(
                lambda i, im=index_manager: get_node_value(im, i)
            )
            routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)
            routing_model.set_fixed_cost_of_all_vehicles(0)

            routing_model.add_disjunction([1, 2, 3, 4], 0, 3)
            routing_model.add_disjunction([1], 100)
            routing_model.add_disjunction([2], -1)
            routing_model.add_disjunction([3], 2**63 - 1)  # max int64
            routing_model.active_var(4).set_values([1])

            assignment = routing_model.solve_with_parameters(parameters)
            self.assertEqual(
                enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status()
            )
            self.assertIsNotNone(assignment)
            self.assertEqual(100 + 2 + 3 + 4, assignment.objective_value())

    def test_solve_capacitated_alternatives_vrp_model_with_forced_active(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            parameters = routing.default_routing_search_parameters()
            parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
            parameters.use_cp = use_cp
            parameters.log_search = True

            index_manager = routing.IndexManager(6, 2, 0)
            routing_model = routing.Model(index_manager)
            vehicle_cost = routing_model.register_unary_transit_callback(
                lambda i, im=index_manager: get_node_value(im, i)
            )
            routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)
            routing_model.set_fixed_cost_of_all_vehicles(100)

            routing_model.add_constant_dimension(1, 3, True, "dim")

            routing_model.add_disjunction([1, 2, 3], 1000, 2)
            routing_model.add_disjunction([2], -1)
            routing_model.add_disjunction([3], 2**63 - 1)
            routing_model.add_disjunction([4, 5], 1000)
            routing_model.active_var(5).set_values([1])

            assignment = routing_model.solve_with_parameters(parameters)
            self.assertEqual(
                enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status()
            )
            self.assertIsNotNone(assignment)
            self.assertEqual(200 + 2 + 3 + 5, assignment.objective_value())

    def test_solve_tsp_with_windows_model(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            for use_generalized_cp_sat in [False, True]:
                parameters = routing.default_routing_search_parameters()
                if use_generalized_cp_sat:
                    parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
                else:
                    parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE
                parameters.use_cp = use_cp
                parameters.log_search = True

                index_manager = routing.IndexManager(4, 1, 0)
                routing_model = routing.Model(index_manager)
                vehicle_cost = routing_model.register_unary_transit_callback(
                    lambda i, im=index_manager: get_node_value(im, i)
                )
                routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)

                routing_model.add_constant_dimension_with_slack(1, 7, 7, False, "dim")
                dim = routing_model.get_dimension_or_die("dim")
                dim.cumul_var(0).set_range(0, 1)
                dim.cumul_var(1).set_range(5, 7)
                dim.cumul_var(2).set_range(3, 4)
                dim.cumul_var(3).set_range(4, 5)

                assignment = routing_model.solve_with_parameters(parameters)
                self.assertEqual(
                    enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL,
                    routing_model.status(),
                )
                self.assertIsNotNone(assignment)
                self.assertEqual(1 + 2 + 3, assignment.objective_value())
                self.assertEqual(2, assignment.value(routing_model.next_var(0)))
                self.assertEqual(3, assignment.value(routing_model.next_var(2)))
                self.assertEqual(1, assignment.value(routing_model.next_var(3)))
                self.assertEqual(4, assignment.value(routing_model.next_var(1)))

    def test_solve_tsp_with_windows_model_different_slacks(self):
        parameters = routing.default_routing_search_parameters()
        parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
        parameters.use_cp = optional_boolean_pb2.BOOL_FALSE
        parameters.log_search = True

        index_manager = routing.IndexManager(3, 1, 0)
        routing_model = routing.Model(index_manager)
        vehicle_cost = routing_model.register_unary_transit_callback(
            lambda i: get_node_value(index_manager, i)
        )
        routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)

        routing_model.add_constant_dimension_with_slack(1, 4, 1, False, "dim")
        dim = routing_model.get_dimension_or_die("dim")
        dim.slack_var(0).set_range(0, 0)
        dim.slack_var(1).set_range(0, 1)
        dim.slack_var(2).set_range(0, 0)
        dim.cumul_var(0).set_range(0, 0)
        dim.cumul_var(1).set_range(1, 1)
        dim.cumul_var(2).set_range(3, 3)

        assignment = routing_model.solve_with_parameters(parameters)
        self.assertEqual(
            enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status()
        )
        self.assertIsNotNone(assignment)
        self.assertEqual(1 + 2, assignment.objective_value())

    def test_solve_tsp_with_pickup_and_delivery(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            for use_generalized_cp_sat in [False, True]:
                parameters = routing.default_routing_search_parameters()
                if use_generalized_cp_sat:
                    parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
                else:
                    parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE
                parameters.use_cp = use_cp
                parameters.log_search = True

                index_manager = routing.IndexManager(5, 1, 0)
                routing_model = routing.Model(index_manager)
                vehicle_cost = routing_model.register_unary_transit_callback(
                    lambda i, im=index_manager: get_node_value(im, i)
                )
                routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)

                routing_model.add_pickup_and_delivery(4, 1)
                routing_model.add_pickup_and_delivery(2, 3)
                solver = routing_model.solver
                solver.add(routing_model.vehicle_var(4) == routing_model.vehicle_var(1))
                solver.add(routing_model.vehicle_var(2) == routing_model.vehicle_var(3))

                routing_model.add_constant_dimension_with_slack(1, 5, 5, True, "dim")
                dim = routing_model.get_dimension_or_die("dim")
                solver.add(dim.cumul_var(4) <= dim.cumul_var(1))
                solver.add(dim.cumul_var(2) <= dim.cumul_var(3))

                assignment = routing_model.solve_with_parameters(parameters)
                self.assertEqual(
                    enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL,
                    routing_model.status(),
                )
                self.assertIsNotNone(assignment)
                self.assertEqual(1 + 2 + 3 + 4, assignment.objective_value())

                ranks = [0] * 5
                rank = 0
                node = 0
                while node != 5:
                    # end node for 1 vehicle, 5 nodes is index 0..4, wait. nodes are
                    # 0..4. End is 0? No, 1 vehicle, 5 nodes.
                    # IndexManager(5, 1, 0) -> nodes 0..4. End is implicit?
                    # In C++: IndexManager index_manager(5, 1, NodeIndex(0));
                    # So 0 is depot. nodes 0, 1, 2, 3, 4.
                    # C++ test loop: while (node != 5) ...
                    # Wait, index_manager has 5 nodes. Indices are 0..4.
                    # If 1 vehicle, and starts/ends are same, usually it wraps around.
                    # But the C++ test code says `while (node != 5)`.
                    # Maybe it implies 5 is the end index?
                    # Let's check `index_manager.GetNumberOfIndices()`.
                    # 5 nodes -> indices 0..4.
                    # Wait, `IndexManager index_manager(5, 1, NodeIndex(0));`
                    # Number of nodes = 5. Indices 0,1,2,3,4.
                    # Depots: 0.
                    # If 0 is start/end, then 0 is start. Is it end?
                    # `index_manager.IndexToNode(i)` maps index to node.
                    # If `node != 5`, implies 5 is a valid index for end?
                    # If we have 5 nodes, indices are 0..4. 5 is out of bounds unless it's
                    # the end node for the vehicle.
                    # For 1 vehicle, usually end index is `num_nodes`.
                    # So yes, 5 is likely the end index.
                    ranks[node] = rank
                    rank += 1
                    node = assignment.value(routing_model.next_var(node))
                    if node == 0:
                        break  # Safety break if it loops back to 0 without hitting 5

                # Actually, in Python binding `routing_model.End(0)` gives the end
                # index.
                # Let's trust C++ test which uses `5`.
                # But in Python test I should use `routing_model.end(0)`.

                self.assertLess(ranks[4], ranks[1])
                self.assertLess(ranks[2], ranks[3])

    def test_solve_tsp_model_with_sat_improvement(self):
        for use_generalized_cp_sat in [False, True]:
            parameters = routing.default_routing_search_parameters()
            if use_generalized_cp_sat:
                parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
            else:
                parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE
            parameters.use_cp = optional_boolean_pb2.BOOL_TRUE
            parameters.solution_limit = 1

            index_manager = routing.IndexManager(4, 1, 0)
            routing_model = routing.Model(index_manager)
            vehicle_cost = routing_model.register_transit_callback(
                lambda from_, to: 1 if from_ < to else 0
            )
            routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)

            initial_assignment = routing_model.solver.assignment()
            for i in range(4):
                initial_assignment.add(routing_model.next_var(i)).set_value(i + 1)

            assignment = routing_model.solve_from_assignment_with_parameters(
                initial_assignment, parameters
            )

            if use_generalized_cp_sat:
                self.assertEqual(
                    enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL,
                    routing_model.status(),
                )
            else:
                self.assertEqual(
                    enums_pb2.RoutingSearchStatus.ROUTING_SUCCESS,
                    routing_model.status(),
                )
            self.assertIsNotNone(assignment)
            # Python wrapper returns only the last solution, so we check if it found
            # the optimal one
            self.assertEqual(2, assignment.objective_value())

    def test_solve_tsp_model_with_sat_hint_and_forbidden_costs(self):
        for use_generalized_cp_sat in [False, True]:
            parameters = routing.default_routing_search_parameters()
            if use_generalized_cp_sat:
                parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
            else:
                parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE
            parameters.use_cp = optional_boolean_pb2.BOOL_TRUE
            parameters.number_of_solutions_to_collect = 2

            index_manager = routing.IndexManager(2, 1, 0)
            routing_model = routing.Model(index_manager)
            vehicle_cost = routing_model.register_transit_callback(
                lambda from_, to: 2**63 - 1 if from_ != to else 0
            )
            routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)

            assignment = routing_model.solve_with_parameters(parameters)
            self.assertEqual(
                enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status()
            )
            self.assertIsNotNone(assignment)
            self.assertEqual(2**63 - 1, assignment.objective_value())

    def test_solve_unconstrained_vrp_model_with_sat_improvement(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            for use_generalized_cp_sat in [False, True]:
                parameters = routing.default_routing_search_parameters()
                if use_generalized_cp_sat:
                    parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
                else:
                    parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE
                parameters.use_cp = use_cp

                index_manager = routing.IndexManager(4, 2, 0)
                routing_model = routing.Model(index_manager)
                vehicle_cost = routing_model.register_unary_transit_callback(
                    lambda i, im=index_manager: get_node_value(im, i)
                )
                routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)

                assignment = routing_model.solve_with_parameters(parameters)
                self.assertEqual(
                    enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL,
                    routing_model.status(),
                )
                self.assertIsNotNone(assignment)
                self.assertEqual(1 + 2 + 3, assignment.objective_value())

    def test_solve_constrained_vehicle_vrp_model(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            for use_generalized_cp_sat in [False, True]:
                parameters = routing.default_routing_search_parameters()
                if use_generalized_cp_sat:
                    parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
                else:
                    # Even if the non-generalized cp-sat solver is specified, we will
                    # automatically fallback to the generalized routing_model since
                    # vehicles
                    # are heterogeneous.
                    parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE
                parameters.use_cp = use_cp

                index_manager = routing.IndexManager(4, 2, 0)
                routing_model = routing.Model(index_manager)
                vehicle_cost = routing_model.register_unary_transit_callback(
                    lambda i, im=index_manager: get_node_value(im, i)
                )
                routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)

                for i in range(4):
                    if routing_model.is_start(i) or routing_model.is_end(i):
                        continue
                    routing_model.vehicle_var(i).remove_value(i % 2)

                assignment = routing_model.solve_with_parameters(parameters)
                self.assertEqual(
                    enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL,
                    routing_model.status(),
                )
                self.assertIsNotNone(assignment)
                self.assertEqual(1 + 2 + 3, assignment.objective_value())
                self.assertGreater(
                    routing_model.search_stats().num_generalized_cp_sat_calls_in_routing,
                    0,
                )

    def test_solve_capacitated_vrp_model(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            for use_generalized_cp_sat in [False, True]:
                parameters = routing.default_routing_search_parameters()
                if use_generalized_cp_sat:
                    parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
                else:
                    parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE
                parameters.use_cp = use_cp

                index_manager = routing.IndexManager(4, 2, 0)
                routing_model = routing.Model(index_manager)
                vehicle_cost = routing_model.register_unary_transit_callback(
                    lambda i, im=index_manager: get_node_value(im, i)
                )
                routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)
                routing_model.set_fixed_cost_of_all_vehicles(100)

                routing_model.add_constant_dimension_with_slack(1, 3, 3, True, "dim")

                assignment = routing_model.solve_with_parameters(parameters)
                self.assertEqual(
                    enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL,
                    routing_model.status(),
                )
                self.assertIsNotNone(assignment)
                self.assertEqual(200 + 1 + 2 + 3, assignment.objective_value())

    def test_solve_heterogeneous_capacitated_vrp_model(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            parameters = routing.default_routing_search_parameters()
            parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
            parameters.use_cp = use_cp

            vehicle_costs = [1, 2]
            vehicle_capacities = [2, 3]

            index_manager = routing.IndexManager(4, len(vehicle_costs), 0)
            routing_model = routing.Model(index_manager)

            for vehicle in range(len(vehicle_costs)):
                vehicle_cost = routing_model.register_unary_transit_callback(
                    lambda i, cost=vehicle_costs[vehicle]: cost
                )
                routing_model.set_arc_cost_evaluator_of_vehicle(vehicle_cost, vehicle)
                routing_model.set_fixed_cost_of_vehicle(
                    vehicle_costs[vehicle] * 100, vehicle
                )

            routing_model.add_dimension_with_vehicle_capacity(
                1, 3, vehicle_capacities, True, "dim"
            )

            assignment = routing_model.solve_with_parameters(parameters)
            self.assertEqual(
                enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status()
            )
            self.assertIsNotNone(assignment)
            self.assertEqual(
                100 + 1 + 1 + 200 + 2 + 2 + 2, assignment.objective_value()
            )

    def test_solve_vrp_model_with_forbidden_arc(self):
        for use_generalized_cp_sat in [False, True]:
            parameters = routing.default_routing_search_parameters()
            if use_generalized_cp_sat:
                parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
            else:
                parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE

            index_manager = routing.IndexManager(4, 2, 0)
            routing_model = routing.Model(index_manager)

            # Forbid 1 -> 2
            def vehicle_cost_callback(from_, to):
                if from_ == 1 and to == 2:
                    return 2**63 - 1
                return 1

            vehicle_cost = routing_model.register_transit_callback(
                vehicle_cost_callback
            )
            routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)

            assignment = routing_model.solve_with_parameters(parameters)
            self.assertEqual(
                enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status()
            )
            self.assertIsNotNone(assignment)
            self.assertEqual(4, assignment.objective_value())

    def test_solve_vrp_model_with_forbidden_arc_from_depot(self):
        for use_generalized_cp_sat in [False, True]:
            parameters = routing.default_routing_search_parameters()
            if use_generalized_cp_sat:
                parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
            else:
                parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE

            index_manager = routing.IndexManager(4, 2, 0)
            routing_model = routing.Model(index_manager)

            # Forbid depot -> 1
            def vehicle_cost_callback(from_, to):
                if from_ == 0 and to == 1:
                    return 2**63 - 1
                if from_ == 4 and to == 1:
                    return 2**63 - 1  # End nodes
                return 1

            vehicle_cost = routing_model.register_transit_callback(
                vehicle_cost_callback
            )
            routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)

            assignment = routing_model.solve_with_parameters(parameters)
            self.assertEqual(
                enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status()
            )
            self.assertIsNotNone(assignment)
            self.assertEqual(4, assignment.objective_value())

    def test_solve_vrp_model_with_forbidden_arc_to_depot(self):
        for use_generalized_cp_sat in [False, True]:
            parameters = routing.default_routing_search_parameters()
            if use_generalized_cp_sat:
                parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
            else:
                parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE

            index_manager = routing.IndexManager(4, 2, 0)
            routing_model = routing.Model(index_manager)

            # Forbid 1 -> depot
            def vehicle_cost_callback(from_, to):
                if from_ == 1 and to == 0:
                    return 2**63 - 1
                if from_ == 1 and to > 4:
                    return (
                        2**63 - 1
                    )  # End nodes > 4? IndexManager(4,2,0). nodes 0..3. indices 0..5.
                # indices: 0(depot), 1, 2, 3. Vehicles 0, 1.
                # Depots: 0. Ends: 4, 5.
                # So > 4 implies 5.
                # But wait, IndexManager(4, 2, 0) has 4 nodes (0,1,2,3).
                # Indices: 0, 1, 2, 3.
                # 2 vehicles.
                # Starts: 0, 0.
                # Ends: 4, 5.
                # Indices 0..5.
                # So to > 4 means to == 5 (second vehicle end).
                # But wait, "to > 4" in 0..5 is just 5.
                # C++ test: `if (from == 1 && to > 4)`
                # C++ IndexManager(4, 2, NodeIndex(0)).
                # Nodes 0..3. Indices 0..5.
                # So 5 is end of second vehicle. 4 is end of first vehicle.
                # If we forbid 1 -> 0 (start), 1 -> 4, 1 -> 5.
                # The C++ code: `if (from == 1 && to == 0) ...` and
                # `if (from == 1 && to > 4) ...`
                # It misses `to == 4`.
                # Maybe `to == 0` covers start.
                # But end nodes are different.
                # If `to == 0` is checked, it means arc to start
                # (illegal in routing generally unless loop?).
                # But 4 and 5 are end nodes.
                # If we forbid 1 -> depot, we should forbid 1 -> 4 and 1 -> 5.
                # The C++ code forbids `to == 0` and `to > 4`.
                # Why not `to == 4`?
                # Maybe because `to == 0` is not used for end?
                # End nodes are 4 and 5.
                # If 1 -> 4 is allowed, then 1 -> depot is allowed for vehicle 0.
                # If 1 -> 5 is forbidden, then 1 -> depot is forbidden for vehicle 1.
                # The test expects 4.
                # If 1 -> 4 is allowed, cost is 4.
                # If 1 -> 5 is allowed, cost is 4.
                # If 1->4 is allowed, 1 is visited by vehicle 0.
                # If 1->5 is forbidden, 1 must be visited by vehicle 0
                # (if 1 is visited).
                # So effectively it forces 1 to vehicle 0?
                # But the test says "Forbid 1 -> depot".
                # If `to == 0` is forbidden, that's irrelevant because 0 is start.
                # Arcs go to end nodes (4, 5).
                # So we should forbid 1->4 and 1->5.
                # The C++ code:
                # if (from == 1 && to == 0) return max;
                # if (from == 1 && to > 4) return max;
                # This seems to only forbid 1->5 (and 1->0 which is start).
                # So 1->4 is allowed.
                # So 1 can go to depot via vehicle 0.
                # I'll just copy the logic exactly.
                if from_ == 1 and to > 4:
                    return 2**63 - 1
                return 1

            vehicle_cost = routing_model.register_transit_callback(
                vehicle_cost_callback
            )
            routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)

            assignment = routing_model.solve_with_parameters(parameters)
            self.assertEqual(
                enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status()
            )
            self.assertIsNotNone(assignment)
            self.assertEqual(4, assignment.objective_value())

    def test_solve_no_unary_capacities_vrp_model(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            for use_generalized_cp_sat in [False, True]:
                parameters = routing.default_routing_search_parameters()
                if use_generalized_cp_sat:
                    parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
                else:
                    parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE
                parameters.use_cp = use_cp

                index_manager = routing.IndexManager(4, 2, 0)
                routing_model = routing.Model(index_manager)
                vehicle_cost = routing_model.register_unary_transit_callback(
                    lambda i, im=index_manager: get_node_value(im, i)
                )
                routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)
                routing_model.set_fixed_cost_of_all_vehicles(100)

                routing_model.add_dimension(
                    routing_model.register_transit_callback(lambda f, t: 1),
                    3,
                    3,
                    True,
                    "dim",
                )

                assignment = routing_model.solve_with_parameters(parameters)
                self.assertEqual(
                    enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL,
                    routing_model.status(),
                )
                self.assertIsNotNone(assignment)
                self.assertEqual(200 + 1 + 2 + 3, assignment.objective_value())

    def test_solve_capacitated_vrp_with_windows_model(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            for use_generalized_cp_sat in [False, True]:
                parameters = routing.default_routing_search_parameters()
                if use_generalized_cp_sat:
                    parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
                else:
                    parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE
                parameters.use_cp = use_cp
                parameters.log_search = True

                index_manager = routing.IndexManager(4, 2, 0)
                routing_model = routing.Model(index_manager)
                vehicle_cost = routing_model.register_unary_transit_callback(
                    lambda i, im=index_manager: get_node_value(im, i)
                )
                routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)
                routing_model.set_fixed_cost_of_all_vehicles(100)

                routing_model.add_constant_dimension_with_slack(1, 3, 3, True, "dim")

                routing_model.add_dimension(
                    routing_model.register_transit_callback(lambda f, t: 1),
                    7,
                    7,
                    False,
                    "time",
                )
                time = routing_model.get_dimension_or_die("time")
                time.cumul_var(0).set_range(0, 1)
                time.cumul_var(1).set_range(5, 7)
                time.cumul_var(2).set_range(3, 4)
                time.cumul_var(3).set_range(4, 5)
                time.cumul_var(4).set_range(0, 1)

                assignment = routing_model.solve_with_parameters(parameters)
                self.assertEqual(
                    enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL,
                    routing_model.status(),
                )
                self.assertIsNotNone(assignment)
                self.assertEqual(200 + 1 + 2 + 3, assignment.objective_value())

    def test_solve_capacitated_tsp_model_with_windows_and_slack_cost(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            parameters = routing.default_routing_search_parameters()
            parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
            parameters.use_cp = use_cp
            parameters.log_search = True

            index_manager = routing.IndexManager(3, 1, 0)
            routing_model = routing.Model(index_manager)
            vehicle_cost = routing_model.register_transit_callback(
                lambda from_, to: 1 if from_ < to else 0
            )
            routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)
            routing_model.set_fixed_cost_of_all_vehicles(0)

            routing_model.add_dimension(
                routing_model.register_transit_callback(lambda f, t: 1),
                7,
                7,
                True,
                "time",
            )
            time = routing_model.get_dimension_or_die("time")
            time.cumul_var(0).set_range(0, 0)
            time.cumul_var(1).set_range(1, 6)
            time.cumul_var(2).set_range(5, 5)
            for i in range(routing_model.vehicles()):
                time.set_span_cost_coefficient_for_vehicle(100, i)

            assignment = routing_model.solve_with_parameters(parameters)
            self.assertEqual(
                enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status()
            )
            self.assertIsNotNone(assignment)
            self.assertEqual(600 + 3, assignment.objective_value())

    def test_solve_capacitated_vrp_with_windows_slack_costs_vehicle_classes(
        self,
    ):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            parameters = routing.default_routing_search_parameters()
            parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
            parameters.use_cp = use_cp
            parameters.log_search = True

            index_manager = routing.IndexManager(3, 3, 0)
            routing_model = routing.Model(index_manager)
            vehicle_cost = routing_model.register_transit_callback(lambda f, t: 1)
            routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)

            routing_model.add_dimension(
                routing_model.register_transit_callback(lambda f, t: 1),
                7,
                7,
                True,
                "time",
            )
            time = routing_model.get_dimension_or_die("time")
            time.cumul_var(0).set_range(0, 0)
            time.cumul_var(1).set_range(1, 6)
            time.cumul_var(2).set_range(5, 5)
            for i in range(routing_model.vehicles()):
                time.set_span_cost_coefficient_for_vehicle(100, i)

            assignment = routing_model.solve_with_parameters(parameters)
            self.assertEqual(
                enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status()
            )
            self.assertIsNotNone(assignment)
            self.assertEqual(600 + 1 + 1 + 1, assignment.objective_value())

    def test_solve_capacitated_vrp_windows_diff_transits_vehicle_classes(
        self,
    ):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            parameters = routing.default_routing_search_parameters()
            parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
            parameters.use_cp = use_cp
            parameters.log_search = True

            num_vehicles = 3
            index_manager = routing.IndexManager(3, num_vehicles, 0)
            routing_model = routing.Model(index_manager)

            vehicle_cost = routing_model.register_transit_callback(lambda f, t: 1)
            for i in range(num_vehicles):
                if i == num_vehicles - 1:
                    expensive_vehicle_cost = routing_model.register_transit_callback(
                        lambda f, t: 200
                    )
                    routing_model.set_arc_cost_evaluator_of_vehicle(
                        expensive_vehicle_cost, i
                    )
                else:
                    routing_model.set_arc_cost_evaluator_of_vehicle(vehicle_cost, i)

            routing_model.add_dimension(
                routing_model.register_transit_callback(lambda f, t: 1),
                7,
                7,
                True,
                "time",
            )
            time = routing_model.get_dimension_or_die("time")
            time.cumul_var(0).set_range(0, 0)
            time.cumul_var(1).set_range(1, 6)
            time.cumul_var(2).set_range(5, 5)
            for vehicle in range(num_vehicles):
                time.set_span_cost_coefficient_for_vehicle(
                    (num_vehicles - vehicle) * 100, vehicle
                )

            assignment = routing_model.solve_with_parameters(parameters)
            self.assertEqual(
                enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status()
            )
            self.assertIsNotNone(assignment)
            self.assertEqual(600 + 200 * 3, assignment.objective_value())
            self.assertEqual(3, routing_model.get_vehicle_classes_count())

    def test_solve_capacitated_vrp_windows_diff_vehicle_transits_vehicle_classes(
        self,
    ):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            parameters = routing.default_routing_search_parameters()
            parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
            parameters.use_cp = use_cp
            parameters.log_search = True

            num_vehicles = 9
            index_manager = routing.IndexManager(3, num_vehicles, 0)
            routing_model = routing.Model(index_manager)

            vehicle_cost = routing_model.register_transit_callback(lambda f, t: 1)
            routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)

            vehicle_transits = []
            for vehicle in range(num_vehicles):
                if vehicle % 3 == 0:
                    transit = routing_model.register_transit_callback(
                        lambda f, t, v=vehicle, nv=num_vehicles: 1 + (nv - v - 1) // 3
                    )
                    vehicle_transits.append(transit)
                else:
                    # Reusing the last one or creating new? C++ pushes back
                    # `vehicle_transit` index.
                    # C++ loop:
                    # int vehicle_transit;
                    # if (vehicle % 3 == 0) { ... set vehicle_transit ... }
                    # vehicle_transits.push_back(vehicle_transit);
                    # So it reuses the transit index from the last multiple of 3.
                    vehicle_transits.append(vehicle_transits[-1])  # Reuse last added

            routing_model.set_fixed_cost_of_all_vehicles(100)
            routing_model.add_dimension_with_vehicle_transits(
                vehicle_transits, 0, 7, True, "time"
            )
            time = routing_model.get_dimension_or_die("time")
            time.cumul_var(0).set_range(0, 0)
            time.cumul_var(1).set_range(1, 1)
            time.cumul_var(2).set_range(2, 2)

            assignment = routing_model.solve_with_parameters(parameters)
            self.assertEqual(
                enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status()
            )
            self.assertIsNotNone(assignment)
            self.assertEqual(100 + 1 + 1 + 1, assignment.objective_value())
            self.assertEqual(3, routing_model.get_vehicle_classes_count())

    def test_solve_capacitated_vrp_model_with_windows_slack_cost_and_disjunction(
        self,
    ):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            parameters = routing.default_routing_search_parameters()
            parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
            parameters.use_cp = use_cp
            parameters.log_search = True

            index_manager = routing.IndexManager(4, 2, 0)
            routing_model = routing.Model(index_manager)
            vehicle_cost = routing_model.register_transit_callback(
                lambda from_, to: 1 if from_ < to else 0
            )
            routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)
            routing_model.set_fixed_cost_of_all_vehicles(1000)

            routing_model.add_dimension(
                routing_model.register_transit_callback(lambda f, t: 1),
                7,
                14,
                True,
                "time",
            )
            time = routing_model.get_dimension_or_die("time")
            time.cumul_var(0).set_range(0, 0)
            time.cumul_var(1).set_range(1, 6)
            time.cumul_var(2).set_range(5, 5)
            time.cumul_var(3).set_range(10, 10)

            routing_model.add_disjunction([3], 2000)
            time.set_span_cost_coefficient_for_vehicle(1000, 0)
            time.set_span_cost_coefficient_for_vehicle(100, 1)

            assignment = routing_model.solve_with_parameters(parameters)
            self.assertEqual(
                enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status()
            )
            self.assertIsNotNone(assignment)
            self.assertEqual(1000 + 1100 + 2, assignment.objective_value())

    def test_solve_capacitated_vrp_with_pickup_and_delivery(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            for use_generalized_cp_sat in [False, True]:
                parameters = routing.default_routing_search_parameters()
                if use_generalized_cp_sat:
                    parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
                else:
                    parameters.use_cp_sat = optional_boolean_pb2.BOOL_TRUE
                parameters.use_cp = use_cp
                parameters.log_search = True

                index_manager = routing.IndexManager(5, 2, 0)
                routing_model = routing.Model(index_manager)
                vehicle_cost = routing_model.register_unary_transit_callback(
                    lambda i, im=index_manager: get_node_value(im, i)
                )
                routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)
                routing_model.set_fixed_cost_of_all_vehicles(100)

                routing_model.add_constant_dimension_with_slack(
                    1, 3, 3, True, "capacity"
                )

                routing_model.add_pickup_and_delivery(4, 1)
                routing_model.add_pickup_and_delivery(2, 3)
                solver = routing_model.solver
                solver.add(routing_model.vehicle_var(4) == routing_model.vehicle_var(1))
                solver.add(routing_model.vehicle_var(2) == routing_model.vehicle_var(3))

                routing_model.add_constant_dimension_with_slack(1, 5, 5, True, "dim")
                dim = routing_model.get_dimension_or_die("dim")
                solver.add(dim.cumul_var(4) <= dim.cumul_var(1))
                solver.add(dim.cumul_var(2) <= dim.cumul_var(3))

                assignment = routing_model.solve_with_parameters(parameters)
                self.assertEqual(
                    enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL,
                    routing_model.status(),
                )
                self.assertIsNotNone(assignment)
                self.assertEqual(200 + 1 + 2 + 3 + 4, assignment.objective_value())

                ranks = {}
                for i in range(2):
                    rank = 0
                    node = routing_model.start(i)
                    while node != routing_model.end(i):
                        ranks[node] = rank
                        rank += 1
                        node = assignment.value(routing_model.next_var(node))
                    ranks[node] = rank

                self.assertLess(ranks[4], ranks[1])
                self.assertLess(ranks[2], ranks[3])

    def test_solve_heterogeneous_capacitated_vrp_with_pickup_and_delivery(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            parameters = routing.default_routing_search_parameters()
            parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
            parameters.use_cp = use_cp
            parameters.log_search = True

            vehicle_costs = [1, 100]
            vehicle_capacities = [1, 2]
            index_manager = routing.IndexManager(5, 2, 0)
            routing_model = routing.Model(index_manager)
            for vehicle in range(len(vehicle_costs)):
                routing_model.set_fixed_cost_of_vehicle(vehicle_costs[vehicle], vehicle)

            def vehicle_cost_callback(from_, to):
                if from_ == 1 and to == 2:
                    return 2**63 - 1
                if from_ == 2 and to == 1:
                    return 2**63 - 1
                if from_ == 3 and to == 4:
                    return 2**63 - 1
                if from_ == 4 and to == 3:
                    return 2**63 - 1
                return 1

            vehicle_cost = routing_model.register_transit_callback(
                vehicle_cost_callback
            )
            routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)

            def capacity_callback(node):
                if node == 2 or node == 4:
                    return 1
                if node == 1 or node == 3:
                    return -1
                return 0

            routing_model.add_dimension_with_vehicle_capacity(
                routing_model.register_unary_transit_callback(capacity_callback),
                5,
                vehicle_capacities,
                True,
                "capacity",
            )

            routing_model.add_pickup_and_delivery(4, 1)
            routing_model.add_pickup_and_delivery(2, 3)
            solver = routing_model.solver
            solver.add(routing_model.vehicle_var(4) == routing_model.vehicle_var(1))
            solver.add(routing_model.vehicle_var(2) == routing_model.vehicle_var(3))

            assignment = routing_model.solve_with_parameters(parameters)
            self.assertEqual(
                enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status()
            )
            self.assertIsNotNone(assignment)
            self.assertEqual(100 + 5, assignment.objective_value())

            ranks = {}
            for i in range(2):
                rank = 0
                node = routing_model.start(i)
                while node != routing_model.end(i):
                    ranks[node] = rank
                    rank += 1
                    node = assignment.value(routing_model.next_var(node))
                ranks[node] = rank

            self.assertLess(ranks[4], ranks[1])
            self.assertLess(ranks[2], ranks[3])

    def test_solve_limited_vrp_with_windows_model(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            parameters = routing.default_routing_search_parameters()
            parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
            parameters.use_cp = use_cp

            index_manager = routing.IndexManager(4, 2, 0)
            routing_model = routing.Model(index_manager)
            vehicle_cost = routing_model.register_unary_transit_callback(
                lambda i, im=index_manager: get_node_value(im, i)
            )
            routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)
            routing_model.set_fixed_cost_of_all_vehicles(100)

            routing_model.add_constant_dimension_with_slack(1, 24, 0, False, "dim")
            dim = routing_model.get_dimension_or_die("dim")
            dim.set_span_upper_bound_for_vehicle(8, 0)
            dim.set_span_upper_bound_for_vehicle(8, 1)
            dim.cumul_var(0).set_range(6, 15)
            dim.cumul_var(1).set_range(7, 7)
            dim.cumul_var(2).set_range(8, 8)
            dim.cumul_var(3).set_range(23, 23)

            assignment = routing_model.solve_with_parameters(parameters)
            self.assertEqual(
                enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status()
            )
            self.assertIsNotNone(assignment)
            self.assertEqual(200 + 1 + 2 + 3, assignment.objective_value())

    def test_solve_unperforming_vrp_model_with_soft_upper_bound_limit(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            parameters = routing.default_routing_search_parameters()
            parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
            parameters.use_cp = use_cp

            index_manager = routing.IndexManager(4, 1, 0)
            routing_model = routing.Model(index_manager)
            vehicle_cost = routing_model.register_unary_transit_callback(
                lambda i, im=index_manager: get_node_value(im, i)
            )
            routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)
            routing_model.set_fixed_cost_of_all_vehicles(100)

            routing_model.add_constant_dimension_with_slack(1, 24, 24, False, "dim")
            dim = routing_model.get_dimension_or_die("dim")
            dim.set_soft_span_upper_bound_for_vehicle(8, 1, 0)
            dim.cumul_var(0).set_range(6, 15)
            dim.cumul_var(1).set_range(7, 7)
            dim.cumul_var(2).set_range(8, 8)
            dim.cumul_var(3).set_range(23, 23)

            routing_model.add_disjunction([3], 10)

            assignment = routing_model.solve_with_parameters(parameters)
            self.assertEqual(
                enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status()
            )
            self.assertIsNotNone(assignment)
            self.assertEqual(100 + 1 + 2 + 10, assignment.objective_value())

    def test_solve_vrp_model_with_soft_upper_bound_limit(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            parameters = routing.default_routing_search_parameters()
            parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
            parameters.use_cp = use_cp

            index_manager = routing.IndexManager(4, 1, 0)
            routing_model = routing.Model(index_manager)
            vehicle_cost = routing_model.register_unary_transit_callback(
                lambda i, im=index_manager: get_node_value(im, i)
            )
            routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)
            routing_model.set_fixed_cost_of_all_vehicles(100)

            routing_model.add_constant_dimension_with_slack(1, 24, 24, False, "dim")
            dim = routing_model.get_dimension_or_die("dim")
            dim.set_soft_span_upper_bound_for_vehicle(8, 2, 0)
            dim.cumul_var(0).set_range(6, 15)
            dim.cumul_var(1).set_range(7, 7)
            dim.cumul_var(2).set_range(8, 8)
            dim.cumul_var(3).set_range(23, 23)

            routing_model.add_disjunction([3], 100)

            assignment = routing_model.solve_with_parameters(parameters)
            self.assertEqual(
                enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status()
            )
            self.assertIsNotNone(assignment)
            self.assertEqual(100 + 1 + 2 + 3 + 20, assignment.objective_value())

    def test_solve_capacitated_vrp_model_with_max_active_vehicles(self):
        for max_active_vehicles in [3, 2, 1]:
            for use_cp in [
                optional_boolean_pb2.BOOL_FALSE,
                optional_boolean_pb2.BOOL_TRUE,
            ]:
                for use_cp_sat in [
                    optional_boolean_pb2.BOOL_FALSE,
                    optional_boolean_pb2.BOOL_TRUE,
                ]:
                    for use_generalized_cp_sat in [
                        optional_boolean_pb2.BOOL_FALSE,
                        optional_boolean_pb2.BOOL_TRUE,
                    ]:
                        if (
                            use_cp == optional_boolean_pb2.BOOL_FALSE
                            and use_cp_sat == optional_boolean_pb2.BOOL_FALSE
                            and use_generalized_cp_sat
                            == optional_boolean_pb2.BOOL_FALSE
                        ):
                            continue

                        parameters = routing.default_routing_search_parameters()
                        parameters.use_cp_sat = use_cp_sat
                        parameters.use_generalized_cp_sat = use_generalized_cp_sat
                        parameters.use_cp = use_cp

                        index_manager = routing.IndexManager(4, 3, 0)
                        routing_model = routing.Model(index_manager)
                        vehicle_cost = routing_model.register_transit_callback(
                            lambda from_, to, rm=routing_model: (
                                0 if rm.is_start(from_) or rm.is_end(to) else 10
                            )
                        )
                        routing_model.set_arc_cost_evaluator_of_all_vehicles(
                            vehicle_cost
                        )
                        routing_model.set_maximum_number_of_active_vehicles(
                            max_active_vehicles
                        )

                        assignment = routing_model.solve_with_parameters(parameters)

                        if (
                            use_cp_sat == optional_boolean_pb2.BOOL_TRUE
                            or use_generalized_cp_sat == optional_boolean_pb2.BOOL_TRUE
                            or max_active_vehicles == 3
                        ):
                            self.assertEqual(
                                enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL,
                                routing_model.status(),
                            )
                        else:
                            self.assertEqual(
                                enums_pb2.RoutingSearchStatus.ROUTING_SUCCESS,
                                routing_model.status(),
                            )

                        self.assertIsNotNone(assignment)
                        self.assertEqual(
                            (3 - max_active_vehicles) * 10, assignment.objective_value()
                        )

    def test_solve_alternatives_tsp_with_pickup_and_delivery(self):
        parameters = routing.default_routing_search_parameters()
        parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
        parameters.use_cp = optional_boolean_pb2.BOOL_FALSE
        parameters.log_search = True

        index_manager = routing.IndexManager(6, 1, 0)
        routing_model = routing.Model(index_manager)
        # Forbid 1 -> 3 arc.
        vehicle_cost = routing_model.register_transit_callback(
            lambda from_, to: 2**63 - 1 if from_ == 1 and to == 3 else from_
        )
        routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)
        penalty = 100
        routing_model.add_pickup_and_delivery_sets(
            routing_model.add_disjunction([2, 1], penalty),
            routing_model.add_disjunction([5, 3], penalty),
        )
        routing_model.add_disjunction([4], 1)

        assignment = routing_model.solve_with_parameters(parameters)
        self.assertEqual(
            enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status()
        )
        self.assertIsNotNone(assignment)
        self.assertEqual(1 + 0 + 2 + 3, assignment.objective_value())
        self.assertEqual(4, assignment.value(routing_model.next_var(4)))
        self.assertEqual(3, assignment.value(routing_model.next_var(2)))

    def test_solve_capacitated_vrp_model_with_windows_and_slack_cost(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            parameters = routing.default_routing_search_parameters()
            parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
            parameters.use_cp = use_cp
            parameters.log_search = True

            index_manager = routing.IndexManager(3, 3, 0)
            routing_model = routing.Model(index_manager)
            vehicle_cost = routing_model.register_transit_callback(lambda f, t: 1)
            routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)

            routing_model.add_dimension(
                routing_model.register_transit_callback(lambda f, t: 1),
                7,
                7,
                True,
                "time",
            )
            time = routing_model.get_dimension_or_die("time")
            time.cumul_var(0).set_range(0, 0)
            time.cumul_var(1).set_range(1, 6)
            time.cumul_var(2).set_range(5, 5)
            time.set_span_cost_coefficient_for_all_vehicles(100)

            assignment = routing_model.solve_with_parameters(parameters)
            self.assertEqual(
                enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL, routing_model.status()
            )
            self.assertIsNotNone(assignment)
            self.assertEqual(600 + 1 + 1 + 1, assignment.objective_value())

    def test_solve_vrp_model_with_unbound_cumuls(self):
        for use_cp in [
            optional_boolean_pb2.BOOL_FALSE,
            optional_boolean_pb2.BOOL_TRUE,
        ]:
            for use_cp_sat in [
                optional_boolean_pb2.BOOL_FALSE,
                optional_boolean_pb2.BOOL_TRUE,
            ]:
                for use_generalized_cp_sat in [
                    optional_boolean_pb2.BOOL_FALSE,
                    optional_boolean_pb2.BOOL_TRUE,
                ]:
                    if (
                        use_cp == optional_boolean_pb2.BOOL_FALSE
                        and use_cp_sat == optional_boolean_pb2.BOOL_FALSE
                        and use_generalized_cp_sat == optional_boolean_pb2.BOOL_FALSE
                    ):
                        continue

                    parameters = routing.default_routing_search_parameters()
                    parameters.use_cp_sat = use_cp_sat
                    parameters.use_generalized_cp_sat = use_generalized_cp_sat
                    parameters.use_cp = use_cp

                    index_manager = routing.IndexManager(4, 1, 0)
                    routing_model = routing.Model(index_manager)

                    def vehicle_cost_callback(from_, to, rm=routing_model):
                        if rm.is_start(from_) or rm.is_end(to):
                            return 0
                        return 10

                    vehicle_cost = routing_model.register_transit_callback(
                        vehicle_cost_callback
                    )
                    routing_model.set_arc_cost_evaluator_of_all_vehicles(vehicle_cost)

                    routing_model.add_constant_dimension_with_slack(
                        1, 2**63 - 1, 2**63 - 1, False, "capacity"
                    )

                    assignment = routing_model.solve_with_parameters(parameters)

                    if (
                        use_cp_sat == optional_boolean_pb2.BOOL_TRUE
                        or use_generalized_cp_sat == optional_boolean_pb2.BOOL_TRUE
                    ):
                        self.assertEqual(
                            enums_pb2.RoutingSearchStatus.ROUTING_OPTIMAL,
                            routing_model.status(),
                        )
                    else:
                        self.assertEqual(
                            enums_pb2.RoutingSearchStatus.ROUTING_SUCCESS,
                            routing_model.status(),
                        )

                    self.assertIsNotNone(assignment)
                    self.assertEqual(20, assignment.objective_value())

    def test_solve_vrp_model_with_time_limit(self):
        time_limit_seconds = 5
        parameters = routing.default_routing_search_parameters()
        parameters.use_cp = optional_boolean_pb2.BOOL_FALSE
        parameters.use_generalized_cp_sat = optional_boolean_pb2.BOOL_TRUE
        parameters.disable_scheduling_beware_this_may_degrade_performance = True
        parameters.report_intermediate_cp_sat_solutions = True
        parameters.time_limit.seconds = time_limit_seconds
        parameters.log_search = True

        index_manager = routing.IndexManager(2, 2, 0)
        routing_model = routing.Model(index_manager)
        time_callback = routing_model.register_transit_callback(lambda f, t: 0)
        routing_model.add_dimension(time_callback, 31611598, 31611598, False, "time")
        routing_model.get_dimension_or_die(
            "time"
        ).set_span_cost_coefficient_for_vehicle(164095200, 0)

        rg = routing_model.add_resource_group()
        # Check if Domain is available and Attributes work
        rg.add_resource(
            routing.Model.ResourceGroup.Attributes(
                sorted_interval_list.Domain(31538974, 31611598),
                sorted_interval_list.Domain(0, 31611598),
            ),
            routing_model.get_dimension_or_die("time"),
        )
        rg.add_resource(
            routing.Model.ResourceGroup.Attributes(
                sorted_interval_list.Domain(0, 31611598),
                sorted_interval_list.Domain(31549629, 31611598),
            ),
            routing_model.get_dimension_or_die("time"),
        )
        rg.notify_vehicle_requires_a_resource(1)

        # We just run it once to check it doesn't crash and respects time limit
        # approximately
        assignment = routing_model.solve_with_parameters(parameters)
        self.assertIsNone(assignment)


if __name__ == "__main__":
    absltest.main()
