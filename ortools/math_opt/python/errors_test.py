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

"""Tests of the `errors` package."""

from absl.testing import absltest
from ortools.math_opt import rpc_pb2
from ortools.math_opt.python import errors


class StatusProtoToExceptionTest(absltest.TestCase):

    def test_ok(self) -> None:
        self.assertIsNone(
            errors.status_proto_to_exception(
                rpc_pb2.StatusProto(code=errors._StatusCode.OK.value)
            )
        )

    def test_invalid_argument(self) -> None:
        error = errors.status_proto_to_exception(
            rpc_pb2.StatusProto(
                code=errors._StatusCode.INVALID_ARGUMENT.value, message="something"
            )
        )
        self.assertIsInstance(error, ValueError)
        self.assertEqual(str(error), "something (was C++ INVALID_ARGUMENT)")

    def test_failed_precondition(self) -> None:
        error = errors.status_proto_to_exception(
            rpc_pb2.StatusProto(
                code=errors._StatusCode.FAILED_PRECONDITION.value,
                message="something",
            )
        )
        self.assertIsInstance(error, AssertionError)
        self.assertEqual(str(error), "something (was C++ FAILED_PRECONDITION)")

    def test_unimplemented(self) -> None:
        error = errors.status_proto_to_exception(
            rpc_pb2.StatusProto(
                code=errors._StatusCode.UNIMPLEMENTED.value, message="something"
            )
        )
        self.assertIsInstance(error, NotImplementedError)
        self.assertEqual(str(error), "something (was C++ UNIMPLEMENTED)")

    def test_internal(self) -> None:
        error = errors.status_proto_to_exception(
            rpc_pb2.StatusProto(
                code=errors._StatusCode.INTERNAL.value, message="something"
            )
        )
        self.assertIsInstance(error, errors.InternalMathOptError)
        self.assertEqual(str(error), "something (was C++ INTERNAL)")

    def test_unexpected_code(self) -> None:
        error = errors.status_proto_to_exception(
            rpc_pb2.StatusProto(
                code=errors._StatusCode.DEADLINE_EXCEEDED.value, message="something"
            )
        )
        self.assertIsInstance(error, errors.InternalMathOptError)
        self.assertEqual(
            str(error), "unexpected C++ error DEADLINE_EXCEEDED: something"
        )

    def test_unknown_code(self) -> None:
        error = errors.status_proto_to_exception(
            rpc_pb2.StatusProto(code=-5, message="something")
        )
        self.assertIsInstance(error, errors.InternalMathOptError)
        self.assertEqual(str(error), "unknown C++ error (code = -5): something")


if __name__ == "__main__":
    absltest.main()
