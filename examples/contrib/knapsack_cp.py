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
from ortools.constraint_solver.python import constraint_solver as cp


def knapsack(solver, values, weights, n):
  z = solver.new_int_var(0, 10000)
  x = [solver.new_int_var(0, 1, "x(%i)" % i) for i in range(len(values))]
  solver.add(z >= 0)
  solver.add(z == solver.weighted_sum(x, values))
  solver.add(solver.weighted_sum(x, weights) <= n)

  return [x, z]


def main(values, weights, n):
  # Create the solver.
  solver = cp.Solver("knapsack_cp")

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
  objective = solver.maximize(z, 1)

  #
  # solution and search
  #
  solution = solver.assignment()
  solution.add(x)
  solution.add(z)

  # db: DecisionBuilder
  db = solver.phase(x, cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND, cp.IntValueStrategy.ASSIGN_MAX_VALUE)

  solver.new_search(db, [objective])
  num_solutions = 0
  while solver.next_solution():
    print("x:", [x[i].value() for i in range(len(values))])
    print("z:", z.value())
    print()
    num_solutions += 1
  solver.end_search()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.num_failures)
  print("branches:", solver.num_branches)
  print("WallTime:", solver.wall_time_ms)


values = [15, 100, 90, 60, 40, 15, 10, 1, 12, 12, 100]
weights = [2, 20, 20, 30, 40, 30, 60, 10, 21, 12, 2]
n = 102

if __name__ == "__main__":
  main(values, weights, n)
