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

  3 jugs problem using MIP in Google or-tools.

  A.k.a. water jugs problem.

  Problem from Taha 'Introduction to Operations Research',
  page 245f .

  Compare with the CP model:
     http://www.hakank.org/google_or_tools/3_jugs_regular

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
  n = 15
  start = 0  # start node
  end = 14  # end node
  M = 999  # a large number

  nodes = [
      '8,0,0',  # start
      '5,0,3',
      '5,3,0',
      '2,3,3',
      '2,5,1',
      '7,0,1',
      '7,1,0',
      '4,1,3',
      '3,5,0',
      '3,2,3',
      '6,2,0',
      '6,0,2',
      '1,5,2',
      '1,4,3',
      '4,4,0'  # goal!
  ]

  # distance
  d = [[M, 1, M, M, M, M, M, M, 1, M, M, M, M, M, M],
       [M, M, 1, M, M, M, M, M, M, M, M, M, M, M, M],
       [M, M, M, 1, M, M, M, M, 1, M, M, M, M, M, M],
       [M, M, M, M, 1, M, M, M, M, M, M, M, M, M, M],
       [M, M, M, M, M, 1, M, M, 1, M, M, M, M, M, M],
       [M, M, M, M, M, M, 1, M, M, M, M, M, M, M, M],
       [M, M, M, M, M, M, M, 1, 1, M, M, M, M, M, M],
       [M, M, M, M, M, M, M, M, M, M, M, M, M, M, 1],
       [M, M, M, M, M, M, M, M, M, 1, M, M, M, M, M],
       [M, 1, M, M, M, M, M, M, M, M, 1, M, M, M, M],
       [M, M, M, M, M, M, M, M, M, M, M, 1, M, M, M],
       [M, 1, M, M, M, M, M, M, M, M, M, M, 1, M, M],
       [M, M, M, M, M, M, M, M, M, M, M, M, M, 1, M],
       [M, 1, M, M, M, M, M, M, M, M, M, M, M, M, 1],
       [M, M, M, M, M, M, M, M, M, M, M, M, M, M, M]]

  #
  # variables
  #

  # requirements (right hand statement)
  rhs = [solver.IntVar(-1, 1, 'rhs[%i]' % i) for i in range(n)]

  x = {}
  for i in range(n):
    for j in range(n):
      x[i, j] = solver.IntVar(0, 1, 'x[%i,%i]' % (i, j))

  out_flow = [solver.IntVar(0, 1, 'out_flow[%i]' % i) for i in range(n)]
  in_flow = [solver.IntVar(0, 1, 'in_flow[%i]' % i) for i in range(n)]

  # length of path, to be minimized
  z = solver.Sum(
      [d[i][j] * x[i, j] for i in range(n) for j in range(n) if d[i][j] < M])

  #
  # constraints
  #

  for i in range(n):
    if i == start:
      solver.Add(rhs[i] == 1)
    elif i == end:
      solver.Add(rhs[i] == -1)
    else:
      solver.Add(rhs[i] == 0)

  # outflow constraint
  for i in range(n):
    solver.Add(
        out_flow[i] == solver.Sum([x[i, j] for j in range(n) if d[i][j] < M]))

  # inflow constraint
  for j in range(n):
    solver.Add(
        in_flow[j] == solver.Sum([x[i, j] for i in range(n) if d[i][j] < M]))

  # inflow = outflow
  for i in range(n):
    solver.Add(out_flow[i] - in_flow[i] == rhs[i])

  # objective
  objective = solver.Minimize(z)

  #
  # solution and search
  #
  solver.Solve()

  print()
  print('z: ', int(solver.Objective().Value()))

  t = start
  while t != end:
    print(nodes[t], '->', end=' ')
    for j in range(n):
      if x[t, j].SolutionValue() == 1:
        print(nodes[j])
        t = j
        break

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
