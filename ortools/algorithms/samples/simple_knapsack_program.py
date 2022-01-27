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
"""A simple knapsack problem."""
# [START import]
from ortools.algorithms import pywrapknapsack_solver
# [END import]


def main():
    # Create the solver.
    # [START solver]
    solver = pywrapknapsack_solver.KnapsackSolver(
        pywrapknapsack_solver.KnapsackSolver.
        KNAPSACK_DYNAMIC_PROGRAMMING_SOLVER, "test")
    # [END solver]

    # [START data]
    weights = [[
        565, 406, 194, 130, 435, 367, 230, 315, 393, 125, 670, 892, 600, 293,
        712, 147, 421, 255
    ]]
    capacities = [850]
    values = weights[0]
    # [END data]

    # [START solve]
    solver.Init(values, weights, capacities)
    computed_value = solver.Solve()
    # [END solve]

    # [START print_solution]
    packed_items = [
        x for x in range(0, len(weights[0])) if solver.BestSolutionContains(x)
    ]
    packed_weights = [weights[0][i] for i in packed_items]

    print("Packed items: ", packed_items)
    print("Packed weights: ", packed_weights)
    print("Total weight (same as total value): ", computed_value)
    # [END print_solution]


if __name__ == "__main__":
    main()
# [END program]
