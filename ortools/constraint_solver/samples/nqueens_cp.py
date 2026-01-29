#!/usr/bin/env python3
# Copyright 2010-2025 Google LLC
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

# [START program]
"""OR-Tools solution to the N-queens problem."""
# [START import]
import sys
from ortools.constraint_solver.python import constraint_solver as cp

# [END import]


def main(board_size):
    # Creates the solver.
    # [START solver]
    solver = cp.Solver("n-queens")
    # [END solver]

    # Creates the variables.
    # [START variables]
    # The array index is the column, and the value is the row.
    queens = [solver.new_int_var(0, board_size - 1, f"x{i}") for i in range(board_size)]
    # [END variables]

    # Creates the constraints.
    # [START constraints]
    # All rows must be different.
    solver.add_all_different(queens)

    # No two queens can be on the same diagonal.
    solver.add_all_different([queens[i] + i for i in range(board_size)])
    solver.add_all_different([queens[i] - i for i in range(board_size)])
    # [END constraints]

    # [START db]
    db = solver.phase(
        queens,
        cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
        cp.IntValueStrategy.ASSIGN_MIN_VALUE,
    )
    # [END db]

    # [START solve]
    # Iterates through the solutions, displaying each.
    num_solutions = 0
    solver.new_search(db)
    while solver.next_solution():
        # Displays the solution just computed.
        for i in range(board_size):
            for j in range(board_size):
                if queens[j].value() == i:
                    # There is a queen in column j, row i.
                    print("Q", end=" ")
                else:
                    print("_", end=" ")
            print()
        print()
        num_solutions += 1
    solver.end_search()
    # [END solve]

    # Statistics.
    # [START statistics]
    print("\nStatistics")
    print(f"  failures: {solver.num_failures}")
    print(f"  branches: {solver.num_branches}")
    print(f"  wall time: {solver.wall_time_ms} ms")
    print(f"  Solutions found: {num_solutions}")
    # [END statistics]


if __name__ == "__main__":
    # By default, solve the 8x8 problem.
    size = 8
    if len(sys.argv) > 1:
        size = int(sys.argv[1])
    main(size)
# [END program]
