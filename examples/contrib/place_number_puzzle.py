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

  Place number puzzle Google CP Solver.

  http://ai.uwaterloo.ca/~vanbeek/Courses/Slides/introduction.pdf
  '''
  Place numbers 1 through 8 on nodes
  - each number appears exactly once
  - no connected nodes have consecutive numbers
       2 - 5
     / | X | \
   1 - 3 - 6 - 8
     \ | X | /
       4 - 7
  ""

  Compare with the following models:
  * MiniZinc: http://www.hakank.org/minizinc/place_number.mzn
  * Comet: http://www.hakank.org/comet/place_number_puzzle.co
  * ECLiPSe: http://www.hakank.org/eclipse/place_number_puzzle.ecl
  * SICStus Prolog: http://www.hakank.org/sicstus/place_number_puzzle.pl
  * Gecode: http://www.hakank.org/gecode/place_number_puzzle.cpp

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/

"""

import sys

from ortools.constraint_solver.python import constraint_solver as cp


def main():

    # Create the solver.
    solver = cp.Solver("Place number")

    # data
    m = 32
    n = 8
    # Note: this is 1-based for compatibility (and lazyness)
    graph = [
        [1, 2],
        [1, 3],
        [1, 4],
        [2, 1],
        [2, 3],
        [2, 5],
        [2, 6],
        [3, 2],
        [3, 4],
        [3, 6],
        [3, 7],
        [4, 1],
        [4, 3],
        [4, 6],
        [4, 7],
        [5, 2],
        [5, 3],
        [5, 6],
        [5, 8],
        [6, 2],
        [6, 3],
        [6, 4],
        [6, 5],
        [6, 7],
        [6, 8],
        [7, 3],
        [7, 4],
        [7, 6],
        [7, 8],
        [8, 5],
        [8, 6],
        [8, 7],
    ]

    # declare variables
    x = [solver.new_int_var(1, n, "x%i" % i) for i in range(n)]

    #
    # constraints
    #
    solver.add_all_different(x)
    for i in range(m):
        # Note: make 0-based
        solver.add(abs(x[graph[i][0] - 1] - x[graph[i][1] - 1]) > 1)

    # symmetry breaking
    solver.add(x[0] < x[n - 1])

    #
    # solution and search
    #
    solution = solver.assignment()
    solution.add(x)

    collector = solver.all_solution_collector(solution)

    solver.solve(
        solver.phase(
            x,
            cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
            cp.IntValueStrategy.ASSIGN_MIN_VALUE,
        ),
        [collector],
    )

    num_solutions = collector.solution_count
    for s in range(num_solutions):
        print("x:", [collector.value(s, x[i]) for i in range(len(x))])

    print()
    print("num_solutions:", num_solutions)
    print("failures:", solver.num_failures)
    print("branches:", solver.num_branches)
    print("WallTime:", solver.wall_time_ms)
    print()


if __name__ == "__main__":
    main()
