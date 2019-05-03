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
"""Solves a binpacking problem using the CP-SAT solver."""

from __future__ import print_function

from ortools.sat.python import cp_model


def BinpackingProblemSat():
    """Solves a bin-packing problem using the CP-SAT solver."""
    # Data.
    bin_capacity = 100
    slack_capacity = 20
    num_bins = 5
    all_bins = range(num_bins)

    items = [(20, 6), (15, 6), (30, 4), (45, 3)]
    num_items = len(items)
    all_items = range(num_items)

    # Model.
    model = cp_model.CpModel()

    # Main variables.
    x = {}
    for i in all_items:
        num_copies = items[i][1]
        for b in all_bins:
            x[(i, b)] = model.NewIntVar(0, num_copies, 'x_%i_%i' % (i, b))

    # Load variables.
    load = [model.NewIntVar(0, bin_capacity, 'load_%i' % b) for b in all_bins]

    # Slack variables.
    slacks = [model.NewBoolVar('slack_%i' % b) for b in all_bins]

    # Links load and x.
    for b in all_bins:
        model.Add(load[b] == sum(x[(i, b)] * items[i][0] for i in all_items))

    # Place all items.
    for i in all_items:
        model.Add(sum(x[(i, b)] for b in all_bins) == items[i][1])

    # Links load and slack through an equivalence relation.
    safe_capacity = bin_capacity - slack_capacity
    for b in all_bins:
        # slack[b] => load[b] <= safe_capacity.
        model.Add(load[b] <= safe_capacity).OnlyEnforceIf(slacks[b])
        # not(slack[b]) => load[b] > safe_capacity.
        model.Add(load[b] > safe_capacity).OnlyEnforceIf(slacks[b].Not())

    # Maximize sum of slacks.
    model.Maximize(sum(slacks))

    # Solves and prints out the solution.
    solver = cp_model.CpSolver()
    status = solver.Solve(model)
    print('Solve status: %s' % solver.StatusName(status))
    if status == cp_model.OPTIMAL:
        print('Optimal objective value: %i' % solver.ObjectiveValue())
    print('Statistics')
    print('  - conflicts : %i' % solver.NumConflicts())
    print('  - branches  : %i' % solver.NumBranches())
    print('  - wall time : %f s' % solver.WallTime())


BinpackingProblemSat()
