#!/usr/bin/env python3
# Copyright 2010-2024 Google LLC
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

"""Solves a simple shift scheduling problem."""

from typing import Sequence
from absl import app
from ortools.sat.python import cp_model


class SolutionPrinter(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions."""

    def __init__(
        self,
        num_vendors,
        num_hours,
        possible_schedules,
        selected_schedules,
        hours_stat,
        min_vendors,
    ):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self.__solution_count = 0
        self.__num_vendors = num_vendors
        self.__num_hours = num_hours
        self.__possible_schedules = possible_schedules
        self.__selected_schedules = selected_schedules
        self.__hours_stat = hours_stat
        self.__min_vendors = min_vendors

    def on_solution_callback(self):
        """Called at each new solution."""
        self.__solution_count += 1
        print("Solution %i: ", self.__solution_count)
        print("  min vendors:", self.__min_vendors)
        for i in range(self.__num_vendors):
            print(
                "  - vendor %i: " % i,
                self.__possible_schedules[self.value(self.__selected_schedules[i])],
            )
        print()

        for j in range(self.__num_hours):
            print("  - # workers on day%2i: " % j, end=" ")
            print(self.value(self.__hours_stat[j]), end=" ")
            print()
        print()

    def solution_count(self):
        """Returns the number of solution found."""
        return self.__solution_count


def vendor_scheduling_sat():
    """Create the shift scheduling model and solve it."""
    # Create the model.
    model = cp_model.CpModel()

    #
    # data
    #
    num_vendors = 9
    num_hours = 10
    num_work_types = 1

    traffic = [100, 500, 100, 200, 320, 300, 200, 220, 300, 120]
    max_traffic_per_vendor = 100

    # Last columns are :
    #   index_of_the_schedule, sum of worked hours (per work type).
    # The index is useful for branching.
    possible_schedules = [
        [1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 8],
        [1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 4],
        [0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 2, 5],
        [0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 3, 4],
        [1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 4, 3],
        [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0],
    ]

    num_possible_schedules = len(possible_schedules)
    selected_schedules = []
    vendors_stat = []
    hours_stat = []

    # Auxiliary data
    min_vendors = [t // max_traffic_per_vendor for t in traffic]
    all_vendors = range(num_vendors)
    all_hours = range(num_hours)

    #
    # Declare variables
    #
    x = {}

    for v in all_vendors:
        tmp = []
        for h in all_hours:
            x[v, h] = model.new_int_var(0, num_work_types, "x[%i,%i]" % (v, h))
            tmp.append(x[v, h])
        selected_schedule = model.new_int_var(
            0, num_possible_schedules - 1, "s[%i]" % v
        )
        hours = model.new_int_var(0, num_hours, "h[%i]" % v)
        selected_schedules.append(selected_schedule)
        vendors_stat.append(hours)
        tmp.append(selected_schedule)
        tmp.append(hours)

        model.add_allowed_assignments(tmp, possible_schedules)

    #
    # Statistics and constraints for each hour
    #
    for h in all_hours:
        workers = model.new_int_var(0, 1000, "workers[%i]" % h)
        model.add(workers == sum(x[v, h] for v in all_vendors))
        hours_stat.append(workers)
        model.add(workers * max_traffic_per_vendor >= traffic[h])

    #
    # Redundant constraint: sort selected_schedules
    #
    for v in range(num_vendors - 1):
        model.add(selected_schedules[v] <= selected_schedules[v + 1])

    # Solve model.
    solver = cp_model.CpSolver()
    solver.parameters.enumerate_all_solutions = True
    solution_printer = SolutionPrinter(
        num_vendors,
        num_hours,
        possible_schedules,
        selected_schedules,
        hours_stat,
        min_vendors,
    )
    status = solver.solve(model, solution_printer)
    print("Status = %s" % solver.status_name(status))

    print("Statistics")
    print("  - conflicts : %i" % solver.num_conflicts)
    print("  - branches  : %i" % solver.num_branches)
    print("  - wall time : %f s" % solver.wall_time)
    print("  - number of solutions found: %i" % solution_printer.solution_count())


def main(argv: Sequence[str]) -> None:
    if len(argv) > 1:
        raise app.UsageError("Too many command-line arguments.")
    vendor_scheduling_sat()


if __name__ == "__main__":
    app.run(main)
