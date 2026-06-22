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

"""Minimal example of getting a callback on every integer solution."""

import threading
# [START solve_callback_stream_solutions_py_imports]
from collections.abc import Sequence

from absl import app

from ortools.math_opt.python import mathopt

# [END solve_callback_stream_solutions_py_imports]


def main(argv: Sequence[str]) -> None:
    del argv  # Unused.

    # [START solve_callback_stream_solutions_py]
    model = mathopt.Model()
    x = model.add_binary_variable(name="x")
    model.maximize(x)

    cb_mutex = threading.Lock()

    def callback(cb_data: mathopt.CallbackData) -> mathopt.CallbackResult:
        # Warning: either the callback should be threadsafe, or you must rely on
        # solver specific guarantees that the callback isn't invoked concurrently.
        with cb_mutex:
            if cb_data.event != mathopt.Event.MIP_SOLUTION:
                raise RuntimeError(f"callback event not SOLUTION, was: {cb_data.event}")
            x_value = cb_data.solution[x]
            print(f"Found solution x = {x_value}", flush=True)
            return mathopt.CallbackResult()

    cb_reg = mathopt.CallbackRegistration(events={mathopt.Event.MIP_SOLUTION})
    # Gurobi, CpSat, and SCIP all support this callback.
    solver = mathopt.SolverType.GSCIP
    result = mathopt.solve(model, solver, callback_reg=cb_reg, cb=callback)

    if result.termination.reason != mathopt.TerminationReason.OPTIMAL:
        raise RuntimeError(f"model failed to solve: {result.termination}")
    print(f"Optimal x = {result.variable_values(x)}", flush=True)
    # [END solve_callback_stream_solutions_py]


if __name__ == "__main__":
    app.run(main)
