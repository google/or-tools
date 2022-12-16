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

from unittest import mock

from absl.testing import absltest
from ortools.math_opt import model_update_pb2
from ortools.math_opt import sparse_containers_pb2
from ortools.math_opt.python.testing import proto_matcher


class _ConsumesUpdate:

    def __init__(self):
        pass

    def on_update(self, update: model_update_pb2.ModelUpdateProto):
        pass


class MathOptProtoAssertionsTest(absltest.TestCase):

    def test_mock_eq(self):
        update1 = model_update_pb2.ObjectiveUpdatesProto(
            direction_update=True, offset_update=0.0
        )
        update2 = model_update_pb2.ObjectiveUpdatesProto(direction_update=True)
        update3 = model_update_pb2.ObjectiveUpdatesProto(
            direction_update=True,
            linear_coefficients=sparse_containers_pb2.SparseDoubleVectorProto(),
        )
        self.assertFalse(
            proto_matcher.MathOptProtoEquivMatcher(update1).__eq__(update2)
        )
        self.assertTrue(proto_matcher.MathOptProtoEquivMatcher(update1).__ne__(update2))
        self.assertTrue(proto_matcher.MathOptProtoEquivMatcher(update2).__eq__(update3))
        self.assertFalse(
            proto_matcher.MathOptProtoEquivMatcher(update2).__ne__(update3)
        )

    def test_mock_function_when_equal(self):
        consumer = _ConsumesUpdate()
        consumer.on_update = mock.MagicMock()

        update = model_update_pb2.ModelUpdateProto(deleted_variable_ids=[0, 4, 5])

        consumer.on_update(update)

        expected = model_update_pb2.ModelUpdateProto(deleted_variable_ids=[0, 4, 5])
        consumer.on_update.assert_called_with(
            proto_matcher.MathOptProtoEquivMatcher(expected)
        )


if __name__ == "__main__":
    absltest.main()
