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

  Assignment problem using MIP in Google or-tools.

  From GLPK:s example assign.mod:
  '''
  The assignment problem is one of the fundamental combinatorial
  optimization problems.

  In its most general form, the problem is as follows:

  There are a number of agents and a number of tasks. Any agent can be
  assigned to perform any task, incurring some cost that may vary
  depending on the agent-task assignment. It is required to perform all
  tasks by assigning exactly one agent to each task in such a way that
  the total cost of the assignment is minimized.

  (From Wikipedia, the free encyclopedia.)
  '''

  Compare with the Comet model:
     http://www.hakank.org/comet/assignment6.co


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

  # number of agents
  m = 8

  # number of tasks
  n = 8

  # set of agents
  I = list(range(m))

  # set of tasks
  J = list(range(n))

  # cost of allocating task j to agent i
  # """
  # These data correspond to an example from [Christofides].
  #
  # Optimal solution is 76
  # """
  c = [[13, 21, 20, 12, 8, 26, 22, 11], [12, 36, 25, 41, 40, 11, 4, 8],
       [35, 32, 13, 36, 26, 21, 13, 37], [34, 54, 7, 8, 12, 22, 11, 40],
       [21, 6, 45, 18, 24, 34, 12, 48], [42, 19, 39, 15, 14, 16, 28, 46],
       [16, 34, 38, 3, 34, 40, 22, 24], [26, 20, 5, 17, 45, 31, 37, 43]]

  #
  # variables
  #

  # For the output: the assignment as task number.
  assigned = [solver.IntVar(0, 10000, 'assigned[%i]' % j) for j in J]

  costs = [solver.IntVar(0, 10000, 'costs[%i]' % i) for i in I]

  x = {}
  for i in range(n):
    for j in range(n):
      x[i, j] = solver.IntVar(0, 1, 'x[%i,%i]' % (i, j))

  # total cost, to be minimized
  z = solver.Sum([c[i][j] * x[i, j] for i in I for j in J])

  #
  # constraints
  #
  # each agent can perform at most one task
  for i in I:
    solver.Add(solver.Sum([x[i, j] for j in J]) <= 1)

  # each task must be assigned exactly to one agent
  for j in J:
    solver.Add(solver.Sum([x[i, j] for i in I]) == 1)

  # to which task and what cost is person i assigned (for output in MiniZinc)
  for i in I:
    solver.Add(assigned[i] == solver.Sum([j * x[i, j] for j in J]))
    solver.Add(costs[i] == solver.Sum([c[i][j] * x[i, j] for j in J]))

  # objective
  objective = solver.Minimize(z)

  #
  # solution and search
  #
  solver.Solve()

  print()
  print('z: ', int(solver.Objective().Value()))

  print('Assigned')
  for j in J:
    print(int(assigned[j].SolutionValue()), end=' ')
  print()

  print('Matrix:')
  for i in I:
    for j in J:
      print(int(x[i, j].SolutionValue()), end=' ')
    print()
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
