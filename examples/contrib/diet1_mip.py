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

  Simple diet problem using MIP in Google CP Solver.

  Standard Operations Research example.


  Minimize the cost for the products:
  Type of                        Calories   Chocolate    Sugar    Fat
  Food                                      (ounces)     (ounces) (ounces)
  Chocolate Cake (1 slice)       400           3            2      2
  Chocolate ice cream (1 scoop)  200           2            2      4
  Cola (1 bottle)                150           0            4      1
  Pineapple cheesecake (1 piece) 500           0            4      5

  Compare with the CP model:
    http://www.hakank.org/google_or_tools/diet1.py


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.linear_solver import pywraplp


def main(sol='CBC'):

  # Create the solver.

  print('Solver: ', sol)

  if sol == 'GLPK':
    # using GLPK
    solver = pywraplp.Solver('CoinsGridGLPK',
                             pywraplp.Solver.GLPK_MIXED_INTEGER_PROGRAMMING)
  else:
    # Using CBC
    solver = pywraplp.Solver('CoinsGridCLP',
                             pywraplp.Solver.CBC_MIXED_INTEGER_PROGRAMMING)

  #
  # data
  #
  n = 4
  price = [50, 20, 30, 80]  # in cents
  limits = [500, 6, 10, 8]  # requirements for each nutrition type

  # nutritions for each product
  calories = [400, 200, 150, 500]
  chocolate = [3, 2, 0, 0]
  sugar = [2, 2, 4, 4]
  fat = [2, 4, 1, 5]

  #
  # declare variables
  #
  x = [solver.IntVar(0, 100, 'x%d' % i) for i in range(n)]
  cost = solver.Sum([x[i] * price[i] for i in range(n)])

  #
  # constraints
  #
  solver.Add(solver.Sum([x[i] * calories[i] for i in range(n)]) >= limits[0])
  solver.Add(solver.Sum([x[i] * chocolate[i] for i in range(n)]) >= limits[1])
  solver.Add(solver.Sum([x[i] * sugar[i] for i in range(n)]) >= limits[2])
  solver.Add(solver.Sum([x[i] * fat[i] for i in range(n)]) >= limits[3])

  # objective
  objective = solver.Minimize(cost)

  #
  # solution
  #
  solver.Solve()

  print('Cost:', solver.Objective().Value())
  print([int(x[i].SolutionValue()) for i in range(n)])

  print()
  print('WallTime:', solver.WallTime())
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
