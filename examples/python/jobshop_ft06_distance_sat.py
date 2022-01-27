#!/usr/bin/env python3
# Copyright 2010-2021 Google LLC
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


def distance_between_jobs(x, y):
    """Returns the distance between tasks of job x and tasks of job y."""
    return abs(x - y)


def jobshop_ft06_distance():
    """Solves the ft06 jobshop with distances between tasks."""
    # Creates the model.
    model = cp_model.CpModel()

    machines_count = 6
    jobs_count = 6
    all_machines = range(0, machines_count)
    all_jobs = range(0, jobs_count)

    durations = [[1, 3, 6, 7, 3, 6], [8, 5, 10, 10, 10, 4], [5, 4, 8, 9, 1, 7],
                 [5, 5, 5, 3, 8, 9], [9, 3, 5, 4, 3, 1], [3, 3, 9, 10, 4, 1]]

    machines = [[2, 0, 1, 3, 5, 4], [1, 2, 4, 5, 0, 3], [2, 3, 5, 0, 1, 4],
                [1, 0, 2, 3, 4, 5], [2, 1, 4, 5, 0, 3], [1, 3, 5, 0, 4, 2]]

    # Computes horizon statically.
    horizon = 150

    task_type = collections.namedtuple('task_type', 'start end interval')

    # Creates jobs.
    all_tasks = {}
    for i in all_jobs:
        for j in all_machines:
            start_var = model.NewIntVar(0, horizon, 'start_%i_%i' % (i, j))
            duration = durations[i][j]
            end_var = model.NewIntVar(0, horizon, 'end_%i_%i' % (i, j))
            interval_var = model.NewIntervalVar(start_var, duration, end_var,
                                                'interval_%i_%i' % (i, j))
            all_tasks[(i, j)] = task_type(start=start_var,
                                          end=end_var,
                                          interval=interval_var)

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
        model.AddNoOverlap(job_intervals)

        arcs = []
        for j1 in range(len(job_intervals)):
            # Initial arc from the dummy node (0) to a task.
            start_lit = model.NewBoolVar('%i is first job' % j1)
            arcs.append([0, j1 + 1, start_lit])
            # Final arc from an arc to the dummy node.
            arcs.append([j1 + 1, 0, model.NewBoolVar('%i is last job' % j1)])

            for j2 in range(len(job_intervals)):
                if j1 == j2:
                    continue

                lit = model.NewBoolVar('%i follows %i' % (j2, j1))
                arcs.append([j1 + 1, j2 + 1, lit])

                # We add the reified precedence to link the literal with the
                # times of the two tasks.
                min_distance = distance_between_jobs(j1, j2)
                model.Add(job_starts[j2] >= job_ends[j1] +
                          min_distance).OnlyEnforceIf(lit)

        model.AddCircuit(arcs)

    # Precedences inside a job.
    for i in all_jobs:
        for j in range(0, machines_count - 1):
            model.Add(all_tasks[(i, j + 1)].start >= all_tasks[(i, j)].end)

    # Makespan objective.
    obj_var = model.NewIntVar(0, horizon, 'makespan')
    model.AddMaxEquality(
        obj_var, [all_tasks[(i, machines_count - 1)].end for i in all_jobs])
    model.Minimize(obj_var)

    # Solve model.
    solver = cp_model.CpSolver()
    status = solver.Solve(model)

    # Output solution.
    if status == cp_model.OPTIMAL:
        print('Optimal makespan: %i' % solver.ObjectiveValue())


jobshop_ft06_distance()
