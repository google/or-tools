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
"""Simple solve."""
# [START import]
from ortools.sat.python import cp_model
# [END import]


def SimpleSatProgram():
    """Minimal CP-SAT example to showcase calling the solver."""
    # Creates the model.
    # [START model]
    model = cp_model.CpModel()
    # [END model]

    # Creates the variables.
    # [START variables]
    num_vals = 3
    x = model.NewIntVar(0, num_vals - 1, 'x')
    y = model.NewIntVar(0, num_vals - 1, 'y')
    z = model.NewIntVar(0, num_vals - 1, 'z')
    # [END variables]

    # Creates the constraints.
    # [START constraints]
    model.Add(x != y)
    # [END constraints]

    # Creates a solver and solves the model.
    # [START solve]
    solver = cp_model.CpSolver()
    status = solver.Solve(model)
    # [END solve]

    # [START print_solution]
    if status == cp_model.OPTIMAL or status == cp_model.FEASIBLE:
        print('x = %i' % solver.Value(x))
        print('y = %i' % solver.Value(y))
        print('z = %i' % solver.Value(z))
    else:
        print('No solution found.')
    # [END print_solution]


SimpleSatProgram()
# [END program]
