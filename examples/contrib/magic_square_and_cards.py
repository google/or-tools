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

  Magic squares and cards problem in Google CP Solver.

  Martin Gardner (July 1971)
  '''
  Allowing duplicates values, what is the largest constant sum for an order-3
  magic square that can be formed with nine cards from the deck.
  '''



  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver import pywrapcp


def main(n=3):
  # Create the solver.
  solver = pywrapcp.Solver("n-queens")

  #
  # data
  #
  # n = 3

  #
  # declare variables
  #
  x = {}
  for i in range(n):
    for j in range(n):
      x[(i, j)] = solver.IntVar(1, 13, "x(%i,%i)" % (i, j))
  x_flat = [x[(i, j)] for i in range(n) for j in range(n)]

  s = solver.IntVar(1, 13 * 4, "s")
  counts = [solver.IntVar(0, 4, "counts(%i)" % i) for i in range(14)]

  #
  # constraints
  #
  solver.Add(solver.Distribute(x_flat, list(range(14)), counts))

  # the standard magic square constraints (sans all_different)
  [solver.Add(solver.Sum([x[(i, j)] for j in range(n)]) == s) for i in range(n)]
  [solver.Add(solver.Sum([x[(i, j)] for i in range(n)]) == s) for j in range(n)]

  solver.Add(solver.Sum([x[(i, i)] for i in range(n)]) == s)  # diag 1
  solver.Add(solver.Sum([x[(i, n - i - 1)] for i in range(n)]) == s)  # diag 2

  # redundant constraint
  solver.Add(solver.Sum(counts) == n * n)

  # objective
  objective = solver.Maximize(s, 1)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(x_flat)
  solution.Add(s)
  solution.Add(counts)

  # db: DecisionBuilder
  db = solver.Phase(x_flat, solver.CHOOSE_FIRST_UNBOUND,
                    solver.ASSIGN_MAX_VALUE)

  solver.NewSearch(db, [objective])
  num_solutions = 0
  while solver.NextSolution():
    print("s:", s.Value())
    print("counts:", [counts[i].Value() for i in range(14)])
    for i in range(n):
      for j in range(n):
        print(x[(i, j)].Value(), end=" ")
      print()

    print()
    num_solutions += 1
  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


n = 3
if __name__ == "__main__":
  if len(sys.argv) > 1:
    n = int(sys.argv[1])
  main(n)
