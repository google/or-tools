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
"""Assignment problem in Google CP Solver.

Winston 'Operations Research', Assignment Problems, page 393f
(generalized version with added test column)

Compare with the following models:
* Comet   : http://www.hakank.org/comet/assignment.co
* ECLiPSE : http://www.hakank.org/eclipse/assignment.ecl
* Gecode  : http://www.hakank.org/gecode/assignment.cpp
* MiniZinc: http://www.hakank.org/minizinc/assignment.mzn
* Tailor/Essence': http://www.hakank.org/tailor/assignment.eprime
* SICStus: http://hakank.org/sicstus/assignment.pl

This model was created by Hakan Kjellerstrand (hakank@gmail.com)
Also see my other Google CP Solver models:
http://www.hakank.org/google_or_tools/
"""

from ortools.constraint_solver.python import constraint_solver as cp


def main(cost, rows, cols):
    # Create the solver.
    solver = cp.Solver("n-queens")

    #
    # data
    #

    # declare variables
    total_cost = solver.new_int_var(0, 100, "total_cost")
    x = []
    for i in range(rows):
        t = []
        for j in range(cols):
            t.append(solver.new_int_var(0, 1, "x[%i,%i]" % (i, j)))
        x.append(t)
    x_flat = [x[i][j] for i in range(rows) for j in range(cols)]

    #
    # constraints
    #

    # total_cost
    solver.add(
        total_cost
        == solver.sum(
            [solver.weighted_sum(x_row, cost_row) for (x_row, cost_row) in zip(x, cost)]
        )
    )

    # exacly one assignment per row, all rows must be assigned
    [
        solver.add(solver.sum([x[row][j] for j in range(cols)]) == 1)
        for row in range(rows)
    ]

    # zero or one assignments per column
    [
        solver.add(solver.sum([x[i][col] for i in range(rows)]) <= 1)
        for col in range(cols)
    ]

    objective = solver.minimize(total_cost, 1)

    #
    # solution and search
    #
    solution = solver.assignment()
    solution.add(x_flat)
    solution.add(total_cost)

    # db: DecisionBuilder
    db = solver.phase(
        x_flat,
        cp.IntVarStrategy.INT_VAR_SIMPLE,
        cp.IntValueStrategy.ASSIGN_MIN_VALUE,
    )

    solver.new_search(db, [objective])
    num_solutions = 0
    while solver.next_solution():
        print("total_cost:", total_cost.value())
        for i in range(rows):
            for j in range(cols):
                print(x[i][j].value(), end=" ")
            print()
        print()

        for i in range(rows):
            print("Task:", i, end=" ")
            for j in range(cols):
                if x[i][j].value() == 1:
                    print(" is done by ", j)
        print()

        num_solutions += 1
    solver.end_search()

    print()
    print("num_solutions:", num_solutions)
    print("failures:", solver.num_failures)
    print("branches:", solver.num_branches)
    print("WallTime:", solver.wall_time_ms)


# Problem instance
# hakank: I added the fifth column to make it more
#         interesting
rows = 4
cols = 5
cost = [[14, 5, 8, 7, 15], [2, 12, 6, 5, 3], [7, 8, 3, 9, 7], [2, 4, 6, 10, 1]]

if __name__ == "__main__":
    main(cost, rows, cols)
