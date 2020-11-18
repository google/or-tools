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

  Game theory in Google or-tools.

  2 player zero sum game.

  From Taha, Operations Research (8'th edition), page 528.

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.linear_solver import pywraplp


def main(sol='CBC'):

  # Create the solver.

  # using GLPK
  if sol == 'GLPK':
    solver = pywraplp.Solver('CoinsGridGLPK',
                             pywraplp.Solver.GLPK_LINEAR_PROGRAMMING)
  else:
    # Using CLP
    solver = pywraplp.Solver('CoinsGridCLP',
                             pywraplp.Solver.CLP_LINEAR_PROGRAMMING)

  # data
  rows = 3
  cols = 3

  game = [[3.0, -1.0, -3.0], [-2.0, 4.0, -1.0], [-5.0, -6.0, 2.0]]

  #
  # declare variables
  #

  #
  # row player
  #
  x1 = [solver.NumVar(0, 1, 'x1[%i]' % i) for i in range(rows)]

  v = solver.NumVar(-2, 2, 'v')

  for i in range(rows):
    solver.Add(v - solver.Sum([x1[j] * game[j][i] for j in range(cols)]) <= 0)

  solver.Add(solver.Sum(x1) == 1)

  objective = solver.Maximize(v)

  solver.Solve()

  print()
  print('row player:')
  print('v = ', solver.Objective().Value())
  print('Strategies: ')
  for i in range(rows):
    print(x1[i].SolutionValue(), end=' ')
  print()
  print()

  #
  # For column player:
  #
  x2 = [solver.NumVar(0, 1, 'x2[%i]' % i) for i in range(cols)]

  v2 = solver.NumVar(-2, 2, 'v2')

  for i in range(cols):
    solver.Add(v2 - solver.Sum([x2[j] * game[i][j] for j in range(rows)]) >= 0)

  solver.Add(solver.Sum(x2) == 1)

  objective = solver.Minimize(v2)

  solver.Solve()

  print()
  print('column player:')
  print('v2 = ', solver.Objective().Value())
  print('Strategies: ')
  for i in range(rows):
    print(x2[i].SolutionValue(), end=' ')
  print()

  print()
  print('walltime  :', solver.WallTime(), 'ms')
  print('iterations:', solver.Iterations())
  print()


if __name__ == '__main__':
  sol = 'CBC'
  if len(sys.argv) > 1:
    sol = sys.argv[1]
    if sol != 'GLPK' and sol != 'CBC':
      print('Solver must be either GLPK or CBC')
      sys.exit(1)

  main(sol)
