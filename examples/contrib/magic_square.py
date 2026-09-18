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

  Magic squares in Google CP Solver.

  Magic square problem.

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver.python import constraint_solver as cp


def main(n, limit):
  # Create the solver.
  solver = cp.Solver("n-queens")
  if not solver:
    return

  #
  # data
  #

  #
  # declare variables
  #
  x = {}
  for i in range(n):
    for j in range(n):
      x[(i, j)] = solver.new_int_var(1, n * n, "x(%i,%i)" % (i, j))
  x_flat = [x[(i, j)] for i in range(n) for j in range(n)]

  # the sum
  # s = ( n * (n*n + 1)) / 2
  s = solver.new_int_var(1, n * n * n, "s")

  #
  # constraints
  #
  # solver.add(s == ( n * (n*n + 1)) / 2)

  solver.add_all_different(x_flat)

  [solver.add(solver.sum([x[(i, j)] for j in range(n)]) == s) for i in range(n)]
  [solver.add(solver.sum([x[(i, j)] for i in range(n)]) == s) for j in range(n)]

  solver.add(solver.sum([x[(i, i)] for i in range(n)]) == s)  # diag 1
  solver.add(solver.sum([x[(i, n - i - 1)] for i in range(n)]) == s)  # diag 2

  # symmetry breaking
  # solver.add(x[(0,0)] == 1)

  #
  # solution and search
  #
  solution = solver.assignment()
  solution.add(x_flat)
  solution.add(s)

  # db: DecisionBuilder
  db = solver.phase(
      x_flat,
      # cp.IntVarStrategy.INT_VAR_DEFAULT,
      cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
      # cp.IntVarStrategy.CHOOSE_MIN_SIZE_LOWEST_MAX,

      # cp.IntValueStrategy.ASSIGN_MIN_VALUE
      cp.IntValueStrategy.ASSIGN_CENTER_VALUE)

  solver.new_search(db)
  num_solutions = 0
  while solver.next_solution():
    print("s:", s.value())
    for i in range(n):
      for j in range(n):
        print("%2i" % x[(i, j)].value(), end=" ")
      print()

    print()
    num_solutions += 1
    if num_solutions > limit:
      break
  solver.end_search()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.num_failures)
  print("branches:", solver.num_branches)
  print("WallTime:", solver.wall_time_ms)


n = 4
limit=100
if __name__ == "__main__":
  if len(sys.argv) > 1:
    n = int(sys.argv[1])
  if len(sys.argv) > 2:
    limit = int(sys.argv[2])

  main(n, limit)
