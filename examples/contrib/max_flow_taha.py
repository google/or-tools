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

  From Taha 'Introduction to Operations Research', Example 6.4-2

  Translated from the AMPL code at
  http://taha.ineg.uark.edu/maxflo.txt

  Compare with the following model:
  * MiniZinc: http://www.hakank.org/minizinc/max_flow_taha.mzn

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
from ortools.constraint_solver import pywrapcp


def main():

  # Create the solver.
  solver = pywrapcp.Solver('Max flow problem, Taha')

  #
  # data
  #
  n = 5
  start = 0
  end = n - 1

  nodes = list(range(n))

  # cost matrix
  c = [[0, 20, 30, 10, 0], [0, 0, 40, 0, 30], [0, 0, 0, 10, 20],
       [0, 0, 5, 0, 20], [0, 0, 0, 0, 0]]

  #
  # declare variables
  #
  x = {}
  for i in nodes:
    for j in nodes:
      x[i, j] = solver.IntVar(0, c[i][j], 'x[%i,%i]' % (i, j))

  x_flat = [x[i, j] for i in nodes for j in nodes]
  out_flow = [solver.IntVar(0, 10000, 'out_flow[%i]' % i) for i in nodes]
  in_flow = [solver.IntVar(0, 10000, 'in_flow[%i]' % i) for i in nodes]

  total = solver.IntVar(0, 10000, 'z')

  #
  # constraints
  #
  cost_sum = solver.Sum([x[start, j] for j in nodes if c[start][j] > 0])
  solver.Add(total == cost_sum)

  for i in nodes:
    in_flow_sum = solver.Sum([x[j, i] for j in nodes if c[j][i] > 0])
    solver.Add(in_flow[i] == in_flow_sum)

    out_flow_sum = solver.Sum([x[i, j] for j in nodes if c[i][j] > 0])
    solver.Add(out_flow[i] == out_flow_sum)

  # in_flow == out_flow
  for i in nodes:
    if i != start and i != end:
      solver.Add(out_flow[i] - in_flow[i] == 0)

  s1 = [x[i, start] for i in nodes if c[i][start] > 0]
  if len(s1) > 0:
    solver.Add(solver.Sum([x[i, start] for i in nodes if c[i][start] > 0] == 0))

  s2 = [x[end, j] for j in nodes if c[end][j] > 0]
  if len(s2) > 0:
    solver.Add(solver.Sum([x[end, j] for j in nodes if c[end][j] > 0]) == 0)

  # objective: maximize total cost
  objective = solver.Maximize(total, 1)

  #
  # solution and search
  #
  db = solver.Phase(x_flat, solver.INT_VAR_DEFAULT, solver.ASSIGN_MAX_VALUE)

  solver.NewSearch(db, [objective])
  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    print('total:', total.Value())
    print('in_flow:', [in_flow[i].Value() for i in nodes])
    print('out_flow:', [out_flow[i].Value() for i in nodes])
    for i in nodes:
      for j in nodes:
        print('%2i' % x[i, j].Value(), end=' ')
      print()
    print()

  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('WallTime:', solver.WallTime(), 'ms')


if __name__ == '__main__':
  main()
