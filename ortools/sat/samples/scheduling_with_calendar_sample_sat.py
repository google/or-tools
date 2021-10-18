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
"""Code sample to demonstrate how an interval can span across a break."""

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


def SchedulingWithCalendarSampleSat():
    """Interval spanning across a lunch break."""
    model = cp_model.CpModel()

    # The data is the following:
    #   Work starts at 8h, ends at 18h, with a lunch break between 13h and 14h.
    #   We need to schedule a task that needs 3 hours of processing time.
    #   Total duration can be 3 or 4 (if it spans the lunch break).
    #
    # Because the duration is at least 3 hours, work cannot start after 15h.
    # Because of the break, work cannot start at 13h.

    start = model.NewIntVarFromDomain(
        cp_model.Domain.FromIntervals([(8, 12), (14, 15)]), 'start')
    duration = model.NewIntVar(3, 4, 'duration')
    end = model.NewIntVar(8, 18, 'end')
    unused_interval = model.NewIntervalVar(start, duration, end, 'interval')

    # We have 2 states (spanning across lunch or not)
    across = model.NewBoolVar('across')
    non_spanning_hours = cp_model.Domain.FromValues([8, 9, 10, 14, 15])
    model.AddLinearExpressionInDomain(start, non_spanning_hours).OnlyEnforceIf(
        across.Not())
    model.AddLinearConstraint(start, 11, 12).OnlyEnforceIf(across)
    model.Add(duration == 3).OnlyEnforceIf(across.Not())
    model.Add(duration == 4).OnlyEnforceIf(across)

    # Search for x values in increasing order.
    model.AddDecisionStrategy([start], cp_model.CHOOSE_FIRST,
                              cp_model.SELECT_MIN_VALUE)

    # Create a solver and solve with a fixed search.
    solver = cp_model.CpSolver()

    # Force the solver to follow the decision strategy exactly.
    solver.parameters.search_branching = cp_model.FIXED_SEARCH
    # Enumerate all solutions.
    solver.parameters.enumerate_all_solutions = True

    # Search and print all solutions.
    solution_printer = VarArraySolutionPrinter([start, duration, across])
    solver.Solve(model, solution_printer)


SchedulingWithCalendarSampleSat()
