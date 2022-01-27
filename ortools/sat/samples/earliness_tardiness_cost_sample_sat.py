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
"""Encodes an convex piecewise linear function."""

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


def earliness_tardiness_cost_sample_sat():
    """Encode the piecewise linear expression."""

    earliness_date = 5  # ed.
    earliness_cost = 8
    lateness_date = 15  # ld.
    lateness_cost = 12

    # Model.
    model = cp_model.CpModel()

    # Declare our primary variable.
    x = model.NewIntVar(0, 20, 'x')

    # Create the expression variable and implement the piecewise linear function.
    #
    #  \        /
    #   \______/
    #   ed    ld
    #
    large_constant = 1000
    expr = model.NewIntVar(0, large_constant, 'expr')

    # First segment.
    s1 = model.NewIntVar(-large_constant, large_constant, 's1')
    model.Add(s1 == earliness_cost * (earliness_date - x))

    # Second segment.
    s2 = 0

    # Third segment.
    s3 = model.NewIntVar(-large_constant, large_constant, 's3')
    model.Add(s3 == lateness_cost * (x - lateness_date))

    # Link together expr and x through s1, s2, and s3.
    model.AddMaxEquality(expr, [s1, s2, s3])

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
    solution_printer = VarArraySolutionPrinter([x, expr])
    solver.Solve(model, solution_printer)


earliness_tardiness_cost_sample_sat()
