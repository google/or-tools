#!/usr/bin/env python3
# Copyright 2010-2024 Google LLC
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

"""Tests for ModelBuilder."""

import math
from typing import Any, Callable, Dict, Mapping, Union

from absl.testing import absltest
from absl.testing import parameterized
import numpy as np
import numpy.testing as np_testing
import pandas as pd

import os

from google.protobuf import text_format
from ortools.linear_solver import linear_solver_pb2
from ortools.linear_solver.python import model_builder as mb


def build_dict(expr: mb.LinearExprT) -> Dict[mb.Variable, float]:
    res = {}
    for i, c in zip(expr.variable_indices, expr.coefficients):
        if not c:
            continue
        var = mb.Variable(expr.helper, lb=i, ub=None, is_integral=None, name=None)
        res[var] = c
    return res


class ModelBuilderTest(absltest.TestCase):
    # Number of decimal places to use for numerical tolerance for
    # checking primal, dual, objective values and other values.
    NUM_PLACES = 5

    # pylint: disable=too-many-statements
    def run_minimal_linear_example(self, solver_name):
        """Minimal Linear Example."""
        model = mb.Model()
        model.name = "minimal_linear_example"
        x1 = model.new_num_var(0.0, math.inf, "x1")
        x2 = model.new_num_var(0.0, math.inf, "x2")
        x3 = model.new_num_var(0.0, math.inf, "x3")
        self.assertEqual(3, model.num_variables)
        self.assertFalse(x1.is_integral)
        self.assertEqual(0.0, x1.lower_bound)
        self.assertEqual(math.inf, x2.upper_bound)
        x1.lower_bound = 1.0
        self.assertEqual(1.0, x1.lower_bound)

        model.maximize(10.0 * x1 + 6 * x2 + 4.0 * x3 - 3.5)
        self.assertEqual(4.0, x3.objective_coefficient)
        self.assertEqual(-3.5, model.objective_offset)
        model.objective_offset = -5.5
        self.assertEqual(-5.5, model.objective_offset)

        c0 = model.add(x1 + x2 + x3 <= 100.0)
        self.assertEqual(100, c0.upper_bound)
        c1 = model.add(10 * x1 + 4.0 * x2 + 5.0 * x3 <= 600.0, "c1")
        self.assertEqual("c1", c1.name)
        c2 = model.add(2.0 * x1 + 2.0 * x2 + 6.0 * x3 <= 300.0)
        self.assertEqual(-math.inf, c2.lower_bound)

        solver = mb.Solver(solver_name)
        if not solver.solver_is_supported():
            print(f"Solver {solver_name} is not supported")
            return
        self.assertTrue(pd.isna(solver.value(x1)))
        self.assertTrue(pd.isna(solver.value(x2)))
        self.assertTrue(pd.isna(solver.value(x3)))
        self.assertTrue(pd.isna(solver.reduced_cost(x1)))
        self.assertTrue(pd.isna(solver.reduced_cost(x2)))
        self.assertTrue(pd.isna(solver.dual_value(c0)))
        self.assertTrue(pd.isna(solver.dual_value(c1)))
        self.assertTrue(pd.isna(solver.dual_value(c2)))
        self.assertTrue(pd.isna(solver.activity(c0)))
        self.assertTrue(pd.isna(solver.activity(c1)))
        self.assertTrue(pd.isna(solver.activity(c2)))
        self.assertTrue(pd.isna(solver.objective_value))
        self.assertTrue(pd.isna(solver.best_objective_bound))
        self.assertEqual(mb.SolveStatus.OPTIMAL, solver.solve(model))

        # The problem has an optimal solution.
        self.assertAlmostEqual(
            733.333333 + model.objective_offset,
            solver.objective_value,
            places=self.NUM_PLACES,
        )
        self.assertAlmostEqual(
            solver.value(10.0 * x1 + 6 * x2 + 4.0 * x3 - 5.5),
            solver.objective_value,
            places=self.NUM_PLACES,
        )
        self.assertAlmostEqual(33.333333, solver.value(x1), places=self.NUM_PLACES)
        self.assertAlmostEqual(66.666667, solver.value(x2), places=self.NUM_PLACES)
        self.assertAlmostEqual(0.0, solver.value(x3), places=self.NUM_PLACES)

        dual_objective_value = (
            solver.dual_value(c0) * c0.upper_bound
            + solver.dual_value(c1) * c1.upper_bound
            + solver.dual_value(c2) * c2.upper_bound
            + model.objective_offset
        )
        self.assertAlmostEqual(
            solver.objective_value, dual_objective_value, places=self.NUM_PLACES
        )

        # x1 and x2 are basic
        self.assertAlmostEqual(0.0, solver.reduced_cost(x1), places=self.NUM_PLACES)
        self.assertAlmostEqual(0.0, solver.reduced_cost(x2), places=self.NUM_PLACES)
        # x3 is non-basic
        x3_expected_reduced_cost = (
            4.0 - 1.0 * solver.dual_value(c0) - 5.0 * solver.dual_value(c1)
        )
        self.assertAlmostEqual(
            x3_expected_reduced_cost,
            solver.reduced_cost(x3),
            places=self.NUM_PLACES,
        )

        self.assertAlmostEqual(100.0, solver.activity(c0), places=self.NUM_PLACES)
        self.assertAlmostEqual(600.0, solver.activity(c1), places=self.NUM_PLACES)
        self.assertAlmostEqual(200.0, solver.activity(c2), places=self.NUM_PLACES)

        self.assertIn("minimal_linear_example", model.export_to_lp_string(False))
        self.assertIn("minimal_linear_example", model.export_to_mps_string(False))

    def test_minimal_linear_example(self):
        self.run_minimal_linear_example("glop")

    def test_import_from_mps_string(self):
        mps_data = """
* Generated by MPModelProtoExporter
*   Name             : SupportedMaximizationProblem
*   Format           : Free
*   Constraints      : 0
*   Variables        : 1
*     Binary         : 0
*     Integer        : 0
*     Continuous     : 1
NAME          SupportedMaximizationProblem
OBJSENSE
  MAX
ROWS
 N  COST
COLUMNS
    X_ONE   COST         1
BOUNDS
 UP BOUND   X_ONE        4
ENDATA
"""
        model = mb.Model()
        self.assertTrue(model.import_from_mps_string(mps_data))
        self.assertEqual(model.name, "SupportedMaximizationProblem")

    def test_import_from_mps_file(self):
        path = os.path.dirname(__file__)
        mps_path = f"{path}/../testdata/maximization.mps"
        model = mb.Model()
        self.assertTrue(model.import_from_mps_file(mps_path))
        self.assertEqual(model.name, "SupportedMaximizationProblem")

    def test_import_from_lp_string(self):
        lp_data = """
      min: x + y;
      bin: b1, b2, b3;
      1 <= x <= 42;
      constraint_num1: 5 b1 + 3b2 + x <= 7;
      4 y + b2 - 3 b3 <= 2;
      constraint_num2: -4 b1 + b2 - 3 z <= -2;
"""
        model = mb.Model()
        self.assertTrue(model.import_from_lp_string(lp_data))
        self.assertEqual(6, model.num_variables)
        self.assertEqual(3, model.num_constraints)
        self.assertEqual(1, model.var_from_index(0).lower_bound)
        self.assertEqual(42, model.var_from_index(0).upper_bound)
        self.assertEqual("x", model.var_from_index(0).name)

    def test_import_from_lp_file(self):
        path = os.path.dirname(__file__)
        lp_path = f"{path}/../testdata/small_model.lp"
        model = mb.Model()
        self.assertTrue(model.import_from_lp_file(lp_path))
        self.assertEqual(6, model.num_variables)
        self.assertEqual(3, model.num_constraints)
        self.assertEqual(1, model.var_from_index(0).lower_bound)
        self.assertEqual(42, model.var_from_index(0).upper_bound)
        self.assertEqual("x", model.var_from_index(0).name)

    def test_class_api(self):
        model = mb.Model()
        x = model.new_int_var(0, 10, "x")
        y = model.new_int_var(1, 10, "y")
        z = model.new_int_var(2, 10, "z")
        t = model.new_int_var(3, 10, "t")

        e1 = mb.LinearExpr.sum([x, y, z])
        expected_vars = np.array([0, 1, 2], dtype=np.int32)
        np_testing.assert_array_equal(expected_vars, e1.variable_indices)
        np_testing.assert_array_equal(
            np.array([1, 1, 1], dtype=np.double), e1.coefficients
        )
        self.assertEqual(e1.constant, 0.0)
        self.assertEqual(e1.__str__(), "x + y + z")

        e2 = mb.LinearExpr.sum([e1, 4.0])
        np_testing.assert_array_equal(expected_vars, e2.variable_indices)
        np_testing.assert_array_equal(
            np.array([1, 1, 1], dtype=np.double), e2.coefficients
        )
        self.assertEqual(e2.constant, 4.0)
        self.assertEqual(e2.__str__(), "x + y + z + 4.0")

        e3 = mb.LinearExpr.term(e2, 2)
        np_testing.assert_array_equal(expected_vars, e3.variable_indices)
        np_testing.assert_array_equal(
            np.array([2, 2, 2], dtype=np.double), e3.coefficients
        )
        self.assertEqual(e3.constant, 8.0)
        self.assertEqual(e3.__str__(), "2.0 * x + 2.0 * y + 2.0 * z + 8.0")

        e4 = mb.LinearExpr.weighted_sum([x, t], [-1, 1], constant=2)
        np_testing.assert_array_equal(
            np.array([0, 3], dtype=np.int32), e4.variable_indices
        )
        np_testing.assert_array_equal(
            np.array([-1, 1], dtype=np.double), e4.coefficients
        )
        self.assertEqual(e4.constant, 2.0)
        self.assertEqual(e4.__str__(), "-x + t + 2.0")

        e4b = mb.LinearExpr.weighted_sum([e4 * 3], [1])
        np_testing.assert_array_equal(
            np.array([0, 3], dtype=np.int32), e4b.variable_indices
        )
        np_testing.assert_array_equal(
            np.array([-3, 3], dtype=np.double), e4b.coefficients
        )
        self.assertEqual(e4b.constant, 6.0)
        self.assertEqual(e4b.__str__(), "-3.0 * x + 3.0 * t + 6.0")

        e5 = mb.LinearExpr.sum([e1, -3, e4])
        np_testing.assert_array_equal(
            np.array([1, 2, 3], dtype=np.int32), e5.variable_indices
        )
        np_testing.assert_array_equal(
            np.array([1, 1, 1], dtype=np.double), e5.coefficients
        )
        self.assertEqual(e5.constant, -1.0)
        self.assertEqual(e5.__str__(), "y + z + t - 1.0")

        e6 = mb.LinearExpr.term(x, 2.0, constant=1.0)
        np_testing.assert_array_equal(
            np.array([0], dtype=np.int32), e6.variable_indices
        )
        np_testing.assert_array_equal(np.array([2], dtype=np.double), e6.coefficients)
        self.assertEqual(e6.constant, 1.0)

        e7 = mb.LinearExpr.term(x, 1.0, constant=0.0)
        self.assertEqual(x, e7)

        e8 = mb.LinearExpr.term(2, 3, constant=4)
        self.assertEqual(e8, 10)

        e9 = mb.LinearExpr.term(x * 2 + 3, 1, constant=0)
        e10 = mb.LinearExpr.term(x, 2, constant=3)
        self.assertEqual(
            str(mb._as_flat_linear_expression(e9)),
            str(mb._as_flat_linear_expression(e10)),
        )

    def test_variables(self):
        model = mb.Model()
        x = model.new_int_var(0.0, 4.0, "x")
        self.assertEqual(0, x.index)
        self.assertEqual(0.0, x.lower_bound)
        self.assertEqual(4.0, x.upper_bound)
        self.assertEqual("x", x.name)
        x.lower_bound = 1.0
        x.upper_bound = 3.0
        self.assertEqual(1.0, x.lower_bound)
        self.assertEqual(3.0, x.upper_bound)
        self.assertTrue(x.is_integral)

        # Tests the equality operator.
        y = model.new_int_var(0.0, 4.0, "y")
        x_copy = model.var_from_index(0)
        self.assertEqual(x, x)
        self.assertEqual(x, x_copy)
        self.assertNotEqual(x, y)

        # Tests the hash method.
        var_set = set()
        var_set.add(x)
        self.assertIn(x, var_set)
        self.assertIn(x_copy, var_set)
        self.assertNotIn(y, var_set)

    def test_duplicate_variables(self):
        model = mb.Model()
        x = model.new_int_var(0.0, 4.0, "x")
        y = model.new_int_var(0.0, 4.0, "y")
        z = model.new_int_var(0.0, 4.0, "z")
        model.add(x + 2 * y == x - z)
        model.minimize(x + y + z)
        solver = mb.Solver("sat")
        self.assertEqual(mb.SolveStatus.OPTIMAL, solver.solve(model))

    def test_add_term(self):
        model = mb.Model()
        x = model.new_int_var(0.0, 4.0, "x")
        y = model.new_int_var(0.0, 4.0, "y")
        z = model.new_int_var(0.0, 4.0, "z")
        t = model.new_int_var(0.0, 4.0, "t")
        ct = model.add(x + 2 * y == 3)
        self.assertEqual(ct.helper.constraint_var_indices(ct.index), [0, 1])
        self.assertEqual(ct.helper.constraint_coefficients(ct.index), [1, 2])
        ct.add_term(x, 2)
        self.assertEqual(ct.helper.constraint_var_indices(ct.index), [0, 1])
        self.assertEqual(ct.helper.constraint_coefficients(ct.index), [3, 2])
        ct.set_coefficient(x, 5)
        self.assertEqual(ct.helper.constraint_var_indices(ct.index), [0, 1])
        self.assertEqual(ct.helper.constraint_coefficients(ct.index), [5, 2])
        ct.add_term(z, 4)
        self.assertEqual(ct.helper.constraint_var_indices(ct.index), [0, 1, 2])
        self.assertEqual(ct.helper.constraint_coefficients(ct.index), [5, 2, 4])
        ct.set_coefficient(t, -1)
        self.assertEqual(ct.helper.constraint_var_indices(ct.index), [0, 1, 2, 3])
        self.assertEqual(ct.helper.constraint_coefficients(ct.index), [5, 2, 4, -1])

    def test_issue_3614(self):
        total_number_of_choices = 5 + 1
        total_unique_products = 3
        standalone_features = list(range(5))
        feature_bundle_incidence_matrix = {}
        for idx in range(len(standalone_features)):
            feature_bundle_incidence_matrix[idx, 0] = 0
        feature_bundle_incidence_matrix[0, 0] = 1
        feature_bundle_incidence_matrix[1, 0] = 1

        bundle_start_idx = len(standalone_features)
        # Model
        model = mb.Model()
        y = {}
        v = {}
        for i in range(total_number_of_choices):
            y[i] = model.new_bool_var(f"y_{i}")

        for j in range(total_unique_products):
            for i in range(len(standalone_features)):
                v[i, j] = model.new_bool_var(f"v_{(i,j)}")
                model.add(
                    v[i, j]
                    == (
                        y[i]
                        + (
                            feature_bundle_incidence_matrix[(i, 0)]
                            * y[bundle_start_idx]
                        )
                    )
                )

        solver = mb.Solver("sat")
        status = solver.solve(model)
        self.assertEqual(mb.SolveStatus.OPTIMAL, status)

    def test_vareqvar(self):
        model = mb.Model()
        x = model.new_int_var(0.0, 4.0, "x")
        y = model.new_int_var(0.0, 4.0, "y")
        ct = x == y
        self.assertEqual(ct.left.index, x.index)
        self.assertEqual(ct.right.index, y.index)

    def test_create_false_ct(self):
        # Create the model.
        model = mb.Model()
        x = model.new_num_var(0.0, math.inf, "x")

        ct = model.add(False)
        self.assertTrue(ct.is_under_specified)
        self.assertRaises(ValueError, ct.add_term, x, 1)

        model.maximize(x)

        solver = mb.Solver("glop")
        status = solver.solve(model)
        self.assertEqual(status, mb.SolveStatus.INFEASIBLE)

    def test_create_true_ct(self):
        # Create the model.
        model = mb.Model()
        x = model.new_num_var(0.0, 5.0, "x")

        ct = model.add(True)
        self.assertEqual(ct.lower_bound, 0.0)
        self.assertEqual(ct.upper_bound, 0.0)
        self.assertTrue(ct.is_under_specified)
        self.assertRaises(ValueError, ct.add_term, x, 1)

        model.maximize(x)

        solver = mb.Solver("glop")
        status = solver.solve(model)

        self.assertEqual(status, mb.SolveStatus.OPTIMAL)


