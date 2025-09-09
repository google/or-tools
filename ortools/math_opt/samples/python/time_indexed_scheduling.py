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

"""Solve a time-indexed scheduling problem.

Problem statement:

Schedule jobs sequentially (on a single machine) to minimize the sum of the
completion times, where each job cannot start until its release time. In the
scheduling literature, this problem is 1|r_i|sum_i C_i. This problem is NP-Hard
(e.g. see "Elements of Scheduling" by Lenstra and Shmoys 2020, chapter 4).

Data:
 * n jobs
 * processing time p_i for i = 1,...,n
 * release time r_i for i = 1,...,n
 * Implied: T = max_i r_i + sum_i p_i, the time horizon, all jobs must start
   in [0, T].

Variables:
 * x_it for job i = 1,...,n and time t = 1,...,T, if job i starts at time t.

Model:
  min   sum_i sum_t (t + p_i) * x_it
  s.t.  sum_t x_it = 1                     for all i = 1,...,n     (1)
        sum_i sum_{s=t-p_i+1}^t x_is <= 1  for all t = 0,...,T     (2)
        x_it = 0                           for all i, for t < r_i  (3)
        x_it in {0, 1}                     for all i and t

In the objective, t + p_i is the time the job is completed if it starts at t.
Constraint (1) ensures that each job is scheduled once, constraint (2)
ensures that no two jobs overlap in when they are running, and constraint (3)
enforces the release dates.
"""

from collections.abc import Sequence
import dataclasses
import random

from absl import app
from absl import flags

from ortools.math_opt.python import mathopt

_SOLVER_TYPE = flags.DEFINE_enum_class(
    "solver_type",
    mathopt.SolverType.GSCIP,
    mathopt.SolverType,
    "The solver needs to support binary IP.",
)

_NUM_JOBS = flags.DEFINE_integer("num_jobs", 30, "How many jobs to schedule.")

_USE_TEST_DATA = flags.DEFINE_boolean(
    "use_test_data",
    False,
    "Solve a small hardcoded instance instead of a large random one.",
)


@dataclasses.dataclass(frozen=True)
class Jobs:
    """Data for jobs in a time-indexed scheduling problem.

    Attributes:
      processing_times: The duration of each job.
      release_times: The earliest time at which each job can begin.
    """

    processing_times: tuple[int, ...]
    release_times: tuple[int, ...]


def random_jobs(num_jobs: int) -> Jobs:
    """Generates a random set of jobs to be scheduled."""
    # Processing times are uniform in [1, processing_time_ub].
    processing_time_ub = 20

    # Release times are uniform in [0, release_time_ub].
    release_time_ub = num_jobs * processing_time_ub // 2

    processing_times: tuple[int, ...] = tuple(
        random.randrange(1, processing_time_ub) for _ in range(num_jobs)
    )
    release_times: tuple[int, ...] = tuple(
        random.randrange(1, release_time_ub) for _ in range(num_jobs)
    )

    return Jobs(processing_times=processing_times, release_times=release_times)


# A small instance for testing. The optimal solution is to run:
#   Job 1 at time 1
#   Job 2 at time 2
#   Job 0 at time 7
# This gives a sum of completion times of 2 + 7 + 17 = 26.
#
# Note that the above schedule idles at time 0. If instead, we did
#   Job 2 at time 0
#   Job 1 at time 5
#   Job 0 at time 6
# This gives a sum of completion times of 5 + 6 + 16 = 27.
def _test_instance() -> Jobs:
    return Jobs(processing_times=(10, 1, 5), release_times=(0, 1, 0))


def time_horizon(jobs: Jobs) -> int:
    """Computes the time horizon of the problem."""
    max_release = max(jobs.release_times, default=0)
    sum_processing = sum(jobs.processing_times)
    return max_release + sum_processing


@dataclasses.dataclass(frozen=True)
class Schedule:
    """The solution to a time-indexed scheduling problem.

    Attributes:
      start_times: The time at which each job begins.
      sum_of_completion_times: The sum of times at which jobs complete.
    """

    start_times: tuple[int, ...] = ()
    sum_of_completion_times: int = 0


def solve(jobs: Jobs, solver_type: mathopt.SolverType) -> Schedule:
    """Solves a time indexed scheduling problem, returning the best schedule.

    Args:
      jobs: The jobs to be scheduled, each with processing and release times.
      solver_type: The IP solver used to solve the problem.

    Returns:
      The schedule of jobs that minimizes the sum of completion times.

    Raises:
      RuntimeError: On solve errors.
    """
    processing_times = jobs.processing_times
    release_times = jobs.release_times
    assert len(processing_times) == len(release_times)
    num_jobs = len(processing_times)

    horizon = time_horizon(jobs)
    model = mathopt.Model(name="time_indexed_scheduling")

    sum_completion_times = 0

    # x[i][t] == 1 indicates that we start job i at time t.
    x = [[] for i in range(num_jobs)]

    for i in range(num_jobs):
        for t in range(horizon):
            v = model.add_binary_variable(name=f"x_{i}_{t}")
            completion_time = t + processing_times[i]
            sum_completion_times += completion_time * v
            if t < release_times[i]:
                v.upper_bound = 0
            x[i].append(v)

        # Pick one time to run job i.
        model.add_linear_constraint(sum(x[i]) == 1)

    model.minimize(sum_completion_times)

    # Run at most one job at a time.
    for t in range(horizon):
        conflicts = 0
        for i in range(num_jobs):
            for s in range(max(0, t - processing_times[i] + 1), t + 1):
                conflicts += x[i][s]
        model.add_linear_constraint(conflicts <= 1)

    result = mathopt.solve(model, solver_type)

    if result.termination.reason != mathopt.TerminationReason.OPTIMAL:
        raise RuntimeError(
            "Failed to solve time-indexed scheduling problem to "
            f" optimality: {result.termination}"
        )

    start_times = []

    # Add the start times for the jobs.
    for i in range(num_jobs):
        for t in range(horizon):
            var_value = result.variable_values(x[i][t])
            if var_value > 0.5:
                start_times.append(t)
                break

    return Schedule(tuple(start_times), int(round(result.objective_value())))


def print_schedule(jobs: Jobs, schedule: Schedule) -> None:
    """Displays the schedule, one job per line."""
    processing_times = jobs.processing_times
    release_times = jobs.release_times
    num_jobs = len(processing_times)
    start_times = schedule.start_times

    print("Sum of completion times:", schedule.sum_of_completion_times)
    jobs_by_start_time = []

    for i in range(num_jobs):
        jobs_by_start_time.append(
            (start_times[i], processing_times[i], release_times[i])
        )

    jobs_by_start_time.sort(key=lambda job: job[0])

    print("Start time, processing time, release time:")
    for job in jobs_by_start_time:
        print(job[0], job[1], job[2])


def main(argv: Sequence[str]) -> None:
    del argv  # Unused.
    if _USE_TEST_DATA.value:
        jobs = _test_instance()
        schedule = solve(jobs, _SOLVER_TYPE.value)
    else:
        jobs = random_jobs(_NUM_JOBS.value)
        schedule = solve(jobs, _SOLVER_TYPE.value)

    print_schedule(jobs, schedule)


if __name__ == "__main__":
    app.run(main)
