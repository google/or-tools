# Copyright 2010-2018 Google LLC
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
"""Example of a simple nurse scheduling problem."""

# [START program]
from __future__ import print_function
import math
# [START import]
from ortools.sat.python import cp_model

# [END import]


# [START solution_printer]
class NursesPartialSolutionPrinter(cp_model.CpSolverSolutionCallback):
    """Print intermediate solutions."""

    def __init__(self, shifts, num_nurses, num_days, num_shifts, sols):
        cp_model.CpSolverSolutionCallback.__init__(self)
        self._shifts = shifts
        self._num_nurses = num_nurses
        self._num_days = num_days
        self._num_shifts = num_shifts
        self._solutions = set(sols)
        self._solution_count = 0

    def on_solution_callback(self):
        self._solution_count += 1
        if self._solution_count in self._solutions:
            print('Solution %i' % self._solution_count)
            for d in range(self._num_days):
                print('Day %i' % d)
                for n in range(self._num_nurses):
                    is_working = False
                    for s in range(self._num_shifts):
                        if self.Value(self._shifts[(n, d, s)]):
                            is_working = True
                            print('  Nurse %i works shift %i' % (n, s))
                    if not is_working:
                        print('  Nurse {} does not work'.format(n))
            print()

    def solution_count(self):
        return self._solution_count


# [END solution_printer]


def main():
    # Data.
    # [START data]
    num_nurses = 4
    num_shifts = 3
    num_days = 3
    all_nurses = range(num_nurses)
    all_shifts = range(num_shifts)
    all_days = range(num_days)
    # [END data]
    # Creates the model.
    # [START model]
    model = cp_model.CpModel()
    # [END model]

    # Creates shift variables.
    # shifts[(n, d, s)]: nurse 'n' works shift 's' on day 'd'.
    # [START variables]
    shifts = {}
    for n in all_nurses:
        for d in all_days:
            for s in all_shifts:
                shifts[(n, d, s)] = model.NewBoolVar('shift_n%id%is%i' % (n, d,
                                                                          s))
    # [END variables]

    # Each shift is assigned to exactly one nurse.
    # [START exactly_one_nurse]
    for d in all_days:
        for s in all_shifts:
            model.Add(sum(shifts[(n, d, s)] for n in all_nurses) == 1)
    # [END exactly_one_nurse]

    # Each nurse works at most one shift per day.
    # [START at_most_one_shift]
    for n in all_nurses:
        for d in all_days:
            model.Add(sum(shifts[(n, d, s)] for s in all_shifts) <= 1)
    # [END at_most_one_shift]

    # works_shift[(n, s)] is 1 if nurse n works shift s at least one day in
    # the schedule period.
    works_shift = {}
    for n in all_nurses:
        for s in all_shifts:
            works_shift[(n, s)] = model.NewBoolVar('works_shift_n%is%i' % (n,
                                                                           s))
            model.AddMaxEquality(works_shift[(n, s)],
                                 [shifts[(n, d, s)] for d in all_days])

    # [START assign_nurses_evenly]
    # min_shifts_assigned is the largest integer such that every nurse can be
    # assigned at least that number of shifts.
    min_shifts_assigned = int(math.floor(num_shifts * num_days / num_nurses))
    for n in all_nurses:
        model.Add(
            sum(works_shift[(n, s)] for s in all_shifts) >= min_shifts_assigned)
    # [END assign_nurses_evenly]

    # Creates the solver and solve.
    # [START solve]
    solver = cp_model.CpSolver()
    # Display the first five solutions.
    a_few_solutions = range(5)
    solution_printer = NursesPartialSolutionPrinter(
        shifts, num_nurses, num_days, num_shifts, a_few_solutions)
    solver.SearchForAllSolutions(model, solution_printer)
    # [END solve]

    # Statistics.
    print()
    print('Statistics')
    print('  - conflicts       : %i' % solver.NumConflicts())
    print('  - branches        : %i' % solver.NumBranches())
    print('  - wall time       : %f ms' % solver.WallTime())
    print('  - solutions found : %i' % solution_printer.solution_count())


if __name__ == '__main__':
    main()
# [END program]
