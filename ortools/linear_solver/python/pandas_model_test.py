#!/usr/bin/env python3
# Copyright 2010-2022 Google LLC
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
from typing import Callable, Union

from absl.testing import absltest
from absl.testing import parameterized
import pandas as pd

from google.protobuf import text_format
from ortools.linear_solver import linear_solver_pb2
from ortools.linear_solver.python import pandas_model as pdm


# This test suite should not be depended on as a public API.
class InternalHelperTest(absltest.TestCase):
    def test_anonymous_variables(self):
        helper = pdm.OptimizationModel(name="test_name")._helper
        index = helper.add_var()
        variable = pdm._Variable(helper, index)
        self.assertEqual(variable._name, f"variable#{index}")

    def test_anonymous_constraints(self):
        helper = pdm.OptimizationModel(name="test_name")._helper
        index = helper.add_linear_constraint()
        constraint = pdm._LinearConstraint(helper, index)
        self.assertEqual(constraint._name, f"linear_constraint#{index}")


class LinearBaseTest(parameterized.TestCase):
    def setUp(self):
        super().setUp()
        simple_model = pdm.OptimizationModel(name="test_name")
        simple_model.create_variables(name="x", index=pd.Index(range(3), name="i"))
        simple_model.create_variables(name="y", index=pd.Index(range(5), name="i"))
        self.simple_model = simple_model

    @parameterized.named_parameters(
        # Variable / Indexing
        dict(
            testcase_name="x[0]",
            expr=lambda x, y: x[0],
            expected_repr="x[0]",
        ),
        dict(
            testcase_name="x[1]",
            expr=lambda x, y: x[1],
            expected_repr="x[1]",
        ),
        dict(
            testcase_name="x[2]",
            expr=lambda x, y: x[2],
            expected_repr="x[2]",
        ),
        dict(
            testcase_name="y[0]",
            expr=lambda x, y: y[0],
            expected_repr="y[0]",
        ),
        dict(
            testcase_name="y[4]",
            expr=lambda x, y: y[4],
            expected_repr="y[4]",
        ),
        # Sum
        dict(
            testcase_name="x[0] + 5",
            expr=lambda x, y: x[0] + 5,
            expected_repr="5.0 + x[0]",
        ),
        dict(
            testcase_name="x[0] - 5",
            expr=lambda x, y: x[0] - 5,
            expected_repr="-5.0 + x[0]",
        ),
        dict(
            testcase_name="5 - x[0]",
            expr=lambda x, y: 5 - x[0],
            expected_repr="5.0 - x[0]",
        ),
        dict(
            testcase_name="5 + x[0]",
            expr=lambda x, y: 5 + x[0],
            expected_repr="5.0 + x[0]",
        ),
        dict(
            testcase_name="x[0] + y[0]",
            expr=lambda x, y: x[0] + y[0],
            expected_repr="0.0 + x[0] + y[0]",
        ),
        dict(
            testcase_name="x[0] + y[0] + 5",
            expr=lambda x, y: x[0] + y[0] + 5,
            expected_repr="5.0 + x[0] + y[0]",
        ),
        dict(
            testcase_name="5 + x[0] + y[0]",
            expr=lambda x, y: 5 + x[0] + y[0],
            expected_repr="5.0 + x[0] + y[0]",
        ),
        dict(
            testcase_name="5 + x[0] - x[0]",
            expr=lambda x, y: 5 + x[0] - x[0],
            expected_repr="5.0",
        ),
        dict(
            testcase_name="5 + x[0] - y[0]",
            expr=lambda x, y: 5 + x[0] - y[0],
            expected_repr="5.0 + x[0] - y[0]",
        ),
        dict(
            testcase_name="x.sum()",
            expr=lambda x, y: x.sum(),
            expected_repr="0.0 + x[0] + x[1] + x[2]",
        ),
        dict(
            testcase_name="x.add(y, fill_value=0).sum() + 5",
            expr=lambda x, y: x.add(y, fill_value=0).sum() + 5,
            expected_repr="5.0 + x[0] + x[1] + x[2] + y[0] + y[1] + ...",
        ),
        # Product
        dict(
            testcase_name="- x.sum()",
            expr=lambda x, y: -x.sum(),
            expected_repr="0.0 - x[0] - x[1] - x[2]",
        ),
        dict(
            testcase_name="5 - x.sum()",
            expr=lambda x, y: 5 - x.sum(),
            expected_repr="5.0 - x[0] - x[1] - x[2]",
        ),
        dict(
            testcase_name="x.sum() / 2.0",
            expr=lambda x, y: x.sum() / 2.0,
            expected_repr="0.0 + 0.5 * x[0] + 0.5 * x[1] + 0.5 * x[2]",
        ),
        dict(
            testcase_name="(3 * x).sum()",
            expr=lambda x, y: (3 * x).sum(),
            expected_repr="0.0 + 3.0 * x[0] + 3.0 * x[1] + 3.0 * x[2]",
        ),
        dict(
            testcase_name="(x * 3).sum()",
            expr=lambda x, y: (x * 3).sum(),
            expected_repr="0.0 + 3.0 * x[0] + 3.0 * x[1] + 3.0 * x[2]",
        ),
        dict(
            testcase_name="x.sum() * 3",
            expr=lambda x, y: x.sum() * 3,
            expected_repr="0.0 + 3.0 * x[0] + 3.0 * x[1] + 3.0 * x[2]",
        ),
        dict(
            testcase_name="3 * x.sum()",
            expr=lambda x, y: 3 * x.sum(),
            expected_repr="0.0 + 3.0 * x[0] + 3.0 * x[1] + 3.0 * x[2]",
        ),
        dict(
            testcase_name="0 * x.sum() + y.sum()",
            expr=lambda x, y: 0 * x.sum() + y.sum(),
            expected_repr="0.0 + y[0] + y[1] + y[2] + y[3] + y[4]",
        ),
        # LinearExpression
        dict(
            testcase_name="_as_flat_linear_expression(x.sum())",
            expr=lambda x, y: pdm._as_flat_linear_expression(x.sum()),
            expected_repr="0.0 + x[0] + x[1] + x[2]",
        ),
        dict(
            testcase_name=(
                "_as_flat_linear_expression(_as_flat_linear_expression(x.sum()))"
            ),
            # pylint: disable=g-long-lambda
            expr=lambda x, y: pdm._as_flat_linear_expression(
                pdm._as_flat_linear_expression(x.sum())
            ),
            expected_repr="0.0 + x[0] + x[1] + x[2]",
        ),
        dict(
            testcase_name="""_as_flat_linear_expression(sum([
            _as_flat_linear_expression(x.sum()),
            _as_flat_linear_expression(x.sum()),
          ]))""",
            # pylint: disable=g-long-lambda
            expr=lambda x, y: pdm._as_flat_linear_expression(
                sum(
                    [
                        pdm._as_flat_linear_expression(x.sum()),
                        pdm._as_flat_linear_expression(x.sum()),
                    ]
                )
            ),
            expected_repr="0.0 + 2.0 * x[0] + 2.0 * x[1] + 2.0 * x[2]",
        ),
    )
    def test_repr(self, expr, expected_repr):
        x = self.simple_model.get_variable_references("x")
        y = self.simple_model.get_variable_references("y")
        self.assertEqual(repr(expr(x, y)), expected_repr)


class LinearBaseErrorsTest(absltest.TestCase):
    def test_unknown_linear_type(self):
        with self.assertRaisesRegex(TypeError, r"Unrecognized linear expression"):

            class UnknownLinearType(pdm._LinearBase):
                pass

            pdm._as_flat_linear_expression(UnknownLinearType())

    def test_division_by_zero(self):
        with self.assertRaises(ZeroDivisionError):
            model = pdm.OptimizationModel(name="divide_by_zero")
            x = model.create_variables(name="x", index=pd.Index(range(1)))
            print(x / 0)

    def test_boolean_expression(self):
        with self.assertRaisesRegex(
            NotImplementedError, r"LinearExpression as a Boolean value"
        ):
            model = pdm.OptimizationModel(name="boolean_expression")
            x = model.create_variables(name="x", index=pd.Index(range(1)))
            bool(x.sum())


