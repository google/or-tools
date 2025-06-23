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

"""Tests for adding and removing "Elements" (see Elemental) from the model."""

from collections.abc import Callable
import dataclasses
from typing import Generic, Iterator, Protocol, TypeVar, Union

from absl.testing import absltest
from absl.testing import parameterized
from ortools.math_opt.python import indicator_constraints
from ortools.math_opt.python import linear_constraints
from ortools.math_opt.python import model
from ortools.math_opt.python import objectives
from ortools.math_opt.python import quadratic_constraints
from ortools.math_opt.python import variables


T = TypeVar("T")


# We cannot use Callable here because we need to support a named argument.
class GetElement(Protocol, Generic[T]):

  def __call__(
      self, mod: model.Model, element_id: int, *, validate: bool = True
  ) -> T:
    pass


@dataclasses.dataclass(frozen=True)
class ElementAdapter(Generic[T]):
  add: Callable[[model.Model], T]
  delete: Callable[[model.Model, T], None]
  has: Callable[[model.Model, int], bool]
  get: GetElement[T]
  get_all: Callable[[model.Model], Iterator[T]]
  num: Callable[[model.Model], int]
  next_id: Callable[[model.Model], int]
  ensure_next_id: Callable[[model.Model, int], None]


_VARIABLE_ADAPTER = ElementAdapter[variables.Variable](
    add=model.Model.add_variable,
    delete=model.Model.delete_variable,
    has=model.Model.has_variable,
    get=model.Model.get_variable,
    get_all=model.Model.variables,
    num=model.Model.get_num_variables,
    next_id=model.Model.get_next_variable_id,
    ensure_next_id=model.Model.ensure_next_variable_id_at_least,
)

_LINEAR_CONSTRAINT_ADAPTER = ElementAdapter[
    linear_constraints.LinearConstraint
](
    add=model.Model.add_linear_constraint,
    delete=model.Model.delete_linear_constraint,
    has=model.Model.has_linear_constraint,
    get=model.Model.get_linear_constraint,
    get_all=model.Model.linear_constraints,
    num=model.Model.get_num_linear_constraints,
    next_id=model.Model.get_next_linear_constraint_id,
    ensure_next_id=model.Model.ensure_next_linear_constraint_id_at_least,
)


def _aux_add(mod: model.Model) -> objectives.AuxiliaryObjective:
  return mod.add_auxiliary_objective(priority=1)


_AUX_OBJECTIVE_ADAPTER = ElementAdapter[objectives.AuxiliaryObjective](
    add=_aux_add,
    delete=model.Model.delete_auxiliary_objective,
    has=model.Model.has_auxiliary_objective,
    get=model.Model.get_auxiliary_objective,
    get_all=model.Model.auxiliary_objectives,
    num=model.Model.num_auxiliary_objectives,
    next_id=model.Model.next_auxiliary_objective_id,
    ensure_next_id=model.Model.ensure_next_auxiliary_objective_id_at_least,
)

_QUADRATIC_CONSTRAINT_ADAPTER = ElementAdapter[
    quadratic_constraints.QuadraticConstraint
](
    add=model.Model.add_quadratic_constraint,
    delete=model.Model.delete_quadratic_constraint,
    has=model.Model.has_quadratic_constraint,
    get=model.Model.get_quadratic_constraint,
    get_all=model.Model.get_quadratic_constraints,
    num=model.Model.get_num_quadratic_constraints,
    next_id=model.Model.get_next_quadratic_constraint_id,
    ensure_next_id=model.Model.ensure_next_quadratic_constraint_id_at_least,
)

_INDICTOR_CONSTRAINT_ADAPTER = ElementAdapter[
    indicator_constraints.IndicatorConstraint
](
    add=model.Model.add_indicator_constraint,
    delete=model.Model.delete_indicator_constraint,
    has=model.Model.has_indicator_constraint,
    get=model.Model.get_indicator_constraint,
    get_all=model.Model.get_indicator_constraints,
    num=model.Model.get_num_indicator_constraints,
    next_id=model.Model.get_next_indicator_constraint_id,
    ensure_next_id=model.Model.ensure_next_indicator_constraint_id_at_least,
)

_ADAPTER = Union[
    ElementAdapter[variables.Variable],
    ElementAdapter[linear_constraints.LinearConstraint],
    ElementAdapter[objectives.AuxiliaryObjective],
    ElementAdapter[quadratic_constraints.QuadraticConstraint],
    ElementAdapter[indicator_constraints.IndicatorConstraint],
]


