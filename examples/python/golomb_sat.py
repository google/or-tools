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

"""This is the Golomb ruler problem.

This model aims at maximizing radar interferences in a minimum space.
It is known as the Golomb Ruler problem.

The idea is to put marks on a rule such that all differences
between all marks are all different. The objective is to minimize the length
of the rule.
see: https://en.wikipedia.org/wiki/Golomb_ruler
"""

from typing import Sequence
from absl import app
from absl import flags

from google.protobuf import text_format
from ortools.sat.python import cp_model

_ORDER = flags.DEFINE_integer("order", 8, "Order of the ruler.")
_PARAMS = flags.DEFINE_string(
    "params",
    "num_search_workers:16,log_search_progress:true,max_time_in_seconds:45",
    "Sat solver parameters.",
)


def solve_golomb_ruler(order: int, params: str):
    """Solve the Golomb ruler problem."""
    # Create the model.
    model = cp_model.CpModel()

    var_max = order * order
    all_vars = list(range(0, order))

    marks = [model.new_int_var(0, var_max, f"marks_{i}") for i in all_vars]

    model.add(marks[0] == 0)
    for i in range(order - 2):
        model.add(marks[i + 1] > marks[i])

    diffs = []
    for i in range(order - 1):
        for j in range(i + 1, order):
            diff = model.new_int_var(0, var_max, f"diff [{j},{i}]")
            model.add(diff == marks[j] - marks[i])
            diffs.append(diff)
    model.add_all_different(diffs)

    # symmetry breaking
    if order > 2:
        model.add(marks[order - 1] - marks[order - 2] > marks[1] - marks[0])

    # Objective
    model.minimize(marks[order - 1])

    # Solve the model.
    solver = cp_model.CpSolver()
    if params:
        text_format.Parse(params, solver.parameters)
    solution_printer = cp_model.ObjectiveSolutionPrinter()
    print(f"Golomb ruler(order={order})")
    status = solver.solve(model, solution_printer)

    # Print solution.
    print(f"status: {solver.status_name(status)}")
    if status in (cp_model.OPTIMAL, cp_model.FEASIBLE):
        for idx, var in enumerate(marks):
            print(f"mark[{idx}]: {solver.value(var)}")
        intervals = [solver.value(diff) for diff in diffs]
        intervals.sort()
        print(f"intervals: {intervals}")

    print("Statistics:")
    print(f"- conflicts: {solver.num_conflicts}")
    print(f"- branches : {solver.num_branches}")
    print(f"- wall time: {solver.wall_time}s\n")


def main(argv: Sequence[str]) -> None:
    if len(argv) > 1:
        raise app.UsageError("Too many command-line arguments.")
    solve_golomb_ruler(_ORDER.value, _PARAMS.value)


if __name__ == "__main__":
    app.run(main)
