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

"""This model implements a sudoku solver."""

from __future__ import print_function
from ortools.constraint_solver import pywrapcp

def main():
  # Create the solver.
  solver = pywrapcp.Solver('sudoku')

  cell_size = 3
  line_size = cell_size ** 2
  line = list(range(0, line_size))
  cell = list(range(0, cell_size))

  initial_grid = [[0, 6, 0, 0, 5, 0, 0, 2, 0],
                  [0, 0, 0, 3, 0, 0, 0, 9, 0],
                  [7, 0, 0, 6, 0, 0, 0, 1, 0],
                  [0, 0, 6, 0, 3, 0, 4, 0, 0],
                  [0, 0, 4, 0, 7, 0, 1, 0, 0],
                  [0, 0, 5, 0, 9, 0, 8, 0, 0],
                  [0, 4, 0, 0, 0, 1, 0, 0, 6],
                  [0, 3, 0, 0, 0, 8, 0, 0, 0],
                  [0, 2, 0, 0, 4, 0, 0, 5, 0]]

  grid = {}
  for i in line:
    for j in line:
      grid[(i, j)] = solver.IntVar(1, line_size, 'grid %i %i' % (i, j))

  # AllDifferent on rows.
  for i in line:
    solver.Add(solver.AllDifferent([grid[(i, j)] for j in line]))

  # AllDifferent on columns.
  for j in line:
    solver.Add(solver.AllDifferent([grid[(i, j)] for i in line]))

  # AllDifferent on cells.
  for i in cell:
    for j in cell:
      one_cell = []
      for di in cell:
        for dj in cell:
          one_cell.append(grid[(i * cell_size + di, j * cell_size + dj)])

      solver.Add(solver.AllDifferent(one_cell))

  # Initial values.
  for i in line:
    for j in line:
      if initial_grid[i][j]:
        solver.Add(grid[(i, j)] == initial_grid[i][j])

  # Regroup all variables into a list.
  all_vars = [grid[(i, j)] for i in line for j in line]

  # Create search phases.
  vars_phase = solver.Phase(all_vars,
                            solver.INT_VAR_SIMPLE,
                            solver.INT_VALUE_SIMPLE)

  solution = solver.Assignment()
  solution.Add(all_vars)
  collector = solver.FirstSolutionCollector(solution)

  # And solve.
  solver.Solve(vars_phase, [collector])

  if collector.SolutionCount() == 1:
    for i in line:
      print([int(collector.Value(0, grid[(i, j)])) for j in line])


if __name__ == '__main__':
  main()
