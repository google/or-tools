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

  Grocery problem in Google CP Solver.

  From  Christian Schulte, Gert Smolka, Finite Domain
  http://www.mozart-oz.org/documentation/fdt/
  Constraint Programming in Oz. A Tutorial. 2001.
  '''
  A kid goes into a grocery store and buys four items. The cashier
  charges $7.11, the kid pays and is about to leave when the cashier
  calls the kid back, and says 'Hold on, I multiplied the four items
  instead of adding them; I'll try again; Hah, with adding them the
  price still comes to $7.11'. What were the prices of the four items?
  '''

  Compare with the following models:
  * MiniZinc: http://hakank.org/minizinc/grocery.mzn
  * Comet: http://hakank.org/comet/grocery.co
  * Zinc: http://hakank.org/minizinc/grocery.zinc

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys

from ortools.constraint_solver import pywrapcp
from functools import reduce


def main():

  # Create the solver.
  solver = pywrapcp.Solver("Grocery")

  #
  # data
  #
  n = 4
  c = 711

  #
  # declare variables
  #
  item = [solver.IntVar(0, c, "item[%i]" % i) for i in range(n)]

  #
  # constraints
  #
  solver.Add(solver.Sum(item) == c)
  solver.Add(reduce(lambda x, y: x * y, item) == c * 100**3)

  # symmetry breaking
  for i in range(1, n):
    solver.Add(item[i - 1] < item[i])

  #
  # search and result
  #
  db = solver.Phase(item, solver.INT_VAR_SIMPLE, solver.INT_VALUE_SIMPLE)

  solver.NewSearch(db)
  num_solutions = 0
  while solver.NextSolution():
    print("item:", [item[i].Value() for i in range(n)])
    print()
    num_solutions += 1

  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


if __name__ == "__main__":
  main()
