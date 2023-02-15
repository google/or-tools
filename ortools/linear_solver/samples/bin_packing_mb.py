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
"""Solve a simple bin packing problem using a MIP solver."""
# [START program]
# [START import]
from ortools.linear_solver.python import model_builder
# [END import]


# [START program_part1]
# [START data_model]
def create_data_model():
    """Create the data for the example."""
    data = {}
    weights = [48, 30, 19, 36, 36, 27, 42, 42, 36, 24, 30]
    data['weights'] = weights
    data['items'] = list(range(len(weights)))
    data['bins'] = data['items']
    data['bin_capacity'] = 100
    return data

# [END data_model]


def main():
    # [START data]
    data = create_data_model()
    # [END data]
    # [END program_part1]

    # [START solver]
    # Create the model.
    model = model_builder.ModelBuilder()
    # [END solver]

    # [START program_part2]
    # [START variables]
    # Variables
    # x[i, j] = 1 if item i is packed in bin j.
    x = {}
    for i in data['items']:
        for j in data['bins']:
            x[(i, j)] = model.new_bool_var(f'x_{i}_{j}')

    # y[j] = 1 if bin j is used.
    y = {}
    for j in data['bins']:
        y[j] = model.new_bool_var(f'y_{j}')
    # [END variables]

    # [START constraints]
    # Constraints
    # Each item must be in exactly one bin.
    for i in data['items']:
        model.add(sum(x[i, j] for j in data['bins']) == 1)

    # The amount packed in each bin cannot exceed its capacity.
    for j in data['bins']:
        model.add(
            sum(x[(i, j)] * data['weights'][i] for i in data['items']) <= y[j] *
            data['bin_capacity'])
    # [END constraints]

    # [START objective]
    # Objective: minimize the number of bins used.
    model.minimize(model_builder.LinearExpr.sum([y[j] for j in data['bins']]))
    # [END objective]

    # [START solve]
    # Create the solver with the CP-SAT backend, and solve the model.
    solver = model_builder.ModelSolver('sat')
    status = solver.solve(model)
    # [END solve]

    # [START print_solution]
    if status == model_builder.SolveStatus.OPTIMAL:
        num_bins = 0.
        for j in data['bins']:
            if solver.value(y[j]) == 1:
                bin_items = []
                bin_weight = 0
                for i in data['items']:
                    if solver.value(x[i, j]) > 0:
                        bin_items.append(i)
                        bin_weight += data['weights'][i]
                if bin_weight > 0:
                    num_bins += 1
                    print('Bin number', j)
                    print('  Items packed:', bin_items)
                    print('  Total weight:', bin_weight)
                    print()
        print()
        print('Number of bins used:', num_bins)
        print('Time = ', solver.wall_time, ' seconds')
    else:
        print('The problem does not have an optimal solution.')
    # [END print_solution]


if __name__ == '__main__':
    main()
# [END program_part2]
# [END program]