class InternalHelperTest(absltest.TestCase):

    def test_anonymous_variables(self):
        helper = mb.Model().helper
        index = helper.add_var()
        variable = mb.Variable(helper, index, None, None, None)
        self.assertEqual(variable.name, f"variable#{index}")

    def test_anonymous_constraints(self):
        helper = mb.Model().helper
        index = helper.add_linear_constraint()
        constraint = mb.LinearConstraint(helper, index=index)
        self.assertEqual(constraint.name, f"linear_constraint#{index}")


class LinearBaseTest(parameterized.TestCase):

    def setUp(self):
        super().setUp()
        simple_model = mb.Model()
        self.x = simple_model.new_var_series(
            name="x", index=pd.Index(range(3), name="i")
        )
        self.y = simple_model.new_var_series(
            name="y", index=pd.Index(range(5), name="i")
        )
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
            expected_repr="x[0] + 5.0",
        ),
        dict(
            testcase_name="x[0] - 5",
            expr=lambda x, y: x[0] - 5,
            expected_repr="x[0] - 5.0",
        ),
        dict(
            testcase_name="5 - x[0]",
            expr=lambda x, y: 5 - x[0],
            expected_repr="-x[0] + 5.0",
        ),
        dict(
            testcase_name="5 + x[0]",
            expr=lambda x, y: 5 + x[0],
            expected_repr="x[0] + 5.0",
        ),
        dict(
            testcase_name="x[0] + y[0]",
            expr=lambda x, y: x[0] + y[0],
            expected_repr="x[0] + y[0]",
        ),
        dict(
            testcase_name="x[0] + y[0] + 5",
            expr=lambda x, y: x[0] + y[0] + 5,
            expected_repr="x[0] + y[0] + 5.0",
        ),
        dict(
            testcase_name="5 + x[0] + y[0]",
            expr=lambda x, y: 5 + x[0] + y[0],
            expected_repr="x[0] + y[0] + 5.0",
        ),
        dict(
            testcase_name="5 + x[0] - x[0]",
            expr=lambda x, y: 5 + x[0] - x[0],
            expected_repr="5.0",
        ),
        dict(
            testcase_name="5 + x[0] - y[0]",
            expr=lambda x, y: 5 + x[0] - y[0],
            expected_repr="x[0] - y[0] + 5.0",
        ),
        dict(
            testcase_name="x.sum()",
            expr=lambda x, y: x.sum(),
            expected_repr="x[0] + x[1] + x[2]",
        ),
        dict(
            testcase_name="x.add(y, fill_value=0).sum() + 5",
            expr=lambda x, y: x.add(y, fill_value=0).sum() + 5,
            expected_repr="x[0] + x[1] + x[2] + y[0] + y[1] + ... + 5.0",
        ),
        dict(
            testcase_name="sum(x, y + 5)",
            expr=lambda x, y: mb.LinearExpr.sum([x.sum(), y.sum() + 5]),
            expected_repr="x[0] + x[1] + x[2] + y[0] + y[1] + ... + 5.0",
        ),
        # Product
        dict(
            testcase_name="- x.sum()",
            expr=lambda x, y: -x.sum(),
            expected_repr="-x[0] - x[1] - x[2]",
        ),
        dict(
            testcase_name="5 - x.sum()",
            expr=lambda x, y: 5 - x.sum(),
            expected_repr="-x[0] - x[1] - x[2] + 5.0",
        ),
        dict(
            testcase_name="x.sum() / 2.0",
            expr=lambda x, y: x.sum() / 2.0,
            expected_repr="0.5 * x[0] + 0.5 * x[1] + 0.5 * x[2]",
        ),
        dict(
            testcase_name="(3 * x).sum()",
            expr=lambda x, y: (3 * x).sum(),
            expected_repr="3.0 * x[0] + 3.0 * x[1] + 3.0 * x[2]",
        ),
        dict(
            testcase_name="(x * 3).sum()",
            expr=lambda x, y: (x * 3).sum(),
            expected_repr="3.0 * x[0] + 3.0 * x[1] + 3.0 * x[2]",
        ),
        dict(
            testcase_name="x.sum() * 3",
            expr=lambda x, y: x.sum() * 3,
            expected_repr="3.0 * x[0] + 3.0 * x[1] + 3.0 * x[2]",
        ),
        dict(
            testcase_name="3 * x.sum()",
            expr=lambda x, y: 3 * x.sum(),
            expected_repr="3.0 * x[0] + 3.0 * x[1] + 3.0 * x[2]",
        ),
        dict(
            testcase_name="0 * x.sum() + y.sum()",
            expr=lambda x, y: 0 * x.sum() + y.sum(),
            expected_repr="y[0] + y[1] + y[2] + y[3] + y[4]",
        ),
        # LinearExpression
        dict(
            testcase_name="_as_flat_linear_expression(x.sum())",
            expr=lambda x, y: mb._as_flat_linear_expression(x.sum()),
            expected_repr="x[0] + x[1] + x[2]",
        ),
        dict(
            testcase_name=(
                "_as_flat_linear_expression(_as_flat_linear_expression(x.sum()))"
            ),
            # pylint: disable=g-long-lambda
            expr=lambda x, y: mb._as_flat_linear_expression(
                mb._as_flat_linear_expression(x.sum())
            ),
            expected_repr="x[0] + x[1] + x[2]",
        ),
        dict(
            testcase_name="""_as_flat_linear_expression(sum([
            _as_flat_linear_expression(x.sum()),
            _as_flat_linear_expression(x.sum()),
          ]))""",
            # pylint: disable=g-long-lambda
            expr=lambda x, y: mb._as_flat_linear_expression(
                sum(
                    [
                        mb._as_flat_linear_expression(x.sum()),
                        mb._as_flat_linear_expression(x.sum()),
                    ]
                )
            ),
            expected_repr="2.0 * x[0] + 2.0 * x[1] + 2.0 * x[2]",
        ),
    )
    def test_repr(self, expr, expected_repr):
        x = self.x
        y = self.y
        self.assertEqual(repr(expr(x, y)), expected_repr)


