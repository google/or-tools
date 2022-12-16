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
from ortools.math_opt.python import bounded_expressions

_BAD_BOOL_ERROR = "two-sided or ranged"


class BoundedExpressionTest(absltest.TestCase):

    def test_bounded_expression_read(self) -> None:
        b = bounded_expressions.BoundedExpression(
            lower_bound=-3.0, expression="e123", upper_bound=4.5
        )
        self.assertEqual(b.lower_bound, -3.0)
        self.assertEqual(b.upper_bound, 4.5)
        self.assertEqual(b.expression, "e123")
        self.assertEqual(str(b), "-3.0 <= e123 <= 4.5")
        self.assertEqual(repr(b), "-3.0 <= 'e123' <= 4.5")
        with self.assertRaisesRegex(TypeError, _BAD_BOOL_ERROR):
            bool(b)

    def test_lower_bounded_expression_read(self) -> None:
        b = bounded_expressions.LowerBoundedExpression(
            lower_bound=-3.0, expression="e123"
        )
        self.assertEqual(b.lower_bound, -3.0)
        self.assertEqual(b.upper_bound, math.inf)
        self.assertEqual(b.expression, "e123")
        self.assertEqual(str(b), "e123 >= -3.0")
        self.assertEqual(repr(b), "'e123' >= -3.0")
        with self.assertRaisesRegex(TypeError, _BAD_BOOL_ERROR):
            bool(b)

    def test_upper_bounded_expression_read(self) -> None:
        b = bounded_expressions.UpperBoundedExpression(
            expression="e123", upper_bound=4.5
        )
        self.assertEqual(b.lower_bound, -math.inf)
        self.assertEqual(b.upper_bound, 4.5)
        self.assertEqual(b.expression, "e123")
        self.assertEqual(str(b), "e123 <= 4.5")
        self.assertEqual(repr(b), "'e123' <= 4.5")
        with self.assertRaisesRegex(TypeError, _BAD_BOOL_ERROR):
            bool(b)

    def test_lower_bounded_to_bounded(self) -> None:
        lb = bounded_expressions.LowerBoundedExpression(
            lower_bound=-3.0, expression="e123"
        )
        bounded = lb <= 4.5
        self.assertIsInstance(bounded, bounded_expressions.BoundedExpression)
        self.assertEqual(bounded.lower_bound, -3.0)
        self.assertEqual(bounded.upper_bound, 4.5)
        self.assertEqual(bounded.expression, "e123")

    def test_upper_bounded_to_bounded(self) -> None:
        ub = bounded_expressions.UpperBoundedExpression(
            expression="e123", upper_bound=4.5
        )
        bounded = -3.0 <= ub
        self.assertIsInstance(bounded, bounded_expressions.BoundedExpression)
        self.assertEqual(bounded.lower_bound, -3.0)
        self.assertEqual(bounded.upper_bound, 4.5)
        self.assertEqual(bounded.expression, "e123")


if __name__ == "__main__":
    absltest.main()
