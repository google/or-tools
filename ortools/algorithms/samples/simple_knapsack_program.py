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
"""A simple knapsack problem."""
# [START import]
from ortools.algorithms.python import knapsack_solver

# [END import]


def main():
  # Create the solver.
  # [START solver]
  solver = knapsack_solver.KnapsackSolver(
      knapsack_solver.SolverType.KNAPSACK_DYNAMIC_PROGRAMMING_SOLVER,
      "test",
  )
  # [END solver]

  # [START data]
  weights = [
      # fmt:off
      [565, 406, 194, 130, 435, 367, 230, 315, 393, 125, 670, 892, 600, 293, 712, 147, 421, 255],
      # fmt:on
  ]
  capacities = [850]
  values = weights[0]
  # [END data]

  # [START solve]
  solver.init(values, weights, capacities)
  computed_value = solver.solve()
  # [END solve]

  # [START print_solution]
  packed_items = [
      x for x in range(0, len(weights[0])) if solver.best_solution_contains(x)
  ]
  packed_weights = [weights[0][i] for i in packed_items]

  print("Packed items: ", packed_items)
  print("Packed weights: ", packed_weights)
  print("Total weight (same as total value): ", computed_value)
  # [END print_solution]


if __name__ == "__main__":
  main()
# [END program]
