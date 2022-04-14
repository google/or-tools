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
"""This is the Golomb ruler problem.

This model aims at maximizing radar interferences in a minimum space.
It is known as the Golomb Ruler problem.

The idea is to put marks on a rule such that all differences
between all marks are all different. The objective is to minimize the length
of the rule.
see: https://en.wikipedia.org/wiki/Golomb_ruler
"""

from absl import app
from absl import flags
from ortools.constraint_solver import pywrapcp

FLAGS = flags.FLAGS
flags.DEFINE_integer('order', 8, 'Order of the ruler.')


def solve_golomb_ruler(order):
    # Create the solver.
    solver = pywrapcp.Solver('golomb ruler')

    var_max = order * order
    all_vars = list(range(0, order))

    marks = [solver.IntVar(0, var_max, f'marks_{i}') for i in all_vars]

    solver.Add(marks[0] == 0)
    for i in range(order - 2):
        solver.Add(marks[i + 1] > marks[i])

    # We expand the creation of the diff array to avoid a pylint warning.
    diffs = []
    for i in range(order - 1):
        for j in range(i + 1, order):
            diffs.append(marks[j] - marks[i])
    solver.Add(solver.AllDifferent(diffs))

    # symmetry breaking
    if order > 2:
        solver.Add(marks[order - 1] - marks[order - 2] > marks[1] - marks[0])

    # objective
    objective = solver.Minimize(marks[order - 1], 1)

    # Solve the model.
    solution = solver.Assignment()
    for mark in marks:
        solution.Add(mark)
    for diff in diffs:
        solution.Add(diff)
    collector = solver.AllSolutionCollector(solution)

    solver.Solve(
        solver.Phase(
            marks,
            solver.CHOOSE_FIRST_UNBOUND,
            solver.ASSIGN_MIN_VALUE),
        [objective, collector])

    # Print solution.
    for i in range(0, collector.SolutionCount()):
        obj_value = collector.Value(i, marks[order - 1])
        print(f'Solution #{i}: value = {obj_value}')
        for idx, var in enumerate(marks):
            print(f'mark[{idx}]: {collector.Value(i, var)}')
        intervals = [collector.Value(i, diff) for diff in diffs]
        intervals.sort()
        print(f'intervals: {intervals}')

        print('Statistics:')
        print(f'- conflicts: {collector.Failures(i)}')
        print(f'- branches : {collector.Branches(i)}')
        print(f'- wall time: {collector.WallTime(i)}ms\n')

    print('Global Statistics:')
    print(f'- total conflicts: {solver.Failures()}')
    print(f'- total branches : {solver.Branches()}')
    print(f'- total wall time: {solver.WallTime()}ms\n')


def main(_=None):
    solve_golomb_ruler(FLAGS.order)


if __name__ == '__main__':
    app.run(main)
