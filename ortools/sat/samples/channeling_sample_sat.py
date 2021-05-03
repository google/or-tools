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
"""Link integer constraints together."""

from ortools.sat.python import cp_model


class VarArraySolutionPrinter(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions."""

    def __init__(self, variables):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__variables = variables
        self.__solution_count = 0

    def on_solution_callback(self):
        self.__solution_count += 1
        for v in self.__variables:
            print('%s=%i' % (v, self.Value(v)), end=' ')
        print()

    def solution_count(self):
        return self.__solution_count


def ChannelingSampleSat():
    """Demonstrates how to link integer constraints together."""

    # Create the CP-SAT model.
    model = cp_model.CpModel()

    # Declare our two primary variables.
    x = model.NewIntVar(0, 10, 'x')
    y = model.NewIntVar(0, 10, 'y')

    # Declare our intermediate boolean variable.
    b = model.NewBoolVar('b')

    # Implement b == (x >= 5).
    model.Add(x >= 5).OnlyEnforceIf(b)
    model.Add(x < 5).OnlyEnforceIf(b.Not())

    # Create our two half-reified constraints.
    # First, b implies (y == 10 - x).
    model.Add(y == 10 - x).OnlyEnforceIf(b)
    # Second, not(b) implies y == 0.
    model.Add(y == 0).OnlyEnforceIf(b.Not())

    # Search for x values in increasing order.
    model.AddDecisionStrategy([x], cp_model.CHOOSE_FIRST,
                              cp_model.SELECT_MIN_VALUE)

    # Create a solver and solve with a fixed search.
    solver = cp_model.CpSolver()

    # Force the solver to follow the decision strategy exactly.
    solver.parameters.search_branching = cp_model.FIXED_SEARCH
    # Enumerate all solutions.
    solver.parameters.enumerate_all_solutions = True

    # Search and print out all solutions.
    solution_printer = VarArraySolutionPrinter([x, y, b])
    solver.Solve(model, solution_printer)


ChannelingSampleSat()
