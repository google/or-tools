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

"""High-level tests for min_cost_flow.

Do not test edge cases here, as they are tested in the C++ unit tests. Here,
we just test the interface of the Python module.
"""

from absl.testing import absltest

from ortools.graph.python import min_cost_flow


class SimpleMinCostFlowTest(absltest.TestCase):

    def test_min_cost_flow(self) -> None:
        flow = min_cost_flow.SimpleMinCostFlow()
        arc = flow.add_arc_with_capacity_and_unit_cost(
            tail=0, head=1, capacity=2, unit_cost=3
        )
        flow.set_node_supply(node=0, supply=2)
        flow.set_node_supply(node=1, supply=-2)

        self.assertEqual(flow.num_arcs(), 1)
        self.assertEqual(flow.solve(), flow.OPTIMAL)
        self.assertEqual(flow.optimal_cost(), 6)
        self.assertEqual(flow.flow(arc), 2)


if __name__ == "__main__":
    absltest.main()
