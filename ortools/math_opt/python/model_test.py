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
from typing import Optional
from absl.testing import absltest
from absl.testing import parameterized
from ortools.math_opt import model_pb2
from ortools.math_opt import model_update_pb2
from ortools.math_opt import sparse_containers_pb2
from ortools.math_opt.python import linear_constraints
from ortools.math_opt.python import model
from ortools.math_opt.python import variables
from ortools.math_opt.python.testing import compare_proto


class ModelTest(compare_proto.MathOptProtoAssertions, parameterized.TestCase):

    def test_name(self) -> None:
        mod = model.Model(name="test_model")
        self.assertEqual("test_model", mod.name)

    def test_name_empty(self) -> None:
        mod = model.Model()
        self.assertEqual("", mod.name)

    def test_add_and_read_variables(self) -> None:
        mod = model.Model(name="test_model")
        v1 = mod.add_variable(lb=-1.0, ub=2.5, is_integer=True, name="x")
        v2 = mod.add_variable()
        self.assertEqual(-1.0, v1.lower_bound)
        self.assertEqual(2.5, v1.upper_bound)
        self.assertTrue(v1.integer)
        self.assertEqual("x", v1.name)
        self.assertEqual(0, v1.id)
        self.assertEqual("x", str(v1))
        self.assertEqual("<Variable id: 0, name: 'x'>", repr(v1))

        self.assertEqual(-math.inf, v2.lower_bound)
        self.assertEqual(math.inf, v2.upper_bound)
        self.assertFalse(v2.integer)
        self.assertEqual("", v2.name)
        self.assertEqual(1, v2.id)
        self.assertEqual("variable_1", str(v2))
        self.assertEqual("<Variable id: 1, name: ''>", repr(v2))

        self.assertListEqual([v1, v2], list(mod.variables()))
        self.assertEqual(v1, mod.get_variable(0))
        self.assertEqual(v2, mod.get_variable(1))

    def test_add_integer_variable(self) -> None:
        mod = model.Model(
            name="test_model",
        )
        v1 = mod.add_integer_variable(lb=-1.0, ub=2.5, name="x")
        self.assertEqual(-1.0, v1.lower_bound)
        self.assertEqual(2.5, v1.upper_bound)
        self.assertTrue(v1.integer)
        self.assertEqual("x", v1.name)
        self.assertEqual(0, v1.id)

    def test_add_binary_variable(self) -> None:
        mod = model.Model(name="test_model")
        v1 = mod.add_binary_variable(name="x")
        self.assertEqual(0.0, v1.lower_bound)
        self.assertEqual(1.0, v1.upper_bound)
        self.assertTrue(v1.integer)
        self.assertEqual("x", v1.name)
        self.assertEqual(0, v1.id)

    def test_update_variable(self) -> None:
        mod = model.Model(name="test_model")
        v1 = mod.add_variable(lb=-1.0, ub=2.5, is_integer=True, name="x")
        v1.lower_bound = -math.inf
        v1.upper_bound = -3.0
        v1.integer = False
        self.assertEqual(-math.inf, v1.lower_bound)
        self.assertEqual(-3.0, v1.upper_bound)
        self.assertFalse(v1.integer)

    def test_read_deleted_variable(self) -> None:
        mod = model.Model(name="test_model")
        x = mod.add_binary_variable(name="x")
        mod.delete_variable(x)
        with self.assertRaises(ValueError):
            x.lower_bound  # pylint: disable=pointless-statement

    def test_update_deleted_variable(self) -> None:
        mod = model.Model(name="test_model")
        x = mod.add_binary_variable(name="x")
        mod.delete_variable(x)
        with self.assertRaises(ValueError):
            x.upper_bound = 2.0

    def test_add_and_read_linear_constraints(self) -> None:
        mod = model.Model(name="test_model")
        c = mod.add_linear_constraint(lb=-1.0, ub=2.5, name="c")
        d = mod.add_linear_constraint()
        self.assertEqual(-1.0, c.lower_bound)
        self.assertEqual(2.5, c.upper_bound)
        self.assertEqual("c", c.name)
        self.assertEqual(0, c.id)
        self.assertEqual("c", str(c))
        self.assertEqual("<LinearConstraint id: 0, name: 'c'>", repr(c))

        self.assertEqual(-math.inf, d.lower_bound)
        self.assertEqual(math.inf, d.upper_bound)
        self.assertEqual("", d.name)
        self.assertEqual(1, d.id)
        self.assertEqual("linear_constraint_1", str(d))
        self.assertEqual("<LinearConstraint id: 1, name: ''>", repr(d))

        self.assertListEqual([c, d], list(mod.linear_constraints()))
        self.assertEqual(c, mod.get_linear_constraint(0))
        self.assertEqual(d, mod.get_linear_constraint(1))

    def test_linear_constraint_as_bounded_expression(self) -> None:
        mod = model.Model(name="test_model")
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        c = mod.add_linear_constraint(lb=-1.0, ub=2.5, name="c", expr=3 * x - 2 * y)
        bounded_expr = c.as_bounded_linear_expression()
        self.assertEqual(bounded_expr.lower_bound, -1.0)
        self.assertEqual(bounded_expr.upper_bound, 2.5)
        expr = variables.as_flat_linear_expression(bounded_expr.expression)
        self.assertEqual(expr.offset, 0.0)
        self.assertDictEqual(dict(expr.terms), {x: 3.0, y: -2.0})

    def test_update_linear_constraint(self) -> None:
        mod = model.Model(name="test_model")
        c = mod.add_linear_constraint(lb=-1.0, ub=2.5, name="c")
        c.lower_bound = -math.inf
        c.upper_bound = -3.0
        self.assertEqual(-math.inf, c.lower_bound)
        self.assertEqual(-3.0, c.upper_bound)

    def test_read_deleted_linear_constraint(self) -> None:
        mod = model.Model(name="test_model")
        c = mod.add_linear_constraint(lb=-1.0, ub=2.5, name="c")
        mod.delete_linear_constraint(c)
        with self.assertRaises(ValueError):
            c.name  # pylint: disable=pointless-statement

    def test_update_deleted_linear_constraint(self) -> None:
        mod = model.Model(name="test_model")
        c = mod.add_linear_constraint(lb=-1.0, ub=2.5, name="c")
        mod.delete_linear_constraint(c)
        with self.assertRaises(ValueError):
            c.lower_bound = -12.0

    def test_linear_constraint_matrix(self) -> None:
        mod = model.Model(name="test_model")
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")
        c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
        d = mod.add_linear_constraint(ub=1.0, name="d")
        c.set_coefficient(x, 1.0)
        c.set_coefficient(y, 0.0)
        d.set_coefficient(x, 2.0)
        d.set_coefficient(z, -1.0)
        self.assertEqual(1.0, c.get_coefficient(x))
        self.assertEqual(0.0, c.get_coefficient(y))
        self.assertEqual(0.0, c.get_coefficient(z))
        self.assertEqual(2.0, d.get_coefficient(x))
        self.assertEqual(0.0, d.get_coefficient(y))
        self.assertEqual(-1.0, d.get_coefficient(z))

        self.assertEqual(c.name, "c")
        self.assertEqual(d.name, "d")

        self.assertCountEqual([c, d], mod.column_nonzeros(x))
        self.assertCountEqual([], mod.column_nonzeros(y))
        self.assertCountEqual([d], mod.column_nonzeros(z))

        self.assertCountEqual([x], mod.row_nonzeros(c))
        self.assertCountEqual([x, z], mod.row_nonzeros(d))

        self.assertCountEqual(
            [repr(variables.LinearTerm(variable=x, coefficient=1.0))],
            [repr(term) for term in c.terms()],
        )
        self.assertCountEqual(
            [
                repr(variables.LinearTerm(variable=x, coefficient=2.0)),
                repr(variables.LinearTerm(variable=z, coefficient=-1.0)),
            ],
            [repr(term) for term in d.terms()],
        )

        self.assertCountEqual(
            [
                linear_constraints.LinearConstraintMatrixEntry(
                    linear_constraint=c, variable=x, coefficient=1.0
                ),
                linear_constraints.LinearConstraintMatrixEntry(
                    linear_constraint=d, variable=x, coefficient=2.0
                ),
                linear_constraints.LinearConstraintMatrixEntry(
                    linear_constraint=d, variable=z, coefficient=-1.0
                ),
            ],
            list(mod.linear_constraint_matrix_entries()),
        )

    def test_linear_constraint_expression(self) -> None:
        mod = model.Model(name="test_model")
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")
        c = mod.add_linear_constraint(lb=0.0, expr=x + 1.0, ub=1.0, name="c")
        self.assertEqual(1.0, c.get_coefficient(x))
        self.assertEqual(0.0, c.get_coefficient(y))
        self.assertEqual(0.0, c.get_coefficient(z))
        self.assertEqual(-1.0, c.lower_bound)
        self.assertEqual(0.0, c.upper_bound)

        d = mod.add_linear_constraint(ub=1.0, expr=2 * x - z, name="d")
        self.assertEqual(2.0, d.get_coefficient(x))
        self.assertEqual(0.0, d.get_coefficient(y))
        self.assertEqual(-1.0, d.get_coefficient(z))
        self.assertEqual(-math.inf, d.lower_bound)
        self.assertEqual(1.0, d.upper_bound)

        e = mod.add_linear_constraint(lb=0.0)
        self.assertEqual(0.0, e.get_coefficient(x))
        self.assertEqual(0.0, e.get_coefficient(y))
        self.assertEqual(0.0, e.get_coefficient(z))
        self.assertEqual(0.0, e.lower_bound)
        self.assertEqual(math.inf, e.upper_bound)

        f = mod.add_linear_constraint(expr=1, ub=2)
        self.assertEqual(0.0, f.get_coefficient(x))
        self.assertEqual(0.0, f.get_coefficient(y))
        self.assertEqual(0.0, f.get_coefficient(z))
        self.assertEqual(-math.inf, f.lower_bound)
        self.assertEqual(1, f.upper_bound)

    def test_linear_constraint_bounded_expression(self) -> None:
        mod = model.Model(name="test_model")
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")

        c = mod.add_linear_constraint((0.0 <= x + 1.0) <= 1.0, name="c")
        self.assertEqual(1.0, c.get_coefficient(x))
        self.assertEqual(0.0, c.get_coefficient(y))
        self.assertEqual(0.0, c.get_coefficient(z))
        self.assertEqual(-1.0, c.lower_bound)
        self.assertEqual(0.0, c.upper_bound)

    def test_linear_constraint_upper_bounded_expression(self) -> None:
        mod = model.Model(name="test_model")
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")

        d = mod.add_linear_constraint(2 * x - z + 2.0 <= 1.0, name="d")
        self.assertEqual(2.0, d.get_coefficient(x))
        self.assertEqual(0.0, d.get_coefficient(y))
        self.assertEqual(-1.0, d.get_coefficient(z))
        self.assertEqual(-math.inf, d.lower_bound)
        self.assertEqual(-1.0, d.upper_bound)

    def test_linear_constraint_lower_bounded_expression(self) -> None:
        mod = model.Model(name="test_model")
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")

        e = mod.add_linear_constraint(1.0 <= x + y + 2.0, name="e")
        self.assertEqual(1.0, e.get_coefficient(x))
        self.assertEqual(1.0, e.get_coefficient(y))
        self.assertEqual(0.0, e.get_coefficient(z))
        self.assertEqual(-1.0, e.lower_bound)
        self.assertEqual(math.inf, e.upper_bound)

    def test_linear_constraint_number_eq_expression(self) -> None:
        mod = model.Model(name="test_model")
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")

        f = mod.add_linear_constraint(1.0 == x + y + 2.0, name="e")
        self.assertEqual(1.0, f.get_coefficient(x))
        self.assertEqual(1.0, f.get_coefficient(y))
        self.assertEqual(0.0, f.get_coefficient(z))
        self.assertEqual(-1.0, f.lower_bound)
        self.assertEqual(-1.0, f.upper_bound)

    def test_linear_constraint_expression_eq_expression(self) -> None:
        mod = model.Model(name="test_model")
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")

        f = mod.add_linear_constraint(1.0 - x == y + 2.0, name="e")
        self.assertEqual(-1.0, f.get_coefficient(x))
        self.assertEqual(-1.0, f.get_coefficient(y))
        self.assertEqual(0.0, f.get_coefficient(z))
        self.assertEqual(1.0, f.lower_bound)
        self.assertEqual(1.0, f.upper_bound)

    def test_linear_constraint_variable_eq_variable(self) -> None:
        mod = model.Model(name="test_model")
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")

        f = mod.add_linear_constraint(x == y, name="e")
        self.assertEqual(1.0, f.get_coefficient(x))
        self.assertEqual(-1.0, f.get_coefficient(y))
        self.assertEqual(0.0, f.get_coefficient(z))
        self.assertEqual(0.0, f.lower_bound)
        self.assertEqual(0.0, f.upper_bound)

    def test_linear_constraint_errors(self) -> None:
        mod = model.Model(name="test_model")
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")

        with self.assertRaisesRegex(
            TypeError,
            "Unsupported type for bounded_expr.*bool.*!= constraints.*constant" " left",
        ):
            mod.add_linear_constraint(x != y)

        with self.assertRaisesRegex(TypeError, "!= constraints.*"):
            mod.add_linear_constraint(x + y != y)

        with self.assertRaisesRegex(TypeError, "!= constraints.*"):
            mod.add_linear_constraint(x != x + y)

        with self.assertRaisesRegex(
            TypeError,
            "Unsupported type for bounded_expr.*bool.*!= constraints.*constant" " left",
        ):
            mod.add_linear_constraint(1 <= 2)  # pylint: disable=comparison-of-constants

        with self.assertRaisesRegex(
            TypeError,
            "Unsupported type for bounded_expr.*bool.*!= constraints.*constant" " left",
        ):
            mod.add_linear_constraint(1 <= 0)  # pylint: disable=comparison-of-constants

        with self.assertRaisesRegex(
            TypeError,
            "Unsupported type for bounded_expr.*bool.*!= constraints.*constant" " left",
        ):
            mod.add_linear_constraint(True)

        with self.assertRaisesRegex(
            TypeError,
            "__bool__ is unsupported.*\n.*two-sided or ranged linear inequality",
        ):
            mod.add_linear_constraint(x <= y <= z)

        with self.assertRaisesRegex(
            TypeError,
            "unsupported operand.*\n.*two or more non-constant linear expressions",
        ):
            mod.add_linear_constraint((x <= y) <= z)

        with self.assertRaisesRegex(
            TypeError,
            "unsupported operand.*\n.*two or more non-constant linear expressions",
        ):
            mod.add_linear_constraint(x <= (y <= z))

        with self.assertRaisesRegex(TypeError, "unsupported operand.*"):
            mod.add_linear_constraint((0 <= x) >= z)

        with self.assertRaisesRegex(AssertionError, "lb cannot be specified.*"):
            mod.add_linear_constraint(x + y == 1, lb=1)

        with self.assertRaisesRegex(AssertionError, "ub cannot be specified.*"):
            mod.add_linear_constraint(x + y == 1, ub=1)

        with self.assertRaisesRegex(AssertionError, "expr cannot be specified.*"):
            mod.add_linear_constraint(x + y == 1, expr=2 * x)

        with self.assertRaisesRegex(
            TypeError, "Unsupported type for expr argument.*str"
        ):
            mod.add_linear_constraint(expr="string")

        with self.assertRaisesRegex(ValueError, ".*infinite offset."):
            mod.add_linear_constraint(expr=math.inf, lb=0.0)

    def test_linear_constraint_matrix_with_variable_deletion(self) -> None:
        mod = model.Model(name="test_model")
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
        d = mod.add_linear_constraint(lb=0.0, ub=1.0, name="d")
        c.set_coefficient(x, 1.0)
        c.set_coefficient(y, 2.0)
        d.set_coefficient(x, 1.0)
        mod.delete_variable(x)
        self.assertCountEqual(
            [
                linear_constraints.LinearConstraintMatrixEntry(
                    linear_constraint=c, variable=y, coefficient=2.0
                )
            ],
            mod.linear_constraint_matrix_entries(),
        )
        self.assertCountEqual([c], mod.column_nonzeros(y))
        self.assertCountEqual(
            [repr(variables.LinearTerm(variable=y, coefficient=2.0))],
            [repr(term) for term in c.terms()],
        )
        self.assertCountEqual([], d.terms())
        with self.assertRaises(ValueError):
            c.get_coefficient(x)

    def test_linear_constraint_matrix_with_linear_constraint_deletion(
        self,
    ) -> None:
        mod = model.Model(name="test_model")
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
        d = mod.add_linear_constraint(lb=0.0, ub=1.0, name="d")
        c.set_coefficient(x, 1.0)
        c.set_coefficient(y, 2.0)
        d.set_coefficient(x, 1.0)
        mod.delete_linear_constraint(c)
        self.assertCountEqual(
            [
                linear_constraints.LinearConstraintMatrixEntry(
                    linear_constraint=d, variable=x, coefficient=1.0
                )
            ],
            list(mod.linear_constraint_matrix_entries()),
        )
        self.assertCountEqual([d], mod.column_nonzeros(x))
        self.assertCountEqual([], mod.column_nonzeros(y))
        self.assertCountEqual(
            [repr(variables.LinearTerm(variable=x, coefficient=1.0))],
            [repr(term) for term in d.terms()],
        )

    def test_linear_constraint_matrix_wrong_model(self) -> None:
        mod1 = model.Model(name="test_model1")
        x1 = mod1.add_binary_variable(name="x")
        mod2 = model.Model(name="test_model2")
        mod2.add_binary_variable(name="x")
        c2 = mod2.add_linear_constraint(lb=0.0, ub=1.0, name="c")
        with self.assertRaises(ValueError):
            c2.set_coefficient(x1, 1.0)

    @parameterized.named_parameters(
        {"testcase_name": "default", "remove_names": None},
        {"testcase_name": "with_names", "remove_names": False},
        {"testcase_name": "without_names", "remove_names": True},
    )
    def test_export(self, remove_names: Optional[bool]) -> None:
        """Test the export_model() function.

        Args:
          remove_names: Optional value for the remove_names parameters. When None,
            calls export_model() without the parameter to test the default value.
        """
        mod = model.Model(name="test_model")
        mod.objective.offset = 2.0
        mod.objective.is_maximize = True
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        c = mod.add_linear_constraint(lb=0.0, ub=2.0, name="c")
        c.set_coefficient(x, 1.0)
        c.set_coefficient(y, 2.0)
        mod.objective.set_linear_coefficient(y, 3.0)
        expected = model_pb2.ModelProto(
            name="test_model" if not remove_names else "",
            variables=model_pb2.VariablesProto(
                ids=[0, 1],
                lower_bounds=[0.0, 0.0],
                upper_bounds=[1.0, 1.0],
                integers=[True, True],
                names=["x", "y"] if not remove_names else [],
            ),
            linear_constraints=model_pb2.LinearConstraintsProto(
                ids=[0],
                lower_bounds=[0.0],
                upper_bounds=[2.0],
                names=["c"] if not remove_names else [],
            ),
            objective=model_pb2.ObjectiveProto(
                maximize=True,
                offset=2.0,
                linear_coefficients=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[1], values=[3.0]
                ),
            ),
            linear_constraint_matrix=sparse_containers_pb2.SparseDoubleMatrixProto(
                row_ids=[0, 0], column_ids=[0, 1], coefficients=[1.0, 2.0]
            ),
        )
        self.assert_protos_equiv(
            expected,
            (
                mod.export_model()
                if remove_names is None
                else mod.export_model(remove_names=remove_names)
            ),
        )

    def test_from_model_proto(self) -> None:
        model_proto = model_pb2.ModelProto(
            name="test_model",
            variables=model_pb2.VariablesProto(
                ids=[0, 1],
                lower_bounds=[0.0, 1.0],
                upper_bounds=[2.0, 3.0],
                integers=[True, False],
                names=["x", "y"],
            ),
            linear_constraints=model_pb2.LinearConstraintsProto(
                ids=[0],
                lower_bounds=[-1.0],
                upper_bounds=[2.0],
                names=["c"],
            ),
            objective=model_pb2.ObjectiveProto(
                maximize=True,
                offset=2.0,
                linear_coefficients=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[1], values=[3.0]
                ),
            ),
            linear_constraint_matrix=sparse_containers_pb2.SparseDoubleMatrixProto(
                row_ids=[0, 0], column_ids=[0, 1], coefficients=[1.0, 2.0]
            ),
        )
        mod = model.Model.from_model_proto(model_proto)
        self.assertEqual(mod.name, "test_model")
        self.assertEqual(mod.get_num_variables(), 2)
        x = mod.get_variable(0)
        y = mod.get_variable(1)
        self.assertEqual(x.name, "x")
        self.assertEqual(x.lower_bound, 0.0)
        self.assertEqual(x.upper_bound, 2.0)
        self.assertTrue(x.integer)
        self.assertEqual(y.name, "y")
        self.assertEqual(y.lower_bound, 1.0)
        self.assertEqual(y.upper_bound, 3.0)
        self.assertFalse(y.integer)
        self.assertEqual(mod.get_num_linear_constraints(), 1)
        c = mod.get_linear_constraint(0)
        self.assertEqual(c.name, "c")
        self.assertEqual(c.lower_bound, -1.0)
        self.assertEqual(c.upper_bound, 2.0)
        self.assertEqual(c.get_coefficient(x), 1.0)
        self.assertEqual(c.get_coefficient(y), 2.0)
        self.assertTrue(mod.objective.is_maximize)
        self.assertEqual(mod.objective.offset, 2.0)
        self.assertEqual(mod.objective.get_linear_coefficient(x), 0.0)
        self.assertEqual(mod.objective.get_linear_coefficient(y), 3.0)

    def test_update_tracker_simple(self) -> None:
        mod = model.Model(name="test_model")
        x = mod.add_binary_variable(name="x")
        t = mod.add_update_tracker()
        x.upper_bound = 2.0
        expected = model_update_pb2.ModelUpdateProto(
            variable_updates=model_update_pb2.VariableUpdatesProto(
                upper_bounds=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[0], values=[2.0]
                )
            )
        )
        self.assert_protos_equiv(expected, t.export_update())
        self.assert_protos_equiv(expected, t.export_update())
        t.advance_checkpoint()
        self.assertIsNone(t.export_update())

    def test_two_update_trackers(self) -> None:
        mod = model.Model(name="test_model")
        t1 = mod.add_update_tracker()
        x = mod.add_binary_variable(name="x")
        t2 = mod.add_update_tracker()
        x.upper_bound = 2.0
        expected1 = model_update_pb2.ModelUpdateProto(
            new_variables=model_pb2.VariablesProto(
                ids=[0],
                lower_bounds=[0.0],
                upper_bounds=[2.0],
                integers=[True],
                names=["x"],
            )
        )
        expected2 = model_update_pb2.ModelUpdateProto(
            variable_updates=model_update_pb2.VariableUpdatesProto(
                upper_bounds=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[0], values=[2.0]
                )
            )
        )
        self.assert_protos_equiv(expected1, t1.export_update())
        self.assert_protos_equiv(expected2, t2.export_update())

    def test_remove_tracker(self) -> None:
        mod = model.Model(name="test_model")
        x = mod.add_binary_variable(name="x")
        t1 = mod.add_update_tracker()
        t2 = mod.add_update_tracker()
        x.upper_bound = 2.0
        mod.remove_update_tracker(t1)
        x.lower_bound = -1.0
        expected = model_update_pb2.ModelUpdateProto(
            variable_updates=model_update_pb2.VariableUpdatesProto(
                upper_bounds=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[0], values=[2.0]
                ),
                lower_bounds=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[0], values=[-1.0]
                ),
            )
        )
        self.assert_protos_equiv(expected, t2.export_update())
        with self.assertRaises(ValueError):
            t1.export_update()
        with self.assertRaises(ValueError):
            t1.advance_checkpoint()
        with self.assertRaises(ValueError):
            mod.remove_update_tracker(t1)


