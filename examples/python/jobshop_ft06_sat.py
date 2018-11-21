# Copyright 2010-2018 Google LLC
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
"""This model implements a simple jobshop named ft06.

A jobshop is a standard scheduling problem when you must sequence a
series of task_types on a set of machines. Each job contains one task_type per
machine. The order of execution and the length of each job on each
machine is task_type dependent.

The objective is to minimize the maximum completion time of all
jobs. This is called the makespan.
"""
from __future__ import print_function

import collections

from ortools.sat.python import visualization
from ortools.sat.python import cp_model


def jobshop_ft06():
    """Solves the ft06 jobshop."""
    # Creates the solver.
    model = cp_model.CpModel()

    machines_count = 6
    jobs_count = 6
    all_machines = range(0, machines_count)
    all_jobs = range(0, jobs_count)

    durations = [[1, 3, 6, 7, 3, 6], [8, 5, 10, 10, 10, 4], [5, 4, 8, 9, 1, 7],
                 [5, 5, 5, 3, 8, 9], [9, 3, 5, 4, 3, 1], [3, 3, 9, 10, 4, 1]]

    machines = [[2, 0, 1, 3, 5, 4], [1, 2, 4, 5, 0, 3], [2, 3, 5, 0, 1, 4],
                [1, 0, 2, 3, 4, 5], [2, 1, 4, 5, 0, 3], [1, 3, 5, 0, 4, 2]]

    # Computes horizon dynamically.
    horizon = sum([sum(durations[i]) for i in all_jobs])

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
            all_tasks[(i, j)] = task_type(
                start=start_var, end=end_var, interval=interval_var)

    # Create disjuctive constraints.
    machine_to_jobs = {}
    for i in all_machines:
        machines_jobs = []
        for j in all_jobs:
            for k in all_machines:
                if machines[j][k] == i:
                    machines_jobs.append(all_tasks[(j, k)].interval)
        machine_to_jobs[i] = machines_jobs
        model.AddNoOverlap(machines_jobs)

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
        if visualization.RunFromIPython():
            starts = [[
                solver.Value(all_tasks[(i, j)][0]) for j in all_machines
            ] for i in all_jobs]
            visualization.DisplayJobshop(starts, durations, machines, 'FT06')
        else:
            print('Optimal makespan: %i' % solver.ObjectiveValue())


jobshop_ft06()
