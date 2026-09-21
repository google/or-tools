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
"""Nontransitive dice in Google CP Solver.

 From
 http://en.wikipedia.org/wiki/Nontransitive_dice
 '''
 A set of nontransitive dice is a set of dice for which the relation
 'is more likely to roll a higher number' is not transitive. See also
 intransitivity.

 This situation is similar to that in the game Rock, Paper, Scissors,
 in which each element has an advantage over one choice and a
 disadvantage to the other.
 '''

 I start with the 3 dice version
 '''
    * die A has sides {2,2,4,4,9,9},
    * die B has sides {1,1,6,6,8,8}, and
    * die C has sides {3,3,5,5,7,7}.
 '''

 3 dice:
 Maximum winning: 27
 comp: [19, 27, 19]
 dice:
 [[0, 0, 3, 6, 6, 6],
 [2, 5, 5, 5, 5, 5],
 [1, 1, 4, 4, 4, 7]]
 max_win: 27

 Number of solutions:  1
 Nodes: 1649873  Time: 25.94
 getFailures: 1649853
 getBacktracks: 1649873
 getPropags: 98105090

Max winnings where they are the same: 21
  comp: [21, 21, 21]
  dice:
  [[0, 0, 3, 3, 3, 6],
  [2, 2, 2, 2, 2, 5],
  [1, 1, 1, 4, 4, 4]]
  max_win: 21

  Compare with these models:
  * MiniZinc: http://hakank.org/minizinc/nontransitive_dice.mzn
  * Comet: http://hakank.org/comet/nontransitive_dice.co


 This model was created by Hakan Kjellerstrand (hakank@gmail.com)
 Also see my other Google CP Solver models:
 http://www.hakank.org/google_or_tools/
"""

import sys

from ortools.constraint_solver.python import constraint_solver as cp


def main(m=3, n=6, minimize_val=0):

    # Create the solver.
    solver = cp.Solver("Nontransitive dice")

    #
    # data
    #
    print("number of dice:", m)
    print("number of sides:", n)

    #
    # declare variables
    #

    dice = {}
    for i in range(m):
        for j in range(n):
            dice[(i, j)] = solver.new_int_var(1, n * 2, "dice(%i,%i)" % (i, j))
    dice_flat = [dice[(i, j)] for i in range(m) for j in range(n)]

    comp = {}
    for i in range(m):
        for j in range(2):
            comp[(i, j)] = solver.new_int_var(0, n * n, "comp(%i,%i)" % (i, j))
    comp_flat = [comp[(i, j)] for i in range(m) for j in range(2)]

    # The following variables are for summaries or objectives
    gap = [solver.new_int_var(0, n * n, "gap(%i)" % i) for i in range(m)]
    gap_sum = solver.new_int_var(0, m * n * n, "gap_sum")

    max_val = solver.new_int_var(0, n * 2, "max_val")
    max_win = solver.new_int_var(0, n * n, "max_win")

    # number of occurrences of each value of the dice
    counts = [solver.new_int_var(0, n * m, "counts(%i)" % i) for i in range(n * 2 + 1)]

    #
    # constraints
    #

    # number of occurrences for each number
    solver.add_distribute(dice_flat, list(range(n * 2 + 1)), counts)

    solver.add(max_win == solver.max(comp_flat))
    solver.add(max_val == solver.max(dice_flat))

    # order of the number of each die, lowest first
    [
        solver.add(dice[(i, j)] <= dice[(i, j + 1)])
        for i in range(m)
        for j in range(n - 1)
    ]

    # nontransitivity
    [comp[i, 0] > comp[i, 1] for i in range(m)],

    # probability gap
    [solver.add(gap[i] == comp[i, 0] - comp[i, 1]) for i in range(m)]
    [solver.add(gap[i] > 0) for i in range(m)]
    solver.add(gap_sum == solver.sum(gap))

    # and now we roll...
    #  Number of wins for [A vs B, B vs A]
    for d in range(m):
        b1 = [
            solver.add_is_greater_var(dice[d % m, r1], dice[(d + 1) % m, r2])
            for r1 in range(n)
            for r2 in range(n)
        ]
        solver.add(comp[d % m, 0] == solver.sum(b1))

        b2 = [
            solver.add_is_greater_var(dice[(d + 1) % m, r1], dice[d % m, r2])
            for r1 in range(n)
            for r2 in range(n)
        ]
        solver.add(comp[d % m, 1] == solver.sum(b2))

    # objective
    if minimize_val != 0:
        print("Minimizing max_val")
        objective = solver.minimize(max_val, 1)
        # other experiments
        # objective = solver.maximize(max_win, 1)
        # objective = solver.maximize(gap_sum, 1)

    #
    # solution and search
    #
    db = solver.phase(
        dice_flat + comp_flat,
        cp.IntVarStrategy.INT_VAR_DEFAULT,
        cp.IntValueStrategy.ASSIGN_MIN_VALUE,
    )

    if minimize_val:
        solver.new_search(db, [objective])
    else:
        solver.new_search(db)

    num_solutions = 0
    while solver.next_solution():
        print("gap_sum:", gap_sum.value())
        print("gap:", [gap[i].value() for i in range(m)])
        print("max_val:", max_val.value())
        print("max_win:", max_win.value())
        print("dice:")
        for i in range(m):
            for j in range(n):
                print(dice[(i, j)].value(), end=" ")
            print()
        print("comp:")
        for i in range(m):
            for j in range(2):
                print(comp[(i, j)].value(), end=" ")
            print()
        print("counts:", [counts[i].value() for i in range(n * 2 + 1)])
        print()

        num_solutions += 1

    solver.end_search()

    print()
    print("num_solutions:", num_solutions)
    print("failures:", solver.num_failures)
    print("branches:", solver.num_branches)
    print("WallTime:", solver.wall_time_ms)


m = 3  # number of dice
n = 6  # number of sides of each die
minimize_val = 0  # Minimizing max value (0: no, 1: yes)
if __name__ == "__main__":
    if len(sys.argv) > 1:
        m = int(sys.argv[1])
    if len(sys.argv) > 2:
        n = int(sys.argv[2])
    if len(sys.argv) > 3:
        minimize_val = int(sys.argv[3])

    main(m, n, minimize_val)
