#!/usr/bin/env python3
# This Python file uses the following encoding: utf-8
# Copyright 2018 Google LLC
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
import unittest

from ortools.linear_solver import pywraplp

class TestSiriusXpress(unittest.TestCase):
    def test_lp(self):
        # max   x + 2y
        # st.  -x +  y <= 1
        #      2x + 3y <= 12
        #      3x + 2y <= 12
        #       x ,  y \in R+
        params = [
            (
                "SIRIUS_LINEAR_PROGRAMMING",
                pywraplp.Solver.SIRIUS_LINEAR_PROGRAMMING
            ),
            (
                "XPRESS_LINEAR_PROGRAMMING",
                pywraplp.Solver.XPRESS_LINEAR_PROGRAMMING
            ),
        ]
        for name, problem_type in params:
            with self.subTest(name):
                solver = pywraplp.Solver(
                    name,
                    problem_type
                )
                # Create the two variables and let them take on any value.
                x = solver.NumVar(-solver.infinity(), solver.infinity(), 'x')
                y = solver.NumVar(-solver.infinity(), solver.infinity(), 'y')

                # Objective function: Maximize x + 2y.
                objective = solver.Objective()
                objective.SetCoefficient(x, 1)
                objective.SetCoefficient(y, 2)
                objective.SetMaximization()

                # Constraint 0: -x + y <= 1.
                constraint0 = solver.Constraint(-solver.infinity(), 1)
                constraint0.SetCoefficient(x, -1)
                constraint0.SetCoefficient(y, 1)

                # Constraint 2: 3x + 2y <= 12.
                constraint1 = solver.Constraint(-solver.infinity(), 12)
                constraint1.SetCoefficient(x, 3)
                constraint1.SetCoefficient(y, 2)

                # Constraint 1: 2x + 3y <= 12.
                constraint2 = solver.Constraint(-solver.infinity(), 12)
                constraint2.SetCoefficient(x, 2)
                constraint2.SetCoefficient(y, 3)

                self.assertEqual(solver.NumVariables(), 2)
                self.assertEqual(solver.NumConstraints(), 3)

                # Solve the system.
                status = solver.Solve()
                self.assertEqual(status, pywraplp.Solver.OPTIMAL)

                self.assertAlmostEqual(x.solution_value(), 1.8)
                self.assertAlmostEqual(y.solution_value(), 2.8)
                self.assertAlmostEqual(objective.Value(), 7.4)
                self.assertAlmostEqual(x.reduced_cost(), 0)
                self.assertAlmostEqual(y.reduced_cost(), 0)

                activities = solver.ComputeConstraintActivities()
                self.assertAlmostEqual(activities[constraint0.index()] , 1)
                self.assertAlmostEqual(activities[constraint1.index()] , 11)
                self.assertAlmostEqual(activities[constraint2.index()] , 12)

                if name == "XPRESS_LINEAR_PROGRAMMING":
                    self.assertAlmostEqual(constraint0.dual_value(), 0.2)
                    self.assertAlmostEqual(constraint1.dual_value(), 0)
                    self.assertAlmostEqual(constraint2.dual_value(), 0.6)
                else:
                    self.assertAlmostEqual(constraint0.dual_value(), -0.2)
                    self.assertAlmostEqual(constraint1.dual_value(), -0)
                    self.assertAlmostEqual(constraint2.dual_value(), -0.6)

    def test_mip(self):
        # max   x + 2y
        # st.  -x +  y <= 1
        #      2x + 3y <= 12
        #      3x + 2y <= 12
        #       x ,  y >= 0
        #       x ,  y \in Z
        params = [
            (
                "SIRIUS_MIXED_INTEGER_PROGRAMMING",
                pywraplp.Solver.SIRIUS_MIXED_INTEGER_PROGRAMMING
            ),
            (
                "XPRESS_MIXED_INTEGER_PROGRAMMING",
                pywraplp.Solver.XPRESS_MIXED_INTEGER_PROGRAMMING
            ),
        ]
        for name, problem_type in params:
            with self.subTest(name):
                solver = pywraplp.Solver(
                    name,
                    problem_type
                )
                # Create the two variables and let them take on any integer value.
                x = solver.IntVar(0, solver.infinity(), 'x')
                y = solver.IntVar(0, solver.infinity(), 'y')

                # Objective function: Maximize x + 2y.
                objective = solver.Objective()
                objective.SetCoefficient(x, 1)
                objective.SetCoefficient(y, 2)
                objective.SetMaximization()

                # Constraint 0: -x + y <= 1.
                constraint0 = solver.Constraint(-solver.infinity(), 1)
                constraint0.SetCoefficient(x, -1)
                constraint0.SetCoefficient(y, 1)

                # Constraint 2: 3x + 2y <= 12.
                constraint1 = solver.Constraint(-solver.infinity(), 12)
                constraint1.SetCoefficient(x, 3)
                constraint1.SetCoefficient(y, 2)

                # Constraint 1: 2x + 3y <= 12.
                constraint2 = solver.Constraint(-solver.infinity(), 12)
                constraint2.SetCoefficient(x, 2)
                constraint2.SetCoefficient(y, 3)

                self.assertEqual(solver.NumVariables(), 2)
                self.assertEqual(solver.NumConstraints(), 3)

                # Solve the system.
                status = solver.Solve()

                if name == "XPRESS_MIXED_INTEGER_PROGRAMMING":
                    self.assertEqual(status, pywraplp.Solver.OPTIMAL)
                    self.assertEqual(x.solution_value(), 2)
                    self.assertAlmostEqual(y.solution_value(), 2)
                    self.assertAlmostEqual(objective.Value(), 6)
                    self.assertAlmostEqual(objective.BestBound(), 6)

if __name__ == '__main__':
    unittest.main()
