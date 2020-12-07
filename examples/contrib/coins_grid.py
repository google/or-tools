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
  Coins grid problem in Google CP Solver.

  Problem from
  Tony Hurlimann: "A coin puzzle - SVOR-contest 2007"
  http://www.svor.ch/competitions/competition2007/AsroContestSolution.pdf
  '''
  In a quadratic grid (or a larger chessboard) with 31x31 cells, one should
  place coins in such a way that the following conditions are fulfilled:
     1. In each row exactly 14 coins must be placed.
     2. In each column exactly 14 coins must be placed.
     3. The sum of the quadratic horizontal distance from the main diagonal
        of all cells containing a coin must be as small as possible.
     4. In each cell at most one coin can be placed.
  The description says to place 14x31 = 434 coins on the chessboard each row
  containing 14 coins and each column also containing 14 coins.
  '''

  Cf the LPL model:
  http://diuflx71.unifr.ch/lpl/GetModel?name=/puzzles/coin

  Note: Laurent Perron helped me to improve this model.

  Compare with the following models:
  * Tailor/Essence': http://hakank.org/tailor/coins_grid.eprime
  * MiniZinc: http://hakank.org/minizinc/coins_grid.mzn
  * SICStus: http://hakank.org/sicstus/coins_grid.pl
  * Zinc: http://hakank.org/minizinc/coins_grid.zinc
  * Choco: http://hakank.org/choco/CoinsGrid.java
  * Comet: http://hakank.org/comet/coins_grid.co
  * ECLiPSe: http://hakank.org/eclipse/coins_grid.ecl
  * Gecode: http://hakank.org/gecode/coins_grid.cpp
  * Gecode/R: http://hakank.org/gecode_r/coins_grid.rb
  * JaCoP: http://hakank.org/JaCoP/CoinsGrid.java

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""

import sys
from ortools.constraint_solver import pywrapcp


def main(n, c):
  # Create the solver.
  solver = pywrapcp.Solver("Coins grid")
  # data

  print("n: ", n)
  print("c: ", c)

  # declare variables
  x = {}
  for i in range(n):
    for j in range(n):
      x[(i, j)] = solver.BoolVar("x %i %i" % (i, j))

  #
  # constraints
  #

  # sum rows/columns == c
  for i in range(n):
    solver.Add(solver.SumEquality([x[(i, j)] for j in range(n)], c))  # sum rows
    solver.Add(solver.SumEquality([x[(j, i)] for j in range(n)], c))  # sum cols

  # quadratic horizonal distance var
  objective_var = solver.Sum(
      [x[(i, j)] * (i - j) * (i - j) for i in range(n) for j in range(n)])

  # objective
  objective = solver.Minimize(objective_var, 1)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add([x[(i, j)] for i in range(n) for j in range(n)])
  solution.AddObjective(objective_var)

  # last solutions
  collector = solver.LastSolutionCollector(solution)
  search_log = solver.SearchLog(1000000, objective_var)
  restart = solver.ConstantRestart(300)
  solver.Solve(
      solver.Phase([x[(i, j)] for i in range(n) for j in range(n)],
                   solver.CHOOSE_RANDOM, solver.ASSIGN_MAX_VALUE),
      [collector, search_log, objective])

  print("objective:", collector.ObjectiveValue(0))
  for i in range(n):
    for j in range(n):
      print(collector.Value(0, x[(i, j)]), end=" ")
    print()
  print()

  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


if __name__ == "__main__":
  # data
  n = 5  # the grid size
  c = 2  # number of coins per row/column
  if len(sys.argv) > 1:
    n = int(sys.argv[1])
  if len(sys.argv) > 2:
    c = int(sys.argv[2])

  main(n, c)
