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
"""Code sample to demonstrates how to rank intervals."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from ortools.sat.python import cp_model


def RankTasks(model, starts, presences, ranks):
    """This method adds constraints and variables to links tasks and ranks.

  This method assumes that all starts are disjoint, meaning that all tasks have
  a strictly positive duration, and they appear in the same NoOverlap
  constraint.

  Args:
    model: The CpModel to add the constraints to.
    starts: The array of starts variables of all tasks.
    presences: The array of presence variables of all tasks.
    ranks: The array of rank variables of all tasks.
  """

    num_tasks = len(starts)
    all_tasks = range(num_tasks)

    # Creates precedence variables between pairs of intervals.
    precedences = {}
    for i in all_tasks:
        for j in all_tasks:
            if i == j:
                precedences[(i, j)] = presences[i]
            else:
                prec = model.NewBoolVar('%i before %i' % (i, j))
                precedences[(i, j)] = prec
                model.Add(starts[i] < starts[j]).OnlyEnforceIf(prec)

    # Treats optional intervals.
    for i in range(num_tasks - 1):
        for j in range(i + 1, num_tasks):
            tmp_array = [precedences[(i, j)], precedences[(j, i)]]
            if presences[i] != 1:
                tmp_array.append(presences[i].Not())
                # Makes sure that if i is not performed, all precedences are false.
                model.AddImplication(presences[i].Not(), precedences[(i,
                                                                      j)].Not())
                model.AddImplication(presences[i].Not(), precedences[(j,
                                                                      i)].Not())
            if presences[j] != 1:
                tmp_array.append(presences[j].Not())
                # Makes sure that if j is not performed, all precedences are false.
                model.AddImplication(presences[j].Not(), precedences[(i,
                                                                      j)].Not())
                model.AddImplication(presences[j].Not(), precedences[(j,
                                                                      i)].Not())
            # The following bool_or will enforce that for any two intervals:
            #    i precedes j or j precedes i or at least one interval is not
            #        performed.
            model.AddBoolOr(tmp_array)
            # Redundant constraint: it propagates early that at most one precedence
            # is true.
            model.AddImplication(precedences[(i, j)], precedences[(j, i)].Not())
            model.AddImplication(precedences[(j, i)], precedences[(i, j)].Not())

    # Links precedences and ranks.
    for i in all_tasks:
        model.Add(ranks[i] == sum(precedences[(j, i)] for j in all_tasks) - 1)


def RankingSampleSat():
    """Ranks tasks in a NoOverlap constraint."""

    model = cp_model.CpModel()
    horizon = 100
    num_tasks = 4
    all_tasks = range(num_tasks)

    starts = []
    ends = []
    intervals = []
    presences = []
    ranks = []

    # Creates intervals, half of them are optional.
    for t in all_tasks:
        start = model.NewIntVar(0, horizon, 'start_%i' % t)
        duration = t + 1
        end = model.NewIntVar(0, horizon, 'end_%i' % t)
        if t < num_tasks / 2:
            interval = model.NewIntervalVar(start, duration, end,
                                            'interval_%i' % t)
            presence = True
        else:
            presence = model.NewBoolVar('presence_%i' % t)
            interval = model.NewOptionalIntervalVar(
                start, duration, end, presence, 'o_interval_%i' % t)
        starts.append(start)
        ends.append(end)
        intervals.append(interval)
        presences.append(presence)

        # Ranks = -1 if and only if the tasks is not performed.
        ranks.append(model.NewIntVar(-1, num_tasks - 1, 'rank_%i' % t))

    # Adds NoOverlap constraint.
    model.AddNoOverlap(intervals)

    # Adds ranking constraint.
    RankTasks(model, starts, presences, ranks)

    # Adds a constraint on ranks.
    model.Add(ranks[0] < ranks[1])

    # Creates makespan variable.
    makespan = model.NewIntVar(0, horizon, 'makespan')
    for t in all_tasks:
        if presences[t] == 1:
            model.Add(ends[t] <= makespan)
        else:
            model.Add(ends[t] <= makespan).OnlyEnforceIf(presences[t])

    # Minimizes makespan - fixed gain per tasks performed.
    # As the fixed cost is less that the duration of the last interval,
    # the solver will not perform the last interval.
    model.Minimize(2 * makespan - 7 * sum(presences[t] for t in all_tasks))

    # Solves the model model.
    solver = cp_model.CpSolver()
    status = solver.Solve(model)

    if status == cp_model.OPTIMAL:
        # Prints out the makespan and the start times and ranks of all tasks.
        print('Optimal cost: %i' % solver.ObjectiveValue())
        print('Makespan: %i' % solver.Value(makespan))
        for t in all_tasks:
            if solver.Value(presences[t]):
                print('Task %i starts at %i with rank %i' %
                      (t, solver.Value(starts[t]), solver.Value(ranks[t])))
            else:
                print('Task %i in not performed and ranked at %i' %
                      (t, solver.Value(ranks[t])))
    else:
        print('Solver exited with nonoptimal status: %i' % status)


RankingSampleSat()
