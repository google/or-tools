# Copyright 2010-2022 Google LLC
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

"""Minimal example for Getting Started page."""

from typing import Sequence

from absl import app

# [START imports]
from ortools.math_opt.python import mathopt
# [END imports]


def main(argv: Sequence[str]) -> None:
    del argv  # Unused.

    # [START build_model]
    # Build the model.
    model = mathopt.Model(name="getting_started_lp")
    x = model.add_variable(lb=-1.0, ub=1.5, name="x")
    y = model.add_variable(lb=0.0, ub=1.0, name="y")
    model.add_linear_constraint(x + y <= 1.5)
    model.maximize(x + 2 * y)
    # [END build_model]

    # [START solve_args]
    # Set parameters, e.g. turn on logging.
    params = mathopt.SolveParameters(enable_output=True)
    # [END solve_args]

    # [START solve]
    # Solve and ensure an optimal solution was found with no errors.
    # (mathopt.solve may raise a RuntimeError on invalid input or internal solver
    # errors.)
    result = mathopt.solve(model, mathopt.SolverType.GLOP, params=params)
    if result.termination.reason != mathopt.TerminationReason.OPTIMAL:
        raise RuntimeError(f"model failed to solve: {result.termination}")
    # [END solve]

    # [START print_results]
    # Print some information from the result.
    print("MathOpt solve succeeded")
    print("Objective value:", result.objective_value())
    print("x:", result.variable_values()[x])
    print("y:", result.variable_values()[y])
    # [END print_results]


if __name__ == "__main__":
    app.run(main)
