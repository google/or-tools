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
from typing import Dict

from absl.testing import absltest
from ortools.math_opt.python import indicator_constraints
from ortools.math_opt.python import model
from ortools.math_opt.python import variables


def _terms_dict(
    con: indicator_constraints.IndicatorConstraint,
) -> Dict[variables.Variable, float]:
    return {term.variable: term.coefficient for term in con.terms()}


class IndicatorConstraintsTest(absltest.TestCase):

    def test_getters_empty(self) -> None:
        mod = model.Model()
        con = mod.add_indicator_constraint()
        self.assertIsNone(con.indicator_variable)
        self.assertEmpty(list(con.terms()))
        self.assertEqual(con.lower_bound, -math.inf)
        self.assertEqual(con.upper_bound, math.inf)
        self.assertFalse(con.activate_on_zero)
        self.assertEqual(con.name, "")
        bounded_expr = con.get_implied_constraint()
        self.assertEqual(bounded_expr.lower_bound, -math.inf)
        self.assertEqual(bounded_expr.upper_bound, math.inf)
        expr = variables.as_flat_linear_expression(bounded_expr.expression)
        self.assertEqual(expr.offset, 0.0)
        self.assertEmpty(expr.terms)

    def test_getters_nonempty(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable()
        y = mod.add_variable()
        z = mod.add_variable()
        con = mod.add_indicator_constraint(
            indicator=x,
            activate_on_zero=True,
            implied_constraint=2 * y + z == 3.0,
            name="c123",
        )
        self.assertEqual(con.indicator_variable, x)
        self.assertDictEqual(_terms_dict(con), {y: 2.0, z: 1.0})
        self.assertEqual(con.lower_bound, 3.0)
        self.assertEqual(con.upper_bound, 3.0)
        self.assertTrue(con.activate_on_zero)
        self.assertEqual(con.name, "c123")
        self.assertEqual(con.get_coefficient(y), 2.0)
        self.assertEqual(con.get_coefficient(x), 0.0)

        bounded_expr = con.get_implied_constraint()
        self.assertEqual(bounded_expr.lower_bound, 3.0)
        self.assertEqual(bounded_expr.upper_bound, 3.0)
        expr = variables.as_flat_linear_expression(bounded_expr.expression)
        self.assertEqual(expr.offset, 0.0)
        self.assertEqual(expr.terms, {y: 2.0, z: 1.0})

    def test_create_by_attrs(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable()
        y = mod.add_variable()
        z = mod.add_variable()
        con = mod.add_indicator_constraint(
            indicator=x,
            activate_on_zero=True,
            implied_lb=4.0,
            implied_ub=5.0,
            implied_expr=10 * y + 9 * z + 3.0,
            name="c123",
        )
        self.assertEqual(con.indicator_variable, x)
        self.assertDictEqual(_terms_dict(con), {y: 10.0, z: 9.0})
        self.assertEqual(con.lower_bound, 1.0)
        self.assertEqual(con.upper_bound, 2.0)
        self.assertTrue(con.activate_on_zero)
        self.assertEqual(con.name, "c123")

    def test_get_coefficient_wrong_model(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        mod2 = model.Model()
        con = mod2.add_indicator_constraint()
        with self.assertRaises(ValueError):
            con.get_coefficient(x)

    def test_eq(self) -> None:
        mod_a = model.Model()
        con_a1 = mod_a.add_indicator_constraint()
        con_a2 = mod_a.add_indicator_constraint()
        con_a1_alt = mod_a.get_indicator_constraint(0)

        mod_b = model.Model()
        con_b1 = mod_b.add_indicator_constraint()

        self.assertEqual(con_a1, con_a1)
        self.assertEqual(con_a1, con_a1_alt)
        self.assertNotEqual(con_a1, con_a2)
        self.assertNotEqual(con_a1, con_b1)
        self.assertNotEqual(con_a1, "cat")

    def test_hash_no_crash(self) -> None:
        mod_a = model.Model()
        con = mod_a.add_indicator_constraint()
        self.assertIsInstance(hash(con), int)


if __name__ == "__main__":
    absltest.main()
