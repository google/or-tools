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

"""Test of the fp_min_cost_flow Python wrapper."""

import math
import numpy as np
from absl.testing import absltest
from ortools.graph.python import fp_min_cost_flow


class FpMinCostFlowTest(absltest.TestCase):

    def test_success(self) -> None:
        """Test a successful solve.

        We use the same test as scenario as the
        SimpleFloatingPointMinCostFlowTest.Assignment test in
        fp_min_cost_flow_test.cc.
        """
        solver = fp_min_cost_flow.SimpleFloatingPointMinCostFlow()
        solver.set_node_supply(node=0, supply=4.0)
        solver.set_node_supply(node=1, supply=3.0)
        solver.set_node_supply(node=2, supply=4.0)
        solver.set_node_supply(node=3, supply=-5.0)
        solver.set_node_supply(node=4, supply=-4.0)
        a03 = solver.add_arc_with_capacity_and_unit_cost(
            tail=0, head=3, capacity=5.1, unit_cost=2
        )
        a13 = solver.add_arc_with_capacity_and_unit_cost(
            tail=1, head=3, capacity=4.0, unit_cost=1
        )
        a14 = solver.add_arc_with_capacity_and_unit_cost(
            tail=1, head=4, capacity=5.0, unit_cost=2
        )
        a24 = solver.add_arc_with_capacity_and_unit_cost(
            tail=2, head=4, capacity=3.0, unit_cost=3
        )

        status = solver.solve_max_flow_with_min_cost()
        self.assertEqual(
            status, fp_min_cost_flow.SimpleFloatingPointMinCostFlow.Status.OPTIMAL
        )

        self.assertEqual(solver.flow(a03), 3)
        self.assertEqual(solver.flow(a13), 2)
        self.assertEqual(solver.flow(a14), 1)
        self.assertEqual(solver.flow(a24), 3)

    def test_success_with_numpy(self) -> None:
        """Test a successful solve using numpy inputs/outputs.

        Same test as test_success() but with numpy vector functions.
        """
        solver = fp_min_cost_flow.SimpleFloatingPointMinCostFlow()
        solver.set_nodes_supplies(
            nodes=np.arange(5), supplies=np.array([4.0, 3.0, 4.0, -5.0, -4.0])
        )
        arcs = solver.add_arcs_with_capacity_and_unit_cost(
            tails=np.array([0, 1, 1, 2]),
            heads=np.array([3, 3, 4, 4]),
            capacities=np.array([5.1, 4.0, 5.0, 3.0]),
            unit_costs=np.array([2, 1, 2, 3]),
        )

        status = solver.solve_max_flow_with_min_cost()
        self.assertEqual(
            status, fp_min_cost_flow.SimpleFloatingPointMinCostFlow.Status.OPTIMAL
        )

        np.testing.assert_array_equal(
            solver.flows(arcs), np.array([3.0, 2.0, 1.0, 3.0])
        )

    def test_failure(self) -> None:
        """Test a failing solve.

        We use an infinite supply on a node, which is not supported.
        """
        solver = fp_min_cost_flow.SimpleFloatingPointMinCostFlow()
        solver.set_node_supply(node=0, supply=math.inf)
        status = solver.solve_max_flow_with_min_cost()
        self.assertEqual(
            status,
            fp_min_cost_flow.SimpleFloatingPointMinCostFlow.Status.BAD_CAPACITY_RANGE,
        )

    def test_solve_stats(self) -> None:
        solve_stats = fp_min_cost_flow.SimpleFloatingPointMinCostFlow.SolveStats()
        solve_stats.scale = 0.1
        solve_stats.num_tested_scales = 3
        self.assertEqual(
            repr(solve_stats), "SolveStats{ scale: 0.1, num_tested_scales: 3 }"
        )
        self.assertEqual(str(solve_stats), "{ scale: 0.1, num_tested_scales: 3 }")

    def test_last_solve_stats(self) -> None:
        """Test getting the last solve statistics."""
        solver = fp_min_cost_flow.SimpleFloatingPointMinCostFlow()
        solver.set_node_supply(node=0, supply=2.0)

        status = solver.solve_max_flow_with_min_cost()
        self.assertEqual(
            status, fp_min_cost_flow.SimpleFloatingPointMinCostFlow.Status.OPTIMAL
        )

        solve_stats = solver.last_solve_stats()
        self.assertIsInstance(
            solve_stats, fp_min_cost_flow.SimpleFloatingPointMinCostFlow.SolveStats
        )

        # 2.0**61 is the last power of two such as:
        #
        #  (2.0 * 2.0**61) < 2**63 - 1
        #
        # This is the largest scale we expect to use.
        self.assertEqual(solve_stats.scale, 2.0**61)

        # The code should have directly tested the correct largest scale.
        self.assertEqual(solve_stats.num_tested_scales, 1)


if __name__ == "__main__":
    absltest.main()
