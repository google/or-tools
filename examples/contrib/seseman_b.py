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

  Seseman Convent problem in Google CP Solver.


  n is the length of a border
  There are (n-2)^2 "holes", i.e.
  there are n^2 - (n-2)^2 variables to find out.

  The simplest problem, n = 3 (n x n matrix)
  which is represented by the following matrix:

   a b c
   d   e
   f g h

  Where the following constraints must hold:

    a + b + c = border_sum
    a + d + f = border_sum
    c + e + h = border_sum
    f + g + h = border_sum
    a + b + c + d + e + f = total_sum


  Compare with the following models:
  * Tailor/Essence': http://hakank.org/tailor/seseman.eprime
  * MiniZinc: http://hakank.org/minizinc/seseman.mzn
  * SICStus: http://hakank.org/sicstus/seseman.pl
  * Zinc: http://hakank.org/minizinc/seseman.zinc
  * Choco: http://hakank.org/choco/Seseman.java
  * Comet: http://hakank.org/comet/seseman.co
  * ECLiPSe: http://hakank.org/eclipse/seseman.ecl
  * Gecode: http://hakank.org/gecode/seseman.cpp
  * Gecode/R: http://hakank.org/gecode_r/seseman.rb
  * JaCoP: http://hakank.org/JaCoP/Seseman.java

  This version use a better way of looping through all solutions.


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
from ortools.constraint_solver import pywrapcp


def main(unused_argv):
  # Create the solver.
  solver = pywrapcp.Solver("Seseman Convent problem")

  # data
  n = 3
  border_sum = n * n

  # declare variables
  total_sum = solver.IntVar(1, n * n * n * n, "total_sum")
  # x[0..n-1,0..n-1]
  x = {}
  for i in range(n):
    for j in range(n):
      x[(i, j)] = solver.IntVar(0, n * n, "x %i %i" % (i, j))

  #
  # constraints
  #
  # zero all middle cells
  for i in range(1, n - 1):
    for j in range(1, n - 1):
      solver.Add(x[(i, j)] == 0)

  # all borders must be >= 1
  for i in range(n):
    for j in range(n):
      if i == 0 or j == 0 or i == n - 1 or j == n - 1:
        solver.Add(x[(i, j)] >= 1)

  # sum the borders (border_sum)
  solver.Add(solver.Sum([x[(i, 0)] for i in range(n)]) == border_sum)
  solver.Add(solver.Sum([x[(i, n - 1)] for i in range(n)]) == border_sum)
  solver.Add(solver.Sum([x[(0, i)] for i in range(n)]) == border_sum)
  solver.Add(solver.Sum([x[(n - 1, i)] for i in range(n)]) == border_sum)

  # total
  solver.Add(
      solver.Sum([x[(i, j)] for i in range(n) for j in range(n)]) == total_sum)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add([x[(i, j)] for i in range(n) for j in range(n)])
  solution.Add(total_sum)

  db = solver.Phase([x[(i, j)] for i in range(n) for j in range(n)],
                    solver.CHOOSE_PATH, solver.ASSIGN_MIN_VALUE)

  solver.NewSearch(db)

  num_solutions = 0

  while solver.NextSolution():
    num_solutions += 1
    print("total_sum:", total_sum.Value())
    for i in range(n):
      for j in range(n):
        print(x[(i, j)].Value(), end=" ")
      print()
    print()

  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


if __name__ == "__main__":
  main("cp sample")
