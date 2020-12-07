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

  Blending problem in Google or-tools.

  From the OPL model blending.mod.

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
  NbMetals = 3
  NbRaw = 2
  NbScrap = 2
  NbIngo = 1
  Metals = list(range(NbMetals))
  Raws = list(range(NbRaw))
  Scraps = list(range(NbScrap))
  Ingos = list(range(NbIngo))

  CostMetal = [22, 10, 13]
  CostRaw = [6, 5]
  CostScrap = [7, 8]
  CostIngo = [9]
  Low = [0.05, 0.30, 0.60]
  Up = [0.10, 0.40, 0.80]
  PercRaw = [[0.20, 0.01], [0.05, 0], [0.05, 0.30]]
  PercScrap = [[0, 0.01], [0.60, 0], [0.40, 0.70]]
  PercIngo = [[0.10], [0.45], [0.45]]
  Alloy = 71

  #
  # variables
  #
  p = [solver.NumVar(0, solver.Infinity(), 'p[%i]' % i) for i in Metals]
  r = [solver.NumVar(0, solver.Infinity(), 'r[%i]' % i) for i in Raws]
  s = [solver.NumVar(0, solver.Infinity(), 's[%i]' % i) for i in Scraps]
  ii = [solver.IntVar(0, solver.Infinity(), 'ii[%i]' % i) for i in Ingos]
  metal = [
      solver.NumVar(Low[j] * Alloy, Up[j] * Alloy, 'metal[%i]' % j)
      for j in Metals
  ]

  z = solver.NumVar(0, solver.Infinity(), 'z')

  #
  # constraints
  #

  solver.Add(z == solver.Sum([CostMetal[i] * p[i] for i in Metals]) +
             solver.Sum([CostRaw[i] * r[i] for i in Raws]) +
             solver.Sum([CostScrap[i] * s[i] for i in Scraps]) +
             solver.Sum([CostIngo[i] * ii[i] for i in Ingos]))

  for j in Metals:
    solver.Add(
        metal[j] == p[j] + solver.Sum([PercRaw[j][k] * r[k] for k in Raws]) +
        solver.Sum([PercScrap[j][k] * s[k] for k in Scraps]) +
        solver.Sum([PercIngo[j][k] * ii[k] for k in Ingos]))

  solver.Add(solver.Sum(metal) == Alloy)

  objective = solver.Minimize(z)

  #
  # solution and search
  #
  solver.Solve()

  print()

  print('z = ', solver.Objective().Value())
  print('Metals')
  for i in Metals:
    print(p[i].SolutionValue(), end=' ')
  print()

  print('Raws')
  for i in Raws:
    print(r[i].SolutionValue(), end=' ')
  print()

  print('Scraps')
  for i in Scraps:
    print(s[i].SolutionValue(), end=' ')
  print()

  print('Ingos')
  for i in Ingos:
    print(ii[i].SolutionValue(), end=' ')
  print()

  print('Metals')
  for i in Metals:
    print(metal[i].SolutionValue(), end=' ')
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
