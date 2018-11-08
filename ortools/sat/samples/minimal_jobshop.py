# Copyright 2010-2017 Google
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
"""Minimal jobshop example."""

# [START program]
from __future__ import print_function

import collections

# [START model]
# Import Python wrapper for or-tools CP-SAT solver.
from ortools.sat.python import cp_model


def main():
  """Minimal jobshop problem."""
  # Create the model.
  model = cp_model.CpModel()
  # [END model]

  # [START data]
  machines_count = 3
  jobs_count = 3
  all_machines = range(0, machines_count)
  all_jobs = range(0, jobs_count)
  # Define data.
  machines = [[0, 1, 2], [0, 2, 1], [1, 2]]

  processing_times = [[3, 2, 2], [2, 1, 4], [4, 3]]
  # [END data]

  # Compute horizon.
  horizon = 0
  for job in all_jobs:
    horizon += sum(processing_times[job])

  # [START variables]
  task_type = collections.namedtuple('task_type', 'start end interval')
  assigned_task_type = collections.namedtuple('assigned_task_type',
                                              'start job index')

  # Create jobs.
  all_tasks = {}
  for job in all_jobs:
    for index in range(0, len(machines[job])):
      start_var = model.NewIntVar(0, horizon, 'start_%i_%i' % (job, index))
      duration = processing_times[job][index]
      end_var = model.NewIntVar(0, horizon, 'end_%i_%i' % (job, index))
      interval_var = model.NewIntervalVar(start_var, duration, end_var,
                                          'interval_%i_%i' % (job, index))
      all_tasks[(job, index)] = task_type(
          start=start_var, end=end_var, interval=interval_var)
  # [END variables]

  # [START constraints]
  # Create and add disjunctive constraints.
  for machine in all_machines:
    intervals = []
    for job in all_jobs:
      for index in range(0, len(machines[job])):
        if machines[job][index] == machine:
          intervals.append(all_tasks[(job, index)].interval)
    model.AddNoOverlap(intervals)

  # Add precedence contraints.
  for job in all_jobs:
    for index in range(0, len(machines[job]) - 1):
      model.Add(
          all_tasks[(job, index + 1)].start >= all_tasks[(job, index)].end)
  # [END constraints]

  # [START objective]
  # Makespan objective.
  obj_var = model.NewIntVar(0, horizon, 'makespan')
  model.AddMaxEquality(
      obj_var,
      [all_tasks[(job, len(machines[job]) - 1)].end for job in all_jobs])
  model.Minimize(obj_var)
  # [END objective]

  # [START solver]
  # Solve model.
  solver = cp_model.CpSolver()
  status = solver.Solve(model)
  # [END solver]

  if status == cp_model.OPTIMAL:
    # [START solution_printing]
    # Print out makespan.
    print('Optimal Schedule Length: %i' % solver.ObjectiveValue())
    print()

    # Create one list of assigned tasks per machine.
    assigned_jobs = [[] for _ in range(machines_count)]
    for job in all_jobs:
      for index in range(len(machines[job])):
        machine = machines[job][index]
        assigned_jobs[machine].append(
            assigned_task_type(
                start=solver.Value(all_tasks[(job, index)].start),
                job=job,
                index=index))

    disp_col_width = 10
    sol_line = ''
    sol_line_tasks = ''

    print('Optimal Schedule', '\n')

    for machine in all_machines:
      # Sort by starting time.
      assigned_jobs[machine].sort()
      sol_line += 'Machine ' + str(machine) + ': '
      sol_line_tasks += 'Machine ' + str(machine) + ': '

      for assigned_task in assigned_jobs[machine]:
        name = 'job_%i_%i' % (assigned_task.job, assigned_task.index)
        # Add spaces to output to align columns.
        sol_line_tasks += name + ' ' * (disp_col_width - len(name))
        start = assigned_task.start
        duration = processing_times[assigned_task.job][assigned_task.index]

        sol_tmp = '[%i,%i]' % (start, start + duration)
        # Add spaces to output to align columns.
        sol_line += sol_tmp + ' ' * (disp_col_width - len(sol_tmp))

      sol_line += '\n'
      sol_line_tasks += '\n'

    print(sol_line_tasks)
    print('Time Intervals for task_types\n')
    print(sol_line)
    # [END solution_printing]


if __name__ == '__main__':
  main()
