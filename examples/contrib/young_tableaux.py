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
from ortools.constraint_solver import pywrapcp


def main(n=5):

  # Create the solver.
  solver = pywrapcp.Solver("Problem")

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
      x[(i, j)] = solver.IntVar(1, n + 1, "x(%i,%i)" % (i, j))

  x_flat = [x[(i, j)] for i in range(n) for j in range(n)]

  # partition structure
  p = [solver.IntVar(0, n + 1, "p%i" % i) for i in range(n)]

  #
  # constraints
  #

  # 1..n is used exactly once
  for i in range(1, n + 1):
    solver.Add(solver.Count(x_flat, i, 1))

  solver.Add(x[(0, 0)] == 1)

  # row wise
  for i in range(n):
    for j in range(1, n):
      solver.Add(x[(i, j)] >= x[(i, j - 1)])

  # column wise
  for j in range(n):
    for i in range(1, n):
      solver.Add(x[(i, j)] >= x[(i - 1, j)])

  # calculate the structure (the partition)
  for i in range(n):
    # MiniZinc/Zinc version:
    # p[i] == sum(j in 1..n) (bool2int(x[i,j] <= n))

    b = [solver.IsLessOrEqualCstVar(x[(i, j)], n) for j in range(n)]
    solver.Add(p[i] == solver.Sum(b))

  solver.Add(solver.Sum(p) == n)

  for i in range(1, n):
    solver.Add(p[i - 1] >= p[i])

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(x_flat)
  solution.Add(p)

  # db: DecisionBuilder
  db = solver.Phase(x_flat + p, solver.CHOOSE_FIRST_UNBOUND,
                    solver.ASSIGN_MIN_VALUE)

  solver.NewSearch(db)
  num_solutions = 0
  while solver.NextSolution():
    print("p:", [p[i].Value() for i in range(n)])
    print("x:")
    for i in range(n):
      for j in range(n):
        val = x_flat[i * n + j].Value()
        if val <= n:
          print(val, end=" ")
      if p[i].Value() > 0:
        print()
    print()
    num_solutions += 1

  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


n = 5
if __name__ == "__main__":
  if len(sys.argv) > 1:
    n = int(sys.argv[1])

  main(n)
