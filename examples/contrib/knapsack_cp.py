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

  Knapsack problem in Google CP Solver.

  Simple knapsack problem.

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
from ortools.constraint_solver import pywrapcp


def knapsack(solver, values, weights, n):
  z = solver.IntVar(0, 10000)
  x = [solver.IntVar(0, 1, "x(%i)" % i) for i in range(len(values))]
  solver.Add(z >= 0)
  solver.Add(z == solver.ScalProd(x, values))
  solver.Add(solver.ScalProd(x, weights) <= n)

  return [x, z]


def main(values, weights, n):
  # Create the solver.
  solver = pywrapcp.Solver("knapsack_cp")

  #
  # data
  #
  print("values:", values)
  print("weights:", weights)
  print("n:", n)
  print()

  # declare variables

  #
  # constraints
  #
  [x, z] = knapsack(solver, values, weights, n)

  # objective
  objective = solver.Maximize(z, 1)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(x)
  solution.Add(z)

  # db: DecisionBuilder
  db = solver.Phase(x, solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MAX_VALUE)

  solver.NewSearch(db, [objective])
  num_solutions = 0
  while solver.NextSolution():
    print("x:", [x[i].Value() for i in range(len(values))])
    print("z:", z.Value())
    print()
    num_solutions += 1
  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


values = [15, 100, 90, 60, 40, 15, 10, 1, 12, 12, 100]
weights = [2, 20, 20, 30, 40, 30, 60, 10, 21, 12, 2]
n = 102

if __name__ == "__main__":
  main(values, weights, n)
