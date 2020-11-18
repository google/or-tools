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

  P-median problem in Google CP Solver.

  Model and data from the OPL Manual, which describes the problem:
  '''
  The P-Median problem is a well known problem in Operations Research.
  The problem can be stated very simply, like this: given a set of customers
  with known amounts of demand, a set of candidate locations for warehouses,
  and the distance between each pair of customer-warehouse, choose P
  warehouses to open that minimize the demand-weighted distance of serving
  all customers from those P warehouses.
  '''

  Compare with the following models:
  * MiniZinc: http://hakank.org/minizinc/p_median.mzn
  * Comet: http://hakank.org/comet/p_median.co

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver import pywrapcp


def main():

  # Create the solver.
  solver = pywrapcp.Solver('P-median problem')

  #
  # data
  #
  p = 2

  num_customers = 4
  customers = list(range(num_customers))
  Albert, Bob, Chris, Daniel = customers
  num_warehouses = 3
  warehouses = list(range(num_warehouses))
  Santa_Clara, San_Jose, Berkeley = warehouses

  demand = [100, 80, 80, 70]
  distance = [[2, 10, 50], [2, 10, 52], [50, 60, 3], [40, 60, 1]]

  #
  # declare variables
  #
  open = [solver.IntVar(warehouses, 'open[%i]% % i') for w in warehouses]
  ship = {}
  for c in customers:
    for w in warehouses:
      ship[c, w] = solver.IntVar(0, 1, 'ship[%i,%i]' % (c, w))
  ship_flat = [ship[c, w] for c in customers for w in warehouses]

  z = solver.IntVar(0, 1000, 'z')

  #
  # constraints
  #
  z_sum = solver.Sum([
      demand[c] * distance[c][w] * ship[c, w]
      for c in customers
      for w in warehouses
  ])
  solver.Add(z == z_sum)

  for c in customers:
    s = solver.Sum([ship[c, w] for w in warehouses])
    solver.Add(s == 1)

  solver.Add(solver.Sum(open) == p)

  for c in customers:
    for w in warehouses:
      solver.Add(ship[c, w] <= open[w])

  # objective
  objective = solver.Minimize(z, 1)

  #
  # solution and search
  #
  db = solver.Phase(open + ship_flat, solver.INT_VAR_DEFAULT,
                    solver.INT_VALUE_DEFAULT)

  solver.NewSearch(db, [objective])

  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    print('z:', z.Value())
    print('open:', [open[w].Value() for w in warehouses])
    for c in customers:
      for w in warehouses:
        print(ship[c, w].Value(), end=' ')
      print()
    print()

  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('WallTime:', solver.WallTime(), 'ms')


if __name__ == '__main__':
  main()
