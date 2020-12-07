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

  xkcd problem (Knapsack)  in Google CP Solver.

  http://xkcd.com/287/

  Some amount (or none) of each dish should be ordered to give a total
  of exact 15.05


  Compare with the following models:
  * Comet: http://www.hakank.org/comet/xkcd.co
  * ECLiPSE: http://www.hakank.org/eclipse/xkcd.ecl
  * Gecode: http://www.hakank.org/gecode/xkcd.cpp
  * Gecode/R: http://www.hakank.org/gecode_r/xkcd.rb
  * MiniZinc: http://www.hakank.org/minizinc/xkcd.mzn
  * Tailor: http://www.hakank.org/minizinc/xkcd.mzn
  * SICtus: http://www.hakank.org/sicstus/xkcd.pl
  * Zinc: http://www.hakank.org/minizinc/xkcd.zinc


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_cp_solver/
"""
from ortools.constraint_solver import pywrapcp


def main():

  # Create the solver.
  solver = pywrapcp.Solver("xkcd knapsack")

  #
  # data
  #
  num_prices = 6
  # for price and total: multiplied by 100 to be able to use integers
  price = [215, 275, 335, 355, 420, 580]
  total = 1505

  products = [
      "mixed fruit", "french fries", "side salad", "host wings",
      "mozzarella sticks", "samples place"
  ]

  # declare variables

  # how many items of each dish
  x = [solver.IntVar(0, 10, "x%i" % i) for i in range(num_prices)]
  z = solver.IntVar(0, 1505, "z")

  #
  # constraints
  #
  solver.Add(z == solver.Sum([x[i] * price[i] for i in range(num_prices)]))
  solver.Add(z == total)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add([x[i] for i in range(num_prices)])
  solution.Add(z)

  collector = solver.AllSolutionCollector(solution)
  # collector = solver.FirstSolutionCollector(solution)
  # search_log = solver.SearchLog(100, x[0])
  solver.Solve(
      solver.Phase([x[i] for i in range(num_prices)], solver.INT_VAR_SIMPLE,
                   solver.ASSIGN_MIN_VALUE), [collector])

  num_solutions = collector.SolutionCount()
  print("num_solutions: ", num_solutions)
  if num_solutions > 0:
    for s in range(num_solutions):
      print("z:", collector.Value(s, z) / 100.0)
      xval = [collector.Value(s, x[i]) for i in range(num_prices)]
      print("x:", xval)
      for i in range(num_prices):
        if xval[i] > 0:
          print(xval[i], "of", products[i], ":", price[i] / 100.0)
      print()

    print()
    print("num_solutions:", num_solutions)
    print("failures:", solver.Failures())
    print("branches:", solver.Branches())
    print("WallTime:", solver.WallTime())

  else:
    print("No solutions found")


if __name__ == "__main__":
  main()
