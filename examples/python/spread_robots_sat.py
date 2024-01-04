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

"""maximize the minimum of pairwise distances between n robots in a square space."""

import math
from typing import Sequence
from absl import app
from absl import flags

from google.protobuf import text_format
from ortools.sat.python import cp_model

_NUM_ROBOTS = flags.DEFINE_integer("num_robots", 8, "Number of robots to place.")
_ROOM_SIZE = flags.DEFINE_integer(
    "room_size", 20, "Size of the square room where robots are."
)
_PARAMS = flags.DEFINE_string(
    "params",
    "num_search_workers:16, max_time_in_seconds:20",
    "Sat solver parameters.",
)


def spread_robots(num_robots: int, room_size: int, params: str):
    """Optimize robots placement."""
    model = cp_model.CpModel()

    # Create the list of coordinates (x, y) for each robot.
    x = [model.new_int_var(1, room_size, f"x_{i}") for i in range(num_robots)]
    y = [model.new_int_var(1, room_size, f"y_{i}") for i in range(num_robots)]

    # The specification of the problem is to maximize the minimum euclidian
    # distance between any two robots. Unfortunately, the euclidian distance
    # uses the square root operation which is not defined on integer variables.
    # To work around, we will create a min_square_distance variable, then we make
    # sure that its value is less than the square of the euclidian distance
    # between any two robots.
    #
    # This encoding has a low precision. To improve the precision, we will scale
    # the domain of the min_square_distance variable by a constant factor, then
    # multiply the square of the euclidian distance between two robots by the same
    # factor.
    #
    # we create a scaled_min_square_distance variable with a domain of
    # [0..scaling * max euclidian distance**2] such that
    #   forall i:
    #     scaled_min_square_distance <= scaling * (x_diff_sq[i] + y_diff_sq[i])
    scaling = 1000
    scaled_min_square_distance = model.new_int_var(
        0, 2 * scaling * room_size**2, "scaled_min_square_distance"
    )

    # Build intermediate variables and get the list of squared distances on
    # each dimension.
    for i in range(num_robots - 1):
        for j in range(i + 1, num_robots):
            # Compute the distance on each dimension between robot i and robot j.
            x_diff = model.new_int_var(-room_size, room_size, f"x_diff{i}")
            y_diff = model.new_int_var(-room_size, room_size, f"y_diff{i}")
            model.add(x_diff == x[i] - x[j])
            model.add(y_diff == y[i] - y[j])

            # Compute the square of the previous differences.
            x_diff_sq = model.new_int_var(0, room_size**2, f"x_diff_sq{i}")
            y_diff_sq = model.new_int_var(0, room_size**2, f"y_diff_sq{i}")
            model.add_multiplication_equality(x_diff_sq, x_diff, x_diff)
            model.add_multiplication_equality(y_diff_sq, y_diff, y_diff)

            # We just need to be <= to the scaled square distance as we are
            # maximizing the min distance, which is equivalent as maximizing the min
            # square distance.
            model.add(scaled_min_square_distance <= scaling * (x_diff_sq + y_diff_sq))

    # Naive symmetry breaking.
    for i in range(1, num_robots):
        model.add(x[0] <= x[i])
        model.add(y[0] <= y[i])

    # Objective
    model.maximize(scaled_min_square_distance)

    # Creates a solver and solves the model.
    solver = cp_model.CpSolver()
    if params:
        text_format.Parse(params, solver.parameters)
    solver.parameters.log_search_progress = True
    status = solver.solve(model)

    # Prints the solution.
    if status == cp_model.OPTIMAL or status == cp_model.FEASIBLE:
        print(
            f"Spread {num_robots} with a min pairwise distance of"
            f" {math.sqrt(solver.objective_value / scaling)}"
        )
        for i in range(num_robots):
            print(f"robot {i}: x={solver.value(x[i])} y={solver.value(y[i])}")
    else:
        print("No solution found.")


def main(argv: Sequence[str]) -> None:
    if len(argv) > 1:
        raise app.UsageError("Too many command-line arguments.")

    spread_robots(_NUM_ROBOTS.value, _ROOM_SIZE.value, _PARAMS.value)


if __name__ == "__main__":
    app.run(main)