class BoundedLinearBaseTest(parameterized.TestCase):
    def setUp(self):
        super().setUp()
        simple_model = pdm.OptimizationModel(name="test_name")
        simple_model.create_variables(name="x", index=pd.Index(range(3), name="i"))
        simple_model.create_variables(name="y", index=pd.Index(range(5), name="i"))
        self.simple_model = simple_model

    @parameterized.product(
        lhs=(
            lambda x, y: x.sum(),
            lambda x, y: -x.sum(),
            lambda x, y: x.sum() * 0,
            lambda x, y: x.sum() * 3,
            lambda x, y: x[0],
            lambda x, y: x[1],
            lambda x, y: x[2],
            lambda x, y: -math.inf,
            lambda x, y: -1,
            lambda x, y: 0,
            lambda x, y: 1,
            lambda x, y: 1.1,
            lambda x, y: math.inf,
        ),
        rhs=(
            lambda x, y: y.sum(),
            lambda x, y: -y.sum(),
            lambda x, y: y.sum() * 0,
            lambda x, y: y.sum() * 3,
            lambda x, y: y[0],
            lambda x, y: y[1],
            lambda x, y: y[2],
            lambda x, y: -math.inf,
            lambda x, y: -1,
            lambda x, y: 0,
            lambda x, y: 1,
            lambda x, y: 1.1,
            lambda x, y: math.inf,
        ),
        op=(
            lambda lhs, rhs: lhs == rhs,
            lambda lhs, rhs: lhs <= rhs,
            lambda lhs, rhs: lhs >= rhs,
        ),
    )
    def test_repr(self, lhs, rhs, op):
        x = self.simple_model.get_variable_references("x")
        y = self.simple_model.get_variable_references("y")
        l: pdm._LinearType = lhs(x, y)
        r: pdm._LinearType = rhs(x, y)
        result = op(l, r)
        if isinstance(l, pdm._LinearBase) or isinstance(r, pdm._LinearBase):
            self.assertIsInstance(result, pdm._BoundedLinearBase)
            self.assertIn("=", repr(result), msg="is one of ==, <=, or >=")
        else:
            self.assertIsInstance(result, bool)

    def test_doublesided_bounded_expressions(self):
        x = self.simple_model.get_variable_references("x")
        self.assertEqual(
            "0 <= x[0] <= 1", repr(pdm._BoundedLinearExpression(x[0], 0, 1))
        )

    def test_free_bounded_expressions(self):
        x = self.simple_model.get_variable_references("x")
        self.assertEqual(
            "x[0] free",
            repr(pdm._BoundedLinearExpression(x[0], -math.inf, math.inf)),
        )

    def test_var_eq_var_as_bool(self):
        x = self.simple_model.get_variable_references("x")
        y = self.simple_model.get_variable_references("y")
        self.assertEqual(x[0], x[0])
        self.assertNotEqual(x[0], x[1])
        self.assertNotEqual(x[0], y[0])

        self.assertEqual(x[1], x[1])
        self.assertNotEqual(x[1], x[0])
        self.assertNotEqual(x[1], y[1])

        self.assertEqual(y[0], y[0])
        self.assertNotEqual(y[0], y[1])
        self.assertNotEqual(y[0], x[0])

        self.assertEqual(y[1], y[1])
        self.assertNotEqual(y[1], y[0])
        self.assertNotEqual(y[1], x[1])


class BoundedLinearBaseErrorsTest(absltest.TestCase):
    def test_bounded_linear_expression_as_bool(self):
        with self.assertRaisesRegex(NotImplementedError, "Boolean value"):
            model = pdm.OptimizationModel(name="bounded_linear_expression_as_bool")
            x = model.create_variables(name="x", index=pd.Index(range(1)))
            bool(pdm._BoundedLinearExpression(x, 0, 1))


class OptimizationModelMetadataTest(absltest.TestCase):
    def test_name(self):
        model = pdm.OptimizationModel(name="test_name")
        self.assertEqual("test_name", model.get_name())

    def test_schema_empty(self):
        model = pdm.OptimizationModel(name="test_name")
        self.assertIsInstance(model.get_schema(), pd.DataFrame)

    def test_schema_no_constraints(self):
        model = pdm.OptimizationModel(name="test_name")
        x = model.create_variables(
            name="x",
            index=pd.Index(range(3)),
            lower_bound=0,
            upper_bound=1,
            is_integer=True,
        )
        y = model.create_variables(
            name="y",
            index=pd.MultiIndex.from_tuples([(1, 2), (3, 4)], names=["i", "j"]),
            lower_bound=0,
        )
        z = model.create_variables(
            name="z",
            index=pd.MultiIndex.from_product(((1, 2), ("a", "b", "c"))),
            lower_bound=-5,
            upper_bound=10,
            is_integer=True,
        )
        schema = model.get_schema()
        self.assertIsInstance(schema, pd.DataFrame)
        self.assertLen(schema, 3)
        self.assertSequenceAlmostEqual(
            schema.columns, ["type", "name", "dimensions", "count"]
        )
        self.assertSequenceAlmostEqual(schema.type, ["variable"] * 3)
        self.assertSequenceAlmostEqual(schema.name, ["x", "y", "z"])
        self.assertSequenceAlmostEqual(
            schema.dimensions, [(None,), ("i", "j"), (None, None)]
        )
        self.assertSequenceAlmostEqual(schema["count"], [len(x), len(y), len(z)])
        self.assertEqual(
            repr(model),
            """OptimizationModel(name=test_name) with the following schema:
       type name    dimensions  count
0  variable    x        [None]      3
1  variable    y    ['i', 'j']      2
2  variable    z  [None, None]      6""",
        )

    def test_full_schema(self):
        model = pdm.OptimizationModel(name="test_name")
        x = model.create_variables(
            name="x",
            index=pd.Index(range(3)),
            lower_bound=0,
            upper_bound=1,
            is_integer=True,
        )
        y = model.create_variables(
            name="y",
            index=pd.MultiIndex.from_tuples([(1, 2), (3, 4)], names=["i", "j"]),
            lower_bound=0,
        )
        z = model.create_variables(
            name="z",
            index=pd.MultiIndex.from_product(
                ((1, 2), ("a", "b", "c")), names=["i", "k"]
            ),
            lower_bound=-5,
            upper_bound=10,
            is_integer=True,
        )
        c1 = model.create_linear_constraints(
            name="x_sum_le_constant",
            bounded_exprs=(x.sum() <= 10),
        )
        c2 = model.create_linear_constraints(
            name="y_groupbyj_sum_ge_constant",
            bounded_exprs=y.groupby("j").sum().apply(lambda expr: expr >= 3),
        )
        c3 = model.create_linear_constraints(
            name="y_groupbyi_sum_eq_z_groupbyi_sum",
            bounded_exprs=y.groupby("i")
            .sum()
            .sub(z.groupby("i").sum())
            .dropna()
            .apply(lambda expr: expr == 0),
        )
        schema = model.get_schema()
        self.assertIsInstance(schema, pd.DataFrame)
        self.assertLen(schema, 6)
        self.assertSequenceAlmostEqual(
            schema.columns, ["type", "name", "dimensions", "count"]
        )
        self.assertSequenceAlmostEqual(
            schema.type, ["variable"] * 3 + ["linear_constraint"] * 3
        )
        self.assertSequenceAlmostEqual(
            schema.name,
            [
                "x",
                "y",
                "z",
                "x_sum_le_constant",
                "y_groupbyj_sum_ge_constant",
                "y_groupbyi_sum_eq_z_groupbyi_sum",
            ],
        )
        self.assertSequenceAlmostEqual(
            schema.dimensions,
            [(None,), ("i", "j"), ("i", "k"), (None,), ("j",), ("i",)],
        )
        self.assertSequenceAlmostEqual(
            schema["count"], [len(x), len(y), len(z), len(c1), len(c2), len(c3)]
        )
        self.assertEqual(
            repr(model),
            """OptimizationModel(name=test_name) with the following schema:
                type                              name  dimensions  count
0           variable                                 x      [None]      3
1           variable                                 y  ['i', 'j']      2
2           variable                                 z  ['i', 'k']      6
3  linear_constraint                 x_sum_le_constant      [None]      1
4  linear_constraint        y_groupbyj_sum_ge_constant       ['j']      2
5  linear_constraint  y_groupbyi_sum_eq_z_groupbyi_sum       ['i']      3""",
        )


