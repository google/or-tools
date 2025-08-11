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

"""Example of using stubby_remote_streaming_solve module."""

from collections.abc import Sequence
import datetime

from absl import app

from ortools.math_opt import rpc_stubby_pyclif
from ortools.math_opt.python import mathopt
from ortools.math_opt.python.ipc import solve_service_stubby_client
from ortools.math_opt.python.ipc import stubby_remote_streaming_solve


# Time limit for the solve.
_TIME_LIMIT = datetime.timedelta(seconds=10)

# An estimation of the time taken, around the actual call to the underlying
# solver. This includes an evaluation of:
# - the network latency ~500ms for a round trip,
# - some small overflow of the time limit by the underlying solver (solvers may
#   not check that they hit the time limit frequently),
# - the time taken to create the solution (which is proportional to the model
#   size),
# - the time taken to stream the request,
# - the time taken to stream the solution.
_PRE_POST_SOLVE_PROCESSING = datetime.timedelta(seconds=5)


class SolveCancelledError(Exception):
    """Raised when the solve is cancelled."""


class SolveFailedError(Exception):
    """Raised when the solve failed."""


def _check_remaining_time(deadline: datetime.datetime, num_retries: int) -> None:
    """Checks that we have enough remaining time.

    Raises an error when the remaining deadline is too short at the start of a
    try.

    Args:
      deadline: The RPC deadline.
      num_retries: The current number of retries that have failed (0 on first
        call).

    Raises:
      SolveCancelledError: If we run out of time to solve.
    """
    remaining_time = deadline - datetime.datetime.now(datetime.timezone.utc)
    if remaining_time < _TIME_LIMIT + _PRE_POST_SOLVE_PROCESSING:
        raise SolveCancelledError(
            f"Not enough deadline remaining ({remaining_time}) to complete a solve"
            f" with a time limit of {_TIME_LIMIT} and a the pre/postprocessing of"
            f" the solve in {_PRE_POST_SOLVE_PROCESSING}  after"
            f" {num_retries} retries."
        )


def call_solve(stub: rpc_stubby_pyclif.SolveService.ClientStub) -> float:
    """Tests the remote streaming solve with the provided stub.

    This function is exported to be unit tested in Tin test that will provide a
    custom stub. The binary will use the default production stub.

    Args:
      stub: The stub to use to run the test.

    Returns:
      The objective value of the found solution. Expects close to 2.0.

    Raises:
      SolveCancelledError: If we run out of deadline time to do the solve.
      SolveFailedError: If the solver did not return a feasible solution.
    """
    model = mathopt.Model(name="remote_solve_example_model")
    x = model.add_integer_variable(lb=0.0, ub=1.0, name="x")
    model.maximize(2 * x)

    # A deadline representing the time after which we don't care about getting an
    # answer. For an RPC server, this is usually provided by the server itself.
    deadline = datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(
        minutes=3
    )

    def update_solve_arguments(
        data: stubby_remote_streaming_solve.RetriableCallData,
        solve_args: stubby_remote_streaming_solve.SolveArguments,
    ) -> None:
        _check_remaining_time(deadline, data.num_retries)
        # Set a message callback that includes the try number as a prefix.
        solve_args.msg_cb = mathopt.printer_message_callback(
            prefix=f"solve (try #{data.num_retries}) | "
        )

    # Retry calls as long as there is enough time until the deadline to give the
    # server time to do a solve (_TIME_LIMIT) and to do the post solve processing
    # (_PRE_POST_SOLVE_PROCESSING).
    result = stubby_remote_streaming_solve.remote_streaming_solve_with_retries(
        stub,
        model,
        mathopt.SolverType.GUROBI,
        deadline=deadline,
        params=mathopt.SolveParameters(time_limit=_TIME_LIMIT),
        update_solve_arguments=update_solve_arguments,
    )

    if result.termination.reason not in (
        mathopt.TerminationReason.OPTIMAL,
        mathopt.TerminationReason.FEASIBLE,
    ):
        raise SolveFailedError(f"Failed to solve, termination: {result.termination}")

    return result.objective_value()


def main(argv: Sequence[str]) -> None:
    if len(argv) > 1:
        raise app.UsageError("Too many command-line arguments.")

    stub = solve_service_stubby_client.solve_server_stub()

    objective_value = call_solve(stub)

    print(f"objective_value: {objective_value}")


if __name__ == "__main__":
    app.run(main)
