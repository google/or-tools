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

"""Translate ortools/util StatusProto to Python exceptions."""

import enum
import typing
from typing import Optional

from ortools.util import status_pb2


class StatusCode(enum.Enum):
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


class StatusError(Exception):
    """Base class for exception returned by status_proto_to_exception().

    This class must not be instantiated; subclasses should.

    Some exceptions may inherit from another exception class to be caught as
    regular Python errors. E.g., InvalidArgumentError also inherits from
    ValueError, which would be the expected error thrown by an invalid argument in
    Python.

    Attributes:
      code: The code of the StatusProto (an integer). It should be one of the
        known values in StatusCode, except for UnexpectedCodeError.
      message: The message of the StatusProto.
    """

    def __init__(self, code: int, message: str):
        try:
            code_name = StatusCode(code).name
        except ValueError:
            code_name = str(code)
        super().__init__(f"{message} (was C++ {code_name})")
        self.code = code
        self.message = message


class CancelledError(StatusError):
    """Exception corresponding to StatusCode.CANCELLED."""

    def __init__(self, message: str):
        super().__init__(StatusCode.CANCELLED.value, message)


class UnknownError(StatusError):
    """Exception corresponding to StatusCode.UNKNOWN."""

    def __init__(self, message: str):
        super().__init__(StatusCode.UNKNOWN.value, message)


class InvalidArgumentError(StatusError, ValueError):
    """Exception corresponding to StatusCode.INVALID_ARGUMENT."""

    def __init__(self, message: str):
        super().__init__(StatusCode.INVALID_ARGUMENT.value, message)


class DeadlineExceededError(StatusError):
    """Exception corresponding to StatusCode.DEADLINE_EXCEEDED."""

    def __init__(self, message: str):
        super().__init__(StatusCode.DEADLINE_EXCEEDED.value, message)


class NotFoundError(StatusError):
    """Exception corresponding to StatusCode.NOT_FOUND."""

    def __init__(self, message: str):
        super().__init__(StatusCode.NOT_FOUND.value, message)


class AlreadyExistsError(StatusError):
    """Exception corresponding to StatusCode.ALREADY_EXISTS."""

    def __init__(self, message: str):
        super().__init__(StatusCode.ALREADY_EXISTS.value, message)


class PermissionDeniedError(StatusError):
    """Exception corresponding to StatusCode.PERMISSION_DENIED."""

    def __init__(self, message: str):
        super().__init__(StatusCode.PERMISSION_DENIED.value, message)


class UnauthenticatedError(StatusError):
    """Exception corresponding to StatusCode.UNAUTHENTICATED."""

    def __init__(self, message: str):
        super().__init__(StatusCode.UNAUTHENTICATED.value, message)


class ResourceExhaustedError(StatusError):
    """Exception corresponding to StatusCode.RESOURCE_EXHAUSTED."""

    def __init__(self, message: str):
        super().__init__(StatusCode.RESOURCE_EXHAUSTED.value, message)


class FailedPreconditionError(StatusError, AssertionError):
    """Exception corresponding to StatusCode.FAILED_PRECONDITION."""

    def __init__(self, message: str):
        super().__init__(StatusCode.FAILED_PRECONDITION.value, message)


class AbortedError(StatusError):
    """Exception corresponding to StatusCode.ABORTED."""

    def __init__(self, message: str):
        super().__init__(StatusCode.ABORTED.value, message)


class OutOfRangeError(StatusError):
    """Exception corresponding to StatusCode.OUT_OF_RANGE."""

    def __init__(self, message: str):
        super().__init__(StatusCode.OUT_OF_RANGE.value, message)


class UnimplementedError(StatusError, NotImplementedError):
    """Exception corresponding to StatusCode.UNIMPLEMENTED."""

    def __init__(self, message: str):
        super().__init__(StatusCode.UNIMPLEMENTED.value, message)


class InternalError(StatusError):
    """Exception corresponding to StatusCode.INTERNAL."""

    def __init__(self, message: str):
        super().__init__(StatusCode.INTERNAL.value, message)


class UnavailableError(StatusError):
    """Exception corresponding to StatusCode.UNAVAILABLE."""

    def __init__(self, message: str):
        super().__init__(StatusCode.UNAVAILABLE.value, message)


class DataLossError(StatusError):
    """Exception corresponding to StatusCode.DATA_LOSS."""

    def __init__(self, message: str):
        super().__init__(StatusCode.DATA_LOSS.value, message)


class UnexpectedCodeError(StatusError):
    """Exception corresponding to a code not listed in StatusCode.

    This is different from UnknownError which is the error for the
    StatusCode.UNKNOWN (2). This error is used when StatusProto.code is not a
    value in StatusCode (which should be rare).
    """


def status_proto_to_exception(
    status_proto: status_pb2.StatusProto,
) -> Optional[StatusError]:
    """Returns a Python exception that matches the input absl::Status.

    Some of StatusError sub-classes also inherit from another Python exception
    that a Python library would use:
    - InvalidArgumentError: ValueError
    - FailedPreconditionError: AssertionError
    - UnimplementedError: NotImplementedError

    Args:
      status_proto: The input proto to convert to an exception.

    Returns:
      The corresponding exception. None if the input status is OK. If the input
      status_proto.code is not matching any value from StatusCode, an
      UnexpectedCodeError is returned.
    """
    try:
        code = StatusCode(status_proto.code)
    except ValueError:
        return UnexpectedCodeError(status_proto.code, status_proto.message)

    # Define a short variable name to make all lines below fit in 80 columns.
    msg = status_proto.message

    match code:
        case StatusCode.OK:
            return None
        case StatusCode.CANCELLED:
            return CancelledError(msg)
        case StatusCode.UNKNOWN:
            return UnknownError(msg)
        case StatusCode.INVALID_ARGUMENT:
            return InvalidArgumentError(msg)
        case StatusCode.DEADLINE_EXCEEDED:
            return DeadlineExceededError(msg)
        case StatusCode.NOT_FOUND:
            return NotFoundError(msg)
        case StatusCode.ALREADY_EXISTS:
            return AlreadyExistsError(msg)
        case StatusCode.PERMISSION_DENIED:
            return PermissionDeniedError(msg)
        case StatusCode.UNAUTHENTICATED:
            return UnauthenticatedError(msg)
        case StatusCode.RESOURCE_EXHAUSTED:
            return ResourceExhaustedError(msg)
        case StatusCode.FAILED_PRECONDITION:
            return FailedPreconditionError(msg)
        case StatusCode.ABORTED:
            return AbortedError(msg)
        case StatusCode.OUT_OF_RANGE:
            return OutOfRangeError(msg)
        case StatusCode.UNIMPLEMENTED:
            return UnimplementedError(msg)
        case StatusCode.INTERNAL:
            return InternalError(msg)
        case StatusCode.UNAVAILABLE:
            return UnavailableError(msg)
        case StatusCode.DATA_LOSS:
            return DataLossError(msg)
        case _ as unreachable:
            typing.assert_never(unreachable)
