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

  Production planning problem in Google or-tools.

  From the OPL model production.mod.

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

  #
  # data
  #
  kluski = 0
  capellini = 1
  fettucine = 2
  products = ['kluski', 'capellini', 'fettucine']
  num_products = len(products)

  flour = 0
  eggs = 1
  resources = ['flour', 'eggs']
  num_resources = len(resources)

  consumption = [[0.5, 0.2], [0.4, 0.4], [0.3, 0.6]]
  capacity = [20, 40]
  demand = [100, 200, 300]
  inside_cost = [0.6, 0.8, 0.3]
  outside_cost = [0.8, 0.9, 0.4]

  #
  # declare variables
  #
  inside = [
      solver.NumVar(0, 10000, 'inside[%i]' % p) for p in range(num_products)
  ]
  outside = [
      solver.NumVar(0, 10000, 'outside[%i]' % p) for p in range(num_products)
  ]

  # to minimize
  z = solver.Sum([
      inside_cost[p] * inside[p] + outside_cost[p] * outside[p]
      for p in range(num_products)
  ])

  #
  # constraints
  #
  for r in range(num_resources):
    solver.Add(
        solver.Sum([consumption[p][r] * inside[p]
                    for p in range(num_products)]) <= capacity[r])

  for p in range(num_products):
    solver.Add(inside[p] + outside[p] >= demand[p])

  objective = solver.Minimize(z)

  solver.Solve()

  print()
  print('z = ', solver.Objective().Value())

  for p in range(num_products):
    print(
        products[p],
        ': inside:',
        inside[p].SolutionValue(),
        '(ReducedCost:',
        inside[p].ReducedCost(),
        ')',
        end=' ')
    print('outside:', outside[p].SolutionValue(), ' (ReducedCost:',
          outside[p].ReducedCost(), ')')
  print()


if __name__ == '__main__':

  sol = 'CBC'
  if len(sys.argv) > 1:
    sol = sys.argv[1]
    if sol != 'GLPK' and sol != 'CBC':
      print('Solver must be either GLPK or CBC')
      sys.exit(1)

  main(sol)