class WrongAttributeTest(absltest.TestCase):
    """Test case that verifies that wrong attributes are detected.

    In some the tests below we have to disable pytype checks since it also detects
    the issue now that the code uses __slots__.
    """

    def test_variable(self) -> None:
        mod = model.Model(name="test_model")
        x = mod.add_variable()
        with self.assertRaises(AttributeError):
            x.loer_bnd = 4  # pytype: disable=not-writable

    def test_linear_constraint(self) -> None:
        mod = model.Model(name="test_model")
        c = mod.add_linear_constraint()
        with self.assertRaises(AttributeError):
            c.uper_bound = 8  # pytype: disable=not-writable

    def test_objective(self) -> None:
        mod = model.Model(name="test_model")
        with self.assertRaises(AttributeError):
            mod.objective.matimuze = True  # pytype: disable=not-writable

    def test_aux_objective(self) -> None:
        mod = model.Model(name="test_model")
        aux = mod.add_auxiliary_objective(priority=1)
        with self.assertRaises(AttributeError):
            aux.matimuze = True  # pytype: disable=not-writable

    def test_model(self) -> None:
        mod = model.Model(name="test_model")
        with self.assertRaises(AttributeError):
            mod.objectize = None  # pytype: disable=not-writable


if __name__ == "__main__":
    absltest.main()
