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

"""High-level tests for max_flow.

Do not test edge cases here, as they are tested in the C++ unit tests. Here,
we just test the interface of the Python module.
"""

from absl.testing import absltest

from ortools.graph.python import max_flow


class SimpleMaxFlowTest(absltest.TestCase):

    def test_max_flow(self) -> None:
        flow = max_flow.SimpleMaxFlow()
        arc = flow.add_arc_with_capacity(tail=0, head=1, capacity=2)
        flow.add_arc_with_capacity(tail=1, head=2, capacity=2)

        self.assertEqual(flow.num_nodes(), 3)
        self.assertEqual(flow.num_arcs(), 2)
        self.assertEqual(flow.solve(source=0, sink=2), flow.OPTIMAL)
        self.assertEqual(flow.optimal_flow(), 2)
        self.assertEqual(flow.flow(arc), 2)


if __name__ == "__main__":
    absltest.main()