class LinearBaseErrorsTest(absltest.TestCase):

    def test_unknown_linear_type(self):
        with self.assertRaisesRegex(TypeError, r"Unrecognized linear expression"):

            class UnknownLinearType(mb.LinearExpr):
                pass

            mb._as_flat_linear_expression(UnknownLinearType())

    def test_division_by_zero(self):
        with self.assertRaises(ZeroDivisionError):
            model = mb.Model()
            x = model.new_var_series(name="x", index=pd.Index(range(1)))
            print(x / 0)

    def test_boolean_expression(self):
        with self.assertRaisesRegex(NotImplementedError, r"Cannot use a LinearExpr"):
            model = mb.Model()
            x = model.new_var_series(name="x", index=pd.Index(range(1)))
            bool(x.sum())


class BoundedLinearBaseTest(parameterized.TestCase):

    def setUp(self):
        super().setUp()
        simple_model = mb.Model()
        self.x = simple_model.new_var_series(
            name="x", index=pd.Index(range(3), name="i")
        )
        self.y = simple_model.new_var_series(
            name="y", index=pd.Index(range(5), name="i")
        )
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
        x = self.x
        y = self.y
        l: mb.LinearExprT = lhs(x, y)
        r: mb.LinearExprT = rhs(x, y)
        result = op(l, r)
        if isinstance(l, mb.LinearExpr) or isinstance(r, mb.LinearExpr):
            self.assertIsInstance(result, mb._BoundedLinearExpr)
            self.assertIn("=", repr(result), msg="is one of ==, <=, or >=")
        else:
            self.assertIsInstance(result, bool)

    def test_doublesided_bounded_expressions(self):
        x = self.x
        self.assertEqual(
            "0.0 <= x[0] <= 1.0", repr(mb.BoundedLinearExpression(x[0], 0, 1))
        )

    def test_free_bounded_expressions(self):
        self.assertEqual(
            "x[0] free",
            repr(mb.BoundedLinearExpression(self.x[0], -math.inf, math.inf)),
        )

    def test_var_eq_var_as_bool(self):
        x = self.x
        y = self.y
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
            model = mb.Model()
            x = model.new_var_series(name="x", index=pd.Index(range(1)))
            bool(mb.BoundedLinearExpression(x, 0, 1))


