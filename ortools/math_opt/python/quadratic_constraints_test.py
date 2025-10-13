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

import math

from absl.testing import absltest
from ortools.math_opt.python import model


class QuadraticConstraintsTest(absltest.TestCase):

    def test_empty_constraint(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        quad_con = mod.add_quadratic_constraint()
        self.assertEqual(quad_con.lower_bound, -math.inf)
        self.assertEqual(quad_con.upper_bound, math.inf)
        self.assertEqual(quad_con.id, 0)
        self.assertEqual(quad_con.name, "")
        self.assertEqual(quad_con.get_linear_coefficient(x), 0.0)
        self.assertEqual(quad_con.get_quadratic_coefficient(x, x), 0.0)
        self.assertEmpty(list(quad_con.linear_terms()))
        self.assertEmpty(list(quad_con.quadratic_terms()))

    def test_full_constraint(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        quad_con = mod.add_quadratic_constraint(
            lb=3.0, ub=4.0, expr=5 * x + 6 * y + 7 * x * y + 8 * y * y, name="c"
        )
        self.assertEqual(quad_con.lower_bound, 3.0)
        self.assertEqual(quad_con.upper_bound, 4.0)
        self.assertEqual(quad_con.id, 0)
        self.assertEqual(quad_con.name, "c")
        self.assertEqual(quad_con.get_linear_coefficient(x), 5.0)
        self.assertEqual(quad_con.get_linear_coefficient(y), 6.0)
        self.assertEqual(quad_con.get_quadratic_coefficient(x, y), 7.0)
        self.assertEqual(quad_con.get_quadratic_coefficient(y, y), 8.0)
        self.assertDictEqual(
            {term.variable: term.coefficient for term in quad_con.linear_terms()},
            {x: 5.0, y: 6.0},
        )
        self.assertDictEqual(
            {
                (term.key.first_var, term.key.second_var): term.coefficient
                for term in quad_con.quadratic_terms()
            },
            {(x, y): 7.0, (y, y): 8.0},
        )

    def test_eq(self) -> None:
        mod1 = model.Model()
        mod2 = model.Model()
        q1 = mod1.add_quadratic_constraint()
        q2 = mod1.add_quadratic_constraint()
        q3 = mod2.add_quadratic_constraint()
        q1_other = mod1.get_quadratic_constraint(0)
        self.assertEqual(q1, q1_other)
        self.assertNotEqual(q1, q2)
        self.assertNotEqual(q1, q3)
        self.assertNotEqual(q1, "cat")

    def test_str(self) -> None:
        mod = model.Model()
        quad_con = mod.add_quadratic_constraint(name="qqq")
        self.assertEqual(str(quad_con), "qqq")
        self.assertEqual(repr(quad_con), "<QuadraticConstraint id: 0, name: 'qqq'>")

    def test_get_coefficient_variable_wrong_model(self) -> None:
        mod1 = model.Model()
        mod2 = model.Model()
        q1 = mod1.add_quadratic_constraint()
        # Ensure the bad model, not the bad id, causes the error.
        mod1.add_variable()
        x2 = mod2.add_variable()
        with self.assertRaises(ValueError):
            q1.get_linear_coefficient(x2)
        with self.assertRaises(ValueError):
            q1.get_quadratic_coefficient(x2, x2)


if __name__ == "__main__":
    absltest.main()
