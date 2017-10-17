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

"""This model implements a simple jobshop named ft06.

A jobshop is a standard scheduling problem when you must sequence a
series of tasks on a set of machines. Each job contains one task per
machine. The order of execution and the length of each job on each
machine is task dependent.

The objective is to minimize the maximum completion time of all
jobs. This is called the makespan.
"""


from ortools.constraint_solver import pywrapcp


class Dist:
  def __init__(self):
    pass

  def distance(self, x, y):
    return abs(x - y)


def main():
  # Creates the solver.
  solver = pywrapcp.Solver('jobshop ft06')

  machines_count = 6
  jobs_count = 6
  all_machines = range(0, machines_count)
  all_jobs = range(0, jobs_count)

  durations = [[1, 3, 6, 7, 3, 6],
               [8, 5, 10, 10, 10, 4],
               [5, 4, 8, 9, 1, 7],
               [5, 5, 5, 3, 8, 9],
               [9, 3, 5, 4, 3, 1],
               [3, 3, 9, 10, 4, 1]]

  machines = [[2, 0, 1, 3, 5, 4],
              [1, 2, 4, 5, 0, 3],
              [2, 3, 5, 0, 1, 4],
              [1, 0, 2, 3, 4, 5],
              [2, 1, 4, 5, 0, 3],
              [1, 3, 5, 0, 4, 2]]

  # Computes horizon dynamically.
  horizon = sum([sum(durations[i]) for i in all_jobs])

  # Creates jobs.
  all_tasks = {}
  for i in all_jobs:
    for j in all_machines:
      all_tasks[(i, j)] = solver.FixedDurationIntervalVar(0,
                                                          horizon,
                                                          durations[i][j],
                                                          False,
                                                          'Job_%i_%i' % (i, j))

  # Creates sequence variables and add disjuctive constraints.
  all_sequences = {}
  all_transitions = []
  for i in all_machines:
    machines_jobs = []
    for j in all_jobs:
      for k in all_machines:
        if machines[j][k] == i:
          machines_jobs.append(all_tasks[(j, k)])
    disj = solver.DisjunctiveConstraint(machines_jobs, 'machine %i' % i)
    distance_obj = Dist()
    distance_callback = distance_obj.distance
    # Store all instances of the distance callbacks to have the same
    # life cycle as the solver.
    all_transitions.append(distance_callback)
    disj.SetTransitionTime(distance_callback)
    all_sequences[i] = disj.SequenceVar()
    solver.Add(disj)

  # Makespan objective.
  obj_var = solver.Max([all_tasks[(i, machines_count - 1)].EndExpr()
                        for i in all_jobs])
  objective = solver.Minimize(obj_var, 1)

  # Precedences inside a job.
  for i in all_jobs:
    for j in range(0, machines_count - 1):
      solver.Add(all_tasks[(i, j + 1)].StartsAfterEnd(all_tasks[(i, j)]))

  # Creates search phases.
  vars_phase = solver.Phase([obj_var],
                            solver.CHOOSE_FIRST_UNBOUND,
                            solver.ASSIGN_MIN_VALUE)
  sequence_phase = solver.Phase([all_sequences[i] for i in all_machines],
                                solver.SEQUENCE_DEFAULT)
  main_phase = solver.Compose([sequence_phase, vars_phase])

  # Creates the search log.
  search_log = solver.SearchLog(100, obj_var)

  # Solves the problem.
  solver.Solve(main_phase, [search_log, objective])


if __name__ == '__main__':
  main()
