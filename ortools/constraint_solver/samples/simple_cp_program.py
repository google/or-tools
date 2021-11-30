#!/usr/bin/env python3
# Copyright 2010-2021 Google LLC
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
from ortools.constraint_solver import pywrapcp
# [END import]


def main():
    """Entry point of the program."""
    # Instantiate the solver.
    # [START solver]
    solver = pywrapcp.Solver('CPSimple')
    # [END solver]

    # Create the variables.
    # [START variables]
    num_vals = 3
    x = solver.IntVar(0, num_vals - 1, 'x')
    y = solver.IntVar(0, num_vals - 1, 'y')
    z = solver.IntVar(0, num_vals - 1, 'z')
    # [END variables]

    # Constraint 0: x != y.
    # [START constraints]
    solver.Add(x != y)
    print('Number of constraints: ', solver.Constraints())
    # [END constraints]

    # Solve the problem.
    # [START solve]
    decision_builder = solver.Phase([x, y, z], solver.CHOOSE_FIRST_UNBOUND,
                                    solver.ASSIGN_MIN_VALUE)
    # [END solve]

    # Print solution on console.
    # [START print_solution]
    count = 0
    solver.NewSearch(decision_builder)
    while solver.NextSolution():
        count += 1
        solution = 'Solution {}:\n'.format(count)
        for var in [x, y, z]:
            solution += ' {} = {}'.format(var.Name(), var.Value())
        print(solution)
    solver.EndSearch()
    print('Number of solutions found: ', count)
    # [END print_solution]

    # [START advanced]
    print('Advanced usage:')
    print('Problem solved in ', solver.WallTime(), 'ms')
    print('Memory usage: ', pywrapcp.Solver.MemoryUsage(), 'bytes')
    # [END advanced]


if __name__ == '__main__':
    main()
# [END program]
