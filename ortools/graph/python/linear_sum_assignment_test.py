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

"""High-level tests for linear_sum_assignment.

Do not test edge cases here, as they are tested in the C++ unit tests. Here,
we just test the interface of the Python module.
"""

from absl.testing import absltest

from ortools.graph.python import linear_sum_assignment


class SimpleLinearSumAssignmentTest(absltest.TestCase):

    def test_linear_sum_assignment(self) -> None:
        assignment = linear_sum_assignment.SimpleLinearSumAssignment()
        for left_node, right_node, cost in ((0, 0, 1), (0, 1, 5), (1, 0, 2),
                                            (1, 1, 4)):
            assignment.add_arc_with_cost(
                left_node=left_node, right_node=right_node, cost=cost
            )

        self.assertEqual(assignment.num_nodes(), 2)
        self.assertEqual(assignment.num_arcs(), 4)
        self.assertEqual(assignment.solve(), assignment.OPTIMAL)
        self.assertEqual(assignment.optimal_cost(), 5)
        self.assertEqual(
            [assignment.right_mate(i) for i in range(assignment.num_nodes())],
            [0, 1],
        )


if __name__ == "__main__":
    absltest.main()
