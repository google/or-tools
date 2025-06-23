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
"""Solve assignment problem for given group of workers."""
# [START import]
from ortools.linear_solver import pywraplp

# [END import]


def main():
  # Data
  # [START data]
  costs = [
      [90, 76, 75, 70, 50, 74],
      [35, 85, 55, 65, 48, 101],
      [125, 95, 90, 105, 59, 120],
      [45, 110, 95, 115, 104, 83],
      [60, 105, 80, 75, 59, 62],
      [45, 65, 110, 95, 47, 31],
      [38, 51, 107, 41, 69, 99],
      [47, 85, 57, 71, 92, 77],
      [39, 63, 97, 49, 118, 56],
      [47, 101, 71, 60, 88, 109],
      [17, 39, 103, 64, 61, 92],
      [101, 45, 83, 59, 92, 27],
  ]
  num_workers = len(costs)
  num_tasks = len(costs[0])
  # [END data]

  # Allowed groups of workers:
  # [START allowed_groups]
  group1 = [  # Subgroups of workers 0 - 3
      [2, 3],
      [1, 3],
      [1, 2],
      [0, 1],
      [0, 2],
  ]

  group2 = [  # Subgroups of workers 4 - 7
      [6, 7],
      [5, 7],
      [5, 6],
      [4, 5],
      [4, 7],
  ]

  group3 = [  # Subgroups of workers 8 - 11
      [10, 11],
      [9, 11],
      [9, 10],
      [8, 10],
      [8, 11],
  ]
  # [END allowed_groups]

  # Solver.
  # [START solver]
  # Create the mip solver with the SCIP backend.
  solver = pywraplp.Solver.CreateSolver("SCIP")
  if not solver:
    return
  # [END solver]

  # Variables
  # [START variables]
  # x[worker, task] is an array of 0-1 variables, which will be 1
  # if the worker is assigned to the task.
  x = {}
  for worker in range(num_workers):
    for task in range(num_tasks):
      x[worker, task] = solver.BoolVar(f"x[{worker},{task}]")
  # [END variables]

  # Constraints
  # [START constraints]
  # The total size of the tasks each worker takes on is at most total_size_max.
  for worker in range(num_workers):
    solver.Add(solver.Sum([x[worker, task] for task in range(num_tasks)]) <= 1)

  # Each task is assigned to exactly one worker.
  for task in range(num_tasks):
    solver.Add(
        solver.Sum([x[worker, task] for worker in range(num_workers)]) == 1
    )
  # [END constraints]

  # [START assignments]
  # Create variables for each worker, indicating whether they work on some task.
  work = {}
  for worker in range(num_workers):
    work[worker] = solver.BoolVar(f"work[{worker}]")

  for worker in range(num_workers):
    solver.Add(
        work[worker]
        == solver.Sum([x[worker, task] for task in range(num_tasks)])
    )

  # Group1
  constraint_g1 = solver.Constraint(1, 1)
  for index, _ in enumerate(group1):
    # a*b can be transformed into 0 <= a + b - 2*p <= 1 with p in [0,1]
    # p is True if a AND b, False otherwise
    constraint = solver.Constraint(0, 1)
    constraint.SetCoefficient(work[group1[index][0]], 1)
    constraint.SetCoefficient(work[group1[index][1]], 1)
    p = solver.BoolVar(f"g1_p{index}")
    constraint.SetCoefficient(p, -2)

    constraint_g1.SetCoefficient(p, 1)

  # Group2
  constraint_g2 = solver.Constraint(1, 1)
  for index, _ in enumerate(group2):
    # a*b can be transformed into 0 <= a + b - 2*p <= 1 with p in [0,1]
    # p is True if a AND b, False otherwise
    constraint = solver.Constraint(0, 1)
    constraint.SetCoefficient(work[group2[index][0]], 1)
    constraint.SetCoefficient(work[group2[index][1]], 1)
    p = solver.BoolVar(f"g2_p{index}")
    constraint.SetCoefficient(p, -2)

    constraint_g2.SetCoefficient(p, 1)

  # Group3
  constraint_g3 = solver.Constraint(1, 1)
  for index, _ in enumerate(group3):
    # a*b can be transformed into 0 <= a + b - 2*p <= 1 with p in [0,1]
    # p is True if a AND b, False otherwise
    constraint = solver.Constraint(0, 1)
    constraint.SetCoefficient(work[group3[index][0]], 1)
    constraint.SetCoefficient(work[group3[index][1]], 1)
    p = solver.BoolVar(f"g3_p{index}")
    constraint.SetCoefficient(p, -2)

    constraint_g3.SetCoefficient(p, 1)
  # [END assignments]

  # Objective
  # [START objective]
  objective_terms = []
  for worker in range(num_workers):
    for task in range(num_tasks):
      objective_terms.append(costs[worker][task] * x[worker, task])
  solver.Minimize(solver.Sum(objective_terms))
  # [END objective]

  # Solve
  # [START solve]
  print(f"Solving with {solver.SolverVersion()}")
  status = solver.Solve()
  # [END solve]

  # Print solution.
  # [START print_solution]
  if status == pywraplp.Solver.OPTIMAL or status == pywraplp.Solver.FEASIBLE:
    print(f"Total cost = {solver.Objective().Value()}\n")
    for worker in range(num_workers):
      for task in range(num_tasks):
        if x[worker, task].solution_value() > 0.5:
          print(
              f"Worker {worker} assigned to task {task}."
              + f" Cost: {costs[worker][task]}"
          )
  else:
    print("No solution found.")
  # [END print_solution]


if __name__ == "__main__":
  main()
# [END program]
