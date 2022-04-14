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

from google.protobuf import text_format
from ortools.sat.python import cp_model

FLAGS = flags.FLAGS
flags.DEFINE_integer('order', 8, 'Order of the ruler.')
flags.DEFINE_string('params', 'max_time_in_seconds:10.0',
                    'Sat solver parameters.')


def solve_golomb_ruler(order, params):
    # Create the model.
    model = cp_model.CpModel()

    var_max = order * order
    all_vars = list(range(0, order))

    marks = [model.NewIntVar(0, var_max, f'marks_{i}') for i in all_vars]

    model.Add(marks[0] == 0)
    for i in range(order - 2):
        model.Add(marks[i + 1] > marks[i])

    diffs = []
    for i in range(order - 1):
        for j in range(i + 1, order):
            diff = model.NewIntVar(0, var_max, f'diff [{j},{i}]')
            model.Add(diff == marks[j] - marks[i])
            diffs.append(diff)
    model.AddAllDifferent(diffs)

    # symmetry breaking
    if order > 2:
        model.Add(marks[order - 1] - marks[order - 2] > marks[1] - marks[0])

    # Objective
    model.Minimize(marks[order - 1])

    # Solve the model.
    solver = cp_model.CpSolver()
    if params:
        text_format.Parse(params, solver.parameters)
    solution_printer = cp_model.ObjectiveSolutionPrinter()
    print(f'Golomb ruler(order={order})')
    status = solver.Solve(model, solution_printer)

    # Print solution.
    print(f'status: {solver.StatusName(status)}')
    if status in (cp_model.OPTIMAL, cp_model.FEASIBLE):
        for idx, var in enumerate(marks):
            print(f'mark[{idx}]: {solver.Value(var)}')
        intervals = [solver.Value(diff) for diff in diffs]
        intervals.sort()
        print(f'intervals: {intervals}')

    print('Statistics:')
    print(f'- conflicts: {solver.NumConflicts()}')
    print(f'- branches : {solver.NumBranches()}')
    print(f'- wall time: {solver.WallTime()}s\n')


def main(_=None):
    solve_golomb_ruler(FLAGS.order, FLAGS.params)


if __name__ == '__main__':
    app.run(main)