class OptimizationModelErrorsTest(absltest.TestCase):
    def test_name_errors(self):
        with self.assertRaisesRegex(ValueError, r"not a valid identifier"):
            pdm.OptimizationModel(name="")

    def test_create_variables_errors(self):
        with self.assertRaisesRegex(TypeError, r"Non-index object"):
            model = pdm.OptimizationModel(name="test_name")
            model.create_variables(name="", index=pd.DataFrame())
        with self.assertRaisesRegex(TypeError, r"invalid type"):
            model = pdm.OptimizationModel(name="test_name")
            model.create_variables(name="x", index=pd.Index([0]), lower_bound="0")
        with self.assertRaisesRegex(TypeError, r"invalid type"):
            model = pdm.OptimizationModel(name="test_name")
            model.create_variables(name="x", index=pd.Index([0]), upper_bound="0")
        with self.assertRaisesRegex(TypeError, r"invalid type"):
            model = pdm.OptimizationModel(name="test_name")
            model.create_variables(name="x", index=pd.Index([0]), is_integer="True")
        with self.assertRaisesRegex(ValueError, r"not a valid identifier"):
            model = pdm.OptimizationModel(name="test_name")
            model.create_variables(name="", index=pd.Index([0]))
        with self.assertRaisesRegex(ValueError, r"already exists"):
            model = pdm.OptimizationModel(name="test_name")
            model.create_variables(name="x", index=pd.Index([0]))
            model.create_variables(name="x", index=pd.Index([0]))
        with self.assertRaisesRegex(ValueError, r"is greater than"):
            model = pdm.OptimizationModel(name="test_name")
            model.create_variables(
                name="x",
                index=pd.Index([0]),
                lower_bound=0.2,
                upper_bound=0.1,
            )
        with self.assertRaisesRegex(ValueError, r"is greater than"):
            model = pdm.OptimizationModel(name="test_name")
            model.create_variables(
                name="x",
                index=pd.Index([0]),
                lower_bound=0.1,
                upper_bound=0.2,
                is_integer=True,
            )
        with self.assertRaisesRegex(ValueError, r"index does not match"):
            model = pdm.OptimizationModel(name="test_name")
            model.create_variables(
                name="x", index=pd.Index([0]), lower_bound=pd.Series([1, 2])
            )
        with self.assertRaisesRegex(ValueError, r"index does not match"):
            model = pdm.OptimizationModel(name="test_name")
            model.create_variables(
                name="x", index=pd.Index([0]), upper_bound=pd.Series([1, 2])
            )
        with self.assertRaisesRegex(ValueError, r"index does not match"):
            model = pdm.OptimizationModel(name="test_name")
            model.create_variables(
                name="x", index=pd.Index([0]), is_integer=pd.Series([False, True])
            )

    def test_get_variables_errors(self):
        model = pdm.OptimizationModel(name="test_name")
        model.create_variables(name="x", index=pd.Index(range(3)))
        with self.assertRaisesRegex(KeyError, r"no variable set named"):
            model.get_variables(name="nonexistent_variable")

    def test_get_variable_references_errors(self):
        model = pdm.OptimizationModel(name="test_name")
        model.create_variables(name="x", index=pd.Index(range(3)))
        with self.assertRaisesRegex(KeyError, r"no variable set named"):
            model.get_variable_references(None)
        with self.assertRaisesRegex(KeyError, r"no variable set named"):
            model.get_variable_references(name="")

    def test_create_linear_constraints_errors(self):
        with self.assertRaisesRegex(ValueError, r"not a valid identifier"):
            model = pdm.OptimizationModel(name="test_name")
            x = model.create_variables(name="x", index=pd.Index(range(1)))
            model.create_linear_constraints(name="", bounded_exprs=x[0] == 0)
        with self.assertRaisesRegex(ValueError, r"already exists"):
            model = pdm.OptimizationModel(name="test_name")
            x = model.create_variables(name="x", index=pd.Index(range(1)))
            model.create_linear_constraints(name="c", bounded_exprs=x[0] <= 0)
            model.create_linear_constraints(name="c", bounded_exprs=x[0] >= 0)
        with self.assertRaisesRegex(TypeError, r"invalid type"):
            model = pdm.OptimizationModel(name="test_name")
            model.create_linear_constraints(name="c", bounded_exprs="True")
        with self.assertRaisesRegex(TypeError, r"invalid type"):
            model = pdm.OptimizationModel(name="test_name")
            model.create_linear_constraints(name="c", bounded_exprs=pd.Series(["T"]))

    def test_get_linear_constraint_references_errors(self):
        with self.assertRaises(KeyError):
            model = pdm.OptimizationModel(name="test_name")
            model.get_linear_constraint_references("c")


