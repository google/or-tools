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

"""Unit tests for solver_resources."""

from absl.testing import absltest
from ortools.math_opt import rpc_pb2
from ortools.math_opt.python import solver_resources
from ortools.math_opt.python.testing import compare_proto


class SolverResourcesTest(
    compare_proto.MathOptProtoAssertions, absltest.TestCase
):

  def test_to_proto_empty(self):
    self.assert_protos_equiv(
        solver_resources.SolverResources().to_proto(),
        rpc_pb2.SolverResourcesProto(),
    )

  def test_to_proto_with_cpu(self):
    self.assert_protos_equiv(
        solver_resources.SolverResources(cpu=3.5).to_proto(),
        rpc_pb2.SolverResourcesProto(cpu=3.5),
    )

  def test_to_proto_with_ram(self):
    self.assert_protos_equiv(
        solver_resources.SolverResources(ram=50 * 1024 * 1024).to_proto(),
        rpc_pb2.SolverResourcesProto(ram=50 * 1024 * 1024),
    )


if __name__ == "__main__":
  absltest.main()
