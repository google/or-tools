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

  Young tableaux in Google CP Solver.

  See
  http://mathworld.wolfram.com/YoungTableau.html
  and
  http://en.wikipedia.org/wiki/Young_tableau
  '''
  The partitions of 4 are
  {4}, {3,1}, {2,2}, {2,1,1}, {1,1,1,1}

  And the corresponding standard Young tableaux are:

1.   1 2 3 4

2.   1 2 3         1 2 4    1 3 4
         4             3        2

3.   1 2           1 3
     3 4           2 4

4    1 2           1 3      1 4
     3             2        2
     4             4        3

5.   1
     2
     3
     4
   '''

   Thanks to Laurent Perron for improving this model.

   Compare with the following models:
   * MiniZinc: http://www.hakank.org/minizinc/young_tableaux.mzn
   * Choco   : http://www.hakank.org/choco/YoungTableuax.java
   * JaCoP   : http://www.hakank.org/JaCoP/YoungTableuax.java
   * Comet   : http://www.hakank.org/comet/young_tableaux.co
   * Gecode  : http://www.hakank.org/gecode/young_tableaux.cpp
   * ECLiPSe : http://www.hakank.org/eclipse/young_tableaux.ecl
   * Tailor/Essence' : http://www.hakank.org/tailor/young_tableaux.eprime
   * SICStus: http://hakank.org/sicstus/young_tableaux.pl
   * Zinc: http://hakank.org/minizinc/young_tableaux.zinc

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver.python import constraint_solver as cp


def main(n=5):

  # Create the solver.
  solver = cp.Solver("Problem")

  #
  # data
  #
  print("n:", n)

  #
  # declare variables
  #
  x = {}
  for i in range(n):
    for j in range(n):
      x[(i, j)] = solver.new_int_var(1, n + 1, "x(%i,%i)" % (i, j))

  x_flat = [x[(i, j)] for i in range(n) for j in range(n)]

  # partition structure
  p = [solver.new_int_var(0, n + 1, "p%i" % i) for i in range(n)]

  #
  # constraints
  #

  # 1..n is used exactly once
  for i in range(1, n + 1):
    solver.add_count(x_flat, i, 1)

  solver.add(x[(0, 0)] == 1)

  # row wise
  for i in range(n):
    for j in range(1, n):
      solver.add(x[(i, j)] >= x[(i, j - 1)])

  # column wise
  for j in range(n):
    for i in range(1, n):
      solver.add(x[(i, j)] >= x[(i - 1, j)])

  # calculate the structure (the partition)
  for i in range(n):
    # MiniZinc/Zinc version:
    # p[i] == sum(j in 1..n) (bool2int(x[i,j] <= n))

    b = [solver.add_is_less_or_equal_cst_var(x[(i, j)], n) for j in range(n)]
    solver.add(p[i] == solver.sum(b))

  solver.add(solver.sum(p) == n)

  for i in range(1, n):
    solver.add(p[i - 1] >= p[i])

  #
  # solution and search
  #
  solution = solver.assignment()
  solution.add(x_flat)
  solution.add(p)

  # db: DecisionBuilder
  db = solver.phase(x_flat + p, cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
                    cp.IntValueStrategy.ASSIGN_MIN_VALUE)

  solver.new_search(db)
  num_solutions = 0
  while solver.next_solution():
    print("p:", [p[i].value() for i in range(n)])
    print("x:")
    for i in range(n):
      for j in range(n):
        val = x_flat[i * n + j].value()
        if val <= n:
          print(val, end=" ")
      if p[i].value() > 0:
        print()
    print()
    num_solutions += 1

  solver.end_search()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.num_failures)
  print("branches:", solver.num_branches)
  print("WallTime:", solver.wall_time_ms)


n = 5
if __name__ == "__main__":
  if len(sys.argv) > 1:
    n = int(sys.argv[1])

  main(n)
