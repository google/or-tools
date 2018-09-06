# Copyright 2010-2017 Google
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
from __future__ import print_function
from ortools.constraint_solver import pywrapcp


def main():
  # Instantiate a cp solver.
  solver = pywrapcp.Solver('transportation_with_sizes')
  cost = [[90, 76, 75, 70, 50, 74], [35, 85, 55, 65, 48, 101],
          [125, 95, 90, 105, 59, 120], [45, 110, 95, 115, 104, 83],
          [60, 105, 80, 75, 59, 62], [45, 65, 110, 95, 47, 31],
          [38, 51, 107, 41, 69, 99], [47, 85, 57, 71, 92, 77],
          [39, 63, 97, 49, 118, 56], [47, 101, 71, 60, 88, 109],
          [17, 39, 103, 64, 61, 92], [101, 45, 83, 59, 92, 27]]

  group1 = [
      [0, 0, 1, 1],  # Workers 2, 3
      [0, 1, 0, 1],  # Workers 1, 3
      [0, 1, 1, 0],  # Workers 1, 2
      [1, 1, 0, 0],  # Workers 0, 1
      [1, 0, 1, 0]
  ]  # Workers 0, 2

  group2 = [
      [0, 0, 1, 1],  # Workers 6, 7
      [0, 1, 0, 1],  # Workers 5, 7
      [0, 1, 1, 0],  # Workers 5, 6
      [1, 1, 0, 0],  # Workers 4, 5
      [1, 0, 0, 1]
  ]  # Workers 4, 7

  group3 = [
      [0, 0, 1, 1],  # Workers 10, 11
      [0, 1, 0, 1],  # Workers 9, 11
      [0, 1, 1, 0],  # Workers 9, 10
      [1, 0, 1, 0],  # Workers 8, 10
      [1, 0, 0, 1]
  ]  # Workers 8, 11

  sizes = [10, 7, 3, 12, 15, 4, 11, 5]
  total_size_max = 15
  num_workers = len(cost)
  num_tasks = len(cost[1])
  # Variables
  total_cost = solver.IntVar(0, 1000, 'total_cost')
  x = []
  works = []
  for i in range(num_workers):
    work = solver.BoolVar('work[%i]' % i)
    works.append(work)
    t = []
    for j in range(num_tasks):
      t.append(solver.IntVar(0, 1, 'x[%i,%i]' % (i, j)))
    x.append(t)
    solver.Add(solver.MaxEquality(x[i], work))
  x_array = [x[i][j] for i in range(num_workers) for j in range(num_tasks)]

  # Constraints

  # Each task is assigned to at least one worker.
  [
      solver.Add(solver.Sum(x[i][j]
                            for i in range(num_workers)) >= 1)
      for j in range(num_tasks)
  ]

  # Total task size for each worker is at most total_size_max

  [
      solver.Add(
          solver.Sum(sizes[j] * x[i][j]
                     for j in range(num_tasks)) <= total_size_max)
      for i in range(num_workers)
  ]

  # Workers forms valid groups.
  solver.Add(
      solver.AllowedAssignments([works[0], works[1], works[2], works[3]],
                                group1))
  solver.Add(
      solver.AllowedAssignments([works[4], works[5], works[6], works[7]],
                                group2))
  solver.Add(
      solver.AllowedAssignments([works[8], works[9], works[10], works[11]],
                                group3))

  # Total cost
  solver.Add(total_cost == solver.Sum(
      [solver.ScalProd(x_row, cost_row) for (x_row, cost_row) in zip(x, cost)]))
  objective = solver.Minimize(total_cost, 1)
  db = solver.Phase(x_array, solver.CHOOSE_FIRST_UNBOUND,
                    solver.ASSIGN_MIN_VALUE)

  # Create a solution collector.
  collector = solver.LastSolutionCollector()
  # Add decision variables
  #collector.Add(x_array)

  for i in range(num_workers):
    collector.Add(x[i])

  # Add objective
  collector.AddObjective(total_cost)
  solver.Solve(db, [objective, collector])

  if collector.SolutionCount() > 0:
    best_solution = collector.SolutionCount() - 1
    print('Total cost = ', collector.ObjectiveValue(best_solution))
    print()

    for i in range(num_workers):

      for j in range(num_tasks):

        if collector.Value(best_solution, x[i][j]) == 1:
          print('Worker ', i, ' assigned to task ', j, '  Cost = ', cost[i][j])

    print()
    print('Time = ', solver.WallTime(), 'milliseconds')


if __name__ == '__main__':
  main()
