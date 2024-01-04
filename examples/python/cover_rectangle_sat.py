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

"""Fill a 60x50 rectangle by a minimum number of non-overlapping squares."""

from typing import Sequence
from absl import app
from ortools.sat.python import cp_model


def cover_rectangle(num_squares: int) -> bool:
    """Try to fill the rectangle with a given number of squares."""
    size_x = 60
    size_y = 50

    model = cp_model.CpModel()

    areas = []
    sizes = []
    x_intervals = []
    y_intervals = []
    x_starts = []
    y_starts = []

    # Creates intervals for the NoOverlap2D and size variables.
    for i in range(num_squares):
        size = model.new_int_var(1, size_y, "size_%i" % i)
        start_x = model.new_int_var(0, size_x, "sx_%i" % i)
        end_x = model.new_int_var(0, size_x, "ex_%i" % i)
        start_y = model.new_int_var(0, size_y, "sy_%i" % i)
        end_y = model.new_int_var(0, size_y, "ey_%i" % i)

        interval_x = model.new_interval_var(start_x, size, end_x, "ix_%i" % i)
        interval_y = model.new_interval_var(start_y, size, end_y, "iy_%i" % i)

        area = model.new_int_var(1, size_y * size_y, "area_%i" % i)
        model.add_multiplication_equality(area, [size, size])

        areas.append(area)
        x_intervals.append(interval_x)
        y_intervals.append(interval_y)
        sizes.append(size)
        x_starts.append(start_x)
        y_starts.append(start_y)

    # Main constraint.
    model.add_no_overlap_2d(x_intervals, y_intervals)

    # Redundant constraints.
    model.add_cumulative(x_intervals, sizes, size_y)
    model.add_cumulative(y_intervals, sizes, size_x)

    # Forces the rectangle to be exactly covered.
    model.add(sum(areas) == size_x * size_y)

    # Symmetry breaking 1: sizes are ordered.
    for i in range(num_squares - 1):
        model.add(sizes[i] <= sizes[i + 1])

        # Define same to be true iff sizes[i] == sizes[i + 1]
        same = model.new_bool_var("")
        model.add(sizes[i] == sizes[i + 1]).only_enforce_if(same)
        model.add(sizes[i] < sizes[i + 1]).only_enforce_if(~same)

        # Tie break with starts.
        model.add(x_starts[i] <= x_starts[i + 1]).only_enforce_if(same)

    # Symmetry breaking 2: first square in one quadrant.
    model.add(x_starts[0] < (size_x + 1) // 2)
    model.add(y_starts[0] < (size_y + 1) // 2)

    # Creates a solver and solves.
    solver = cp_model.CpSolver()
    solver.parameters.num_workers = 8
    solver.parameters.max_time_in_seconds = 10.0
    status = solver.solve(model)
    print("%s found in %0.2fs" % (solver.status_name(status), solver.wall_time))

    # Prints solution.
    solution_found = status == cp_model.OPTIMAL or status == cp_model.FEASIBLE
    if solution_found:
        display = [[" " for _ in range(size_x)] for _ in range(size_y)]
        for i in range(num_squares):
            sol_x = solver.value(x_starts[i])
            sol_y = solver.value(y_starts[i])
            sol_s = solver.value(sizes[i])
            char = format(i, "01x")
            for j in range(sol_s):
                for k in range(sol_s):
                    if display[sol_y + j][sol_x + k] != " ":
                        print(
                            "ERROR between %s and %s"
                            % (display[sol_y + j][sol_x + k], char)
                        )
                    display[sol_y + j][sol_x + k] = char

        for line in range(size_y):
            print(" ".join(display[line]))
    return solution_found


def main(argv: Sequence[str]) -> None:
    if len(argv) > 1:
        raise app.UsageError("Too many command-line arguments.")
    for num_squares in range(1, 15):
        print("Trying with size =", num_squares)
        if cover_rectangle(num_squares):
            break


if __name__ == "__main__":
    app.run(main)
