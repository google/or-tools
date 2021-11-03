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
from ortools.linear_solver import pywraplp
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
    # [END data]

    # Allowed groups of workers:
    # [START allowed_groups]
    group1 = [  # Subgroups of workers 0 - 3
        [2, 3],
        [1, 3],
        [1, 2],
        [0, 1],
        [0, 2],
    ]

    group2 = [  # Subgroups of workers 4 - 7
        [6, 7],
        [5, 7],
        [5, 6],
        [4, 5],
        [4, 7],
    ]

    group3 = [  # Subgroups of workers 8 - 11
        [10, 11],
        [9, 11],
        [9, 10],
        [8, 10],
        [8, 11],
    ]

    allowed_groups = []
    for workers_g1 in group1:
        for workers_g2 in group2:
            for workers_g3 in group3:
                allowed_groups.append(workers_g1 + workers_g2 + workers_g3)
    # [END allowed_groups]

    # [START solves]
    min_val = 1e6
    total_time = 0
    for group in allowed_groups:
        res = assignment(costs, group)
        status_tmp = res[0]
        solver_tmp = res[1]
        x_tmp = res[2]
        if status_tmp == pywraplp.Solver.OPTIMAL or status_tmp == pywraplp.Solver.FEASIBLE:
            if solver_tmp.Objective().Value() < min_val:
                min_val = solver_tmp.Objective().Value()
                min_group = group
                min_solver = solver_tmp
                min_x = x_tmp
        total_time += solver_tmp.WallTime()
    # [END solves]

    # Print best solution.
    # [START print_solution]
    if min_val < 1e6:
        print(f'Total cost = {min_solver.Objective().Value()}\n')
        num_tasks = len(costs[0])
        for worker in min_group:
            for task in range(num_tasks):
                if min_x[worker, task].solution_value() > 0.5:
                    print(f'Worker {worker} assigned to task {task}.' +
                          f' Cost = {costs[worker][task]}')
    else:
        print('No solution found.')
    print(f'Time = {total_time} ms')
    # [END print_solution]


def assignment(costs, group):
    """Solve the assignment problem for one allowed group combinaison."""
    num_tasks = len(costs[1])
    # Solver
    # [START solver]
    # Create the mip solver with the SCIP backend.
    solver = pywraplp.Solver.CreateSolver('SCIP')
    # [END solver]

    # Variables
    # [START variables]
    # x[worker, task] is an array of 0-1 variables, which will be 1
    # if the worker is assigned to the task.
    x = {}
    for worker in group:
        for task in range(num_tasks):
            x[worker, task] = solver.BoolVar(f'x[{worker},{task}]')
    # [END variables]

    # Constraints
    # [START constraints]
    # The total size of the tasks each worker takes on is at most total_size_max.
    for worker in group:
        solver.Add(
            solver.Sum([x[worker, task] for task in range(num_tasks)]) <= 1)

    # Each task is assigned to exactly one worker.
    for task in range(num_tasks):
        solver.Add(solver.Sum([x[worker, task] for worker in group]) == 1)
    # [END constraints]

    # Objective
    # [START objective]
    objective_terms = []
    for worker in group:
        for task in range(num_tasks):
            objective_terms.append(costs[worker][task] * x[worker, task])
    solver.Minimize(solver.Sum(objective_terms))
    # [END objective]

    # Solve
    # [START solve]
    status = solver.Solve()
    # [END solve]

    return [status, solver, x]


if __name__ == '__main__':
    main()
# [END program]
