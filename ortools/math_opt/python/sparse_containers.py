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

"""Sparse vectors and matrices using variables and constraints from Model.

Analogous to sparse_containers.proto, with bidirectional conversion.
"""
from typing import Dict, FrozenSet, Generic, Iterable, Mapping, Optional, Set, TypeVar

from ortools.math_opt import sparse_containers_pb2
from ortools.math_opt.python import model


VarOrConstraintType = TypeVar(
    "VarOrConstraintType", model.Variable, model.LinearConstraint
)


def to_sparse_double_vector_proto(
    terms: Mapping[VarOrConstraintType, float]
) -> sparse_containers_pb2.SparseDoubleVectorProto:
    """Converts a sparse vector from proto to dict representation."""
    result = sparse_containers_pb2.SparseDoubleVectorProto()
    if terms:
        id_and_values = [(key.id, value) for (key, value) in terms.items()]
        id_and_values.sort()
        ids, values = zip(*id_and_values)
        result.ids[:] = ids
        result.values[:] = values
    return result


def to_sparse_int32_vector_proto(
    terms: Mapping[VarOrConstraintType, int]
) -> sparse_containers_pb2.SparseInt32VectorProto:
    """Converts a sparse vector from proto to dict representation."""
    result = sparse_containers_pb2.SparseInt32VectorProto()
    if terms:
        id_and_values = [(key.id, value) for (key, value) in terms.items()]
        id_and_values.sort()
        ids, values = zip(*id_and_values)
        result.ids[:] = ids
        result.values[:] = values
    return result


def parse_variable_map(
    proto: sparse_containers_pb2.SparseDoubleVectorProto, mod: model.Model
) -> Dict[model.Variable, float]:
    """Converts a sparse vector of variables from proto to dict representation."""
    result = {}
    for index, var_id in enumerate(proto.ids):
        result[mod.get_variable(var_id)] = proto.values[index]
    return result


def parse_linear_constraint_map(
    proto: sparse_containers_pb2.SparseDoubleVectorProto, mod: model.Model
) -> Dict[model.LinearConstraint, float]:
    """Converts a sparse vector of linear constraints from proto to dict representation."""
    result = {}
    for index, lin_con_id in enumerate(proto.ids):
        result[mod.get_linear_constraint(lin_con_id)] = proto.values[index]
    return result


class SparseVectorFilter(Generic[VarOrConstraintType]):
    """Restricts the variables or constraints returned in a sparse vector.

    The default behavior is to return entries for all variables/constraints.

    E.g. when requesting the solution to an optimization problem, use this class
    to restrict the variables that values are returned for.

    Attributes:
      skip_zero_values: Do not include key value pairs with value zero.
      filtered_items: If not None, include only key value pairs these keys. Note
        that the empty set is different (don't return any keys) from None (return
        all keys).
    """

    def __init__(
        self,
        *,
        skip_zero_values: bool = False,
        filtered_items: Optional[Iterable[VarOrConstraintType]] = None,
    ):
        self._skip_zero_values: bool = skip_zero_values
        self._filtered_items: Optional[Set[VarOrConstraintType]] = (
            None if filtered_items is None else frozenset(filtered_items)
        )  # pytype: disable=annotation-type-mismatch  # attribute-variable-annotations

    @property
    def skip_zero_values(self) -> bool:
        return self._skip_zero_values

    @property
    def filtered_items(self) -> Optional[FrozenSet[VarOrConstraintType]]:
        return (
            self._filtered_items
        )  # pytype: disable=bad-return-type  # attribute-variable-annotations

    def to_proto(self):
        """Returns an equivalent proto representation."""
        result = sparse_containers_pb2.SparseVectorFilterProto()
        result.skip_zero_values = self._skip_zero_values
        if self._filtered_items is not None:
            result.filter_by_ids = True
            result.filtered_ids[:] = sorted(t.id for t in self._filtered_items)
        return result


VariableFilter = SparseVectorFilter[model.Variable]
LinearConstraintFilter = SparseVectorFilter[model.LinearConstraint]
