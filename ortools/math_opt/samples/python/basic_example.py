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

"""Testing correctness of the code snippets in the comments of model.py."""

from collections.abc import Sequence

from absl import app

from ortools.math_opt.python import mathopt


# Model the problem:
#   max 2.0 * x + y
#   s.t. x + y <= 1.5
#            x in {0.0, 1.0}
#            y in [0.0, 2.5]
#
def main(argv: Sequence[str]) -> None:
    del argv  # Unused.

    model = mathopt.Model(name="my_model")
    x = model.add_binary_variable(name="x")
    y = model.add_variable(lb=0.0, ub=2.5, name="y")
    # We can directly use linear combinations of variables ...
    model.add_linear_constraint(x + y <= 1.5, name="c")
    # ... or build them incrementally.
    objective_expression = 0
    objective_expression += 2 * x
    objective_expression += y
    model.maximize(objective_expression)

    # May raise a RuntimeError on invalid input or internal solver errors.
    result = mathopt.solve(model, mathopt.SolverType.GSCIP)

    if result.termination.reason not in (
        mathopt.TerminationReason.OPTIMAL,
        mathopt.TerminationReason.FEASIBLE,
    ):
        raise RuntimeError(f"model failed to solve: {result.termination}")

    print(f"Objective value: {result.objective_value()}")
    print(f"Value for variable x: {result.variable_values()[x]}")


if __name__ == "__main__":
    app.run(main)
