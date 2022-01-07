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
"""Solves a multiple knapsack problem using the CP-SAT solver."""
# [START import]
from ortools.sat.python import cp_model
# [END import]


def main():
    # [START data]
    data = {}
    data['weights'] = [
        48, 30, 42, 36, 36, 48, 42, 42, 36, 24, 30, 30, 42, 36, 36
    ]
    data['values'] = [
        10, 30, 25, 50, 35, 30, 15, 40, 30, 35, 45, 10, 20, 30, 25
    ]
    assert len(data['weights']) == len(data['values'])
    data['num_items'] = len(data['weights'])
    data['all_items'] = range(data['num_items'])

    data['bin_capacities'] = [100, 100, 100, 100, 100]
    data['num_bins'] = len(data['bin_capacities'])
    data['all_bins'] = range(data['num_bins'])
    # [END data]

    # [START model]
    model = cp_model.CpModel()
    # [END model]

    # Variables.
    # [START variables]
    # x[i, b] = 1 if item i is packed in bin b.
    x = {}
    for i in data['all_items']:
        for b in data['all_bins']:
            x[i, b] = model.NewBoolVar(f'x_{i}_{b}')
    # [END variables]

    # Constraints.
    # [START constraints]
    # Each item is assigned to at most one bin.
    for i in data['all_items']:
        model.Add(sum(x[i, b] for b in data['all_bins']) <= 1)

    # The amount packed in each bin cannot exceed its capacity.
    for b in data['all_bins']:
        model.Add(
            sum(x[i, b] * data['weights'][i]
                for i in data['all_items']) <= data['bin_capacities'][b])
    # [END constraints]

    # Objective.
    # [START objective]
    # Maximize total value of packed items.
    objective = []
    for i in data['all_items']:
        for b in data['all_bins']:
            objective.append(
                cp_model.LinearExpr.Term(x[i, b], data['values'][i]))
    model.Maximize(cp_model.LinearExpr.Sum(objective))
    # [END objective]

    # [START solve]
    solver = cp_model.CpSolver()
    status = solver.Solve(model)
    # [END solve]

    # [START print_solution]
    if status == cp_model.OPTIMAL:
        print(f'Total packed value: {solver.ObjectiveValue()}')
        total_weight = 0
        for b in data['all_bins']:
            print(f'Bin {b}')
            bin_weight = 0
            bin_value = 0
            for i in data['all_items']:
                if solver.Value(x[i, b]) > 0:
                    print(
                        f"Item {i} weight: {data['weights'][i]} value: {data['values'][i]}"
                    )
                    bin_weight += data['weights'][i]
                    bin_value += data['values'][i]
            print(f'Packed bin weight: {bin_weight}')
            print(f'Packed bin value: {bin_value}\n')
            total_weight += bin_weight
        print(f'Total packed weight: {total_weight}')
    else:
        print('The problem does not have an optimal solution.')
    # [END print_solution]


if __name__ == '__main__':
    main()
# [END program]