class ModelBuilderErrorsTest(absltest.TestCase):

    def test_new_var_series_errors(self):
        with self.assertRaisesRegex(TypeError, r"Non-index object"):
            model = mb.Model()
            model.new_var_series(name="", index=pd.DataFrame())
        with self.assertRaisesRegex(TypeError, r"invalid type"):
            model = mb.Model()
            model.new_var_series(name="x", index=pd.Index([0]), lower_bounds="0")
        with self.assertRaisesRegex(TypeError, r"invalid type"):
            model = mb.Model()
            model.new_var_series(name="x", index=pd.Index([0]), upper_bounds="0")
        with self.assertRaisesRegex(TypeError, r"invalid type"):
            model = mb.Model()
            model.new_var_series(name="x", index=pd.Index([0]), is_integral="True")
        with self.assertRaisesRegex(ValueError, r"not a valid identifier"):
            model = mb.Model()
            model.new_var_series(name="", index=pd.Index([0]))
        with self.assertRaisesRegex(ValueError, r"is greater than"):
            model = mb.Model()
            model.new_var_series(
                name="x",
                index=pd.Index([0]),
                lower_bounds=0.2,
                upper_bounds=0.1,
            )
        with self.assertRaisesRegex(ValueError, r"is greater than"):
            model = mb.Model()
            model.new_var_series(
                name="x",
                index=pd.Index([0]),
                lower_bounds=0.1,
                upper_bounds=0.2,
                is_integral=True,
            )
        with self.assertRaisesRegex(ValueError, r"index does not match"):
            model = mb.Model()
            model.new_var_series(
                name="x", index=pd.Index([0]), lower_bounds=pd.Series([1, 2])
            )
        with self.assertRaisesRegex(ValueError, r"index does not match"):
            model = mb.Model()
            model.new_var_series(
                name="x", index=pd.Index([0]), upper_bounds=pd.Series([1, 2])
            )
        with self.assertRaisesRegex(ValueError, r"index does not match"):
            model = mb.Model()
            model.new_var_series(
                name="x", index=pd.Index([0]), is_integral=pd.Series([False, True])
            )

    def test_add_linear_constraints_errors(self):
        with self.assertRaisesRegex(TypeError, r"Not supported"):
            model = mb.Model()
            model.add("True", name="c")
        with self.assertRaisesRegex(TypeError, r"invalid type="):
            model = mb.Model()
            model.add(pd.Series(["T"]), name="c")


