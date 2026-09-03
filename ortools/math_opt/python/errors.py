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

"""Translate C++'s absl::Status errors to Python standard errors.

Here we try to use the standard Python errors we would use if the C++ code was
instead implemented in Python. This will give Python users a more familiar API.
"""

from typing import Optional

from ortools.util import status_pb2
from ortools.util.python import status_streaming


class InternalMathOptError(RuntimeError):
    """Some MathOpt internal error.

    This error is usually raised because of a bug in MathOpt or one of the solver
    library it wraps.
    """


def status_proto_to_exception(
    status_proto: status_pb2.StatusProto,
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
    exception = status_streaming.status_proto_to_exception(status_proto)
    if exception is None:
        return None
    if isinstance(exception, status_streaming.InternalError):
        return InternalMathOptError(str(exception))
    # Exception returned by status_streaming already inherit from the expected
    # exceptions.
    return exception
