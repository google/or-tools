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

# [START program]
"""Code sample to demonstrate how an interval can span across a break."""

from ortools.sat.python import cp_model


class VarArraySolutionPrinter(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions."""

    def __init__(self, variables: list[cp_model.IntVar]):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__variables = variables

    def on_solution_callback(self) -> None:
        for v in self.__variables:
            print(f"{v}={self.value(v)}", end=" ")
        print()


def scheduling_with_calendar_sample_sat():
    """Interval spanning across a lunch break."""
    model = cp_model.CpModel()

    # The data is the following:
    #   Work starts at 8h, ends at 18h, with a lunch break between 13h and 14h.
    #   We need to schedule a task that needs 3 hours of processing time.
    #   Total duration can be 3 or 4 (if it spans the lunch break).
    #
    # Because the duration is at least 3 hours, work cannot start after 15h.
    # Because of the break, work cannot start at 13h.

    start = model.new_int_var_from_domain(
        cp_model.Domain.from_intervals([(8, 12), (14, 15)]), "start"
    )
    duration = model.new_int_var(3, 4, "duration")
    end = model.new_int_var(8, 18, "end")
    unused_interval = model.new_interval_var(start, duration, end, "interval")

    # We have 2 states (spanning across lunch or not)
    across = model.new_bool_var("across")
    non_spanning_hours = cp_model.Domain.from_values([8, 9, 10, 14, 15])
    model.add_linear_expression_in_domain(start, non_spanning_hours).only_enforce_if(
        ~across
    )
    model.add_linear_constraint(start, 11, 12).only_enforce_if(across)
    model.add(duration == 3).only_enforce_if(~across)
    model.add(duration == 4).only_enforce_if(across)

    # Search for x values in increasing order.
    model.add_decision_strategy(
        [start], cp_model.CHOOSE_FIRST, cp_model.SELECT_MIN_VALUE
    )

    # Create a solver and solve with a fixed search.
    solver = cp_model.CpSolver()

    # Force the solver to follow the decision strategy exactly.
    solver.parameters.search_branching = cp_model.FIXED_SEARCH
    # Enumerate all solutions.
    solver.parameters.enumerate_all_solutions = True

    # Search and print all solutions.
    solution_printer = VarArraySolutionPrinter([start, duration, across])
    solver.solve(model, solution_printer)


scheduling_with_calendar_sample_sat()
# [END program]
