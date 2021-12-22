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
        [90, 76, 75, 70],
        [35, 85, 55, 65],
        [125, 95, 90, 105],
        [45, 110, 95, 115],
        [60, 105, 80, 75],
        [45, 65, 110, 95],
    ]
    num_workers = len(costs)
    num_tasks = len(costs[0])

    team1 = [0, 2, 4]
    team2 = [1, 3, 5]
    # Maximum total of tasks for any team
    team_max = 2
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
            x[worker, task] = solver.BoolVar(f'x[{worker},{task}]')
    # [END variables]

    # Constraints
    # [START constraints]
    # Each worker is assigned at most 1 task.
    for worker in range(num_workers):
        solver.Add(
            solver.Sum([x[worker, task] for task in range(num_tasks)]) <= 1)

    # Each task is assigned to exactly one worker.
    for task in range(num_tasks):
        solver.Add(
            solver.Sum([x[worker, task] for worker in range(num_workers)]) == 1)

    # Each team takes at most two tasks.
    team1_tasks = []
    for worker in team1:
        for task in range(num_tasks):
            team1_tasks.append(x[worker, task])
    solver.Add(solver.Sum(team1_tasks) <= team_max)

    team2_tasks = []
    for worker in team2:
        for task in range(num_tasks):
            team2_tasks.append(x[worker, task])
    solver.Add(solver.Sum(team2_tasks) <= team_max)
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
    print(f'Time = {solver.WallTime()} ms')
    # [END print_solution]


if __name__ == '__main__':
    main()
# [END program]
