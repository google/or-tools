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


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
from ortools.sat.python import cp_model


def main(unused_argv):
  # Create the solver.
  model = cp_model.CpModel()

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
  x = [model.NewIntVar(0, 100, "x%d" % i) for i in range(n)]
  cost = model.NewIntVar(0, 10000, "cost")

  #
  # constraints
  #
  model.Add(sum(x[i] * calories[i] for i in range(n)) >= limits[0])
  model.Add(sum(x[i] * chocolate[i] for i in range(n)) >= limits[1])
  model.Add(sum(x[i] * sugar[i] for i in range(n)) >= limits[2])
  model.Add(sum(x[i] * fat[i] for i in range(n)) >= limits[3])

  # objective
  model.Minimize(cost)

  # Solve model.
  solver = cp_model.CpSolver()
  status = solver.Solve(model)

  # Output solution.
  if status == cp_model.OPTIMAL:
    print("cost:", solver.ObjectiveValue())
    print([("abcdefghij" [i], solver.Value(x[i])) for i in range(n)])
    print()
    print('  - status          : %s' % solver.StatusName(status))
    print('  - conflicts       : %i' % solver.NumConflicts())
    print('  - branches        : %i' % solver.NumBranches())
    print('  - wall time       : %f ms' % solver.WallTime())
    print()


if __name__ == "__main__":
  main("cp sample")
