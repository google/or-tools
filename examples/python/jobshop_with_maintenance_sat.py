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

    jobs = [[(5, 0, 20)], [(10, 1, 20)], [(6, 3, 60)], [(10, 2, 100)],
            [(50, 4, 500)], [(12, 4, 50)], [(3, 0, 300)], [(5, 4, 500)],
            [(3, 3, 300)], [(5, 4, 500)]]

    machines_count = max(task[1] for job in jobs for task in job) + 1
    all_machines = range(machines_count)

    # Computes horizon dynamically as the sum of all durations.
    horizon = sum(task[0] for job in jobs for task in job)

    # Named tuple to store information about created variables.
    task_type = collections.namedtuple('Task', 'start end interval')
    # Named tuple to store information about created variables.
    assigned_task_type = collections.namedtuple('assigned_task_type',
                                                'start job index duration')

    # Creates job intervals and add to the corresponding machine lists.
    all_tasks = {}
    machine_to_intervals = collections.defaultdict(list)

    for j, job in enumerate(jobs):
        for t, task in enumerate(job):
            suffix = '_%i_%i' % (j, t)
            start_var = model.NewIntVar(0, horizon, 'start' + suffix)
            end_var = model.NewIntVar(0, horizon, 'end' + suffix)
            interval_var = model.NewIntervalVar(start_var, task[0], end_var,
                                                'interval' + suffix)
            all_tasks[j, t] = task_type(
                start=start_var, end=end_var, interval=interval_var)
            machine_to_intervals[task[1]].append(interval_var)

    # Add maintenance interval.
    machine_to_intervals[0].append(model.NewIntervalVar(6, 4, 10, 'weekend_0'))

    # Create disjuctive constraints.
    for m in all_machines:
        model.AddNoOverlap(machine_to_intervals[m])

    # Add deadlines on tasks.
    for j, job in enumerate(jobs):
        for t, task in enumerate(job):
            model.Add(all_tasks[j, t].end <= task[2])

    # Precedences inside a job.
    for j, job in enumerate(jobs):
        for t in range(len(job) - 1):
            model.Add(all_tasks[j, t + 1].start >= all_tasks[j, t].end)

    # Makespan objective.
    obj_var = model.NewIntVar(0, horizon, 'makespan')
    model.AddMaxEquality(
        obj_var,
        [all_tasks[j, len(job) - 1].end for j, job in enumerate(jobs)])
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
        for j, job in enumerate(jobs):
            for t, task in enumerate(job):
                machine = task[1]
                assigned_jobs[machine].append(
                    assigned_task_type(
                        start=solver.Value(all_tasks[j, t].start),
                        job=j,
                        index=t,
                        duration=task[0]))

        # Create per machine output lines.
        sol_line = ''
        sol_line_tasks = ''

        for machine in all_machines:
            # Sort by starting time.
            assigned_jobs[machine].sort()
            sol_line += '  machine ' + str(machine) + ': '
            sol_line_tasks += '  machine ' + str(machine) + ': '

            for assigned_task in assigned_jobs[machine]:
                name = 'job_%i_%i' % (assigned_task.job, assigned_task.index)
                sol_line_tasks += '%10s' % name

                start = assigned_task.start
                duration = assigned_task.duration
                sol_tmp = '[%2i,%2i]' % (start, start + duration)
                sol_line += '%10s' % sol_tmp

            sol_line += '\n'
            sol_line_tasks += '\n'

        # Finally print the solution found.
        print('Optimal Schedule')
        print(sol_line_tasks)
        print('Task Time Intervals')
        print(sol_line)


if __name__ == '__main__':
    main()
