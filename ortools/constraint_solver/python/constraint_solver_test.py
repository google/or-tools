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

"""Test for constraint_solver pybind11 layer."""

from absl.testing import absltest
from ortools.constraint_solver.python import constraint_solver


class ConstraintSolverTest(absltest.TestCase):

    def test_create_solver(self):
        print("test_create_solver")
        solver = constraint_solver.Solver("test_create_solver")
        print(solver)

    def test_create_int_var(self):
        print("test_create_int_var")
        solver = constraint_solver.Solver("test_create_int_var")
        x = solver.new_int_var(0, 10, "x")
        self.assertEqual(str(x), "x(0..10)")
        self.assertEqual(x.min, 0)
        self.assertEqual(x.max, 10)
        self.assertEqual(x.name, "x")

        y = solver.new_int_var([0, 2, 4])
        self.assertEqual(y.min, 0)
        self.assertEqual(y.max, 4)
        self.assertEmpty(y.name)
        y.name = "y"
        self.assertEqual(y.name, "y")

    def test_create_int_expr(self):
        print("test_create_int_expr")
        solver = constraint_solver.Solver("test_create_int_expr")
        x = solver.new_int_var(0, 10, "x")
        y = solver.new_int_var(0, 10, "y")

        x_plus_3 = x + 3
        self.assertEqual(str(x_plus_3), "(x(0..10) + 3)")
        print(x_plus_3)
        self.assertEqual(x_plus_3.min, 3)
        self.assertEqual(x_plus_3.max, 13)

        self.assertEqual(str(x * 5), "(x(0..10) * 5)")
        self.assertEqual(str(x + y), "(x(0..10) + y(0..10))")
        self.assertEqual(str(2 + x), "(x(0..10) + 2)")
        self.assertEqual(str(7 * x), "(x(0..10) * 7)")
        self.assertEqual(str(x * y), "(x(0..10) * y(0..10))")
        self.assertEqual(str(x + 2 * y + 5), "((x(0..10) + (y(0..10) * 2)) + 5)")

    def test_fail_outside_solve(self):
        print("test_fail_outside_solve")
        solver = constraint_solver.Solver("test_fail_outside_solve")
        x = solver.new_int_var(0, 10, "x")
        try:
            x.set_min(20)
        except ValueError:
            print("  fail caught")

    def test_rabbits_pheasants(self):
        print("test_rabbits_pheasants")
        solver = constraint_solver.Solver("test_rabbits_pheasants")
        rabbits = solver.new_int_var(0, 20, "rabbits")
        pheasants = solver.new_int_var(0, 20, "pheasants")
        solver.add(rabbits + pheasants == 20)
        solver.add(4 * rabbits + 2 * pheasants == 56)
        solver.accept(solver.print_model_visitor())


if __name__ == "__main__":
    absltest.main()
