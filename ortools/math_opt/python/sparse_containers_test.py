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

from absl.testing import absltest
from ortools.math_opt import sparse_containers_pb2
from ortools.math_opt.python import model
from ortools.math_opt.python import sparse_containers
from ortools.math_opt.python.testing import compare_proto


class SparseDoubleVectorTest(compare_proto.MathOptProtoAssertions, absltest.TestCase):

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

    def test_parse_var_map(self) -> None:
        mod = model.Model(name="test_model")
        x = mod.add_binary_variable(name="x")
        mod.add_binary_variable(name="y")
        z = mod.add_binary_variable(name="z")
        actual = sparse_containers.parse_variable_map(
            sparse_containers_pb2.SparseDoubleVectorProto(
                ids=[0, 2], values=[1.0, 4.0]
            ),
            mod,
        )
        self.assertDictEqual(actual, {x: 1.0, z: 4.0})

    def test_parse_var_map_empty(self) -> None:
        mod = model.Model(name="test_model")
        mod.add_binary_variable(name="x")
        mod.add_binary_variable(name="y")
        mod.add_binary_variable(name="z")
        actual = sparse_containers.parse_variable_map(
            sparse_containers_pb2.SparseDoubleVectorProto(), mod
        )
        self.assertDictEqual(actual, {})

    def test_parse_lin_con_map(self) -> None:
        mod = model.Model(name="test_model")
        mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
        d = mod.add_linear_constraint(lb=0.0, ub=1.0, name="d")
        e = mod.add_linear_constraint(lb=0.0, ub=1.0, name="e")
        actual = sparse_containers.parse_linear_constraint_map(
            sparse_containers_pb2.SparseDoubleVectorProto(
                ids=[1, 2], values=[5.0, 4.0]
            ),
            mod,
        )
        self.assertDictEqual(actual, {d: 5.0, e: 4.0})

    def test_parse_lin_con_map_empty(self) -> None:
        mod = model.Model(name="test_model")
        mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
        mod.add_linear_constraint(lb=0.0, ub=1.0, name="d")
        mod.add_linear_constraint(lb=0.0, ub=1.0, name="e")
        actual = sparse_containers.parse_linear_constraint_map(
            sparse_containers_pb2.SparseDoubleVectorProto(), mod
        )
        self.assertDictEqual(actual, {})


class SparseInt32VectorTest(compare_proto.MathOptProtoAssertions, absltest.TestCase):

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


class SparseVectorFilterTest(compare_proto.MathOptProtoAssertions, absltest.TestCase):

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
