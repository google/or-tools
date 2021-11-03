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
"""Solve a simple assignment problem."""
# [START import]
from ortools.sat.python import cp_model
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

    # Each team takes at most two tasks.
    team1_tasks = []
    for worker in team1:
        for task in range(num_tasks):
            team1_tasks.append(x[worker, task])
    model.Add(sum(team1_tasks) <= team_max)

    team2_tasks = []
    for worker in team2:
        for task in range(num_tasks):
            team2_tasks.append(x[worker, task])
    model.Add(sum(team2_tasks) <= team_max)
    # [END constraints]

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
