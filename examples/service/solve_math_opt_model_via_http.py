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

"""Example of solving a MathOpt model through the OR API.

The model is built using the Python API, and the corresponding proto is
serialize to JSON to make the HTTP request.
"""

from collections.abc import Sequence

from absl import app
from absl import flags

from ortools.math_opt.python import mathopt
from ortools.math_opt.python.ipc import remote_http_solve

_API_KEY = flags.DEFINE_string("api_key", None, "API key for the OR API")


def request_example() -> None:
  """Run example using MathOpt `remote_http_solve` function."""
  # Set up the API key.
  api_key = _API_KEY.value
  if not api_key:
    print(
        "API key is required. See"
        " https://developers.google.com/optimization/service/setup for"
        " instructions."
    )
    return

  # Build a MathOpt model
  model = mathopt.Model(name="my_model")
  x = model.add_binary_variable(name="x")
  y = model.add_variable(lb=0.0, ub=2.5, name="y")
  model.add_linear_constraint(x + y <= 1.5, name="c")
  model.maximize(2 * x + y)
  try:
    result, logs = remote_http_solve.remote_http_solve(
        model,
        mathopt.SolverType.GSCIP,
        mathopt.SolveParameters(enable_output=True),
        api_key=api_key,
    )
    print("Objective value: ", result.objective_value())
    print("x: ", result.variable_values(x))
    print("y: ", result.variable_values(y))
    print("\n".join(logs))
  except remote_http_solve.OptimizationServiceError as err:
    print(err)


def main(argv: Sequence[str]) -> None:
  del argv  # Unused.
  request_example()


if __name__ == "__main__":
  app.run(main)
