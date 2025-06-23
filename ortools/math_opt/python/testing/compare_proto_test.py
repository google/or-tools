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

"""Test MathOpt's custom proto test assertions."""

from absl.testing import absltest
from ortools.math_opt import model_pb2
from ortools.math_opt.python import normalize
from ortools.math_opt.python.testing import compare_proto


class MathOptProtoAssertionsTest(
    absltest.TestCase, compare_proto.MathOptProtoAssertions
):

  def test_assertions_match_but_not_equal(self) -> None:
    model_with_empty_vars = model_pb2.ModelProto()
    model_with_empty_vars.variables.SetInParent()
    empty = model_pb2.ModelProto()
    with self.assertRaisesRegex(AssertionError, ".*variables.*"):
      self.assert_protos_equal(model_with_empty_vars, empty)
    with self.assertRaisesRegex(AssertionError, ".*variables.*"):
      self.assert_protos_equal(empty, model_with_empty_vars)

    self.assert_protos_equiv(model_with_empty_vars, empty)
    self.assert_protos_equiv(empty, model_with_empty_vars)

    normalize.math_opt_normalize_proto(model_with_empty_vars)
    self.assert_protos_equal(model_with_empty_vars, empty)
    self.assert_protos_equal(empty, model_with_empty_vars)
    self.assert_protos_equiv(model_with_empty_vars, empty)
    self.assert_protos_equiv(empty, model_with_empty_vars)

  def test_do_not_match(self) -> None:
    with_maximize = model_pb2.ModelProto()
    with_maximize.objective.maximize = True
    empty = model_pb2.ModelProto()

    with self.assertRaisesRegex(AssertionError, ".*maximize.*"):
      self.assert_protos_equal(with_maximize, empty)
    with self.assertRaisesRegex(AssertionError, ".*maximize.*"):
      self.assert_protos_equal(empty, with_maximize)

    with self.assertRaisesRegex(AssertionError, ".*maximize.*"):
      self.assert_protos_equiv(with_maximize, empty)
    with self.assertRaisesRegex(AssertionError, ".*maximize.*"):
      self.assert_protos_equiv(empty, with_maximize)


if __name__ == "__main__":
  absltest.main()
