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

"""Test normalize for mathopt protos."""

from absl.testing import absltest
from ortools.math_opt import model_parameters_pb2
from ortools.math_opt import model_pb2
from ortools.math_opt import model_update_pb2
from ortools.math_opt import parameters_pb2
from ortools.math_opt import result_pb2
from ortools.math_opt import sparse_containers_pb2
from ortools.math_opt.python import normalize
from ortools.math_opt.python.testing import compare_proto


class MathOptProtoAssertionsTest(
    compare_proto.MathOptProtoAssertions, absltest.TestCase
):

    def test_removes_empty_message(self) -> None:
        model_with_empty_vars = model_pb2.ModelProto()
        model_with_empty_vars.variables.SetInParent()
        with self.assertRaisesRegex(AssertionError, ".*variables.*"):
            self.assert_protos_equal(model_with_empty_vars, model_pb2.ModelProto())

        normalize.math_opt_normalize_proto(model_with_empty_vars)
        self.assert_protos_equal(model_with_empty_vars, model_pb2.ModelProto())

    def test_keeps_nonempty_message(self) -> None:
        model_with_vars = model_pb2.ModelProto()
        model_with_vars.variables.ids[:] = [1, 3]

        expected = model_pb2.ModelProto()
        expected.variables.ids[:] = [1, 3]

        normalize.math_opt_normalize_proto(model_with_vars)
        self.assert_protos_equal(model_with_vars, expected)

    def test_keeps_optional_scalar_at_default_message(self) -> None:
        objective = model_update_pb2.ObjectiveUpdatesProto(offset_update=0.0)

        normalize.math_opt_normalize_proto(objective)

        wrong = model_update_pb2.ObjectiveUpdatesProto()
        with self.assertRaisesRegex(AssertionError, ".*offset_update.*"):
            self.assert_protos_equal(objective, wrong)

        expected = model_update_pb2.ObjectiveUpdatesProto(offset_update=0.0)
        self.assert_protos_equal(objective, expected)

    def test_recursive_cleanup(self) -> None:
        update_rec_empty = model_update_pb2.ModelUpdateProto()
        update_rec_empty.variable_updates.lower_bounds.SetInParent()

        normalize.math_opt_normalize_proto(update_rec_empty)
        self.assert_protos_equal(update_rec_empty, model_update_pb2.ModelUpdateProto())

    def test_duration_no_cleanup(self) -> None:
        params = parameters_pb2.SolveParametersProto()
        params.time_limit.SetInParent()

        with self.assertRaisesRegex(AssertionError, ".*time_limit.*"):
            normalize.math_opt_normalize_proto(params)
            self.assert_protos_equal(params, parameters_pb2.SolveParametersProto())

    def test_repeated_scalar_no_cleanup(self) -> None:
        vec = sparse_containers_pb2.SparseDoubleVectorProto()
        vec.ids[:] = [0, 0]
        normalize.math_opt_normalize_proto(vec)

        expected = sparse_containers_pb2.SparseDoubleVectorProto()
        expected.ids[:] = [0, 0]

        self.assert_protos_equal(vec, expected)

    def test_reaches_into_map(self) -> None:
        model = model_pb2.ModelProto()
        model.quadratic_constraints[2].linear_terms.SetInParent()
        normalize.math_opt_normalize_proto(model)

        expected = model_pb2.ModelProto()
        expected.quadratic_constraints[2]  # pylint: disable=pointless-statement

        self.assert_protos_equal(model, expected)

    def test_reaches_into_vector(self) -> None:
        params = model_parameters_pb2.ModelSolveParametersProto()
        params.solution_hints.add().variable_values.SetInParent()
        normalize.math_opt_normalize_proto(params)

        expected = model_parameters_pb2.ModelSolveParametersProto()
        expected.solution_hints.add()

        self.assert_protos_equal(params, expected)

    def test_oneof_is_not_cleared(self) -> None:
        result = result_pb2.SolveResultProto()
        result.gscip_output.SetInParent()
        normalize.math_opt_normalize_proto(result)

        expected = result_pb2.SolveResultProto()
        expected.gscip_output.SetInParent()

        self.assert_protos_equal(result, expected)

    def test_reaches_into_oneof(self) -> None:
        result = result_pb2.SolveResultProto()
        result.gscip_output.stats.SetInParent()
        normalize.math_opt_normalize_proto(result)

        expected = result_pb2.SolveResultProto()
        expected.gscip_output.SetInParent()

        self.assert_protos_equal(result, expected)


if __name__ == "__main__":
    absltest.main()
