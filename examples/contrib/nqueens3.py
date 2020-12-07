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

  Faster than the previous versions:
  - http://www.hakank.org/gogle_cp_solver/nqueens.py
  - http://www.hakank.org/gogle_cp_solver/nqueens2.py


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver import pywrapcp


def main(n=8, num_sol=0, print_sol=1):
  # Create the solver.
  solver = pywrapcp.Solver("n-queens")

  #
  # data
  #
  print("n:", n)
  print("num_sol:", num_sol)
  print("print_sol:", print_sol)

  # declare variables
  q = [solver.IntVar(0, n - 1, "x%i" % i) for i in range(n)]

  #
  # constraints
  #
  solver.Add(solver.AllDifferent(q))
  solver.Add(solver.AllDifferent([q[i] + i for i in range(n)]))
  solver.Add(solver.AllDifferent([q[i] - i for i in range(n)]))

  # symmetry breaking
  # solver.Add(q[0] == 0)

  #
  # search
  #

  db = solver.Phase(q, solver.CHOOSE_MIN_SIZE_LOWEST_MAX,
                    solver.ASSIGN_CENTER_VALUE)

  solver.NewSearch(db)
  num_solutions = 0
  while solver.NextSolution():
    if print_sol:
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
    if num_sol > 0 and num_solutions >= num_sol:
      break

  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime(), "ms")


n = 8
num_sol = 0
print_sol = 1
if __name__ == "__main__":
  if len(sys.argv) > 1:
    n = int(sys.argv[1])
  if len(sys.argv) > 2:
    num_sol = int(sys.argv[2])
  if len(sys.argv) > 3:
    print_sol = int(sys.argv[3])

  main(n, num_sol, print_sol)

  # print_sol = False
  # show_all = False
  # for n in range(1000,1001):
  #     print
  #     main(n, num_sol, print_sol)
