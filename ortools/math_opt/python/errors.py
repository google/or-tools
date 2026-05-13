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

"""Translate C++'s absl::Status errors to Python standard errors.

Here we try to use the standard Python errors we would use if the C++ code was
instead implemented in Python. This will give Python users a more familiar API.
"""

import enum
from typing import Optional, Type
from ortools.math_opt import rpc_pb2


class _StatusCode(enum.Enum):
    """The C++ absl::Status::code() values."""

    OK = 0
    CANCELLED = 1
    UNKNOWN = 2
    INVALID_ARGUMENT = 3
    DEADLINE_EXCEEDED = 4
    NOT_FOUND = 5
    ALREADY_EXISTS = 6
    PERMISSION_DENIED = 7
    UNAUTHENTICATED = 16
    RESOURCE_EXHAUSTED = 8
    FAILED_PRECONDITION = 9
    ABORTED = 10
    OUT_OF_RANGE = 11
    UNIMPLEMENTED = 12
    INTERNAL = 13
    UNAVAILABLE = 14
    DATA_LOSS = 15


class InternalMathOptError(RuntimeError):
    """Some MathOpt internal error.

    This error is usually raised because of a bug in MathOpt or one of the solver
    library it wraps.
    """


def status_proto_to_exception(
    status_proto: rpc_pb2.StatusProto,
) -> Optional[Exception]:
    """Returns the Python exception that best match the input absl::Status.

    There are some Status that we expect the MathOpt code to return, for those the
    matching exceptions are:
    - InvalidArgument: ValueError
    - FailedPrecondition: AssertionError
    - Unimplemented: NotImplementedError
    - Internal: InternalMathOptError

    Other Status's are not used by MathOpt, if they are seen a
    InternalMathOptError is raised (as if the Status was Internal) and the error
    message contains the unexpected code.

    Args:
      status_proto: The input proto to convert to an exception.

    Returns:
      The corresponding exception. None if the input status is OK.
    """
    try:
        code = _StatusCode(status_proto.code)
    except ValueError:
        return InternalMathOptError(
            f"unknown C++ error (code = {status_proto.code}):"
            f" {status_proto.message}"
        )

    if code == _StatusCode.OK:
        return None

    # For expected errors we compute the corresponding class.
    error_type: Optional[Type[Exception]] = None
    if code == _StatusCode.INVALID_ARGUMENT:
        error_type = ValueError
    if code == _StatusCode.FAILED_PRECONDITION:
        error_type = AssertionError
    if code == _StatusCode.UNIMPLEMENTED:
        error_type = NotImplementedError
    if code == _StatusCode.INTERNAL:
        error_type = InternalMathOptError

    if error_type is not None:
        return error_type(f"{status_proto.message} (was C++ {code.name})")

    return InternalMathOptError(
        f"unexpected C++ error {code.name}: {status_proto.message}"
    )
