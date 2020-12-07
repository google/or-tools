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

  Max flow problem in Google CP Solver.

  From Winston 'Operations Research', page 420f, 423f
  Sunco Oil example.

  Compare with the following models:
  * MiniZinc: http://www.hakank.org/minizinc/max_flow_winston1.mzn
  * Comet: http://hakank.org/comet/max_flow_winston1.co


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver import pywrapcp


def main():

  # Create the solver.
  solver = pywrapcp.Solver('Max flow problem, Winston')

  #
  # data
  #
  n = 5
  nodes = list(range(n))

  # the arcs
  # Note:
  # This is 1-based to be compatible with other
  # implementations.
  arcs1 = [[1, 2], [1, 3], [2, 3], [2, 4], [3, 5], [4, 5], [5, 1]]

  # convert arcs to 0-based
  arcs = []
  for (a_from, a_to) in arcs1:
    a_from -= 1
    a_to -= 1
    arcs.append([a_from, a_to])

  num_arcs = len(arcs)

  # capacities
  cap = [2, 3, 3, 4, 2, 1, 100]

  # convert arcs to matrix
  # for sanity checking below
  mat = {}
  for i in nodes:
    for j in nodes:
      c = 0
      for k in range(num_arcs):
        if arcs[k][0] == i and arcs[k][1] == j:
          c = 1
      mat[i, j] = c

  #
  # declare variables
  #
  flow = {}
  for i in nodes:
    for j in nodes:
      flow[i, j] = solver.IntVar(0, 200, 'flow %i %i' % (i, j))

  flow_flat = [flow[i, j] for i in nodes for j in nodes]

  z = solver.IntVar(0, 10000, 'z')

  #
  # constraints
  #
  solver.Add(z == flow[n - 1, 0])

  # capacity of arcs
  for i in range(num_arcs):
    solver.Add(flow[arcs[i][0], arcs[i][1]] <= cap[i])

  # inflows == outflows
  for i in nodes:
    s1 = solver.Sum([
        flow[arcs[k][0], arcs[k][1]] for k in range(num_arcs) if arcs[k][1] == i
    ])
    s2 = solver.Sum([
        flow[arcs[k][0], arcs[k][1]] for k in range(num_arcs) if arcs[k][0] == i
    ])
    solver.Add(s1 == s2)

  # sanity: just arcs with connections can have a flow
  for i in nodes:
    for j in nodes:
      if mat[i, j] == 0:
        solver.Add(flow[i, j] == 0)

  # objective: maximize z
  objective = solver.Maximize(z, 1)

  #
  # solution and search
  #
  db = solver.Phase(flow_flat, solver.INT_VAR_DEFAULT, solver.INT_VALUE_DEFAULT)

  solver.NewSearch(db, [objective])
  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    print('z:', z.Value())
    for i in nodes:
      for j in nodes:
        print(flow[i, j].Value(), end=' ')
      print()
    print()

  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('WallTime:', solver.WallTime(), 'ms')


if __name__ == '__main__':
  main()