class OptimizationModelVariablesTest(parameterized.TestCase):
    _variable_indices = (
        pd.Index(range(3)),
        pd.Index(range(5), name="i"),
        pd.MultiIndex.from_product(((1, 2), ("a", "b", "c")), names=["i", "j"]),
        pd.MultiIndex.from_product((("a", "b"), (1, 2, 3))),
    )
    _bounds = (
        lambda index: (-math.inf, -10.5),
        lambda index: (-math.inf, -1),
        lambda index: (-math.inf, 0),
        lambda index: (-math.inf, 10),
        lambda index: (-math.inf, math.inf),
        lambda index: (-10, -1.1),
        lambda index: (-10, 0),
        lambda index: (-10, -10),
        lambda index: (-10, 3),
        lambda index: (-9, math.inf),
        lambda index: (-1, 1),
        lambda index: (0, 0),
        lambda index: (0, 1),
        lambda index: (0, math.inf),
        lambda index: (1, 1),
        lambda index: (1, 10.1),
        lambda index: (1, math.inf),
        lambda index: (100.1, math.inf),
        # pylint: disable=g-long-lambda
        lambda index: (
            pd.Series(-math.inf, index=index),
            pd.Series(-10.5, index=index),
        ),
        lambda index: (
            pd.Series(-math.inf, index=index),
            pd.Series(-1, index=index),
        ),
        lambda index: (
            pd.Series(-math.inf, index=index),
            pd.Series(0, index=index),
        ),
        lambda index: (
            pd.Series(-math.inf, index=index),
            pd.Series(10, index=index),
        ),
        lambda index: (
            pd.Series(-math.inf, index=index),
            pd.Series(math.inf, index=index),
        ),
        lambda index: (pd.Series(-10, index=index), pd.Series(-1.1, index=index)),
        lambda index: (pd.Series(-10, index=index), pd.Series(0, index=index)),
        lambda index: (pd.Series(-10, index=index), pd.Series(-10, index=index)),
        lambda index: (pd.Series(-10, index=index), pd.Series(3, index=index)),
        lambda index: (
            pd.Series(-9, index=index),
            pd.Series(math.inf, index=index),
        ),
        lambda index: (pd.Series(-1, index=index), pd.Series(1, index=index)),
        lambda index: (pd.Series(0, index=index), pd.Series(0, index=index)),
        lambda index: (pd.Series(0, index=index), pd.Series(1, index=index)),
        lambda index: (
            pd.Series(0, index=index),
            pd.Series(math.inf, index=index),
        ),
        lambda index: (pd.Series(1, index=index), pd.Series(1, index=index)),
        lambda index: (pd.Series(1, index=index), pd.Series(10.1, index=index)),
        lambda index: (
            pd.Series(1, index=index),
            pd.Series(math.inf, index=index),
        ),
        lambda index: (
            pd.Series(100.1, index=index),
            pd.Series(math.inf, index=index),
        ),
    )
    _is_integer = (
        lambda index: False,
        lambda index: True,
        lambda index: pd.Series(False, index=index),
        lambda index: pd.Series(True, index=index),
    )

    @parameterized.product(
        index=_variable_indices, bounds=_bounds, is_integer=_is_integer
    )
    def test_create_variables(self, index, bounds, is_integer):
        model = pdm.OptimizationModel(name="test_name")
        variables = model.create_variables(
            name="test_variable",
            index=index,
            lower_bound=bounds(index)[0],
            upper_bound=bounds(index)[1],
            is_integer=is_integer(index),
        )
        self.assertLen(variables, len(index))
        self.assertLen(set(variables), len(index))
        for i in index:
            self.assertEqual(repr(variables[i]), f"test_variable[{i}]")

    @parameterized.named_parameters(
        dict(testcase_name="all", variable_name=None, variable_count=3 + 5),
        dict(testcase_name="x", variable_name="x", variable_count=3),
        dict(testcase_name="y", variable_name="y", variable_count=5),
    )
    def test_get_variables(self, variable_name, variable_count):
        model = pdm.OptimizationModel(name="test_name")
        model.create_variables(name="x", index=pd.Index(range(3)))
        model.create_variables(name="y", index=pd.Index(range(5)))
        for variables, expected_count in (
            (model.get_variables(), 3 + 5),
            (model.get_variables(variable_name), variable_count),
            (model.get_variables(name=variable_name), variable_count),
        ):
            self.assertIsInstance(variables, pd.Index)
            self.assertLen(variables, expected_count)

    @parameterized.product(
        index=_variable_indices, bounds=_bounds, is_integer=_is_integer
    )
    def test_get_variable_lower_bounds(self, index, bounds, is_integer):
        lower_bound, upper_bound = bounds(index)
        model = pdm.OptimizationModel(name="test_name")
        model.create_variables(
            name="x",
            index=index,
            lower_bound=lower_bound,
            upper_bound=upper_bound,
            is_integer=is_integer(index),
        )
        model.create_variables(
            name="y",
            index=index,
            lower_bound=lower_bound,
            upper_bound=upper_bound,
            is_integer=is_integer(index),
        )
        for lower_bounds in (
            model.get_variable_lower_bounds(model.get_variables("x")),
            model.get_variable_lower_bounds(model.get_variables("y")),
            model.get_variable_lower_bounds(model.get_variable_references("x")),
            model.get_variable_lower_bounds(model.get_variable_references("y")),
        ):
            self.assertSequenceAlmostEqual(
                lower_bounds,
                pdm._convert_to_series_and_validate_index(lower_bound, index),
            )
        self.assertSequenceAlmostEqual(
            model.get_variable_lower_bounds(),
            pd.concat(
                [
                    model.get_variable_lower_bounds(model.get_variables("x")),
                    model.get_variable_lower_bounds(model.get_variables("y")),
                ]
            ),
        )
        for variables in (model.get_variables("x"), model.get_variables("y")):
            lower_bounds = model.get_variable_lower_bounds(variables)
            self.assertSequenceAlmostEqual(lower_bounds.index, variables)
        for variables in (
            model.get_variable_references("x"),
            model.get_variable_references("y"),
        ):
            lower_bounds = model.get_variable_lower_bounds(variables)
            self.assertSequenceAlmostEqual(lower_bounds.index, variables.index)

    @parameterized.named_parameters(
        dict(testcase_name="x", variable_name="x", variable_count=3),
        dict(testcase_name="y", variable_name="y", variable_count=5),
    )
    def test_get_variable_references(self, variable_name, variable_count):
        model = pdm.OptimizationModel(name="test_name")
        model.create_variables(name="x", index=pd.Index(range(3)))
        model.create_variables(name="y", index=pd.Index(range(5)))
        self.assertLen(model.get_variables(), 3 + 5)
        for variables in (
            model.get_variable_references(variable_name),
            model.get_variable_references(name=variable_name),
        ):
            self.assertLen(variables, variable_count)

    @parameterized.product(
        index=_variable_indices, bounds=_bounds, is_integer=_is_integer
    )
    def test_get_variable_upper_bounds(self, index, bounds, is_integer):
        lower_bound, upper_bound = bounds(index)
        model = pdm.OptimizationModel(name="test_name")
        model.create_variables(
            name="x",
            index=index,
            lower_bound=lower_bound,
            upper_bound=upper_bound,
            is_integer=is_integer(index),
        )
        model.create_variables(
            name="y",
            index=index,
            lower_bound=lower_bound,
            upper_bound=upper_bound,
            is_integer=is_integer(index),
        )
        for upper_bounds in (
            model.get_variable_upper_bounds(model.get_variables("x")),
            model.get_variable_upper_bounds(model.get_variables("y")),
            model.get_variable_upper_bounds(model.get_variable_references("x")),
            model.get_variable_upper_bounds(model.get_variable_references("y")),
        ):
            self.assertSequenceAlmostEqual(
                upper_bounds,
                pdm._convert_to_series_and_validate_index(upper_bound, index),
            )
        self.assertSequenceAlmostEqual(
            model.get_variable_upper_bounds(),
            pd.concat(
                [
                    model.get_variable_upper_bounds(model.get_variables("x")),
                    model.get_variable_upper_bounds(model.get_variables("y")),
                ]
            ),
        )
        for variables in (model.get_variables("x"), model.get_variables("y")):
            upper_bounds = model.get_variable_upper_bounds(variables)
            self.assertSequenceAlmostEqual(upper_bounds.index, variables)
        for variables in (
            model.get_variable_references("x"),
            model.get_variable_references("y"),
        ):
            upper_bounds = model.get_variable_upper_bounds(variables)
            self.assertSequenceAlmostEqual(upper_bounds.index, variables.index)


