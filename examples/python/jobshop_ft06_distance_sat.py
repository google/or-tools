#!/usr/bin/env python3
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

"""This model implements a variation of the ft06 jobshop.

A jobshop is a standard scheduling problem when you must sequence a
series of tasks on a set of machines. Each job contains one task per
machine. The order of execution and the length of each job on each
machine is task dependent.

The objective is to minimize the maximum completion time of all
jobs. This is called the makespan.

This variation introduces a minimum distance between all the jobs on each
machine.
"""

import collections

from ortools.sat.python import cp_model


def distance_between_jobs(x: int, y: int) -> int:
    """Returns the distance between tasks of job x and tasks of job y."""
    return abs(x - y)


def jobshop_ft06_distance() -> None:
    """Solves the ft06 jobshop with distances between tasks."""
    # Creates the model.
    model = cp_model.CpModel()

    machines_count = 6
    jobs_count = 6
    all_machines = range(0, machines_count)
    all_jobs = range(0, jobs_count)

    durations = [
        [1, 3, 6, 7, 3, 6],
        [8, 5, 10, 10, 10, 4],
        [5, 4, 8, 9, 1, 7],
        [5, 5, 5, 3, 8, 9],
        [9, 3, 5, 4, 3, 1],
        [3, 3, 9, 10, 4, 1],
    ]

    machines = [
        [2, 0, 1, 3, 5, 4],
        [1, 2, 4, 5, 0, 3],
        [2, 3, 5, 0, 1, 4],
        [1, 0, 2, 3, 4, 5],
        [2, 1, 4, 5, 0, 3],
        [1, 3, 5, 0, 4, 2],
    ]

    # Computes horizon statically.
    horizon = 150

    task_type = collections.namedtuple("task_type", "start end interval")

    # Creates jobs.
    all_tasks = {}
    for i in all_jobs:
        for j in all_machines:
            start_var = model.new_int_var(0, horizon, f"start_{i}_{j}")
            duration = durations[i][j]
            end_var = model.new_int_var(0, horizon, f"end_{i}_{j}")
            interval_var = model.new_interval_var(
                start_var, duration, end_var, f"interval_{i}_{j}"
            )
            all_tasks[(i, j)] = task_type(
                start=start_var, end=end_var, interval=interval_var
            )

    # Create disjuctive constraints.
    for i in all_machines:
        job_intervals = []
        job_indices = []
        job_starts = []
        job_ends = []
        for j in all_jobs:
            for k in all_machines:
                if machines[j][k] == i:
                    job_intervals.append(all_tasks[(j, k)].interval)
                    job_indices.append(j)
                    job_starts.append(all_tasks[(j, k)].start)
                    job_ends.append(all_tasks[(j, k)].end)
        model.add_no_overlap(job_intervals)

        arcs = []
        for j1 in range(len(job_intervals)):
            # Initial arc from the dummy node (0) to a task.
            start_lit = model.new_bool_var(f"{j1} is first job")
            arcs.append((0, j1 + 1, start_lit))
            # Final arc from an arc to the dummy node.
            arcs.append((j1 + 1, 0, model.new_bool_var(f"{j1} is last job")))

            for j2 in range(len(job_intervals)):
                if j1 == j2:
                    continue

                lit = model.new_bool_var(f"{j2} follows {j1}")
                arcs.append((j1 + 1, j2 + 1, lit))

                # We add the reified precedence to link the literal with the
                # times of the two tasks.
                min_distance = distance_between_jobs(j1, j2)
                model.add(
                    job_starts[j2] >= job_ends[j1] + min_distance
                ).only_enforce_if(lit)

        model.add_circuit(arcs)

    # Precedences inside a job.
    for i in all_jobs:
        for j in range(0, machines_count - 1):
            model.add(all_tasks[(i, j + 1)].start >= all_tasks[(i, j)].end)

    # Makespan objective.
    obj_var = model.new_int_var(0, horizon, "makespan")
    model.add_max_equality(
        obj_var, [all_tasks[(i, machines_count - 1)].end for i in all_jobs]
    )
    model.minimize(obj_var)

    # Solve model.
    solver = cp_model.CpSolver()
    status = solver.solve(model)

    # Output solution.
    if status == cp_model.OPTIMAL:
        print(f"Optimal makespan: {solver.objective_value}")
    print(solver.response_stats())


jobshop_ft06_distance()
