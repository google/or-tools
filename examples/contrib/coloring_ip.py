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

  Simple coloring problem using MIP in Google CP Solver.

  Inspired by the GLPK:s model color.mod
  '''
  COLOR, Graph Coloring Problem

  Written in GNU MathProg by Andrew Makhorin <mao@mai2.rcnet.ru>

  Given an undirected loopless graph G = (V, E), where V is a set of
  nodes, E <= V x V is a set of arcs, the Graph Coloring Problem is to
  find a mapping (coloring) F: V -> C, where C = {1, 2, ... } is a set
  of colors whose cardinality is as small as possible, such that
  F(i) != F(j) for every arc (i,j) in E, that is adjacent nodes must
  be assigned different colors.
  '''

  Compare with the MiniZinc model:
    http://www.hakank.org/minizinc/coloring_ip.mzn

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

  # max number of colors
  # [we know that 4 suffices for normal maps]
  nc = 5

  # number of nodes
  n = 11
  # set of nodes
  V = list(range(n))

  num_edges = 20

  #
  # Neighbours
  #
  # This data correspond to the instance myciel3.col from:
  # http://mat.gsia.cmu.edu/COLOR/instances.html
  #
  # Note: 1-based (adjusted below)
  E = [[1, 2], [1, 4], [1, 7], [1, 9], [2, 3], [2, 6], [2, 8], [3, 5], [3, 7],
       [3, 10], [4, 5], [4, 6], [4, 10], [5, 8], [5, 9], [6, 11], [7, 11],
       [8, 11], [9, 11], [10, 11]]

  #
  # declare variables
  #

  # x[i,c] = 1 means that node i is assigned color c
  x = {}
  for v in V:
    for j in range(nc):
      x[v, j] = solver.IntVar(0, 1, 'v[%i,%i]' % (v, j))

  # u[c] = 1 means that color c is used, i.e. assigned to some node
  u = [solver.IntVar(0, 1, 'u[%i]' % i) for i in range(nc)]

  # number of colors used, to minimize
  obj = solver.Sum(u)

  #
  # constraints
  #

  # each node must be assigned exactly one color
  for i in V:
    solver.Add(solver.Sum([x[i, c] for c in range(nc)]) == 1)

  # adjacent nodes cannot be assigned the same color
  # (and adjust to 0-based)
  for i in range(num_edges):
    for c in range(nc):
      solver.Add(x[E[i][0] - 1, c] + x[E[i][1] - 1, c] <= u[c])

  # objective
  objective = solver.Minimize(obj)

  #
  # solution
  #
  solver.Solve()

  print()
  print('number of colors:', int(solver.Objective().Value()))
  print('colors used:', [int(u[i].SolutionValue()) for i in range(nc)])
  print()

  for v in V:
    print('v%i' % v, ' color ', end=' ')
    for c in range(nc):
      if int(x[v, c].SolutionValue()) == 1:
        print(c)

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
