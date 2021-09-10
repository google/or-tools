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
"""Solve a simple bin packing problem using a MIP solver."""
# [START program]
# [START import]
from ortools.linear_solver import pywraplp
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

    # y[j] = 1 if bin j is used.
    y = {}
    for j in data['bins']:
        y[j] = solver.IntVar(0, 1, 'y[%i]' % j)
    # [END variables]

    # [START constraints]
    # Constraints
    # Each item must be in exactly one bin.
    for i in data['items']:
        solver.Add(sum(x[i, j] for j in data['bins']) == 1)

    # The amount packed in each bin cannot exceed its capacity.
    for j in data['bins']:
        solver.Add(
            sum(x[(i, j)] * data['weights'][i] for i in data['items']) <= y[j] *
            data['bin_capacity'])
    # [END constraints]

    # [START objective]
    # Objective: minimize the number of bins used.
    solver.Minimize(solver.Sum([y[j] for j in data['bins']]))
    # [END objective]

    # [START solve]
    status = solver.Solve()
    # [END solve]

    # [START print_solution]
    if status == pywraplp.Solver.OPTIMAL:
        num_bins = 0.
        for j in data['bins']:
            if y[j].solution_value() == 1:
                bin_items = []
                bin_weight = 0
                for i in data['items']:
                    if x[i, j].solution_value() > 0:
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
        print('Time = ', solver.WallTime(), ' milliseconds')
    else:
        print('The problem does not have an optimal solution.')
    # [END print_solution]


if __name__ == '__main__':
    main()
# [END program_part2]
# [END program]
