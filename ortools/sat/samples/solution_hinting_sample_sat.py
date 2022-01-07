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
"""Code sample that solves a model using solution hinting."""

# [START program]
from ortools.sat.python import cp_model


def SolutionHintingSampleSat():
    """Showcases solution hinting."""
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

    # [START objective]
    model.Maximize(x + 2 * y + 3 * z)
    # [END objective]

    # Solution hinting: x <- 1, y <- 2
    model.AddHint(x, 1)
    model.AddHint(y, 2)

    # Creates a solver and solves.
    # [START solve]
    solver = cp_model.CpSolver()
    solution_printer = cp_model.VarArrayAndObjectiveSolutionPrinter([x, y, z])
    status = solver.Solve(model, solution_printer)
    # [END solve]

    print('Status = %s' % solver.StatusName(status))
    print('Number of solutions found: %i' % solution_printer.solution_count())


SolutionHintingSampleSat()
# [END program]