class OptimizationModelLinearConstraintsTest(parameterized.TestCase):
    constraint_test_cases = [
        # pylint: disable=g-long-lambda
        dict(
            testcase_name="True",
            name="true",
            bounded_exprs=lambda x, y: True,
            constraint_count=1,
            lower_bounds=[0],
            upper_bounds=[0],
            expression_terms=lambda x, y: [{}],
            expression_offsets=[0],
        ),
        dict(
            testcase_name="False",
            name="false",
            bounded_exprs=lambda x, y: False,
            constraint_count=1,
            lower_bounds=[1],
            upper_bounds=[1],
            expression_terms=lambda x, y: [{}],
            expression_offsets=[0],
        ),
        dict(
            testcase_name="x[0] <= 1.5",
            name="x0_le_c",
            bounded_exprs=lambda x, y: x[0] <= 1.5,
            constraint_count=1,
            lower_bounds=[-math.inf],
            upper_bounds=[1.5],
            expression_terms=lambda x, y: [{x[0]: 1}],
            expression_offsets=[0],
        ),
        dict(
            testcase_name="x[0] == 1",
            name="x0_eq_c",
            bounded_exprs=lambda x, y: x[0] == 1,
            constraint_count=1,
            lower_bounds=[1],
            upper_bounds=[1],
            expression_terms=lambda x, y: [{x[0]: 1}],
            expression_offsets=[0],
        ),
        dict(
            testcase_name="x[0] >= -1",
            name="x0_ge_c",
            bounded_exprs=lambda x, y: x[0] >= -1,
            constraint_count=1,
            lower_bounds=[-1],
            upper_bounds=[math.inf],
            expression_terms=lambda x, y: [{x[0]: 1}],
            expression_offsets=[0],
        ),
        dict(
            testcase_name="-1.5 <= x[0]",
            name="c_le_x0",
            bounded_exprs=lambda x, y: -1.5 <= x[0],
            constraint_count=1,
            lower_bounds=[-1.5],
            upper_bounds=[math.inf],
            expression_terms=lambda x, y: [{x[0]: 1}],
            expression_offsets=[0],
        ),
        dict(
            testcase_name="0 == x[0]",
            name="c_eq_x0",
            bounded_exprs=lambda x, y: 0 == x[0],
            constraint_count=1,
            lower_bounds=[0],
            upper_bounds=[0],
            expression_terms=lambda x, y: [{x[0]: 1}],
            expression_offsets=[0],
        ),
        dict(
            testcase_name="10 >= x[0]",
            name="c_ge_x0",
            bounded_exprs=lambda x, y: 10 >= x[0],
            constraint_count=1,
            lower_bounds=[-math.inf],
            upper_bounds=[10],
            expression_terms=lambda x, y: [{x[0]: 1}],
            expression_offsets=[0],
        ),
        dict(
            testcase_name="x[0] <= x[0]",
            name="x0_le_x0",
            bounded_exprs=lambda x, y: x[0] <= x[0],
            constraint_count=1,
            lower_bounds=[-math.inf],
            upper_bounds=[0],
            expression_terms=lambda x, y: [{x[0]: 0}],
            expression_offsets=[0],
        ),
        dict(
            testcase_name="x[0] == x[0]",
            name="x0_eq_x0",
            bounded_exprs=lambda x, y: x[0] == x[0],
            constraint_count=1,
            lower_bounds=[0],
            upper_bounds=[0],
            expression_terms=lambda x, y: [{x[0]: 0}],
            expression_offsets=[0],
        ),
        dict(
            testcase_name="x[0] >= x[0]",
            name="x0_ge_x0",
            bounded_exprs=lambda x, y: x[0] >= x[0],
            constraint_count=1,
            lower_bounds=[0],
            upper_bounds=[math.inf],
            expression_terms=lambda x, y: [{x[0]: 0}],
            expression_offsets=[0],
        ),
        dict(
            # x[0] - x[0] <= 3
            testcase_name="x[0] - 1 <= x[0] + 2",
            name="x0c_le_x0c",
            bounded_exprs=lambda x, y: pd.Series(x[0] - 1 <= x[0] + 2),
            constraint_count=1,
            lower_bounds=[-math.inf],
            upper_bounds=[3],
            expression_terms=lambda x, y: [{x[0]: 0}],
            expression_offsets=[0],
        ),
        dict(
            # x[0] - x[0] == 3
            testcase_name="x[0] - 1 == x[0] + 2",
            name="x0c_eq_x0c",
            bounded_exprs=lambda x, y: pd.Series(x[0] - 1 == x[0] + 2),
            constraint_count=1,
            lower_bounds=[3],
            upper_bounds=[3],
            expression_terms=lambda x, y: [{x[0]: 0}],
            expression_offsets=[0],
        ),
        dict(
            # x[0] - x[0] >= 3
            testcase_name="x[0] - 1 >= x[0] + 2",
            name="x0c_ge_x0c",
            bounded_exprs=lambda x, y: pd.Series(x[0] - 1 >= x[0] + 2),
            constraint_count=1,
            lower_bounds=[3],
            upper_bounds=[math.inf],
            expression_terms=lambda x, y: [{x[0]: 0}],
            expression_offsets=[0],
        ),
        dict(
            testcase_name="x[0] <= x[1]",
            name="x0_le_x1",
            bounded_exprs=lambda x, y: x[0] <= x[1],
            constraint_count=1,
            lower_bounds=[-math.inf],
            upper_bounds=[0],
            expression_terms=lambda x, y: [{x[0]: 1, x[1]: -1}],
            expression_offsets=[0],
        ),
        dict(
            testcase_name="x[0] == x[1]",
            name="x0_eq_x1",
            bounded_exprs=lambda x, y: x[0] == x[1],
            constraint_count=1,
            lower_bounds=[0],
            upper_bounds=[0],
            expression_terms=lambda x, y: [{x[0]: 1, x[1]: -1}],
            expression_offsets=[0],
        ),
        dict(
            testcase_name="x[0] >= x[1]",
            name="x0_ge_x1",
            bounded_exprs=lambda x, y: x[0] >= x[1],
            constraint_count=1,
            lower_bounds=[0],
            upper_bounds=[math.inf],
            expression_terms=lambda x, y: [{x[0]: 1, x[1]: -1}],
            expression_offsets=[0],
        ),
        dict(
            # x[0] - x[1] <= -3
            testcase_name="x[0] + 1 <= x[1] - 2",
            name="x0c_le_x1c",
            bounded_exprs=lambda x, y: x[0] + 1 <= x[1] - 2,
            constraint_count=1,
            lower_bounds=[-math.inf],
            upper_bounds=[-3],
            expression_terms=lambda x, y: [{x[0]: 1, x[1]: -1}],
            expression_offsets=[0],
        ),
        dict(
            # x[0] - x[1] == -3
            testcase_name="x[0] + 1 == x[1] - 2",
            name="x0c_eq_x1c",
            bounded_exprs=lambda x, y: x[0] + 1 == x[1] - 2,
            constraint_count=1,
            lower_bounds=[-3],
            upper_bounds=[-3],
            expression_terms=lambda x, y: [{x[0]: 1, x[1]: -1}],
            expression_offsets=[0],
        ),
        dict(
            # x[0] - x[1] >= -3
            testcase_name="x[0] + 1 >= x[1] - 2",
            name="x0c_ge_x1c",
            bounded_exprs=lambda x, y: pd.Series(x[0] + 1 >= x[1] - 2),
            constraint_count=1,
            lower_bounds=[-3],
            upper_bounds=[math.inf],
            expression_terms=lambda x, y: [{x[0]: 1, x[1]: -1}],
            expression_offsets=[0],
        ),
        dict(
            testcase_name="x <= 0",
            name="x_le_c",
            bounded_exprs=lambda x, y: x.apply(lambda expr: expr <= 0),
            constraint_count=3,
            lower_bounds=[-math.inf] * 3,
            upper_bounds=[0] * 3,
            expression_terms=lambda x, y: [{xi: 1} for xi in x],
            expression_offsets=[0] * 3,
        ),
        dict(
            testcase_name="x >= 0",
            name="x_ge_c",
            bounded_exprs=lambda x, y: x.apply(lambda expr: expr >= 0),
            constraint_count=3,
            lower_bounds=[0] * 3,
            upper_bounds=[math.inf] * 3,
            expression_terms=lambda x, y: [{xi: 1} for xi in x],
            expression_offsets=[0] * 3,
        ),
        dict(
            testcase_name="x == 0",
            name="x_eq_c",
            bounded_exprs=lambda x, y: x.apply(lambda expr: expr == 0),
            constraint_count=3,
            lower_bounds=[0] * 3,
            upper_bounds=[0] * 3,
            expression_terms=lambda x, y: [{xi: 1} for xi in x],
            expression_offsets=[0] * 3,
        ),
        dict(
            testcase_name="y == 0",
            name="y_eq_c",
            bounded_exprs=(lambda x, y: y.apply(lambda expr: expr == 0)),
            constraint_count=2 * 3,
            lower_bounds=[0] * 2 * 3,
            upper_bounds=[0] * 2 * 3,
            expression_terms=lambda x, y: [{yi: 1} for yi in y],
            expression_offsets=[0] * 3 * 2,
        ),
        dict(
            testcase_name='y.groupby("i").sum() == 0',
            name="ygroupbyi_eq_c",
            bounded_exprs=(
                lambda x, y: y.groupby("i").sum().apply(lambda expr: expr == 0)
            ),
            constraint_count=2,
            lower_bounds=[0] * 2,
            upper_bounds=[0] * 2,
            expression_terms=lambda x, y: [
                {y[1, "a"]: 1, y[1, "b"]: 1, y[1, "c"]: 1},
                {y[2, "a"]: 1, y[2, "b"]: 1, y[2, "c"]: 1},
            ],
            expression_offsets=[0] * 2,
        ),
        dict(
            testcase_name='y.groupby("j").sum() == 0',
            name="ygroupbyj_eq_c",
            bounded_exprs=(
                lambda x, y: y.groupby("j").sum().apply(lambda expr: expr == 0)
            ),
            constraint_count=3,
            lower_bounds=[0] * 3,
            upper_bounds=[0] * 3,
            expression_terms=lambda x, y: [
                {y[1, "a"]: 1, y[2, "a"]: 1},
                {y[1, "b"]: 1, y[2, "b"]: 1},
                {y[1, "c"]: 1, y[2, "c"]: 1},
            ],
            expression_offsets=[0] * 3,
        ),
        dict(
            testcase_name='3 * x + y.groupby("i").sum() <= 0',
            name="broadcast_align_fill",
            bounded_exprs=(
                lambda x, y: (3 * x)
                .add(y.groupby("i").sum(), fill_value=0)
                .apply(lambda expr: expr <= 0)
            ),
            constraint_count=3,
            lower_bounds=[-math.inf] * 3,
            upper_bounds=[0] * 3,
            expression_terms=lambda x, y: [
                {x[0]: 3},
                {x[1]: 3, y[1, "a"]: 1, y[1, "b"]: 1, y[1, "c"]: 1},
                {x[2]: 3, y[2, "a"]: 1, y[2, "b"]: 1, y[2, "c"]: 1},
            ],
            expression_offsets=[0] * 3,
        ),
    ]

    def create_test_model(self, name, bounded_exprs):
        model = pdm.OptimizationModel(name="test_name")
        x = model.create_variables(
            name="x",
            index=pd.Index(range(3), name="i"),
        )
        y = model.create_variables(
            name="y",
            index=pd.MultiIndex.from_product(
                ((1, 2), ("a", "b", "c")), names=["i", "j"]
            ),
        )
        model.create_linear_constraints(name=name, bounded_exprs=bounded_exprs(x, y))
        return model

    @parameterized.named_parameters(
        # pylint: disable=g-complex-comprehension
        {
            f: tc[f]
            for f in [
                "testcase_name",
                "name",
                "bounded_exprs",
                "constraint_count",
            ]
        }
        for tc in constraint_test_cases
    )
    def test_get_linear_constraints(
        self,
        name,
        bounded_exprs,
        constraint_count,
    ):
        model = self.create_test_model(name, bounded_exprs)
        for linear_constraints, expected_count in (
            (model.get_linear_constraints(), constraint_count),
            (model.get_linear_constraints(name), constraint_count),
            (model.get_linear_constraints(name), constraint_count),
        ):
            self.assertIsInstance(linear_constraints, pd.Index)
            self.assertLen(linear_constraints, expected_count)

    def test_get_linear_constraints_empty(self):
        linear_constraints = pdm.OptimizationModel(
            name="test_name"
        ).get_linear_constraints()
        self.assertIsInstance(linear_constraints, pd.Index)
        self.assertEmpty(linear_constraints)

    @parameterized.named_parameters(
        # pylint: disable=g-complex-comprehension
        {
            f: tc[f]
            for f in [
                "testcase_name",
                "name",
                "bounded_exprs",
                "constraint_count",
            ]
        }
        for tc in constraint_test_cases
    )
    def test_get_linear_constraint_references(
        self,
        name,
        bounded_exprs,
        constraint_count,
    ):
        model = self.create_test_model(name, bounded_exprs)
        for linear_constraints, expected_count in (
            (model.get_linear_constraint_references(name=name), constraint_count),
            (model.get_linear_constraint_references(name=name), constraint_count),
        ):
            self.assertIsInstance(linear_constraints, pd.Series)
            self.assertLen(linear_constraints, expected_count)
            for i in linear_constraints.index:
                self.assertEqual(repr(linear_constraints[i]), f"{name}[{i}]")

    @parameterized.named_parameters(
        # pylint: disable=g-complex-comprehension
        {
            f: tc[f]
            for f in [
                "testcase_name",
                "name",
                "bounded_exprs",
                "lower_bounds",
            ]
        }
        for tc in constraint_test_cases
    )
    def test_get_linear_constraint_lower_bounds(
        self,
        name,
        bounded_exprs,
        lower_bounds,
    ):
        model = self.create_test_model(name, bounded_exprs)
        for linear_constraint_lower_bounds in (
            model.get_linear_constraint_lower_bounds(),
            model.get_linear_constraint_lower_bounds(model.get_linear_constraints()),
            model.get_linear_constraint_lower_bounds(
                model.get_linear_constraint_references(name)
            ),
        ):
            self.assertSequenceAlmostEqual(linear_constraint_lower_bounds, lower_bounds)

    @parameterized.named_parameters(
        # pylint: disable=g-complex-comprehension
        {
            f: tc[f]
            for f in [
                "testcase_name",
                "name",
                "bounded_exprs",
                "upper_bounds",
            ]
        }
        for tc in constraint_test_cases
    )
    def test_get_linear_constraint_upper_bounds(
        self,
        name,
        bounded_exprs,
        upper_bounds,
    ):
        model = self.create_test_model(name, bounded_exprs)
        for linear_constraint_upper_bounds in (
            model.get_linear_constraint_upper_bounds(),
            model.get_linear_constraint_upper_bounds(model.get_linear_constraints()),
            model.get_linear_constraint_upper_bounds(
                model.get_linear_constraint_references(name)
            ),
        ):
            self.assertSequenceAlmostEqual(linear_constraint_upper_bounds, upper_bounds)

    @parameterized.named_parameters(
        # pylint: disable=g-complex-comprehension
        {
            f: tc[f]
            for f in [
                "testcase_name",
                "name",
                "bounded_exprs",
                "expression_terms",
                "expression_offsets",
            ]
        }
        for tc in constraint_test_cases
    )
    def test_get_linear_constraint_expressions(
        self,
        name,
        bounded_exprs,
        expression_terms,
        expression_offsets,
    ):
        model = self.create_test_model(name, bounded_exprs)
        x = model.get_variable_references(name="x")
        y = model.get_variable_references(name="y")
        for linear_constraint_expressions in (
            model.get_linear_constraint_expressions(),
            model.get_linear_constraint_expressions(model.get_linear_constraints()),
            model.get_linear_constraint_expressions(
                model.get_linear_constraint_references(name)
            ),
        ):
            expr_terms = expression_terms(x, y)
            self.assertLen(linear_constraint_expressions, len(expr_terms))
            for expr, expr_term in zip(linear_constraint_expressions, expr_terms):
                self.assertDictEqual(expr._terms, expr_term)
            self.assertSequenceAlmostEqual(
                [expr._offset for expr in linear_constraint_expressions],
                expression_offsets,
            )


