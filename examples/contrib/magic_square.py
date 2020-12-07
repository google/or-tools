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
from ortools.constraint_solver import pywrapcp


def main(n, limit):
  # Create the solver.
  solver = pywrapcp.Solver("n-queens")

  #
  # data
  #

  #
  # declare variables
  #
  x = {}
  for i in range(n):
    for j in range(n):
      x[(i, j)] = solver.IntVar(1, n * n, "x(%i,%i)" % (i, j))
  x_flat = [x[(i, j)] for i in range(n) for j in range(n)]

  # the sum
  # s = ( n * (n*n + 1)) / 2
  s = solver.IntVar(1, n * n * n, "s")

  #
  # constraints
  #
  # solver.Add(s == ( n * (n*n + 1)) / 2)

  solver.Add(solver.AllDifferent(x_flat))

  [solver.Add(solver.Sum([x[(i, j)] for j in range(n)]) == s) for i in range(n)]
  [solver.Add(solver.Sum([x[(i, j)] for i in range(n)]) == s) for j in range(n)]

  solver.Add(solver.Sum([x[(i, i)] for i in range(n)]) == s)  # diag 1
  solver.Add(solver.Sum([x[(i, n - i - 1)] for i in range(n)]) == s)  # diag 2

  # symmetry breaking
  # solver.Add(x[(0,0)] == 1)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(x_flat)
  solution.Add(s)

  # db: DecisionBuilder
  db = solver.Phase(
      x_flat,
      # solver.INT_VAR_DEFAULT,
      solver.CHOOSE_FIRST_UNBOUND,
      # solver.CHOOSE_MIN_SIZE_LOWEST_MAX,

      # solver.ASSIGN_MIN_VALUE
      solver.ASSIGN_CENTER_VALUE)

  solver.NewSearch(db)
  num_solutions = 0
  while solver.NextSolution():
    print("s:", s.Value())
    for i in range(n):
      for j in range(n):
        print("%2i" % x[(i, j)].Value(), end=" ")
      print()

    print()
    num_solutions += 1
    if num_solutions > limit:
      break
  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


n = 4
limit=100
if __name__ == "__main__":
  if len(sys.argv) > 1:
    n = int(sys.argv[1])
  if len(sys.argv) > 2:
    limit = int(sys.argv[2])

  main(n, limit)
