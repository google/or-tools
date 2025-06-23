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

from collections.abc import Callable
import dataclasses
from typing import Dict, Generic, Protocol, TypeVar, Union

from absl.testing import absltest
from absl.testing import parameterized
from ortools.math_opt import sparse_containers_pb2
from ortools.math_opt.python import linear_constraints
from ortools.math_opt.python import model
from ortools.math_opt.python import quadratic_constraints
from ortools.math_opt.python import sparse_containers
from ortools.math_opt.python import variables
from ortools.math_opt.python.testing import compare_proto


class SparseDoubleVectorTest(
    compare_proto.MathOptProtoAssertions, absltest.TestCase
):

  def test_to_proto_empty(self) -> None:
    actual = sparse_containers.to_sparse_double_vector_proto({})
    self.assert_protos_equiv(
        actual, sparse_containers_pb2.SparseDoubleVectorProto()
    )

  def test_to_proto_vars(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    mod.add_binary_variable(name="y")
    z = mod.add_binary_variable(name="z")
    self.assert_protos_equiv(
        sparse_containers.to_sparse_double_vector_proto({z: 4.0, x: 1.0}),
        sparse_containers_pb2.SparseDoubleVectorProto(
            ids=[0, 2], values=[1.0, 4.0]
        ),
    )

  def test_to_proto_lin_cons(self) -> None:
    mod = model.Model(name="test_model")
    c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
    d = mod.add_linear_constraint(lb=0.0, ub=1.0, name="d")
    self.assert_protos_equiv(
        sparse_containers.to_sparse_double_vector_proto({c: 4.0, d: 1.0}),
        sparse_containers_pb2.SparseDoubleVectorProto(
            ids=[0, 1], values=[4.0, 1.0]
        ),
    )


T = TypeVar("T")


# We cannot use Callable here because we need to support a named argument.
class ParseMap(Protocol, Generic[T]):

  def __call__(
      self,
      vec: sparse_containers_pb2.SparseDoubleVectorProto,
      mod: model.Model,
      *,
      validate: bool = True,
  ) -> Dict[T, float]:
    ...


@dataclasses.dataclass(frozen=True)
class ParseMapAdapater(Generic[T]):
  add_element: Callable[[model.Model], T]
  get_element_no_validate: Callable[[model.Model, int], T]
  parse_map: ParseMap[T]


_VAR_ADAPTER = ParseMapAdapater(
    model.Model.add_variable,
    lambda mod, id: mod.get_variable(id, validate=False),
    sparse_containers.parse_variable_map,
)
_LIN_CON_ADAPTER = ParseMapAdapater(
    model.Model.add_linear_constraint,
    lambda mod, id: mod.get_linear_constraint(id, validate=False),
    sparse_containers.parse_linear_constraint_map,
)
_QUAD_CON_ADAPTER = ParseMapAdapater(
    model.Model.add_quadratic_constraint,
    lambda mod, id: mod.get_quadratic_constraint(id, validate=False),
    sparse_containers.parse_quadratic_constraint_map,
)

_ADAPTERS = Union[
    ParseMapAdapater[variables.Variable],
    ParseMapAdapater[linear_constraints.LinearConstraint],
    ParseMapAdapater[quadratic_constraints.QuadraticConstraint],
]


@parameterized.named_parameters(
    ("variable", _VAR_ADAPTER),
    ("linear_constraint", _LIN_CON_ADAPTER),
    ("quadratic_constraint", _QUAD_CON_ADAPTER),
)
class ParseVariableMapTest(
    compare_proto.MathOptProtoAssertions, parameterized.TestCase
):

  def test_parse_map(self, adapter: _ADAPTERS) -> None:
    mod = model.Model()
    x = adapter.add_element(mod)
    adapter.add_element(mod)
    z = adapter.add_element(mod)
    actual = adapter.parse_map(
        sparse_containers_pb2.SparseDoubleVectorProto(
            ids=[0, 2], values=[1.0, 4.0]
        ),
        mod,
    )
    self.assertDictEqual(actual, {x: 1.0, z: 4.0})

  def test_parse_map_empty(self, adapter: _ADAPTERS) -> None:
    mod = model.Model()
    adapter.add_element(mod)
    adapter.add_element(mod)
    actual = adapter.parse_map(
        sparse_containers_pb2.SparseDoubleVectorProto(), mod
    )
    self.assertDictEqual(actual, {})

  def test_parse_var_map_bad_var(self, adapter: _ADAPTERS) -> None:
    mod = model.Model()
    bad_proto = sparse_containers_pb2.SparseDoubleVectorProto(
        ids=[2], values=[4.0]
    )
    actual = adapter.parse_map(bad_proto, mod, validate=False)
    bad_elem = adapter.get_element_no_validate(mod, 2)
    self.assertDictEqual(actual, {bad_elem: 4.0})
    with self.assertRaises(KeyError):
      adapter.parse_map(bad_proto, mod, validate=True)


class SparseInt32VectorTest(
    compare_proto.MathOptProtoAssertions, absltest.TestCase
):

  def test_to_proto_empty(self) -> None:
    self.assert_protos_equiv(
        sparse_containers.to_sparse_int32_vector_proto({}),
        sparse_containers_pb2.SparseInt32VectorProto(),
    )

  def test_to_proto_vars(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    mod.add_binary_variable(name="y")
    z = mod.add_binary_variable(name="z")
    self.assert_protos_equiv(
        sparse_containers.to_sparse_int32_vector_proto({z: 4, x: 1}),
        sparse_containers_pb2.SparseInt32VectorProto(ids=[0, 2], values=[1, 4]),
    )

  def test_to_proto_lin_cons(self) -> None:
    mod = model.Model(name="test_model")
    c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
    d = mod.add_linear_constraint(lb=0.0, ub=1.0, name="d")
    self.assert_protos_equiv(
        sparse_containers.to_sparse_int32_vector_proto({c: 4, d: 1}),
        sparse_containers_pb2.SparseInt32VectorProto(ids=[0, 1], values=[4, 1]),
    )


class SparseVectorFilterTest(
    compare_proto.MathOptProtoAssertions, absltest.TestCase
):

  def test_is_none(self) -> None:
    f = sparse_containers.SparseVectorFilter(skip_zero_values=True)
    self.assertTrue(f.skip_zero_values)
    self.assertIsNone(f.filtered_items)
    expected_proto = sparse_containers_pb2.SparseVectorFilterProto(
        skip_zero_values=True
    )
    self.assert_protos_equiv(f.to_proto(), expected_proto)

  def test_ids_is_empty(self) -> None:
    f = sparse_containers.SparseVectorFilter(filtered_items=[])
    self.assertFalse(f.skip_zero_values)
    self.assertEmpty(f.filtered_items)
    expected_proto = sparse_containers_pb2.SparseVectorFilterProto(
        filter_by_ids=True
    )
    self.assert_protos_equiv(f.to_proto(), expected_proto)

  def test_ids_are_lin_cons(self) -> None:
    mod = model.Model(name="test_model")
    mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
    d = mod.add_linear_constraint(lb=0.0, ub=1.0, name="d")
    f = sparse_containers.LinearConstraintFilter(
        skip_zero_values=True, filtered_items=[d]
    )
    self.assertTrue(f.skip_zero_values)
    self.assertSetEqual(f.filtered_items, {d})
    expected_proto = sparse_containers_pb2.SparseVectorFilterProto(
        skip_zero_values=True, filter_by_ids=True, filtered_ids=[1]
    )
    self.assert_protos_equiv(f.to_proto(), expected_proto)

  def test_ids_are_vars(self) -> None:
    mod = model.Model(name="test_model")
    w = mod.add_binary_variable(name="w")
    x = mod.add_binary_variable(name="x")
    mod.add_binary_variable(name="y")
    z = mod.add_binary_variable(name="z")
    f = sparse_containers.VariableFilter(filtered_items=(z, w, x))
    self.assertFalse(f.skip_zero_values)
    self.assertSetEqual(f.filtered_items, {w, x, z})
    expected_proto = sparse_containers_pb2.SparseVectorFilterProto(
        filter_by_ids=True, filtered_ids=[0, 1, 3]
    )
    self.assert_protos_equiv(f.to_proto(), expected_proto)


if __name__ == "__main__":
  absltest.main()
