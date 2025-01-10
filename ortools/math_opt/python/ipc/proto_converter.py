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

"""Conversion functions for MathOpt protos.

Provides several conversion functions to transform from/to protos exposed in the
Operations Research API to the internal protos in
/ortools/math_opt/.*.proto.
"""

from google.protobuf import message
from ortools.service.v1 import optimization_pb2
from ortools.math_opt import rpc_pb2
from ortools.math_opt.python import normalize

_UNSUPPORTED_SOLVER_SPECIFIC_PARAMETERS = (
    "gscip",
    "gurobi",
    "glop",
    "cp_sat",
    "osqp",
    "glpk",
    "highs",
)


def convert_request(
    request: rpc_pb2.SolveRequest,
) -> optimization_pb2.SolveMathOptModelRequest:
    """Converts a `SolveRequest` to a `SolveMathOptModelRequest`.

    Args:
      request: A `SolveRequest` request built from a MathOpt model.

    Returns:
      A `SolveMathOptModelRequest` for the Operations Research API.

    Raises:
      ValueError: If a field that is not supported in the expernal proto is
        present in the request or if the request can't be parsed to a
        `SolveMathOptModelRequest`.
    """
    normalize.math_opt_normalize_proto(request)
    if request.HasField("initializer"):
        raise ValueError(str("initializer is not supported"))
    for param in _UNSUPPORTED_SOLVER_SPECIFIC_PARAMETERS:
        if request.parameters.HasField(param):
            raise ValueError(f"SolveParameters.{param} not supported")

    try:
        external_request = optimization_pb2.SolveMathOptModelRequest.FromString(
            request.SerializeToString()
        )
        return external_request
    except (message.DecodeError, message.EncodeError):
        raise ValueError("request can not be parsed") from None


def convert_response(
    api_response: optimization_pb2.SolveMathOptModelResponse,
) -> rpc_pb2.SolveResponse:
    """Converts a `SolveMathOptModelResponse` to a `SolveResponse`.

    Args:
      api_response: A `SolveMathOptModelResponse` response built from a MathOpt
        model.

    Returns:
      A `SolveResponse` response built from a MathOpt model.
    """
    api_response.DiscardUnknownFields()
    normalize.math_opt_normalize_proto(api_response)
    response = rpc_pb2.SolveResponse.FromString(api_response.SerializeToString())
    response.DiscardUnknownFields()
    return response
