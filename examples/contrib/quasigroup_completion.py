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

  Quasigroup completion Google CP Solver.


  See Carla P. Gomes and David Shmoys:
  "Completing Quasigroups or Latin Squares: Structured Graph Coloring Problem"

  See also
  Ivars Peterson "Completing Latin Squares"
  http://www.maa.org/mathland/mathtrek_5_8_00.html
  '''
    Using only the numbers 1, 2, 3, and 4, arrange four sets of these numbers
    into
    a four-by-four array so that no column or row contains the same two numbers.
    The result is known as a Latin square.
    ...
    The so-called quasigroup completion problem concerns a table that is
    correctly
    but only partially filled in. The question is whether the remaining blanks
    in
    the table can be filled in to obtain a complete Latin square (or a proper
    quasigroup multiplication table).
    '''

    Compare with the following models:
    * Choco: http://www.hakank.org/choco/QuasigroupCompletion.java
    * Comet: http://www.hakank.org/comet/quasigroup_completion.co
    * ECLiPSE: http://www.hakank.org/eclipse/quasigroup_completion.ecl
    * Gecode: http://www.hakank.org/gecode/quasigroup_completion.cpp
    * Gecode/R: http://www.hakank.org/gecode_r/quasigroup_completion.rb
    * JaCoP: http://www.hakank.org/JaCoP/QuasigroupCompletion.java
    * MiniZinc: http://www.hakank.org/minizinc/quasigroup_completion.mzn
    * Tailor/Essence': http://www.hakank.org/tailor/quasigroup_completion.eprime
    * SICStus: http://hakank.org/sicstus/quasigroup_completion.pl
    * Zinc: http://hakank.org/minizinc/quasigroup_completion.zinc

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver import pywrapcp

default_n = 5
X = 0
# default problem
# (This is the same as quasigroup1.txt)
default_puzzle = [[1, X, X, X, 4], [X, 5, X, X, X], [4, X, X, 2, X],
                  [X, 4, X, X, X], [X, X, 5, X, 1]]


def main(puzzle="", n=0):

  # Create the solver.
  solver = pywrapcp.Solver("Quasigroup completion")

  #
  # data
  #

  if puzzle == "":
    puzzle = default_puzzle
    n = default_n

  print("Problem:")
  print_game(puzzle, n, n)

  # declare variables
  x = {}
  for i in range(n):
    for j in range(n):
      x[(i, j)] = solver.IntVar(1, n, "x %i %i" % (i, j))

  xflat = [x[(i, j)] for i in range(n) for j in range(n)]

  #
  # constraints
  #

  #
  # set the clues
  #
  for i in range(n):
    for j in range(n):
      if puzzle[i][j] > X:
        solver.Add(x[i, j] == puzzle[i][j])

  #
  # rows and columns must be different
  #
  for i in range(n):
    solver.Add(solver.AllDifferent([x[i, j] for j in range(n)]))
    solver.Add(solver.AllDifferent([x[j, i] for j in range(n)]))

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(xflat)

  # This version prints out the solution directly, and
  # don't collect them as solver.FirstSolutionCollector(solution) do
  # (db: DecisionBuilder)
  db = solver.Phase(xflat, solver.INT_VAR_SIMPLE, solver.ASSIGN_MIN_VALUE)

  solver.NewSearch(db)
  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    print("Solution %i" % num_solutions)
    xval = [x[(i, j)].Value() for i in range(n) for j in range(n)]
    for i in range(n):
      for j in range(n):
        print(xval[i * n + j], end=" ")
      print()
    print()
  solver.EndSearch()

  if num_solutions == 0:
    print("No solutions found")

  # # Note: AllSolution may take very much RAM, hence I choose to
  # # show just the first solution.
  # # collector = solver.AllSolutionCollector(solution)
  # collector = solver.FirstSolutionCollector(solution)
  # solver.Solve(solver.Phase([x[(i,j)] for i in range(n) for j in range(n)],
  #                           solver.CHOOSE_FIRST_UNBOUND,
  #                           solver.ASSIGN_MIN_VALUE),
  #                           [collector])
  #
  # num_solutions = collector.SolutionCount()
  # print "\nnum_solutions: ", num_solutions
  # if num_solutions > 0:
  #     print "\nJust showing the first solution..."
  #     for s in range(num_solutions):
  #         xval = [collector.Value(s, x[(i,j)]) for i in range(n) for j in range(n)]
  #         for i in range(n):
  #             for j in range(n):
  #                 print xval[i*n+j],
  #             print
  #         print

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


#
# Read a problem instance from a file
#
def read_problem(file):
  f = open(file, "r")
  n = int(f.readline())
  game = []
  for i in range(n):
    x = f.readline()
    row_x = (x.rstrip()).split(" ")
    row = [0] * n
    for j in range(n):
      if row_x[j] == ".":
        tmp = 0
      else:
        tmp = int(row_x[j])
      row[j] = tmp
    game.append(row)
  return [game, n]


def print_board(x, rows, cols):
  for i in range(rows):
    for j in range(cols):
      print("% 2s" % x[i, j], end=" ")
    print("")


def print_game(game, rows, cols):
  for i in range(rows):
    for j in range(cols):
      print("% 2s" % game[i][j], end=" ")
    print("")


if __name__ == "__main__":

  if len(sys.argv) > 1:
    file = sys.argv[1]
    print("Problem instance from", file)
    [game, n] = read_problem(file)
    main(game, n)
  else:
    main()
