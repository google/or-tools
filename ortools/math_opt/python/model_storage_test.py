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
from typing import Any, Callable

from absl.testing import absltest
from absl.testing import parameterized
from ortools.math_opt import model_pb2
from ortools.math_opt import sparse_containers_pb2
from ortools.math_opt.python import hash_model_storage
from ortools.math_opt.python import model_storage
from ortools.math_opt.python.testing import compare_proto

_StorageClass = model_storage.ModelStorageImplClass
_MatEntry = model_storage.LinearConstraintMatrixIdEntry
_ObjEntry = model_storage.LinearObjectiveEntry


@parameterized.parameters((hash_model_storage.HashModelStorage,))
class ModelStorageTest(compare_proto.MathOptProtoAssertions, parameterized.TestCase):

    def test_add_and_read_variables(self, storage_class: _StorageClass) -> None:
        storage = storage_class("test_model")
        self.assertEqual(0, storage.next_variable_id())
        v1 = storage.add_variable(-1.0, 2.5, True, "x")
        v2 = storage.add_variable(-math.inf, math.inf, False, "")
        self.assertEqual("test_model", storage.name)

        self.assertEqual(-1.0, storage.get_variable_lb(v1))
        self.assertEqual(2.5, storage.get_variable_ub(v1))
        self.assertTrue(storage.get_variable_is_integer(v1))
        self.assertEqual("x", storage.get_variable_name(v1))
        self.assertEqual(0, v1)
        self.assertTrue(storage.variable_exists(v1))

        self.assertEqual(-math.inf, storage.get_variable_lb(v2))
        self.assertEqual(math.inf, storage.get_variable_ub(v2))
        self.assertFalse(storage.get_variable_is_integer(v2))
        self.assertEqual("", storage.get_variable_name(v2))
        self.assertEqual(1, v2)
        self.assertTrue(storage.variable_exists(v2))

        self.assertFalse(storage.variable_exists(max(v1, v2) + 1))
        self.assertListEqual([v1, v2], list(storage.get_variables()))
        self.assertEqual(2, storage.next_variable_id())

    def test_set_variable_lb(self, storage_class: _StorageClass) -> None:
        storage = storage_class()
        v1 = storage.add_variable(-1.0, 2.5, True, "x")
        storage.set_variable_lb(v1, -5.5)
        self.assertEqual(-5.5, storage.get_variable_lb(v1))

    def test_set_variable_ub(self, storage_class: _StorageClass) -> None:
        storage = storage_class()
        v1 = storage.add_variable(-1.0, 2.5, True, "x")
        storage.set_variable_ub(v1, 1.2)
        self.assertEqual(1.2, storage.get_variable_ub(v1))

    def test_set_variable_is_integer(self, storage_class: _StorageClass) -> None:
        storage = storage_class()
        v1 = storage.add_variable(-1.0, 2.5, True, "x")
        storage.set_variable_is_integer(v1, False)
        self.assertFalse(storage.get_variable_is_integer(v1))

    def test_add_and_read_linear_constraints(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        self.assertEqual(0, storage.next_linear_constraint_id())
        c1 = storage.add_linear_constraint(-1.0, 2.5, "c")
        c2 = storage.add_linear_constraint(-math.inf, math.inf, "")

        self.assertEqual(-1.0, storage.get_linear_constraint_lb(c1))
        self.assertEqual(2.5, storage.get_linear_constraint_ub(c1))
        self.assertEqual("c", storage.get_linear_constraint_name(c1))
        self.assertEqual(0, c1)
        self.assertTrue(storage.linear_constraint_exists(c1))

        self.assertEqual(-math.inf, storage.get_linear_constraint_lb(c2))
        self.assertEqual(math.inf, storage.get_linear_constraint_ub(c2))
        self.assertEqual("", storage.get_linear_constraint_name(c2))
        self.assertEqual(1, c2)
        self.assertTrue(storage.linear_constraint_exists(c2))

        self.assertListEqual([c1, c2], list(storage.get_linear_constraints()))
        self.assertFalse(storage.linear_constraint_exists(1 + max(c1, c2)))
        self.assertEqual(2, storage.next_linear_constraint_id())

    def test_set_linear_constraint_lb(self, storage_class: _StorageClass) -> None:
        storage = storage_class()
        c1 = storage.add_linear_constraint(-1.0, 2.5, "c")
        storage.set_linear_constraint_lb(c1, -5.5)
        self.assertEqual(-5.5, storage.get_linear_constraint_lb(c1))

    def test_set_linear_constraint_ub(self, storage_class: _StorageClass) -> None:
        storage = storage_class()
        c1 = storage.add_linear_constraint(-1.0, 2.5, "c")
        storage.set_linear_constraint_ub(c1, 1.2)
        self.assertEqual(1.2, storage.get_linear_constraint_ub(c1))

    def test_delete_variable_get_other(self, storage_class: _StorageClass) -> None:
        storage = storage_class()
        v1 = storage.add_variable(-1.0, 2.5, True, "x")
        v2 = storage.add_variable(-3.0, 4.5, False, "y")
        storage.delete_variable(v1)
        self.assertEqual(-3.0, storage.get_variable_lb(v2))
        self.assertEqual(4.5, storage.get_variable_ub(v2))
        self.assertFalse(storage.get_variable_is_integer(v2))
        self.assertEqual("y", storage.get_variable_name(v2))
        self.assertEqual(1, v2)
        self.assertFalse(storage.variable_exists(v1))
        self.assertTrue(storage.variable_exists(v2))

        self.assertListEqual([v2], list(storage.get_variables()))

    def test_double_variable_delete(self, storage_class: _StorageClass) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        storage.delete_variable(x)
        with self.assertRaises(model_storage.BadVariableIdError) as cm:
            storage.delete_variable(x)
        self.assertEqual(x, cm.exception.id)

    def _deleted_variable_invoke_lookup(
        self,
        storage_class: _StorageClass,
        getter: Callable[[model_storage.ModelStorage, int], Any],
    ) -> None:
        storage = storage_class()
        v1 = storage.add_variable(-1.0, 2.5, True, "x")
        storage.delete_variable(v1)
        with self.assertRaises(model_storage.BadVariableIdError) as cm:
            getter(storage, v1)
        self.assertEqual(v1, cm.exception.id)

    def test_delete_variable_lb_error(self, storage_class: _StorageClass) -> None:
        self._deleted_variable_invoke_lookup(
            storage_class, storage_class.get_variable_lb
        )

    def test_delete_variable_ub_error(self, storage_class: _StorageClass) -> None:
        self._deleted_variable_invoke_lookup(
            storage_class, storage_class.get_variable_ub
        )

    def test_delete_variable_is_integer_error(
        self, storage_class: _StorageClass
    ) -> None:
        self._deleted_variable_invoke_lookup(
            storage_class, storage_class.get_variable_is_integer
        )

    def test_delete_variable_name_error(self, storage_class: _StorageClass) -> None:
        self._deleted_variable_invoke_lookup(
            storage_class, storage_class.get_variable_name
        )

    def test_delete_variable_set_lb_error(self, storage_class: _StorageClass) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        storage.delete_variable(x)
        with self.assertRaises(model_storage.BadVariableIdError) as cm:
            storage.set_variable_lb(x, -2.0)
        self.assertEqual(x, cm.exception.id)

    def test_delete_variable_set_ub_error(self, storage_class: _StorageClass) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        storage.delete_variable(x)
        with self.assertRaises(model_storage.BadVariableIdError) as cm:
            storage.set_variable_ub(x, 12.0)
        self.assertEqual(x, cm.exception.id)

    def test_delete_variable_set_integer_error(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        storage.delete_variable(x)
        with self.assertRaises(model_storage.BadVariableIdError) as cm:
            storage.set_variable_is_integer(x, False)
        self.assertEqual(x, cm.exception.id)

    def test_delete_linear_constraint_get_other(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        c1 = storage.add_linear_constraint(-1.0, 2.5, "c1")
        c2 = storage.add_linear_constraint(-math.inf, 5.0, "c2")
        storage.delete_linear_constraint(c1)
        self.assertEqual(-math.inf, storage.get_linear_constraint_lb(c2))
        self.assertEqual(5.0, storage.get_linear_constraint_ub(c2))
        self.assertEqual("c2", storage.get_linear_constraint_name(c2))
        self.assertEqual(1, c2)

        self.assertListEqual([c2], list(storage.get_linear_constraints()))

    def test_double_linear_constraint_delete(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        c = storage.add_linear_constraint(-1.0, 2.5, "c")
        storage.delete_linear_constraint(c)
        with self.assertRaises(model_storage.BadLinearConstraintIdError) as cm:
            storage.delete_linear_constraint(c)
        self.assertEqual(c, cm.exception.id)

    def _deleted_linear_constraint_invoke_lookup(
        self,
        storage_class: _StorageClass,
        getter: Callable[[model_storage.ModelStorage, int], Any],
    ) -> None:
        storage = storage_class()
        c1 = storage.add_linear_constraint(-1.0, 2.5, "c1")
        storage.delete_linear_constraint(c1)
        with self.assertRaises(model_storage.BadLinearConstraintIdError) as cm:
            getter(storage, c1)
        self.assertEqual(c1, cm.exception.id)

    def test_delete_linear_constraint_lb_error(
        self, storage_class: _StorageClass
    ) -> None:
        self._deleted_linear_constraint_invoke_lookup(
            storage_class, storage_class.get_linear_constraint_lb
        )

    def test_delete_linear_constraint_ub_error(
        self, storage_class: _StorageClass
    ) -> None:
        self._deleted_linear_constraint_invoke_lookup(
            storage_class, storage_class.get_linear_constraint_ub
        )

    def test_delete_linear_constraint_name_error(
        self, storage_class: _StorageClass
    ) -> None:
        self._deleted_linear_constraint_invoke_lookup(
            storage_class, storage_class.get_linear_constraint_name
        )

    def test_delete_linear_constraint_set_lb_error(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        c = storage.add_linear_constraint(-1.0, 2.5, "c")
        storage.delete_linear_constraint(c)
        with self.assertRaises(model_storage.BadLinearConstraintIdError) as cm:
            storage.set_linear_constraint_lb(c, -2.0)
        self.assertEqual(c, cm.exception.id)

    def test_delete_linear_constraint_set_ub_error(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        c = storage.add_linear_constraint(-1.0, 2.5, "c")
        storage.delete_linear_constraint(c)
        with self.assertRaises(model_storage.BadLinearConstraintIdError) as cm:
            storage.set_linear_constraint_ub(c, 12.0)
        self.assertEqual(c, cm.exception.id)

    def test_objective_offset(self, storage_class: _StorageClass) -> None:
        storage = storage_class()
        self.assertEqual(0.0, storage.get_objective_offset())
        storage.set_objective_offset(1.5)
        self.assertEqual(1.5, storage.get_objective_offset())

    def test_objective_direction(self, storage_class: _StorageClass) -> None:
        storage = storage_class()
        self.assertFalse(storage.get_is_maximize())
        storage.set_is_maximize(True)
        self.assertTrue(storage.get_is_maximize())
        storage.set_is_maximize(False)
        self.assertFalse(storage.get_is_maximize())

    def test_set_linear_objective_coefficient(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        y = storage.add_variable(0.0, 1.0, False, "y")
        z = storage.add_variable(0.0, 1.0, True, "z")
        storage.set_linear_objective_coefficient(x, 2.0)
        storage.set_linear_objective_coefficient(z, -5.5)
        self.assertEqual(2.0, storage.get_linear_objective_coefficient(x))
        self.assertEqual(0.0, storage.get_linear_objective_coefficient(y))
        self.assertEqual(-5.5, storage.get_linear_objective_coefficient(z))

        self.assertCountEqual(
            [
                _ObjEntry(variable_id=x, coefficient=2.0),
                _ObjEntry(variable_id=z, coefficient=-5.5),
            ],
            storage.get_linear_objective_coefficients(),
        )

    def test_clear_linear_objective_coefficient(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        y = storage.add_variable(0.0, 1.0, False, "y")
        z = storage.add_variable(0.0, 1.0, True, "z")
        storage.set_linear_objective_coefficient(x, 2.0)
        storage.set_linear_objective_coefficient(z, -5.5)
        storage.set_objective_offset(1.0)
        self.assertEqual(2.0, storage.get_linear_objective_coefficient(x))
        self.assertEqual(0.0, storage.get_linear_objective_coefficient(y))
        self.assertEqual(-5.5, storage.get_linear_objective_coefficient(z))
        self.assertEqual(1.0, storage.get_objective_offset())
        storage.clear_objective()
        self.assertEqual(0.0, storage.get_linear_objective_coefficient(x))
        self.assertEqual(0.0, storage.get_linear_objective_coefficient(y))
        self.assertEqual(0.0, storage.get_linear_objective_coefficient(z))
        self.assertEqual(0.0, storage.get_objective_offset())

    def test_set_linear_objective_coefficient_bad_id(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        with self.assertRaises(model_storage.BadVariableIdError) as cm:
            storage.set_linear_objective_coefficient(x + 1, 2.0)
        self.assertEqual(x + 1, cm.exception.id)

    def test_set_linear_objective_coefficient_deleted_id(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        y = storage.add_variable(-1.0, 2.5, True, "y")
        storage.set_linear_objective_coefficient(y, 3.0)
        storage.delete_variable(x)
        self.assertEqual(3.0, storage.get_linear_objective_coefficient(y))
        self.assertCountEqual(
            [model_storage.LinearObjectiveEntry(variable_id=y, coefficient=3.0)],
            storage.get_linear_objective_coefficients(),
        )
        with self.assertRaises(model_storage.BadVariableIdError) as cm:
            storage.set_linear_objective_coefficient(x, 2.0)
        self.assertEqual(x, cm.exception.id)

    def test_get_linear_objective_coefficient_deleted_nonzero(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        y = storage.add_variable(-1.0, 2.5, True, "y")
        storage.set_linear_objective_coefficient(x, 1.0)
        storage.set_linear_objective_coefficient(y, 3.0)
        storage.delete_variable(x)
        self.assertEqual(3.0, storage.get_linear_objective_coefficient(y))
        with self.assertRaises(model_storage.BadVariableIdError) as cm:
            storage.get_linear_objective_coefficient(x)
        self.assertEqual(x, cm.exception.id)

    def test_set_quadratic_objective_coefficient(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        y = storage.add_variable(0.0, 1.0, False, "y")
        z = storage.add_variable(0.0, 1.0, True, "z")
        storage.set_quadratic_objective_coefficient(x, y, 2.0)
        storage.set_quadratic_objective_coefficient(z, z, -5.5)
        storage.set_quadratic_objective_coefficient(z, y, 1.5)
        self.assertEqual(2.0, storage.get_quadratic_objective_coefficient(x, y))
        self.assertEqual(0.0, storage.get_quadratic_objective_coefficient(y, y))
        self.assertEqual(-5.5, storage.get_quadratic_objective_coefficient(z, z))
        self.assertEqual(1.5, storage.get_quadratic_objective_coefficient(y, z))

        self.assertCountEqual(
            [
                model_storage.QuadraticEntry(
                    id_key=model_storage.QuadraticTermIdKey(x, y), coefficient=2.0
                ),
                model_storage.QuadraticEntry(
                    id_key=model_storage.QuadraticTermIdKey(z, z), coefficient=-5.5
                ),
                model_storage.QuadraticEntry(
                    id_key=model_storage.QuadraticTermIdKey(y, z), coefficient=1.5
                ),
            ],
            storage.get_quadratic_objective_coefficients(),
        )

        self.assertCountEqual(
            [y], storage.get_quadratic_objective_adjacent_variables(x)
        )
        self.assertCountEqual(
            [x, z], storage.get_quadratic_objective_adjacent_variables(y)
        )
        self.assertCountEqual(
            [y, z], storage.get_quadratic_objective_adjacent_variables(z)
        )

    def test_clear_quadratic_objective_coefficient(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        y = storage.add_variable(0.0, 1.0, False, "y")
        z = storage.add_variable(0.0, 1.0, True, "z")
        storage.set_linear_objective_coefficient(x, 2.0)
        storage.set_linear_objective_coefficient(z, -5.5)
        storage.set_quadratic_objective_coefficient(x, y, 2.0)
        storage.set_quadratic_objective_coefficient(z, z, -5.5)
        storage.set_quadratic_objective_coefficient(z, y, 1.5)
        storage.set_objective_offset(1.0)
        storage.clear_objective()
        self.assertEqual(0.0, storage.get_linear_objective_coefficient(x))
        self.assertEqual(0.0, storage.get_linear_objective_coefficient(y))
        self.assertEqual(0.0, storage.get_linear_objective_coefficient(z))
        self.assertEqual(0.0, storage.get_quadratic_objective_coefficient(x, y))
        self.assertEqual(0.0, storage.get_quadratic_objective_coefficient(y, y))
        self.assertEqual(0.0, storage.get_quadratic_objective_coefficient(z, z))
        self.assertEqual(0.0, storage.get_quadratic_objective_coefficient(y, z))
        self.assertEqual(0.0, storage.get_objective_offset())
        self.assertEmpty(list(storage.get_quadratic_objective_adjacent_variables(x)))
        self.assertEmpty(list(storage.get_quadratic_objective_adjacent_variables(y)))
        self.assertEmpty(list(storage.get_quadratic_objective_adjacent_variables(z)))

    def test_set_quadratic_objective_coefficient_bad_id(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        with self.assertRaises(model_storage.BadVariableIdError) as cm:
            storage.set_quadratic_objective_coefficient(x, x + 1, 2.0)
        with self.assertRaises(model_storage.BadVariableIdError) as cm:
            storage.set_quadratic_objective_coefficient(x + 1, x, 2.0)
        self.assertEqual(x + 1, cm.exception.id)

    def test_get_quadratic_objective_coefficient_bad_id(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        with self.assertRaises(model_storage.BadVariableIdError) as cm:
            storage.get_quadratic_objective_coefficient(x, x + 1)
        with self.assertRaises(model_storage.BadVariableIdError) as cm:
            storage.get_quadratic_objective_coefficient(x + 1, x)
        self.assertEqual(x + 1, cm.exception.id)
        with self.assertRaises(model_storage.BadVariableIdError) as cm:
            list(storage.get_quadratic_objective_adjacent_variables(x + 1))
        self.assertEqual(x + 1, cm.exception.id)

    def test_set_quadratic_objective_coefficient_existing_to_zero(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        y = storage.add_variable(-1.0, 2.5, True, "y")
        storage.set_quadratic_objective_coefficient(x, x, -1.0)
        storage.set_quadratic_objective_coefficient(x, y, 1.0)
        storage.set_quadratic_objective_coefficient(y, y, 3.0)

        storage.set_quadratic_objective_coefficient(x, x, 0.0)
        storage.set_quadratic_objective_coefficient(x, y, 0.0)
        self.assertEqual(0.0, storage.get_quadratic_objective_coefficient(x, x))
        self.assertEqual(0.0, storage.get_quadratic_objective_coefficient(x, y))
        self.assertEqual(3.0, storage.get_quadratic_objective_coefficient(y, y))
        self.assertCountEqual(
            [y], storage.get_quadratic_objective_adjacent_variables(y)
        )
        self.assertEmpty(list(storage.get_quadratic_objective_adjacent_variables(x)))

    def test_set_quadratic_objective_coefficient_deleted_id(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        y = storage.add_variable(-1.0, 2.5, True, "y")
        storage.set_quadratic_objective_coefficient(x, y, 1.0)
        storage.set_quadratic_objective_coefficient(y, y, 3.0)
        storage.delete_variable(x)
        self.assertEqual(3.0, storage.get_quadratic_objective_coefficient(y, y))
        self.assertCountEqual(
            [y], storage.get_quadratic_objective_adjacent_variables(y)
        )

    def test_set_quadratic_objective_coefficient_deleted_id_get_coeff_error(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        y = storage.add_variable(-1.0, 2.5, True, "y")
        storage.set_quadratic_objective_coefficient(x, y, 1.0)
        storage.set_quadratic_objective_coefficient(y, y, 3.0)
        storage.delete_variable(x)

        with self.assertRaises(model_storage.BadVariableIdError) as cm:
            storage.get_quadratic_objective_coefficient(x, y)
        self.assertEqual(x, cm.exception.id)

    def test_set_quadratic_objective_coefficient_deleted_id_set_coeff_error(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        y = storage.add_variable(-1.0, 2.5, True, "y")
        storage.set_quadratic_objective_coefficient(x, y, 1.0)
        storage.set_quadratic_objective_coefficient(y, y, 3.0)
        storage.delete_variable(x)

        with self.assertRaises(model_storage.BadVariableIdError) as cm:
            storage.set_quadratic_objective_coefficient(x, y, 1.0)
        self.assertEqual(x, cm.exception.id)

    def test_set_quadratic_objective_coefficient_deleted_id_adjacent_error(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        y = storage.add_variable(-1.0, 2.5, True, "y")
        storage.set_quadratic_objective_coefficient(x, y, 1.0)
        storage.set_quadratic_objective_coefficient(y, y, 3.0)
        storage.delete_variable(x)

        with self.assertRaises(model_storage.BadVariableIdError) as cm:
            list(storage.get_quadratic_objective_adjacent_variables(x))
        self.assertEqual(x, cm.exception.id)

    def test_constraint_matrix(self, storage_class: _StorageClass) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        y = storage.add_variable(-1.0, 2.5, False, "y")
        z = storage.add_variable(0.0, 1.0, True, "z")
        c = storage.add_linear_constraint(-math.inf, 3.0, "c")
        d = storage.add_linear_constraint(-math.inf, 1.0, "d")
        storage.set_linear_constraint_coefficient(c, y, 1.0)
        storage.set_linear_constraint_coefficient(d, x, 2.0)
        storage.set_linear_constraint_coefficient(d, y, -1.0)
        storage.set_linear_constraint_coefficient(d, z, 1.0)
        storage.set_linear_constraint_coefficient(d, z, 0.0)

        self.assertEqual(0.0, storage.get_linear_constraint_coefficient(c, x))
        self.assertEqual(1.0, storage.get_linear_constraint_coefficient(c, y))
        self.assertEqual(0.0, storage.get_linear_constraint_coefficient(c, z))

        self.assertEqual(2.0, storage.get_linear_constraint_coefficient(d, x))
        self.assertEqual(-1.0, storage.get_linear_constraint_coefficient(d, y))
        self.assertEqual(0.0, storage.get_linear_constraint_coefficient(d, z))

        self.assertCountEqual([y], storage.get_variables_for_linear_constraint(c))
        self.assertCountEqual([x, y], storage.get_variables_for_linear_constraint(d))

        self.assertCountEqual([d], storage.get_linear_constraints_with_variable(x))
        self.assertCountEqual([c, d], storage.get_linear_constraints_with_variable(y))
        self.assertCountEqual([], storage.get_linear_constraints_with_variable(z))

        self.assertCountEqual(
            [
                _MatEntry(linear_constraint_id=c, variable_id=y, coefficient=1.0),
                _MatEntry(linear_constraint_id=d, variable_id=x, coefficient=2.0),
                _MatEntry(linear_constraint_id=d, variable_id=y, coefficient=-1.0),
            ],
            storage.get_linear_constraint_matrix_entries(),
        )

    def test_constraint_matrix_zero_unset_entry(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        c = storage.add_linear_constraint(-math.inf, 3.0, "c")
        storage.set_linear_constraint_coefficient(c, x, 0.0)
        self.assertEmpty(list(storage.get_linear_objective_coefficients()))
        self.assertEmpty(list(storage.get_variables_for_linear_constraint(c)))
        self.assertEmpty(list(storage.get_linear_constraints_with_variable(x)))
        self.assertEqual(0.0, storage.get_linear_constraint_coefficient(c, x))

    def test_constraint_matrix_with_deletion(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        y = storage.add_variable(-1.0, 2.5, False, "y")
        z = storage.add_variable(0.0, 1.0, True, "z")
        c = storage.add_linear_constraint(-math.inf, 3.0, "c")
        d = storage.add_linear_constraint(-math.inf, 1.0, "d")
        storage.set_linear_constraint_coefficient(c, y, 1.0)
        storage.set_linear_constraint_coefficient(d, x, 2.0)
        storage.set_linear_constraint_coefficient(d, y, -1.0)
        storage.set_linear_constraint_coefficient(c, z, 1.0)

        storage.delete_variable(y)
        storage.delete_linear_constraint(c)

        self.assertEqual(2.0, storage.get_linear_constraint_coefficient(d, x))
        self.assertEqual(0.0, storage.get_linear_constraint_coefficient(d, z))

        self.assertCountEqual([x], storage.get_variables_for_linear_constraint(d))

        self.assertCountEqual([d], storage.get_linear_constraints_with_variable(x))
        self.assertCountEqual([], storage.get_linear_constraints_with_variable(z))

        self.assertCountEqual(
            [_MatEntry(linear_constraint_id=d, variable_id=x, coefficient=2.0)],
            storage.get_linear_constraint_matrix_entries(),
        )

    def test_variables_for_linear_constraint_deleted_error(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        c = storage.add_linear_constraint(-math.inf, 3.0, "c")
        storage.set_linear_constraint_coefficient(c, x, 1.0)
        storage.delete_linear_constraint(c)
        with self.assertRaises(model_storage.BadLinearConstraintIdError) as cm:
            list(storage.get_variables_for_linear_constraint(c))
        self.assertEqual(c, cm.exception.id)

    def test_linear_constraints_with_variable_deleted_error(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        c = storage.add_linear_constraint(-math.inf, 3.0, "c")
        storage.set_linear_constraint_coefficient(c, x, 1.0)
        storage.delete_variable(x)
        with self.assertRaises(model_storage.BadVariableIdError) as cm:
            list(storage.get_linear_constraints_with_variable(x))
        self.assertEqual(x, cm.exception.id)

    def test_constraint_matrix_set_deleted_var(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        c = storage.add_linear_constraint(-math.inf, 3.0, "c")
        storage.delete_variable(x)
        with self.assertRaises(model_storage.BadVariableIdError) as cm:
            storage.set_linear_constraint_coefficient(c, x, 2.0)
        self.assertEqual(x, cm.exception.id)

    def test_constraint_matrix_get_deleted_var(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        c = storage.add_linear_constraint(-math.inf, 3.0, "c")
        storage.delete_variable(x)
        with self.assertRaises(model_storage.BadVariableIdError) as cm:
            storage.get_linear_constraint_coefficient(c, x)
        self.assertEqual(x, cm.exception.id)

    def test_constraint_matrix_set_deleted_constraint(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        c = storage.add_linear_constraint(-math.inf, 3.0, "c")
        storage.delete_linear_constraint(c)
        with self.assertRaises(model_storage.BadLinearConstraintIdError) as cm:
            storage.set_linear_constraint_coefficient(c, x, 2.0)
        self.assertEqual(c, cm.exception.id)

    def test_constraint_matrix_get_deleted_constraint(
        self, storage_class: _StorageClass
    ) -> None:
        storage = storage_class()
        x = storage.add_variable(-1.0, 2.5, True, "x")
        c = storage.add_linear_constraint(-math.inf, 3.0, "c")
        storage.delete_linear_constraint(c)
        with self.assertRaises(model_storage.BadLinearConstraintIdError) as cm:
            storage.get_linear_constraint_coefficient(c, x)
        self.assertEqual(c, cm.exception.id)

    def test_proto_export(self, storage_class: _StorageClass) -> None:
        storage = storage_class("test_model")
        x = storage.add_variable(-1.0, 2.5, True, "x")
        y = storage.add_variable(-1.0, 2.5, False, "")
        z = storage.add_variable(0.0, 1.0, True, "z")
        c = storage.add_linear_constraint(-math.inf, 3.0, "")
        d = storage.add_linear_constraint(0.0, 1.0, "d")
        storage.set_linear_constraint_coefficient(c, y, 1.0)
        storage.set_linear_constraint_coefficient(d, x, 2.0)
        storage.set_linear_constraint_coefficient(d, y, -1.0)
        storage.set_linear_constraint_coefficient(d, z, 1.0)
        storage.set_linear_constraint_coefficient(d, z, 0.0)
        storage.set_linear_objective_coefficient(x, 2.5)
        storage.set_linear_objective_coefficient(z, -1.0)
        storage.set_quadratic_objective_coefficient(x, x, 3.0)
        storage.set_quadratic_objective_coefficient(x, y, 4.0)
        storage.set_quadratic_objective_coefficient(x, z, 5.0)
        storage.set_is_maximize(True)
        storage.set_objective_offset(7.0)

        expected = model_pb2.ModelProto(
            name="test_model",
            variables=model_pb2.VariablesProto(
                ids=[0, 1, 2],
                lower_bounds=[-1.0, -1.0, 0.0],
                upper_bounds=[2.5, 2.5, 1.0],
                integers=[True, False, True],
                names=["x", "", "z"],
            ),
            linear_constraints=model_pb2.LinearConstraintsProto(
                ids=[0, 1],
                lower_bounds=[-math.inf, 0.0],
                upper_bounds=[3.0, 1.0],
                names=["", "d"],
            ),
            objective=model_pb2.ObjectiveProto(
                maximize=True,
                offset=7.0,
                linear_coefficients=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[0, 2], values=[2.5, -1.0]
                ),
                quadratic_coefficients=sparse_containers_pb2.SparseDoubleMatrixProto(
                    row_ids=[0, 0, 0],
                    column_ids=[0, 1, 2],
                    coefficients=[3.0, 4.0, 5.0],
                ),
            ),
            linear_constraint_matrix=sparse_containers_pb2.SparseDoubleMatrixProto(
                row_ids=[0, 1, 1],
                column_ids=[1, 0, 1],
                coefficients=[1.0, 2.0, -1.0],
            ),
        )
        self.assert_protos_equiv(expected, storage.export_model())

    def test_proto_export_with_deletes(self, storage_class: _StorageClass) -> None:
        storage = storage_class("test_model")
        x = storage.add_variable(-1.0, 2.5, True, "x")
        y = storage.add_variable(-1.0, 2.5, False, "")
        z = storage.add_variable(0.0, 1.0, True, "z")
        c = storage.add_linear_constraint(-math.inf, 3.0, "")
        d = storage.add_linear_constraint(0.0, 1.0, "d")
        storage.set_linear_constraint_coefficient(c, y, 1.0)
        storage.set_linear_constraint_coefficient(d, x, 2.0)
        storage.set_linear_constraint_coefficient(d, y, -1.0)
        storage.set_linear_constraint_coefficient(d, z, 1.0)
        storage.set_linear_constraint_coefficient(d, z, 0.0)
        storage.set_linear_objective_coefficient(x, 2.5)
        storage.set_quadratic_objective_coefficient(x, x, 3.0)
        storage.set_quadratic_objective_coefficient(x, y, 4.0)
        storage.set_quadratic_objective_coefficient(x, z, 5.0)
        storage.set_is_maximize(False)
        storage.delete_variable(y)
        storage.delete_linear_constraint(c)

        expected = model_pb2.ModelProto(
            name="test_model",
            variables=model_pb2.VariablesProto(
                ids=[0, 2],
                lower_bounds=[-1.0, 0.0],
                upper_bounds=[2.5, 1.0],
                integers=[True, True],
                names=["x", "z"],
            ),
            linear_constraints=model_pb2.LinearConstraintsProto(
                ids=[1], lower_bounds=[0.0], upper_bounds=[1.0], names=["d"]
            ),
            objective=model_pb2.ObjectiveProto(
                maximize=False,
                offset=0.0,
                linear_coefficients=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[0], values=[2.5]
                ),
                quadratic_coefficients=sparse_containers_pb2.SparseDoubleMatrixProto(
                    row_ids=[0, 0], column_ids=[0, 2], coefficients=[3.0, 5.0]
                ),
            ),
            linear_constraint_matrix=sparse_containers_pb2.SparseDoubleMatrixProto(
                row_ids=[1], column_ids=[0], coefficients=[2.0]
            ),
        )
        self.assert_protos_equiv(expected, storage.export_model())

    def test_proto_export_empty(self, storage_class: _StorageClass) -> None:
        storage = storage_class("test_model")
        expected = model_pb2.ModelProto(name="test_model")
        self.assert_protos_equiv(expected, storage.export_model())

    def test_proto_export_feasibility(self, storage_class: _StorageClass) -> None:
        storage = storage_class("test_model")
        storage.add_variable(-1.0, 2.5, True, "x")
        expected = model_pb2.ModelProto(
            name="test_model",
            variables=model_pb2.VariablesProto(
                ids=[0],
                lower_bounds=[-1.0],
                upper_bounds=[2.5],
                integers=[True],
                names=["x"],
            ),
        )
        self.assert_protos_equiv(expected, storage.export_model())

    def test_proto_export_empty_names(self, storage_class: _StorageClass) -> None:
        storage = storage_class("")
        storage.add_variable(-1.0, 2.5, True, "")
        storage.add_linear_constraint(0.0, 1.0, "")
        expected = model_pb2.ModelProto(
            variables=model_pb2.VariablesProto(
                ids=[0],
                lower_bounds=[-1.0],
                upper_bounds=[2.5],
                integers=[True],
                # NOTE: names is the empty list not a list with an empty string.
                names=[],
            ),
            linear_constraints=model_pb2.LinearConstraintsProto(
                ids=[0],
                lower_bounds=[0.0],
                upper_bounds=[1.0],
                # NOTE: names is the empty list not a list with an empty string.
                names=[],
            ),
        )
        self.assert_protos_equiv(expected, storage.export_model())

    def _assert_nan(self, x):
        self.assertTrue(math.isnan(x), f"Expected nan, found {x}")

    # Ensure that we don't silently drop NaNs.
    def test_nans_pass_through(self, storage_class: _StorageClass) -> None:
        storage = storage_class("nan_model")
        nan = math.nan
        x = storage.add_variable(nan, 2.5, True, "x")
        y = storage.add_variable(-1.0, nan, True, "y")
        c = storage.add_linear_constraint(nan, math.inf, "c")
        d = storage.add_linear_constraint(0.0, nan, "d")
        storage.set_objective_offset(nan)
        storage.set_linear_objective_coefficient(x, 1.0)
        storage.set_linear_objective_coefficient(y, nan)
        storage.set_quadratic_objective_coefficient(x, x, 3.0)
        storage.set_quadratic_objective_coefficient(x, y, nan)
        storage.set_linear_constraint_coefficient(c, x, nan)
        storage.set_linear_constraint_coefficient(c, y, 1.0)
        storage.set_linear_constraint_coefficient(d, y, nan)

        # Test the getters.
        self.assertEqual("nan_model", storage.name)
        self._assert_nan(storage.get_objective_offset())
        self._assert_nan(storage.get_variable_lb(x))
        self.assertEqual(2.5, storage.get_variable_ub(x))
        self.assertEqual(-1.0, storage.get_variable_lb(y))
        self._assert_nan(storage.get_variable_ub(y))
        self.assertEqual(1.0, storage.get_linear_objective_coefficient(x))
        self._assert_nan(storage.get_linear_objective_coefficient(y))
        self._assert_nan(storage.get_linear_constraint_lb(c))
        self.assertEqual(math.inf, storage.get_linear_constraint_ub(c))
        self.assertEqual(0.0, storage.get_linear_constraint_lb(d))
        self._assert_nan(storage.get_linear_constraint_ub(d))
        self._assert_nan(storage.get_linear_constraint_coefficient(c, x))
        self.assertEqual(1.0, storage.get_linear_constraint_coefficient(c, y))
        self.assertEqual(0.0, storage.get_linear_constraint_coefficient(d, x))
        self.assertEqual(3.0, storage.get_quadratic_objective_coefficient(x, x))
        self.assertEqual(0.0, storage.get_quadratic_objective_coefficient(y, y))
        self._assert_nan(storage.get_quadratic_objective_coefficient(x, y))
        self._assert_nan(storage.get_linear_constraint_coefficient(d, y))

        # Test the iterators that interact with the NaN values.
        self.assertCountEqual([x, y], storage.get_variables_for_linear_constraint(c))
        self.assertCountEqual([y], storage.get_variables_for_linear_constraint(d))

        self.assertCountEqual([c], storage.get_linear_constraints_with_variable(x))
        self.assertCountEqual([c, d], storage.get_linear_constraints_with_variable(y))

        mat_entries = {}
        for e in storage.get_linear_constraint_matrix_entries():
            key = (e.linear_constraint_id, e.variable_id)
            self.assertNotIn(
                key,
                mat_entries,
                msg=f"found key:{key} twice, e:{e} mat_entries:{mat_entries}",
            )
            mat_entries[key] = e.coefficient
        self.assertSetEqual(set(mat_entries.keys()), set(((c, x), (c, y), (d, y))))
        self._assert_nan(mat_entries[(c, x)])
        self.assertEqual(mat_entries[(c, y)], 1.0)
        self._assert_nan(mat_entries[(d, y)])

        obj_entries = {}
        for e in storage.get_linear_objective_coefficients():
            self.assertNotIn(
                e.variable_id,
                obj_entries,
                msg=(
                    f"found variable:{e.variable_id} twice,"
                    f" e:{e} obj_entries:{obj_entries}"
                ),
            )
            obj_entries[e.variable_id] = e.coefficient
        self.assertSetEqual(set(obj_entries.keys()), set((x, y)))
        self.assertEqual(obj_entries[x], 1.0)
        self._assert_nan(obj_entries[y])

        # Export to proto
        expected = model_pb2.ModelProto(
            name="nan_model",
            variables=model_pb2.VariablesProto(
                ids=[0, 1],
                lower_bounds=[nan, -1.0],
                upper_bounds=[2.5, nan],
                integers=[True, True],
                names=["x", "y"],
            ),
            linear_constraints=model_pb2.LinearConstraintsProto(
                ids=[0, 1],
                lower_bounds=[nan, 0.0],
                upper_bounds=[math.inf, nan],
                names=["c", "d"],
            ),
            objective=model_pb2.ObjectiveProto(
                maximize=False,
                offset=nan,
                linear_coefficients=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[0, 1], values=[1.0, nan]
                ),
                quadratic_coefficients=sparse_containers_pb2.SparseDoubleMatrixProto(
                    row_ids=[0, 0], column_ids=[0, 1], coefficients=[3.0, nan]
                ),
            ),
            linear_constraint_matrix=sparse_containers_pb2.SparseDoubleMatrixProto(
                row_ids=[0, 0, 1],
                column_ids=[0, 1, 1],
                coefficients=[nan, 1.0, nan],
            ),
        )
        self.assert_protos_equiv(expected, storage.export_model())


if __name__ == "__main__":
    absltest.main()
