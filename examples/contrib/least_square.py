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

  Least square optimization problem in Google or-tools.

  Solving a fourth grade least square equation.

  From the Swedish book 'Optimeringslara' [Optimization Theory],
  page 286f.

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
  # number of points
  num = 14

  # temperature
  t = [20, 30, 80, 125, 175, 225, 275, 325, 360, 420, 495, 540, 630, 700]

  # percentage gas
  F = [
      0.0, 5.8, 14.7, 31.6, 43.2, 58.3, 78.4, 89.4, 96.4, 99.1, 99.5, 99.9,
      100.0, 100.0
  ]

  p = 4

  #
  # declare variables
  #
  a = [solver.NumVar(-100, 100, 'a[%i]' % i) for i in range(p + 1)]

  # to minimize
  z = solver.Sum([
      (F[i] - (sum([a[j] * t[i]**j for j in range(p + 1)]))) for i in range(num)
  ])

  #
  # constraints
  #
  solver.Add(solver.Sum([20**i * a[i] for i in range(p + 1)]) == 0)

  solver.Add((a[0] + sum([700.0**j * a[j] for j in range(1, p + 1)])) == 100.0)

  for i in range(num):
    solver.Add(
        solver.Sum([j * a[j] * t[i]**(j - 1) for j in range(p + 1)]) >= 0)

  objective = solver.Minimize(z)

  solver.Solve()

  print()
  print('z = ', solver.Objective().Value())
  for i in range(p + 1):
    print(a[i].SolutionValue(), end=' ')
  print()


if __name__ == '__main__':

  sol = 'CBC'
  if len(sys.argv) > 1:
    sol = sys.argv[1]
    if sol != 'GLPK' and sol != 'CBC':
      print('Solver must be either GLPK or CBC')
      sys.exit(1)

  main(sol)
