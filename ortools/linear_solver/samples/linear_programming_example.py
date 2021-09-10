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
"""Linear optimization example."""
# [START program]
# [START import]
from ortools.linear_solver import pywraplp
# [END import]


def LinearProgrammingExample():
    """Linear programming sample."""
    # Instantiate a Glop solver, naming it LinearExample.
    # [START solver]
    solver = pywraplp.Solver.CreateSolver('GLOP')
    # [END solver]

    # Create the two variables and let them take on any non-negative value.
    # [START variables]
    x = solver.NumVar(0, solver.infinity(), 'x')
    y = solver.NumVar(0, solver.infinity(), 'y')

    print('Number of variables =', solver.NumVariables())
    # [END variables]

    # [START constraints]
    # Constraint 0: x + 2y <= 14.
    solver.Add(x + 2 * y <= 14.0)

    # Constraint 1: 3x - y >= 0.
    solver.Add(3 * x - y >= 0.0)

    # Constraint 2: x - y <= 2.
    solver.Add(x - y <= 2.0)

    print('Number of constraints =', solver.NumConstraints())
    # [END constraints]

    # [START objective]
    # Objective function: 3x + 4y.
    solver.Maximize(3 * x + 4 * y)
    # [END objective]

    # Solve the system.
    # [START solve]
    status = solver.Solve()
    # [END solve]

    # [START print_solution]
    if status == pywraplp.Solver.OPTIMAL:
        print('Solution:')
        print('Objective value =', solver.Objective().Value())
        print('x =', x.solution_value())
        print('y =', y.solution_value())
    else:
        print('The problem does not have an optimal solution.')
    # [END print_solution]

    # [START advanced]
    print('\nAdvanced usage:')
    print('Problem solved in %f milliseconds' % solver.wall_time())
    print('Problem solved in %d iterations' % solver.iterations())
    # [END advanced]


LinearProgrammingExample()
# [END program]
