# Copyright 2010-2018 Google
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
"""Jobshop with maintenance tasks using the CP-SAT solver."""

from __future__ import absolute_import
from __future__ import print_function

import collections

from ortools.sat.python import cp_model


def main():
    """Solves a jobshop with maintenance on one machine."""
    model = cp_model.CpModel()

    jobs_data = [  # task = (machine_id, processing_time).
        [(0, 3), (1, 2), (2, 2)],  # Job0
        [(0, 2), (2, 1), (1, 4)],  # Job1
        [(1, 4), (2, 3)]  # Job2
    ]

    machines_count = 1 + max(task[0] for job in jobs_data for task in job)
    all_machines = range(machines_count)

    # Computes horizon dynamically as the sum of all durations.
    horizon = sum(task[1] for job in jobs_data for task in job)

    # Named tuple to store information about created variables.
    task_type = collections.namedtuple('Task', 'start end interval')
    # Named tuple to store information about created variables.
    assigned_task_type = collections.namedtuple('assigned_task_type',
                                                'start job index duration')

    # Creates job intervals and add to the corresponding machine lists.
    all_tasks = {}
    machine_to_intervals = collections.defaultdict(list)

    for j, job in enumerate(jobs_data):
        for t, task in enumerate(job):
            suffix = '_%i_%i' % (j, t)
            start_var = model.NewIntVar(0, horizon, 'start' + suffix)
            end_var = model.NewIntVar(0, horizon, 'end' + suffix)
            interval_var = model.NewIntervalVar(start_var, task[1], end_var,
                                                'interval' + suffix)
            all_tasks[j, t] = task_type(
                start=start_var, end=end_var, interval=interval_var)
            machine_to_intervals[task[0]].append(interval_var)

    # Add maintenance interval.
    machine_to_intervals[0].append(model.NewIntervalVar(4, 4, 8, 'weekend_0'))

    # Create disjuctive constraints.
    for m in all_machines:
        model.AddNoOverlap(machine_to_intervals[m])

    # Precedences inside a job.
    for j, job in enumerate(jobs_data):
        for t in range(len(job) - 1):
            model.Add(all_tasks[j, t + 1].start >= all_tasks[j, t].end)

    # Makespan objective.
    obj_var = model.NewIntVar(0, horizon, 'makespan')
    model.AddMaxEquality(
        obj_var,
        [all_tasks[j, len(job) - 1].end for j, job in enumerate(jobs_data)])
    model.Minimize(obj_var)

    # Solve model.
    solver = cp_model.CpSolver()
    status = solver.Solve(model)

    # Output solution.
    if status == cp_model.OPTIMAL:
        print('Optimal makespan: %i' % solver.ObjectiveValue())
        print()

        # Create one list of assigned tasks per machine.
        assigned_jobs = collections.defaultdict(list)
        for j, job in enumerate(jobs_data):
            for t, task in enumerate(job):
                machine = task[0]
                assigned_jobs[machine].append(
                    assigned_task_type(
                        start=solver.Value(all_tasks[j, t].start),
                        job=j,
                        index=t,
                        duration=task[1]))

          # Create per machine output lines.
        output = ''
        for machine in all_machines:
            # Sort by starting time.
            assigned_jobs[machine].sort()
            sol_line_tasks = '  - machine ' + str(machine) + ': '
            sol_line = '               '

            for assigned_task in assigned_jobs[machine]:
                name = 'job_%i_%i' % (assigned_task.job, assigned_task.index)
                # Add spaces to output to align columns.
                sol_line_tasks += '%-10s' % name
                start = assigned_task.start
                duration = jobs_data[assigned_task.job][assigned_task.index][1]

                sol_tmp = '[%i,%i]' % (start, start + duration)
                # Add spaces to output to align columns.
                sol_line += '%-10s' % sol_tmp

            sol_line += '\n'
            sol_line_tasks += '\n'
            output += sol_line_tasks
            output += sol_line

        # Finally print the solution found.
        print('Optimal Schedule')
        print(output)


if __name__ == '__main__':
    main()
