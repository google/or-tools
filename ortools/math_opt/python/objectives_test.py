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

from typing import Dict, Iterable, Tuple
import unittest

from absl.testing import absltest
from absl.testing import parameterized
from ortools.math_opt.elemental.python import cpp_elemental
from ortools.math_opt.python import model
from ortools.math_opt.python import objectives
from ortools.math_opt.python import variables


def _model_and_objective(
    primary: bool, obj_name=""
) -> Tuple[model.Model, objectives.Objective]:
    mod = model.Model(primary_objective_name=(obj_name if primary else ""))
    obj = (
        mod.objective
        if primary
        else mod.add_auxiliary_objective(priority=0, name=obj_name)
    )
    return (mod, obj)


def _assert_linear_terms_equal_dict(
    test: unittest.TestCase,
    actual: Iterable[variables.LinearTerm],
    expected: Dict[variables.Variable, float],
) -> None:
    actual = list(actual)
    actual_dict = {term.variable: term.coefficient for term in actual}
    test.assertDictEqual(actual_dict, expected)
    test.assertEqual(len(actual), len(actual_dict))


def _assert_quadratic_terms_equal_dict(
    test: unittest.TestCase,
    actual: Iterable[variables.QuadraticTerm],
    expected: Dict[Tuple[variables.Variable, variables.Variable], float],
) -> None:
    actual = list(actual)
    actual_dict = {
        (term.key.first_var, term.key.second_var): term.coefficient for term in actual
    }
    test.assertDictEqual(actual_dict, expected)
    test.assertEqual(len(actual), len(actual_dict))


