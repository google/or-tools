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
from ortools.math_opt import parameters_pb2
from ortools.math_opt.python import init_arguments
from ortools.math_opt.python.testing import compare_proto
from ortools.math_opt.solvers import gurobi_pb2


class GurobiISVKeyTest(absltest.TestCase, compare_proto.MathOptProtoAssertions):

  def test_proto_conversions(self) -> None:
    isv = init_arguments.GurobiISVKey(
        name="cat", application_name="hat", expiration=4, key="bat"
    )
    proto_isv = gurobi_pb2.GurobiInitializerProto.ISVKey(
        name="cat", application_name="hat", expiration=4, key="bat"
    )
    self.assert_protos_equiv(isv.to_proto(), proto_isv)
    self.assertEqual(init_arguments.gurobi_isv_key_from_proto(proto_isv), isv)


class StreamableGurobiInitArgumentsTest(
    absltest.TestCase, compare_proto.MathOptProtoAssertions
):

  def test_proto_conversions_isv_key_set(self) -> None:
    init = init_arguments.StreamableGurobiInitArguments(
        isv_key=init_arguments.GurobiISVKey(
            name="cat", application_name="hat", expiration=4, key="bat"
        )
    )
    proto_init = gurobi_pb2.GurobiInitializerProto(
        isv_key=gurobi_pb2.GurobiInitializerProto.ISVKey(
            name="cat", application_name="hat", expiration=4, key="bat"
        )
    )
    self.assert_protos_equiv(init.to_proto(), proto_init)
    self.assertEqual(
        init_arguments.streamable_gurobi_init_arguments_from_proto(proto_init),
        init,
    )

  def test_proto_conversions_isv_key_not_set(self) -> None:
    init = init_arguments.StreamableGurobiInitArguments()
    proto_init = gurobi_pb2.GurobiInitializerProto()
    self.assert_protos_equiv(init.to_proto(), proto_init)
    self.assertEqual(
        init_arguments.streamable_gurobi_init_arguments_from_proto(proto_init),
        init,
    )


class StreamableSolverInitArgumentsTest(
    absltest.TestCase, compare_proto.MathOptProtoAssertions
):

  def test_proto_conversions_gurobi_set(self) -> None:
    init = init_arguments.StreamableSolverInitArguments(
        gurobi=init_arguments.StreamableGurobiInitArguments(
            isv_key=init_arguments.GurobiISVKey(
                name="cat", application_name="hat", expiration=4, key="bat"
            )
        )
    )
    proto_init = parameters_pb2.SolverInitializerProto(
        gurobi=gurobi_pb2.GurobiInitializerProto(
            isv_key=gurobi_pb2.GurobiInitializerProto.ISVKey(
                name="cat", application_name="hat", expiration=4, key="bat"
            )
        )
    )
    self.assert_protos_equiv(init.to_proto(), proto_init)
    self.assertEqual(
        init_arguments.streamable_solver_init_arguments_from_proto(proto_init),
        init,
    )

  def test_proto_conversions_gurobi_not_set(self) -> None:
    init = init_arguments.StreamableSolverInitArguments()
    proto_init = parameters_pb2.SolverInitializerProto()
    self.assert_protos_equiv(init.to_proto(), proto_init)
    self.assertEqual(
        init_arguments.streamable_solver_init_arguments_from_proto(proto_init),
        init,
    )


if __name__ == "__main__":
  absltest.main()
