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
"""Code sample to demonstrates how to rank intervals."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


def RankingSample():
  model = cp_model.CpModel()
  horizon = 100
  num_tasks = 4
  all_tasks = range(num_tasks)

  starts = []
  ends = []
  intervals = []
  presences = []
  ranks = []

  # Create intervals, half of them are optional.
  for t in all_tasks:
    start = model.NewIntVar(0, horizon, 'start_%i' % t)
    duration = t + 1
    end = model.NewIntVar(0, horizon, 'end_%i' % t)
    if t < num_tasks / 2:
      interval = model.NewIntervalVar(start, duration, end, 'interval_%i' % t)
      presence = True
    else:
      presence = model.NewBoolVar('presence_%i' % t)
      interval = model.NewOptionalIntervalVar(start, duration, end, presence,
                                              'o_interval_%i' % t)
    starts.append(start)
    ends.append(end)
    intervals.append(interval)
    presences.append(presence)

    # Ranks = -1 if and only if the tasks is not performed.
    ranks.append(model.NewIntVar(-1, num_tasks - 1, 'rank_%i' % t))

  # Add NoOverlap constraint.
  model.AddNoOverlap(intervals)

  # Create precedence variables.
  precedences = {}
  for i in all_tasks:
    for j in all_tasks:
      if i == j:
        precedences[(i, j)] = presences[i]
      else:
        prec = model.NewBoolVar('%i before %i' % (i, j))
        precedences[(i, j)] = prec
        model.Add(starts[i] < starts[j]).OnlyEnforceIf(prec)

  # Treat optional intervals.
  for i in range(num_tasks - 1):
    for j in range(i + 1, num_tasks):
      tmp_array = [precedences[(i, j)], precedences[(j, i)]]
      if presences[i] != True:
          tmp_array.append(presences[i].Not())
          model.AddImplication(presences[i].Not(), precedences[(i, j)].Not())
          model.AddImplication(presences[i].Not(), precedences[(j, i)].Not())
      if presences[j] != True:
          tmp_array.append(presences[j].Not())
          model.AddImplication(presences[j].Not(), precedences[(i, j)].Not())
          model.AddImplication(presences[j].Not(), precedences[(j, i)].Not())
      model.AddBoolOr(tmp_array)
      # Redundant constraint
      model.AddImplication(precedences[(i, j)], precedences[(j, i)].Not())
      model.AddImplication(precedences[(j, i)], precedences[(i, j)].Not())

  # Link precedences and ranks.
  for i in all_tasks:
    model.Add(ranks[i] == sum(precedences[(j, i)] for j in all_tasks) - 1)

  # Create makespan variable
  makespan = model.NewIntVar(0, horizon, 'makespan')
  for t in all_tasks:
    if presences[t] == True:
      model.Add(ends[t] <= makespan)
    else:
      model.Add(ends[t] <= makespan).OnlyEnforceIf(presences[t])

  # Minimize makespan - fixed gain per tasks performed.
  # As the fixed cost is less that the duration of the last interval,
  # the solver will not perform the last interval.
  model.Minimize(makespan - 3 * sum(presences[t] for t in all_tasks))

  # Solve model.
  solver = cp_model.CpSolver()
  status = solver.Solve(model)

  if status == cp_model.OPTIMAL:
    # Print out makespan and the start times for all tasks.
    print('Optimal cost: %i' % solver.ObjectiveValue())
    print('Makespan: %i' % solver.Value(makespan))
    for t in all_tasks:
      if solver.Value(presences[t]):
        print('Task %i starts at %i' % (t, solver.Value(starts[t])))
  else:
    print('Solver exited with nonoptimal status: %i' % status)


RankingSample()
