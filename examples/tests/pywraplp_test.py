#!/usr/bin/env python3
# Copyright 2010-2021 Google LLC
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
"""Simple unit tests for python/linear_solver.swig. Not exhaustive."""


import unittest
from ortools.linear_solver import linear_solver_pb2
from ortools.linear_solver import pywraplp
from google.protobuf import text_format

TEXT_MODEL = """
variable {
  lower_bound: 1.0
  upper_bound: 10.0
  objective_coefficient: 2.0
}
variable {
  lower_bound: 1.0
  upper_bound: 10.0
  objective_coefficient: 1.0
}
constraint {
  lower_bound: -10000.0
  upper_bound: 4.0
  var_index: 0
  var_index: 1
  coefficient: 1.0
  coefficient: 2.0
}
"""


class PyWrapLp(unittest.TestCase):

    def test_proto(self):
        input_proto = linear_solver_pb2.MPModelProto()
        text_format.Merge(TEXT_MODEL, input_proto)
        solver = pywraplp.Solver('solveFromProto',
                                 pywraplp.Solver.CBC_MIXED_INTEGER_PROGRAMMING)
        # For now, create the model from the proto by parsing the proto
        errors = solver.LoadModelFromProto(input_proto)
        self.assertFalse(errors)
        solver.Solve()
        # Fill solution
        solution = linear_solver_pb2.MPSolutionResponse()
        solver.FillSolutionResponseProto(solution)
        self.assertEqual(solution.objective_value, 3.0)
        self.assertEqual(solution.variable_value[0], 1.0)
        self.assertEqual(solution.variable_value[1], 1.0)
        self.assertEqual(solution.best_objective_bound, 3.0)

    def test_external_api(self):
        solver = pywraplp.Solver('TestExternalAPI',
                                 pywraplp.Solver.GLOP_LINEAR_PROGRAMMING)
        infinity = solver.Infinity()
        infinity2 = solver.infinity()
        self.assertEqual(infinity, infinity2)
        # x1, x2 and x3 are continuous non-negative variables.
        x1 = solver.NumVar(0.0, infinity, 'x1')
        x2 = solver.NumVar(0.0, infinity, 'x2')
        x3 = solver.NumVar(0.0, infinity, 'x3')
        self.assertEqual(x1.Lb(), 0)
        self.assertEqual(x1.Ub(), infinity)
        self.assertFalse(x1.Integer())
        solver.Maximize(10 * x1 + 6 * x2 + 4 * x3 + 5)
        self.assertEqual(solver.Objective().Offset(), 5)
        c0 = solver.Add(10 * x1 + 4 * x2 + 5 * x3 <= 600, 'ConstraintName0')
        c1 = solver.Add(2 * x1 + 2 * x2 + 6 * x3 <= 300)
        sum_of_vars = sum([x1, x2, x3])
        solver.Add(sum_of_vars <= 100.0, 'OtherConstraintName')
        self.assertEqual(c1.Lb(), -infinity)
        self.assertEqual(c1.Ub(), 300)
        c1.SetLb(-100000)
        self.assertEqual(c1.Lb(), -100000)
        c1.SetUb(301)
        self.assertEqual(c1.Ub(), 301)

        solver.SetTimeLimit(10000)
        result_status = solver.Solve()

        # The problem has an optimal solution.
        self.assertEqual(result_status, pywraplp.Solver.OPTIMAL)
        self.assertAlmostEqual(x1.ReducedCost(), 0.0)
        self.assertAlmostEqual(c0.DualValue(), 0.6666666666666667)


if __name__ == '__main__':
    unittest.main()