@parameterized.named_parameters(("primary", True), ("auxiliary", False))
class LinearObjectiveTest(parameterized.TestCase):
    """Tests that primary and auxiliary objectives handle linear terms."""

    def test_same_model(self, primary: bool) -> None:
        mod, obj = _model_and_objective(primary)
        mod.check_compatible(obj)

    def test_name(self, primary: bool) -> None:
        _, obj = _model_and_objective(primary, "my_obj")
        self.assertEqual(obj.name, "my_obj")

    def test_maximize(self, primary: bool) -> None:
        _, obj = _model_and_objective(primary)
        self.assertFalse(obj.is_maximize)
        obj.is_maximize = True
        self.assertTrue(obj.is_maximize)

    def test_offset(self, primary: bool) -> None:
        _, obj = _model_and_objective(primary)
        self.assertEqual(obj.offset, 0.0)
        obj.offset = 3.2
        self.assertEqual(obj.offset, 3.2)

    def test_priority(self, primary: bool) -> None:
        _, obj = _model_and_objective(primary)
        self.assertEqual(obj.priority, 0)
        obj.priority = 12
        self.assertEqual(obj.priority, 12)

    def test_linear_coefficients_basic(self, primary: bool) -> None:
        mod, obj = _model_and_objective(primary)
        x = mod.add_variable()
        y = mod.add_variable()
        z = mod.add_variable()

        self.assertEqual(obj.get_linear_coefficient(x), 0.0)
        self.assertEqual(obj.get_linear_coefficient(y), 0.0)
        self.assertEqual(obj.get_linear_coefficient(z), 0.0)
        self.assertEmpty(list(obj.linear_terms()))

        obj.set_linear_coefficient(x, 2.1)
        obj.set_linear_coefficient(z, 3.4)

        self.assertEqual(obj.get_linear_coefficient(x), 2.1)
        self.assertEqual(obj.get_linear_coefficient(y), 0.0)
        self.assertEqual(obj.get_linear_coefficient(z), 3.4)
        _assert_linear_terms_equal_dict(self, obj.linear_terms(), {x: 2.1, z: 3.4})

    def test_linear_coefficients_restore_to_zero(self, primary: bool) -> None:
        mod, obj = _model_and_objective(primary)
        x = mod.add_variable()
        obj.set_linear_coefficient(x, 2.1)
        self.assertEqual(obj.get_linear_coefficient(x), 2.1)

        obj.set_linear_coefficient(x, 0.0)

        self.assertEqual(obj.get_linear_coefficient(x), 0.0)
        self.assertEmpty(list(obj.linear_terms()))

    def test_clear(self, primary: bool) -> None:
        mod, obj = _model_and_objective(primary)
        x = mod.add_variable()
        obj.set_linear_coefficient(x, 2.1)
        obj.is_maximize = True
        obj.offset = 4.0

        obj.clear()

        self.assertTrue(obj.is_maximize)
        self.assertEqual(obj.offset, 0.0)
        self.assertEmpty(list(obj.linear_terms()))

    def test_as_linear_expression(self, primary: bool) -> None:
        mod, obj = _model_and_objective(primary)
        x = mod.add_variable()
        obj.set_linear_coefficient(x, 2.1)
        obj.offset = 4.0

        expr = obj.as_linear_expression()

        self.assertEqual(expr.offset, 4.0)
        self.assertDictEqual(dict(expr.terms), {x: 2.1})

    def test_add_linear(self, primary: bool) -> None:
        mod, obj = _model_and_objective(primary)
        x = mod.add_variable()
        y = mod.add_variable()
        obj.set_linear_coefficient(x, 2.0)
        obj.offset = 5.5

        obj.add_linear(1.0 + x + 4.0 * y)

        self.assertAlmostEqual(obj.offset, 6.5, delta=1e-10)
        _assert_linear_terms_equal_dict(self, obj.linear_terms(), {x: 3.0, y: 4.0})

    def test_add(self, primary: bool) -> None:
        mod, obj = _model_and_objective(primary)
        x = mod.add_variable()
        obj.set_linear_coefficient(x, 2.0)
        obj.offset = 5.5

        obj.add(1.0 + x)

        self.assertAlmostEqual(obj.offset, 6.5, delta=1e-10)
        _assert_linear_terms_equal_dict(self, obj.linear_terms(), {x: 3.0})

    def test_add_linear_rejects_quadratic(self, primary: bool) -> None:
        mod, obj = _model_and_objective(primary)
        x = mod.add_variable()
        with self.assertRaisesRegex(TypeError, "Quadratic"):
            obj.add_linear(x * x)

    def test_set_to_linear(self, primary: bool) -> None:
        mod, obj = _model_and_objective(primary)
        x = mod.add_variable()
        y = mod.add_variable()
        obj.set_linear_coefficient(x, 2.0)
        obj.offset = 5.5

        obj.set_to_linear_expression(1.0 + x + 4.0 * y)

        self.assertEqual(obj.offset, 1.0)
        _assert_linear_terms_equal_dict(self, obj.linear_terms(), {x: 1.0, y: 4.0})

    def test_set_to_linear_rejects_quadratic(self, primary: bool) -> None:
        mod, obj = _model_and_objective(primary)
        x = mod.add_variable()
        with self.assertRaisesRegex(TypeError, "Quadratic"):
            obj.set_to_linear_expression(x * x)

    def test_set_to_expression(self, primary: bool) -> None:
        mod, obj = _model_and_objective(primary)
        x = mod.add_variable()
        obj.set_linear_coefficient(x, 2.0)
        obj.offset = 5.5

        obj.set_to_expression(1.0 + x)

        self.assertEqual(obj.offset, 1.0)
        _assert_linear_terms_equal_dict(self, obj.linear_terms(), {x: 1.0})

    def test_get_linear_coef_of_deleted_variable(self, primary: bool) -> None:
        mod, obj = _model_and_objective(primary)
        x = mod.add_variable()

        mod.delete_variable(x)

        with self.assertRaises(ValueError):
            obj.get_linear_coefficient(x)

    def test_set_linear_coef_of_deleted_variable(self, primary: bool) -> None:
        mod, obj = _model_and_objective(primary)
        x = mod.add_variable()

        mod.delete_variable(x)

        with self.assertRaises(ValueError):
            obj.set_linear_coefficient(x, 2.0)

    def test_get_quadratic_coef_of_deleted_variable(self, primary: bool) -> None:
        mod, obj = _model_and_objective(primary)
        x = mod.add_variable()
        y = mod.add_variable()

        mod.delete_variable(x)

        with self.assertRaises(ValueError):
            obj.get_quadratic_coefficient(x, x)

        with self.assertRaises(ValueError):
            obj.get_quadratic_coefficient(x, y)

    def test_delete_variable_terms_removed(self, primary: bool) -> None:
        mod, obj = _model_and_objective(primary)
        x = mod.add_variable()
        y = mod.add_variable()
        obj.set_to_expression(x + y + 3.0)

        mod.delete_variable(x)

        self.assertEqual(obj.offset, 3.0)
        _assert_linear_terms_equal_dict(self, obj.linear_terms(), {y: 1.0})

    def test_objective_wrong_model_linear(self, primary: bool) -> None:
        mod1, _ = _model_and_objective(primary)
        mod2, obj2 = _model_and_objective(primary)
        x1 = mod1.add_variable()
        mod2.add_variable()
        with self.assertRaises(ValueError):
            obj2.set_linear_coefficient(x1, 1.0)
        with self.assertRaises(ValueError):
            obj2.get_linear_coefficient(x1)

    def test_objective_wrong_model_get_quadratic(self, primary: bool) -> None:
        mod1, _ = _model_and_objective(primary)
        mod2, obj2 = _model_and_objective(primary)
        x = mod1.add_variable()
        other_x = mod2.add_variable(name="x")
        with self.assertRaises(ValueError):
            obj2.get_quadratic_coefficient(x, other_x)
        with self.assertRaises(ValueError):
            obj2.get_quadratic_coefficient(other_x, x)


