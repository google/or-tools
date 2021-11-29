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
"""Solve an assignment problem with combination constraints on workers."""


from ortools.sat.python import cp_model


def solve_assignment():
    """Solve the assignment problem."""
    # Data.
    cost = [[90, 76, 75, 70, 50, 74], [35, 85, 55, 65, 48,
                                       101], [125, 95, 90, 105, 59, 120],
            [45, 110, 95, 115, 104, 83], [60, 105, 80, 75, 59, 62], [
                45, 65, 110, 95, 47, 31
            ], [38, 51, 107, 41, 69, 99], [47, 85, 57, 71,
                                           92, 77], [39, 63, 97, 49, 118, 56],
            [47, 101, 71, 60, 88, 109], [17, 39, 103, 64, 61,
                                         92], [101, 45, 83, 59, 92, 27]]

    group1 = [
        [0, 0, 1, 1],  # Workers 2, 3
        [0, 1, 0, 1],  # Workers 1, 3
        [0, 1, 1, 0],  # Workers 1, 2
        [1, 1, 0, 0],  # Workers 0, 1
        [1, 0, 1, 0]
    ]  # Workers 0, 2

    group2 = [
        [0, 0, 1, 1],  # Workers 6, 7
        [0, 1, 0, 1],  # Workers 5, 7
        [0, 1, 1, 0],  # Workers 5, 6
        [1, 1, 0, 0],  # Workers 4, 5
        [1, 0, 0, 1]
    ]  # Workers 4, 7

    group3 = [
        [0, 0, 1, 1],  # Workers 10, 11
        [0, 1, 0, 1],  # Workers 9, 11
        [0, 1, 1, 0],  # Workers 9, 10
        [1, 0, 1, 0],  # Workers 8, 10
        [1, 0, 0, 1]
    ]  # Workers 8, 11

    sizes = [10, 7, 3, 12, 15, 4, 11, 5]
    total_size_max = 15
    num_workers = len(cost)
    num_tasks = len(cost[1])
    all_workers = range(num_workers)
    all_tasks = range(num_tasks)

    # Model.

    model = cp_model.CpModel()
    # Variables
    selected = [[model.NewBoolVar('x[%i,%i]' % (i, j)) for j in all_tasks]
                for i in all_workers]
    works = [model.NewBoolVar('works[%i]' % i) for i in all_workers]

    # Constraints

    # Link selected and workers.
    for i in range(num_workers):
        model.AddMaxEquality(works[i], selected[i])

    # Each task is assigned to at least one worker.
    for j in all_tasks:
        model.Add(sum(selected[i][j] for i in all_workers) >= 1)

    # Total task size for each worker is at most total_size_max
    for i in all_workers:
        model.Add(
            sum(sizes[j] * selected[i][j] for j in all_tasks) <= total_size_max)

    # Group constraints.
    model.AddAllowedAssignments([works[0], works[1], works[2], works[3]],
                                group1)
    model.AddAllowedAssignments([works[4], works[5], works[6], works[7]],
                                group2)
    model.AddAllowedAssignments([works[8], works[9], works[10], works[11]],
                                group3)

    # Objective
    model.Minimize(
        sum(selected[i][j] * cost[i][j] for j in all_tasks
            for i in all_workers))

    # Solve and output solution.
    solver = cp_model.CpSolver()
    status = solver.Solve(model)

    if status == cp_model.OPTIMAL:
        print('Total cost = %i' % solver.ObjectiveValue())
        print()
        for i in all_workers:
            for j in all_tasks:
                if solver.BooleanValue(selected[i][j]):
                    print('Worker ', i, ' assigned to task ', j, '  Cost = ',
                          cost[i][j])

        print()

    print('Statistics')
    print('  - conflicts : %i' % solver.NumConflicts())
    print('  - branches  : %i' % solver.NumBranches())
    print('  - wall time : %f s' % solver.WallTime())


solve_assignment()
