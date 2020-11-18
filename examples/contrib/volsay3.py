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

  Volsay problem in Google or-tools.

  From the OPL model volsay.mod
  Using arrays.

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
from ortools.linear_solver import pywraplp


def main(unused_argv):

  # Create the solver.

  # using GLPK
  # solver = pywraplp.Solver('CoinsGridGLPK',
  #                          pywraplp.Solver.GLPK_LINEAR_PROGRAMMING)

  # Using CLP
  solver = pywraplp.Solver('CoinsGridCLP',
                           pywraplp.Solver.CLP_LINEAR_PROGRAMMING)

  # data
  num_products = 2

  products = ['Gas', 'Chloride']
  components = ['nitrogen', 'hydrogen', 'chlorine']

  demand = [[1, 3, 0], [1, 4, 1]]
  profit = [30, 40]
  stock = [50, 180, 40]

  # declare variables
  production = [
      solver.NumVar(0, 100000, 'production[%i]' % i)
      for i in range(num_products)
  ]

  #
  # constraints
  #
  for c in range(len(components)):
    solver.Add(
        solver.Sum([demand[p][c] * production[p]
                    for p in range(len(products))]) <= stock[c])

  # objective
  # Note: there is no support for solver.ScalProd in the LP/IP interface
  objective = solver.Maximize(
      solver.Sum([production[p] * profit[p] for p in range(num_products)]))

  print('NumConstraints:', solver.NumConstraints())
  print('NumVariables:', solver.NumVariables())
  print()

  #
  # solution and search
  #
  solver.Solve()

  print()
  print('objective = ', solver.Objective().Value())
  for i in range(num_products):
    print(products[i], '=', production[i].SolutionValue(), end=' ')
    print('ReducedCost = ', production[i].ReducedCost())

  print()
  print('walltime  :', solver.WallTime(), 'ms')
  print('iterations:', solver.Iterations())


if __name__ == '__main__':
  main('Volsay')