class PrimaryObjectiveTest(absltest.TestCase):

    def test_eq(self) -> None:
        mod1 = model.Model()
        mod2 = model.Model()

        self.assertEqual(mod1.objective, mod1.objective)
        self.assertNotEqual(mod1.objective, mod2.objective)

    def test_quadratic_coefficients_basic(self) -> None:
        mod = model.Model()
        obj = mod.objective
        x = mod.add_variable()
        y = mod.add_variable()
        z = mod.add_variable()

        self.assertEqual(obj.get_quadratic_coefficient(x, x), 0.0)
        self.assertEqual(obj.get_quadratic_coefficient(y, z), 0.0)
        self.assertEqual(obj.get_quadratic_coefficient(z, x), 0.0)
        self.assertEmpty(list(obj.quadratic_terms()))

        obj.set_quadratic_coefficient(x, x, 2.1)
        obj.set_quadratic_coefficient(y, z, 3.1)
        obj.set_quadratic_coefficient(z, x, 4.1)

        self.assertEqual(obj.get_quadratic_coefficient(x, x), 2.1)
        self.assertEqual(obj.get_quadratic_coefficient(y, z), 3.1)
        self.assertEqual(obj.get_quadratic_coefficient(z, y), 3.1)
        self.assertEqual(obj.get_quadratic_coefficient(z, x), 4.1)
        self.assertEqual(obj.get_quadratic_coefficient(x, z), 4.1)
        self.assertEqual(obj.get_quadratic_coefficient(y, y), 0.0)
        _assert_quadratic_terms_equal_dict(
            self, obj.quadratic_terms(), {(x, x): 2.1, (y, z): 3.1, (x, z): 4.1}
        )

    def test_quadratic_coefficients_restore_to_zero(self) -> None:
        mod = model.Model()
        obj = mod.objective
        x = mod.add_variable()
        obj.set_quadratic_coefficient(x, x, 2.1)
        self.assertEqual(obj.get_quadratic_coefficient(x, x), 2.1)

        obj.set_quadratic_coefficient(x, x, 0.0)

        self.assertEqual(obj.get_quadratic_coefficient(x, x), 0.0)
        self.assertEmpty(list(obj.quadratic_terms()))

    def test_clear(self) -> None:
        mod = model.Model()
        obj = mod.objective
        x = mod.add_variable()
        obj.set_quadratic_coefficient(x, x, 2.1)

        obj.clear()

        self.assertEqual(obj.get_quadratic_coefficient(x, x), 0.0)
        self.assertEmpty(list(obj.quadratic_terms()))

    def test_as_linear_expression_fails(self) -> None:
        mod = model.Model()
        obj = mod.objective
        x = mod.add_variable()
        obj.set_quadratic_coefficient(x, x, 2.1)

        with self.assertRaisesRegex(TypeError, "quadratic"):
            obj.as_linear_expression()

    def test_as_quadratic_expression(self) -> None:
        mod = model.Model()
        obj = mod.objective
        x = mod.add_variable()
        y = mod.add_variable()
        obj.offset = 2.1
        obj.set_linear_coefficient(y, 3.1)
        obj.set_quadratic_coefficient(x, x, 4.1)
        obj.set_quadratic_coefficient(x, y, 5.1)

        quad_expr = obj.as_quadratic_expression()

        self.assertEqual(quad_expr.offset, 2.1)
        self.assertDictEqual(dict(quad_expr.linear_terms), {y: 3.1})
        self.assertDictEqual(
            dict(quad_expr.quadratic_terms),
            {
                variables.QuadraticTermKey(x, x): 4.1,
                variables.QuadraticTermKey(x, y): 5.1,
            },
        )

    def test_add_quadratic(self) -> None:
        mod = model.Model()
        obj = mod.objective
        x = mod.add_variable()
        y = mod.add_variable()
        obj.offset = -2.0
        obj.set_linear_coefficient(y, 3.0)
        obj.set_quadratic_coefficient(x, x, 4.0)

        obj.add_quadratic(1.0 + x + 2 * y + x * x + 3.0 * x * y)

        self.assertEqual(obj.offset, -1.0)
        _assert_linear_terms_equal_dict(self, obj.linear_terms(), {x: 1.0, y: 5.0})
        _assert_quadratic_terms_equal_dict(
            self, obj.quadratic_terms(), {(x, x): 5.0, (x, y): 3.0}
        )

    def test_add(self) -> None:
        mod = model.Model()
        obj = mod.objective
        x = mod.add_variable()
        obj.set_linear_coefficient(x, 2.0)

        obj.add(x * x)

        self.assertEqual(obj.offset, 0.0)
        _assert_linear_terms_equal_dict(self, obj.linear_terms(), {x: 2.0})
        _assert_quadratic_terms_equal_dict(self, obj.quadratic_terms(), {(x, x): 1.0})

    def test_set_to_quadratic_expression(self) -> None:
        mod = model.Model()
        obj = mod.objective
        x = mod.add_variable()
        y = mod.add_variable()
        obj.offset = -2.0
        obj.set_linear_coefficient(y, 3.0)
        obj.set_quadratic_coefficient(x, x, 4.0)

        obj.set_to_quadratic_expression(1.0 + x + 2 * y + x * x + 3.0 * x * y)

        self.assertEqual(obj.offset, 1.0)
        _assert_linear_terms_equal_dict(self, obj.linear_terms(), {x: 1.0, y: 2.0})
        _assert_quadratic_terms_equal_dict(
            self, obj.quadratic_terms(), {(x, x): 1.0, (x, y): 3.0}
        )

    def test_set_to_expression(self) -> None:
        mod = model.Model()
        obj = mod.objective
        x = mod.add_variable()
        obj.set_linear_coefficient(x, 2.0)

        obj.set_to_expression(x * x)

        self.assertEqual(obj.offset, 0.0)
        self.assertEmpty(list(obj.linear_terms()))
        _assert_quadratic_terms_equal_dict(self, obj.quadratic_terms(), {(x, x): 1.0})

    def test_set_quadratic_coef_of_deleted_variable(self) -> None:
        mod = model.Model()
        x = mod.add_variable()
        y = mod.add_variable()

        mod.delete_variable(x)

        with self.assertRaises(ValueError):
            mod.objective.set_quadratic_coefficient(x, x, 1.0)

        with self.assertRaises(ValueError):
            mod.objective.set_quadratic_coefficient(x, y, 1.0)

    def test_delete_variable_quad_terms_removed(self) -> None:
        mod = model.Model()
        obj = mod.objective
        x = mod.add_variable()
        y = mod.add_variable()
        obj.set_to_expression(x * x + x * y + y * y)

        mod.delete_variable(x)

        _assert_quadratic_terms_equal_dict(
            self, mod.objective.quadratic_terms(), {(y, y): 1.0}
        )

    def test_objective_wrong_model_set_quadratic(self) -> None:
        mod1 = model.Model()
        x = mod1.add_variable()
        mod2 = model.Model()
        other_x = mod2.add_variable(name="x")
        with self.assertRaises(ValueError):
            mod2.objective.set_quadratic_coefficient(x, other_x, 1.0)
        with self.assertRaises(ValueError):
            mod2.objective.set_quadratic_coefficient(other_x, x, 1.0)


