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
from typing import Dict, Tuple

from absl.testing import absltest
from ortools.math_opt.python import bounded_expressions
from ortools.math_opt.python import model
from ortools.math_opt.python import normalized_inequality
from ortools.math_opt.python import variables


class NormalizedLinearInequalityTest(absltest.TestCase):

    def test_init_all_present(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        inequality = normalized_inequality.NormalizedLinearInequality(
            lb=-4.0, expr=2.0 * x + 3.0, ub=8.0
        )
        self.assertAlmostEqual(inequality.lb, -7.0, delta=1e-10)
        self.assertAlmostEqual(inequality.ub, 5.0, delta=1e-10)
        self.assertEqual(inequality.coefficients, {x: 2.0})

    def test_init_all_missing(self) -> None:
        inequality = normalized_inequality.NormalizedLinearInequality(
            lb=None, expr=None, ub=None
        )
        self.assertEqual(inequality.lb, -math.inf)
        self.assertEqual(inequality.ub, math.inf)
        self.assertEmpty(inequality.coefficients)

    def test_init_offset_only(self) -> None:
        inequality = normalized_inequality.NormalizedLinearInequality(
            lb=None, expr=2.0, ub=None
        )
        self.assertEqual(inequality.lb, -math.inf)
        self.assertEqual(inequality.ub, math.inf)
        self.assertEmpty(inequality.coefficients)

    def test_init_infinite_offset_error(self) -> None:
        with self.assertRaisesRegex(ValueError, "infinite"):
            normalized_inequality.NormalizedLinearInequality(
                lb=1.0, expr=math.inf, ub=2.0
            )

    def test_init_expr_wrong_type_error(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        with self.assertRaises(TypeError):
            normalized_inequality.NormalizedLinearInequality(lb=1.0, expr=x * x, ub=2.0)

    def test_as_normalized_inequality_from_parts(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        inequality = normalized_inequality.as_normalized_linear_inequality(
            lb=-4.0, expr=2.0 * x + 3.0, ub=8.0
        )
        self.assertAlmostEqual(inequality.lb, -7.0, delta=1e-10)
        self.assertAlmostEqual(inequality.ub, 5.0, delta=1e-10)
        self.assertEqual(inequality.coefficients, {x: 2.0})

    def test_as_normalized_inequality_from_none(self) -> None:
        inequality = normalized_inequality.as_normalized_linear_inequality()
        self.assertEqual(inequality.lb, -math.inf)
        self.assertEqual(inequality.ub, math.inf)
        self.assertEmpty(inequality.coefficients)

    def test_as_normalized_inequality_from_var_eq_var(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        inequality = normalized_inequality.as_normalized_linear_inequality(x == y)
        self.assertEqual(inequality.lb, 0.0)
        self.assertEqual(inequality.ub, 0.0)
        self.assertEqual(inequality.coefficients.keys(), {x, y})
        self.assertEqual(set(inequality.coefficients.values()), {-1.0, 1.0})

    def test_as_normalized_inequality_from_upper_bounded_expr(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        inequality = normalized_inequality.as_normalized_linear_inequality(
            3.0 * x + y <= 2.0
        )
        self.assertEqual(inequality.lb, -math.inf)
        self.assertEqual(inequality.ub, 2.0)
        self.assertEqual(inequality.coefficients, {x: 3.0, y: 1.0})

    def test_as_normalized_inequality_from_lower_bounded_expr(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        inequality = normalized_inequality.as_normalized_linear_inequality(
            2.0 <= 3.0 * x + y
        )
        self.assertEqual(inequality.lb, 2.0)
        self.assertEqual(inequality.ub, math.inf)
        self.assertEqual(inequality.coefficients, {x: 3.0, y: 1.0})

    def test_lb_and_bounded_expr_error(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        with self.assertRaisesRegex(AssertionError, "lb cannot be specified"):
            normalized_inequality.as_normalized_linear_inequality(x <= 1.0, lb=-3.0)

    def test_ub_and_bounded_expr_error(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        with self.assertRaisesRegex(AssertionError, "ub cannot be specified"):
            normalized_inequality.as_normalized_linear_inequality(x <= 1.0, ub=-3.0)

    def test_expr_and_bounded_expr_error(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        with self.assertRaisesRegex(AssertionError, "expr cannot be specified"):
            normalized_inequality.as_normalized_linear_inequality(x <= 1.0, expr=3 * x)

    def test_bounded_expr_bad_type_raise_error(self) -> None:
        with self.assertRaisesRegex(TypeError, "bounded_expr has bad type"):
            normalized_inequality.as_normalized_linear_inequality("dogdog")

    def test_bounded_expr_inner_expr_bad_type_raise_error(self) -> None:
        with self.assertRaisesRegex(
            TypeError, "Bad type of expression in bounded_expr"
        ):
            bounded = bounded_expressions.BoundedExpression(
                lower_bound=1.0, expression="dogdog", upper_bound=1.0
            )
            normalized_inequality.as_normalized_linear_inequality(bounded)


def _quad_coef_dict(
    quad_inequality: normalized_inequality.NormalizedQuadraticInequality,
) -> Dict[Tuple[variables.Variable, variables.Variable], float]:
    return {
        (key.first_var, key.second_var): coef
        for (key, coef) in quad_inequality.quadratic_coefficients.items()
    }


class NormalizedQuadraticInequalityTest(absltest.TestCase):

    def test_init_all_present(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        inequality = normalized_inequality.NormalizedQuadraticInequality(
            lb=-4.0, expr=4.0 * x * x + 2.0 * x + 3.0, ub=8.0
        )
        self.assertAlmostEqual(inequality.lb, -7.0, delta=1e-10)
        self.assertAlmostEqual(inequality.ub, 5.0, delta=1e-10)
        self.assertEqual(inequality.linear_coefficients, {x: 2.0})
        self.assertEqual(_quad_coef_dict(inequality), {(x, x): 4.0})

    def test_init_all_missing(self) -> None:
        inequality = normalized_inequality.NormalizedQuadraticInequality(
            lb=None, expr=None, ub=None
        )
        self.assertEqual(inequality.lb, -math.inf)
        self.assertEqual(inequality.ub, math.inf)
        self.assertEmpty(inequality.linear_coefficients)
        self.assertEmpty(inequality.quadratic_coefficients)

    def test_init_offset_only(self) -> None:
        inequality = normalized_inequality.NormalizedQuadraticInequality(
            lb=None, expr=2.0, ub=None
        )
        self.assertEqual(inequality.lb, -math.inf)
        self.assertEqual(inequality.ub, math.inf)
        self.assertEmpty(inequality.linear_coefficients)
        self.assertEmpty(inequality.quadratic_coefficients)

    def test_init_infinite_offset_error(self) -> None:
        with self.assertRaisesRegex(ValueError, "infinite"):
            normalized_inequality.NormalizedQuadraticInequality(
                lb=1.0, expr=math.inf, ub=2.0
            )

    def test_init_expr_wrong_type_error(self) -> None:
        with self.assertRaises(TypeError):
            normalized_inequality.NormalizedQuadraticInequality(
                lb=1.0, expr="dog", ub=2.0
            )

    def test_as_normalized_inequality_from_parts(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        inequality = normalized_inequality.as_normalized_quadratic_inequality(
            lb=-4.0, expr=4.0 * x * x + 2.0 * x + 3.0, ub=8.0
        )
        self.assertAlmostEqual(inequality.lb, -7.0, delta=1e-10)
        self.assertAlmostEqual(inequality.ub, 5.0, delta=1e-10)
        self.assertEqual(inequality.linear_coefficients, {x: 2.0})
        self.assertEqual(_quad_coef_dict(inequality), {(x, x): 4.0})

    def test_as_normalized_inequality_from_none(self) -> None:
        inequality = normalized_inequality.as_normalized_quadratic_inequality()
        self.assertEqual(inequality.lb, -math.inf)
        self.assertEqual(inequality.ub, math.inf)
        self.assertEmpty(inequality.linear_coefficients)
        self.assertEmpty(inequality.quadratic_coefficients)

    def test_as_normalized_inequality_from_var_eq_var(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        inequality = normalized_inequality.as_normalized_quadratic_inequality(x == y)
        self.assertEqual(inequality.lb, 0.0)
        self.assertEqual(inequality.ub, 0.0)
        self.assertEqual(inequality.linear_coefficients.keys(), {x, y})
        self.assertEqual(set(inequality.linear_coefficients.values()), {-1.0, 1.0})
        self.assertEmpty(inequality.quadratic_coefficients)

    def test_as_normalized_inequality_from_upper_bounded_expr(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        inequality = normalized_inequality.as_normalized_quadratic_inequality(
            4.0 * x * x + 3.0 * x + y <= 2.0
        )
        self.assertEqual(inequality.lb, -math.inf)
        self.assertEqual(inequality.ub, 2.0)
        self.assertEqual(inequality.linear_coefficients, {x: 3.0, y: 1.0})
        self.assertEqual(_quad_coef_dict(inequality), {(x, x): 4.0})

    def test_as_normalized_inequality_from_lower_bounded_expr(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        inequality = normalized_inequality.as_normalized_quadratic_inequality(
            2.0 <= 4.0 * x * x + 3.0 * x + y
        )
        self.assertEqual(inequality.lb, 2.0)
        self.assertEqual(inequality.ub, math.inf)
        self.assertEqual(inequality.linear_coefficients, {x: 3.0, y: 1.0})
        self.assertEqual(_quad_coef_dict(inequality), {(x, x): 4.0})

    def test_lb_and_boundex_expr_error(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        with self.assertRaisesRegex(AssertionError, "lb cannot be specified"):
            normalized_inequality.as_normalized_quadratic_inequality(x <= 1.0, lb=-3.0)

    def test_ub_and_boundex_expr_error(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        with self.assertRaisesRegex(AssertionError, "ub cannot be specified"):
            normalized_inequality.as_normalized_quadratic_inequality(x <= 1.0, ub=-3.0)

    def test_expr_and_boundex_expr_error(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        with self.assertRaisesRegex(AssertionError, "expr cannot be specified"):
            normalized_inequality.as_normalized_quadratic_inequality(
                x <= 1.0, expr=3 * x
            )

    def test_bounded_expr_bad_type_raise_error(self) -> None:
        with self.assertRaisesRegex(TypeError, "bounded_expr has bad type"):
            normalized_inequality.as_normalized_quadratic_inequality("dogdog")

    def test_bounded_expr_inner_expr_bad_type_raise_error(self) -> None:
        with self.assertRaisesRegex(TypeError, "bounded_expr.expression has bad type"):
            bounded = bounded_expressions.BoundedExpression(
                lower_bound=1.0, expression="dogdog", upper_bound=1.0
            )
            normalized_inequality.as_normalized_quadratic_inequality(bounded)


if __name__ == "__main__":
    absltest.main()
