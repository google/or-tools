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

from ortools.sat.python import cp_model


def main():
    # Instantiate a cp model.
    cost = [[90, 76, 75, 70, 50, 74, 12, 68], [35, 85, 55, 65, 48, 101, 70, 83],
            [125, 95, 90, 105, 59,
             120, 36, 73], [45, 110, 95, 115, 104, 83, 37,
                            71], [60, 105, 80, 75, 59, 62, 93,
                                  88], [45, 65, 110, 95, 47, 31, 81, 34],
            [38, 51, 107, 41, 69, 99, 115,
             48], [47, 85, 57, 71, 92, 77, 109,
                   36], [39, 63, 97, 49, 118, 56,
                         92, 61], [47, 101, 71, 60, 88, 109, 52, 90]]

    sizes = [10, 7, 3, 12, 15, 4, 11, 5]
    total_size_max = 15
    num_workers = len(cost)
    num_tasks = len(cost[1])
    all_workers = range(num_workers)
    all_tasks = range(num_tasks)

    model = cp_model.CpModel()
    # Variables
    total_cost = model.NewIntVar(0, 1000, 'total_cost')
    x = []
    for i in all_workers:
        t = []
        for j in all_tasks:
            t.append(model.NewBoolVar('x[%i,%i]' % (i, j)))
        x.append(t)

    # Constraints

    # Each task is assigned to at least one worker.
    [model.Add(sum(x[i][j] for i in all_workers) >= 1) for j in all_tasks]

    # Total task size for each worker is at most total_size_max
    for i in all_workers:
        model.Add(sum(sizes[j] * x[i][j] for j in all_tasks) <= total_size_max)

    # Total cost
    model.Add(total_cost == sum(x[i][j] * cost[i][j]
                                for j in all_tasks for i in all_workers))
    model.Minimize(total_cost)

    solver = cp_model.CpSolver()
    status = solver.Solve(model)

    if status == cp_model.OPTIMAL:
        print('Total cost = %i' % solver.ObjectiveValue())
        print()
        for i in all_workers:
            for j in all_tasks:
                if solver.Value(x[i][j]) == 1:
                    print('Worker ', i, ' assigned to task ', j, '  Cost = ',
                          cost[i][j])

        print()

    print('Statistics')
    print('  - conflicts : %i' % solver.NumConflicts())
    print('  - branches  : %i' % solver.NumBranches())
    print('  - wall time : %f s' % solver.WallTime())


if __name__ == '__main__':
    main()
