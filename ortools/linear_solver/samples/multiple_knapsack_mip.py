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
"""Solve a multiple knapsack problem using a MIP solver."""
# [START import]
from ortools.linear_solver import pywraplp
# [END import]


# [START data_model]
def create_data_model():
    """Create the data for the example."""
    data = {}
    weights = [48, 30, 42, 36, 36, 48, 42, 42, 36, 24, 30, 30, 42, 36, 36]
    values = [10, 30, 25, 50, 35, 30, 15, 40, 30, 35, 45, 10, 20, 30, 25]
    data['weights'] = weights
    data['values'] = values
    data['num_items'] = len(weights)
    data['all_items'] = range(data['num_items'])
    data['bin_capacities'] = [100, 100, 100, 100, 100]
    data['num_bins'] = len(data['bin_capacities'])
    data['all_bins'] = range(data['num_bins'])
    return data

# [END data_model]


# [START solution_printer]
def print_solutions(data, objective, x):
    """Display the solution."""
    print('Total packed value:', objective.Value())
    total_weight = 0
    for b in data['all_bins']:
        print(f'Bin {b}\n')
        bin_weight = 0
        bin_value = 0
        for idx, weight in enumerate(data['weights']):
            if x[idx, b].solution_value() > 0:
                print(f"Item {idx} - weight: {weight} value: {data['values'][idx]}")
                bin_weight += weight
                bin_value += data['values'][idx]
        print(f'Packed bin weight: {bin_weight}')
        print(f'Packed bin value: {bin_value}\n')
        total_weight += bin_weight
    print(f'Total packed weight: {total_weight}')
# [END solution_printer]


def main():
    # [START data]
    data = create_data_model()
    # [END data]

    # Create the mip solver with the SCIP backend.
    # [START solver]
    solver = pywraplp.Solver.CreateSolver('SCIP')
    # [END solver]

    # Main variables.
    # [START variables]
    # Variables
    # x[i, j] = 1 if item i is packed in bin j.
    x = {}
    for idx in data['all_items']:
        for b in data['all_bins']:
            x[idx, b] = solver.IntVar(0, 1, f'x_{idx}_{b}')
    # [END variables]

    # Constraints
    # [START constraints]
    # Each item can be in at most one bin.
    for idx in data['all_items']:
        solver.Add(sum(x[idx, b] for b in data['all_bins']) <= 1)

    # The amount packed in each bin cannot exceed its capacity.
    for b in data['all_bins']:
        solver.Add(
            sum(x[i, b] * data['weights'][i]
                for i in data['all_items']) <= data['bin_capacities'][b])
    # [END constraints]

    # Objective
    # [START objective]
    # Maximize total value of packed items.
    objective = solver.Objective()
    for i in data['all_items']:
        for b in data['all_bins']:
            objective.SetCoefficient(x[i, b], data['values'][i])
    objective.SetMaximization()
    # [END objective]

    # [START solve]
    status = solver.Solve()
    # [END solve]

    # [START print_solution]
    if status == pywraplp.Solver.OPTIMAL:
        print_solutions(data, objective, x)
    else:
        print('The problem does not have an optimal solution.')
    # [END print_solution]


if __name__ == '__main__':
    main()
# [END program]
