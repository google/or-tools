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
"""Solve a multiple knapsack problem using a MIP solver."""
# [START program]
# [START import]
from ortools.linear_solver import pywraplp
# [END import]


# [START program_part1]
# [START data_model]
def create_data_model():
    """Create the data for the example."""
    data = {}
    weights = [48, 30, 42, 36, 36, 48, 42, 42, 36, 24, 30, 30, 42, 36, 36]
    values = [10, 30, 25, 50, 35, 30, 15, 40, 30, 35, 45, 10, 20, 30, 25]
    data['weights'] = weights
    data['values'] = values
    data['items'] = list(range(len(weights)))
    data['num_items'] = len(weights)
    num_bins = 5
    data['bins'] = list(range(num_bins))
    data['bin_capacities'] = [100, 100, 100, 100, 100]
    return data

# [END data_model]


def main():
    # [START data]
    data = create_data_model()
    # [END data]
    # [END program_part1]

    # [START solver]
    # Create the mip solver with the SCIP backend.
    solver = pywraplp.Solver.CreateSolver('SCIP')
    # [END solver]

    # [START program_part2]
    # [START variables]
    # Variables
    # x[i, j] = 1 if item i is packed in bin j.
    x = {}
    for i in data['items']:
        for j in data['bins']:
            x[(i, j)] = solver.IntVar(0, 1, 'x_%i_%i' % (i, j))
    # [END variables]

    # [START constraints]
    # Constraints
    # Each item can be in at most one bin.
    for i in data['items']:
        solver.Add(sum(x[i, j] for j in data['bins']) <= 1)
    # The amount packed in each bin cannot exceed its capacity.
    for j in data['bins']:
        solver.Add(
            sum(x[(i, j)] * data['weights'][i]
                for i in data['items']) <= data['bin_capacities'][j])
    # [END constraints]

    # [START objective]
    # Objective
    objective = solver.Objective()

    for i in data['items']:
        for j in data['bins']:
            objective.SetCoefficient(x[(i, j)], data['values'][i])
    objective.SetMaximization()
    # [END objective]

    # [START solve]
    status = solver.Solve()
    # [END solve]

    # [START print_solution]
    if status == pywraplp.Solver.OPTIMAL:
        print('Total packed value:', objective.Value())
        total_weight = 0
        for j in data['bins']:
            bin_weight = 0
            bin_value = 0
            print('Bin ', j, '\n')
            for i in data['items']:
                if x[i, j].solution_value() > 0:
                    print('Item', i, '- weight:', data['weights'][i], ' value:',
                          data['values'][i])
                    bin_weight += data['weights'][i]
                    bin_value += data['values'][i]
            print('Packed bin weight:', bin_weight)
            print('Packed bin value:', bin_value)
            print()
            total_weight += bin_weight
        print('Total packed weight:', total_weight)
    else:
        print('The problem does not have an optimal solution.')
    # [END print_solution]


if __name__ == '__main__':
    main()
# [END program_part2]
# [END program]