class OptimizationModelObjectiveTest(parameterized.TestCase):
    _expressions = (
        lambda x, y: -3,
        lambda x, y: 0,
        lambda x, y: 10,
        lambda x, y: x[0],
        lambda x, y: x[1],
        lambda x, y: x[2],
        lambda x, y: y[0],
        lambda x, y: y[1],
        lambda x, y: x[0] + 5,
        lambda x, y: -3 + y[1],
        lambda x, y: 3 * x[0],
        lambda x, y: x[0] * 3 * 5 - 3,
        lambda x, y: x.sum(),
        lambda x, y: 101 + 2 * 3 * x.sum(),
        lambda x, y: x.sum() * 2,
        lambda x, y: sum(y),
        lambda x, y: x.sum() + 2 * y.sum() + 3,
    )
    _variable_indices = (
        pd.Index(range(3)),
        pd.Index(range(3), name="i"),
        pd.Index(range(10), name="i"),
    )

    def assertLinearExpressionAlmostEqual(
        self,
        expr1: pdm._LinearExpression,
        expr2: pdm._LinearExpression,
    ) -> None:
        """Test that the two linear expressions are almost equal."""
        for variable, coeff in expr1._terms.items():
            self.assertAlmostEqual(expr2._terms.get(variable, 0), coeff)
        for variable, coeff in expr2._terms.items():
            self.assertAlmostEqual(expr1._terms.get(variable, 0), coeff)
        self.assertAlmostEqual(expr1._offset, expr2._offset)

    @parameterized.product(
        expression=_expressions,
        variable_indices=_variable_indices,
        sense=(pdm.ObjectiveSense.MINIMIZE, pdm.ObjectiveSense.MAXIMIZE),
    )
    def test_set_objective(
        self,
        expression: Callable[[pd.Series, pd.Series], pdm._LinearType],
        variable_indices: pd.Index,
        sense: pdm.ObjectiveSense,
    ):
        model = pdm.OptimizationModel(name="test_name")
        x = model.create_variables(name="x", index=variable_indices)
        y = model.create_variables(name="y", index=variable_indices)
        objective_expression = pdm._as_flat_linear_expression(expression(x, y))
        model.set_objective(expression=objective_expression, sense=sense)
        self.assertEqual(model.get_objective_sense(), sense)
        got_objective_expression = model.get_objective_expression()
        self.assertLinearExpressionAlmostEqual(
            got_objective_expression, objective_expression
        )

    def test_set_new_objective(self):
        model = pdm.OptimizationModel(name="test_name")
        x = model.create_variables(name="x", index=pd.Index(range(3)))
        old_objective_expression = 1
        new_objective_expression = pdm._as_flat_linear_expression(x.sum() - 2.3)

        # Set and check for old objective.
        model.set_objective(
            expression=old_objective_expression, sense=pdm.ObjectiveSense.MAXIMIZE
        )
        self.assertEqual(model.get_objective_sense(), pdm.ObjectiveSense.MAXIMIZE)
        got_objective_expression = model.get_objective_expression()
        for var_coeff in got_objective_expression._terms.values():
            self.assertAlmostEqual(var_coeff, 0)
        self.assertAlmostEqual(got_objective_expression._offset, 1)

        # Set to a new objective and check that it is different.
        model.set_objective(
            expression=new_objective_expression, sense=pdm.ObjectiveSense.MINIMIZE
        )
        self.assertEqual(model.get_objective_sense(), pdm.ObjectiveSense.MINIMIZE)
        got_objective_expression = model.get_objective_expression()
        self.assertLinearExpressionAlmostEqual(
            got_objective_expression, new_objective_expression
        )

    @parameterized.product(
        expression=_expressions,
        variable_indices=_variable_indices,
    )
    def test_minimize(
        self,
        expression: Callable[[pd.Series, pd.Series], pdm._LinearType],
        variable_indices: pd.Index,
    ):
        model = pdm.OptimizationModel(name="test_name")
        x = model.create_variables(name="x", index=variable_indices)
        y = model.create_variables(name="y", index=variable_indices)
        objective_expression = pdm._as_flat_linear_expression(expression(x, y))
        model.minimize(expression=objective_expression)
        self.assertEqual(model.get_objective_sense(), pdm.ObjectiveSense.MINIMIZE)
        got_objective_expression = model.get_objective_expression()
        self.assertLinearExpressionAlmostEqual(
            got_objective_expression, objective_expression
        )

    @parameterized.product(
        expression=_expressions,
        variable_indices=_variable_indices,
    )
    def test_maximize(
        self,
        expression: Callable[[pd.Series, pd.Series], float],
        variable_indices: pd.Index,
    ):
        model = pdm.OptimizationModel(name="test_name")
        x = model.create_variables(name="x", index=variable_indices)
        y = model.create_variables(name="y", index=variable_indices)
        objective_expression = pdm._as_flat_linear_expression(expression(x, y))
        model.maximize(expression=objective_expression)
        self.assertEqual(model.get_objective_sense(), pdm.ObjectiveSense.MAXIMIZE)
        got_objective_expression = model.get_objective_expression()
        self.assertLinearExpressionAlmostEqual(
            got_objective_expression, objective_expression
        )


