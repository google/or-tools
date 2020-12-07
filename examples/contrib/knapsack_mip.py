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

  Knapsack problem using MIP in Google or-tools.

  From the OPL model knapsack.mod

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.linear_solver import pywraplp


def main(sol='CBC'):

  # Create the solver.

  print('Solver: ', sol)

  # using GLPK
  if sol == 'GLPK':
    solver = pywraplp.Solver('CoinsGridGLPK',
                             pywraplp.Solver.GLPK_MIXED_INTEGER_PROGRAMMING)
  else:
    # Using CBC
    solver = pywraplp.Solver('CoinsGridCBC',
                             pywraplp.Solver.CBC_MIXED_INTEGER_PROGRAMMING)

  #
  # data
  #
  nb_items = 12
  nb_resources = 7
  items = list(range(nb_items))
  resources = list(range(nb_resources))

  capacity = [18209, 7692, 1333, 924, 26638, 61188, 13360]
  value = [96, 76, 56, 11, 86, 10, 66, 86, 83, 12, 9, 81]
  use = [[19, 1, 10, 1, 1, 14, 152, 11, 1, 1, 1, 1],
         [0, 4, 53, 0, 0, 80, 0, 4, 5, 0, 0, 0],
         [4, 660, 3, 0, 30, 0, 3, 0, 4, 90, 0, 0],
         [7, 0, 18, 6, 770, 330, 7, 0, 0, 6, 0, 0],
         [0, 20, 0, 4, 52, 3, 0, 0, 0, 5, 4, 0],
         [0, 0, 40, 70, 4, 63, 0, 0, 60, 0, 4, 0],
         [0, 32, 0, 0, 0, 5, 0, 3, 0, 660, 0, 9]]

  max_value = max(capacity)

  #
  # variables
  #
  take = [solver.IntVar(0, max_value, 'take[%i]' % j) for j in items]

  # total cost, to be maximized
  z = solver.Sum([value[i] * take[i] for i in items])

  #
  # constraints
  #
  for r in resources:
    solver.Add(solver.Sum([use[r][i] * take[i] for i in items]) <= capacity[r])

  # objective
  objective = solver.Maximize(z)

  #
  # solution and search
  #
  solver.Solve()

  print()
  print('z: ', int(solver.Objective().Value()))

  print('take:', end=' ')
  for i in items:
    print(int(take[i].SolutionValue()), end=' ')
  print()

  print()
  print('walltime  :', solver.WallTime(), 'ms')
  if sol == 'CBC':
    print('iterations:', solver.Iterations())


if __name__ == '__main__':

  sol = 'CBC'
  if len(sys.argv) > 1:
    sol = sys.argv[1]
    if sol != 'GLPK' and sol != 'CBC':
      print('Solver must be either GLPK or CBC')
      sys.exit(1)

  main(sol)
