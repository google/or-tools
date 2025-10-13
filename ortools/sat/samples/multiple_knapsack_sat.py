#!/usr/bin/env python3
# Copyright 2010-2025 Google LLC
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


def main() -> None:
    # [START data]
    data = {}
    data["weights"] = [48, 30, 42, 36, 36, 48, 42, 42, 36, 24, 30, 30, 42, 36, 36]
    data["values"] = [10, 30, 25, 50, 35, 30, 15, 40, 30, 35, 45, 10, 20, 30, 25]
    assert len(data["weights"]) == len(data["values"])
    num_items = len(data["weights"])
    all_items = range(num_items)

    data["bin_capacities"] = [100, 100, 100, 100, 100]
    num_bins = len(data["bin_capacities"])
    all_bins = range(num_bins)
    # [END data]

    # [START model]
    model = cp_model.CpModel()
    # [END model]

    # Variables.
    # [START variables]
    # x[i, b] = 1 if item i is packed in bin b.
    x = {}
    for i in all_items:
        for b in all_bins:
            x[i, b] = model.new_bool_var(f"x_{i}_{b}")
    # [END variables]

    # Constraints.
    # [START constraints]
    # Each item is assigned to at most one bin.
    for i in all_items:
        model.add_at_most_one(x[i, b] for b in all_bins)

    # The amount packed in each bin cannot exceed its capacity.
    for b in all_bins:
        model.add(
            sum(x[i, b] * data["weights"][i] for i in all_items)
            <= data["bin_capacities"][b]
        )
    # [END constraints]

    # Objective.
    # [START objective]
    # maximize total value of packed items.
    objective = []
    for i in all_items:
        for b in all_bins:
            objective.append(cp_model.LinearExpr.term(x[i, b], data["values"][i]))
    model.maximize(cp_model.LinearExpr.sum(objective))
    # [END objective]

    # [START solve]
    solver = cp_model.CpSolver()
    status = solver.solve(model)
    # [END solve]

    # [START print_solution]
    if status == cp_model.OPTIMAL:
        print(f"Total packed value: {solver.objective_value}")
        total_weight = 0
        for b in all_bins:
            print(f"Bin {b}")
            bin_weight = 0
            bin_value = 0
            for i in all_items:
                if solver.value(x[i, b]) > 0:
                    print(
                        f'Item:{i} weight:{data["weights"][i]} value:{data["values"][i]}'
                    )
                    bin_weight += data["weights"][i]
                    bin_value += data["values"][i]
            print(f"Packed bin weight: {bin_weight}")
            print(f"Packed bin value: {bin_value}\n")
            total_weight += bin_weight
        print(f"Total packed weight: {total_weight}")
    else:
        print("The problem does not have an optimal solution.")
    # [END print_solution]


if __name__ == "__main__":
    main()
# [END program]
