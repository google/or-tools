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
# [START program]
"""Solve assignment problem for given group of workers."""
# [START import]
from ortools.sat.python import cp_model
# [END import]


def main():
    # Data
    # [START data]
    costs = [
        [90, 76, 75, 70, 50, 74],
        [35, 85, 55, 65, 48, 101],
        [125, 95, 90, 105, 59, 120],
        [45, 110, 95, 115, 104, 83],
        [60, 105, 80, 75, 59, 62],
        [45, 65, 110, 95, 47, 31],
        [38, 51, 107, 41, 69, 99],
        [47, 85, 57, 71, 92, 77],
        [39, 63, 97, 49, 118, 56],
        [47, 101, 71, 60, 88, 109],
        [17, 39, 103, 64, 61, 92],
        [101, 45, 83, 59, 92, 27],
    ]
    num_workers = len(costs)
    num_tasks = len(costs[0])
    # [END data]

    # Allowed groups of workers:
    # [START allowed_groups]
    group1 = [
        [0, 0, 1, 1],  # Workers 2, 3
        [0, 1, 0, 1],  # Workers 1, 3
        [0, 1, 1, 0],  # Workers 1, 2
        [1, 1, 0, 0],  # Workers 0, 1
        [1, 0, 1, 0],  # Workers 0, 2
    ]

    group2 = [
        [0, 0, 1, 1],  # Workers 6, 7
        [0, 1, 0, 1],  # Workers 5, 7
        [0, 1, 1, 0],  # Workers 5, 6
        [1, 1, 0, 0],  # Workers 4, 5
        [1, 0, 0, 1],  # Workers 4, 7
    ]

    group3 = [
        [0, 0, 1, 1],  # Workers 10, 11
        [0, 1, 0, 1],  # Workers 9, 11
        [0, 1, 1, 0],  # Workers 9, 10
        [1, 0, 1, 0],  # Workers 8, 10
        [1, 0, 0, 1],  # Workers 8, 11
    ]
    # [END allowed_groups]

    # Model
    # [START model]
    model = cp_model.CpModel()
    # [END model]

    # Variables
    # [START variables]
    x = {}
    for worker in range(num_workers):
        for task in range(num_tasks):
            x[worker, task] = model.NewBoolVar(f'x[{worker},{task}]')
    # [END variables]

    # Constraints
    # [START constraints]
    # Each worker is assigned to at most one task.
    for worker in range(num_workers):
        model.Add(sum(x[worker, task] for task in range(num_tasks)) <= 1)

    # Each task is assigned to exactly one worker.
    for task in range(num_tasks):
        model.Add(sum(x[worker, task] for worker in range(num_workers)) == 1)
    # [END constraints]

    # [START assignments]
    # Create variables for each worker, indicating whether they work on some task.
    work = {}
    for worker in range(num_workers):
        work[worker] = model.NewBoolVar(f'work[{worker}]')

    for worker in range(num_workers):
        for task in range(num_tasks):
            model.Add(work[worker] == sum(
                x[worker, task] for task in range(num_tasks)))

    # Define the allowed groups of worders
    model.AddAllowedAssignments([work[0], work[1], work[2], work[3]], group1)
    model.AddAllowedAssignments([work[4], work[5], work[6], work[7]], group2)
    model.AddAllowedAssignments([work[8], work[9], work[10], work[11]], group3)
    # [END assignments]

    # Objective
    # [START objective]
    objective_terms = []
    for worker in range(num_workers):
        for task in range(num_tasks):
            objective_terms.append(costs[worker][task] * x[worker, task])
    model.Minimize(sum(objective_terms))
    # [END objective]

    # Solve
    # [START solve]
    solver = cp_model.CpSolver()
    status = solver.Solve(model)
    # [END solve]

    # Print solution.
    # [START print_solution]
    if status == cp_model.OPTIMAL or status == cp_model.FEASIBLE:
        print(f'Total cost = {solver.ObjectiveValue()}\n')
        for worker in range(num_workers):
            for task in range(num_tasks):
                if solver.BooleanValue(x[worker, task]):
                    print(f'Worker {worker} assigned to task {task}.' +
                          f' Cost = {costs[worker][task]}')
    else:
        print('No solution found.')
    # [END print_solution]


if __name__ == '__main__':
    main()
# [END program]
