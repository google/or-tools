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

  Survo puzzle Google CP Solver.

  http://en.wikipedia.org/wiki/Survo_Puzzle
  '''
  Survo puzzle is a kind of logic puzzle presented (in April 2006) and studied
  by Seppo Mustonen. The name of the puzzle is associated to Mustonen's
  Survo system which is a general environment for statistical computing and
  related areas.

  In a Survo puzzle the task is to fill an m * n table by integers 1,2,...,m*n
  so
  that each of these numbers appears only once and their row and column sums are
  equal to integers given on the bottom and the right side of the table.
  Often some of the integers are given readily in the table in order to
  guarantee uniqueness of the solution and/or for making the task easier.
  '''

  See also
  http://www.survo.fi/english/index.html
  http://www.survo.fi/puzzles/index.html

  References:
  Mustonen, S. (2006b). "On certain cross sum puzzles"
  http://www.survo.fi/papers/puzzles.pdf
  Mustonen, S. (2007b). "Enumeration of uniquely solvable open Survo puzzles."
  http://www.survo.fi/papers/enum_survo_puzzles.pdf
  Kimmo Vehkalahti: "Some comments on magic squares and Survo puzzles"
  http://www.helsinki.fi/~kvehkala/Kimmo_Vehkalahti_Windsor.pdf
  R code: http://koti.mbnet.fi/tuimala/tiedostot/survo.R

  Compare with the following models:
  * Choco   : http://www.hakank.org/choco/SurvoPuzzle.java
  * Comet   : http://www.hakank.org/comet/survo_puzzle.co
  * ECLiPSE : http://www.hakank.org/eclipse/survo_puzzle.ecl
  * Gecode  : http://www.hakank.org/gecode/survo_puzzle.cpp
  * Gecode/R: http://www.hakank.org/gecode_r/survo_puzzle.rb
  * JaCoP   : http://www.hakank.org/JaCoP/SurvoPuzzle.java
  * MiniZinc: http://www.hakank.org/minizinc/survo_puzzle.mzn
  * Tailor/Essence': http://www.hakank.org/tailor/survo_puzzle.eprime
  * Zinc: http://www.hakank.org/minizinc/survo_puzzle.zinc


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver import pywrapcp


def main(r=0, c=0, rowsums=[], colsums=[], game=[]):

  # Create the solver.
  solver = pywrapcp.Solver("Survo puzzle")

  #
  # data
  #
  if r == 0:
    r = 3
    c = 4
    rowsums = [30, 18, 30]
    colsums = [27, 16, 10, 25]
    game = [[0, 6, 0, 0], [8, 0, 0, 0], [0, 0, 3, 0]]

  print("r:", r, "c:", c)

  # declare variables
  x = {}
  for i in range(r):
    for j in range(c):
      x[(i, j)] = solver.IntVar(1, r * c, "x %i %i" % (i, j))

  #
  # constraints
  #

  #
  # set the clues
  #
  for i in range(r):
    for j in range(c):
      if game[i][j] > 0:
        solver.Add(x[i, j] == game[i][j])

  xflat = [x[(i, j)] for i in range(r) for j in range(c)]
  solver.Add(solver.AllDifferent(xflat))
  #
  # calculate rowsums and colsums
  #
  for i in range(r):
    solver.Add(rowsums[i] == solver.Sum([x[i, j] for j in range(c)]))

  for j in range(c):
    solver.Add(colsums[j] == solver.Sum([x[i, j] for i in range(r)]))

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add([x[(i, j)] for i in range(r) for j in range(c)])

  collector = solver.AllSolutionCollector(solution)
  solver.Solve(
      solver.Phase(xflat, solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE),
      [collector])

  num_solutions = collector.SolutionCount()
  print("\nnum_solutions: ", num_solutions)
  if num_solutions > 0:
    for s in range(num_solutions):
      xval = [collector.Value(s, x[(i, j)]) for i in range(r) for j in range(c)]

      for i in range(r):
        for j in range(c):
          print("%2i" % (xval[i * c + j]), end=" ")
        print()
      print()

    print()
    print("num_solutions:", num_solutions)
    print("failures:", solver.Failures())
    print("branches:", solver.Branches())
    print("WallTime:", solver.WallTime())

  else:
    print("No solutions found")


#
# Read a problem instance from a file
#
def read_problem(file):
  f = open(file, "r")
  r = int(f.readline())
  c = int(f.readline())
  rowsums = f.readline()
  colsums = f.readline()
  rowsums = [int(t) for t in (rowsums.rstrip()).split(",")]
  colsums = [int(t) for t in (colsums.rstrip()).split(",")]
  game = []
  for i in range(r):
    x = f.readline()
    x = [int(t) for t in (x.rstrip()).split(",")]
    row = [0] * c
    for j in range(c):
      row[j] = int(x[j])
    game.append(row)
  return [r, c, rowsums, colsums, game]


if __name__ == "__main__":
  if len(sys.argv) > 1:
    file = sys.argv[1]
    [r, c, rowsums, colsums, game] = read_problem(file)
    main(r, c, rowsums, colsums, game)
  else:
    main()
