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

import math
from typing import Type

from absl.testing import absltest
from absl.testing import parameterized
from ortools.math_opt import model_pb2
from ortools.math_opt import model_update_pb2
from ortools.math_opt import sparse_containers_pb2
from ortools.math_opt.python import hash_model_storage
from ortools.math_opt.python import model
from ortools.math_opt.python import model_storage
from ortools.math_opt.python.testing import compare_proto

StorageClass = Type[model_storage.ModelStorage]
_MatEntry = model_storage.LinearConstraintMatrixIdEntry
_ObjEntry = model_storage.LinearObjectiveEntry


@parameterized.parameters((hash_model_storage.HashModelStorage,))
class ModelTest(compare_proto.MathOptProtoAssertions, parameterized.TestCase):

    def test_name(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        self.assertEqual("test_model", mod.name)

    def test_name_empty(self, storage_class: StorageClass) -> None:
        mod = model.Model(storage_class=storage_class)
        self.assertEqual("", mod.name)

    def test_add_and_read_variables(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
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

    def test_get_variable_does_not_exist_key_error(
        self, storage_class: StorageClass
    ) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        with self.assertRaisesRegex(KeyError, "does not exist.*3"):
            mod.get_variable(3)

    def test_add_integer_variable(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        v1 = mod.add_integer_variable(lb=-1.0, ub=2.5, name="x")
        self.assertEqual(-1.0, v1.lower_bound)
        self.assertEqual(2.5, v1.upper_bound)
        self.assertTrue(v1.integer)
        self.assertEqual("x", v1.name)
        self.assertEqual(0, v1.id)

    def test_add_binary_variable(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        v1 = mod.add_binary_variable(name="x")
        self.assertEqual(0.0, v1.lower_bound)
        self.assertEqual(1.0, v1.upper_bound)
        self.assertTrue(v1.integer)
        self.assertEqual("x", v1.name)
        self.assertEqual(0, v1.id)

    def test_update_variable(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        v1 = mod.add_variable(lb=-1.0, ub=2.5, is_integer=True, name="x")
        v1.lower_bound = -math.inf
        v1.upper_bound = -3.0
        v1.integer = False
        self.assertEqual(-math.inf, v1.lower_bound)
        self.assertEqual(-3.0, v1.upper_bound)
        self.assertFalse(v1.integer)

    def test_delete_variable(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")
        self.assertListEqual([x, y, z], list(mod.variables()))
        mod.delete_variable(y)
        self.assertListEqual([x, z], list(mod.variables()))

    def test_delete_variable_twice(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        mod.delete_variable(x)
        with self.assertRaises(LookupError):
            mod.delete_variable(x)

    def test_read_deleted_variable(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        mod.delete_variable(x)
        with self.assertRaises(LookupError):
            x.lower_bound  # pylint: disable=pointless-statement

    def test_update_deleted_variable(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        mod.delete_variable(x)
        with self.assertRaises(LookupError):
            x.upper_bound = 2.0

    def test_delete_variable_wrong_model(self, storage_class: StorageClass) -> None:
        mod1 = model.Model(name="mod1", storage_class=storage_class)
        mod1.add_binary_variable(name="x")
        mod2 = model.Model(name="mod2", storage_class=storage_class)
        x2 = mod2.add_binary_variable(name="x")
        with self.assertRaises(ValueError):
            mod1.delete_variable(x2)

    def test_add_and_read_linear_constraints(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
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

    def test_get_linear_constraint_does_not_exist_key_error(
        self, storage_class: StorageClass
    ) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        with self.assertRaisesRegex(KeyError, "does not exist.*3"):
            mod.get_linear_constraint(3)

    def test_update_linear_constraint(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        c = mod.add_linear_constraint(lb=-1.0, ub=2.5, name="c")
        c.lower_bound = -math.inf
        c.upper_bound = -3.0
        self.assertEqual(-math.inf, c.lower_bound)
        self.assertEqual(-3.0, c.upper_bound)

    def test_delete_linear_constraint(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        c = mod.add_linear_constraint(lb=-1.0, ub=2.5, name="c")
        d = mod.add_linear_constraint(lb=0.0, ub=1.0, name="d")
        e = mod.add_linear_constraint(lb=1.0, name="e")
        self.assertListEqual([c, d, e], list(mod.linear_constraints()))
        mod.delete_linear_constraint(d)
        self.assertListEqual([c, e], list(mod.linear_constraints()))

    def test_delete_linear_constraint_twice(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        c = mod.add_linear_constraint(lb=-1.0, ub=2.5, name="c")
        mod.delete_linear_constraint(c)
        with self.assertRaises(LookupError):
            mod.delete_linear_constraint(c)

    def test_read_deleted_linear_constraint(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        c = mod.add_linear_constraint(lb=-1.0, ub=2.5, name="c")
        mod.delete_linear_constraint(c)
        with self.assertRaises(LookupError):
            c.name  # pylint: disable=pointless-statement

    def test_update_deleted_linear_constraint(
        self, storage_class: StorageClass
    ) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        c = mod.add_linear_constraint(lb=-1.0, ub=2.5, name="c")
        mod.delete_linear_constraint(c)
        with self.assertRaises(LookupError):
            c.lower_bound = -12.0

    def test_delete_linear_constraint_wrong_model(
        self, storage_class: StorageClass
    ) -> None:
        mod1 = model.Model(name="test_model", storage_class=storage_class)
        mod1.add_linear_constraint(lb=-1.0, ub=2.5, name="c")
        mod2 = model.Model(name="mod2", storage_class=storage_class)
        c2 = mod2.add_linear_constraint(lb=-1.0, ub=2.5, name="c")
        with self.assertRaises(ValueError):
            mod1.delete_linear_constraint(c2)

    def test_set_objective_linear(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")
        w = mod.add_binary_variable(name="w")

        mod.set_objective(2 * (x - 2 * y) + 1 + 3 * z, is_maximize=True)
        self.assertEqual(2.0, mod.objective.get_linear_coefficient(x))
        self.assertEqual(-4.0, mod.objective.get_linear_coefficient(y))
        self.assertEqual(3.0, mod.objective.get_linear_coefficient(z))
        self.assertEqual(0.0, mod.objective.get_linear_coefficient(w))
        self.assertEqual(1.0, mod.objective.offset)
        self.assertTrue(mod.objective.is_maximize)

        mod.set_objective(w + 2, is_maximize=False)
        self.assertEqual(0.0, mod.objective.get_linear_coefficient(x))
        self.assertEqual(0.0, mod.objective.get_linear_coefficient(y))
        self.assertEqual(0.0, mod.objective.get_linear_coefficient(z))
        self.assertEqual(1.0, mod.objective.get_linear_coefficient(w))
        self.assertEqual(2.0, mod.objective.offset)
        self.assertFalse(mod.objective.is_maximize)

    def test_set_linear_objective(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")
        w = mod.add_binary_variable(name="w")

        mod.set_linear_objective(2 * (x - 2 * y) + 1 + 3 * z, is_maximize=True)
        self.assertEqual(2.0, mod.objective.get_linear_coefficient(x))
        self.assertEqual(-4.0, mod.objective.get_linear_coefficient(y))
        self.assertEqual(3.0, mod.objective.get_linear_coefficient(z))
        self.assertEqual(0.0, mod.objective.get_linear_coefficient(w))
        self.assertEqual(1.0, mod.objective.offset)
        self.assertTrue(mod.objective.is_maximize)

        mod.set_linear_objective(w + 2, is_maximize=False)
        self.assertEqual(0.0, mod.objective.get_linear_coefficient(x))
        self.assertEqual(0.0, mod.objective.get_linear_coefficient(y))
        self.assertEqual(0.0, mod.objective.get_linear_coefficient(z))
        self.assertEqual(1.0, mod.objective.get_linear_coefficient(w))
        self.assertEqual(2.0, mod.objective.offset)
        self.assertFalse(mod.objective.is_maximize)

    def test_set_objective_quadratic(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")

        mod.set_objective(2 * x * (x - 2 * y) + 1 + 3 * x, is_maximize=True)
        self.assertEqual(3.0, mod.objective.get_linear_coefficient(x))
        self.assertEqual(0.0, mod.objective.get_linear_coefficient(y))
        self.assertEqual(2.0, mod.objective.get_quadratic_coefficient(x, x))
        self.assertEqual(-4.0, mod.objective.get_quadratic_coefficient(x, y))
        self.assertEqual(0.0, mod.objective.get_quadratic_coefficient(y, y))
        self.assertEqual(1.0, mod.objective.offset)
        self.assertTrue(mod.objective.is_maximize)

        mod.set_objective(x * x + 2, is_maximize=False)
        self.assertEqual(0.0, mod.objective.get_linear_coefficient(x))
        self.assertEqual(0.0, mod.objective.get_linear_coefficient(y))
        self.assertEqual(1.0, mod.objective.get_quadratic_coefficient(x, x))
        self.assertEqual(0.0, mod.objective.get_quadratic_coefficient(x, y))
        self.assertEqual(0.0, mod.objective.get_quadratic_coefficient(y, y))
        self.assertEqual(2.0, mod.objective.offset)
        self.assertFalse(mod.objective.is_maximize)

    def test_set_quadratic_objective(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")

        mod.set_quadratic_objective(2 * x * (x - 2 * y) + 1 + 3 * x, is_maximize=True)
        self.assertEqual(3.0, mod.objective.get_linear_coefficient(x))
        self.assertEqual(0.0, mod.objective.get_linear_coefficient(y))
        self.assertEqual(2.0, mod.objective.get_quadratic_coefficient(x, x))
        self.assertEqual(-4.0, mod.objective.get_quadratic_coefficient(x, y))
        self.assertEqual(0.0, mod.objective.get_quadratic_coefficient(y, y))
        self.assertEqual(1.0, mod.objective.offset)
        self.assertTrue(mod.objective.is_maximize)

        mod.set_quadratic_objective(x * x + 2, is_maximize=False)
        self.assertEqual(0.0, mod.objective.get_linear_coefficient(x))
        self.assertEqual(0.0, mod.objective.get_linear_coefficient(y))
        self.assertEqual(1.0, mod.objective.get_quadratic_coefficient(x, x))
        self.assertEqual(0.0, mod.objective.get_quadratic_coefficient(x, y))
        self.assertEqual(0.0, mod.objective.get_quadratic_coefficient(y, y))
        self.assertEqual(2.0, mod.objective.offset)
        self.assertFalse(mod.objective.is_maximize)

    def test_maximize_expr_linear(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        mod.maximize(2 * x - y + 1)
        self.assertEqual(2.0, mod.objective.get_linear_coefficient(x))
        self.assertEqual(-1.0, mod.objective.get_linear_coefficient(y))
        self.assertEqual(1.0, mod.objective.offset)
        self.assertTrue(mod.objective.is_maximize)

        mod.objective.clear()
        mod.maximize_linear_objective(2 * x - y + 1)
        self.assertEqual(2.0, mod.objective.get_linear_coefficient(x))
        self.assertEqual(-1.0, mod.objective.get_linear_coefficient(y))
        self.assertEqual(1.0, mod.objective.offset)
        self.assertTrue(mod.objective.is_maximize)

    def test_maximize_expr_quadratic(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        mod.maximize(2 * x - y + 1 + x * x)
        self.assertEqual(2.0, mod.objective.get_linear_coefficient(x))
        self.assertEqual(-1.0, mod.objective.get_linear_coefficient(y))
        self.assertEqual(1.0, mod.objective.get_quadratic_coefficient(x, x))
        self.assertEqual(0.0, mod.objective.get_quadratic_coefficient(x, y))
        self.assertEqual(0.0, mod.objective.get_quadratic_coefficient(y, y))
        self.assertEqual(1.0, mod.objective.offset)
        self.assertTrue(mod.objective.is_maximize)

        mod.objective.clear()
        mod.maximize_quadratic_objective(2 * x - y + 1 + x * x)
        self.assertEqual(2.0, mod.objective.get_linear_coefficient(x))
        self.assertEqual(-1.0, mod.objective.get_linear_coefficient(y))
        self.assertEqual(1.0, mod.objective.get_quadratic_coefficient(x, x))
        self.assertEqual(0.0, mod.objective.get_quadratic_coefficient(x, y))
        self.assertEqual(0.0, mod.objective.get_quadratic_coefficient(y, y))
        self.assertEqual(1.0, mod.objective.offset)
        self.assertTrue(mod.objective.is_maximize)

    def test_minimize_expr_linear(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        mod.minimize(2 * x - y + 1)
        self.assertEqual(2.0, mod.objective.get_linear_coefficient(x))
        self.assertEqual(-1.0, mod.objective.get_linear_coefficient(y))
        self.assertEqual(1.0, mod.objective.offset)
        self.assertFalse(mod.objective.is_maximize)

        mod.objective.clear()
        mod.minimize_linear_objective(2 * x - y + 1)
        self.assertEqual(2.0, mod.objective.get_linear_coefficient(x))
        self.assertEqual(-1.0, mod.objective.get_linear_coefficient(y))
        self.assertEqual(1.0, mod.objective.offset)
        self.assertFalse(mod.objective.is_maximize)

    def test_minimize_expr_quadratic(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        mod.minimize(2 * x - y + 1 + x * x)
        self.assertEqual(2.0, mod.objective.get_linear_coefficient(x))
        self.assertEqual(-1.0, mod.objective.get_linear_coefficient(y))
        self.assertEqual(1.0, mod.objective.get_quadratic_coefficient(x, x))
        self.assertEqual(0.0, mod.objective.get_quadratic_coefficient(x, y))
        self.assertEqual(0.0, mod.objective.get_quadratic_coefficient(y, y))
        self.assertEqual(1.0, mod.objective.offset)
        self.assertFalse(mod.objective.is_maximize)

        mod.objective.clear()
        mod.minimize_quadratic_objective(2 * x - y + 1 + x * x)
        self.assertEqual(2.0, mod.objective.get_linear_coefficient(x))
        self.assertEqual(-1.0, mod.objective.get_linear_coefficient(y))
        self.assertEqual(1.0, mod.objective.get_quadratic_coefficient(x, x))
        self.assertEqual(0.0, mod.objective.get_quadratic_coefficient(x, y))
        self.assertEqual(0.0, mod.objective.get_quadratic_coefficient(y, y))
        self.assertEqual(1.0, mod.objective.offset)
        self.assertFalse(mod.objective.is_maximize)

    def test_add_to_objective_linear(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        mod.minimize(2 * x - y + 1)
        mod.objective.add(x - 3 * y - 2)
        self.assertEqual(3.0, mod.objective.get_linear_coefficient(x))
        self.assertEqual(-4.0, mod.objective.get_linear_coefficient(y))
        self.assertEqual(-1.0, mod.objective.offset)
        self.assertFalse(mod.objective.is_maximize)

        mod.minimize(2 * x - y + 1)
        mod.objective.add_linear(x - 3 * y - 2)
        self.assertEqual(3.0, mod.objective.get_linear_coefficient(x))
        self.assertEqual(-4.0, mod.objective.get_linear_coefficient(y))
        self.assertEqual(-1.0, mod.objective.offset)
        self.assertFalse(mod.objective.is_maximize)

    def test_add_to_objective_quadratic(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        mod.minimize(2 * x - y + 1 + x * x)
        mod.objective.add(x - 3 * y - 2 - 2 * x * x + x * y)
        self.assertEqual(3.0, mod.objective.get_linear_coefficient(x))
        self.assertEqual(-4.0, mod.objective.get_linear_coefficient(y))
        self.assertEqual(-1.0, mod.objective.get_quadratic_coefficient(x, x))
        self.assertEqual(1.0, mod.objective.get_quadratic_coefficient(x, y))
        self.assertEqual(0.0, mod.objective.get_quadratic_coefficient(y, y))
        self.assertEqual(-1.0, mod.objective.offset)
        self.assertFalse(mod.objective.is_maximize)

        mod.minimize(2 * x - y + 1 + x * x)
        mod.objective.add_quadratic(x - 3 * y - 2 - 2 * x * x + x * y)
        self.assertEqual(3.0, mod.objective.get_linear_coefficient(x))
        self.assertEqual(-4.0, mod.objective.get_linear_coefficient(y))
        self.assertEqual(-1.0, mod.objective.get_quadratic_coefficient(x, x))
        self.assertEqual(1.0, mod.objective.get_quadratic_coefficient(x, y))
        self.assertEqual(0.0, mod.objective.get_quadratic_coefficient(y, y))
        self.assertEqual(-1.0, mod.objective.offset)
        self.assertFalse(mod.objective.is_maximize)

    def test_add_to_objective_type_errors(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        with self.assertRaises(TypeError):
            mod.objective.add_linear(x * x)  # pytype: disable=wrong-arg-types
        with self.assertRaises(TypeError):
            mod.objective.add("obj")  # pytype: disable=wrong-arg-types
        with self.assertRaises(TypeError):
            mod.objective.add_quadratic("obj")  # pytype: disable=wrong-arg-types

    def test_clear_objective(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        mod.minimize(2 * x - y + 1 + x * x)
        mod.objective.clear()
        self.assertEqual(0.0, mod.objective.get_linear_coefficient(x))
        self.assertEqual(0.0, mod.objective.get_linear_coefficient(y))
        self.assertEqual(0.0, mod.objective.get_quadratic_coefficient(x, x))
        self.assertEqual(0.0, mod.objective.get_quadratic_coefficient(x, y))
        self.assertEqual(0.0, mod.objective.get_quadratic_coefficient(y, y))
        self.assertEqual(0.0, mod.objective.offset)
        self.assertFalse(mod.objective.is_maximize)

    def test_objective_offset(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        self.assertEqual(0.0, mod.objective.offset)
        mod.objective.offset = 4.4
        self.assertEqual(4.4, mod.objective.offset)

    def test_objective_direction(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        self.assertFalse(mod.objective.is_maximize)
        mod.objective.is_maximize = True
        self.assertTrue(mod.objective.is_maximize)
        mod.objective.is_maximize = False
        self.assertFalse(mod.objective.is_maximize)

    def test_objective_linear_terms(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        w = mod.add_binary_variable(name="w")
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")
        for v in (w, x, y, z):
            self.assertEqual(0.0, mod.objective.get_linear_coefficient(v))
        self.assertCountEqual([], mod.objective.linear_terms())
        mod.objective.set_linear_coefficient(x, 0.0)
        mod.objective.set_linear_coefficient(y, 1.0)
        mod.objective.set_linear_coefficient(z, 10.0)
        self.assertEqual(0.0, mod.objective.get_linear_coefficient(w))
        self.assertEqual(0.0, mod.objective.get_linear_coefficient(x))
        self.assertEqual(1.0, mod.objective.get_linear_coefficient(y))
        self.assertEqual(10.0, mod.objective.get_linear_coefficient(z))
        self.assertCountEqual(
            [
                repr(model.LinearTerm(variable=y, coefficient=1.0)),
                repr(model.LinearTerm(variable=z, coefficient=10.0)),
            ],
            [repr(term) for term in mod.objective.linear_terms()],
        )

        mod.objective.set_linear_coefficient(z, 0.0)
        self.assertEqual(0.0, mod.objective.get_linear_coefficient(z))
        self.assertCountEqual(
            [repr(model.LinearTerm(variable=y, coefficient=1.0))],
            [repr(term) for term in mod.objective.linear_terms()],
        )

    def test_objective_quadratic_terms(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        self.assertCountEqual([], mod.objective.quadratic_terms())
        mod.objective.set_linear_coefficient(x, 0.0)
        mod.objective.set_quadratic_coefficient(x, x, 1.0)
        mod.objective.set_quadratic_coefficient(x, y, 2.0)
        self.assertCountEqual(
            [
                repr(
                    model.QuadraticTerm(
                        key=model.QuadraticTermKey(x, x), coefficient=1.0
                    )
                ),
                repr(
                    model.QuadraticTerm(
                        key=model.QuadraticTermKey(x, y), coefficient=2.0
                    )
                ),
            ],
            [repr(term) for term in mod.objective.quadratic_terms()],
        )

        mod.objective.set_quadratic_coefficient(x, x, 0.0)
        self.assertCountEqual(
            [
                repr(
                    model.QuadraticTerm(
                        key=model.QuadraticTermKey(x, y), coefficient=2.0
                    )
                )
            ],
            [repr(term) for term in mod.objective.quadratic_terms()],
        )

        mod.objective.set_quadratic_coefficient(x, y, 0.0)
        self.assertEmpty(list(mod.objective.quadratic_terms()))

    def test_objective_as_expression_linear(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        mod.maximize(x + 2 * y - 1)
        linear_expr = mod.objective.as_linear_expression()
        quadratic_expr = mod.objective.as_quadratic_expression()
        self.assertEqual(-1, linear_expr.offset)
        self.assertEqual(-1, quadratic_expr.offset)
        self.assertDictEqual(dict(linear_expr.terms), {x: 1.0, y: 2.0})
        self.assertDictEqual(dict(quadratic_expr.linear_terms), {x: 1.0, y: 2.0})
        self.assertDictEqual(dict(quadratic_expr.quadratic_terms), {})

    def test_objective_as_expression_quadratic(
        self, storage_class: StorageClass
    ) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        mod.maximize(3 * x * y + 4 * x * x + x + 2 * y - 1)
        quadratic_expr = mod.objective.as_quadratic_expression()
        self.assertEqual(-1, quadratic_expr.offset)
        self.assertDictEqual(dict(quadratic_expr.linear_terms), {x: 1.0, y: 2.0})
        self.assertDictEqual(
            dict(quadratic_expr.quadratic_terms),
            {model.QuadraticTermKey(x, x): 4, model.QuadraticTermKey(x, y): 3},
        )
        with self.assertRaises(TypeError):
            mod.objective.as_linear_expression()

    def test_objective_with_variable_deletion_linear(
        self, storage_class: StorageClass
    ) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        mod.objective.set_linear_coefficient(x, 1.0)
        mod.objective.set_linear_coefficient(y, 2.0)
        mod.delete_variable(x)
        self.assertEqual(2.0, mod.objective.get_linear_coefficient(y))
        self.assertCountEqual(
            [repr(model.LinearTerm(variable=y, coefficient=2.0))],
            [repr(term) for term in mod.objective.linear_terms()],
        )
        with self.assertRaises(LookupError):
            mod.objective.get_linear_coefficient(x)

    def test_objective_with_variable_deletion_quadratic(
        self, storage_class: StorageClass
    ) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        mod.objective.set_quadratic_coefficient(x, x, 1.0)
        mod.objective.set_quadratic_coefficient(x, y, 2.0)
        mod.delete_variable(y)
        self.assertEqual(1.0, mod.objective.get_quadratic_coefficient(x, x))
        self.assertCountEqual(
            [
                repr(
                    model.QuadraticTerm(
                        key=model.QuadraticTermKey(x, x), coefficient=1.0
                    )
                )
            ],
            [repr(term) for term in mod.objective.quadratic_terms()],
        )
        with self.assertRaises(LookupError):
            mod.objective.get_quadratic_coefficient(x, y)
        with self.assertRaises(LookupError):
            mod.objective.get_quadratic_coefficient(y, x)

    def test_objective_wrong_model_linear(self, storage_class: StorageClass) -> None:
        mod1 = model.Model(name="test_model1", storage_class=storage_class)
        x = mod1.add_binary_variable(name="x")
        mod2 = model.Model(name="test_model2", storage_class=storage_class)
        mod2.add_binary_variable(name="x")
        with self.assertRaises(ValueError):
            mod2.objective.set_linear_coefficient(x, 1.0)

    def test_objective_wrong_model_quadratic(self, storage_class: StorageClass) -> None:
        mod1 = model.Model(name="test_model1", storage_class=storage_class)
        x = mod1.add_binary_variable(name="x")
        mod2 = model.Model(name="test_model2", storage_class=storage_class)
        other_x = mod2.add_binary_variable(name="x")
        with self.assertRaises(ValueError):
            mod2.objective.set_quadratic_coefficient(x, other_x, 1.0)
        with self.assertRaises(ValueError):
            mod2.objective.set_quadratic_coefficient(other_x, x, 1.0)

    def test_objective_type_errors(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        with self.assertRaises(TypeError):
            mod.set_linear_objective(
                x * x, is_maximize=True
            )  # pytype: disable=wrong-arg-types
        with self.assertRaises(TypeError):
            mod.maximize_linear_objective(x * x)  # pytype: disable=wrong-arg-types
        with self.assertRaises(TypeError):
            mod.minimize_linear_objective(x * x)  # pytype: disable=wrong-arg-types
        with self.assertRaises(TypeError):
            mod.set_quadratic_objective(
                "obj", is_maximize=True
            )  # pytype: disable=wrong-arg-types
        with self.assertRaises(TypeError):
            mod.maximize_quadratic_objective("obj")  # pytype: disable=wrong-arg-types
        with self.assertRaises(TypeError):
            mod.minimize_quadratic_objective("obj")  # pytype: disable=wrong-arg-types
        with self.assertRaises(TypeError):
            mod.set_objective(
                "obj", is_maximize=True
            )  # pytype: disable=wrong-arg-types
        with self.assertRaises(TypeError):
            mod.minimize("obj")  # pytype: disable=wrong-arg-types
        with self.assertRaises(TypeError):
            mod.maximize("obj")  # pytype: disable=wrong-arg-types

    def test_linear_constraint_matrix(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
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

        self.assertCountEqual(
            [repr(model.LinearTerm(variable=x, coefficient=1.0))],
            [repr(term) for term in c.terms()],
        )
        self.assertCountEqual(
            [
                repr(model.LinearTerm(variable=x, coefficient=2.0)),
                repr(model.LinearTerm(variable=z, coefficient=-1.0)),
            ],
            [repr(term) for term in d.terms()],
        )

        self.assertCountEqual(
            [
                model.LinearConstraintMatrixEntry(
                    linear_constraint=c, variable=x, coefficient=1.0
                ),
                model.LinearConstraintMatrixEntry(
                    linear_constraint=d, variable=x, coefficient=2.0
                ),
                model.LinearConstraintMatrixEntry(
                    linear_constraint=d, variable=z, coefficient=-1.0
                ),
            ],
            mod.linear_constraint_matrix_entries(),
        )

    def test_linear_constraint_expression(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
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

    def test_linear_constraint_bounded_expression(
        self, storage_class: StorageClass
    ) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")

        c = mod.add_linear_constraint((0.0 <= x + 1.0) <= 1.0, name="c")
        self.assertEqual(1.0, c.get_coefficient(x))
        self.assertEqual(0.0, c.get_coefficient(y))
        self.assertEqual(0.0, c.get_coefficient(z))
        self.assertEqual(-1.0, c.lower_bound)
        self.assertEqual(0.0, c.upper_bound)

    def test_linear_constraint_upper_bounded_expression(
        self, storage_class: StorageClass
    ) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")

        d = mod.add_linear_constraint(2 * x - z + 2.0 <= 1.0, name="d")
        self.assertEqual(2.0, d.get_coefficient(x))
        self.assertEqual(0.0, d.get_coefficient(y))
        self.assertEqual(-1.0, d.get_coefficient(z))
        self.assertEqual(-math.inf, d.lower_bound)
        self.assertEqual(-1.0, d.upper_bound)

    def test_linear_constraint_lower_bounded_expression(
        self, storage_class: StorageClass
    ) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")

        e = mod.add_linear_constraint(1.0 <= x + y + 2.0, name="e")
        self.assertEqual(1.0, e.get_coefficient(x))
        self.assertEqual(1.0, e.get_coefficient(y))
        self.assertEqual(0.0, e.get_coefficient(z))
        self.assertEqual(-1.0, e.lower_bound)
        self.assertEqual(math.inf, e.upper_bound)

    def test_linear_constraint_number_eq_expression(
        self, storage_class: StorageClass
    ) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")

        f = mod.add_linear_constraint(1.0 == x + y + 2.0, name="e")
        self.assertEqual(1.0, f.get_coefficient(x))
        self.assertEqual(1.0, f.get_coefficient(y))
        self.assertEqual(0.0, f.get_coefficient(z))
        self.assertEqual(-1.0, f.lower_bound)
        self.assertEqual(-1.0, f.upper_bound)

    def test_linear_constraint_expression_eq_expression(
        self, storage_class: StorageClass
    ) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")

        f = mod.add_linear_constraint(1.0 - x == y + 2.0, name="e")
        self.assertEqual(-1.0, f.get_coefficient(x))
        self.assertEqual(-1.0, f.get_coefficient(y))
        self.assertEqual(0.0, f.get_coefficient(z))
        self.assertEqual(1.0, f.lower_bound)
        self.assertEqual(1.0, f.upper_bound)

    def test_linear_constraint_variable_eq_variable(
        self, storage_class: StorageClass
    ) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")

        f = mod.add_linear_constraint(x == y, name="e")
        self.assertEqual(1.0, f.get_coefficient(x))
        self.assertEqual(-1.0, f.get_coefficient(y))
        self.assertEqual(0.0, f.get_coefficient(z))
        self.assertEqual(0.0, f.lower_bound)
        self.assertEqual(0.0, f.upper_bound)

    def test_linear_constraint_errors(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")

        with self.assertRaisesRegex(
            TypeError,
            "unsupported type for bounded_expr.*bool.*!= constraints.*constant" " left",
        ):
            mod.add_linear_constraint(x != y)

        with self.assertRaisesRegex(TypeError, "!= constraints.*"):
            mod.add_linear_constraint(x + y != y)

        with self.assertRaisesRegex(TypeError, "!= constraints.*"):
            mod.add_linear_constraint(x != x + y)

        with self.assertRaisesRegex(
            TypeError,
            "unsupported type for bounded_expr.*bool.*!= constraints.*constant" " left",
        ):
            mod.add_linear_constraint(1 <= 2)  # pylint: disable=comparison-of-constants

        with self.assertRaisesRegex(
            TypeError,
            "unsupported type for bounded_expr.*bool.*!= constraints.*constant" " left",
        ):
            mod.add_linear_constraint(1 <= 0)  # pylint: disable=comparison-of-constants

        with self.assertRaisesRegex(
            TypeError,
            "unsupported type for bounded_expr.*bool.*!= constraints.*constant" " left",
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

        with self.assertRaisesRegex(ValueError, "lb cannot be specified.*"):
            mod.add_linear_constraint(x + y == 1, lb=1)

        with self.assertRaisesRegex(ValueError, "ub cannot be specified.*"):
            mod.add_linear_constraint(x + y == 1, ub=1)

        with self.assertRaisesRegex(ValueError, "expr cannot be specified.*"):
            mod.add_linear_constraint(x + y == 1, expr=2 * x)

        with self.assertRaisesRegex(
            TypeError, "unsupported type for expr argument.*str"
        ):
            mod.add_linear_constraint(expr="string")  # pytype: disable=wrong-arg-types

        with self.assertRaisesRegex(ValueError, ".*infinite offset."):
            mod.add_linear_constraint(expr=math.inf, lb=0.0)

    def test_linear_constraint_matrix_with_variable_deletion(
        self, storage_class: StorageClass
    ) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
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
                model.LinearConstraintMatrixEntry(
                    linear_constraint=c, variable=y, coefficient=2.0
                )
            ],
            mod.linear_constraint_matrix_entries(),
        )
        self.assertCountEqual([c], mod.column_nonzeros(y))
        self.assertCountEqual(
            [repr(model.LinearTerm(variable=y, coefficient=2.0))],
            [repr(term) for term in c.terms()],
        )
        self.assertCountEqual([], d.terms())
        with self.assertRaises(LookupError):
            c.get_coefficient(x)

    def test_linear_constraint_matrix_with_linear_constraint_deletion(
        self, storage_class: StorageClass
    ) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
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
                model.LinearConstraintMatrixEntry(
                    linear_constraint=d, variable=x, coefficient=1.0
                )
            ],
            mod.linear_constraint_matrix_entries(),
        )
        self.assertCountEqual([d], mod.column_nonzeros(x))
        self.assertCountEqual([], mod.column_nonzeros(y))
        self.assertCountEqual(
            [repr(model.LinearTerm(variable=x, coefficient=1.0))],
            [repr(term) for term in d.terms()],
        )

    def test_linear_constraint_matrix_wrong_model(
        self, storage_class: StorageClass
    ) -> None:
        mod1 = model.Model(name="test_model1", storage_class=storage_class)
        x1 = mod1.add_binary_variable(name="x")
        mod2 = model.Model(name="test_model2", storage_class=storage_class)
        mod2.add_binary_variable(name="x")
        c2 = mod2.add_linear_constraint(lb=0.0, ub=1.0, name="c")
        with self.assertRaises(ValueError):
            c2.set_coefficient(x1, 1.0)

    def test_export(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
        mod.objective.offset = 2.0
        mod.objective.is_maximize = True
        x = mod.add_binary_variable(name="x")
        y = mod.add_binary_variable(name="y")
        c = mod.add_linear_constraint(lb=0.0, ub=2.0, name="c")
        c.set_coefficient(x, 1.0)
        c.set_coefficient(y, 2.0)
        mod.objective.set_linear_coefficient(y, 3.0)
        expected = model_pb2.ModelProto(
            name="test_model",
            variables=model_pb2.VariablesProto(
                ids=[0, 1],
                lower_bounds=[0.0, 0.0],
                upper_bounds=[1.0, 1.0],
                integers=[True, True],
                names=["x", "y"],
            ),
            linear_constraints=model_pb2.LinearConstraintsProto(
                ids=[0], lower_bounds=[0.0], upper_bounds=[2.0], names=["c"]
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
        self.assert_protos_equiv(expected, mod.export_model())

    def test_update_tracker_simple(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
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

    def test_two_update_trackers(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
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

    def test_remove_tracker(self, storage_class: StorageClass) -> None:
        mod = model.Model(name="test_model", storage_class=storage_class)
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
        with self.assertRaises(model_storage.UsedUpdateTrackerAfterRemovalError):
            t1.export_update()
        with self.assertRaises(model_storage.UsedUpdateTrackerAfterRemovalError):
            t1.advance_checkpoint()
        with self.assertRaises(KeyError):
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

    def test_model(self) -> None:
        mod = model.Model(name="test_model")
        with self.assertRaises(AttributeError):
            mod.objectize = None  # pytype: disable=not-writable


if __name__ == "__main__":
    absltest.main()