class AuxiliaryObjectiveTest(absltest.TestCase):

    def test_invalid_id_type(self) -> None:
        elemental = cpp_elemental.CppElemental()
        with self.assertRaisesRegex(TypeError, "obj_id type"):
            objectives.AuxiliaryObjective(elemental, "dog")

    def test_eq(self) -> None:
        mod1 = model.Model()
        aux1 = mod1.add_auxiliary_objective(priority=1)
        aux2 = mod1.add_auxiliary_objective(priority=2)
        mod2 = model.Model()
        aux3 = mod2.add_auxiliary_objective(priority=1)

        self.assertEqual(aux1, aux1)
        self.assertEqual(aux1, mod1.get_auxiliary_objective(0))
        self.assertNotEqual(aux1, aux2)
        self.assertNotEqual(aux1, aux3)
        self.assertNotEqual(aux1, mod1.objective)

    def test_id(self) -> None:
        mod = model.Model()
        aux1 = mod.add_auxiliary_objective(priority=1)
        aux2 = mod.add_auxiliary_objective(priority=2)

        self.assertEqual(aux1.id, 0)
        self.assertEqual(aux2.id, 1)

    def test_get_quadratic_coefficients_is_zero(self) -> None:
        mod = model.Model()
        obj = mod.add_auxiliary_objective(priority=1)
        x = mod.add_variable()

        self.assertEqual(obj.get_quadratic_coefficient(x, x), 0.0)
        self.assertEmpty(list(obj.quadratic_terms()))

    def test_set_quadratic_coefficients_is_error(self) -> None:
        mod = model.Model()
        obj = mod.add_auxiliary_objective(priority=1)
        x = mod.add_variable()

        with self.assertRaisesRegex(ValueError, "Quadratic"):
            obj.set_quadratic_coefficient(x, x, 2.1)

    def test_as_quadratic_expression_with_linear_no_crash(self) -> None:
        mod = model.Model()
        obj = mod.add_auxiliary_objective(priority=1)
        y = mod.add_variable()
        obj.offset = 2.1
        obj.set_linear_coefficient(y, 3.1)

        quad_expr = obj.as_quadratic_expression()

        self.assertEqual(quad_expr.offset, 2.1)
        self.assertDictEqual(dict(quad_expr.linear_terms), {y: 3.1})
        self.assertEmpty(list(quad_expr.quadratic_terms))

    def test_add_quadratic_errors(self) -> None:
        mod = model.Model()
        obj = mod.add_auxiliary_objective(priority=1)
        x = mod.add_variable()

        with self.assertRaisesRegex(ValueError, "Quadratic"):
            obj.add_quadratic(x * x)

    def test_add_is_error_if_quad(self) -> None:
        mod = model.Model()
        obj = mod.add_auxiliary_objective(priority=1)
        x = mod.add_variable()

        with self.assertRaisesRegex(ValueError, "Quadratic"):
            obj.add(x * x)

    def test_set_to_quadratic_expression_error(self) -> None:
        mod = model.Model()
        obj = mod.add_auxiliary_objective(priority=1)
        x = mod.add_variable()

        with self.assertRaisesRegex(ValueError, "Quadratic"):
            obj.set_to_quadratic_expression(x * x)

    def test_set_to_expression_error_when_quadratic(self) -> None:
        mod = model.Model()
        obj = mod.add_auxiliary_objective(priority=1)
        x = mod.add_variable()

        with self.assertRaisesRegex(ValueError, "Quadratic"):
            obj.set_to_expression(x * x)


if __name__ == "__main__":
    absltest.main()
