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

import datetime

from google.protobuf import duration_pb2
from absl.testing import absltest
from ortools.math_opt import model_parameters_pb2
from ortools.math_opt import solution_pb2
from ortools.math_opt import sparse_containers_pb2
from ortools.math_opt.python import model
from ortools.math_opt.python import model_parameters
from ortools.math_opt.python import solution
from ortools.math_opt.python import sparse_containers
from ortools.math_opt.python.testing import compare_proto


class ModelParametersTest(
    compare_proto.MathOptProtoAssertions, absltest.TestCase
):

  def test_solution_hint_round_trip(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    y = mod.add_binary_variable(name="y")
    c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
    d = mod.add_linear_constraint(lb=0.0, ub=1.0, name="d")

    hint = model_parameters.SolutionHint(
        variable_values={x: 2.0, y: 3.0}, dual_values={c: 4.0, d: 5.0}
    )
    hint_round_trip = model_parameters.parse_solution_hint(hint.to_proto(), mod)
    self.assertDictEqual(hint_round_trip.variable_values, hint.variable_values)
    self.assertDictEqual(hint_round_trip.dual_values, hint.dual_values)

  def test_objective_parameters_empty_round_trip(self) -> None:
    params = model_parameters.ObjectiveParameters()
    proto = model_parameters_pb2.ObjectiveParametersProto()
    self.assert_protos_equiv(params.to_proto(), proto)
    self.assertEqual(model_parameters.parse_objective_parameters(proto), params)

  def test_objective_parameters_full_round_trip(self) -> None:
    params = model_parameters.ObjectiveParameters(
        objective_degradation_absolute_tolerance=4.1,
        objective_degradation_relative_tolerance=4.2,
        time_limit=datetime.timedelta(minutes=1),
    )
    proto = model_parameters_pb2.ObjectiveParametersProto(
        objective_degradation_absolute_tolerance=4.1,
        objective_degradation_relative_tolerance=4.2,
        time_limit=duration_pb2.Duration(seconds=60),
    )
    self.assert_protos_equiv(params.to_proto(), proto)
    self.assertEqual(model_parameters.parse_objective_parameters(proto), params)

  def test_model_parameters_to_proto_no_basis(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    y = mod.add_binary_variable(name="y")
    c = mod.add_linear_constraint(lb=0.0, ub=1.0, name="c")
    # Ensure q and c have different ids.
    mod.add_quadratic_constraint()
    q = mod.add_quadratic_constraint(name="q")
    params = model_parameters.ModelSolveParameters()
    params.variable_values_filter = sparse_containers.SparseVectorFilter(
        filtered_items=(y,)
    )
    params.reduced_costs_filter = sparse_containers.SparseVectorFilter(
        skip_zero_values=True
    )
    params.dual_values_filter = sparse_containers.SparseVectorFilter(
        filtered_items=(c,)
    )
    params.quadratic_dual_values_filter = sparse_containers.SparseVectorFilter(
        filtered_items=(q,)
    )
    params.solution_hints.append(
        model_parameters.SolutionHint(
            variable_values={x: 1.0, y: 1.0}, dual_values={c: 3.0}
        )
    )
    params.solution_hints.append(
        model_parameters.SolutionHint(variable_values={y: 0.0})
    )
    params.branching_priorities[y] = 2
    actual = params.to_proto()
    expected = model_parameters_pb2.ModelSolveParametersProto(
        variable_values_filter=sparse_containers_pb2.SparseVectorFilterProto(
            filter_by_ids=True, filtered_ids=(1,)
        ),
        reduced_costs_filter=sparse_containers_pb2.SparseVectorFilterProto(
            skip_zero_values=True
        ),
        dual_values_filter=sparse_containers_pb2.SparseVectorFilterProto(
            filter_by_ids=True, filtered_ids=(0,)
        ),
        quadratic_dual_values_filter=sparse_containers_pb2.SparseVectorFilterProto(
            filter_by_ids=True, filtered_ids=(1,)
        ),
        branching_priorities=sparse_containers_pb2.SparseInt32VectorProto(
            ids=[1], values=[2]
        ),
    )
    h1 = expected.solution_hints.add()
    h1.variable_values.ids[:] = [0, 1]
    h1.variable_values.values[:] = [1.0, 1.0]
    h1.dual_values.ids[:] = [0]
    h1.dual_values.values[:] = [3]
    h2 = expected.solution_hints.add()
    h2.variable_values.ids.append(1)
    h2.variable_values.values.append(0.0)
    self.assert_protos_equiv(actual, expected)

  def test_model_parameters_to_proto_with_basis(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    params = model_parameters.ModelSolveParameters()
    params.initial_basis = solution.Basis()
    params.initial_basis.variable_status[x] = (
        solution.BasisStatus.AT_UPPER_BOUND
    )
    actual = params.to_proto()
    expected = model_parameters_pb2.ModelSolveParametersProto()
    expected.initial_basis.variable_status.ids.append(0)
    expected.initial_basis.variable_status.values.append(
        solution_pb2.BASIS_STATUS_AT_UPPER_BOUND
    )
    self.assert_protos_equiv(expected, actual)

  def test_model_parameters_to_proto_with_objective_params(self) -> None:
    mod = model.Model()
    aux1 = mod.add_auxiliary_objective(priority=1)
    mod.add_auxiliary_objective(priority=2)
    aux3 = mod.add_auxiliary_objective(priority=3)

    def make_param(abs_tol: float) -> model_parameters.ObjectiveParameters:
      return model_parameters.ObjectiveParameters(
          objective_degradation_absolute_tolerance=abs_tol
      )

    def make_proto_param(
        abs_tol: float,
    ) -> model_parameters_pb2.ObjectiveParametersProto:
      return model_parameters_pb2.ObjectiveParametersProto(
          objective_degradation_absolute_tolerance=abs_tol
      )

    model_params = model_parameters.ModelSolveParameters(
        objective_parameters={
            mod.objective: make_param(0.1),
            aux1: make_param(0.2),
            aux3: make_param(0.3),
        }
    )
    expected = model_parameters_pb2.ModelSolveParametersProto(
        primary_objective_parameters=make_proto_param(0.1),
        auxiliary_objective_parameters={
            0: make_proto_param(0.2),
            2: make_proto_param(0.3),
        },
    )
    self.assert_protos_equiv(model_params.to_proto(), expected)

  def test_model_parameters_to_proto_with_lazy_constraints(self) -> None:
    mod = model.Model()
    c0 = mod.add_linear_constraint()
    mod.add_linear_constraint()
    c2 = mod.add_linear_constraint()
    model_params = model_parameters.ModelSolveParameters(
        lazy_linear_constraints={c0, c2}
    )
    expected = model_parameters_pb2.ModelSolveParametersProto(
        lazy_linear_constraint_ids=[0, 2]
    )
    self.assert_protos_equiv(model_params.to_proto(), expected)


if __name__ == "__main__":
  absltest.main()
