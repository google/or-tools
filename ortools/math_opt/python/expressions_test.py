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

from absl.testing import absltest
from ortools.math_opt.python import expressions
from ortools.math_opt.python import model
from ortools.math_opt.python import variables


def _type_check_linear_sum(x: variables.LinearSum) -> None:
    """Does nothing at runtime, forces the type checker to run on x."""
    del x  # Unused.


class FastSumTest(absltest.TestCase):

    def test_variables(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable()
        y = mod.add_binary_variable()
        z = 4
        result = expressions.fast_sum([x, y, z])
        _type_check_linear_sum(result)
        self.assertIsInstance(result, variables.LinearSum)
        result_expr = variables.as_flat_linear_expression(result)
        self.assertEqual(result_expr.offset, 4.0)
        self.assertDictEqual(dict(result_expr.terms), {x: 1.0, y: 1.0})

    def test_numbers(self) -> None:
        result = expressions.fast_sum([2.0, 4.0])
        _type_check_linear_sum(result)
        self.assertIsInstance(result, variables.LinearSum)
        result_expr = variables.as_flat_linear_expression(result)
        self.assertEqual(result_expr.offset, 6.0)
        self.assertEmpty(result_expr.terms)

    def test_heterogeneous_linear(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable()
        result = expressions.fast_sum([2.0, 3.0 * x])
        _type_check_linear_sum(result)
        self.assertIsInstance(result, variables.LinearSum)
        result_expr = variables.as_flat_linear_expression(result)
        self.assertEqual(result_expr.offset, 2.0)
        self.assertDictEqual(dict(result_expr.terms), {x: 3.0})

    def test_heterogeneous_quad(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable()
        result = expressions.fast_sum([2.0, 3.0 * x * x, x])
        self.assertIsInstance(result, variables.QuadraticSum)
        result_expr = variables.as_flat_quadratic_expression(result)
        self.assertEqual(result_expr.offset, 2.0)
        self.assertDictEqual(dict(result_expr.linear_terms), {x: 1.0})
        self.assertDictEqual(
            dict(result_expr.quadratic_terms),
            {variables.QuadraticTermKey(x, x): 3.0},
        )

    def test_all_quad(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable()
        result = expressions.fast_sum([3.0 * x * x, x * x])
        self.assertIsInstance(result, variables.QuadraticSum)
        result_expr = variables.as_flat_quadratic_expression(result)
        self.assertEqual(result_expr.offset, 0.0)
        self.assertEmpty(result_expr.linear_terms)
        self.assertDictEqual(
            dict(result_expr.quadratic_terms),
            {variables.QuadraticTermKey(x, x): 4.0},
        )


class EvaluateExpressionTest(absltest.TestCase):

    def test_scalar_expression(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable()
        sol = {x: 1.0}
        expr = 4.0
        self.assertEqual(expressions.evaluate_expression(expr, sol), 4.0)

    def test_linear(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable()
        y = mod.add_variable()
        sol = {x: 1.0, y: 4.0}
        expr = 3 * x + y + 2.0
        self.assertAlmostEqual(
            expressions.evaluate_expression(expr, sol), 9.0, delta=1.0e-10
        )

    def test_quadratic(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable()
        y = mod.add_variable()
        sol = {x: 1.0, y: 4.0}
        expr = 3 * x * y + y * y + 2.0 * x + 2.0
        self.assertAlmostEqual(
            expressions.evaluate_expression(expr, sol), 32.0, delta=1.0e-10
        )


if __name__ == "__main__":
    absltest.main()
