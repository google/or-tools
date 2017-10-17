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


"""Hidato puzzle in Google CP Solver.

  http://www.hidato.com/
  '''
  Puzzles start semi-filled with numbered tiles.
  The first and last numbers are circled.
  Connect the numbers together to win. Consecutive
  number must touch horizontally, vertically, or
  diagonally.
  '''
"""
from __future__ import print_function
from ortools.constraint_solver import pywrapcp


def BuildPairs(rows, cols):
  """Build closeness pairs for consecutive numbers.

  Build set of allowed pairs such that two consecutive numbers touch
  each other in the grid.

  Returns:
    A list of pairs for allowed consecutive position of numbers.

  Args:
    rows: the number of rows in the grid
    cols: the number of columns in the grid
  """
  return [(x * cols + y, (x + dx) * cols + (y + dy))
          for x in range(rows) for y in range(cols)
          for dx in (-1, 0, 1) for dy in (-1, 0, 1)
          if (x + dx >= 0 and x + dx < rows and
              y + dy >= 0 and y + dy < cols and (dx != 0 or dy != 0))]


def main():
  for model in range(1, 7):
    print()
    print(('----- Solving problem %i -----' % model))
    print()
    Solve(model)


def Solve(model):
  """Solve the given model."""
  # Create the solver.
  solver = pywrapcp.Solver('hidato-table')

  #
  # models, a 0 indicates an open cell which number is not yet known.
  #
  #
  puzzle = None
  if model == 1:
    # Simple problem
    puzzle = [[6, 0, 9],
              [0, 2, 8],
              [1, 0, 0]]

  elif model == 2:
    puzzle = [[0, 44, 41, 0, 0, 0, 0],
              [0, 43, 0, 28, 29, 0, 0],
              [0, 1, 0, 0, 0, 33, 0],
              [0, 2, 25, 4, 34, 0, 36],
              [49, 16, 0, 23, 0, 0, 0],
              [0, 19, 0, 0, 12, 7, 0],
              [0, 0, 0, 14, 0, 0, 0]]

  elif model == 3:
    # Problems from the book:
    # Gyora Bededek: "Hidato: 2000 Pure Logic Puzzles"
    # Problem 1 (Practice)
    puzzle = [[0, 0, 20, 0, 0],
              [0, 0, 0, 16, 18],
              [22, 0, 15, 0, 0],
              [23, 0, 1, 14, 11],
              [0, 25, 0, 0, 12]]

  elif model == 4:
    # problem 2 (Practice)
    puzzle = [[0, 0, 0, 0, 14],
              [0, 18, 12, 0, 0],
              [0, 0, 17, 4, 5],
              [0, 0, 7, 0, 0],
              [9, 8, 25, 1, 0]]

  elif model == 5:
    # problem 3 (Beginner)
    puzzle = [[0, 26, 0, 0, 0, 18],
              [0, 0, 27, 0, 0, 19],
              [31, 23, 0, 0, 14, 0],
              [0, 33, 8, 0, 15, 1],
              [0, 0, 0, 5, 0, 0],
              [35, 36, 0, 10, 0, 0]]

  elif model == 6:
    # Problem 15 (Intermediate)
    puzzle = [[64, 0, 0, 0, 0, 0, 0, 0],
              [1, 63, 0, 59, 15, 57, 53, 0],
              [0, 4, 0, 14, 0, 0, 0, 0],
              [3, 0, 11, 0, 20, 19, 0, 50],
              [0, 0, 0, 0, 22, 0, 48, 40],
              [9, 0, 0, 32, 23, 0, 0, 41],
              [27, 0, 0, 0, 36, 0, 46, 0],
              [28, 30, 0, 35, 0, 0, 0, 0]]

  r = len(puzzle)
  c = len(puzzle[0])

  print(('Initial game (%i x %i)' % (r, c)))
  PrintMatrix(puzzle)

  #
  # declare variables
  #
  positions = [solver.IntVar(0, r * c - 1, 'p of %i' % i) for i in range(r * c)]

  #
  # constraints
  #
  solver.Add(solver.AllDifferent(positions))

  #
  # Fill in the clues
  #
  for i in range(r):
    for j in range(c):
      if puzzle[i][j] > 0:
        solver.Add(positions[puzzle[i][j] - 1] == i * c + j)

  # Consecutive numbers much touch each other in the grid.
  # We use an allowed assignment constraint to model it.
  close_tuples = BuildPairs(r, c)
  for k in range(1, r * c - 1):
    solver.Add(solver.AllowedAssignments((positions[k], positions[k + 1]),
                                         close_tuples))

  #
  # solution and search
  #

  # db: DecisionBuilder
  db = solver.Phase(positions,
                    solver.CHOOSE_MIN_SIZE_LOWEST_MIN,
                    solver.ASSIGN_MIN_VALUE)

  solver.NewSearch(db)
  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    PrintOneSolution(positions, r, c, num_solutions)

  solver.EndSearch()

  print(('solutions : %i' % num_solutions))
  print(('failures  : %i' % solver.Failures()))
  print(('branches  : %i' % solver.Branches()))
  print(('wall time : %i' % solver.WallTime()))


def PrintOneSolution(positions, rows, cols, num_solution):
  """Print a current solution."""
  print(('Solution %i:' % num_solution))
  # Create empty board.
  board = []
  for unused_i in range(rows):
    board.append([0] * cols)
  # Fill board with solution value.
  for k in range(rows * cols):
    position = positions[k].Value()
    board[position // cols][position % cols] = k + 1
  # Print the board.
  PrintMatrix(board)


def PrintMatrix(game):
  """Pretty print of a matrix."""
  rows = len(game)
  cols = len(game[0])
  for i in range(rows):
    line = ''
    for j in range(cols):
      if game[i][j] == 0:
        line += '  .'
      else:
        line += '% 3s' % game[i][j]
    print(line)
  print()


if __name__ == '__main__':
  main()
