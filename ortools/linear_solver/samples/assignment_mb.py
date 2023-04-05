#!/usr/bin/env python3
# Copyright 2010-2022 Google LLC
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
"""MIP example that solves an assignment problem."""
# [START program]
# [START import]
import numpy as np

from ortools.linear_solver.python import model_builder
# [END import]


def main():
    # Data
    # [START data_model]
    costs = np.array([
        [90, 80, 75, 70],
        [35, 85, 55, 65],
        [125, 95, 90, 95],
        [45, 110, 95, 115],
        [50, 100, 90, 100],
    ])
    num_workers, num_tasks = costs.shape
    # [END data_model]

    # Solver
    # Create the model.
    model = model_builder.ModelBuilder()
    # [END model]

    # Variables
    # [START variables]
    # x[i, j] is an array of 0-1 variables, which will be 1
    # if worker i is assigned to task j.
    x = model.new_bool_var_array(shape=[num_workers, num_tasks], name='x')  # pytype: disable=wrong-arg-types  # numpy-scalars
    # [END variables]

    # Constraints
    # [START constraints]
    # Each worker is assigned to at most 1 task.
    for i in range(num_workers):
        model.add(np.sum(x[i, :]) <= 1)

    # Each task is assigned to exactly one worker.
    for j in range(num_tasks):
        model.add(np.sum(x[:, j]) == 1)
    # [END constraints]

    # Objective
    # [START objective]
    model.minimize(np.dot(x.flatten(), costs.flatten()))
    # [END objective]

    # [START solve]
    # Create the solver with the CP-SAT backend, and solve the model.
    solver = model_builder.ModelSolver('sat')
    status = solver.solve(model)
    # [END solve]

    # Print solution.
    # [START print_solution]
    if (status == model_builder.SolveStatus.OPTIMAL or
            status == model_builder.SolveStatus.FEASIBLE):
        print(f'Total cost = {solver.objective_value}\n')
        for i in range(num_workers):
            for j in range(num_tasks):
                # Test if x[i,j] is 1 (with tolerance for floating point arithmetic).
                if solver.value(x[i, j]) > 0.5:
                    print(f'Worker {i} assigned to task {j}.' +
                          f' Cost: {costs[i][j]}')
    else:
        print('No solution found.')
    # [END print_solution]


if __name__ == '__main__':
    main()
# [END program]
