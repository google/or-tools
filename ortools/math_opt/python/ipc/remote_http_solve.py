# Copyright 2010-2024 Google LLC
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

"""Solve MathOpt models via HTTP request to the OR API."""

import json
from typing import List, Optional, Tuple
from google.protobuf import json_format
import requests
from ortools.service.v1 import optimization_pb2
from ortools.math_opt import rpc_pb2
from ortools.math_opt.python import mathopt
from ortools.math_opt.python.ipc import proto_converter

_DEFAULT_DEADLINE_SEC = 10
_DEFAULT_ENDPOINT = "https://optimization.googleapis.com/v1/mathopt:solveMathOptModel"
_RELATIVE_TIME_BUFFER = 0.05


class OptimizationServiceError(Exception):
    """Error produced when solving a MathOpt model via HTTP request."""


def remote_http_solve(
    model: mathopt.Model,
    solver_type: mathopt.SolverType,
    params: Optional[mathopt.SolveParameters] = None,
    model_params: Optional[mathopt.ModelSolveParameters] = None,
    endpoint: Optional[str] = _DEFAULT_ENDPOINT,
    api_key: Optional[str] = None,
    deadline_sec: Optional[float] = _DEFAULT_DEADLINE_SEC,
) -> Tuple[mathopt.SolveResult, List[str]]:
    """Solves a MathOpt model via HTTP request to the OR API.

    Args:
      model: The optimization model.
      solver_type: The underlying solver to use.
      params: Optional configuration of the underlying solver.
      model_params: Optional configuration of the solver that is model specific.
      endpoint: An URI identifying the service for remote solves.
      api_key: Key to the OR API.
      deadline_sec: The number of seconds before the request times out.

    Returns:
      A SolveResult containing the termination reason, solution(s) and stats.
      A list of messages with the logs (if specified in the `params`).

    Raises:
      OptimizationServiceError: if an HTTP error is returned while solving a
        model.
    """
    if api_key is None:
        # TODO(b/306709279): Relax this when unauthenticated solves are allowed.
        raise ValueError("api_key can't be None when solving remotely")

    payload = _build_json_payload(model, solver_type, params, model_params)

    session = create_optimization_service_session(api_key, deadline_sec)
    response = session.post(
        url=endpoint,
        json=payload,
        timeout=deadline_sec,
    )

    if not response.ok:
        http_error = json.loads(response.content)["error"]
        raise OptimizationServiceError(
            f'status code {http_error["code"]}: {http_error["message"]}'
        ) from None

    return _build_solve_result(response.content, model)


def create_optimization_service_session(
    api_key: str,
    deadline_sec: float,
) -> requests.Session:
    """Creates a session with the appropriate headers.

    This function sets headers for authentication via an API key, and it sets
    deadlines set for the server and the connection.

    Args:
      api_key: Key to the OR API.
      deadline_sec: The number of seconds before the request times out.

    Returns:
      requests.Session a session with the necessary headers to call the
      optimization serive.
    """
    session = requests.Session()
    server_timeout = deadline_sec * (1 - _RELATIVE_TIME_BUFFER)
    session.headers = {
        "Content-Type": "application/json",
        "Connection": "keep-alive",
        "Keep-Alive": f"timeout={deadline_sec}, max=1",
        "X-Server-Timeout": f"{server_timeout}",
        "X-Goog-Api-Key": api_key,
    }
    return session


def _build_json_payload(
    model: mathopt.Model,
    solver_type: mathopt.SolverType,
    params: Optional[mathopt.SolveParameters],
    model_params: Optional[mathopt.ModelSolveParameters],
):
    """Builds a JSON payload.

    Args:
      model: The optimization model.
      solver_type: The underlying solver to use.
      params: Optional configuration of the underlying solver.
      model_params: Optional configuration of the solver that is model specific.

    Returns:
      A JSON object with a MathOpt model and corresponding parameters.

    Raises:
      SerializationError: If building the OR API proto is not successful or
        deserializing to JSON fails.
    """
    params = params or mathopt.SolveParameters()
    model_params = model_params or mathopt.ModelSolveParameters()
    try:
        request = rpc_pb2.SolveRequest(
            model=model.export_model(),
            solver_type=solver_type.value,
            parameters=params.to_proto(),
            model_parameters=model_params.to_proto(),
        )
        api_request = proto_converter.convert_request(request)
    except ValueError as err:
        raise ValueError from err

    return json.loads(json_format.MessageToJson(api_request))


def _build_solve_result(
    json_response: bytes, model: mathopt.Model
) -> Tuple[mathopt.SolveResult, List[str]]:
    """Parses a JSON representation of a response to a SolveResult object.

    Args:
      json_response: bytes representing the `SolveMathOptModelResponse` in JSON
        format
      model: The optimization model that was solved

    Returns:
      A SolveResult of the model.
      A list of messages with the logs.

    Raises:
      SerializationError: If parsing the json response fails or if converting the
        OR API response to the internal MathOpt response fails.
    """
    try:
        api_response = json_format.Parse(
            json_response, optimization_pb2.SolveMathOptModelResponse()
        )
    except json_format.ParseError as json_err:
        raise ValueError(
            "API response is not a valid SolveMathOptModelResponse JSON"
        ) from json_err

    response = proto_converter.convert_response(api_response)
    return mathopt.parse_solve_result(response.result, model), list(response.messages)
