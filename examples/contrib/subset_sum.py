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

  Subset sum problem in Google CP Solver.

  From Katta G. Murty: 'Optimization Models for Decision Making', page 340
  http://ioe.engin.umich.edu/people/fac/books/murty/opti_model/junior-7.pdf
  '''
  Example 7.8.1

  A bank van had several bags of coins, each containing either
  16, 17, 23, 24, 39, or 40 coins. While the van was parked on the
  street, thieves stole some bags. A total of 100 coins were lost.
  It is required to find how many bags were stolen.
  '''

  Compare with the following models:
  * Comet: http://www.hakank.org/comet/subset_sum.co
  * ECLiPSE: http://www.hakank.org/eclipse/subset_sum.ecl
  * Gecode: http://www.hakank.org/gecode/subset_sum.cpp
  * MiniZinc: http://www.hakank.org/minizinc/subset_sum.mzn
  * Tailor/Essence': http://www.hakank.org/tailor/subset_sum.py
  * SICStus: http://hakank.org/sicstus/subset_sum.pl

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver import pywrapcp


def subset_sum(solver, values, total):
  n = len(values)
  x = [solver.IntVar(0, n) for i in range(n)]
  ss = solver.IntVar(0, n)

  solver.Add(ss == solver.Sum(x))
  solver.Add(total == solver.ScalProd(x, values))

  return x, ss


def main(coins, total):

  # Create the solver.
  solver = pywrapcp.Solver("n-queens")

  #
  # data
  #
  print("coins:", coins)
  print("total:", total)
  print()

  #
  # declare variables
  #

  #
  # constraints
  #
  x, ss = subset_sum(solver, coins, total)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(x)
  solution.Add(ss)

  # db: DecisionBuilder
  db = solver.Phase(x, solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE)

  solver.NewSearch(db)
  num_solutions = 0
  while solver.NextSolution():
    print("ss:", ss.Value())
    print("x: ", [x[i].Value() for i in range(len(x))])
    print()
    num_solutions += 1
  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


coins = [16, 17, 23, 24, 39, 40]
total = 100
if __name__ == "__main__":
  if len(sys.argv) > 1:
    total = int(sys.argv[1])
  main(coins, total)
