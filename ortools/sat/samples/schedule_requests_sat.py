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
"""Nurse scheduling problem with shift requests."""

# [START program]
from __future__ import division
from __future__ import print_function
# [START import]
from ortools.sat.python import cp_model

# [END import]


def main():
    # This program tries to find an optimal assignment of nurses to shifts
    # (3 shifts per day, for 7 days), subject to some constraints (see below).
    # Each nurse can request to be assigned to specific shifts.
    # The optimal assignment maximizes the number of fulfilled shift requests.
    # [START data]
    num_nurses = 5
    num_shifts = 3
    num_days = 7
    all_nurses = range(num_nurses)
    all_shifts = range(num_shifts)
    all_days = range(num_days)
    shift_requests = [[[0, 0, 1], [0, 0, 0], [0, 0, 0], [0, 0, 0], [0, 0, 1],
                       [0, 1, 0], [0, 0, 1]],
                      [[0, 0, 0], [0, 0, 0], [0, 1, 0], [0, 1, 0], [1, 0, 0],
                       [0, 0, 0], [0, 0, 1]],
                      [[0, 1, 0], [0, 1, 0], [0, 0, 0], [1, 0, 0], [0, 0, 0],
                       [0, 1, 0], [0, 0, 0]],
                      [[0, 0, 1], [0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 0],
                       [1, 0, 0], [0, 0, 0]],
                      [[0, 0, 0], [0, 0, 1], [0, 1, 0], [0, 0, 0], [1, 0, 0],
                       [0, 1, 0], [0, 0, 0]]]
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

    # Each shift is assigned to exactly one nurse in .
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

    # [START assign_nurses_evenly]
    # min_shifts_assigned is the largest integer such that every nurse can be
    # assigned at least that number of shifts.
    min_shifts_per_nurse = (num_shifts * num_days) // num_nurses
    max_shifts_per_nurse = min_shifts_per_nurse + 1
    for n in all_nurses:
        num_shifts_worked = sum(
            shifts[(n, d, s)] for d in all_days for s in all_shifts)
        model.Add(min_shifts_per_nurse <= num_shifts_worked)
        model.Add(num_shifts_worked <= max_shifts_per_nurse)
    # [END assign_nurses_evenly]

    # [START objective]
    model.Maximize(
        sum(shift_requests[n][d][s] * shifts[(n, d, s)] for n in all_nurses
            for d in all_days for s in all_shifts))
    # [END objective]
    # Creates the solver and solve.
    # [START solve]
    solver = cp_model.CpSolver()
    solver.Solve(model)
    for d in all_days:
        print('Day', d)
        for n in all_nurses:
            for s in all_shifts:
                if solver.Value(shifts[(n, d, s)]) == 1:
                    if shift_requests[n][d][s] == 1:
                        print('Nurse', n, 'works shift', s, '(requested).')
                    else:
                        print('Nurse', n, 'works shift', s, '(not requested).')
        print()
    # [END solve]

    # Statistics.
    # [START print_solution]
    print()
    print('Statistics')
    print('  - Number of shift requests met = %i' % solver.ObjectiveValue(),
          '(out of', num_nurses * min_shifts_per_nurse, ')')
    print('  - wall time       : %f s' % solver.WallTime())
    # [END print_solution]


if __name__ == '__main__':
    main()
# [END program]
