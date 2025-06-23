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
"""Solve assignment problem using linear assignment solver."""
# [START import]
import numpy as np

from ortools.graph.python import linear_sum_assignment

# [END import]


def main():
  """Linear Sum Assignment example."""
  # [START solver]
  assignment = linear_sum_assignment.SimpleLinearSumAssignment()
  # [END solver]

  # [START data]
  costs = np.array([
      [90, 76, 75, 70],
      [35, 85, 55, 65],
      [125, 95, 90, 105],
      [45, 110, 95, 115],
  ])

  # Let's transform this into 3 parallel vectors (start_nodes, end_nodes,
  # arc_costs)
  end_nodes_unraveled, start_nodes_unraveled = np.meshgrid(
      np.arange(costs.shape[1]), np.arange(costs.shape[0])
  )
  start_nodes = start_nodes_unraveled.ravel()
  end_nodes = end_nodes_unraveled.ravel()
  arc_costs = costs.ravel()
  # [END data]

  # [START constraints]
  assignment.add_arcs_with_cost(start_nodes, end_nodes, arc_costs)
  # [END constraints]

  # [START solve]
  status = assignment.solve()
  # [END solve]

  # [START print_solution]
  if status == assignment.OPTIMAL:
    print(f"Total cost = {assignment.optimal_cost()}\n")
    for i in range(0, assignment.num_nodes()):
      print(
          f"Worker {i} assigned to task {assignment.right_mate(i)}."
          + f"  Cost = {assignment.assignment_cost(i)}"
      )
  elif status == assignment.INFEASIBLE:
    print("No assignment is possible.")
  elif status == assignment.POSSIBLE_OVERFLOW:
    print("Some input costs are too large and may cause an integer overflow.")
  # [END print_solution]


if __name__ == "__main__":
  main()
# [END Program]
