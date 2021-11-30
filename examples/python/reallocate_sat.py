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
"""Reallocate production to smooth it over years."""


import collections

from ortools.sat.python import cp_model


def main():

    # Data
    data_0 = [
        [107, 107, 107, 0, 0],  # pr1
        [0, 47, 47, 47, 0],  # pr2
        [10, 10, 10, 0, 0],  # pr3
        [0, 55, 55, 55, 55],  # pr4
    ]

    data_1 = [
        [119444030, 0, 0, 0],
        [34585586, 38358559, 31860661, 0],
        [19654655, 21798799, 18106106, 0],
        [298836792, 0, 0, 0],
        [3713428, 4118530, 4107277, 3072018],
        [6477273, 7183884, 5358471, 0],
        [1485371, 1647412, 1642911, 1228807]
    ]

    data_2 = [
        [1194440, 0, 0, 0],
        [345855, 383585, 318606, 0],
        [196546, 217987, 181061, 0],
        [2988367, 0, 0, 0],
        [37134, 41185, 41072, 30720],
        [64772, 71838, 53584, 0],
        [14853, 16474, 16429, 12288]
    ]

    pr = data_0

    num_pr = len(pr)
    num_years = len(pr[1])
    total = sum(pr[p][y] for p in range(num_pr) for y in range(num_years))
    avg = total // num_years

    # Model
    model = cp_model.CpModel()

    # Variables
    delta = model.NewIntVar(0, total, 'delta')

    contributions_per_years = collections.defaultdict(list)
    contributions_per_prs = collections.defaultdict(list)
    all_contribs = {}

    for p, inner_l in enumerate(pr):
        for y, item in enumerate(inner_l):
            if item != 0:
                contrib = model.NewIntVar(0, total, 'r%d c%d' % (p, y))
                contributions_per_years[y].append(contrib)
                contributions_per_prs[p].append(contrib)
                all_contribs[p, y] = contrib

    year_var = [
        model.NewIntVar(0, total, 'y[%i]' % i) for i in range(num_years)
    ]

    # Constraints

    # Maintain year_var.
    for y in range(num_years):
        model.Add(year_var[y] == sum(contributions_per_years[y]))

    # Fixed contributions per pr.
    for p in range(num_pr):
        model.Add(sum(pr[p]) == sum(contributions_per_prs[p]))

    # Link delta with variables.
    for y in range(num_years):
        model.Add(year_var[y] >= avg - delta)

    for y in range(num_years):
        model.Add(year_var[y] <= avg + delta)

    # Solve and output
    model.Minimize(delta)

    # Solve model.
    solver = cp_model.CpSolver()
    status = solver.Solve(model)

    # Output solution.
    if status == cp_model.OPTIMAL:
        print('Data')
        print('  - total = ', total)
        print('  - year_average = ', avg)
        print('  - number of projects = ', num_pr)
        print('  - number of years = ', num_years)

        print('  - input production')
        for p in range(num_pr):
            for y in range(num_years):
                if pr[p][y] == 0:
                    print('        ', end='')
                else:
                    print('%10i' % pr[p][y], end='')
            print()

        print('Solution')
        for p in range(num_pr):
            for y in range(num_years):
                if pr[p][y] == 0:
                    print('        ', end='')
                else:
                    print('%10i' % solver.Value(all_contribs[p, y]), end='')
            print()

        for y in range(num_years):
            print('%10i' % solver.Value(year_var[y]), end='')
        print()


if __name__ == '__main__':
    main()
