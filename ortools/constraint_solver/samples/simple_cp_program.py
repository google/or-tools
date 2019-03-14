#!/usr/bin/env python
# This Python file uses the following encoding: utf-8
# Copyright 2018 Google LLC
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
from __future__ import print_function
from ortools.constraint_solver import pywrapcp
# [END import]


def main():
    """Entry point of the program"""
    # [START solver]
    solver = pywrapcp.Solver('ConstraintExample')
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

    print('Number of constraints =', solver.Constraints())
    # [END constraints]

    # Call the solver.
    # [START solve]
    decision_builder = solver.Phase([x, y, z], solver.CHOOSE_FIRST_UNBOUND,
                                    solver.ASSIGN_MIN_VALUE)
    # [END solve]

    # [START print_solution]
    solver.NewSearch(decision_builder)
    while solver.NextSolution():
        solution = 'Solution:'
        for var in [x, y, z]:
            solution += ' {} = {};'.format(var.Name(), var.Value())
        print(solution)
    solver.EndSearch()
    print("Number of solutions found:", solver.Solutions())
    # [END print_solution]

    # [START advanced]
    print('\nAdvanced usage:')
    print('Problem solved in ', solver.WallTime(), ' milliseconds')
    print('Memory usage: ', pywrapcp.Solver.MemoryUsage(), ' bytes')
    # [END advanced]


if __name__ == "__main__":
    main()
# [END program]
