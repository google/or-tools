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

"""Example to solves a pentomino paving problem.

Given a subset of n different pentomino, the problem is to pave a square of
size 5 x n. The problem is reduced to an exact set cover problem and encoded
as a linear boolean problem.

This problem comes from the game Katamino:
http://boardgamegeek.com/boardgame/6931/katamino
"""

from collections.abc import Sequence
from typing import Dict, List

from absl import app
from absl import flags

from google.protobuf import text_format
from ortools.sat.python import cp_model


_PARAMS = flags.DEFINE_string(
    "params",
    "num_search_workers:16,log_search_progress:false,max_time_in_seconds:45",
    "Sat solver parameters.",
)

_PIECES = flags.DEFINE_string(
    "pieces", "FILNPTUVWXYZ", "The subset of pieces to consider."
)


def is_one(mask: List[List[int]], x: int, y: int, orientation: int) -> bool:
    """Returns true if the oriented piece is 1 at position [i][j].

    The 3 bits in orientation respectively mean: transposition, symmetry by
    x axis, symmetry by y axis.

    Args:
      mask: The shape of the piece.
      x: position.
      y: position.
      orientation: between 0 and 7.
    """
    if orientation & 1:
        tmp: int = x
        x = y
        y = tmp
    if orientation & 2:
        x = len(mask[0]) - 1 - x
    if orientation & 4:
        y = len(mask) - 1 - y
    return mask[y][x] == 1


def get_height(mask: List[List[int]], orientation: int) -> int:
    if orientation & 1:
        return len(mask[0])
    return len(mask)


def get_width(mask: List[List[int]], orientation: int) -> int:
    if orientation & 1:
        return len(mask)
    return len(mask[0])


def orientation_is_redundant(mask: List[List[int]], orientation: int) -> bool:
    """Checks if the current rotated figure is the same as a previous rotation."""
    size_i: int = get_width(mask, orientation)
    size_j: int = get_height(mask, orientation)
    for o in range(orientation):
        if size_i != get_width(mask, o):
            continue
        if size_j != get_height(mask, o):
            continue

        is_the_same: bool = True
        for k in range(size_i):
            if not is_the_same:
                break
            for l in range(size_j):
                if not is_the_same:
                    break
                if is_one(mask, k, l, orientation) != is_one(mask, k, l, o):
                    is_the_same = False
        if is_the_same:
            return True
    return False


def generate_and_solve_problem(pieces: Dict[str, List[List[int]]]) -> None:
    """Solves the pentominoes problem."""
    box_width = len(pieces)
    box_height = 5

    model = cp_model.CpModel()
    position_to_variables: List[List[List[cp_model.IntVar]]] = [
        [[] for _ in range(box_width)] for _ in range(box_height)
    ]

    for name, mask in pieces.items():
        all_position_variables = []
        for orientation in range(8):
            if orientation_is_redundant(mask, orientation):
                continue
            piece_width = get_width(mask, orientation)
            piece_height = get_height(mask, orientation)
            for i in range(box_width - piece_width + 1):
                for j in range(box_height - piece_height + 1):
                    v = model.new_bool_var(name)
                    all_position_variables.append(v)
                    for k in range(piece_width):
                        for l in range(piece_height):
                            if is_one(mask, k, l, orientation):
                                position_to_variables[j + l][i + k].append(v)

        # Only one combination is selected.
        model.add_exactly_one(all_position_variables)

    for one_column in position_to_variables:
        for all_pieces_in_one_position in one_column:
            model.add_exactly_one(all_pieces_in_one_position)

    # Solve the model.
    solver = cp_model.CpSolver()
    if _PARAMS.value:
        text_format.Parse(_PARAMS.value, solver.parameters)
    status = solver.solve(model)

    print(
        f"Problem {_PIECES.value} solved in {solver.wall_time}s with status"
        f" {solver.status_name(status)}"
    )

    # Print the solution.
    if status == cp_model.OPTIMAL:
        for y in range(box_height):
            line = ""
            for x in range(box_width):
                for v in position_to_variables[y][x]:
                    if solver.BooleanValue(v):
                        line += v.name
                        break
            print(line)


def main(argv: Sequence[str]) -> None:
    if len(argv) > 1:
        raise app.UsageError("Too many command-line arguments.")

    # Pieces are stored in a matrix. mask[height][width]
    pieces: Dict[str, List[List[int]]] = {
        "F": [[0, 1, 1], [1, 1, 0], [0, 1, 0]],
        "I": [[1, 1, 1, 1, 1]],
        "L": [[1, 1, 1, 1], [1, 0, 0, 0]],
        "N": [[1, 1, 1, 0], [0, 0, 1, 1]],
        "P": [[1, 1, 1], [1, 1, 0]],
        "T": [[1, 1, 1], [0, 1, 0], [0, 1, 0]],
        "U": [[1, 0, 1], [1, 1, 1]],
        "V": [[1, 0, 0], [1, 0, 0], [1, 1, 1]],
        "W": [[1, 0, 0], [1, 1, 0], [0, 1, 1]],
        "X": [[0, 1, 0], [1, 1, 1], [0, 1, 0]],
        "Y": [[1, 1, 1, 1], [0, 1, 0, 0]],
        "Z": [[1, 1, 0], [0, 1, 0], [0, 1, 1]],
    }
    selected_pieces: Dict[str, List[List[int]]] = {}
    for p in _PIECES.value:
        if p not in pieces:
            print(f"Piece {p} not found in the list of pieces")
            return
        selected_pieces[p] = pieces[p]
    generate_and_solve_problem(selected_pieces)


if __name__ == "__main__":
    app.run(main)