@parameterized.named_parameters(
    ("variable", _VARIABLE_ADAPTER),
    ("linear_constraint", _LINEAR_CONSTRAINT_ADAPTER),
    ("auxiliary_objective", _AUX_OBJECTIVE_ADAPTER),
    ("quadratic_constraint", _QUADRATIC_CONSTRAINT_ADAPTER),
    ("indicator_constraint", _INDICTOR_CONSTRAINT_ADAPTER),
)
class ModelElementTest(parameterized.TestCase):

  def test_no_elements(self, element_adapter: _ADAPTER) -> None:
    mod = model.Model()
    self.assertFalse(element_adapter.has(mod, 0))
    self.assertEqual(element_adapter.next_id(mod), 0)
    self.assertEqual(element_adapter.num(mod), 0)
    self.assertEmpty(list(element_adapter.get_all(mod)))

  def test_add_element(self, element_adapter: _ADAPTER) -> None:
    mod = model.Model()
    e0 = element_adapter.add(mod)
    e1 = element_adapter.add(mod)
    e2 = element_adapter.add(mod)

    self.assertTrue(element_adapter.has(mod, 0))
    self.assertTrue(element_adapter.has(mod, 1))
    self.assertTrue(element_adapter.has(mod, 2))
    self.assertFalse(element_adapter.has(mod, 3))

    self.assertEqual(element_adapter.next_id(mod), 3)
    self.assertEqual(element_adapter.num(mod), 3)
    self.assertEqual(list(element_adapter.get_all(mod)), [e0, e1, e2])

    self.assertEqual(element_adapter.get(mod, 1), e1)

  def test_get_invalid_element(self, element_adapter: _ADAPTER) -> None:
    mod = model.Model()
    with self.assertRaises(KeyError):
      element_adapter.get(mod, 0, validate=True)
    # Check that default for validate is True as well
    with self.assertRaises(KeyError):
      element_adapter.get(mod, 0)

    # No crash
    bad_el = element_adapter.get(mod, 0, validate=False)
    del bad_el

  def test_delete_element(self, element_adapter: _ADAPTER) -> None:
    mod = model.Model()
    e0 = element_adapter.add(mod)
    e1 = element_adapter.add(mod)
    e2 = element_adapter.add(mod)

    element_adapter.delete(mod, e1)

    self.assertTrue(element_adapter.has(mod, 0))
    self.assertFalse(element_adapter.has(mod, 1))
    self.assertTrue(element_adapter.has(mod, 2))
    self.assertFalse(element_adapter.has(mod, 3))

    self.assertEqual(element_adapter.next_id(mod), 3)
    self.assertEqual(element_adapter.num(mod), 2)
    self.assertEqual(list(element_adapter.get_all(mod)), [e0, e2])

    self.assertEqual(element_adapter.get(mod, 2), e2)

  def test_delete_invalid_element_error(
      self, element_adapter: _ADAPTER
  ) -> None:
    mod = model.Model()
    bad_el = element_adapter.get(mod, 0, validate=False)
    with self.assertRaises(ValueError):
      element_adapter.delete(mod, bad_el)

  def test_delete_element_twice_error(self, element_adapter: _ADAPTER) -> None:
    mod = model.Model()
    el = element_adapter.add(mod)
    element_adapter.delete(mod, el)
    with self.assertRaises(ValueError):
      element_adapter.delete(mod, el)

  def test_delete_element_wrong_model_error(
      self, element_adapter: _ADAPTER
  ) -> None:
    mod1 = model.Model()
    element_adapter.add(mod1)

    mod2 = model.Model()
    e2 = element_adapter.add(mod2)

    with self.assertRaises(ValueError):
      element_adapter.delete(mod1, e2)

  def test_get_deleted_element_error(self, element_adapter: _ADAPTER) -> None:
    mod = model.Model()
    el = element_adapter.add(mod)
    element_adapter.delete(mod, el)
    with self.assertRaises(KeyError):
      element_adapter.get(mod, 0, validate=True)

    # No crash
    bad_el = element_adapter.get(mod, 0, validate=False)
    del bad_el

  def test_ensure_next_id_with_effect(self, element_adapter: _ADAPTER) -> None:
    mod = model.Model()
    element_adapter.ensure_next_id(mod, 6)

    self.assertEqual(element_adapter.next_id(mod), 6)
    self.assertFalse(element_adapter.has(mod, 0))
    self.assertFalse(element_adapter.has(mod, 6))
    self.assertEqual(element_adapter.num(mod), 0)
    self.assertEmpty(list(element_adapter.get_all(mod)))

    e6 = element_adapter.add(mod)
    e7 = element_adapter.add(mod)

    self.assertFalse(element_adapter.has(mod, 0))
    self.assertTrue(element_adapter.has(mod, 6))
    self.assertTrue(element_adapter.has(mod, 7))
    self.assertFalse(element_adapter.has(mod, 8))

    self.assertEqual(element_adapter.next_id(mod), 8)
    self.assertEqual(element_adapter.num(mod), 2)
    self.assertEqual(list(element_adapter.get_all(mod)), [e6, e7])
    self.assertEqual(element_adapter.get(mod, 6), e6)
    self.assertEqual(element_adapter.get(mod, 7), e7)

  def test_ensure_next_id_no_effect(self, element_adapter: _ADAPTER) -> None:
    mod = model.Model()
    e0 = element_adapter.add(mod)
    e1 = element_adapter.add(mod)
    e2 = element_adapter.add(mod)

    element_adapter.ensure_next_id(mod, 1)

    self.assertEqual(element_adapter.next_id(mod), 3)
    self.assertEqual(element_adapter.num(mod), 3)
    self.assertEqual(list(element_adapter.get_all(mod)), [e0, e1, e2])

    e3 = element_adapter.add(mod)
    self.assertEqual(element_adapter.next_id(mod), 4)
    self.assertEqual(element_adapter.num(mod), 4)
    self.assertEqual(list(element_adapter.get_all(mod)), [e0, e1, e2, e3])
    self.assertEqual(element_adapter.get(mod, 3), e3)


if __name__ == "__main__":
  absltest.main()
