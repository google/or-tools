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
"""Solves an optimization problem and displays all intermediate solutions."""

# [START program]
from ortools.sat.python import cp_model


# You need to subclass the cp_model.CpSolverSolutionCallback class.
# [START print_solution]
class VarArrayAndObjectiveSolutionPrinter(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions."""

    def __init__(self, variables):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__variables = variables
        self.__solution_count = 0

    def on_solution_callback(self):
        print('Solution %i' % self.__solution_count)
        print('  objective value = %i' % self.ObjectiveValue())
        for v in self.__variables:
            print('  %s = %i' % (v, self.Value(v)), end=' ')
        print()
        self.__solution_count += 1

    def solution_count(self):
        return self.__solution_count
        # [END print_solution]


def SolveAndPrintIntermediateSolutionsSampleSat():
    """Showcases printing intermediate solutions found during search."""
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

    # Creates a solver and solves.
    # [START solve]
    solver = cp_model.CpSolver()
    solution_printer = VarArrayAndObjectiveSolutionPrinter([x, y, z])
    status = solver.Solve(model, solution_printer)
    # [END solve]

    print('Status = %s' % solver.StatusName(status))
    print('Number of solutions found: %i' % solution_printer.solution_count())


SolveAndPrintIntermediateSolutionsSampleSat()
# [END program]
