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

  Sicherman Dice in Google CP Solver.

  From http://en.wikipedia.org/wiki/Sicherman_dice
  ""
  Sicherman dice are the only pair of 6-sided dice which are not normal dice,
  bear only positive integers, and have the same probability distribution for
  the sum as normal dice.

  The faces on the dice are numbered 1, 2, 2, 3, 3, 4 and 1, 3, 4, 5, 6, 8.
  ""

  I read about this problem in a book/column by Martin Gardner long
  time ago, and got inspired to model it now by the WolframBlog post
  "Sicherman Dice": http://blog.wolfram.com/2010/07/13/sicherman-dice/

  This model gets the two different ways, first the standard way and
  then the Sicherman dice:

  x1 = [1, 2, 3, 4, 5, 6]
  x2 = [1, 2, 3, 4, 5, 6]
  ----------
  x1 = [1, 2, 2, 3, 3, 4]
  x2 = [1, 3, 4, 5, 6, 8]


  Extra: If we also allow 0 (zero) as a valid value then the
  following two solutions are also valid:

  x1 = [0, 1, 1, 2, 2, 3]
  x2 = [2, 4, 5, 6, 7, 9]
  ----------
  x1 = [0, 1, 2, 3, 4, 5]
  x2 = [2, 3, 4, 5, 6, 7]

  These two extra cases are mentioned here:
  http://mathworld.wolfram.com/SichermanDice.html

  Compare with these models:
  * MiniZinc: http://hakank.org/minizinc/sicherman_dice.mzn
  * Gecode: http://hakank.org/gecode/sicherman_dice.cpp

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver import pywrapcp


def main():

  # Create the solver.
  solver = pywrapcp.Solver("Sicherman dice")

  #
  # data
  #
  n = 6
  m = 10

  # standard distribution
  standard_dist = [1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1]

  #
  # declare variables
  #

  # the two dice
  x1 = [solver.IntVar(0, m, "x1(%i)" % i) for i in range(n)]
  x2 = [solver.IntVar(0, m, "x2(%i)" % i) for i in range(n)]

  #
  # constraints
  #
  # [solver.Add(standard_dist[k] == solver.Sum([x1[i] + x2[j] == k+2 for i in range(n) for j in range(n)]))
  # for k in range(len(standard_dist))]
  for k in range(len(standard_dist)):
    tmp = [solver.BoolVar() for i in range(n) for j in range(n)]
    for i in range(n):
      for j in range(n):
        solver.Add(tmp[i * n + j] == solver.IsEqualCstVar(x1[i] + x2[j], k + 2))
    solver.Add(standard_dist[k] == solver.Sum(tmp))

  # symmetry breaking
  [solver.Add(x1[i] <= x1[i + 1]) for i in range(n - 1)],
  [solver.Add(x2[i] <= x2[i + 1]) for i in range(n - 1)],
  [solver.Add(x1[i] <= x2[i]) for i in range(n - 1)],

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(x1)
  solution.Add(x2)

  # db: DecisionBuilder
  db = solver.Phase(x1 + x2, solver.INT_VAR_SIMPLE, solver.ASSIGN_MIN_VALUE)

  solver.NewSearch(db)
  num_solutions = 0
  while solver.NextSolution():
    print("x1:", [x1[i].Value() for i in range(n)])
    print("x2:", [x2[i].Value() for i in range(n)])
    print()

    num_solutions += 1
  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions, "solver.solutions:",
        solver.Solutions())
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())
  print("MemoryUsage:", solver.MemoryUsage())
  print("SearchDepth:", solver.SearchDepth())
  print("SolveDepth:", solver.SolveDepth())
  print("stamp:", solver.Stamp())
  print("solver", solver)


if __name__ == "__main__":
  main()
