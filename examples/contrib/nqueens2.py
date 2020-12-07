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

  n-queens problem in Google CP Solver.

  N queens problem.

  This version use NewSearch()/NextSolution() for looping through
  the solutions.

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver import pywrapcp


def main(n=8):
  # Create the solver.
  solver = pywrapcp.Solver("n-queens")

  #
  # data
  #
  # n = 8 # size of board (n x n)

  # declare variables
  q = [solver.IntVar(0, n - 1, "x%i" % i) for i in range(n)]

  #
  # constraints
  #
  solver.Add(solver.AllDifferent(q))
  for i in range(n):
    for j in range(i):
      solver.Add(q[i] != q[j])
      solver.Add(q[i] + i != q[j] + j)
      solver.Add(q[i] - i != q[j] - j)

  # for i in range(n):
  #     for j in range(i):
  #         solver.Add(abs(q[i]-q[j]) != abs(i-j))

  # symmetry breaking
  # solver.Add(q[0] == 0)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add([q[i] for i in range(n)])

  # db: DecisionBuilder
  db = solver.Phase(
      [q[i] for i in range(n)],
      # solver.CHOOSE_FIRST_UNBOUND,
      solver.CHOOSE_MIN_SIZE_LOWEST_MAX,
      solver.ASSIGN_CENTER_VALUE)

  solver.NewSearch(db)
  num_solutions = 0
  while solver.NextSolution():
    qval = [q[i].Value() for i in range(n)]
    print("q:", qval)
    for i in range(n):
      for j in range(n):
        if qval[i] == j:
          print("Q", end=" ")
        else:
          print("_", end=" ")
      print()
    print()
    num_solutions += 1
  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


n = 8
if __name__ == "__main__":
  if len(sys.argv) > 1:
    n = int(sys.argv[1])
  main(n)
