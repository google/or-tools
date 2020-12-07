# Copyright 2010 Hakan Kjellerstrand hakank@gmail.com
#
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
"""
  Hidato puzzle in Google CP Solver.

  http://www.shockwave.com/gamelanding/hidato.jsp
  http://www.hidato.com/
  '''
  Puzzles start semi-filled with numbered tiles.
  The first and last numbers are circled.
  Connect the numbers together to win. Consecutive
  number must touch horizontally, vertically, or
  diagonally.
  '''

  Compare with the following models:
  * MiniZinc: http://www.hakank.org/minizinc/hidato.mzn
  * Gecode  : http://www.hakank.org/gecode/hidato.cpp
  * Comet   : http://www.hakank.org/comet/hidato.co
  * Tailopr/Essence': http://hakank.org/tailor/hidato.eprime
  * ECLiPSe: http://hakank.org/eclipse/hidato.ecl
  * SICStus: http://hakank.org/sicstus/hidato.pl

  Note: This model is very slow. Please see Laurent Perron's much faster
        (and more elegant) model: hidato_table.py .

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver import pywrapcp


def main(r, c):
  # Create the solver.
  solver = pywrapcp.Solver("hidato")

  # data
  # Simple problem
  if r == 3 and c == 3:
    puzzle = [[6, 0, 9], [0, 2, 8], [1, 0, 0]]

  if r == 7 and c == 7:
    puzzle = [[0, 44, 41, 0, 0, 0, 0], [0, 43, 0, 28, 29, 0, 0],
              [0, 1, 0, 0, 0, 33, 0], [0, 2, 25, 4, 34, 0, 36],
              [49, 16, 0, 23, 0, 0, 0], [0, 19, 0, 0, 12, 7, 0],
              [0, 0, 0, 14, 0, 0, 0]]

  # Problems from the book:
  # Gyora Bededek: "Hidato: 2000 Pure Logic Puzzles"

  # Problem 1 (Practice)
  # r = 5
  # c = r
  # puzzle = [
  #    [ 0, 0,20, 0, 0],
  #    [ 0, 0, 0,16,18],
  #    [22, 0,15, 0, 0],
  #    [23, 0, 1,14,11],
  #    [ 0,25, 0, 0,12],
  #    ]

  # Problem 2 (Practice)
  if r == 5 and c == 5:
    puzzle = [
        [0, 0, 0, 0, 14],
        [0, 18, 12, 0, 0],
        [0, 0, 17, 4, 5],
        [0, 0, 7, 0, 0],
        [9, 8, 25, 1, 0],
    ]

  # Problem 3 (Beginner)
  if r == 6 and c == 6:
    puzzle = [[0, 26, 0, 0, 0, 18], [0, 0, 27, 0, 0, 19], [31, 23, 0, 0, 14, 0],
              [0, 33, 8, 0, 15, 1], [0, 0, 0, 5, 0, 0], [35, 36, 0, 10, 0, 0]]

  # Problem 15 (Intermediate)
  # Note: This takes very long time to solve...
  if r == 8 and c == 8:
    puzzle = [[64, 0, 0, 0, 0, 0, 0, 0], [1, 63, 0, 59, 15, 57, 53, 0],
              [0, 4, 0, 14, 0, 0, 0, 0], [3, 0, 11, 0, 20, 19, 0, 50],
              [0, 0, 0, 0, 22, 0, 48, 40], [9, 0, 0, 32, 23, 0, 0, 41],
              [27, 0, 0, 0, 36, 0, 46, 0], [28, 30, 0, 35, 0, 0, 0, 0]]

  print_game(puzzle, r, c)

  #
  # declare variables
  #
  x = {}
  for i in range(r):
    for j in range(c):
      x[(i, j)] = solver.IntVar(1, r * c, "dice(%i,%i)" % (i, j))
  x_flat = [x[(i, j)] for i in range(r) for j in range(c)]

  #
  # constraints
  #
  solver.Add(solver.AllDifferent(x_flat))

  #
  # Fill in the clues
  #
  for i in range(r):
    for j in range(c):
      if puzzle[i][j] > 0:
        solver.Add(x[(i, j)] == puzzle[i][j])

  # From the numbers k = 1 to r*c-1, find this position,
  # and then the position of k+1
  for k in range(1, r * c):
    i = solver.IntVar(0, r)
    j = solver.IntVar(0, c)
    a = solver.IntVar(-1, 1)
    b = solver.IntVar(-1, 1)

    # 1) First: fix "this" k
    # 2) and then find the position of the next value (k+1)
    # solver.Add(k == x[(i,j)])
    solver.Add(k == solver.Element(x_flat, i * c + j))
    # solver.Add(k + 1 == x[(i+a,j+b)])
    solver.Add(k + 1 == solver.Element(x_flat, (i + a) * c + (j + b)))

    solver.Add(i + a >= 0)
    solver.Add(j + b >= 0)
    solver.Add(i + a < r)
    solver.Add(j + b < c)

    # solver.Add(((a != 0) | (b != 0)))
    a_nz = solver.BoolVar()
    b_nz = solver.BoolVar()
    solver.Add(a_nz == solver.IsDifferentCstVar(a, 0))
    solver.Add(b_nz == solver.IsDifferentCstVar(b, 0))
    solver.Add(a_nz + b_nz >= 1)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(x_flat)

  # db: DecisionBuilder
  db = solver.Phase(
      x_flat,
      # solver.INT_VAR_DEFAULT
      # solver.INT_VAR_SIMPLE
      # solver.CHOOSE_RANDOM
      # solver.CHOOSE_MIN_SIZE_LOWEST_MIN
      # solver.CHOOSE_MIN_SIZE_HIGHEST_MIN
      # solver.CHOOSE_MIN_SIZE_LOWEST_MAX
      # solver.CHOOSE_MIN_SIZE_HIGHEST_MAX
      # solver.CHOOSE_PATH
      solver.CHOOSE_FIRST_UNBOUND,
      # solver.INT_VALUE_DEFAULT
      # solver.INT_VALUE_SIMPLE
      # solver.ASSIGN_MAX_VALUE
      # solver.ASSIGN_RANDOM_VALUE
      # solver.ASSIGN_CENTER_VALUE
      solver.ASSIGN_MIN_VALUE)

  solver.NewSearch(db)
  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    print("\nSolution:", num_solutions)
    print_board(x, r, c)
    print()

  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


def print_board(x, rows, cols):
  for i in range(rows):
    for j in range(cols):
      print("% 2s" % x[i, j].Value(), end=" ")
    print("")


def print_game(game, rows, cols):
  for i in range(rows):
    for j in range(cols):
      print("% 2s" % game[i][j], end=" ")
    print("")


if __name__ == "__main__":
  # data
  r = 3
  c = r
  if len(sys.argv) > 1:
    r = int(sys.argv[1])
    c = r
  if len(sys.argv) > 2:
    c = int(sys.argv[2])
  main(r, c)
