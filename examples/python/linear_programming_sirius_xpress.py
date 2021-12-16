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
"""Linear optimization example"""

from __future__ import print_function

import os
if os.name == 'nt':
    os.add_dll_directory(os.getenv('SIRIUS_BIN_DIR', ''))
    os.add_dll_directory(os.getenv('XPRESS_BIN_DIR', ''))

from ortools.linear_solver import pywraplp


def main(name, problem_type):
    """Entry point of the program"""
    # Instantiate the solver, naming it LinearExample.
    
    print('------------ {} ------------'.format(name))
    solver = pywraplp.Solver('LinearExample',
                             problem_type)

    # Create the two variables and let them take on any value.
    x = solver.NumVar(-solver.infinity(), solver.infinity(), 'x')
    y = solver.NumVar(-solver.infinity(), solver.infinity(), 'y')

    # Objective function: Maximize 3x + 4y.
    objective = solver.Objective()
    objective.SetCoefficient(x, 3)
    objective.SetCoefficient(y, 4)
    objective.SetMaximization()

    # Constraint 0: x + 2y <= 14.
    constraint0 = solver.Constraint(-solver.infinity(), 14)
    constraint0.SetCoefficient(x, 1)
    constraint0.SetCoefficient(y, 2)

    # Constraint 1: 3x - y >= 0.
    constraint1 = solver.Constraint(0, solver.infinity())
    constraint1.SetCoefficient(x, 3)
    constraint1.SetCoefficient(y, -1)

    # Constraint 2: x - y <= 2.
    constraint2 = solver.Constraint(-solver.infinity(), 2)
    constraint2.SetCoefficient(x, 1)
    constraint2.SetCoefficient(y, -1)

    print('Number of variables =', solver.NumVariables())
    print('Number of constraints =', solver.NumConstraints())

    # Solve the system.
    status = solver.Solve()
    # Check that the problem has an optimal solution.
    if status != pywraplp.Solver.OPTIMAL:
        print("The problem does not have an optimal solution!")
        exit(1)

    print('Solution:')
    print('x =', x.solution_value())
    print('y =', y.solution_value())
    print('Optimal objective value =', objective.Value())
    print('')
    print('Advanced usage:')
    print('Problem solved in ', solver.wall_time(), ' milliseconds')
    print('Problem solved in ', solver.iterations(), ' iterations')
    print('x: reduced cost =', x.reduced_cost())
    print('y: reduced cost =', y.reduced_cost())
    activities = solver.ComputeConstraintActivities()
    print('constraint0: dual value =',
          constraint0.dual_value(), ' activities =',
          activities[constraint0.index()])
    print('constraint1: dual value =',
          constraint1.dual_value(), ' activities =',
          activities[constraint1.index()])
    print('constraint2: dual value =',
          constraint2.dual_value(), ' activities =',
          activities[constraint2.index()])


if __name__ == '__main__':
    main("SIRIUS_LINEAR_PROGRAMMING",
         pywraplp.Solver.SIRIUS_LINEAR_PROGRAMMING)
    main("XPRESS_LINEAR_PROGRAMMING",
         pywraplp.Solver.XPRESS_LINEAR_PROGRAMMING)
