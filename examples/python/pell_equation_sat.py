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

"""Solves Pell's equation x^2 - coeff * y^2 = 1."""

from collections.abc import Sequence

from absl import app
from absl import flags
from ortools.sat.python import cp_model


_COEFF = flags.DEFINE_integer("coeff", 1, "The Pell equation coefficient.")
_MAX_VALUE = flags.DEFINE_integer("max_value", 5000_000, "The maximum value.")


def solve_pell(coeff: int, max_value: int) -> None:
  """Solves Pell's equation x^2 - coeff * y^2 = 1."""
  model = cp_model.CpModel()

  x = model.new_int_var(1, max_value, "x")
  y = model.new_int_var(1, max_value, "y")

  # Pell's equation:
  x_square = model.new_int_var(1, max_value * max_value, "x_square")
  y_square = model.new_int_var(1, max_value * max_value, "y_square")
  model.add_multiplication_equality(x_square, x, x)
  model.add_multiplication_equality(y_square, y, y)
  model.add(x_square - coeff * y_square == 1)

  model.add_decision_strategy(
      [x, y], cp_model.CHOOSE_MIN_DOMAIN_SIZE, cp_model.SELECT_MIN_VALUE
  )

  solver = cp_model.CpSolver()
  solver.parameters.num_workers = 12
  solver.parameters.log_search_progress = True
  solver.parameters.cp_model_presolve = True
  solver.parameters.cp_model_probing_level = 0

  result = solver.solve(model)
  if result == cp_model.OPTIMAL:
    print(f"x={solver.value(x)} y={solver.value(y)} coeff={coeff}")
    if solver.value(x) ** 2 - coeff * (solver.value(y) ** 2) != 1:
      raise ValueError("Pell equation not satisfied.")


def main(argv: Sequence[str]) -> None:
  if len(argv) > 1:
    raise app.UsageError("Too many command-line arguments.")
  solve_pell(_COEFF.value, _MAX_VALUE.value)


if __name__ == "__main__":
  app.run(main)
