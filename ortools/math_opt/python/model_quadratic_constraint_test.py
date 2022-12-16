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

"""Tests for the functions on Model involving quadratic constraints."""

import math
from typing import Dict, Tuple

from absl.testing import absltest
from ortools.math_opt import model_pb2
from ortools.math_opt import model_update_pb2
from ortools.math_opt import sparse_containers_pb2
from ortools.math_opt.python import model
from ortools.math_opt.python import quadratic_constraints
from ortools.math_opt.python import variables
from ortools.math_opt.python.testing import compare_proto


def _lin_terms_dict(
    quad_con: quadratic_constraints.QuadraticConstraint,
) -> Dict[variables.Variable, float]:
    return {term.variable: term.coefficient for term in quad_con.linear_terms()}


def _quad_terms_dict(
    quad_con: quadratic_constraints.QuadraticConstraint,
) -> Dict[Tuple[variables.Variable, variables.Variable], float]:
    return {
        (term.key.first_var, term.key.second_var): term.coefficient
        for term in quad_con.quadratic_terms()
    }


class ModelQuadraticConstraintTest(absltest.TestCase):

    def test_add_quadratic_constraint_expr_with_offset(self):
        mod = model.Model()
        x = mod.add_variable()
        c = mod.add_quadratic_constraint(
            lb=-3.0, ub=3.0, expr=2.0 * x * x + 1.0, name="cde"
        )
        self.assertEqual(c.name, "cde")
        self.assertAlmostEqual(c.lower_bound, -4.0, delta=1e-10)
        self.assertAlmostEqual(c.upper_bound, 2.0, delta=1e-10)
        self.assertEmpty(list(c.linear_terms()))
        self.assertDictEqual(_quad_terms_dict(c), {(x, x): 2.0})

    def test_add_quadratic_constraint_expr_with_offset_unbounded(self):
        mod = model.Model()
        x = mod.add_variable()
        c = mod.add_quadratic_constraint(expr=2.0 * x * x + 1.0)
        self.assertEqual(c.lower_bound, -math.inf)
        self.assertEqual(c.upper_bound, math.inf)
        self.assertEmpty(list(c.linear_terms()))
        self.assertDictEqual(_quad_terms_dict(c), {(x, x): 2.0})

    def test_add_quadratic_constraint_upper_bounded_expr(self):
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        c = mod.add_quadratic_constraint(
            2.0 * x * x + 3.0 * y + 4.0 <= 10.0, name="cde"
        )
        self.assertEqual(c.name, "cde")
        self.assertEqual(c.lower_bound, -math.inf)
        self.assertAlmostEqual(c.upper_bound, 6.0, delta=1e-10)
        self.assertDictEqual(_lin_terms_dict(c), {y: 3.0})
        self.assertDictEqual(_quad_terms_dict(c), {(x, x): 2.0})

    def test_add_quadratic_constraint_lower_bounded_expr(self):
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        c = mod.add_quadratic_constraint(-10.0 <= 2.0 * x * x + 3.0 * y + 4.0)
        self.assertAlmostEqual(c.lower_bound, -14.0, delta=1e-10)
        self.assertEqual(c.upper_bound, math.inf)
        self.assertDictEqual(_lin_terms_dict(c), {y: 3.0})
        self.assertDictEqual(_quad_terms_dict(c), {(x, x): 2.0})

    def test_add_quadratic_constraint_bounded_expr(self):
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        c = mod.add_quadratic_constraint((-10.0 <= 2.0 * x * x + 3.0 * y + 4.0) <= 10.0)
        self.assertAlmostEqual(c.lower_bound, -14.0, delta=1e-10)
        self.assertAlmostEqual(c.upper_bound, 6.0, delta=1e-10)
        self.assertDictEqual(_lin_terms_dict(c), {y: 3.0})
        self.assertDictEqual(_quad_terms_dict(c), {(x, x): 2.0})

    def test_add_quadratic_no_variables_error(self):
        mod = model.Model()
        with self.assertRaisesRegex(TypeError, "constant left-hand-side"):
            mod.add_quadratic_constraint(-10.0 <= 12.0)

    def test_add_quadratic_bad_double_inequality(self):
        mod = model.Model()
        x = mod.add_variable()
        with self.assertRaisesRegex(TypeError, "two-sided or ranged"):
            mod.add_quadratic_constraint(1.0 <= x * x <= 2.0)

    def test_all_linear_terms(self):
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        z = mod.add_variable()
        c = mod.add_quadratic_constraint(expr=2.0 * x + 3.0 * y)
        mod.add_quadratic_constraint(expr=x * x)
        e = mod.add_quadratic_constraint(expr=4.0 * x + 5.0 * z)
        self.assertCountEqual(
            list(mod.quadratic_constraint_linear_nonzeros()),
            [(c, x, 2.0), (c, y, 3.0), (e, x, 4.0), (e, z, 5.0)],
        )

    def test_all_quadratic_terms(self):
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()
        z = mod.add_variable()
        c = mod.add_quadratic_constraint(expr=2.0 * x * x + 3.0 * y * x)
        mod.add_quadratic_constraint(expr=x)
        e = mod.add_quadratic_constraint(expr=4.0 * x * x + 5.0 * y * z)
        self.assertCountEqual(
            list(mod.quadratic_constraint_quadratic_nonzeros()),
            [(c, x, x, 2.0), (c, x, y, 3.0), (e, x, x, 4.0), (e, y, z, 5.0)],
        )

    def test_quadratic_terms_empty(self):
        mod = model.Model()
        self.assertEmpty(list(mod.quadratic_constraint_linear_nonzeros()))
        self.assertEmpty(list(mod.quadratic_constraint_quadratic_nonzeros()))


