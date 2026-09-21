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
"""Simple diet problem in Google CP Solver.

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

from ortools.constraint_solver.python import constraint_solver as cp


def main(unused_argv):
    # Create the solver.
    solver = cp.Solver("Diet")

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
    x = [solver.new_int_var(0, 100, "x%d" % i) for i in range(n)]
    cost = solver.new_int_var(0, 10000, "cost")

    #
    # constraints
    #
    solver.add(solver.weighted_sum(x, calories) >= limits[0])
    solver.add(solver.weighted_sum(x, chocolate) >= limits[1])
    solver.add(solver.weighted_sum(x, sugar) >= limits[2])
    solver.add(solver.weighted_sum(x, fat) >= limits[3])

    # objective
    objective = solver.minimize(cost, 1)

    #
    # solution
    #
    solution = solver.assignment()
    solution.add_objective(cost)
    solution.add(x)

    # last solution since it's a minimization problem
    collector = solver.last_solution_collector(solution)
    search_log = solver.search_log(100, cost)
    solver.solve(
        solver.phase(
            x + [cost],
            cp.IntVarStrategy.INT_VAR_SIMPLE,
            cp.IntValueStrategy.ASSIGN_MIN_VALUE,
        ),
        [objective, search_log, collector],
    )

    # get the first (and only) solution
    print("cost:", collector.objective_value(0))
    print([("abcdefghij"[i], collector.value(0, x[i])) for i in range(n)])
    print()
    print("failures:", solver.num_failures)
    print("branches:", solver.num_branches)
    print("WallTime:", solver.wall_time_ms)
    print()


if __name__ == "__main__":
    main("cp sample")
