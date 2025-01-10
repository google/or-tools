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

"""Solves an optimization problem and displays all intermediate solutions."""

# [START program]
from ortools.sat.python import cp_model


# You need to subclass the cp_model.CpSolverSolutionCallback class.
# [START print_solution]
class VarArrayAndObjectiveSolutionPrinter(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions."""

    def __init__(self, variables: list[cp_model.IntVar]):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__variables = variables
        self.__solution_count = 0

    def on_solution_callback(self) -> None:
        print(f"Solution {self.__solution_count}")
        print(f"  objective value = {self.objective_value}")
        for v in self.__variables:
            print(f"  {v}={self.value(v)}", end=" ")
        print()
        self.__solution_count += 1

    @property
    def solution_count(self) -> int:
        return self.__solution_count
        # [END print_solution]


def solve_and_print_intermediate_solutions_sample_sat():
    """Showcases printing intermediate solutions found during search."""
    # Creates the model.
    # [START model]
    model = cp_model.CpModel()
    # [END model]

    # Creates the variables.
    # [START variables]
    num_vals = 3
    x = model.new_int_var(0, num_vals - 1, "x")
    y = model.new_int_var(0, num_vals - 1, "y")
    z = model.new_int_var(0, num_vals - 1, "z")
    # [END variables]

    # Creates the constraints.
    # [START constraints]
    model.add(x != y)
    # [END constraints]

    # [START objective]
    model.maximize(x + 2 * y + 3 * z)
    # [END objective]

    # Creates a solver and solves.
    # [START solve]
    solver = cp_model.CpSolver()
    solution_printer = VarArrayAndObjectiveSolutionPrinter([x, y, z])
    status = solver.solve(model, solution_printer)
    # [END solve]

    print(f"Status = {solver.status_name(status)}")
    print(f"Number of solutions found: {solution_printer.solution_count}")


solve_and_print_intermediate_solutions_sample_sat()
# [END program]
