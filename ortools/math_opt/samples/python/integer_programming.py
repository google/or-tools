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

"""Simple integer programming example."""

from typing import Sequence

from absl import app

from ortools.math_opt.python import mathopt


# Model and solve the problem:
#   max x + 10 * y
#   s.t. x + 7 * y <= 17.5
#                x <= 3.5
#            x in {0.0, 1.0, 2.0, ...,
#            y in {0.0, 1.0, 2.0, ...,
#
def main(argv: Sequence[str]) -> None:
  del argv  # Unused.

  model = mathopt.Model(name="Linear programming example")

  # Variables
  x = model.add_integer_variable(lb=0.0, name="x")
  y = model.add_integer_variable(lb=0.0, name="y")

  # Constraints
  model.add_linear_constraint(x + 7 * y <= 17.5, name="c1")
  model.add_linear_constraint(x <= 3.5, name="c2")

  # Objective
  model.maximize(x + 10 * y)

  # May raise a RuntimeError on invalid input or internal solver errors.
  result = mathopt.solve(model, mathopt.SolverType.GSCIP)

  # A feasible solution is always available on termination reason kOptimal,
  # and kFeasible, but in the later case the solution may be sub-optimal.
  if result.termination.reason not in (
      mathopt.TerminationReason.OPTIMAL,
      mathopt.TerminationReason.FEASIBLE,
  ):
    raise RuntimeError(f"model failed to solve: {result.termination}")

  print(f"Problem solved in {result.solve_time()}")
  print(f"Objective value: {result.objective_value()}")
  print(
      f"Variable values: [x={round(result.variable_values()[x])}, "
      f"y={round(result.variable_values()[y])}]"
  )


if __name__ == "__main__":
  app.run(main)
