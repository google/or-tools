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
from typing import Any, Dict, List, NamedTuple, Optional, Tuple, Union
from unittest import mock

from absl.testing import absltest
from absl.testing import parameterized
from ortools.math_opt.python import bounded_expressions
from ortools.math_opt.python import model
from ortools.math_opt.python import variables

_LINEAR_TYPES = (
    "Variable",
    "LinearTerm",
    "LinearExpression",
    "LinearSum",
    "LinearProduct",
)
_QUADRATIC_TYPES = (
    "QuadraticTerm",
    "QuadraticExpression",
    "QuadraticSum",
    "LinearLinearProduct",
    "QuadraticProduct",
)


class BoundedLinearExprTest(absltest.TestCase):

    def test_eq_float(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        bounded_expr = x + 2 * y + 1.0 == 2.0
        self.assertIsInstance(bounded_expr, bounded_expressions.BoundedExpression)
        flat_expr = variables.as_flat_linear_expression(bounded_expr.expression)
        self.assertEqual(flat_expr.offset, 1.0)
        self.assertDictEqual(dict(flat_expr.terms), {x: 1.0, y: 2.0})
        self.assertEqual(bounded_expr.lower_bound, 2.0)
        self.assertEqual(bounded_expr.upper_bound, 2.0)

    # Also call __eq__ directly to confirm there are no pytype issues.
    def test_eq_float_explicit(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        bounded_expr = (x + 2 * y + 1.0).__eq__(2.0)
        self.assertIsInstance(bounded_expr, bounded_expressions.BoundedExpression)
        flat_expr = variables.as_flat_linear_expression(bounded_expr.expression)
        self.assertEqual(flat_expr.offset, 1.0)
        self.assertDictEqual(dict(flat_expr.terms), {x: 1.0, y: 2.0})
        self.assertEqual(bounded_expr.lower_bound, 2.0)
        self.assertEqual(bounded_expr.upper_bound, 2.0)

    def test_eq_expr(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        bounded_expr = x + 2 * y + 1.0 == 3 * y - 2.0
        self.assertIsInstance(bounded_expr, bounded_expressions.BoundedExpression)
        flat_expr = variables.as_flat_linear_expression(bounded_expr.expression)
        self.assertEqual(flat_expr.offset, 3.0)
        self.assertDictEqual(dict(flat_expr.terms), {x: 1.0, y: -1.0})
        self.assertEqual(bounded_expr.lower_bound, 0.0)
        self.assertEqual(bounded_expr.upper_bound, 0.0)

        # Check Variable.__eq__ calls LinearBase.__eq__ when appropriate.
        bounded_expr_var_on_lhs = x == 3 * y - 2.0
        self.assertIsInstance(
            bounded_expr_var_on_lhs, bounded_expressions.BoundedExpression
        )
        flat_expr_var_on_lhs = variables.as_flat_linear_expression(
            bounded_expr_var_on_lhs.expression
        )
        self.assertEqual(flat_expr_var_on_lhs.offset, 2.0)
        self.assertDictEqual(dict(flat_expr_var_on_lhs.terms), {x: 1.0, y: -3.0})
        self.assertEqual(bounded_expr_var_on_lhs.lower_bound, 0.0)
        self.assertEqual(bounded_expr_var_on_lhs.upper_bound, 0.0)

        bounded_expr_var_on_rhs = 3 * y - 2.0 == x
        self.assertIsInstance(
            bounded_expr_var_on_rhs, bounded_expressions.BoundedExpression
        )
        flat_expr_var_on_rhs = variables.as_flat_linear_expression(
            bounded_expr_var_on_rhs.expression
        )
        self.assertEqual(flat_expr_var_on_rhs.offset, -2.0)
        self.assertDictEqual(dict(flat_expr_var_on_rhs.terms), {x: -1.0, y: 3.0})
        self.assertEqual(bounded_expr_var_on_rhs.lower_bound, 0.0)
        self.assertEqual(bounded_expr_var_on_rhs.upper_bound, 0.0)

    # Also call __eq__ directly to confirm there are no pytype issues.
    def test_eq_expr_explicit(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        bounded_expr = (x + 2 * y + 1.0).__eq__(3 * y - 2.0)
        self.assertIsInstance(bounded_expr, bounded_expressions.BoundedExpression)
        flat_expr = variables.as_flat_linear_expression(bounded_expr.expression)
        self.assertEqual(flat_expr.offset, 3.0)
        self.assertDictEqual(dict(flat_expr.terms), {x: 1.0, y: -1.0})
        self.assertEqual(bounded_expr.lower_bound, 0.0)
        self.assertEqual(bounded_expr.upper_bound, 0.0)

        # Check Variable.__eq__ calls LinearBase.__eq__ when appropriate.
        bounded_expr_var_on_lhs = x.__eq__(3 * y - 2.0)
        self.assertIsInstance(
            bounded_expr_var_on_lhs, bounded_expressions.BoundedExpression
        )
        flat_expr_var_on_lhs = variables.as_flat_linear_expression(
            bounded_expr_var_on_lhs.expression
        )
        self.assertEqual(flat_expr_var_on_lhs.offset, 2.0)
        self.assertDictEqual(dict(flat_expr_var_on_lhs.terms), {x: 1.0, y: -3.0})
        self.assertEqual(bounded_expr_var_on_lhs.lower_bound, 0.0)
        self.assertEqual(bounded_expr_var_on_lhs.upper_bound, 0.0)

        bounded_expr_var_on_rhs = (3 * y - 2.0).__eq__(x)
        self.assertIsInstance(
            bounded_expr_var_on_rhs, bounded_expressions.BoundedExpression
        )
        flat_expr_var_on_rhs = variables.as_flat_linear_expression(
            bounded_expr_var_on_rhs.expression
        )
        self.assertEqual(flat_expr_var_on_rhs.offset, -2.0)
        self.assertDictEqual(dict(flat_expr_var_on_rhs.terms), {x: -1.0, y: 3.0})
        self.assertEqual(bounded_expr_var_on_rhs.lower_bound, 0.0)
        self.assertEqual(bounded_expr_var_on_rhs.upper_bound, 0.0)

    def test_var_eq_var(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        also_x = x
        bounded_expr = x == y
        self.assertIsInstance(bounded_expr, variables.VarEqVar)
        self.assertEqual(bounded_expr.first_variable, x)
        self.assertEqual(bounded_expr.second_variable, y)

        second_mod = model.Model()
        second_x = second_mod.add_binary_variable(name="x")
        # pylint: disable=g-generic-assert
        self.assertTrue(x == also_x)
        self.assertFalse(x == y)
        self.assertEqual(x.id, second_x.id)
        self.assertFalse(x == second_x)
        # pylint: enable=g-generic-assert

    # Also call __eq__ directly to confirm there are no pytype issues (see
    #  b/227214976).
    def test_var_eq_var_explicit(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        also_x = x
        bounded_expr = x.__eq__(y)
        self.assertIsInstance(bounded_expr, variables.VarEqVar)
        self.assertEqual(bounded_expr.first_variable, x)
        self.assertEqual(bounded_expr.second_variable, y)

        second_mod = model.Model()
        second_x = second_mod.add_binary_variable(name="x")
        # pylint: disable=g-generic-assert
        self.assertTrue(x.__eq__(also_x))
        self.assertFalse(x.__eq__(y))
        self.assertEqual(x.id, second_x.id)
        self.assertFalse(x.__eq__(second_x))
        # pylint: enable=g-generic-assert

    def test_var_neq_var(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        also_x = x
        second_mod = model.Model()
        second_x = second_mod.add_binary_variable(name="x")
        # pylint: disable=g-generic-assert
        self.assertFalse(x != also_x)
        self.assertTrue(x != y)
        self.assertEqual(x.id, second_x.id)
        self.assertTrue(x != second_x)
        # pylint: enable=g-generic-assert

    # Also call __ne__ directly to confirm there are no pytype issues (see
    #  b/227214976).
    def test_var_neq_var_explicit(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        also_x = x
        second_mod = model.Model()
        second_x = second_mod.add_binary_variable(name="x")
        # pylint: disable=g-generic-assert
        self.assertFalse(x.__ne__(also_x))
        self.assertTrue(x.__ne__(y))
        self.assertEqual(x.id, second_x.id)
        self.assertTrue(x.__ne__(second_x))
        # pylint: enable=g-generic-assert

    # Mock Variable.__hash__ to have a collision in the dictionary lookup so that
    # a correct behavior of x == y is needed to recover the values. For instance,
    # if VarEqVar.__bool__ always returned True, this test would fail.
    @mock.patch.object(variables.Variable, "__hash__")
    def test_var_dict(self, fixed_hash: mock.MagicMock) -> None:
        fixed_hash.return_value = 111
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        var_dict = {x: 1.0, y: 2.0}
        self.assertEqual(x.__hash__(), 111)
        self.assertEqual(y.__hash__(), 111)
        self.assertEqual(var_dict[x], 1.0)
        self.assertEqual(var_dict[y], 2.0)

    def test_leq_float(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        bounded_expr = x + 2 * y + 1.0 <= 2.0
        self.assertIsInstance(bounded_expr, bounded_expressions.UpperBoundedExpression)
        flat_expr = variables.as_flat_linear_expression(bounded_expr.expression)
        self.assertEqual(flat_expr.offset, 1.0)
        self.assertDictEqual(dict(flat_expr.terms), {x: 1.0, y: 2.0})
        self.assertEqual(bounded_expr.upper_bound, 2.0)

    def test_leq_float_rev(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        bounded_expr = 2.0 >= x + 2 * y + 1.0
        self.assertIsInstance(bounded_expr, bounded_expressions.UpperBoundedExpression)
        flat_expr = variables.as_flat_linear_expression(bounded_expr.expression)
        self.assertEqual(flat_expr.offset, 1.0)
        self.assertDictEqual(dict(flat_expr.terms), {x: 1.0, y: 2.0})
        self.assertEqual(bounded_expr.upper_bound, 2.0)

    def test_geq_float(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        bounded_expr = x + 2 * y + 1.0 >= 2.0
        self.assertIsInstance(bounded_expr, bounded_expressions.LowerBoundedExpression)
        flat_expr = variables.as_flat_linear_expression(bounded_expr.expression)
        self.assertEqual(flat_expr.offset, 1.0)
        self.assertDictEqual(dict(flat_expr.terms), {x: 1.0, y: 2.0})
        self.assertEqual(bounded_expr.lower_bound, 2.0)

    def test_geq_float_rev(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        bounded_expr = 2.0 <= x + 2 * y + 1.0
        self.assertIsInstance(bounded_expr, bounded_expressions.LowerBoundedExpression)
        flat_expr = variables.as_flat_linear_expression(bounded_expr.expression)
        self.assertEqual(flat_expr.offset, 1.0)
        self.assertDictEqual(dict(flat_expr.terms), {x: 1.0, y: 2.0})
        self.assertEqual(bounded_expr.lower_bound, 2.0)

    def test_geq_leq_float(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        bounded_expr = (0.0 <= x + 2 * y + 1.0) <= 2.0
        self.assertIsInstance(bounded_expr, bounded_expressions.BoundedExpression)
        flat_expr = variables.as_flat_linear_expression(bounded_expr.expression)
        self.assertEqual(flat_expr.offset, 1.0)
        self.assertDictEqual(dict(flat_expr.terms), {x: 1.0, y: 2.0})
        self.assertEqual(bounded_expr.upper_bound, 2.0)
        self.assertEqual(bounded_expr.lower_bound, 0.0)

    def test_geq_leq_float_rev(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        bounded_expr = 2.0 >= (x + 2 * y + 1.0 >= 0)
        self.assertIsInstance(bounded_expr, bounded_expressions.BoundedExpression)
        flat_expr = variables.as_flat_linear_expression(bounded_expr.expression)
        self.assertEqual(flat_expr.offset, 1.0)
        self.assertDictEqual(dict(flat_expr.terms), {x: 1.0, y: 2.0})
        self.assertEqual(bounded_expr.upper_bound, 2.0)
        self.assertEqual(bounded_expr.lower_bound, 0.0)

    def test_leq_geq_float(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        bounded_expr = 0.0 <= (x + 2 * y + 1.0 <= 2.0)
        self.assertIsInstance(bounded_expr, bounded_expressions.BoundedExpression)
        flat_expr = variables.as_flat_linear_expression(bounded_expr.expression)
        self.assertEqual(flat_expr.offset, 1.0)
        self.assertDictEqual(dict(flat_expr.terms), {x: 1.0, y: 2.0})
        self.assertEqual(bounded_expr.upper_bound, 2.0)
        self.assertEqual(bounded_expr.lower_bound, 0.0)

    def test_leq_geq_float_rev(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        bounded_expr = (2.0 >= x + 2 * y + 1.0) >= 0
        self.assertIsInstance(bounded_expr, bounded_expressions.BoundedExpression)
        flat_expr = variables.as_flat_linear_expression(bounded_expr.expression)
        self.assertEqual(flat_expr.offset, 1.0)
        self.assertDictEqual(dict(flat_expr.terms), {x: 1.0, y: 2.0})
        self.assertEqual(bounded_expr.upper_bound, 2.0)
        self.assertEqual(bounded_expr.lower_bound, 0.0)

    def test_leq_expr(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")
        bounded_expr = x + 3 * y + 2.0 <= y - 4.0 * z + 1.0
        self.assertIsInstance(bounded_expr, bounded_expressions.BoundedExpression)
        flat_expr = variables.as_flat_linear_expression(bounded_expr.expression)
        self.assertEqual(flat_expr.offset, 1.0)
        self.assertDictEqual(dict(flat_expr.terms), {x: 1.0, y: 2.0, z: 4.0})
        self.assertEqual(bounded_expr.lower_bound, -math.inf)
        self.assertEqual(bounded_expr.upper_bound, 0.0)

    def test_geq_expr(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")
        bounded_expr = x + 3 * y + 2.0 >= y - 4.0 * z + 1.0
        self.assertIsInstance(bounded_expr, bounded_expressions.BoundedExpression)
        flat_expr = variables.as_flat_linear_expression(bounded_expr.expression)
        self.assertEqual(flat_expr.offset, 1.0)
        self.assertDictEqual(dict(flat_expr.terms), {x: 1.0, y: 2.0, z: 4.0})
        self.assertEqual(bounded_expr.lower_bound, 0.0)
        self.assertEqual(bounded_expr.upper_bound, math.inf)


class BoundedLinearExprErrorTest(absltest.TestCase):

    def test_ne(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        # pylint: disable=pointless-statement
        with self.assertRaisesWithLiteralMatch(
            TypeError, "!= constraints are not supported"
        ):
            x != y - x
        with self.assertRaisesWithLiteralMatch(
            TypeError, "!= constraints are not supported"
        ):
            x.__ne__(y - x)
        with self.assertRaisesWithLiteralMatch(
            TypeError, "!= constraints are not supported"
        ):
            y - x != x
        with self.assertRaisesWithLiteralMatch(
            TypeError, "!= constraints are not supported"
        ):
            (y - x).__ne__(x)
        with self.assertRaisesWithLiteralMatch(
            TypeError, "!= constraints are not supported"
        ):
            y - x != x + y
        with self.assertRaisesWithLiteralMatch(
            TypeError, "!= constraints are not supported"
        ):
            (y - x).__ne__(x + y)
        # pylint: enable=pointless-statement

    def test_eq(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        # pylint: disable=pointless-statement
        with self.assertRaisesWithLiteralMatch(
            TypeError, "unsupported operand type(s) for ==: 'Variable' and 'str'"
        ):
            x == "x"  # pylint: disable=pointless-statement

        # pylint: disable=pointless-statement
        # pytype: disable=unsupported-operands
        with self.assertRaisesWithLiteralMatch(
            TypeError, "unsupported operand type(s) for ==: 'Variable' and 'str'"
        ):
            x.__eq__("x")
        # pylint: enable=pointless-statement
        # pylint: enable=unsupported-operands

    def test_float_le_expr_le_float(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        self.assertIsInstance(
            0.0 <= x + 2 * y + 1.0, bounded_expressions.LowerBoundedExpression
        )
        with self.assertRaisesRegex(
            TypeError,
            "__bool__ is unsupported.*\n.*two-sided or ranged linear inequality",
        ):
            (0.0 <= x + 2 * y + 1.0 <= 2.0)  # pylint: disable=pointless-statement

    def test_float_ge_expr_ge_float(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        self.assertIsInstance(
            2.0 >= x + 2 * y + 1.0, bounded_expressions.UpperBoundedExpression
        )
        with self.assertRaisesRegex(
            TypeError,
            "__bool__ is unsupported.*\n.*two-sided or ranged linear inequality",
        ):
            (2.0 >= x + 2 * y + 1.0 >= 0.0)  # pylint: disable=pointless-statement

    def test_expr_le_expr_le_float(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        self.assertIsInstance(
            x <= x + 2 * y + 1.0, bounded_expressions.BoundedExpression
        )
        with self.assertRaisesRegex(
            TypeError,
            "__bool__ is unsupported.*\n.*two-sided or ranged linear inequality.*",
        ):
            (x <= x + 2 * y + 1.0 <= 2.0)  # pylint: disable=pointless-statement

    def test_expr_ge_expr_ge_float(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        self.assertIsInstance(
            x >= x + 2 * y + 1.0, bounded_expressions.BoundedExpression
        )
        with self.assertRaisesRegex(
            TypeError,
            "__bool__ is unsupported.*\n.*two-sided or ranged linear inequality",
        ):
            (x >= x + 2 * y + 1.0 >= 0.0)  # pylint: disable=pointless-statement

    def test_lower_bounded_expr_leq_expr(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        rhs_expr = 2.0 + x
        lower_bounded_expr = 0.0 <= x + 2 * y + 1.0
        self.assertIsInstance(
            lower_bounded_expr, bounded_expressions.LowerBoundedExpression
        )
        # pylint: disable=pointless-statement
        with self.assertRaisesWithLiteralMatch(
            TypeError,
            "unsupported operand type(s) for <=:"
            f" {type(lower_bounded_expr).__name__!r} and"
            f" {type(rhs_expr).__name__!r}",
        ):
            lower_bounded_expr <= rhs_expr
        # pylint: enable=pointless-statement

    def test_lower_bounded_expr_geq_expr(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        rhs_expr = 2.0 + x
        lower_bounded_expr = 0.0 <= x + 2 * y + 1.0
        self.assertIsInstance(
            lower_bounded_expr, bounded_expressions.LowerBoundedExpression
        )
        # pylint: disable=pointless-statement
        with self.assertRaisesWithLiteralMatch(
            TypeError,
            f"unsupported operand type(s) for <=: {type(rhs_expr).__name__!r} and"
            f" {type(lower_bounded_expr).__name__!r}",
        ):
            lower_bounded_expr >= rhs_expr
        # pylint: enable=pointless-statement

    def test_lower_bounded_expr_geq_float(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        lower_bounded_expr = 0.0 <= x + 2 * y + 1.0
        self.assertIsInstance(
            lower_bounded_expr, bounded_expressions.LowerBoundedExpression
        )
        # pylint: disable=pointless-statement
        with self.assertRaisesWithLiteralMatch(
            TypeError,
            "'>=' not supported between instances of"
            f" {type(lower_bounded_expr).__name__!r} and 'float'",
        ):
            lower_bounded_expr >= 2.0
        # pylint: enable=pointless-statement

    def test_upper_bounded_expr_geq_expr(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        rhs_expr = 1.0 + x
        upper_bounded_expr = 2.0 >= x + 2 * y + 1.0
        self.assertIsInstance(
            upper_bounded_expr, bounded_expressions.UpperBoundedExpression
        )
        # pylint: disable=pointless-statement
        with self.assertRaisesWithLiteralMatch(
            TypeError,
            "unsupported operand type(s) for >=:"
            f" {type(upper_bounded_expr).__name__!r} and"
            f" {type(rhs_expr).__name__!r}",
        ):
            upper_bounded_expr >= rhs_expr
        # pylint: enable=pointless-statement

    def test_upper_bounded_expr_leq_expr(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        rhs_expr = 1.0 + x
        upper_bounded_expr = 2.0 >= x + 2 * y + 1.0
        self.assertIsInstance(
            upper_bounded_expr, bounded_expressions.UpperBoundedExpression
        )
        # pylint: disable=pointless-statement
        with self.assertRaisesWithLiteralMatch(
            TypeError,
            f"unsupported operand type(s) for >=: {type(rhs_expr).__name__!r} and"
            f" {type(upper_bounded_expr).__name__!r}",
        ):
            upper_bounded_expr <= rhs_expr
        # pylint: enable=pointless-statement

    def test_upper_bounded_expr_leq_float(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        upper_bounded_expr = 2.0 >= x + 2 * y + 1.0
        self.assertIsInstance(
            upper_bounded_expr, bounded_expressions.UpperBoundedExpression
        )
        # pylint: disable=pointless-statement
        with self.assertRaisesWithLiteralMatch(
            TypeError,
            "'<=' not supported between instances of"
            f" {type(upper_bounded_expr).__name__!r} and 'float'",
        ):
            upper_bounded_expr <= 2.0
        # pylint: enable=pointless-statement

    def test_bounded_expr_leq_expr(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        rhs_expr = 1.0 + x
        bounded_expr = (0.0 <= x + 2 * y + 1.0) <= 2.0
        self.assertIsInstance(bounded_expr, bounded_expressions.BoundedExpression)
        # pylint: disable=pointless-statement
        with self.assertRaisesRegex(
            TypeError,
            "unsupported operand.*\n.*two or more non-constant linear expressions",
        ):
            bounded_expr <= rhs_expr
        # pylint: enable=pointless-statement

    def test_bounded_expr_leq_float(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        bounded_expr = (0.0 <= x + 2 * y + 1.0) <= 2.0
        self.assertIsInstance(bounded_expr, bounded_expressions.BoundedExpression)
        # pylint: disable=pointless-statement
        with self.assertRaisesWithLiteralMatch(
            TypeError,
            "'<=' not supported between instances of"
            f" {type(bounded_expr).__name__!r} and 'float'",
        ):
            bounded_expr <= 2.0
        # pylint: enable=pointless-statement

    def test_bounded_expr_geq_expr(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        rhs_expr = 1.0 + x
        bounded_expr = (0.0 <= x + 2 * y + 1.0) <= 2.0
        self.assertIsInstance(bounded_expr, bounded_expressions.BoundedExpression)
        # pylint: disable=pointless-statement
        with self.assertRaisesRegex(
            TypeError,
            "unsupported operand.*\n.*two or more non-constant linear expressions",
        ):
            bounded_expr >= rhs_expr
        # pylint: enable=pointless-statement

    def test_bounded_expr_geq_float(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        bounded_expr = (0.0 <= x + 2 * y + 1.0) <= 2.0
        self.assertIsInstance(bounded_expr, bounded_expressions.BoundedExpression)
        # pylint: disable=pointless-statement
        with self.assertRaisesWithLiteralMatch(
            TypeError,
            "'>=' not supported between instances of"
            f" {type(bounded_expr).__name__!r} and 'float'",
        ):
            bounded_expr >= 2.0
        # pylint: enable=pointless-statement


def _assert_any_bounded_quad_equals(
    test_case: absltest.TestCase,
    actual: variables.BoundedQuadraticTypes,
    *,
    lb: float = -math.inf,
    ub: float = math.inf,
    offset: float = 0.0,
    lin: Optional[Dict[variables.Variable, float]] = None,
    quad: Optional[Dict[Tuple[variables.Variable, variables.Variable], float]] = None,
) -> None:
    test_case.assertIsInstance(actual.expression, variables.QuadraticBase)
    lin = lin or {}
    quad = quad or {}
    test_case.assertEqual(actual.lower_bound, lb)
    test_case.assertEqual(actual.upper_bound, ub)
    actual_quad = variables.as_flat_quadratic_expression(actual.expression)
    test_case.assertEqual(actual_quad.offset, offset)
    test_case.assertDictEqual(dict(actual_quad.linear_terms), lin)
    actual_quad_terms = {
        (key.first_var, key.second_var): coef
        for (key, coef) in actual_quad.quadratic_terms.items()
    }
    test_case.assertDictEqual(actual_quad_terms, quad)


# TODO(b/73944659): We do not want to accept bool, but pytype cannot do the
# correct inference on 3 == x * x.
def _assert_bounded_quad_equals(
    test_case: absltest.TestCase,
    actual: Union[variables.BoundedQuadraticExpression, bool],
    *,
    lb: float = -math.inf,
    ub: float = math.inf,
    offset: float = 0.0,
    lin: Optional[Dict[variables.Variable, float]] = None,
    quad: Optional[Dict[Tuple[variables.Variable, variables.Variable], float]] = None,
) -> None:
    test_case.assertIsInstance(actual, bounded_expressions.BoundedExpression)
    _assert_any_bounded_quad_equals(
        test_case, actual, lb=lb, ub=ub, offset=offset, lin=lin, quad=quad
    )


# TODO(b/73944659): We do not want to accept bool, but pytype cannot do the
# correct inference on 3 <= x * x.
def _assert_lower_bounded_quad_equals(
    test_case: absltest.TestCase,
    actual: Union[bool, variables.LowerBoundedQuadraticExpression],
    *,
    lb: float = -math.inf,
    offset: float = 0.0,
    lin: Optional[Dict[variables.Variable, float]] = None,
    quad: Optional[Dict[Tuple[variables.Variable, variables.Variable], float]] = None,
) -> None:
    test_case.assertIsInstance(actual, bounded_expressions.LowerBoundedExpression)
    _assert_any_bounded_quad_equals(
        test_case, actual, lb=lb, ub=math.inf, offset=offset, lin=lin, quad=quad
    )


# TODO(b/73944659): We do not want to accept bool, but pytype cannot do the
# correct inference on 3 >= x * x.
def _assert_upper_bounded_quad_equals(
    test_case: absltest.TestCase,
    actual: Union[bool, variables.UpperBoundedQuadraticExpression],
    *,
    ub: float = math.inf,
    offset: float = 0.0,
    lin: Optional[Dict[variables.Variable, float]] = None,
    quad: Optional[Dict[Tuple[variables.Variable, variables.Variable], float]] = None,
) -> None:
    test_case.assertIsInstance(actual, bounded_expressions.UpperBoundedExpression)
    _assert_any_bounded_quad_equals(
        test_case, actual, lb=-math.inf, ub=ub, offset=offset, lin=lin, quad=quad
    )


class BoundedQuadraticExpressionTest(absltest.TestCase):
    """Tests the creation of bounded quadratic expressions by operators."""

    def test_quad_eq_float(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        bounded = 5.0 * x * x == 3.0
        _assert_bounded_quad_equals(self, bounded, lb=3.0, ub=3.0, quad={(x, x): 5.0})

    def test_float_eq_quad(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        bounded = 3.0 == 5.0 * x * x
        _assert_bounded_quad_equals(self, bounded, lb=3.0, ub=3.0, quad={(x, x): 5.0})

    def test_quad_eq_lin(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        bounded = 5.0 * x * x == x
        _assert_bounded_quad_equals(
            self, bounded, lb=0.0, ub=0.0, lin={x: -1.0}, quad={(x, x): 5.0}
        )

    def test_lin_eq_quad(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        bounded = x == 5.0 * x * x
        _assert_bounded_quad_equals(
            self, bounded, lb=0.0, ub=0.0, lin={x: -1.0}, quad={(x, x): 5.0}
        )

    def test_quad_eq_str_raises_error(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        with self.assertRaisesRegex(TypeError, "==.*str"):
            5.0 * x * x == "hello"  # pylint: disable=pointless-statement

    def test_quad_ne_raises_error(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        with self.assertRaisesRegex(TypeError, "!= constraints"):
            x * x != 1.0  # pylint: disable=pointless-statement

    def test_quad_le_float(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        bounded = 5.0 * x * x <= 3.0
        _assert_upper_bounded_quad_equals(self, bounded, ub=3.0, quad={(x, x): 5.0})

    def test_float_ge_quad(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        bounded = 3.0 >= 5.0 * x * x
        _assert_upper_bounded_quad_equals(self, bounded, ub=3.0, quad={(x, x): 5.0})

    def test_quad_le_lin(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        bounded = 5.0 * x * x <= x + 2.0
        _assert_bounded_quad_equals(
            self, bounded, ub=0.0, quad={(x, x): 5.0}, lin={x: -1}, offset=-2.0
        )

    def test_lin_ge_quad(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        bounded = x + 2.0 >= 5.0 * x * x
        _assert_bounded_quad_equals(
            self, bounded, ub=0.0, quad={(x, x): 5.0}, lin={x: -1}, offset=-2.0
        )

    def test_quad_le_str_raises_error(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        with self.assertRaisesRegex(TypeError, "<=.*str"):
            5.0 * x * x <= "test"  # pylint: disable=pointless-statement

    def test_quad_ge_float(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        bounded = 5.0 * x * x >= 3.0
        _assert_lower_bounded_quad_equals(self, bounded, lb=3.0, quad={(x, x): 5.0})

    def test_float_le_quad(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        bounded = 3.0 <= 5.0 * x * x
        _assert_lower_bounded_quad_equals(self, bounded, lb=3.0, quad={(x, x): 5.0})

    def test_quad_ge_lin(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        bounded = 5.0 * x * x >= x + 2.0
        _assert_bounded_quad_equals(
            self, bounded, lb=0.0, quad={(x, x): 5.0}, lin={x: -1}, offset=-2.0
        )

    def test_lin_le_quad(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        bounded = x + 2.0 <= 5.0 * x * x
        _assert_bounded_quad_equals(
            self, bounded, lb=0.0, quad={(x, x): 5.0}, lin={x: -1}, offset=-2.0
        )

    def test_quad_ge_str_raises_error(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        with self.assertRaisesRegex(TypeError, ">=.*str"):
            5.0 * x * x >= "test"  # pylint: disable=pointless-statement

    def test_ge_twice(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        bounded = (1.0 <= 5.0 * x * x) <= 2.0
        _assert_bounded_quad_equals(
            self,
            bounded,
            lb=1.0,
            ub=2.0,
            quad={(x, x): 5.0},
        )

    def test_ge_twice_fails_when_ambiguous(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        with self.assertRaisesRegex(TypeError, "'BoundedExpression' and 'float'"):
            (x <= 5.0 * x * x) <= 2.0  # pylint: disable=pointless-statement

    def test_no_quad_ge_bounded_expr(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        with self.assertRaisesRegex(TypeError, r"\(a <= b\) <= c"):
            x * x >= (x == 5.0)  # pylint: disable=pointless-statement

    def test_le_twice(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        bounded = 1.0 <= (5.0 * x * x <= 2.0)
        _assert_bounded_quad_equals(
            self,
            bounded,
            lb=1.0,
            ub=2.0,
            quad={(x, x): 5.0},
        )

    def test_le_twice_fails_when_ambiguous(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        with self.assertRaisesRegex(TypeError, "'BoundedExpression' and 'float'"):
            (x >= 5.0 * x * x) >= 2.0  # pylint: disable=pointless-statement

    def test_no_quad_le_bounded_expr(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        with self.assertRaisesRegex(TypeError, r"\(a <= b\) <= c"):
            x * x <= (x == 5.0)  # pylint: disable=pointless-statement


# TODO(b/216492143): change __str__ to match C++ implementation in cl/421649402.
class LinearStrAndReprTest(parameterized.TestCase):

    def test_sorting_ok(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        zero_plus_var_plus_pos_term = 0 + x + 2 * y
        self.assertEqual(
            repr(zero_plus_var_plus_pos_term),
            f'LinearSum((LinearSum((0, <Variable id: 0, name: {"x"!r}>)), '
            f'LinearTerm(<Variable id: 1, name: {"y"!r}>, 2)))',
        )
        # This fails if we don't sort by variable names in Variable.__str__().
        self.assertEqual(str(zero_plus_var_plus_pos_term), "0.0 + 1.0 * x + 2.0 * y")

    def test_simple_expressions(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")

        var_plus_pos_term = x + 2 * y
        self.assertEqual(
            repr(var_plus_pos_term),
            f'LinearSum((<Variable id: 0, name: {"x"!r}>, '
            f'LinearTerm(<Variable id: 1, name: {"y"!r}>, 2)))',
        )
        self.assertEqual(str(var_plus_pos_term), "0.0 + 1.0 * x + 2.0 * y")

        var_plus_neg_term = x - 3 * y
        self.assertEqual(
            repr(var_plus_neg_term),
            f'LinearSum((<Variable id: 0, name: {"x"!r}>, '
            f'LinearTerm(<Variable id: 1, name: {"y"!r}>, -3)))',
        )
        self.assertEqual(str(var_plus_neg_term), "0.0 + 1.0 * x - 3.0 * y")

        var_plus_num = x + 1.0
        self.assertEqual(
            repr(var_plus_num), f'LinearSum((<Variable id: 0, name: {"x"!r}>, 1.0))'
        )
        self.assertEqual(str(var_plus_num), "1.0 + 1.0 * x")

        num_times_var_sum = 2 * (x + y + 3)
        self.assertEqual(
            repr(num_times_var_sum),
            "LinearProduct(2.0, LinearSum((LinearSum((<Variable id: 0, name:"
            f' {"x"!r}>, <Variable id: 1, name: {"y"!r}>)), 3)))',
        )
        self.assertEqual(str(num_times_var_sum), "6.0 + 2.0 * x + 2.0 * y")
        self.assertEqual(
            repr(variables.as_flat_linear_expression(num_times_var_sum)),
            f'LinearExpression(6.0, {"{"!s}'
            f'<Variable id: 0, name: {"x"!r}>: 2.0, '
            f'<Variable id: 1, name: {"y"!r}>: 2.0{"}"!s})',
        )
        self.assertEqual(
            str(variables.as_flat_linear_expression(num_times_var_sum)),
            "6.0 + 2.0 * x + 2.0 * y",
        )

        linear_term = 2 * x
        self.assertEqual(
            repr(linear_term), f'LinearTerm(<Variable id: 0, name: {"x"!r}>, 2)'
        )
        self.assertEqual(str(linear_term), "2 * x")

    def test_sum_expressions(self) -> None:
        mod = model.Model()
        x = [mod.add_binary_variable(name=f"x{i}") for i in range(3)]

        linear_sum = variables.LinearSum(i * x[i] for i in range(3))
        self.assertEqual(
            repr(linear_sum),
            "LinearSum(("
            f'LinearTerm(<Variable id: 0, name: {"x0"!r}>, 0), '
            f'LinearTerm(<Variable id: 1, name: {"x1"!r}>, 1), '
            f'LinearTerm(<Variable id: 2, name: {"x2"!r}>, 2)))',
        )
        self.assertEqual(str(linear_sum), "0.0 + 1.0 * x1 + 2.0 * x2")

        python_sum = sum(i * x[i] for i in range(3))
        self.assertEqual(
            repr(python_sum),
            "LinearSum(("
            "LinearSum(("
            f'LinearSum((0, LinearTerm(<Variable id: 0, name: {"x0"!r}>, 0))), '
            f'LinearTerm(<Variable id: 1, name: {"x1"!r}>, 1))), '
            f'LinearTerm(<Variable id: 2, name: {"x2"!r}>, 2)))',
        )
        self.assertEqual(str(python_sum), "0.0 + 1.0 * x1 + 2.0 * x2")


# TODO(b/216492143): change __str__ to match C++ implementation in cl/421649402.
class QuadraticStrAndReprTest(parameterized.TestCase):

    def test_sorting_ok(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        zero_plus_var_plus_pos_term = 0 + x + 2 * y + x * x
        self.assertEqual(
            repr(zero_plus_var_plus_pos_term),
            "QuadraticSum(("
            f'LinearSum((LinearSum((0, <Variable id: 0, name: {"x"!r}>)), '
            f'LinearTerm(<Variable id: 1, name: {"y"!r}>, 2))), '
            f'QuadraticTerm(QuadraticTermKey(<Variable id: 0, name: {"x"!r}>, '
            f'<Variable id: 0, name: {"x"!r}>), 1.0)))',
        )
        # This fails if we don't sort by variable names in Variable.__str__().
        self.assertEqual(
            str(zero_plus_var_plus_pos_term),
            "0.0 + 1.0 * x + 2.0 * y + 1.0 * x * x",
        )

    def test_simple_expressions(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")

        var_plus_pos_term = x + 2 * y * y
        self.assertEqual(
            repr(var_plus_pos_term),
            f'QuadraticSum((<Variable id: 0, name: {"x"!r}>, '
            f'QuadraticTerm(QuadraticTermKey(<Variable id: 1, name: {"y"!r}>, '
            f'<Variable id: 1, name: {"y"!r}>), 2)))',
        )
        self.assertEqual(str(var_plus_pos_term), "0.0 + 1.0 * x + 2.0 * y * y")

        var_plus_neg_term = x - 3 * y * y
        self.assertEqual(
            repr(var_plus_neg_term),
            f'QuadraticSum((<Variable id: 0, name: {"x"!r}>, '
            f'QuadraticTerm(QuadraticTermKey(<Variable id: 1, name: {"y"!r}>, '
            f'<Variable id: 1, name: {"y"!r}>), -3)))',
        )
        self.assertEqual(str(var_plus_neg_term), "0.0 + 1.0 * x - 3.0 * y * y")

        num_times_term_sum = 2 * (x * x + y + 3)
        self.assertEqual(
            repr(num_times_term_sum),
            "QuadraticProduct(2.0,"
            " QuadraticSum((QuadraticSum((QuadraticTerm(QuadraticTermKey(<Variable"
            f' id: 0, name: {"x"!r}>, <Variable id: 0, name: {"x"!r}>), 1.0),'
            f' <Variable id: 1, name: {"y"!r}>)), 3)))',
        )
        self.assertEqual(str(num_times_term_sum), "6.0 + 2.0 * y + 2.0 * x * x")
        self.assertEqual(
            repr(variables.as_flat_quadratic_expression(num_times_term_sum)),
            f'QuadraticExpression(6.0, {"{"!s}'
            f'<Variable id: 1, name: {"y"!r}>: 2.0{"}"!s}, '
            f'{"{"!s}QuadraticTermKey(<Variable id: 0, name: {"x"!r}>, '
            f'<Variable id: 0, name: {"x"!r}>): 2.0{"}"!s})',
        )
        self.assertEqual(
            str(variables.as_flat_quadratic_expression(num_times_term_sum)),
            "6.0 + 2.0 * y + 2.0 * x * x",
        )

        linear_times_linear = (2 * x) * (1 + y)
        self.assertEqual(
            repr(linear_times_linear),
            f'LinearLinearProduct(LinearTerm(<Variable id: 0, name: {"x"!r}>, 2), '
            f'LinearSum((1, <Variable id: 1, name: {"y"!r}>)))',
        )
        self.assertEqual(str(linear_times_linear), "0.0 + 2.0 * x + 2.0 * x * y")

        quadratic_term = 2 * x * x
        self.assertEqual(
            repr(quadratic_term),
            "QuadraticTerm(QuadraticTermKey(<Variable id: 0, "
            f'name: {"x"!r}>, <Variable id: 0, name: {"x"!r}>), 2)',
        )
        self.assertEqual(str(quadratic_term), "2 * x * x")

    def test_sum_expressions(self) -> None:
        mod = model.Model()
        x = [mod.add_binary_variable(name=f"x{i}") for i in range(3)]

        quadratic_sum = variables.QuadraticSum(i * x[i] * x[i] for i in range(3))
        self.assertEqual(
            repr(quadratic_sum),
            "QuadraticSum(("
            f'QuadraticTerm(QuadraticTermKey(<Variable id: 0, name: {"x0"!r}>, '
            f'<Variable id: 0, name: {"x0"!r}>), 0), '
            f'QuadraticTerm(QuadraticTermKey(<Variable id: 1, name: {"x1"!r}>, '
            f'<Variable id: 1, name: {"x1"!r}>), 1), '
            f'QuadraticTerm(QuadraticTermKey(<Variable id: 2, name: {"x2"!r}>, '
            f'<Variable id: 2, name: {"x2"!r}>), 2)))',
        )
        self.assertEqual(str(quadratic_sum), "0.0 + 1.0 * x1 * x1 + 2.0 * x2 * x2")

        python_sum = sum(i * x[i] * x[i] for i in range(3))
        self.assertEqual(
            repr(python_sum),
            "QuadraticSum(("
            "QuadraticSum(("
            "QuadraticSum((0, QuadraticTerm(QuadraticTermKey(<Variable id: 0, "
            f'name: {"x0"!r}>, <Variable id: 0, name: {"x0"!r}>), 0))), '
            f'QuadraticTerm(QuadraticTermKey(<Variable id: 1, name: {"x1"!r}>, '
            f'<Variable id: 1, name: {"x1"!r}>), 1))), '
            f'QuadraticTerm(QuadraticTermKey(<Variable id: 2, name: {"x2"!r}>, '
            f'<Variable id: 2, name: {"x2"!r}>), 2)))',
        )
        self.assertEqual(str(python_sum), "0.0 + 1.0 * x1 * x1 + 2.0 * x2 * x2")


class LinearNumberOpTestsParameters(NamedTuple):
    linear_type: str
    constant: Union[float, int]
    linear_first: bool

    def test_suffix(self):
        if self.linear_first:
            return f"_{self.linear_type}_{type(self.constant).__name__}"
        else:
            return f"_{type(self.constant).__name__}_{self.linear_type}"


def all_linear_number_op_parameters() -> List[LinearNumberOpTestsParameters]:
    result = []
    for t in _LINEAR_TYPES:
        for c in (2, 0.25):
            for first in (True, False):
                result.append(
                    LinearNumberOpTestsParameters(
                        linear_type=t, constant=c, linear_first=first
                    )
                )
    return result


# Test all operations (including inplace) between a number and a Linear object
class LinearNumberOpTests(parameterized.TestCase):

    @parameterized.named_parameters(
        (p.test_suffix(), p.linear_type, p.constant, p.linear_first)
        for p in all_linear_number_op_parameters()
    )
    def test_mult(
        self, linear_type: str, constant: Union[float, int], linear_first: bool
    ) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        if linear_type == "Variable":
            linear = x
            expected_type = variables.LinearTerm
            expected_offset = 0
            expected_terms = {x: constant}
        elif linear_type == "LinearTerm":
            linear = variables.LinearTerm(x, 2)
            expected_type = variables.LinearTerm
            expected_offset = 0
            expected_terms = {x: 2 * constant}
        elif linear_type == "LinearExpression":
            linear = variables.LinearExpression(x - 2 * y + 3)
            expected_type = variables.LinearProduct
            expected_offset = 3 * constant
            expected_terms = {x: constant, y: -2 * constant}
        elif linear_type == "LinearSum":
            linear = variables.LinearSum((x, -2 * y, 3))
            expected_type = variables.LinearProduct
            expected_offset = 3 * constant
            expected_terms = {x: constant, y: -2 * constant}
        elif linear_type == "LinearProduct":
            linear = variables.LinearProduct(2, x)
            expected_type = variables.LinearProduct
            expected_offset = 0
            expected_terms = {x: 2 * constant}
        else:
            raise AssertionError(f"unknown linear type: {linear_type!r}")

        # Check __mul__ and __rmul__
        s = linear * constant if linear_first else constant * linear
        e = variables.as_flat_linear_expression(s)
        self.assertIsInstance(s, expected_type)
        self.assertEqual(e.offset, expected_offset)
        self.assertDictEqual(dict(e.terms), expected_terms)

        # Also check __imul__
        if linear_first:
            expr = linear
            expr *= constant
        else:
            expr = constant
            expr *= linear
        e_inplace = variables.as_flat_linear_expression(expr)
        self.assertIsInstance(expr, expected_type)
        self.assertEqual(e_inplace.offset, expected_offset)
        self.assertDictEqual(dict(e_inplace.terms), expected_terms)

    @parameterized.named_parameters(
        (p.test_suffix(), p.linear_type, p.constant, p.linear_first)
        for p in all_linear_number_op_parameters()
    )
    def test_div(
        self, linear_type: str, constant: Union[float, int], linear_first: bool
    ) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        if linear_type == "Variable":
            linear = x
            expected_type = variables.LinearTerm
            expected_offset = 0
            expected_terms = {x: 1 / constant}
        elif linear_type == "LinearTerm":
            linear = variables.LinearTerm(x, 2)
            expected_type = variables.LinearTerm
            expected_offset = 0
            expected_terms = {x: 2 / constant}
        elif linear_type == "LinearExpression":
            linear = variables.LinearExpression(x - 2 * y + 3)
            expected_type = variables.LinearProduct
            expected_offset = 3 / constant
            expected_terms = {x: 1 / constant, y: -2 / constant}
        elif linear_type == "LinearSum":
            linear = variables.LinearSum((x, -2 * y, 3))
            expected_type = variables.LinearProduct
            expected_offset = 3 / constant
            expected_terms = {x: 1 / constant, y: -2 / constant}
        elif linear_type == "LinearProduct":
            linear = variables.LinearProduct(2, x)
            expected_type = variables.LinearProduct
            expected_offset = 0
            expected_terms = {x: 2 / constant}
        else:
            raise AssertionError(f"unknown linear type: {linear_type!r}")

        # Check __truediv__
        if linear_first:
            s = linear / constant
            e = variables.as_flat_linear_expression(s)
            self.assertIsInstance(s, expected_type)
            self.assertEqual(e.offset, expected_offset)
            self.assertDictEqual(dict(e.terms), expected_terms)
        else:
            with self.assertRaisesWithLiteralMatch(
                TypeError,
                f"unsupported operand type(s) for /: {type(constant).__name__!r} "
                f"and {type(linear).__name__!r}",
            ):
                s = constant / linear  # pytype: disable=unsupported-operands

        # Also check __itruediv__
        if linear_first:
            linear /= constant
            e_inplace = variables.as_flat_linear_expression(linear)
            self.assertIsInstance(linear, expected_type)
            self.assertEqual(e_inplace.offset, expected_offset)
            self.assertDictEqual(dict(e_inplace.terms), expected_terms)
        else:
            with self.assertRaisesWithLiteralMatch(
                TypeError,
                f"unsupported operand type(s) for /=: {type(constant).__name__!r} "
                f"and {type(linear).__name__!r}",
            ):
                expr = constant
                expr /= linear  # pytype: disable=unsupported-operands

    @parameterized.named_parameters(
        (p.test_suffix(), p.linear_type, p.constant, p.linear_first)
        for p in all_linear_number_op_parameters()
    )
    def test_add(
        self, linear_type: str, constant: Union[float, int], linear_first: bool
    ) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        if linear_type == "Variable":
            linear = x
            expected_offset = constant
            expected_terms = {x: 1}
        elif linear_type == "LinearTerm":
            linear = variables.LinearTerm(x, 2)
            expected_offset = constant
            expected_terms = {x: 2}
        elif linear_type == "LinearExpression":
            linear = variables.LinearExpression(x - 2 * y + 1)
            expected_offset = constant + 1
            expected_terms = {x: 1, y: -2}
        elif linear_type == "LinearSum":
            linear = variables.LinearSum((x, -2 * y, 1))
            expected_offset = constant + 1
            expected_terms = {x: 1, y: -2}
        elif linear_type == "LinearProduct":
            linear = variables.LinearProduct(2, x)
            expected_offset = constant
            expected_terms = {x: 2}
        else:
            raise AssertionError(f"unknown linear type: {linear_type!r}")

        # Check __add__ and __radd__
        s = linear + constant if linear_first else constant + linear
        e = variables.as_flat_linear_expression(s)
        self.assertIsInstance(s, variables.LinearSum)
        self.assertEqual(e.offset, expected_offset)
        self.assertDictEqual(dict(e.terms), expected_terms)

        # Also check __iadd__
        if linear_first:
            expr = linear
            expr += constant
        else:
            expr = constant
            expr += linear
        e_inplace = variables.as_flat_linear_expression(expr)
        self.assertIsInstance(expr, variables.LinearSum)
        self.assertEqual(e_inplace.offset, expected_offset)
        self.assertDictEqual(dict(e_inplace.terms), expected_terms)

    @parameterized.named_parameters(
        (p.test_suffix(), p.linear_type, p.constant, p.linear_first)
        for p in all_linear_number_op_parameters()
    )
    def test_sub(
        self, linear_type: str, constant: Union[float, int], linear_first: bool
    ) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        sign = 1 if linear_first else -1
        if linear_type == "Variable":
            linear = x
            expected_offset = -sign * constant
            expected_terms = {x: sign}
        elif linear_type == "LinearTerm":
            linear = variables.LinearTerm(x, 2)
            expected_offset = -sign * constant
            expected_terms = {x: sign * 2}
        elif linear_type == "LinearExpression":
            linear = variables.LinearExpression(x - 2 * y + 3)
            expected_offset = -sign * constant + 3 * sign
            expected_terms = {x: sign, y: -sign * 2}
        elif linear_type == "LinearSum":
            linear = variables.LinearSum((x, -2 * y, 3))
            expected_offset = -sign * constant + 3 * sign
            expected_terms = {x: sign, y: -sign * 2}
        elif linear_type == "LinearProduct":
            linear = variables.LinearProduct(2, x)
            expected_offset = -sign * constant
            expected_terms = {x: sign * 2}
        else:
            raise AssertionError(f"unknown linear type: {linear_type!r}")

        # Check __sub__ and __rsub__
        s = linear - constant if linear_first else constant - linear
        e = variables.as_flat_linear_expression(s)
        self.assertIsInstance(s, variables.LinearSum)
        self.assertEqual(e.offset, expected_offset)
        self.assertDictEqual(dict(e.terms), expected_terms)

        # Also check __isub__
        if linear_first:
            linear -= constant
            e_inplace = variables.as_flat_linear_expression(linear)
            self.assertIsInstance(linear, variables.LinearSum)
            self.assertEqual(e_inplace.offset, expected_offset)
            self.assertDictEqual(dict(e_inplace.terms), expected_terms)


class QuadraticTermKey(absltest.TestCase):

    # Mock QuadraticTermKey.__hash__ to have a collision in the dictionary lookup
    # so that a correct behavior of term1 == term2 is needed to recover the
    # values. For instance, if QuadraticTermKey.__eq__ only compared equality of
    # the first variables in the keys, this test would fail.
    @mock.patch.object(variables.QuadraticTermKey, "__hash__")
    def test_var_dict(self, fixed_hash: mock.MagicMock) -> None:
        fixed_hash.return_value = 111
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        xx = variables.QuadraticTermKey(x, x)
        xy = variables.QuadraticTermKey(x, y)
        yy = variables.QuadraticTermKey(y, y)
        var_dict = {xx: 1, xy: 2, yy: 3}
        self.assertEqual(xx.__hash__(), 111)
        self.assertEqual(xy.__hash__(), 111)
        self.assertEqual(yy.__hash__(), 111)
        self.assertEqual(var_dict[xx], 1)
        self.assertEqual(var_dict[xy], 2)
        self.assertEqual(var_dict[yy], 3)


class QuadraticNumberOpTestsParameters(NamedTuple):
    quadratic_type: str
    constant: Union[float, int]
    quadratic_first: bool

    def test_suffix(self):
        if self.quadratic_first:
            return f"_{self.quadratic_type}_{type(self.constant).__name__}"
        else:
            return f"_{type(self.constant).__name__}_{self.quadratic_type}"


def all_quadratic_number_op_parameters() -> List[QuadraticNumberOpTestsParameters]:
    result = []
    for t in _QUADRATIC_TYPES:
        for c in (2, 0.25):
            for first in (True, False):
                result.append(
                    QuadraticNumberOpTestsParameters(
                        quadratic_type=t, constant=c, quadratic_first=first
                    )
                )
    return result


# Test all operations (including inplace) between a number and a Quadratic
# object.
@parameterized.named_parameters(
    (p.test_suffix(), p.quadratic_type, p.constant, p.quadratic_first)
    for p in all_quadratic_number_op_parameters()
)
class QuadraticNumberOpTests(parameterized.TestCase):

    def test_mult(
        self,
        quadratic_type: str,
        constant: Union[float, int],
        quadratic_first: bool,
    ) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        xx = variables.QuadraticTermKey(x, x)
        xy = variables.QuadraticTermKey(x, y)
        yy = variables.QuadraticTermKey(y, y)
        if quadratic_type == "QuadraticTerm":
            quadratic = variables.QuadraticTerm(xy, 2)
            expected_type = variables.QuadraticTerm
            expected_offset = 0
            expected_linear_terms = {}
            expected_quadratic_terms = {xy: 2 * constant}
        elif quadratic_type == "QuadraticExpression":
            quadratic = variables.QuadraticExpression(x * x - 2 * x * y - x + 3)
            expected_type = variables.QuadraticProduct
            expected_offset = 3 * constant
            expected_linear_terms = {x: -constant}
            expected_quadratic_terms = {xx: constant, xy: -2 * constant}
        elif quadratic_type == "QuadraticSum":
            quadratic = variables.QuadraticSum((x, -2 * y, 3, y * y))
            expected_type = variables.QuadraticProduct
            expected_offset = 3 * constant
            expected_linear_terms = {x: constant, y: -2 * constant}
            expected_quadratic_terms = {yy: constant}
        elif quadratic_type == "LinearLinearProduct":
            quadratic = variables.LinearLinearProduct(x + y + 1, x + 1)
            expected_type = variables.QuadraticProduct
            expected_offset = constant
            expected_linear_terms = {x: 2 * constant, y: constant}
            expected_quadratic_terms = {xx: constant, xy: constant}
        elif quadratic_type == "QuadraticProduct":
            quadratic = variables.QuadraticProduct(2, x * x)
            expected_type = variables.QuadraticProduct
            expected_offset = 0.0
            expected_linear_terms = {}
            expected_quadratic_terms = {xx: 2 * constant}
        else:
            raise AssertionError(f"unknown quaratic type: {quadratic_type!r}")

        # Check __mul__ and __rmul__
        s = quadratic * constant if quadratic_first else constant * quadratic
        e = variables.as_flat_quadratic_expression(s)
        self.assertIsInstance(s, expected_type)
        self.assertEqual(e.offset, expected_offset)
        self.assertDictEqual(dict(e.linear_terms), expected_linear_terms)
        self.assertDictEqual(dict(e.quadratic_terms), expected_quadratic_terms)

        # Also check __imul__
        if quadratic_first:
            expr = quadratic
            expr *= constant
        else:
            expr = constant
            expr *= quadratic
        e_inplace = variables.as_flat_quadratic_expression(expr)
        self.assertIsInstance(expr, expected_type)
        self.assertEqual(e_inplace.offset, expected_offset)
        self.assertDictEqual(dict(e_inplace.linear_terms), expected_linear_terms)
        self.assertDictEqual(dict(e_inplace.quadratic_terms), expected_quadratic_terms)

    def test_div(
        self,
        quadratic_type: str,
        constant: Union[float, int],
        quadratic_first: bool,
    ) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        xx = variables.QuadraticTermKey(x, x)
        xy = variables.QuadraticTermKey(x, y)
        yy = variables.QuadraticTermKey(y, y)
        if quadratic_type == "QuadraticTerm":
            quadratic = variables.QuadraticTerm(xy, 2)
            expected_type = variables.QuadraticTerm
            expected_offset = 0
            expected_linear_terms = {}
            expected_quadratic_terms = {xy: 2 / constant}
        elif quadratic_type == "QuadraticExpression":
            quadratic = variables.QuadraticExpression(x * x - 2 * x * y - x + 3)
            expected_type = variables.QuadraticProduct
            expected_offset = 3 / constant
            expected_linear_terms = {x: -1 / constant}
            expected_quadratic_terms = {xx: 1 / constant, xy: -2 / constant}
        elif quadratic_type == "QuadraticSum":
            quadratic = variables.QuadraticSum((x, -2 * y, 3, y * y))
            expected_type = variables.QuadraticProduct
            expected_offset = 3 / constant
            expected_linear_terms = {x: 1 / constant, y: -2 / constant}
            expected_quadratic_terms = {yy: 1 / constant}
        elif quadratic_type == "LinearLinearProduct":
            quadratic = variables.LinearLinearProduct(x + y + 1, x + 1)
            expected_type = variables.QuadraticProduct
            expected_offset = 1 / constant
            expected_linear_terms = {x: 2 / constant, y: 1 / constant}
            expected_quadratic_terms = {xx: 1 / constant, xy: 1 / constant}
        elif quadratic_type == "QuadraticProduct":
            quadratic = variables.QuadraticProduct(2, x * x)
            expected_type = variables.QuadraticProduct
            expected_offset = 0.0
            expected_linear_terms = {}
            expected_quadratic_terms = {xx: 2 / constant}
        else:
            raise AssertionError(f"unknown quaratic type: {quadratic_type!r}")

        # Check __truediv__
        if quadratic_first:
            s = quadratic / constant
            e = variables.as_flat_quadratic_expression(s)
            self.assertIsInstance(s, expected_type)
            self.assertEqual(e.offset, expected_offset)
            self.assertDictEqual(dict(e.linear_terms), expected_linear_terms)
            self.assertDictEqual(dict(e.quadratic_terms), expected_quadratic_terms)
        else:
            with self.assertRaisesWithLiteralMatch(
                TypeError,
                f"unsupported operand type(s) for /: {type(constant).__name__!r} "
                f"and {type(quadratic).__name__!r}",
            ):
                s = constant / quadratic  # pytype: disable=unsupported-operands

        # Also check __itruediv__
        if quadratic_first:
            quadratic /= constant
            e_inplace = variables.as_flat_quadratic_expression(quadratic)
            self.assertIsInstance(quadratic, expected_type)
            self.assertEqual(e_inplace.offset, expected_offset)
            self.assertDictEqual(dict(e_inplace.linear_terms), expected_linear_terms)
            self.assertDictEqual(
                dict(e_inplace.quadratic_terms), expected_quadratic_terms
            )
        else:
            with self.assertRaisesWithLiteralMatch(
                TypeError,
                f"unsupported operand type(s) for /=: {type(constant).__name__!r} "
                f"and {type(quadratic).__name__!r}",
            ):
                expr = constant
                expr /= quadratic  # pytype: disable=unsupported-operands

    def test_add(
        self,
        quadratic_type: str,
        constant: Union[float, int],
        quadratic_first: bool,
    ) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        xx = variables.QuadraticTermKey(x, x)
        xy = variables.QuadraticTermKey(x, y)
        yy = variables.QuadraticTermKey(y, y)
        if quadratic_type == "QuadraticTerm":
            quadratic = variables.QuadraticTerm(xy, 2)
            expected_offset = constant
            expected_linear_terms = {}
            expected_quadratic_terms = {xy: 2}
        elif quadratic_type == "QuadraticExpression":
            quadratic = variables.QuadraticExpression(x * x - 2 * x * y - x + 3)
            expected_offset = 3 + constant
            expected_linear_terms = {x: -1}
            expected_quadratic_terms = {xx: 1, xy: -2}
        elif quadratic_type == "QuadraticSum":
            quadratic = variables.QuadraticSum((x, -2 * y, 3, y * y))
            expected_offset = 3 + constant
            expected_linear_terms = {x: 1, y: -2}
            expected_quadratic_terms = {yy: 1}
        elif quadratic_type == "LinearLinearProduct":
            quadratic = variables.LinearLinearProduct(x + y + 1, x + 1)
            expected_offset = 1 + constant
            expected_linear_terms = {x: 2, y: 1}
            expected_quadratic_terms = {xx: 1, xy: 1}
        elif quadratic_type == "QuadraticProduct":
            quadratic = variables.QuadraticProduct(2, x * x)
            expected_offset = constant
            expected_linear_terms = {}
            expected_quadratic_terms = {xx: 2}
        else:
            raise AssertionError(f"unknown quaratic type: {quadratic_type!r}")

        # Check __add__ and __radd__
        s = quadratic + constant if quadratic_first else constant + quadratic
        e = variables.as_flat_quadratic_expression(s)
        self.assertIsInstance(s, variables.QuadraticSum)
        self.assertEqual(e.offset, expected_offset)
        self.assertDictEqual(dict(e.linear_terms), expected_linear_terms)
        self.assertDictEqual(dict(e.quadratic_terms), expected_quadratic_terms)

        # Also check __iadd__
        if quadratic_first:
            expr = quadratic
            expr += constant
        else:
            expr = constant
            expr += quadratic
        e_inplace = variables.as_flat_quadratic_expression(expr)
        self.assertIsInstance(expr, variables.QuadraticSum)
        self.assertEqual(e_inplace.offset, expected_offset)
        self.assertDictEqual(dict(e_inplace.linear_terms), expected_linear_terms)
        self.assertDictEqual(dict(e_inplace.quadratic_terms), expected_quadratic_terms)

    def test_sub(
        self,
        quadratic_type: str,
        constant: Union[float, int],
        quadratic_first: bool,
    ) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        xx = variables.QuadraticTermKey(x, x)
        xy = variables.QuadraticTermKey(x, y)
        yy = variables.QuadraticTermKey(y, y)
        sign = 1 if quadratic_first else -1
        if quadratic_type == "QuadraticTerm":
            quadratic = variables.QuadraticTerm(xy, 2)
            expected_offset = -sign * constant
            expected_linear_terms = {}
            expected_quadratic_terms = {xy: sign * 2}
        elif quadratic_type == "QuadraticExpression":
            quadratic = variables.QuadraticExpression(x * x - 2 * x * y - x + 3)
            expected_offset = sign * 3 - sign * constant
            expected_linear_terms = {x: sign * (-1)}
            expected_quadratic_terms = {xx: sign * 1, xy: sign * (-2)}
        elif quadratic_type == "QuadraticSum":
            quadratic = variables.QuadraticSum((x, -2 * y, 3, y * y))
            expected_offset = sign * 3 - sign * constant
            expected_linear_terms = {x: sign * 1, y: sign * (-2)}
            expected_quadratic_terms = {yy: sign}
        elif quadratic_type == "LinearLinearProduct":
            quadratic = variables.LinearLinearProduct(x + y + 1, x + 1)
            expected_offset = sign * 1 - sign * constant
            expected_linear_terms = {x: sign * 2, y: sign * 1}
            expected_quadratic_terms = {xx: sign, xy: sign}
        elif quadratic_type == "QuadraticProduct":
            quadratic = variables.QuadraticProduct(2, x * x)
            expected_offset = -sign * constant
            expected_linear_terms = {}
            expected_quadratic_terms = {xx: sign * 2}
        else:
            raise AssertionError(f"unknown quaratic type: {quadratic_type!r}")

        # Check __sub__ and __rsub__
        s = quadratic - constant if quadratic_first else constant - quadratic
        e = variables.as_flat_quadratic_expression(s)
        self.assertIsInstance(s, variables.QuadraticSum)
        self.assertEqual(e.offset, expected_offset)
        self.assertDictEqual(dict(e.linear_terms), expected_linear_terms)
        self.assertDictEqual(dict(e.quadratic_terms), expected_quadratic_terms)

        # Also check __isub__
        if quadratic_first:
            quadratic -= constant
            e_inplace = variables.as_flat_quadratic_expression(quadratic)
            self.assertIsInstance(quadratic, variables.QuadraticSum)
            self.assertEqual(e_inplace.offset, expected_offset)
            self.assertDictEqual(dict(e_inplace.linear_terms), expected_linear_terms)
            self.assertDictEqual(
                dict(e_inplace.quadratic_terms), expected_quadratic_terms
            )


class LinearLinearAddSubTestParams(NamedTuple):
    lhs_type: str
    rhs_type: str
    subtract: bool

    def test_suffix(self):
        return (
            f"_{self.lhs_type}_{self.rhs_type}_"
            f'{"subtract" if self.subtract else "add"}'
        )


def all_linear_linear_add_sub_params() -> List[LinearLinearAddSubTestParams]:
    result = []
    for lhs_type in _LINEAR_TYPES:
        for rhs_type in _LINEAR_TYPES:
            for sub in (True, False):
                result.append(
                    LinearLinearAddSubTestParams(
                        lhs_type=lhs_type, rhs_type=rhs_type, subtract=sub
                    )
                )
    return result


# Test add/sub operations (including inplace) between two Linear objects.
class LinearLinearAddSubTest(parameterized.TestCase):

    @parameterized.named_parameters(
        (p.test_suffix(), p.lhs_type, p.rhs_type, p.subtract)
        for p in all_linear_linear_add_sub_params()
    )
    def test_add_and_sub(self, lhs_type: str, rhs_type: str, subtract: bool) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        x_coefficient = 0
        y_coefficient = 0
        expected_offset = 0
        sign = -1 if subtract else 1
        # Setup first linear term.
        if lhs_type == "Variable":
            first_linear = x
            x_coefficient += 1
        elif lhs_type == "LinearTerm":
            first_linear = variables.LinearTerm(x, 2)
            x_coefficient += 2
        elif lhs_type == "LinearExpression":
            first_linear = variables.LinearExpression(x - 2 * y + 1)
            x_coefficient += 1
            y_coefficient += -2
            expected_offset += 1
        elif lhs_type == "LinearSum":
            first_linear = variables.LinearSum((x, -2 * y, 1))
            x_coefficient += 1
            y_coefficient += -2
            expected_offset += 1
        elif lhs_type == "LinearProduct":
            first_linear = variables.LinearProduct(2, x)
            x_coefficient += 2
        else:
            raise AssertionError(f"unknown linear type: {lhs_type!r}")

        # Setup second linear term
        if rhs_type == "Variable":
            second_linear = y
            y_coefficient += sign * 1
        elif rhs_type == "LinearTerm":
            second_linear = variables.LinearTerm(y, 2)
            y_coefficient += sign * 2
        elif rhs_type == "LinearExpression":
            second_linear = variables.LinearExpression(y - 2 * x + 1)
            x_coefficient += sign * (-2)
            y_coefficient += sign * 1
            expected_offset += sign * 1
        elif rhs_type == "LinearSum":
            second_linear = variables.LinearSum((y, -2 * x, 1))
            x_coefficient += sign * (-2)
            y_coefficient += sign * 1
            expected_offset += sign * 1
        elif rhs_type == "LinearProduct":
            second_linear = variables.LinearProduct(2, y)
            y_coefficient += sign * 2
        else:
            raise AssertionError(f"unknown linear type: {rhs_type!r}")

        # Check __add__ and __sub__
        s = first_linear - second_linear if subtract else first_linear + second_linear
        e = variables.as_flat_linear_expression(s)
        self.assertIsInstance(s, variables.LinearSum)
        self.assertEqual(e.offset, expected_offset)
        self.assertDictEqual(dict(e.terms), {x: x_coefficient, y: y_coefficient})

        # Also check __iadd__ and __isub__
        if subtract:
            first_linear -= second_linear
        else:
            first_linear += second_linear
        e_inplace = variables.as_flat_linear_expression(first_linear)
        self.assertIsInstance(first_linear, variables.LinearSum)
        self.assertEqual(e_inplace.offset, expected_offset)
        self.assertDictEqual(
            dict(e_inplace.terms), {x: x_coefficient, y: y_coefficient}
        )


class LinearQuadraticAddSubTestParams(NamedTuple):
    lhs_type: str
    rhs_type: str
    subtract: bool

    def test_suffix(self):
        return (
            f"_{self.lhs_type}_{self.rhs_type}_"
            f'{"subtract" if self.subtract else "add"}'
        )


def all_linear_quadratic_add_sub_params() -> List[LinearQuadraticAddSubTestParams]:
    result = []
    for lhs_type in _LINEAR_TYPES + _QUADRATIC_TYPES:
        for rhs_type in _LINEAR_TYPES + _QUADRATIC_TYPES:
            for sub in (True, False):
                result.append(
                    LinearQuadraticAddSubTestParams(
                        lhs_type=lhs_type, rhs_type=rhs_type, subtract=sub
                    )
                )
    return result


# Test add/sub operations (including inplace) between Quadratic and Linear
# objects. Also re-checks the operations for the pure Linear check when the
# result is intereted as a QuadraticExpression.
class LinearQuadraticAddSubTest(parameterized.TestCase):

    def assertDictEqualWithZeroDefault(
        self, dict1: dict[Any, float], dict2: dict[Any, float]
    ) -> None:
        for key in dict1.keys():
            if key not in dict2:
                dict2[key] = 0.0
        for key in dict2.keys():
            if key not in dict1:
                dict1[key] = 0.0
        self.assertDictEqual(dict1, dict2)

    @parameterized.named_parameters(
        (p.test_suffix(), p.lhs_type, p.rhs_type, p.subtract)
        for p in all_linear_quadratic_add_sub_params()
    )
    def test_add_and_sub(self, lhs_type: str, rhs_type: str, subtract: bool) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        xx = variables.QuadraticTermKey(x, x)
        xy = variables.QuadraticTermKey(x, y)
        yy = variables.QuadraticTermKey(y, y)
        x_coefficient = 0
        y_coefficient = 0
        xx_coefficient = 0
        xy_coefficient = 0
        yy_coefficient = 0
        expected_offset = 0
        sign = -1 if subtract else 1
        # Setup first linear term.
        if lhs_type == "Variable":
            first_linear_or_quadratic = x
            x_coefficient += 1
        elif lhs_type == "LinearTerm":
            first_linear_or_quadratic = variables.LinearTerm(x, 2)
            x_coefficient += 2
        elif lhs_type == "LinearExpression":
            first_linear_or_quadratic = variables.LinearExpression(x - 2 * y + 1)
            x_coefficient += 1
            y_coefficient += -2
            expected_offset += 1
        elif lhs_type == "LinearSum":
            first_linear_or_quadratic = variables.LinearSum((x, -2 * y, 1))
            x_coefficient += 1
            y_coefficient += -2
            expected_offset += 1
        elif lhs_type == "LinearProduct":
            first_linear_or_quadratic = variables.LinearProduct(2, x)
            x_coefficient += 2
        elif lhs_type == "QuadraticTerm":
            first_linear_or_quadratic = variables.QuadraticTerm(xx, 2)
            xx_coefficient += 2
        elif lhs_type == "QuadraticExpression":
            first_linear_or_quadratic = variables.QuadraticExpression(
                x - 2 * y + 1 + 3 * x * x - 4 * x * y
            )
            x_coefficient += 1
            y_coefficient += -2
            expected_offset += 1
            xx_coefficient += 3
            xy_coefficient += -4
        elif lhs_type == "QuadraticSum":
            first_linear_or_quadratic = variables.QuadraticSum(
                (x, -2 * y, 1, y * y, -2 * x * y)
            )
            x_coefficient += 1
            y_coefficient += -2
            expected_offset += 1
            yy_coefficient += 1
            xy_coefficient += -2
        elif lhs_type == "LinearLinearProduct":
            first_linear_or_quadratic = variables.LinearLinearProduct(y, x + y)
            yy_coefficient += 1
            xy_coefficient += 1
        elif lhs_type == "QuadraticProduct":
            first_linear_or_quadratic = variables.QuadraticProduct(2, y * (x + y))
            yy_coefficient += 2
            xy_coefficient += 2
        else:
            raise AssertionError(f"unknown linear type: {lhs_type!r}")

        # Setup second linear term
        if rhs_type == "Variable":
            second_linear_or_quadratic = y
            y_coefficient += 1 * sign
        elif rhs_type == "LinearTerm":
            second_linear_or_quadratic = variables.LinearTerm(y, 2)
            y_coefficient += 2 * sign
        elif rhs_type == "LinearExpression":
            second_linear_or_quadratic = variables.LinearExpression(y - 2 * x + 1)
            x_coefficient += -2 * sign
            y_coefficient += 1 * sign
            expected_offset += 1 * sign
        elif rhs_type == "LinearSum":
            second_linear_or_quadratic = variables.LinearSum((y, -2 * x, 1))
            x_coefficient += -2 * sign
            y_coefficient += 1 * sign
            expected_offset += 1 * sign
        elif rhs_type == "LinearProduct":
            second_linear_or_quadratic = variables.LinearProduct(2, y)
            y_coefficient += 2 * sign
        elif rhs_type == "QuadraticTerm":
            second_linear_or_quadratic = variables.QuadraticTerm(xy, 5)
            xy_coefficient += 5 * sign
        elif rhs_type == "QuadraticExpression":
            second_linear_or_quadratic = variables.QuadraticExpression(
                x - 2 * y + 1 + 3 * x * y - 4 * y * y
            )
            x_coefficient += 1 * sign
            y_coefficient += -2 * sign
            expected_offset += 1 * sign
            xy_coefficient += 3 * sign
            yy_coefficient += -4 * sign
        elif rhs_type == "QuadraticSum":
            second_linear_or_quadratic = variables.QuadraticSum(
                (x, -2 * y, 1, y * x, -2 * x * x)
            )
            x_coefficient += 1 * sign
            y_coefficient += -2 * sign
            expected_offset += 1 * sign
            xy_coefficient += 1 * sign
            xx_coefficient += -2 * sign
        elif rhs_type == "LinearLinearProduct":
            second_linear_or_quadratic = variables.LinearLinearProduct(x, x + y)
            xx_coefficient += sign
            xy_coefficient += sign
        elif rhs_type == "QuadraticProduct":
            second_linear_or_quadratic = variables.QuadraticProduct(2, x * (x + y))
            xx_coefficient += 2 * sign
            xy_coefficient += 2 * sign
        else:
            raise AssertionError(f"unknown linear type: {lhs_type!r}")

        # Check __add__ and __sub__
        s = (
            first_linear_or_quadratic - second_linear_or_quadratic
            if subtract
            else first_linear_or_quadratic + second_linear_or_quadratic
        )
        e = variables.as_flat_quadratic_expression(s)
        self.assertEqual(e.offset, expected_offset)
        self.assertDictEqualWithZeroDefault(
            dict(e.linear_terms), {x: x_coefficient, y: y_coefficient}
        )
        self.assertDictEqualWithZeroDefault(
            dict(e.quadratic_terms),
            {xx: xx_coefficient, xy: xy_coefficient, yy: yy_coefficient},
        )

        # Also check __iadd__ and __isub__
        if subtract:
            first_linear_or_quadratic -= second_linear_or_quadratic
        else:
            first_linear_or_quadratic += second_linear_or_quadratic
        e_inplace = variables.as_flat_quadratic_expression(first_linear_or_quadratic)
        self.assertEqual(e_inplace.offset, expected_offset)
        self.assertDictEqualWithZeroDefault(
            dict(e_inplace.linear_terms), {x: x_coefficient, y: y_coefficient}
        )
        self.assertDictEqualWithZeroDefault(
            dict(e_inplace.quadratic_terms),
            {xx: xx_coefficient, xy: xy_coefficient, yy: yy_coefficient},
        )


# Test multiplication of two Linear objects.
class LinearLinearMulTest(parameterized.TestCase):

    def assertDictEqualWithZeroDefault(
        self, dict1: dict[Any, float], dict2: dict[Any, float]
    ) -> None:
        for key in dict1.keys():
            if key not in dict2:
                dict2[key] = 0.0
        for key in dict2.keys():
            if key not in dict1:
                dict1[key] = 0.0
        self.assertDictEqual(dict1, dict2)

    @parameterized.named_parameters(("_x_first", True), ("_y_first", False))
    def test_var_var(self, x_first: bool) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        xy = variables.QuadraticTermKey(x, y)
        if x_first:
            s = x * y
        else:
            s = y * x
        self.assertIsInstance(s, variables.QuadraticTerm)
        e = variables.as_flat_quadratic_expression(s)
        self.assertDictEqualWithZeroDefault({xy: 1.0}, dict(e.quadratic_terms))
        self.assertDictEqualWithZeroDefault({}, dict(e.linear_terms))
        self.assertEqual(0.0, e.offset)

    @parameterized.named_parameters(("_var_first", True), ("_term_first", False))
    def test_term_term(self, var_first: bool) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        xy = variables.QuadraticTermKey(x, y)
        if var_first:
            s = variables.LinearTerm(x, 2) * variables.LinearTerm(y, 3)
        else:
            s = variables.LinearTerm(x, 3) * variables.LinearTerm(y, 2)
        self.assertIsInstance(s, variables.QuadraticTerm)
        e = variables.as_flat_quadratic_expression(s)
        self.assertDictEqualWithZeroDefault({xy: 6.0}, dict(e.quadratic_terms))
        self.assertDictEqualWithZeroDefault({}, dict(e.linear_terms))
        self.assertEqual(0.0, e.offset)

    @parameterized.named_parameters(("_expr1_first", True), ("_expr2_first", False))
    def test_expr_expr(self, expr1_first: bool) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        xx = variables.QuadraticTermKey(x, x)
        xy = variables.QuadraticTermKey(x, y)
        yy = variables.QuadraticTermKey(y, y)
        expr1 = variables.LinearExpression(x - 2 * y + 3)
        expr2 = variables.LinearExpression(-x + y + 1)
        if expr1_first:
            s = expr1 * expr2
        else:
            s = expr2 * expr1
        self.assertIsInstance(s, variables.LinearLinearProduct)
        e = variables.as_flat_quadratic_expression(s)
        self.assertDictEqualWithZeroDefault(
            {
                xx: -1.0,
                xy: 3.0,
                yy: -2.0,
            },
            dict(e.quadratic_terms),
        )
        self.assertDictEqualWithZeroDefault({x: -2.0, y: 1.0}, dict(e.linear_terms))
        self.assertEqual(3.0, e.offset)

    @parameterized.named_parameters(("_sum1_first", True), ("_sum2_first", False))
    def test_sum_sum(self, sum1_first: bool) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        xx = variables.QuadraticTermKey(x, x)
        xy = variables.QuadraticTermKey(x, y)
        yy = variables.QuadraticTermKey(y, y)
        sum1 = variables.LinearSum((x, -2 * y, 3))
        sum2 = variables.LinearSum((-x, y, 1))
        if sum1_first:
            s = sum1 * sum2
        else:
            s = sum2 * sum1
        self.assertIsInstance(s, variables.LinearLinearProduct)
        e = variables.as_flat_quadratic_expression(s)
        self.assertDictEqualWithZeroDefault(
            {
                xx: -1.0,
                xy: 3.0,
                yy: -2.0,
            },
            dict(e.quadratic_terms),
        )
        self.assertDictEqualWithZeroDefault({x: -2.0, y: 1.0}, dict(e.linear_terms))
        self.assertEqual(3.0, e.offset)

    @parameterized.named_parameters(("_prod1_first", True), ("_prod2_first", False))
    def test_prod_prod(self, prod1_first: bool) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        xx = variables.QuadraticTermKey(x, x)
        xy = variables.QuadraticTermKey(x, y)
        prod1 = variables.LinearProduct(2.0, x)
        prod2 = variables.LinearProduct(3.0, x + 2 * y - 1)
        if prod1_first:
            s = prod1 * prod2
        else:
            s = prod2 * prod1
        self.assertIsInstance(s, variables.LinearLinearProduct)
        e = variables.as_flat_quadratic_expression(s)
        self.assertDictEqualWithZeroDefault(
            {
                xx: 6.0,
                xy: 12.0,
            },
            dict(e.quadratic_terms),
        )
        self.assertDictEqualWithZeroDefault({x: -6.0}, dict(e.linear_terms))
        self.assertEqual(0.0, e.offset)

    @parameterized.named_parameters(("_var_first", True), ("_term_first", False))
    def test_var_term(self, var_first: bool) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        xy = variables.QuadraticTermKey(x, y)
        term = variables.LinearTerm(y, 2)
        if var_first:
            s = x * term
        else:
            s = term * x
        self.assertIsInstance(s, variables.QuadraticTerm)
        e = variables.as_flat_quadratic_expression(s)
        self.assertDictEqualWithZeroDefault({xy: 2.0}, dict(e.quadratic_terms))
        self.assertDictEqualWithZeroDefault({}, dict(e.linear_terms))
        self.assertEqual(0.0, e.offset)

    @parameterized.named_parameters(("_var_first", True), ("_expr_first", False))
    def test_var_expr(self, var_first: bool) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        xx = variables.QuadraticTermKey(x, x)
        xy = variables.QuadraticTermKey(x, y)
        expr = variables.LinearExpression(x - 2 * y + 3)
        if var_first:
            s = x * expr
        else:
            s = expr * x
        self.assertIsInstance(s, variables.LinearLinearProduct)
        e = variables.as_flat_quadratic_expression(s)
        self.assertDictEqualWithZeroDefault(
            {xx: 1.0, xy: -2.0}, dict(e.quadratic_terms)
        )
        self.assertDictEqualWithZeroDefault({x: 3.0}, dict(e.linear_terms))
        self.assertEqual(0.0, e.offset)

    @parameterized.named_parameters(("_var_first", True), ("_sum_first", False))
    def test_var_sum(self, var_first: bool) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        xx = variables.QuadraticTermKey(x, x)
        xy = variables.QuadraticTermKey(x, y)
        linear_sum = variables.LinearSum((x, -2 * y, 3))
        if var_first:
            s = x * linear_sum
        else:
            s = linear_sum * x
        self.assertIsInstance(s, variables.LinearLinearProduct)
        e = variables.as_flat_quadratic_expression(s)
        self.assertDictEqualWithZeroDefault(
            {xx: 1.0, xy: -2.0}, dict(e.quadratic_terms)
        )
        self.assertDictEqualWithZeroDefault({x: 3.0}, dict(e.linear_terms))
        self.assertEqual(0.0, e.offset)

    @parameterized.named_parameters(("_var_first", True), ("_prod_first", False))
    def test_var_prod(self, var_first: bool) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        xy = variables.QuadraticTermKey(x, y)
        expr = variables.LinearProduct(2.0, y)
        if var_first:
            s = x * expr
        else:
            s = expr * x
        self.assertIsInstance(s, variables.LinearLinearProduct)
        e = variables.as_flat_quadratic_expression(s)
        self.assertDictEqualWithZeroDefault({xy: 2.0}, dict(e.quadratic_terms))
        self.assertDictEqualWithZeroDefault({}, dict(e.linear_terms))
        self.assertEqual(0.0, e.offset)

    @parameterized.named_parameters(("_term_first", True), ("_expr_first", False))
    def test_term_expr(self, term_first: bool) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        xx = variables.QuadraticTermKey(x, x)
        xy = variables.QuadraticTermKey(x, y)
        term = variables.LinearTerm(x, 2)
        expr = variables.LinearExpression(x - 2 * y + 3)
        if term_first:
            s = term * expr
        else:
            s = expr * term
        self.assertIsInstance(s, variables.LinearLinearProduct)
        e = variables.as_flat_quadratic_expression(s)
        self.assertDictEqualWithZeroDefault(
            {xx: 2.0, xy: -4.0}, dict(e.quadratic_terms)
        )
        self.assertDictEqualWithZeroDefault({x: 6.0}, dict(e.linear_terms))
        self.assertEqual(0.0, e.offset)

    @parameterized.named_parameters(("_term_first", True), ("_sum_first", False))
    def test_term_sum(self, term_first: bool) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        xx = variables.QuadraticTermKey(x, x)
        xy = variables.QuadraticTermKey(x, y)
        term = variables.LinearTerm(x, 2)
        linear_sum = variables.LinearSum((x, -2 * y, 3))
        if term_first:
            s = term * linear_sum
        else:
            s = linear_sum * term
        self.assertIsInstance(s, variables.LinearLinearProduct)
        e = variables.as_flat_quadratic_expression(s)
        self.assertDictEqualWithZeroDefault(
            {xx: 2.0, xy: -4.0}, dict(e.quadratic_terms)
        )
        self.assertDictEqualWithZeroDefault({x: 6.0}, dict(e.linear_terms))
        self.assertEqual(0.0, e.offset)

    @parameterized.named_parameters(("_term_first", True), ("_prod_first", False))
    def test_term_prod(self, term_first: bool) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        xy = variables.QuadraticTermKey(x, y)
        term = variables.LinearTerm(x, 2)
        prod = variables.LinearProduct(2.0, y)
        if term_first:
            s = term * prod
        else:
            s = prod * term
        self.assertIsInstance(s, variables.LinearLinearProduct)
        e = variables.as_flat_quadratic_expression(s)
        self.assertDictEqualWithZeroDefault({xy: 4.0}, dict(e.quadratic_terms))
        self.assertDictEqualWithZeroDefault({}, dict(e.linear_terms))
        self.assertEqual(0.0, e.offset)

    @parameterized.named_parameters(("_expr_first", True), ("_sum_first", False))
    def test_expr_sum(self, expr_first: bool) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        xx = variables.QuadraticTermKey(x, x)
        xy = variables.QuadraticTermKey(x, y)
        yy = variables.QuadraticTermKey(y, y)
        expr = variables.LinearExpression(-x + y + 1)
        linear_sum = variables.LinearSum((x, -2 * y, 3))
        if expr_first:
            s = expr * linear_sum
        else:
            s = linear_sum * expr
        self.assertIsInstance(s, variables.LinearLinearProduct)
        e = variables.as_flat_quadratic_expression(s)
        self.assertDictEqualWithZeroDefault(
            {
                xx: -1.0,
                xy: 3.0,
                yy: -2.0,
            },
            dict(e.quadratic_terms),
        )
        self.assertDictEqualWithZeroDefault({x: -2.0, y: 1.0}, dict(e.linear_terms))
        self.assertEqual(3.0, e.offset)

    @parameterized.named_parameters(("_expr_first", True), ("_prod_first", False))
    def test_expr_prod(self, expr_first: bool) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        xy = variables.QuadraticTermKey(x, y)
        yy = variables.QuadraticTermKey(y, y)
        expr = variables.LinearExpression(-x + y + 1)
        prod = variables.LinearProduct(2.0, y)
        if expr_first:
            s = expr * prod
        else:
            s = prod * expr
        self.assertIsInstance(s, variables.LinearLinearProduct)
        e = variables.as_flat_quadratic_expression(s)
        self.assertDictEqualWithZeroDefault(
            {xy: -2.0, yy: 2.0}, dict(e.quadratic_terms)
        )
        self.assertDictEqualWithZeroDefault({y: 2.0}, dict(e.linear_terms))
        self.assertEqual(0.0, e.offset)

    @parameterized.named_parameters(("_sum_first", True), ("_prod_first", False))
    def test_sum_prod(self, sum_first: bool) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        xy = variables.QuadraticTermKey(x, y)
        yy = variables.QuadraticTermKey(y, y)
        linear_sum = variables.LinearSum((-x, y, 1))
        prod = variables.LinearProduct(2.0, y)
        if sum_first:
            s = linear_sum * prod
        else:
            s = prod * linear_sum
        self.assertIsInstance(s, variables.LinearLinearProduct)
        e = variables.as_flat_quadratic_expression(s)
        self.assertDictEqualWithZeroDefault(
            {xy: -2.0, yy: 2.0}, dict(e.quadratic_terms)
        )
        self.assertDictEqualWithZeroDefault({y: 2.0}, dict(e.linear_terms))
        self.assertEqual(0.0, e.offset)


# Test negate on Linear and Quadratic objects.
class NegateTest(parameterized.TestCase):

    def test_negate_var(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        s = -x
        self.assertIsInstance(s, variables.LinearTerm)
        e = variables.as_flat_linear_expression(s)
        self.assertEqual(0, e.offset)
        self.assertDictEqual({x: -1}, dict(e.terms))

    def test_negate_linear_term(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        term = variables.LinearTerm(x, 0.5)
        s = -term
        self.assertIsInstance(s, variables.LinearTerm)
        e = variables.as_flat_linear_expression(s)
        self.assertEqual(0, e.offset)
        self.assertDictEqual({x: -0.5}, dict(e.terms))

    def test_negate_linear_expression(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        expression = variables.LinearExpression(y - 2 * x + 1)
        s = -expression
        self.assertIsInstance(s, variables.LinearProduct)
        e = variables.as_flat_linear_expression(s)
        self.assertEqual(-1, e.offset)
        self.assertDictEqual({x: 2, y: -1}, dict(e.terms))

    def test_negate_linear_sum(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        expression = variables.LinearSum((y, -2 * x, 1))
        s = -expression
        self.assertIsInstance(s, variables.LinearProduct)
        e = variables.as_flat_linear_expression(s)
        self.assertEqual(-1, e.offset)
        self.assertDictEqual({x: 2, y: -1}, dict(e.terms))

    def test_negate_ast_product(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        ast_product = variables.LinearProduct(0.5, x)
        s = -ast_product
        self.assertIsInstance(s, variables.LinearProduct)
        e = variables.as_flat_linear_expression(s)
        self.assertEqual(0, e.offset)
        self.assertDictEqual({x: -0.5}, dict(e.terms))

    def test_negate_quadratic_term(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        xx = variables.QuadraticTermKey(x, x)
        term = variables.QuadraticTerm(xx, 0.5)
        s = -term
        self.assertIsInstance(s, variables.QuadraticTerm)
        e = variables.as_flat_quadratic_expression(s)
        self.assertEqual(0, e.offset)
        self.assertDictEqual({}, dict(e.linear_terms))
        self.assertDictEqual({xx: -0.5}, dict(e.quadratic_terms))

    def test_negate_quadratic_expression(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        xy = variables.QuadraticTermKey(x, y)
        expression = variables.QuadraticExpression(y - 2 * x + 1 + x * y)
        s = -expression
        self.assertIsInstance(s, variables.QuadraticProduct)
        e = variables.as_flat_quadratic_expression(s)
        self.assertEqual(-1, e.offset)
        self.assertDictEqual({x: 2, y: -1}, dict(e.linear_terms))
        self.assertDictEqual({xy: -1}, dict(e.quadratic_terms))

    def test_negate_quadratic_sum(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        yy = variables.QuadraticTermKey(y, y)
        expression = variables.QuadraticSum((y, -2 * x, 1, -y * y))
        s = -expression
        self.assertIsInstance(s, variables.QuadraticProduct)
        e = variables.as_flat_quadratic_expression(s)
        self.assertEqual(-1, e.offset)
        self.assertDictEqual({x: 2, y: -1}, dict(e.linear_terms))
        self.assertDictEqual({yy: 1}, dict(e.quadratic_terms))

    def test_negate_linear_linear_product(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        xx = variables.QuadraticTermKey(x, x)
        ast_product = variables.LinearLinearProduct(x, x + 1)
        s = -ast_product
        self.assertIsInstance(s, variables.QuadraticProduct)
        e = variables.as_flat_quadratic_expression(s)
        self.assertEqual(0, e.offset)
        self.assertDictEqual({x: -1}, dict(e.linear_terms))
        self.assertDictEqual({xx: -1}, dict(e.quadratic_terms))

    def test_negate_quadratic_product(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        xx = variables.QuadraticTermKey(x, x)
        ast_product = variables.QuadraticProduct(0.5, x + x * x)
        s = -ast_product
        self.assertIsInstance(s, variables.QuadraticProduct)
        e = variables.as_flat_quadratic_expression(s)
        self.assertEqual(0, e.offset)
        self.assertDictEqual({x: -0.5}, dict(e.linear_terms))
        self.assertDictEqual({xx: -0.5}, dict(e.quadratic_terms))


class UnsupportedProductOperandTestParams(NamedTuple):
    lhs_type: str
    rhs_type: str

    def test_suffix(self):
        return f"_{self.lhs_type}_{self.rhs_type}"


def all_unsupported_product_operand_params() -> (
    List[UnsupportedProductOperandTestParams]
):
    result = []
    for lhs_type in _LINEAR_TYPES:
        result.append(
            UnsupportedProductOperandTestParams(lhs_type=lhs_type, rhs_type="complex")
        )
    for lhs_type in _QUADRATIC_TYPES + ("complex",):
        for rhs_type in _QUADRATIC_TYPES + ("complex",):
            if lhs_type == "complex" and rhs_type == "complex":
                continue
            result.append(
                UnsupportedProductOperandTestParams(
                    lhs_type=lhs_type, rhs_type=rhs_type
                )
            )
    return result


def all_unsupported_division_operand_params() -> (
    List[UnsupportedProductOperandTestParams]
):
    result = []
    for lhs_type in _LINEAR_TYPES + _QUADRATIC_TYPES + ("complex",):
        for rhs_type in _LINEAR_TYPES + _QUADRATIC_TYPES + ("complex",):
            if lhs_type == "complex" and rhs_type == "complex":
                continue
            result.append(
                UnsupportedProductOperandTestParams(
                    lhs_type=lhs_type, rhs_type=rhs_type
                )
            )
    return result


def get_linear_or_quadratic_for_unsupported_operand_test(
    type_string: str,
) -> Union[variables.LinearBase, variables.QuadraticBase, complex]:
    mod_ = model.Model()
    x = mod_.add_binary_variable(name="x")
    y = mod_.add_binary_variable(name="y")
    xy = variables.QuadraticTermKey(x, y)
    if type_string == "Variable":
        return x
    elif type_string == "LinearTerm":
        return variables.LinearTerm(x, 2)
    elif type_string == "LinearExpression":
        return variables.LinearExpression(x - 2 * y + 3)
    elif type_string == "LinearSum":
        return variables.LinearSum((x, -2 * y, 3))
    elif type_string == "LinearProduct":
        return variables.LinearProduct(2, x)
    elif type_string == "QuadraticTerm":
        return variables.QuadraticTerm(xy, 5)
    elif type_string == "QuadraticExpression":
        return variables.QuadraticExpression(x - 2 * y + 1 + 3 * x * y - 4 * y * y)
    elif type_string == "QuadraticSum":
        return variables.QuadraticSum((x, -2 * y, 1, y * x, -2 * x * x))
    elif type_string == "LinearLinearProduct":
        return variables.LinearLinearProduct(x, x + y)
    elif type_string == "QuadraticProduct":
        return variables.QuadraticProduct(2, x * (x + y))
    elif type_string == "complex":
        return 6j
    else:
        raise AssertionError(f"unknown linear or quadratic type: {type_string!r}")


class UnsupportedProductOperandTest(parameterized.TestCase):

    @parameterized.named_parameters(
        (p.test_suffix(), p.lhs_type, p.rhs_type)
        for p in all_unsupported_product_operand_params()
    )
    def test_mult(self, lhs_type: str, rhs_type: str) -> None:
        lhs = get_linear_or_quadratic_for_unsupported_operand_test(lhs_type)
        rhs = get_linear_or_quadratic_for_unsupported_operand_test(rhs_type)

        expected_string = f"unsupported operand.*[*].*{lhs_type}.*and.*{rhs_type}"

        # pylint: disable=pointless-statement
        # pytype: disable=unsupported-operands
        # pytype: disable=wrong-arg-types
        with self.assertRaisesRegex(TypeError, expected_string):
            lhs * rhs

        with self.assertRaisesRegex(TypeError, expected_string):
            lhs *= rhs
        # pylint: enable=pointless-statement
        # pytype: enable=unsupported-operands
        # pytype: enable=wrong-arg-types

    @parameterized.named_parameters(
        (p.test_suffix(), p.lhs_type, p.rhs_type)
        for p in all_unsupported_division_operand_params()
    )
    def test_div(self, lhs_type: str, rhs_type: str) -> None:
        lhs = get_linear_or_quadratic_for_unsupported_operand_test(lhs_type)
        rhs = get_linear_or_quadratic_for_unsupported_operand_test(rhs_type)

        expected_string = f"unsupported operand.*[/].*{lhs_type}.*and.*{rhs_type}"

        # pylint: disable=pointless-statement
        # pytype: disable=unsupported-operands
        # pytype: disable=wrong-arg-types
        with self.assertRaisesRegex(TypeError, expected_string):
            lhs / rhs

        if lhs_type == "str":
            expected_string = f"unsupported operand.*[/].*{lhs_type}.*and.*{rhs_type}"
        with self.assertRaisesRegex(TypeError, expected_string):
            lhs /= rhs
        # pylint: enable=pointless-statement
        # pytype: enable=unsupported-operands
        # pytype: enable=wrong-arg-types


class UnsupportedAdditionOperandTestParams(NamedTuple):
    linear_or_quadratic_type: str
    linear_or_quadratic_first: bool

    def test_suffix(self):
        if self.linear_or_quadratic_first:
            return f"_{self.linear_or_quadratic_type}_str"
        else:
            return f"_str_{self.linear_or_quadratic_type}"


def all_unsupported_addition_operand_params() -> (
    List[UnsupportedAdditionOperandTestParams]
):
    result = []
    for linear_or_quadratic_type in _LINEAR_TYPES + _QUADRATIC_TYPES:
        for linear_or_quadratic_first in (True, False):
            result.append(
                UnsupportedAdditionOperandTestParams(
                    linear_or_quadratic_type=linear_or_quadratic_type,
                    linear_or_quadratic_first=linear_or_quadratic_first,
                )
            )
    return result


@parameterized.named_parameters(
    (p.test_suffix(), p.linear_or_quadratic_type, p.linear_or_quadratic_first)
    for p in all_unsupported_addition_operand_params()
)
class UnsupportedAdditionOperandTest(parameterized.TestCase):

    def test_add(
        self, linear_or_quadratic_type: str, linear_or_quadratic_first: bool
    ) -> None:
        linear_or_quadratic = get_linear_or_quadratic_for_unsupported_operand_test(
            linear_or_quadratic_type
        )
        other = 6j

        expected_string = r"unsupported operand type\(s\) for \+.*"
        if linear_or_quadratic_first:
            expected_string += (
                f"{linear_or_quadratic_type}.*and.*{type(other).__name__}.*"
            )
        else:
            expected_string += (
                f"{type(other).__name__}.*and.*{linear_or_quadratic_type}.*"
            )

        # pylint: disable=pointless-statement
        # pytype: disable=unsupported-operands
        # pytype: disable=wrong-arg-types
        with self.assertRaisesRegex(TypeError, expected_string):
            if linear_or_quadratic_first:
                linear_or_quadratic + other
            else:
                other + linear_or_quadratic

        with self.assertRaisesRegex(TypeError, expected_string):
            if linear_or_quadratic_first:
                linear_or_quadratic += other
            else:
                other += linear_or_quadratic
        # pylint: enable=pointless-statement
        # pytype: enable=unsupported-operands
        # pytype: enable=wrong-arg-types

    def test_sub(
        self, linear_or_quadratic_type: str, linear_or_quadratic_first: bool
    ) -> None:
        linear_or_quadratic = get_linear_or_quadratic_for_unsupported_operand_test(
            linear_or_quadratic_type
        )
        other = 6j

        expected_string = "unsupported operand type[(]s[)] for [-].*"
        if linear_or_quadratic_first:
            expected_string += (
                f"{linear_or_quadratic_type}.*and.*{type(other).__name__}.*"
            )
        else:
            expected_string += (
                f"{type(other).__name__}.*and.*{linear_or_quadratic_type}.*"
            )

        # pylint: disable=pointless-statement
        # pytype: disable=unsupported-operands
        # pytype: disable=wrong-arg-types
        with self.assertRaisesRegex(TypeError, expected_string):
            if linear_or_quadratic_first:
                linear_or_quadratic - other
            else:
                other - linear_or_quadratic

        with self.assertRaisesRegex(TypeError, expected_string):
            if linear_or_quadratic_first:
                linear_or_quadratic -= other
            else:
                other -= linear_or_quadratic
        # pylint: enable=pointless-statement
        # pytype: enable=unsupported-operands
        # pytype: enable=wrong-arg-types


class UnsupportedInitializationTest(parameterized.TestCase):

    def test_linear_sum_not_tuple(self):
        # pytype: disable=wrong-arg-types
        with self.assertRaisesRegex(TypeError, "object is not iterable"):
            variables.LinearSum(2.0)
        # pytype: enable=wrong-arg-types

    def test_linear_sum_not_linear_in_tuple(self):
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        # pytype: disable=wrong-arg-types
        with self.assertRaisesRegex(TypeError, "unsupported type in iterable argument"):
            variables.LinearSum((2.0, x * x))
        # pytype: enable=wrong-arg-types

    def test_quadratic_sum_not_tuple(self):
        # pytype: disable=wrong-arg-types
        with self.assertRaisesRegex(TypeError, "object is not iterable"):
            variables.QuadraticSum(2.0)
        # pytype: enable=wrong-arg-types

    def test_quadratic_sum_not_linear_in_tuple(self):
        # pytype: disable=wrong-arg-types
        with self.assertRaisesRegex(TypeError, "unsupported type in iterable argument"):
            variables.QuadraticSum((2.0, "string"))
        # pytype: enable=wrong-arg-types

    def test_linear_product_not_scalar(self):
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        # pytype: disable=wrong-arg-types
        with self.assertRaisesRegex(
            TypeError, "unsupported type for scalar argument in LinearProduct"
        ):
            variables.LinearProduct(x, x)
        # pytype: enable=wrong-arg-types

    def test_linear_product_not_linear(self):
        # pytype: disable=wrong-arg-types
        with self.assertRaisesRegex(
            TypeError, "unsupported type for linear argument in LinearProduct"
        ):
            variables.LinearProduct(2.0, "string")
        # pytype: enable=wrong-arg-types

    def test_quadratic_product_not_scalar(self):
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        # pytype: disable=wrong-arg-types
        with self.assertRaisesRegex(
            TypeError, "unsupported type for scalar argument in QuadraticProduct"
        ):
            variables.QuadraticProduct(x, x)
        # pytype: enable=wrong-arg-types

    def test_quadratic_product_not_quadratic(self):
        # pytype: disable=wrong-arg-types
        with self.assertRaisesRegex(
            TypeError, "unsupported type for linear argument in QuadraticProduct"
        ):
            variables.QuadraticProduct(2.0, "string")
        # pytype: enable=wrong-arg-types

    def test_linear_linear_product_first_not_linear(self):
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        # pytype: disable=wrong-arg-types
        with self.assertRaisesRegex(
            TypeError,
            "unsupported type for first_linear argument in LinearLinearProduct",
        ):
            variables.LinearLinearProduct("string", x)
        # pytype: enable=wrong-arg-types

    def test_linear_linear_product_second_not_linear(self):
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        # pytype: disable=wrong-arg-types
        with self.assertRaisesRegex(
            TypeError,
            "unsupported type for second_linear argument in LinearLinearProduct",
        ):
            variables.LinearLinearProduct(x, "string")
        # pytype: enable=wrong-arg-types


@parameterized.named_parameters(("_python_sum", True), ("LinearSum", False))
class SumTest(parameterized.TestCase):

    def test_sum_vars(self, python_sum: bool) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")
        array = [x, z, x, x, y]
        if python_sum:
            s = sum(array) + 8.0
        else:
            s = variables.LinearSum(array) + 8.0
        e = variables.as_flat_linear_expression(s)
        self.assertEqual(8.0, e.offset)
        self.assertDictEqual({x: 3, y: 1, z: 1}, dict(e.terms))

    def test_sum_linear_terms(self, python_sum: bool) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")
        array = [1.25 * x, z, x, x, y, -0.5 * y, 1.0]
        if python_sum:
            s = sum(array) + 8.0
        else:
            s = variables.LinearSum(array) + 8.0
        e = variables.as_flat_linear_expression(s)
        self.assertEqual(9.0, e.offset)
        self.assertDictEqual({x: 3.25, y: 0.5, z: 1}, dict(e.terms))

    def test_sum_quadratic_terms(self, python_sum: bool) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")
        xx = variables.QuadraticTermKey(x, x)
        xy = variables.QuadraticTermKey(x, y)
        array = [1.25 * x, z, x, x, y, -0.5 * y, 1.0, 2.5 * x * x, -x * y]
        if python_sum:
            s = sum(array) + 8.0
        else:
            s = variables.QuadraticSum(array) + 8.0
        e = variables.as_flat_quadratic_expression(s)
        self.assertEqual(9.0, e.offset)
        self.assertDictEqual({x: 3.25, y: 0.5, z: 1}, dict(e.linear_terms))
        self.assertDictEqual({xx: 2.5, xy: -1}, dict(e.quadratic_terms))

    def test_sum_linear_expression(self, python_sum: bool) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")
        array = [1.25 * x + z, x, x + y, -0.5 * y + 1.0]
        if python_sum:
            s = sum(array) + 8.0
        else:
            s = variables.LinearSum(array) + 8.0
        e = variables.as_flat_linear_expression(s)
        self.assertEqual(9.0, e.offset)
        self.assertDictEqual({x: 3.25, y: 0.5, z: 1}, dict(e.terms))

    def test_sum_quadratic_expression(self, python_sum: bool) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")
        xx = variables.QuadraticTermKey(x, x)
        xy = variables.QuadraticTermKey(x, y)
        array = [
            1.25 * x + z,
            x,
            x + y,
            -0.5 * y + 1.0,
            2.5 * x * x - x * y,
            x * z + y * z,
        ]
        if python_sum:
            s = sum(array) + 8.0
        else:
            s = variables.QuadraticSum(array) + 8.0
        e = variables.as_flat_quadratic_expression(s)
        self.assertEqual(9.0, e.offset)
        self.assertDictEqual({x: 3.25, y: 0.5, z: 1}, dict(e.linear_terms))
        self.assertDictEqual(
            {
                xx: 2.5,
                xy: -1,
                variables.QuadraticTermKey(x, z): 1.0,
                variables.QuadraticTermKey(y, z): 1,
            },
            dict(e.quadratic_terms),
        )

    def test_generator_sum_vars(self, python_sum: bool) -> None:
        mod = model.Model()
        x = [mod.add_binary_variable(name=f"x[{i}]") for i in range(3)]
        if python_sum:
            s = sum(x[i] for i in range(3)) + 8.0
        else:
            s = variables.LinearSum(x[i] for i in range(3)) + 8.0
        e = variables.as_flat_linear_expression(s)
        self.assertEqual(8.0, e.offset)
        self.assertDictEqual({x[0]: 1, x[1]: 1, x[2]: 1}, dict(e.terms))

    def test_generator_sum_terms(self, python_sum: bool) -> None:
        mod = model.Model()
        x = [mod.add_binary_variable(name=f"x[{i}]") for i in range(3)]
        if python_sum:
            s = sum(i * x[i] for i in range(3)) + 8.0
        else:
            s = variables.LinearSum(i * x[i] for i in range(3)) + 8.0
        e = variables.as_flat_linear_expression(s)
        self.assertEqual(8.0, e.offset)
        self.assertDictEqual({x[0]: 0, x[1]: 1, x[2]: 2}, dict(e.terms))

    def test_generator_sum_quadratic_terms(self, python_sum: bool) -> None:
        mod = model.Model()
        x = [mod.add_binary_variable(name=f"x[{i}]") for i in range(4)]
        if python_sum:
            s = sum(i * x[i] * x[i + 1] for i in range(3)) + 8.0
        else:
            s = variables.QuadraticSum(i * x[i] * x[i + 1] for i in range(3)) + 8.0
        e = variables.as_flat_quadratic_expression(s)
        self.assertEqual(8.0, e.offset)
        self.assertDictEqual({}, dict(e.linear_terms))
        self.assertDictEqual(
            {
                variables.QuadraticTermKey(x[0], x[1]): 0,
                variables.QuadraticTermKey(x[1], x[2]): 1,
                variables.QuadraticTermKey(x[2], x[3]): 2,
            },
            dict(e.quadratic_terms),
        )

    def test_generator_sum_expression(self, python_sum: bool) -> None:
        mod = model.Model()
        x = [mod.add_binary_variable(name=f"x[{i}]") for i in range(3)]
        if python_sum:
            s = sum(2 * x[i] - x[i + 1] + 1.5 for i in range(2)) + 8.0
        else:
            s = variables.LinearSum(2 * x[i] - x[i + 1] + 1.5 for i in range(2)) + 8.0
        e = variables.as_flat_linear_expression(s)
        self.assertEqual(11.0, e.offset)
        self.assertDictEqual({x[0]: 2, x[1]: 1, x[2]: -1}, dict(e.terms))

    def test_generator_quadratic_sum_expression(self, python_sum: bool) -> None:
        mod = model.Model()
        x = [mod.add_binary_variable(name=f"x[{i}]") for i in range(3)]
        if python_sum:
            s = sum(2 * x[i] - x[i + 1] + 1.5 + x[i] * x[i + 1] for i in range(2)) + 8.0
        else:
            s = (
                variables.QuadraticSum(
                    2 * x[i] - x[i + 1] + 1.5 + x[i] * x[i + 1] for i in range(2)
                )
                + 8.0
            )
        e = variables.as_flat_quadratic_expression(s)
        self.assertEqual(11.0, e.offset)
        self.assertDictEqual({x[0]: 2, x[1]: 1, x[2]: -1}, dict(e.linear_terms))
        self.assertDictEqual(
            {
                variables.QuadraticTermKey(x[0], x[1]): 1,
                variables.QuadraticTermKey(x[1], x[2]): 1,
            },
            dict(e.quadratic_terms),
        )


class AstTest(parameterized.TestCase):

    def assertDictEqualWithZeroDefault(
        self, dict1: dict[Any, float], dict2: dict[Any, float]
    ) -> None:
        for key in dict1.keys():
            if key not in dict2:
                dict2[key] = 0.0
        for key in dict2.keys():
            if key not in dict1:
                dict1[key] = 0.0
        self.assertDictEqual(dict1, dict2)

    def test_simple_linear_ast(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")
        s = (6 * (3 * x + y + 3) + 6 * z + 12) / 3 + y + 7 + (x + y) * 2.0 - z * 3.0
        e = variables.as_flat_linear_expression(s)
        self.assertEqual(6 * 3 / 3 + 12 / 3 + 7, e.offset)
        self.assertDictEqualWithZeroDefault(
            {x: 8, y: 3 + 6 / 3, z: 6 / 3 - 3}, dict(e.terms)
        )

    def test_simple_quadratic_ast(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")
        xx = variables.QuadraticTermKey(x, x)
        xy = variables.QuadraticTermKey(x, y)
        yy = variables.QuadraticTermKey(y, y)
        s = (
            (6 * (3 * x + y + 3) * x + 6 * z + 12) / 3
            + y * y
            + 7
            + (x - y) * (x + y) * 2.0
            - z * 3.0
        )
        e = variables.as_flat_quadratic_expression(s)
        self.assertEqual(12 / 3 + 7, e.offset)
        self.assertDictEqualWithZeroDefault({x: 6, z: 6 / 3 - 3}, dict(e.linear_terms))
        self.assertDictEqualWithZeroDefault(
            {xx: 6 * 3 / 3 + 2, xy: 6 / 3, yy: 1 - 2}, dict(e.quadratic_terms)
        )

    def test_linear_sum_ast(self) -> None:
        mod = model.Model()
        x = [mod.add_binary_variable(name=f"x[{i}]") for i in range(5)]
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")
        s = (
            (2 * (z + y) + 2 * y) / 2
            + sum(
                x[i]
                + 0.5 * variables.LinearSum([4 * x[1] - 2 * x[0], 2 * x[2], 2.5, -0.5])
                for i in range(3)
            )
            - variables.LinearSum([x[3], x[4]])
        )
        e = variables.as_flat_linear_expression(s)
        self.assertEqual(3.0, e.offset)
        self.assertDictEqualWithZeroDefault(
            {x[0]: -2, x[1]: 7, x[2]: 4, x[3]: -1, x[4]: -1, y: 2, z: 1},
            dict(e.terms),
        )

    def test_quadratic_sum_ast(self) -> None:
        mod = model.Model()
        x = [mod.add_binary_variable(name=f"x[{i}]") for i in range(3)]
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")
        yy = variables.QuadraticTermKey(y, y)
        zz = variables.QuadraticTermKey(z, z)
        s = (
            1
            + y * y
            + z
            + sum(x[i] + x[i] * variables.LinearSum([y, z]) for i in range(3))
            - variables.QuadraticSum([y, z * z])
        )
        e = variables.as_flat_quadratic_expression(s)
        self.assertEqual(1.0, e.offset)
        self.assertDictEqualWithZeroDefault(
            {x[0]: 1, x[1]: 1, x[2]: 1, y: -1, z: 1}, dict(e.linear_terms)
        )
        self.assertDictEqualWithZeroDefault(
            {
                yy: 1,
                zz: -1,
                variables.QuadraticTermKey(x[0], y): 1,
                variables.QuadraticTermKey(x[1], y): 1,
                variables.QuadraticTermKey(x[2], y): 1,
                variables.QuadraticTermKey(x[0], z): 1,
                variables.QuadraticTermKey(x[1], z): 1,
                variables.QuadraticTermKey(x[2], z): 1,
            },
            dict(e.quadratic_terms),
        )


# Test behavior of LinearExpression and as_flat_linear_expression that is
# not covered by other tests.
class LinearExpressionTest(absltest.TestCase):

    def test_init_to_zero(self) -> None:
        expression = variables.LinearExpression()
        self.assertEqual(expression.offset, 0.0)
        self.assertEmpty(expression.terms)

    def test_terms_read_only(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        expression = variables.LinearExpression(y - 2 * x + 1)
        with self.assertRaisesRegex(TypeError, "does not support item assignment"):
            expression.terms[x] += 1  # pytype: disable=unsupported-operands

    def test_no_copy_of_linear_expression(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        expression = variables.LinearExpression(y - 2 * x + 1)
        self.assertIs(expression, variables.as_flat_linear_expression(expression))

    def test_number_as_flat_linear_expression(self) -> None:
        expression = variables.LinearExpression(2.0)
        self.assertDictEqual(dict(expression.terms), {})
        self.assertEqual(expression.offset, 2.0)

    def test_evaluate(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        expression = variables.LinearExpression(3 * x + y + 2.0)
        self.assertEqual(expression.evaluate({x: 4.0, y: 3.0}), 17.0)


# Test behavior of QuadraticExpression and as_flat_quadratic_expression that is
# not covered by other tests.
class QuadraticExpressionTest(absltest.TestCase):

    def test_terms_read_only(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        yy = variables.QuadraticTermKey(y, y)
        expression = variables.QuadraticExpression(y * y - 2 * x + 1)
        with self.assertRaisesRegex(TypeError, "does not support item assignment"):
            expression.linear_terms[x] += 1  # pytype: disable=unsupported-operands
        with self.assertRaisesRegex(TypeError, "does not support item assignment"):
            expression.quadratic_terms[yy] += 1  # pytype: disable=unsupported-operands

    def test_no_copy_of_quadratic_expression(self) -> None:
        mod = model.Model()
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        expression = variables.QuadraticExpression(y * y - 2 * x + 1)
        self.assertIs(expression, variables.as_flat_quadratic_expression(expression))

    def test_number_as_flat_quadratic_expression(self) -> None:
        expression = variables.QuadraticExpression(2.0)
        self.assertDictEqual(dict(expression.linear_terms), {})
        self.assertDictEqual(dict(expression.quadratic_terms), {})
        self.assertEqual(expression.offset, 2.0)

    def test_evaluate(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        expression = variables.QuadraticExpression(x * x + 2 * x * y + 4 * y + 2.0)
        # 16 + 24 + 12 + 2 = 54
        self.assertEqual(expression.evaluate({x: 4.0, y: 3.0}), 54.0)


if __name__ == "__main__":
    absltest.main()
