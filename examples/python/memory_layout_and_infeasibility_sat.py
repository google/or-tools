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

"""Solves the memory allocation problem, and returns a minimal set of demands to explain infeasibility."""

from collections.abc import Sequence
from typing import List

from absl import app
from absl import flags

from google.protobuf import text_format
from ortools.sat.python import cp_model


_OUTPUT_PROTO = flags.DEFINE_string(
    "output_proto", "", "Output file to write the cp_model proto to."
)
_PARAMS = flags.DEFINE_string(
    "params", "num_workers:1,linearization_level:2", "Sat solver parameters."
)


# Input of the problem.
DEMANDS = [
    [1578, 1583, 43008, 1],
    [1588, 1589, 11264, 1],
    [1590, 1595, 43008, 1],
    [1583, 1588, 47872, 1],
    [1589, 1590, 22848, 1],
    [1586, 1590, 22848, 1],
    [1591, 1594, 43008, 1],
]
CAPACITY = 98304


def solve_hard_model(output_proto: str, params: str) -> bool:
    """Solves the hard assignment model."""
    print("Solving the hard assignment model")
    model = cp_model.CpModel()

    x_intervals: List[cp_model.IntervalVar] = []
    y_starts: List[cp_model.IntVar] = []
    y_intervals: List[cp_model.IntervalVar] = []

    for start, end, demand, unused_alignment in DEMANDS:
        x_interval = model.new_fixed_size_interval_var(start, end - start + 1, "")
        y_start = model.new_int_var(0, CAPACITY - demand, "")
        y_interval = model.new_fixed_size_interval_var(y_start, demand, "")

        x_intervals.append(x_interval)
        y_starts.append(y_start)
        y_intervals.append(y_interval)

    model.add_no_overlap_2d(x_intervals, y_intervals)

    if output_proto:
        model.export_to_file(output_proto)

    solver = cp_model.CpSolver()
    if params:
        text_format.Parse(params, solver.parameters)
    status = solver.solve(model)
    print(solver.response_stats())

    if status == cp_model.FEASIBLE or status == cp_model.OPTIMAL:
        for index, start in enumerate(y_starts):
            print(f"task {index} buffer starts at {solver.value(start)}")

    return status != cp_model.INFEASIBLE


def solve_soft_model_with_assumptions() -> None:
    """Solves the soft model using assumptions."""
    print("Solving the soft model using assumptions")

    model = cp_model.CpModel()

    presences: List[cp_model.IntVar] = []
    x_intervals: List[cp_model.IntervalVar] = []
    y_starts: List[cp_model.IntVar] = []
    y_intervals: List[cp_model.IntervalVar] = []

    for start, end, demand, unused_alignment in DEMANDS:
        presence = model.new_bool_var("")
        x_interval = model.new_optional_fixed_size_interval_var(
            start, end - start + 1, presence, ""
        )
        y_start = model.new_int_var(0, CAPACITY - demand, "")
        y_interval = model.new_optional_fixed_size_interval_var(
            y_start, demand, presence, ""
        )

        presences.append(presence)
        x_intervals.append(x_interval)
        y_starts.append(y_start)
        y_intervals.append(y_interval)

    model.add_no_overlap_2d(x_intervals, y_intervals)
    model.add_assumptions(presences)

    solver = cp_model.CpSolver()
    status = solver.solve(model)
    print(solver.response_stats())
    if status == cp_model.INFEASIBLE:
        # The list actually contains the indices of the variables sufficient to
        # explain infeasibility.
        infeasible_variable_indices = solver.sufficient_assumptions_for_infeasibility()
        infeasible_variable_indices_set = set(infeasible_variable_indices)

        for index, presence in enumerate(presences):
            if presence.index in infeasible_variable_indices_set:
                print(f"using task {index} is sufficient to explain infeasibility")


def solve_soft_model_with_maximization(params: str) -> None:
    """Solves the soft model using maximization."""
    print("Solving the soft model using minimization")

    model = cp_model.CpModel()

    presences: List[cp_model.IntVar] = []
    x_intervals: List[cp_model.IntervalVar] = []
    y_starts: List[cp_model.IntVar] = []
    y_intervals: List[cp_model.IntervalVar] = []

    for start, end, demand, unused_alignment in DEMANDS:
        presence = model.new_bool_var("")
        x_interval = model.new_optional_fixed_size_interval_var(
            start, end - start + 1, presence, ""
        )
        y_start = model.new_int_var(0, CAPACITY - demand, "")
        y_interval = model.new_optional_fixed_size_interval_var(
            y_start, demand, presence, ""
        )

        presences.append(presence)
        x_intervals.append(x_interval)
        y_starts.append(y_start)
        y_intervals.append(y_interval)

    model.add_no_overlap_2d(x_intervals, y_intervals)

    model.maximize(sum(presences))

    solver = cp_model.CpSolver()
    if params:
        text_format.Parse(params, solver.parameters)
    status = solver.solve(model)
    print(solver.response_stats())
    if status == cp_model.OPTIMAL or status == cp_model.FEASIBLE:
        for index, presence in enumerate(presences):
            if not solver.boolean_value(presence):
                print(f"task {index} does not fit")
            else:
                print(f"task {index} buffer starts at {solver.value(y_starts[index])}")


def main(argv: Sequence[str]) -> None:
    if len(argv) > 1:
        raise app.UsageError("Too many command-line arguments.")
    if not solve_hard_model(_OUTPUT_PROTO.value, _PARAMS.value):
        solve_soft_model_with_assumptions()
        solve_soft_model_with_maximization(_PARAMS.value)


if __name__ == "__main__":
    app.run(main)
