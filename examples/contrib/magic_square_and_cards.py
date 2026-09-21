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
"""Magic squares and cards problem in Google CP Solver.

Martin Gardner (July 1971)
'''
Allowing duplicates values, what is the largest constant sum for an order-3
magic square that can be formed with nine cards from the deck.
'''



This model was created by Hakan Kjellerstrand (hakank@gmail.com)
Also see my other Google CP Solver models:
http://www.hakank.org/google_or_tools/
"""

import sys

from ortools.constraint_solver.python import constraint_solver as cp


def main(n=3):
    # Create the solver.
    solver = cp.Solver("n-queens")

    #
    # data
    #
    # n = 3

    #
    # declare variables
    #
    x = {}
    for i in range(n):
        for j in range(n):
            x[(i, j)] = solver.new_int_var(1, 13, "x(%i,%i)" % (i, j))
    x_flat = [x[(i, j)] for i in range(n) for j in range(n)]

    s = solver.new_int_var(1, 13 * 4, "s")
    counts = [solver.new_int_var(0, 4, "counts(%i)" % i) for i in range(14)]

    #
    # constraints
    #
    solver.add_distribute(x_flat, list(range(14)), counts)

    # the standard magic square constraints (sans all_different)
    [solver.add(solver.sum([x[(i, j)] for j in range(n)]) == s) for i in range(n)]
    [solver.add(solver.sum([x[(i, j)] for i in range(n)]) == s) for j in range(n)]

    solver.add(solver.sum([x[(i, i)] for i in range(n)]) == s)  # diag 1
    solver.add(solver.sum([x[(i, n - i - 1)] for i in range(n)]) == s)  # diag 2

    # redundant constraint
    solver.add(solver.sum(counts) == n * n)

    # objective
    objective = solver.maximize(s, 1)

    #
    # solution and search
    #
    solution = solver.assignment()
    solution.add(x_flat)
    solution.add(s)
    solution.add(counts)

    # db: DecisionBuilder
    db = solver.phase(
        x_flat,
        cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
        cp.IntValueStrategy.ASSIGN_MAX_VALUE,
    )

    solver.new_search(db, [objective])
    num_solutions = 0
    while solver.next_solution():
        print("s:", s.value())
        print("counts:", [counts[i].value() for i in range(14)])
        for i in range(n):
            for j in range(n):
                print(x[(i, j)].value(), end=" ")
            print()

        print()
        num_solutions += 1
    solver.end_search()

    print()
    print("num_solutions:", num_solutions)
    print("failures:", solver.num_failures)
    print("branches:", solver.num_branches)
    print("WallTime:", solver.wall_time_ms)


n = 3
if __name__ == "__main__":
    if len(sys.argv) > 1:
        n = int(sys.argv[1])
    main(n)
