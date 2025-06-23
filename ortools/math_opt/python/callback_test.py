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
import math

from absl.testing import absltest
from ortools.math_opt import callback_pb2
from ortools.math_opt import sparse_containers_pb2
from ortools.math_opt.python import callback
from ortools.math_opt.python import model
from ortools.math_opt.python import sparse_containers
from ortools.math_opt.python.testing import compare_proto


class CallbackDataTest(compare_proto.MathOptProtoAssertions, absltest.TestCase):

  def test_parse_callback_data_no_solution(self) -> None:
    mod = model.Model(name="test_model")
    cb_data_proto = callback_pb2.CallbackDataProto(
        event=callback_pb2.CALLBACK_EVENT_PRESOLVE
    )
    cb_data_proto.runtime.FromTimedelta(datetime.timedelta(seconds=16.0))
    cb_data_proto.presolve_stats.removed_variables = 10
    cb_data_proto.simplex_stats.iteration_count = 3
    cb_data_proto.barrier_stats.primal_objective = 2.0
    cb_data_proto.mip_stats.open_nodes = 5
    cb_data = callback.parse_callback_data(cb_data_proto, mod)
    self.assertEqual(cb_data.event, callback.Event.PRESOLVE)
    self.assertIsNone(cb_data.solution)
    self.assertEqual(16.0, cb_data.runtime.seconds)
    self.assert_protos_equiv(
        cb_data.presolve_stats,
        callback_pb2.CallbackDataProto.PresolveStats(removed_variables=10),
    )
    self.assert_protos_equiv(
        cb_data.simplex_stats,
        callback_pb2.CallbackDataProto.SimplexStats(iteration_count=3),
    )
    self.assert_protos_equiv(
        cb_data.barrier_stats,
        callback_pb2.CallbackDataProto.BarrierStats(primal_objective=2.0),
    )
    self.assert_protos_equiv(
        cb_data.mip_stats, callback_pb2.CallbackDataProto.MipStats(open_nodes=5)
    )

  def test_parse_callback_data_with_solution(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    y = mod.add_binary_variable(name="y")
    cb_data_proto = callback_pb2.CallbackDataProto(
        event=callback_pb2.CALLBACK_EVENT_MIP_SOLUTION
    )
    solution = cb_data_proto.primal_solution_vector
    solution.ids[:] = [0, 1]
    solution.values[:] = [0.0, 1.0]
    cb_data_proto.runtime.FromTimedelta(datetime.timedelta(seconds=12.0))
    cb_data = callback.parse_callback_data(cb_data_proto, mod)
    self.assertEqual(cb_data.event, callback.Event.MIP_SOLUTION)
    self.assertDictEqual(cb_data.solution, {x: 0.0, y: 1.0})
    self.assertListEqual(cb_data.messages, [])
    self.assertEqual(12.0, cb_data.runtime.seconds)
    self.assert_protos_equiv(
        cb_data.presolve_stats, callback_pb2.CallbackDataProto.PresolveStats()
    )
    self.assert_protos_equiv(
        cb_data.simplex_stats, callback_pb2.CallbackDataProto.SimplexStats()
    )
    self.assert_protos_equiv(
        cb_data.barrier_stats, callback_pb2.CallbackDataProto.BarrierStats()
    )
    self.assert_protos_equiv(
        cb_data.mip_stats, callback_pb2.CallbackDataProto.MipStats()
    )


class CallbackRegistrationTest(
    compare_proto.MathOptProtoAssertions, absltest.TestCase
):

  def testToProto(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    mod.add_binary_variable(name="y")
    z = mod.add_binary_variable(name="z")

    reg = callback.CallbackRegistration()
    reg.events = {callback.Event.MIP_SOLUTION, callback.Event.MIP_NODE}
    reg.mip_node_filter = sparse_containers.VariableFilter(
        filtered_items=(z, x)
    )
    reg.mip_solution_filter = sparse_containers.VariableFilter(
        skip_zero_values=True
    )
    reg.add_lazy_constraints = True
    reg.add_cuts = False

    self.assert_protos_equiv(
        reg.to_proto(),
        callback_pb2.CallbackRegistrationProto(
            request_registration=[
                callback_pb2.CALLBACK_EVENT_MIP_SOLUTION,
                callback_pb2.CALLBACK_EVENT_MIP_NODE,
            ],
            mip_node_filter=sparse_containers_pb2.SparseVectorFilterProto(
                filter_by_ids=True, filtered_ids=[0, 2]
            ),
            mip_solution_filter=sparse_containers_pb2.SparseVectorFilterProto(
                skip_zero_values=True
            ),
            add_lazy_constraints=True,
            add_cuts=False,
        ),
    )


class GeneratedLinearConstraintTest(
    compare_proto.MathOptProtoAssertions, absltest.TestCase
):

  def testToProto(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    mod.add_binary_variable(name="y")
    z = mod.add_binary_variable(name="z")

    gen_con = callback.GeneratedConstraint()
    gen_con.terms = {x: 2.0, z: 4.0}
    gen_con.lower_bound = -math.inf
    gen_con.upper_bound = 5.0
    gen_con.is_lazy = True

    self.assert_protos_equiv(
        gen_con.to_proto(),
        callback_pb2.CallbackResultProto.GeneratedLinearConstraint(
            lower_bound=-math.inf,
            upper_bound=5.0,
            is_lazy=True,
            linear_expression=sparse_containers_pb2.SparseDoubleVectorProto(
                ids=[0, 2], values=[2.0, 4.0]
            ),
        ),
    )


class CallbackResultTest(
    compare_proto.MathOptProtoAssertions, absltest.TestCase
):

  def testToProto(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    y = mod.add_binary_variable(name="y")
    z = mod.add_binary_variable(name="z")

    result = callback.CallbackResult()
    result.terminate = True
    # Test le/ge combinations to avoid mutants.
    result.add_lazy_constraint(2 * x <= 0)
    result.add_lazy_constraint(2 * x >= 0)
    result.add_user_cut(2 * z >= 2)
    result.add_user_cut(2 * z <= 2)
    result.add_generated_constraint(expr=2 * z, lb=2, is_lazy=False)
    result.add_generated_constraint(expr=2 * z, ub=2, is_lazy=False)
    result.suggested_solutions.append({x: 1.0, y: 0.0, z: 1.0})
    result.suggested_solutions.append({x: 0.0, y: 0.0, z: 0.0})

    expected = callback_pb2.CallbackResultProto(
        terminate=True,
        cuts=[
            callback_pb2.CallbackResultProto.GeneratedLinearConstraint(
                lower_bound=-math.inf,
                upper_bound=0.0,
                is_lazy=True,
                linear_expression=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[0], values=[2.0]
                ),
            ),
            callback_pb2.CallbackResultProto.GeneratedLinearConstraint(
                lower_bound=0.0,
                upper_bound=math.inf,
                is_lazy=True,
                linear_expression=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[0], values=[2.0]
                ),
            ),
            callback_pb2.CallbackResultProto.GeneratedLinearConstraint(
                lower_bound=2.0,
                upper_bound=math.inf,
                is_lazy=False,
                linear_expression=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[2], values=[2.0]
                ),
            ),
            callback_pb2.CallbackResultProto.GeneratedLinearConstraint(
                lower_bound=-math.inf,
                upper_bound=2.0,
                is_lazy=False,
                linear_expression=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[2], values=[2.0]
                ),
            ),
            callback_pb2.CallbackResultProto.GeneratedLinearConstraint(
                lower_bound=2.0,
                upper_bound=math.inf,
                is_lazy=False,
                linear_expression=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[2], values=[2.0]
                ),
            ),
            callback_pb2.CallbackResultProto.GeneratedLinearConstraint(
                lower_bound=-math.inf,
                upper_bound=2.0,
                is_lazy=False,
                linear_expression=sparse_containers_pb2.SparseDoubleVectorProto(
                    ids=[2], values=[2.0]
                ),
            ),
        ],
        suggested_solutions=[
            sparse_containers_pb2.SparseDoubleVectorProto(
                ids=[0, 1, 2], values=[1.0, 0.0, 1.0]
            ),
            sparse_containers_pb2.SparseDoubleVectorProto(
                ids=[0, 1, 2], values=[0.0, 0.0, 0.0]
            ),
        ],
    )
    self.assert_protos_equiv(result.to_proto(), expected)

  def testConstraintErrors(self) -> None:
    mod = model.Model(name="test_model")
    x = mod.add_binary_variable(name="x")
    y = mod.add_binary_variable(name="y")
    z = mod.add_binary_variable(name="z")

    result = callback.CallbackResult()
    with self.assertRaisesRegex(
        TypeError,
        "unsupported operand.*\n.*two or more non-constant linear expressions",
    ):
      result.add_lazy_constraint(x <= (y <= z))
    with self.assertRaisesRegex(AssertionError, "lb cannot be specified.*"):
      result.add_user_cut(x + y == 1, lb=1)

  def testToProtoEmpty(self) -> None:
    result = callback.CallbackResult()
    self.assert_protos_equiv(
        result.to_proto(), callback_pb2.CallbackResultProto()
    )


if __name__ == "__main__":
  absltest.main()
