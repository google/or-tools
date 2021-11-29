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
"""This model implements a sudoku solver."""

from ortools.sat.python import cp_model


def solve_sudoku():
    """Solves the sudoku problem with the CP-SAT solver."""
    # Create the model.
    model = cp_model.CpModel()

    cell_size = 3
    line_size = cell_size**2
    line = list(range(0, line_size))
    cell = list(range(0, cell_size))

    initial_grid = [[0, 6, 0, 0, 5, 0, 0, 2, 0], [0, 0, 0, 3, 0, 0, 0, 9, 0],
                    [7, 0, 0, 6, 0, 0, 0, 1, 0], [0, 0, 6, 0, 3, 0, 4, 0, 0],
                    [0, 0, 4, 0, 7, 0, 1, 0, 0], [0, 0, 5, 0, 9, 0, 8, 0, 0],
                    [0, 4, 0, 0, 0, 1, 0, 0, 6], [0, 3, 0, 0, 0, 8, 0, 0, 0],
                    [0, 2, 0, 0, 4, 0, 0, 5, 0]]

    grid = {}
    for i in line:
        for j in line:
            grid[(i, j)] = model.NewIntVar(1, line_size, 'grid %i %i' % (i, j))

    # AllDifferent on rows.
    for i in line:
        model.AddAllDifferent([grid[(i, j)] for j in line])

    # AllDifferent on columns.
    for j in line:
        model.AddAllDifferent([grid[(i, j)] for i in line])

    # AllDifferent on cells.
    for i in cell:
        for j in cell:
            one_cell = []
            for di in cell:
                for dj in cell:
                    one_cell.append(grid[(i * cell_size + di,
                                          j * cell_size + dj)])

            model.AddAllDifferent(one_cell)

    # Initial values.
    for i in line:
        for j in line:
            if initial_grid[i][j]:
                model.Add(grid[(i, j)] == initial_grid[i][j])

    # Solve and print out the solution.
    solver = cp_model.CpSolver()
    status = solver.Solve(model)
    if status == cp_model.OPTIMAL:
        for i in line:
            print([int(solver.Value(grid[(i, j)])) for j in line])


solve_sudoku()
