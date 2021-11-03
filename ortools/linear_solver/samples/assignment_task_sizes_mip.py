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
"""MIP example that solves an assignment problem."""
# [START import]
from ortools.linear_solver import pywraplp
# [END import]


def main():
    # Data
    # [START data]
    costs = [
        [90, 76, 75, 70, 50, 74, 12, 68],
        [35, 85, 55, 65, 48, 101, 70, 83],
        [125, 95, 90, 105, 59, 120, 36, 73],
        [45, 110, 95, 115, 104, 83, 37, 71],
        [60, 105, 80, 75, 59, 62, 93, 88],
        [45, 65, 110, 95, 47, 31, 81, 34],
        [38, 51, 107, 41, 69, 99, 115, 48],
        [47, 85, 57, 71, 92, 77, 109, 36],
        [39, 63, 97, 49, 118, 56, 92, 61],
        [47, 101, 71, 60, 88, 109, 52, 90],
    ]
    num_workers = len(costs)
    num_tasks = len(costs[0])

    task_sizes = [10, 7, 3, 12, 15, 4, 11, 5]
    # Maximum total of task sizes for any worker
    total_size_max = 15
    # [END data]

    # Solver
    # [START solver]
    # Create the mip solver with the SCIP backend.
    solver = pywraplp.Solver.CreateSolver('SCIP')

    # [END solver]

    # Variables
    # [START variables]
    # x[i, j] is an array of 0-1 variables, which will be 1
    # if worker i is assigned to task j.
    x = {}
    for worker in range(num_workers):
        for task in range(num_tasks):
            x[worker, task] = solver.IntVar(0, 1, f'x[{worker},{task}]')
    # [END variables]

    # Constraints
    # [START constraints]
    # The total size of the tasks each worker takes on is at most total_size_max.
    for worker in range(num_workers):
        solver.Add(
            solver.Sum([
                task_sizes[task] * x[worker, task] for task in range(num_tasks)
            ]) <= total_size_max)

    # Each task is assigned to exactly one worker.
    for task in range(num_tasks):
        solver.Add(
            solver.Sum([x[worker, task] for worker in range(num_workers)]) == 1)
    # [END constraints]

    # Objective
    # [START objective]
    objective_terms = []
    for worker in range(num_workers):
        for task in range(num_tasks):
            objective_terms.append(costs[worker][task] * x[worker, task])
    solver.Minimize(solver.Sum(objective_terms))
    # [END objective]

    # Solve
    # [START solve]
    status = solver.Solve()
    # [END solve]

    # Print solution.
    # [START print_solution]
    if status == pywraplp.Solver.OPTIMAL or status == pywraplp.Solver.FEASIBLE:
        print(f'Total cost = {solver.Objective().Value()}\n')
        for worker in range(num_workers):
            for task in range(num_tasks):
                if x[worker, task].solution_value() > 0.5:
                    print(f'Worker {worker} assigned to task {task}.' +
                          f' Cost = {costs[worker][task]}')
    else:
        print('No solution found.')
    # [END print_solution]


if __name__ == '__main__':
    main()
# [END program]
