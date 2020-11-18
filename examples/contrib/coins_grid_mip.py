# Copyright 2011 Hakan Kjellerstrand hakank@gmail.com
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

  This is a MIP version of
     http://www.hakank.org/google_or_tools/coins_grid.py
  and use

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""

from ortools.linear_solver import pywraplp


def main(unused_argv):
  # Create the solver.

  # using CBC
  solver = pywraplp.Solver('CoinsGridCBC',
                           pywraplp.Solver.CBC_MIXED_INTEGER_PROGRAMMING)

  # Using CLP
  # solver = pywraplp.Solver('CoinsGridCLP',
  #                          pywraplp.Solver.CBC_MIXED_INTEGER_PROGRAMMING)

  # data
  n = 31  # the grid size
  c = 14  # number of coins per row/column

  # declare variables
  x = {}
  for i in range(n):
    for j in range(n):
      x[(i, j)] = solver.IntVar(0, 1, 'x[%i,%i]' % (i, j))

  #
  # constraints
  #

  # sum rows/columns == c
  for i in range(n):
    solver.Add(solver.Sum([x[(i, j)] for j in range(n)]) == c)  # sum rows
    solver.Add(solver.Sum([x[(j, i)] for j in range(n)]) == c)  # sum cols

  # quadratic horizonal distance var
  objective_var = solver.Sum(
      [x[(i, j)] * (i - j) * (i - j) for i in range(n) for j in range(n)])

  # objective
  objective = solver.Minimize(objective_var)

  #
  # solution and search
  #
  solver.Solve()

  for i in range(n):
    for j in range(n):
      # int representation
      print(int(x[(i, j)].SolutionValue()), end=' ')
    print()
  print()

  print()
  print('walltime  :', solver.WallTime(), 'ms')
  # print 'iterations:', solver.Iterations()


if __name__ == '__main__':
  main('coin grids')
