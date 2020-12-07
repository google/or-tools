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

  Simple diet problem in Google CP Solver.

  Standard Operations Research example in Minizinc


  Minimize the cost for the products:
  Type of                        Calories   Chocolate    Sugar    Fat
  Food                                      (ounces)     (ounces) (ounces)
  Chocolate Cake (1 slice)       400           3            2      2
  Chocolate ice cream (1 scoop)  200           2            2      4
  Cola (1 bottle)                150           0            4      1
  Pineapple cheesecake (1 piece) 500           0            4      5

  Compare with the following models:
  * Tailor/Essence': http://hakank.org/tailor/diet1.eprime
  * MiniZinc: http://hakank.org/minizinc/diet1.mzn
  * SICStus: http://hakank.org/sicstus/diet1.pl
  * Zinc: http://hakank.org/minizinc/diet1.zinc
  * Choco: http://hakank.org/choco/Diet.java
  * Comet: http://hakank.org/comet/diet.co
  * ECLiPSe: http://hakank.org/eclipse/diet.ecl
  * Gecode: http://hakank.org/gecode/diet.cpp
  * Gecode/R: http://hakank.org/gecode_r/diet.rb
  * JaCoP: http://hakank.org/JaCoP/Diet.java

  This version use ScalProd() instead of Sum().


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
from ortools.constraint_solver import pywrapcp


def main(unused_argv):
  # Create the solver.
  solver = pywrapcp.Solver("Diet")

  #
  # data
  #
  n = 4
  price = [50, 20, 30, 80]  # in cents
  limits = [500, 6, 10, 8]  # requirements for each nutrition type

  # nutritions for each product
  calories = [400, 200, 150, 500]
  chocolate = [3, 2, 0, 0]
  sugar = [2, 2, 4, 4]
  fat = [2, 4, 1, 5]

  #
  # declare variables
  #
  x = [solver.IntVar(0, 100, "x%d" % i) for i in range(n)]
  cost = solver.IntVar(0, 10000, "cost")

  #
  # constraints
  #
  solver.Add(solver.ScalProd(x, calories) >= limits[0])
  solver.Add(solver.ScalProd(x, chocolate) >= limits[1])
  solver.Add(solver.ScalProd(x, sugar) >= limits[2])
  solver.Add(solver.ScalProd(x, fat) >= limits[3])

  # objective
  objective = solver.Minimize(cost, 1)

  #
  # solution
  #
  solution = solver.Assignment()
  solution.AddObjective(cost)
  solution.Add(x)

  # last solution since it's a minimization problem
  collector = solver.LastSolutionCollector(solution)
  search_log = solver.SearchLog(100, cost)
  solver.Solve(
      solver.Phase(x + [cost], solver.INT_VAR_SIMPLE, solver.ASSIGN_MIN_VALUE),
      [objective, search_log, collector])

  # get the first (and only) solution
  print("cost:", collector.ObjectiveValue(0))
  print([("abcdefghij" [i], collector.Value(0, x[i])) for i in range(n)])
  print()
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())
  print()


if __name__ == "__main__":
  main("cp sample")
