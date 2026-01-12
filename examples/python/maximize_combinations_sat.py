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

"""Maximize the number of valid combinations of Boolean variables."""

from typing import Sequence

from absl import app

from ortools.sat.python import cp_model


def maximize_combinations_sat() -> None:
    """Maximize the number of valid combinations of Boolean variables."""
    model = cp_model.CpModel()
    cards: list[cp_model.IntVar] = [
        model.new_bool_var("card1"),
        model.new_bool_var("card2"),
        model.new_bool_var("card3"),
        model.new_bool_var("card4"),
    ]

    combos: list[list[cp_model.IntVar]] = [
        [cards[0], cards[1]],
        [cards[0], cards[2]],
        [cards[1], cards[3]],
        [cards[0], cards[2], cards[3]],
    ]

    deck_size: int = 3
    model.add(sum(cards) == deck_size)

    valid_combos: list[cp_model.IntVar] = []
    for combination in combos:
        is_valid = model.new_bool_var("")

        # All true implies is_valid.
        model.add_bool_and(is_valid).only_enforce_if(combination)

        # is_valid implies all true.
        for literal in combination:
            model.add_implication(is_valid, literal)
        valid_combos.append(is_valid)

    model.maximize(sum(valid_combos))

    solver = cp_model.CpSolver()
    solver.parameters.log_search_progress = True
    status = solver.solve(model)

    if status == cp_model.OPTIMAL:
        print(
            "chosen cards:",
            [card.name for card in cards if solver.boolean_value(card)],
        )


def main(argv: Sequence[str]) -> None:
    if len(argv) > 1:
        raise app.UsageError("Too many command-line arguments.")
    maximize_combinations_sat()


if __name__ == "__main__":
    app.run(main)
