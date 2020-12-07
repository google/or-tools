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

  Coin application in Google CP Solver.

  From 'Constraint Logic Programming using ECLiPSe'
  pages 99f and 234 ff.
  The solution in ECLiPSe is at page 236.

  '''
  What is the minimum number of coins that allows one to pay _exactly_
  any amount smaller than one Euro? Recall that there are six different
  euro cents, of denomination 1, 2, 5, 10, 20, 50
  '''

  Compare with the following models:
  * MiniZinc: http://hakank.org/minizinc/coins3.mzn
  * Comet   : http://www.hakank.org/comet/coins3.co
  * Gecode  : http://hakank.org/gecode/coins3.cpp
  * SICStus : http://hakank.org/sicstus/coins3.pl


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""


import sys
from ortools.constraint_solver import pywrapcp


def main():
  # Create the solver.
  solver = pywrapcp.Solver("Coins")

  #
  # data
  #
  n = 6  # number of different coins
  variables = [1, 2, 5, 10, 25, 50]

  # declare variables
  x = [solver.IntVar(0, 99, "x%i" % i) for i in range(n)]
  num_coins = solver.IntVar(0, 99, "num_coins")

  #
  # constraints
  #

  # number of used coins, to be minimized
  solver.Add(num_coins == solver.Sum(x))

  # Check that all changes from 1 to 99 can be made.
  for j in range(1, 100):
    tmp = [solver.IntVar(0, 99, "b%i" % i) for i in range(n)]
    solver.Add(solver.ScalProd(tmp, variables) == j)
    [solver.Add(tmp[i] <= x[i]) for i in range(n)]

  # objective
  objective = solver.Minimize(num_coins, 1)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(x)
  solution.Add(num_coins)
  solution.AddObjective(num_coins)

  db = solver.Phase(x, solver.CHOOSE_MIN_SIZE_LOWEST_MAX,
                    solver.ASSIGN_MIN_VALUE)

  solver.NewSearch(db, [objective])
  num_solutions = 0
  while solver.NextSolution():
    print("x: ", [x[i].Value() for i in range(n)])
    print("num_coins:", num_coins.Value())
    print()
    num_solutions += 1
  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


if __name__ == "__main__":
  main()
