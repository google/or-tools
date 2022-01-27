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
# [START program]
"""Solve assignment problem using linear assignment solver."""
# [START import]
from ortools.graph import pywrapgraph
# [END import]


def main():
    """Linear Sum Assignment example."""
    # [START solver]
    assignment = pywrapgraph.LinearSumAssignment()
    # [END solver]

    # [START data]
    costs = [
        [90, 76, 75, 70],
        [35, 85, 55, 65],
        [125, 95, 90, 105],
        [45, 110, 95, 115],
    ]
    num_workers = len(costs)
    num_tasks = len(costs[0])
    # [END data]

    # [START constraints]
    for worker in range(num_workers):
        for task in range(num_tasks):
            if costs[worker][task]:
                assignment.AddArcWithCost(worker, task, costs[worker][task])
    # [END constraints]

    # [START solve]
    status = assignment.Solve()
    # [END solve]

    # [START print_solution]
    if status == assignment.OPTIMAL:
        print(f'Total cost = {assignment.OptimalCost()}\n')
        for i in range(0, assignment.NumNodes()):
            print(f'Worker {i} assigned to task {assignment.RightMate(i)}.' +
                  f'  Cost = {assignment.AssignmentCost(i)}')
    elif status == assignment.INFEASIBLE:
        print('No assignment is possible.')
    elif status == assignment.POSSIBLE_OVERFLOW:
        print(
            'Some input costs are too large and may cause an integer overflow.')
    # [END print_solution]


if __name__ == '__main__':
    main()
# [END Program]