class ModelBuilderVariablesTest(parameterized.TestCase):
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
    def test_new_var_series(self, index, bounds, is_integer):
        model = mb.Model()
        variables = model.new_var_series(
            name="test_variable",
            index=index,
            lower_bounds=bounds(index)[0],
            upper_bounds=bounds(index)[1],
            is_integral=is_integer(index),
        )
        self.assertLen(variables, len(index))
        self.assertLen(set(variables), len(index))
        for i in index:
            self.assertEqual(repr(variables[i]), f"test_variable[{i}]")

    @parameterized.product(
        index=_variable_indices, bounds=_bounds, is_integer=_is_integer
    )
    def test_get_variable_lower_bounds(self, index, bounds, is_integer):
        lower_bound, upper_bound = bounds(index)
        model = mb.Model()
        x = model.new_var_series(
            name="x",
            index=index,
            lower_bounds=lower_bound,
            upper_bounds=upper_bound,
            is_integral=is_integer(index),
        )
        y = model.new_var_series(
            name="y",
            index=index,
            lower_bounds=lower_bound,
            upper_bounds=upper_bound,
            is_integral=is_integer(index),
        )
        for lower_bounds in (
            model.get_variable_lower_bounds(x),
            model.get_variable_lower_bounds(y),
        ):
            self.assertSequenceAlmostEqual(
                lower_bounds,
                mb._convert_to_series_and_validate_index(lower_bound, index),
            )
        self.assertSequenceAlmostEqual(
            model.get_variable_lower_bounds(),
            pd.concat(
                [
                    model.get_variable_lower_bounds(x),
                    model.get_variable_lower_bounds(y),
                ]
            ),
        )
        variables = model.get_variables()
        lower_bounds = model.get_variable_lower_bounds(variables)
        self.assertSequenceAlmostEqual(lower_bounds.index, variables)

    @parameterized.product(
        index=_variable_indices, bounds=_bounds, is_integer=_is_integer
    )
    def test_get_variable_upper_bounds(self, index, bounds, is_integer):
        lower_bound, upper_bound = bounds(index)
        model = mb.Model()
        x = model.new_var_series(
            name="x",
            index=index,
            lower_bounds=lower_bound,
            upper_bounds=upper_bound,
            is_integral=is_integer(index),
        )
        y = model.new_var_series(
            name="y",
            index=index,
            lower_bounds=lower_bound,
            upper_bounds=upper_bound,
            is_integral=is_integer(index),
        )
        for upper_bounds in (
            model.get_variable_upper_bounds(x),
            model.get_variable_upper_bounds(y),
        ):
            self.assertSequenceAlmostEqual(
                upper_bounds,
                mb._convert_to_series_and_validate_index(upper_bound, index),
            )
        self.assertSequenceAlmostEqual(
            model.get_variable_upper_bounds(),
            pd.concat(
                [
                    model.get_variable_upper_bounds(x),
                    model.get_variable_upper_bounds(y),
                ]
            ),
        )
        variables = model.get_variables()
        upper_bounds = model.get_variable_upper_bounds(variables)
        self.assertSequenceAlmostEqual(upper_bounds.index, variables)