class OptimizationModelProtoTest(absltest.TestCase):
    def test_to_proto(self):
        expected = linear_solver_pb2.MPModelProto()
        text_format.Parse(
            """
    name: "test_name"
    maximize: true
    objective_offset: 0
    variable {
      lower_bound: 0
      upper_bound: 1000
      objective_coefficient: 1
      is_integer: false
      name: "x[0]"
    }
    variable {
      lower_bound: 0
      upper_bound: 1000
      objective_coefficient: 1
      is_integer: false
      name: "x[1]"
    }
    constraint {
      var_index: 0
      coefficient: 1
      lower_bound: -inf
      upper_bound: 10
      name: "Ct[0]"
    }
    constraint {
      var_index: 1
      coefficient: 1
      lower_bound: -inf
      upper_bound: 10
      name: "Ct[1]"
    }
    """,
            expected,
        )
        model = pdm.OptimizationModel(name="test_name")
        x = model.create_variables("x", pd.Index(range(2)), 0, 1000)
        model.create_linear_constraints("Ct", x.apply(lambda expr: expr <= 10))
        model.maximize(expression=x.sum())
        self.assertEqual(str(expected), str(model.to_proto()))


class SolverTest(parameterized.TestCase):
    _solvers = (
        {
            "type": pdm.SolverType.CP_SAT,
            "options": pdm.SolveOptions(),
            "is_integer": True,  # CP-SAT supports only pure integer variables.
        },
        {
            "type": pdm.SolverType.GLOP,
            "options": pdm.SolveOptions(
                # Disable GLOP's presolve to correctly trigger unboundedness:
                #   https://github.com/google/or-tools/issues/3319
                solver_specific_parameters="use_preprocessing: False"
            ),
            "is_integer": False,  # GLOP does not properly support integers.
        },
        {
            "type": pdm.SolverType.SCIP,
            "options": pdm.SolveOptions(),
            "is_integer": False,
        },
        {
            "type": pdm.SolverType.SCIP,
            "options": pdm.SolveOptions(),
            "is_integer": True,
        },
    )
    _variable_indices = (
        pd.Index(range(0)),  # No variables.
        pd.Index(range(1)),  # Single variable.
        pd.Index(range(3)),  # Multiple variables.
    )
    _variable_bounds = (-1, 0, 10.1)
    _solve_statuses = (
        pdm.SolveStatus.OPTIMAL,
        pdm.SolveStatus.INFEASIBLE,
        pdm.SolveStatus.UNBOUNDED,
    )
    _set_objectives = (True, False)
    _objective_senses = (
        pdm.ObjectiveSense.MAXIMIZE,
        pdm.ObjectiveSense.MINIMIZE,
    )
    _objective_expressions = (
        lambda x: x.sum(),
        lambda x: x.sum() + 5.2,
        lambda x: -10.1,
        lambda x: 0,
    )

    def _create_model(
        self,
        variable_indices: pd.Index = pd.Index(range(3)),
        variable_bound: float = 0,
        is_integer: bool = False,
        solve_status: pdm.SolveStatus = pdm.SolveStatus.OPTIMAL,
        set_objective: bool = True,
        objective_sense: pdm.ObjectiveSense = pdm.ObjectiveSense.MAXIMIZE,
        objective_expression: Callable[[pd.Series], float] = lambda x: x.sum(),
    ) -> pdm.OptimizationModel:
        """Constructs an optimization problem.

        It has the following formulation:

        ```
        objective_sense (MAXIMIZE / MINIMIZE) objective_expression(x)
        satisfying constraints
          (if solve_status != UNBOUNDED and objective_sense == MAXIMIZE)
            x[variable_indices] <= variable_bound
          (if solve_status != UNBOUNDED and objective_sense == MINIMIZE)
            x[variable_indices] >= variable_bound
          x[variable_indices] is_integer
          False (if solve_status == INFEASIBLE)
        ```

        Args:
          variable_indices (pd.Index): The indices of the variable(s).
          variable_bound (float): The upper- or lower-bound(s) of the variable(s).
          is_integer (bool): Whether the variables should be integer.
          solve_status (pdm.SolveStatus): The solve status to target.
          set_objective (bool): Whether to set the objective of the model.
          objective_sense (pdm.ObjectiveSense): MAXIMIZE or MINIMIZE,
          objective_expression (Callable[[pd.Series], float]): The expression to
            maximize or minimize if set_objective=True.

        Returns:
          pdm.OptimizationModel: The resulting problem.
        """
        model = pdm.OptimizationModel(name="test")
        # Variable(s)
        x = model.create_variables(
            name="x",
            index=pd.Index(variable_indices),
            is_integer=is_integer,
        )
        # Constraint(s)
        if solve_status == pdm.SolveStatus.INFEASIBLE:
            # Force infeasibility here to test that we get pd.NA later.
            model.create_linear_constraints("bool", False)
        elif solve_status != pdm.SolveStatus.UNBOUNDED:
            if objective_sense == pdm.ObjectiveSense.MAXIMIZE:
                model.create_linear_constraints(
                    "upper_bound", x.apply(lambda xi: xi <= variable_bound)
                )
            elif objective_sense == pdm.ObjectiveSense.MINIMIZE:
                model.create_linear_constraints(
                    "lower_bound", x.apply(lambda xi: xi >= variable_bound)
                )
        # Objective
        if set_objective:
            model.set_objective(
                expression=objective_expression(x), sense=objective_sense
            )
        return model

    @parameterized.product(
        solver=_solvers,
        variable_indices=_variable_indices,
        variable_bound=_variable_bounds,
        solve_status=_solve_statuses,
        set_objective=_set_objectives,
        objective_sense=_objective_senses,
        objective_expression=_objective_expressions,
    )
    def test_solve_status(
        self,
        solver: dict[str, Union[pdm.SolverType, pdm.SolveOptions, bool]],
        variable_indices: pd.Index,
        variable_bound: float,
        solve_status: pdm.SolveStatus,
        set_objective: bool,
        objective_sense: pdm.ObjectiveSense,
        objective_expression: Callable[[pd.Series], float],
    ):
        model = self._create_model(
            variable_indices=variable_indices,
            variable_bound=variable_bound,
            is_integer=solver["is_integer"],
            solve_status=solve_status,
            set_objective=set_objective,
            objective_sense=objective_sense,
            objective_expression=objective_expression,
        )
        solve_result = pdm.Solver(solver_type=solver["type"]).solve(
            model=model, options=solver["options"]
        )

        # pylint: disable=g-explicit-length-test
        # (we disable explicit-length-test here because `variable_indices: pd.Index`
        # evaluates to an ambiguous boolean value.)
        if len(variable_indices) > 0:  # Test cases with >=1 variable.
            self.assertNotEmpty(variable_indices)
            if (
                isinstance(
                    objective_expression(model.get_variable_references("x")),
                    (int, float),
                )
                and solve_status != pdm.SolveStatus.INFEASIBLE
            ):
                # Feasibility implies optimality when objective is a constant term.
                self.assertEqual(solve_result.get_status(), pdm.SolveStatus.OPTIMAL)
            elif not set_objective and solve_status != pdm.SolveStatus.INFEASIBLE:
                # Feasibility implies optimality when objective is not set.
                self.assertEqual(solve_result.get_status(), pdm.SolveStatus.OPTIMAL)
            elif (
                solver["type"] == pdm.SolverType.CP_SAT
                and solve_result.get_status() == pdm.SolveStatus.UNKNOWN
            ):
                # CP_SAT returns unknown for some of the infeasible and unbounded cases.
                self.assertIn(
                    solve_status,
                    (pdm.SolveStatus.INFEASIBLE, pdm.SolveStatus.UNBOUNDED),
                )
            else:
                self.assertEqual(solve_result.get_status(), solve_status)
        elif solve_status == pdm.SolveStatus.UNBOUNDED:
            # Unbounded problems are optimal when there are no variables.
            self.assertEqual(solve_result.get_status(), pdm.SolveStatus.OPTIMAL)
        else:
            self.assertEqual(solve_result.get_status(), solve_status)

    @parameterized.product(
        solver=_solvers,
        variable_indices=_variable_indices,
        variable_bound=_variable_bounds,
        solve_status=_solve_statuses,
        set_objective=_set_objectives,
        objective_sense=_objective_senses,
        objective_expression=_objective_expressions,
    )
    def test_get_variable_values(
        self,
        solver: dict[str, Union[pdm.SolverType, pdm.SolveOptions, bool]],
        variable_indices: pd.Index,
        variable_bound: float,
        solve_status: pdm.SolveStatus,
        set_objective: bool,
        objective_sense: pdm.ObjectiveSense,
        objective_expression: Callable[[pd.Series], float],
    ):
        model = self._create_model(
            variable_indices=variable_indices,
            variable_bound=variable_bound,
            is_integer=solver["is_integer"],
            solve_status=solve_status,
            set_objective=set_objective,
            objective_sense=objective_sense,
            objective_expression=objective_expression,
        )
        solve_result = pdm.Solver(solver_type=solver["type"]).solve(
            model=model, options=solver["options"]
        )
        for variables in (
            None,  # We get all variables when none is specified.
            model.get_variables()[:2],  # We can filter to a subset (pd.Index).
            model.get_variable_references("x")[:2],  # It works for pd.Series.
        ):
            variable_values = solve_result.get_variable_values(variables)
            # Test the type of `variable_values` (we always get pd.Series)
            self.assertIsInstance(variable_values, pd.Series)
            # Test the index of `variable_values` (match the input variables [if any])
            self.assertSequenceAlmostEqual(
                variable_values.index,
                pdm._get_index(model._get_variables(variables)),
            )
            if solve_result.get_status() not in (
                pdm.SolveStatus.OPTIMAL,
                pdm.SolveStatus.FEASIBLE,
            ):
                # self.assertSequenceAlmostEqual does not work here because we cannot do
                # equality comparison for NA values (NAs will propagate and we will get
                # 'TypeError: boolean value of NA is ambiguous')
                for variable_value in variable_values:
                    self.assertTrue(pd.isna(variable_value))
            elif set_objective and not isinstance(
                objective_expression(model.get_variable_references("x")),
                (int, float),
            ):
                # The variable values are only well-defined when the objective is set
                # and depends on the variable(s).
                if not solver["is_integer"]:
                    self.assertSequenceAlmostEqual(
                        variable_values, [variable_bound] * len(variable_values)
                    )
                elif objective_sense == pdm.ObjectiveSense.MAXIMIZE:
                    self.assertTrue(solver["is_integer"])  # Assert a known assumption.
                    self.assertSequenceAlmostEqual(
                        variable_values,
                        [math.floor(variable_bound)] * len(variable_values),
                    )
                else:
                    self.assertTrue(solver["is_integer"])  # Assert a known assumption.
                    self.assertEqual(objective_sense, pdm.ObjectiveSense.MINIMIZE)
                    self.assertSequenceAlmostEqual(
                        variable_values,
                        [math.ceil(variable_bound)] * len(variable_values),
                    )

    @parameterized.product(
        solver=_solvers,
        variable_indices=_variable_indices,
        variable_bound=_variable_bounds,
        solve_status=_solve_statuses,
        set_objective=_set_objectives,
        objective_sense=_objective_senses,
        objective_expression=_objective_expressions,
    )
    def test_get_objective_value(
        self,
        solver: dict[str, Union[pdm.SolverType, pdm.SolveOptions, bool]],
        variable_indices: pd.Index,
        variable_bound: float,
        solve_status: pdm.SolveStatus,
        set_objective: bool,
        objective_sense: pdm.ObjectiveSense,
        objective_expression: Callable[[pd.Series], float],
    ):
        model = self._create_model(
            variable_indices=variable_indices,
            variable_bound=variable_bound,
            is_integer=solver["is_integer"],
            solve_status=solve_status,
            set_objective=set_objective,
            objective_sense=objective_sense,
            objective_expression=objective_expression,
        )
        solve_result = pdm.Solver(solver_type=solver["type"]).solve(
            model=model, options=solver["options"]
        )

        # Test objective value
        if solve_result.get_status() not in (
            pdm.SolveStatus.OPTIMAL,
            pdm.SolveStatus.FEASIBLE,
        ):
            self.assertTrue(pd.isna(solve_result.get_objective_value()))
            return
        if set_objective:
            variable_values = solve_result.get_variable_values(model.get_variables())
            self.assertAlmostEqual(
                solve_result.get_objective_value(),
                objective_expression(variable_values),
            )
        else:
            self.assertAlmostEqual(solve_result.get_objective_value(), 0)


if __name__ == "__main__":
    absltest.main()