_PROTO_VARS = model_pb2.VariablesProto(
    ids=[0],
    lower_bounds=[-2.0],
    upper_bounds=[2.0],
    integers=[False],
    names=[""],
)

# -3 <= 5 x^2 + 6 x <= 4
_QUAD_CON = model_pb2.QuadraticConstraintProto(
    name="q1",
    lower_bound=-3.0,
    upper_bound=4.0,
    linear_terms=sparse_containers_pb2.SparseDoubleVectorProto(ids=[0], values=[6.0]),
    quadratic_terms=sparse_containers_pb2.SparseDoubleMatrixProto(
        row_ids=[0], column_ids=[0], coefficients=[5.0]
    ),
)


class ModelQuadraticConstraintExportProtoIntegrationTest(
    compare_proto.MathOptProtoAssertions, absltest.TestCase
):
    """Test ModelProto and ModelUpdateProto export for quadratic constraints.

    These tests are not comprehensive, the proto generation code is completely
    tested in the tests for Elemental. We just want to make sure everything is
    connected.
    """

    def test_export_model(self) -> None:
        mod = model.Model()
        x = mod.add_variable(lb=-2.0, ub=2.0)
        mod.add_quadratic_constraint(
            lb=-3.0, ub=4.0, expr=5.0 * x * x + 6.0 * x, name="q1"
        )
        expected = model_pb2.ModelProto(
            variables=_PROTO_VARS, quadratic_constraints={0: _QUAD_CON}
        )
        self.assert_protos_equal(mod.export_model(), expected)

    def test_export_model_update_add_constraint(self) -> None:
        mod = model.Model()
        x = mod.add_variable(lb=-2.0, ub=2.0)
        tracker = mod.add_update_tracker()
        mod.add_quadratic_constraint(
            lb=-3.0, ub=4.0, expr=5.0 * x * x + 6.0 * x, name="q1"
        )

        expected = model_update_pb2.ModelUpdateProto(
            quadratic_constraint_updates=model_update_pb2.QuadraticConstraintUpdatesProto(
                new_constraints={0: _QUAD_CON}
            )
        )

        self.assert_protos_equal(tracker.export_update(), expected)

    def test_export_model_update_delete_constraint(self) -> None:
        mod = model.Model()
        q = mod.add_quadratic_constraint()
        tracker = mod.add_update_tracker()
        mod.delete_quadratic_constraint(q)

        expected = model_update_pb2.ModelUpdateProto(
            quadratic_constraint_updates=model_update_pb2.QuadraticConstraintUpdatesProto(
                deleted_constraint_ids=[0]
            )
        )

        self.assert_protos_equal(tracker.export_update(), expected)


if __name__ == "__main__":
    absltest.main()
