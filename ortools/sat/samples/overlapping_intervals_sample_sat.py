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
"""Code sample to demonstrates how to detect if two intervals overlap."""

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


def OverlappingIntervals():
    """Create the overlapping Boolean variables and enumerate all states."""
    model = cp_model.CpModel()

    horizon = 7

    # First interval.
    start_var_a = model.NewIntVar(0, horizon, 'start_a')
    duration_a = 3
    end_var_a = model.NewIntVar(0, horizon, 'end_a')
    unused_interval_var_a = model.NewIntervalVar(start_var_a, duration_a,
                                                 end_var_a, 'interval_a')

    # Second interval.
    start_var_b = model.NewIntVar(0, horizon, 'start_b')
    duration_b = 2
    end_var_b = model.NewIntVar(0, horizon, 'end_b')
    unused_interval_var_b = model.NewIntervalVar(start_var_b, duration_b,
                                                 end_var_b, 'interval_b')

    # a_after_b Boolean variable.
    a_after_b = model.NewBoolVar('a_after_b')
    model.Add(start_var_a >= end_var_b).OnlyEnforceIf(a_after_b)
    model.Add(start_var_a < end_var_b).OnlyEnforceIf(a_after_b.Not())

    # b_after_a Boolean variable.
    b_after_a = model.NewBoolVar('b_after_a')
    model.Add(start_var_b >= end_var_a).OnlyEnforceIf(b_after_a)
    model.Add(start_var_b < end_var_a).OnlyEnforceIf(b_after_a.Not())

    # Result Boolean variable.
    a_overlaps_b = model.NewBoolVar('a_overlaps_b')

    # Option a: using only clauses
    model.AddBoolOr([a_after_b, b_after_a, a_overlaps_b])
    model.AddImplication(a_after_b, a_overlaps_b.Not())
    model.AddImplication(b_after_a, a_overlaps_b.Not())

    # Option b: using a sum() == 1.
    # model.Add(a_after_b + b_after_a + a_overlaps_b == 1)

    # Search for start values in increasing order for the two intervals.
    model.AddDecisionStrategy([start_var_a, start_var_b], cp_model.CHOOSE_FIRST,
                              cp_model.SELECT_MIN_VALUE)

    # Create a solver and solve with a fixed search.
    solver = cp_model.CpSolver()

    # Force the solver to follow the decision strategy exactly.
    solver.parameters.search_branching = cp_model.FIXED_SEARCH
    # Enumerate all solutions.
    solver.parameters.enumerate_all_solutions = True

    # Search and print out all solutions.
    solution_printer = VarArraySolutionPrinter(
        [start_var_a, start_var_b, a_overlaps_b])
    solver.Solve(model, solution_printer)


OverlappingIntervals()
