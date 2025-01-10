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

"""Tests for compute_infeasible_subsystem_result.py."""

from absl.testing import absltest
from ortools.math_opt import infeasible_subsystem_pb2
from ortools.math_opt.python import compute_infeasible_subsystem_result
from ortools.math_opt.python import model
from ortools.math_opt.python import result
from ortools.math_opt.python.testing import compare_proto

_ModelSubsetBounds = compute_infeasible_subsystem_result.ModelSubsetBounds
_ModelSubset = compute_infeasible_subsystem_result.ModelSubset
_ComputeInfeasibleSubsystemResult = (
    compute_infeasible_subsystem_result.ComputeInfeasibleSubsystemResult
)


class ModelSubsetBoundsTest(absltest.TestCase, compare_proto.MathOptProtoAssertions):

    def test_empty(self) -> None:
        self.assertTrue(_ModelSubsetBounds().empty())
        self.assertFalse(_ModelSubsetBounds(lower=True).empty())
        self.assertFalse(_ModelSubsetBounds(upper=True).empty())

    def test_proto(self) -> None:
        start_bounds = _ModelSubsetBounds(lower=True)
        self.assert_protos_equiv(
            start_bounds.to_proto(),
            infeasible_subsystem_pb2.ModelSubsetProto.Bounds(lower=True),
        )

    def test_proto_round_trip_lower(self) -> None:
        start_bounds = _ModelSubsetBounds(lower=True)
        self.assertEqual(
            compute_infeasible_subsystem_result.parse_model_subset_bounds(
                start_bounds.to_proto()
            ),
            start_bounds,
        )

    def test_proto_round_trip_upper(self) -> None:
        start_bounds = _ModelSubsetBounds(upper=True)
        self.assertEqual(
            compute_infeasible_subsystem_result.parse_model_subset_bounds(
                start_bounds.to_proto()
            ),
            start_bounds,
        )


class ModelSubsetTest(absltest.TestCase, compare_proto.MathOptProtoAssertions):

    def test_empty(self) -> None:
        m = model.Model()
        x = m.add_binary_variable()
        c = m.add_linear_constraint()
        self.assertTrue(_ModelSubset().empty())
        self.assertFalse(_ModelSubset(variable_integrality=frozenset((x,))).empty())
        self.assertFalse(
            _ModelSubset(variable_bounds={x: _ModelSubsetBounds(lower=True)}).empty()
        )
        self.assertFalse(
            _ModelSubset(linear_constraints={c: _ModelSubsetBounds(upper=True)}).empty()
        )

    def test_to_proto(self) -> None:
        m = model.Model()
        x = m.add_binary_variable()
        y = m.add_binary_variable()
        c = m.add_linear_constraint()
        d = m.add_linear_constraint()
        model_subset = _ModelSubset(
            variable_integrality=frozenset((x, y)),
            variable_bounds={y: _ModelSubsetBounds(upper=True)},
            linear_constraints={
                c: _ModelSubsetBounds(upper=True),
                d: _ModelSubsetBounds(lower=True),
            },
        )
        expected = infeasible_subsystem_pb2.ModelSubsetProto()
        expected.variable_bounds[1].upper = True
        expected.variable_integrality[:] = [0, 1]
        expected.linear_constraints[0].upper = True
        expected.linear_constraints[1].lower = True
        self.assert_protos_equiv(model_subset.to_proto(), expected)

    def test_proto_round_trip_empty(self) -> None:
        m = model.Model()
        subset = _ModelSubset()
        self.assertEqual(
            compute_infeasible_subsystem_result.parse_model_subset(
                subset.to_proto(), m
            ),
            subset,
        )

    def test_proto_round_trip_full(self) -> None:
        m = model.Model()
        x = m.add_binary_variable()
        y = m.add_binary_variable()
        c = m.add_linear_constraint()
        d = m.add_linear_constraint()
        start_subset = _ModelSubset(
            variable_integrality=frozenset((x,)),
            variable_bounds={
                x: _ModelSubsetBounds(lower=True),
                y: _ModelSubsetBounds(upper=True),
            },
            linear_constraints={
                c: _ModelSubsetBounds(upper=True),
                d: _ModelSubsetBounds(lower=True),
            },
        )
        self.assertEqual(
            compute_infeasible_subsystem_result.parse_model_subset(
                start_subset.to_proto(), m
            ),
            start_subset,
        )

    def test_parse_proto_quadratic_constraint_unsupported(self) -> None:
        m = model.Model()
        model_subset = infeasible_subsystem_pb2.ModelSubsetProto()
        model_subset.quadratic_constraints[3].lower = True
        with self.assertRaisesRegex(NotImplementedError, "quadratic_constraints"):
            compute_infeasible_subsystem_result.parse_model_subset(model_subset, m)

    def test_parse_proto_second_order_cone_unsupported(self) -> None:
        m = model.Model()
        model_subset = infeasible_subsystem_pb2.ModelSubsetProto(
            second_order_cone_constraints=[2]
        )
        with self.assertRaisesRegex(
            NotImplementedError, "second_order_cone_constraints"
        ):
            compute_infeasible_subsystem_result.parse_model_subset(model_subset, m)

    def test_parse_proto_sos1_unsupported(self) -> None:
        m = model.Model()
        model_subset = infeasible_subsystem_pb2.ModelSubsetProto(sos1_constraints=[2])
        with self.assertRaisesRegex(NotImplementedError, "sos1_constraints"):
            compute_infeasible_subsystem_result.parse_model_subset(model_subset, m)

    def test_parse_proto_sos2_unsupported(self) -> None:
        m = model.Model()
        model_subset = infeasible_subsystem_pb2.ModelSubsetProto(sos2_constraints=[2])
        with self.assertRaisesRegex(NotImplementedError, "sos2_constraints"):
            compute_infeasible_subsystem_result.parse_model_subset(model_subset, m)

    def test_parse_proto_indicator_unsupported(self) -> None:
        m = model.Model()
        model_subset = infeasible_subsystem_pb2.ModelSubsetProto(
            indicator_constraints=[2]
        )
        with self.assertRaisesRegex(NotImplementedError, "indicator_constraints"):
            compute_infeasible_subsystem_result.parse_model_subset(model_subset, m)


class ComputeInfeasibleSubsystemResultTest(absltest.TestCase):

    def test_to_proto_round_trip(self) -> None:
        m = model.Model()
        x = m.add_binary_variable()
        iis_result = _ComputeInfeasibleSubsystemResult(
            feasibility=result.FeasibilityStatus.INFEASIBLE,
            is_minimal=True,
            infeasible_subsystem=_ModelSubset(variable_integrality=frozenset((x,))),
        )
        self.assertEqual(
            compute_infeasible_subsystem_result.parse_compute_infeasible_subsystem_result(
                iis_result.to_proto(), m
            ),
            iis_result,
        )

    def test_to_proto_round_trip_empty(self) -> None:
        m = model.Model()
        iis_result = _ComputeInfeasibleSubsystemResult(
            feasibility=result.FeasibilityStatus.UNDETERMINED
        )
        self.assertEqual(
            compute_infeasible_subsystem_result.parse_compute_infeasible_subsystem_result(
                iis_result.to_proto(), m
            ),
            iis_result,
        )


if __name__ == "__main__":
    absltest.main()
