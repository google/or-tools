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

"""Testing correctness of the code snippets in the comments of model.py."""
import datetime
from typing import Sequence

from absl import app
from absl import flags

from ortools.math_opt.python import mathopt
from ortools.math_opt.python.ipc import remote_solve
from ortools.math_opt.python.ipc import solve_service_stubby_client

_SOLVER = flags.DEFINE_enum_class(
    "solver", mathopt.SolverType.GSCIP, mathopt.SolverType, "The solver to use."
)
_INTEGER = flags.DEFINE_bool(
    "integer", False, "If the variable should be integer or continuous."
)

_LOGS = flags.DEFINE_bool("logs", True, "Prints solver logs.")
_MODE = flags.DEFINE_enum_class(
    "mode",
    remote_solve.RemoteSolveMode.DEFAULT,
    remote_solve.RemoteSolveMode,
    "The mode to use with remote_solve().",
)


def main(argv: Sequence[str]) -> None:
    del argv  # Unused.

    model = mathopt.Model(name="my_model")
    x = model.add_variable(lb=0.0, ub=1.0, is_integer=_INTEGER.value, name="x")
    y = model.add_variable(lb=0.0, ub=1.0, is_integer=_INTEGER.value, name="y")
    model.add_linear_constraint(x + y <= 1.0, name="c")
    model.maximize(2 * x + 3 * y)

    stub = None
    if _MODE.value in (
        remote_solve.RemoteSolveMode.DEFAULT,
        remote_solve.RemoteSolveMode.STREAMING,
    ):
        stub = solve_service_stubby_client.solve_server_stub()

    msg_cb = None
    if _LOGS.value:
        msg_cb = mathopt.printer_message_callback(prefix="Solver log: ")
    # Raises exceptions on invalid input, internal solver error, rpc timeout etc.
    result = remote_solve.remote_solve(
        stub,
        model,
        _SOLVER.value,
        deadline=datetime.timedelta(minutes=1),
        mode=_MODE.value,
        msg_cb=msg_cb,
    )

    if result.termination.reason not in (
        mathopt.TerminationReason.OPTIMAL,
        mathopt.TerminationReason.FEASIBLE,
    ):
        raise RuntimeError(f"model failed to solve: {result.termination}")

    print(f"Objective value: {result.objective_value()}")
    print(f"Value for variable x: {result.variable_values()[x]}")


if __name__ == "__main__":
    app.run(main)
