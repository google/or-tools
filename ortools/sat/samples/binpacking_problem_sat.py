#!/usr/bin/env python3
# Copyright 2010-2025 Google LLC
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

# [START program]
"""Solves a binpacking problem using the CP-SAT solver."""


from ortools.sat.python import cp_model


def binpacking_problem_sat():
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
            x[(i, b)] = model.new_int_var(0, num_copies, f"x[{i},{b}]")

    # Load variables.
    load = [model.new_int_var(0, bin_capacity, f"load[{b}]") for b in all_bins]

    # Slack variables.
    slacks = [model.new_bool_var(f"slack[{b}]") for b in all_bins]

    # Links load and x.
    for b in all_bins:
        model.add(load[b] == sum(x[(i, b)] * items[i][0] for i in all_items))

    # Place all items.
    for i in all_items:
        model.add(sum(x[(i, b)] for b in all_bins) == items[i][1])

    # Links load and slack through an equivalence relation.
    safe_capacity = bin_capacity - slack_capacity
    for b in all_bins:
        # slack[b] => load[b] <= safe_capacity.
        model.add(load[b] <= safe_capacity).only_enforce_if(slacks[b])
        # not(slack[b]) => load[b] > safe_capacity.
        model.add(load[b] > safe_capacity).only_enforce_if(~slacks[b])

    # Maximize sum of slacks.
    model.maximize(sum(slacks))

    # Solves and prints out the solution.
    solver = cp_model.CpSolver()
    status = solver.solve(model)
    print(f"solve status: {solver.status_name(status)}")
    if status == cp_model.OPTIMAL:
        print(f"Optimal objective value: {solver.objective_value}")
    print("Statistics")
    print(f"  - conflicts : {solver.num_conflicts}")
    print(f"  - branches  : {solver.num_branches}")
    print(f"  - wall time : {solver.wall_time}s")


binpacking_problem_sat()
# [END program]