class ModelBuilderLinearConstraintsTest(parameterized.TestCase):
    constraint_test_cases = [
        # pylint: disable=g-long-lambda
        dict(
            testcase_name="True",
            name="true",
            bounded_exprs=lambda x, y: True,
            constraint_count=1,
            lower_bounds=[0.0],
            upper_bounds=[0.0],
            expression_terms=lambda x, y: [{}],
            expression_offsets=[0],
        ),
        dict(
            testcase_name="pd.Series(True)",
            name="true",
            bounded_exprs=lambda x, y: pd.Series(True),
            constraint_count=1,
            lower_bounds=[0.0],
            upper_bounds=[0.0],
            expression_terms=lambda x, y: [{}],
            expression_offsets=[0],
        ),
        dict(
            testcase_name="False",
            name="false",
            bounded_exprs=lambda x, y: False,
            constraint_count=1,
            lower_bounds=[1.0],
            upper_bounds=[-1.0],
            expression_terms=lambda x, y: [{}],
            expression_offsets=[0],
        ),
        dict(
            testcase_name="pd.Series(False)",
            name="false",
            bounded_exprs=lambda x, y: pd.Series(False),
            constraint_count=1,
            lower_bounds=[1.0],
            upper_bounds=[-1.0],
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
            expression_terms=lambda x, y: [{}],
            expression_offsets=[0],
        ),
        dict(
            testcase_name="x[0] == x[0]",
            name="x0_eq_x0",
            bounded_exprs=lambda x, y: x[0] == x[0],
            constraint_count=1,
            lower_bounds=[0],
            upper_bounds=[0],
            expression_terms=lambda x, y: [{}],
            expression_offsets=[0],
        ),
        dict(
            testcase_name="pd.Series(x[0] == x[0])",
            name="x0_eq_x0_series",
            bounded_exprs=lambda x, y: pd.Series(x[0] == x[0]),
            constraint_count=1,
            lower_bounds=[0],
            upper_bounds=[0],
            expression_terms=lambda x, y: [{}],
            expression_offsets=[0],
        ),
        dict(
            testcase_name="x[0] >= x[0]",
            name="x0_ge_x0",
            bounded_exprs=lambda x, y: x[0] >= x[0],
            constraint_count=1,
            lower_bounds=[0],
            upper_bounds=[math.inf],
            expression_terms=lambda x, y: [{}],
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
            expression_terms=lambda x, y: [{}],
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
            expression_terms=lambda x, y: [{}],
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
            expression_terms=lambda x, y: [{}],
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
        model = mb.Model()
        x = model.new_var_series(
            name="x",
            index=pd.Index(range(3), name="i"),
        )
        y = model.new_var_series(
            name="y",
            index=pd.MultiIndex.from_product(
                ((1, 2), ("a", "b", "c")), names=["i", "j"]
            ),
        )
        model.add(name=name, ct=bounded_exprs(x, y))
        return model, {"x": x, "y": y}

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
        model, _ = self.create_test_model(name, bounded_exprs)
        linear_constraints = model.get_linear_constraints()
        self.assertIsInstance(linear_constraints, pd.Index)
        self.assertLen(linear_constraints, constraint_count)

    def test_get_linear_constraints_empty(self):
        linear_constraints = mb.Model().get_linear_constraints()
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
        model, _ = self.create_test_model(name, bounded_exprs)
        for linear_constraint_lower_bounds in (
            model.get_linear_constraint_lower_bounds(),
            model.get_linear_constraint_lower_bounds(model.get_linear_constraints()),
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
        model, _ = self.create_test_model(name, bounded_exprs)
        for linear_constraint_upper_bounds in (
            model.get_linear_constraint_upper_bounds(),
            model.get_linear_constraint_upper_bounds(model.get_linear_constraints()),
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
        model, variables = self.create_test_model(name, bounded_exprs)
        x = variables["x"]
        y = variables["y"]
        for linear_constraint_expressions in (
            model.get_linear_constraint_expressions(),
            model.get_linear_constraint_expressions(model.get_linear_constraints()),
        ):
            expr_terms = expression_terms(x, y)
            self.assertLen(linear_constraint_expressions, len(expr_terms))
            for expr, expr_term in zip(linear_constraint_expressions, expr_terms):
                self.assertDictEqual(build_dict(expr), expr_term)
            self.assertSequenceAlmostEqual(
                [expr._offset for expr in linear_constraint_expressions],
                expression_offsets,
            )


class ModelBuilderObjectiveTest(parameterized.TestCase):
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
        expr1: mb._LinearExpression,
        expr2: mb._LinearExpression,
    ) -> None:
        """Test that the two linear expressions are almost equal."""
        self.assertEqual(len(expr1.variable_indices), len(expr2.variable_indices))
        if len(expr1.variable_indices) > 0:  # pylint: disable=g-explicit-length-test
            self.assertSequenceEqual(expr1.variable_indices, expr2.variable_indices)
            self.assertSequenceAlmostEqual(
                expr1.coefficients, expr2.coefficients, places=5
            )
        else:
            self.assertEmpty(expr2.coefficients)
        self.assertAlmostEqual(expr1._offset, expr2._offset)

    @parameterized.product(
        expression=_expressions,
        variable_indices=_variable_indices,
        is_maximize=(True, False),
    )
    def test_set_objective(
        self,
        expression: Callable[[pd.Series, pd.Series], mb.LinearExprT],
        variable_indices: pd.Index,
        is_maximize: bool,
    ):
        model = mb.Model()
        x = model.new_var_series(name="x", index=variable_indices)
        y = model.new_var_series(name="y", index=variable_indices)
        objective_expression = mb._as_flat_linear_expression(expression(x, y))
        if is_maximize:
            model.maximize(objective_expression)
        else:
            model.minimize(objective_expression)
        got_objective_expression = model.objective_expression()
        self.assertLinearExpressionAlmostEqual(
            got_objective_expression, objective_expression
        )

    def test_set_new_objective(self):
        model = mb.Model()
        x = model.new_var_series(name="x", index=pd.Index(range(3)))
        old_objective_expression = 1
        new_objective_expression = mb._as_flat_linear_expression(x.sum() - 2.3)

        # Set and check for old objective.
        model.maximize(old_objective_expression)
        got_objective_expression = model.objective_expression()
        for var_coeff in got_objective_expression.coefficients:
            self.assertAlmostEqual(var_coeff, 0)
        self.assertAlmostEqual(got_objective_expression._offset, 1)

        # Set to a new objective and check that it is different.
        model.minimize(new_objective_expression)
        got_objective_expression = model.objective_expression()
        self.assertLinearExpressionAlmostEqual(
            got_objective_expression, new_objective_expression
        )

    @parameterized.product(
        expression=_expressions,
        variable_indices=_variable_indices,
    )
    def test_minimize(
        self,
        expression: Callable[[pd.Series, pd.Series], mb.LinearExprT],
        variable_indices: pd.Index,
    ):
        model = mb.Model()
        x = model.new_var_series(name="x", index=variable_indices)
        y = model.new_var_series(name="y", index=variable_indices)
        objective_expression = mb._as_flat_linear_expression(expression(x, y))
        model.minimize(objective_expression)
        got_objective_expression = model.objective_expression()
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
        model = mb.Model()
        x = model.new_var_series(name="x", index=variable_indices)
        y = model.new_var_series(name="y", index=variable_indices)
        objective_expression = mb._as_flat_linear_expression(expression(x, y))
        model.maximize(objective_expression)
        got_objective_expression = model.objective_expression()
        self.assertLinearExpressionAlmostEqual(
            got_objective_expression, objective_expression
        )


class ModelBuilderProtoTest(absltest.TestCase):

    def test_export_to_proto(self):
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
        model = mb.Model()
        model.name = "test_name"
        x = model.new_var_series("x", pd.Index(range(2)), 0, 1000)
        model.add(ct=x.apply(lambda expr: expr <= 10), name="Ct")
        model.maximize(x.sum())
        self.assertEqual(str(expected), str(model.export_to_proto()))


class SolverTest(parameterized.TestCase):
    _solvers = (
        {
            "name": "sat",
            "is_integer": True,  # CP-SAT supports only pure integer variables.
        },
        {
            "name": "glop",
            "solver_specific_parameters": "use_preprocessing: False",
            "is_integer": False,  # GLOP does not properly support integers.
        },
        {
            "name": "scip",
            "is_integer": False,
        },
        {
            "name": "scip",
            "is_integer": True,
        },
        {
            "name": "highs_lp",
            "is_integer": False,
        },
        {
            "name": "highs",
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
        mb.SolveStatus.OPTIMAL,
        mb.SolveStatus.INFEASIBLE,
        mb.SolveStatus.UNBOUNDED,
    )
    _set_objectives = (True, False)
    _objective_senses = (True, False)
    _objective_expressions = (
        sum,
        lambda x: sum(x) + 5.2,
        lambda x: -10.1,
        lambda x: 0,
    )

    def _create_model(
        self,
        variable_indices: pd.Index = pd.Index(range(3)),
        variable_bound: float = 0,
        is_integer: bool = False,
        solve_status: mb.SolveStatus = mb.SolveStatus.OPTIMAL,
        set_objective: bool = True,
        is_maximize: bool = True,
        objective_expression: Callable[[pd.Series], float] = lambda x: x.sum(),
    ) -> mb.ModelBuilder:
        """Constructs an optimization problem.

        It has the following formulation:

        ```
        maximize / minimize objective_expression(x)
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
          solve_status (mb.SolveStatus): The solve status to target.
          set_objective (bool): Whether to set the objective of the model.
          is_maximize (bool): Whether to maximize the objective of the model.
          objective_expression (Callable[[pd.Series], float]): The expression to
            maximize or minimize if set_objective=True.

        Returns:
          mb.ModelBuilder: The resulting problem.
        """
        model = mb.Model()
        # Variable(s)
        x = model.new_var_series(
            name="x",
            index=pd.Index(variable_indices),
            is_integral=is_integer,
        )
        # Constraint(s)
        if solve_status == mb.SolveStatus.INFEASIBLE:
            # Force infeasibility here to test that we get pd.NA later.
            model.add(False, name="bool")
        elif solve_status != mb.SolveStatus.UNBOUNDED:
            if is_maximize:
                model.add(x.apply(lambda xi: xi <= variable_bound), "upper_bound")
            else:
                model.add(x.apply(lambda xi: xi >= variable_bound), "lower_bound")
        # Objective
        if set_objective:
            if is_maximize:
                model.maximize(objective_expression(x))
            else:
                model.minimize(objective_expression(x))
        return model

    @parameterized.product(
        solver=_solvers,
        variable_indices=_variable_indices,
        variable_bound=_variable_bounds,
        solve_status=_solve_statuses,
        set_objective=_set_objectives,
        is_maximize=_objective_senses,
        objective_expression=_objective_expressions,
    )
    def test_solve_status(
        self,
        solver: Dict[str, Union[str, Mapping[str, Any], bool]],
        variable_indices: pd.Index,
        variable_bound: float,
        solve_status: mb.SolveStatus,
        set_objective: bool,
        is_maximize: bool,
        objective_expression: Callable[[pd.Series], float],
    ):
        model = self._create_model(
            variable_indices=variable_indices,
            variable_bound=variable_bound,
            is_integer=solver["is_integer"],
            solve_status=solve_status,
            set_objective=set_objective,
            is_maximize=is_maximize,
            objective_expression=objective_expression,
        )
        model_solver = mb.Solver(solver["name"])
        if not model_solver.solver_is_supported():
            print(f'Solver {solver["name"]} is not supported')
            return
        if solver.get("solver_specific_parameters"):
            model_solver.set_solver_specific_parameters(
                solver.get("solver_specific_parameters")
            )
        got_solve_status = model_solver.solve(model)

        # pylint: disable=g-explicit-length-test
        # (we disable explicit-length-test here because `variable_indices: pd.Index`
        # evaluates to an ambiguous boolean value.)
        if len(variable_indices) > 0:  # Test cases with >=1 variable.
            self.assertNotEmpty(variable_indices)
            if (
                isinstance(
                    objective_expression(model.get_variables()),
                    (int, float),
                )
                and solve_status != mb.SolveStatus.INFEASIBLE
            ):
                # Feasibility implies optimality when objective is a constant term.
                self.assertEqual(got_solve_status, mb.SolveStatus.OPTIMAL)
            elif not set_objective and solve_status != mb.SolveStatus.INFEASIBLE:
                # Feasibility implies optimality when objective is not set.
                self.assertEqual(got_solve_status, mb.SolveStatus.OPTIMAL)
            elif solver["name"] == "sat" and got_solve_status == 8:
                # CP_SAT returns status=8 for some infeasible and unbounded cases.
                self.assertIn(
                    solve_status,
                    (mb.SolveStatus.INFEASIBLE, mb.SolveStatus.UNBOUNDED),
                )
            elif (
                solver["name"] == "highs"
                and got_solve_status == mb.SolveStatus.INFEASIBLE
                and solve_status == mb.SolveStatus.UNBOUNDED
            ):
                # Highs is can return INFEASIBLE when UNBOUNDED is expected.
                pass
            else:
                self.assertEqual(got_solve_status, solve_status)
        elif solve_status == mb.SolveStatus.UNBOUNDED:
            # Unbounded problems are optimal when there are no variables.
            self.assertEqual(got_solve_status, mb.SolveStatus.OPTIMAL)
        else:
            self.assertEqual(got_solve_status, solve_status)

    @parameterized.product(
        solver=_solvers,
        variable_indices=_variable_indices,
        variable_bound=_variable_bounds,
        solve_status=_solve_statuses,
        set_objective=_set_objectives,
        is_maximize=_objective_senses,
        objective_expression=_objective_expressions,
    )
    def test_get_variable_values(
        self,
        solver: Dict[str, Union[str, Mapping[str, Any], bool]],
        variable_indices: pd.Index,
        variable_bound: float,
        solve_status: mb.SolveStatus,
        set_objective: bool,
        is_maximize: bool,
        objective_expression: Callable[[pd.Series], float],
    ):
        model = self._create_model(
            variable_indices=variable_indices,
            variable_bound=variable_bound,
            is_integer=solver["is_integer"],
            solve_status=solve_status,
            set_objective=set_objective,
            is_maximize=is_maximize,
            objective_expression=objective_expression,
        )
        model_solver = mb.Solver(solver["name"])
        if not model_solver.solver_is_supported():
            print(f'Solver {solver["name"]} is not supported')
            return
        if solver.get("solver_specific_parameters"):
            model_solver.set_solver_specific_parameters(
                solver.get("solver_specific_parameters")
            )
        got_solve_status = model_solver.solve(model)
        variables = model.get_variables()
        variable_values = model_solver.values(variables)
        # Test the type of `variable_values` (we always get pd.Series)
        self.assertIsInstance(variable_values, pd.Series)
        # Test the index of `variable_values` (match the input variables [if any])
        self.assertSequenceAlmostEqual(
            variable_values.index,
            mb._get_index(model._get_variables(variables)),
        )
        if got_solve_status not in (
            mb.SolveStatus.OPTIMAL,
            mb.SolveStatus.FEASIBLE,
        ):
            # self.assertSequenceAlmostEqual does not work here because we cannot do
            # equality comparison for NA values (NAs will propagate and we will get
            # 'TypeError: boolean value of NA is ambiguous')
            for variable_value in variable_values:
                self.assertTrue(pd.isna(variable_value))
        elif set_objective and not isinstance(
            objective_expression(model.get_variables()),
            (int, float),
        ):
            # The variable values are only well-defined when the objective is set
            # and depends on the variable(s).
            if not solver["is_integer"]:
                self.assertSequenceAlmostEqual(
                    variable_values, [variable_bound] * len(variable_values)
                )
            elif is_maximize:
                self.assertTrue(solver["is_integer"])  # Assert a known assumption.
                self.assertSequenceAlmostEqual(
                    variable_values,
                    [math.floor(variable_bound)] * len(variable_values),
                )
            else:
                self.assertTrue(solver["is_integer"])  # Assert a known assumption.
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
        is_maximize=_objective_senses,
        objective_expression=_objective_expressions,
    )
    def test_get_objective_value(
        self,
        solver: Dict[str, Union[str, Mapping[str, Any], bool]],
        variable_indices: pd.Index,
        variable_bound: float,
        solve_status: mb.SolveStatus,
        set_objective: bool,
        is_maximize: bool,
        objective_expression: Callable[[pd.Series], float],
    ):
        model = self._create_model(
            variable_indices=variable_indices,
            variable_bound=variable_bound,
            is_integer=solver["is_integer"],
            solve_status=solve_status,
            set_objective=set_objective,
            is_maximize=is_maximize,
            objective_expression=objective_expression,
        )
        model_solver = mb.Solver(solver["name"])
        if not model_solver.solver_is_supported():
            print(f'Solver {solver["name"]} is not supported')
            return
        if solver.get("solver_specific_parameters"):
            model_solver.set_solver_specific_parameters(
                solver.get("solver_specific_parameters")
            )
        got_status = model_solver.solve(model)

        # Test objective value
        if got_status not in (mb.SolveStatus.OPTIMAL, mb.SolveStatus.FEASIBLE):
            self.assertTrue(pd.isna(model_solver.objective_value))
            return
        if set_objective:
            variable_values = model_solver.values(model.get_variables())
            self.assertAlmostEqual(
                model_solver.objective_value,
                objective_expression(variable_values),
            )
        else:
            self.assertAlmostEqual(model_solver.objective_value, 0)


class ModelBuilderExamplesTest(absltest.TestCase):
    def test_simple_problem(self):
        # max 5x1 + 4x2 + 3x3
        # s.t 2x1 + 3x2 +  x3 <= 5
        #     4x1 +  x2 + 2x3 <= 11
        #     3x1 + 4x2 + 2x3 <= 8
        #      x1,   x2,   x3 >= 0
        # Values = (2,0,1)
        # Reduced Costs = (0,-3,0)
        model = mb.Model()
        x = model.new_var_series(
            "x", pd.Index(range(3)), lower_bounds=0, is_integral=True
        )
        self.assertLen(model.get_variables(), 3)
        model.maximize(x.dot([5, 4, 3]))
        model.add(x.dot([2, 3, 1]) <= 5)
        model.add(x.dot([4, 1, 2]) <= 11)
        model.add(x.dot([3, 4, 2]) <= 8)
        self.assertLen(model.get_linear_constraints(), 3)
        solver = mb.Solver("glop")
        test_red_cost = solver.reduced_costs(model.get_variables())
        test_dual_values = solver.dual_values(model.get_variables())
        self.assertLen(test_red_cost, 3)
        self.assertLen(test_dual_values, 3)
        for reduced_cost in test_red_cost:
            self.assertTrue(pd.isna(reduced_cost))
        for dual_value in test_dual_values:
            self.assertTrue(pd.isna(dual_value))
        run = solver.solve(model)
        self.assertEqual(run, mb.SolveStatus.OPTIMAL)
        i = solver.values(model.get_variables())
        self.assertSequenceAlmostEqual(i, [2, 0, 1])
        red_cost = solver.reduced_costs(model.get_variables())
        dual_val = solver.dual_values(model.get_linear_constraints())
        self.assertSequenceAlmostEqual(red_cost, [0, -3, 0])
        self.assertSequenceAlmostEqual(dual_val, [1, 0, 1])
        self.assertAlmostEqual(2, solver.value(x[0]))
        self.assertAlmostEqual(0, solver.reduced_cost((x[0])))
        self.assertAlmostEqual(-3, solver.reduced_cost((x[1])))
        self.assertAlmostEqual(0, solver.reduced_cost((x[2])))
        self.assertAlmostEqual(1, solver.dual_value((x[0])))
        self.assertAlmostEqual(0, solver.dual_value((x[1])))
        self.assertAlmostEqual(1, solver.dual_value((x[2])))

    def test_graph_k_color(self):
        # Assign a color to each vertex of graph, s.t that no two adjacent
        # vertices share the same color. Assume N vertices, and max number of
        # colors is K
        # Consider graph with edges:
        # Edge 1: (0,1)
        # Edge 2: (0,2)
        # Edge 3: (1,3)
        # Trying to color graph with at most 3 colors (0, 1, 2)
        # Two sets of variables:
        # x - pandas series representing coloring status of nodes
        # y - pandas series indicating whether color has been used
        # Min: y0 + y1 + y2
        # s.t: Every vertex must be assigned exactly one color
        #      if two vertices are adjacent they cannot have the same color

        model = mb.Model()
        num_colors = 3
        num_nodes = 4
        x = model.new_var_series(
            "x",
            pd.MultiIndex.from_product(
                (range(num_nodes), range(num_colors)),
                names=["node", "color"],
            ),
            lower_bounds=0,
            upper_bounds=1,
            is_integral=True,
        )
        y = model.new_var_series(
            "y",
            pd.Index(range(num_colors), name="color"),
            lower_bounds=0,
            upper_bounds=1,
            is_integral=True,
        )
        model.minimize(y.dot([1, 1, 1]))
        # Every vertex must be assigned exactly one color
        model.add(x.groupby("node").sum().apply(lambda expr: expr == 1))
        # If a vertex i is assigned to a color j then color j is used:
        # namely, we re-arrange the terms to express: "x - y <= 0".
        model.add(x.sub(y, fill_value=0).apply(lambda expr: expr <= 0))
        # if two vertices are adjacent they cannot have the same color j
        for j in range(num_colors):
            model.add(x[0, j] + x[1, j] <= 1)
            model.add(x[0, j] + x[2, j] <= 1)
            model.add(x[1, j] + x[3, j] <= 1)
        solver = mb.Solver("sat")
        run = solver.solve(model)
        self.assertEqual(run, mb.SolveStatus.OPTIMAL)
        self.assertEqual(solver.objective_value, 2)

    def test_knapsack_problem(self):
        # Basic Knapsack Problem: Given N items,
        # with N different weights and values, find the maximum possible value of
        # the Items while meeting a weight requirement
        # We have 3 items:
        # Item x1: Weight = 10, Value = 60
        # Item x2: Weight = 20, Value = 100
        # Item x3: Weight = 30, Value = 120
        # Max: 60x1 + 100x2 + 120x3
        # s.t: 10x1 +  20x2 +  30x3  <= 50
        model = mb.Model()
        x = model.new_bool_var_series("x", pd.Index(range(3)))
        self.assertLen(model.get_variables(), 3)
        model.maximize(x.dot([60, 100, 120]))
        model.add(x.dot([10, 20, 30]) <= 50)
        self.assertLen(model.get_linear_constraints(), 1)
        solver = mb.Solver("sat")
        run = solver.solve(model)
        self.assertEqual(run, mb.SolveStatus.OPTIMAL)
        i = solver.values(model.get_variables())
        self.assertEqual(solver.objective_value, 220)
        self.assertSequenceAlmostEqual(i, [0, 1, 1])

    def test_max_flow_problem(self):
        # Testing max flow problem with 8 nodes
        # 0-8, source 0, target 8
        # Edges:
        # (0,1), (0,2), (0,3), (1,4), (2,4),(2,5), (2,6), (3,5), (4,7), (5,7), (6,7)
        # Variables: flow_var- pandas series of flows, indexed by edges
        # Max: flow[(0,1)] + flow[(0,2)] + flow[(0,3)]
        # S.t: flow[(0,1)] <= 3
        #      flow[(0,2)] <= 2
        #      flow[(0,3)] <= 2
        #      flow[(1,4)] <= 5
        #      flow[(1,5)] <= 1
        #      flow[(2,4)] <= 1
        #      flow[(2,5)] <= 3
        #      flow[(2,6)] <= 1
        #      flow[(3,5)] <= 1
        #      flow[(4,7)] <= 4
        #      flow[(5,7)] <= 2
        #      flow[(6,7)] <= 4
        # Flow conservation constraints:
        #      flow[(0,1)] = flow[(1,4)] + flow[(1,5)]
        #      flow[(0,2)] = flow[(2,4)] + flow[(2,5)] + flow[(2,6)]
        #      flow[(0,3)] = flow[(3,5)]
        #      flow[(1,4)] + flow[(2,4)] = flow[(4,7)]
        #      flow[(1,5)] + flow[(2,5)] + flow[(3,5)] = X[(5,7)]
        #      flow[(2,6)] = flow[(6,7)]

        model = mb.Model()
        nodes = [1, 2, 3, 4, 5, 6]
        edge_capacities = pd.Series(
            {
                (0, 1): 3,
                (0, 2): 2,
                (0, 3): 2,
                (1, 4): 5,
                (1, 5): 1,
                (2, 4): 1,
                (2, 5): 3,
                (2, 6): 1,
                (3, 5): 1,
                (4, 7): 4,
                (5, 7): 2,
                (6, 7): 4,
            }
        )
        flow_var = model.new_var_series(
            "flow_var",
            pd.MultiIndex.from_tuples(
                edge_capacities.index, names=("source", "target")
            ),
            lower_bounds=0,
            is_integral=True,
        )
        self.assertLen(model.get_variables(), 12)
        model.maximize(flow_var[0, :].sum())
        model.add(
            (flow_var - edge_capacities).apply(lambda expr: expr <= 0),
            name="capacity_constraint",
        )
        for node in nodes:
            # must specify constraint name when directly comparing two variables
            model.add(
                flow_var.xs(node, level=0).sum() == flow_var.xs(node, level=1).sum(),
                name="flow_conservation",
            )
        solver = mb.Solver("sat")
        run = solver.solve(model)
        self.assertEqual(run, mb.SolveStatus.OPTIMAL)
        self.assertEqual(solver.objective_value, 6)

    def test_add_enforced(self):
        model = mb.Model()
        x = model.new_int_var(0, 10, "x")
        y = model.new_int_var(0, 10, "y")
        z = model.new_bool_var("z")
        ct = model.add_enforced(x + 2 * y >= 10, z, False)
        self.assertEqual(ct.lower_bound, 10.0)
        self.assertEqual(z.index, ct.indicator_variable.index)
        self.assertFalse(ct.indicator_value)


if __name__ == "__main__":
    absltest.main()
