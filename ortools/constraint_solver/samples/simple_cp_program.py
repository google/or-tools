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
"""Simple Constraint optimization example."""

# [START import]
from ortools.constraint_solver.python import constraint_solver as cp

# [END import]


def main():
    """Entry point of the program."""
    # Instantiate the solver.
    # [START solver]
    solver = cp.Solver("CPSimple")
    # [END solver]

    # Create the variables.
    # [START variables]
    num_vals = 3
    x = solver.new_int_var(0, num_vals - 1, "x")
    y = solver.new_int_var(0, num_vals - 1, "y")
    z = solver.new_int_var(0, num_vals - 1, "z")
    # [END variables]

    # Constraint 0: x != y.
    # [START constraints]
    solver.add(x != y)
    print("Number of constraints: ", solver.num_constraints)
    # [END constraints]

    # Solve the problem.
    # [START solve]
    decision_builder = solver.phase(
        [x, y, z],
        cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
        cp.IntValueStrategy.ASSIGN_MIN_VALUE,
    )
    # [END solve]

    # Print solution on console.
    # [START print_solution]
    count = 0
    solver.new_search(decision_builder)
    while solver.next_solution():
        count += 1
        solution = f"Solution {count}:\n"
        for var in [x, y, z]:
            solution += f" {var.name} = {var.value()}"
        print(solution)
    solver.end_search()
    print(f"Number of solutions found: {count}")
    # [END print_solution]

    # [START advanced]
    print("Advanced usage:")
    print(f"Memory usage: {solver.memory_usage()} bytes")
    print(f"Problem solved in {solver.wall_time_ms} ms")
    # [END advanced]


if __name__ == "__main__":
    main()
# [END program]
