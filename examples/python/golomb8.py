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

"""This is the Golomb ruler problem.

This model aims at maximizing radar interferences in a minimum space.
It is known as the Golomb Ruler problem.

The idea is to put marks on a rule such that all differences
between all marks are all different. The objective is to minimize the length
of the rule.
"""

from absl import app

from ortools.constraint_solver.python import constraint_solver as cp

# We disable the following warning because it is a false positive on constraints
# like: solver.Add(x == 0)
# pylint: disable=g-explicit-bool-comparison


def main(_) -> None:
    # Create the solver.
    solver = cp.Solver("golomb ruler")

    size = 8
    var_max = size * size
    all_vars = list(range(0, size))

    marks = [solver.new_int_var(0, var_max, "marks_%d" % i) for i in all_vars]

    objective = solver.minimize(marks[size - 1], 1)

    solver.add(marks[0] == 0)

    # We expand the creation of the diff array to avoid a pylint warning.
    diffs = []
    for i in range(size - 1):
        for j in range(i + 1, size):
            diffs.append(marks[j] - marks[i])
    solver.add_all_different(diffs)

    solver.add(marks[size - 1] - marks[size - 2] > marks[1] - marks[0])
    for i in range(size - 2):
        solver.add(marks[i + 1] > marks[i])

    solution = solver.assignment()
    solution.Add(marks[size - 1])
    collector = solver.all_solution_collector(solution)

    solver.solve(
        solver.phase(
            marks,
            cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
            cp.IntValueStrategy.ASSIGN_MIN_VALUE
        ),
        [objective, collector],
    )
    for i in range(0, collector.solution_count()):
        obj_value = collector.value(i, marks[size - 1])
        time = collector.wall_time_ms(i)
        branches = collector.branches(i)
        failures = collector.failures(i)
        print(
            "Solution #%i: value = %i, failures = %i, branches = %i,time = %i ms"
            % (i, obj_value, failures, branches, time)
        )
    time = solver.wall_time_ms()
    branches = solver.branches()
    failures = solver.failures()
    print(
        (
            "Total run : failures = %i, branches = %i, time = %i ms"
            % (failures, branches, time)
        )
    )


if __name__ == "__main__":
    app.run(main)
