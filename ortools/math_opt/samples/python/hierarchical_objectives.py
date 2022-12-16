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

"""Solves an optimization problem with multiple objectives."""

from collections.abc import Sequence

from absl import app
from absl import flags

from ortools.math_opt.python import mathopt

_ENABLE_OUTPUT = flags.DEFINE_bool(
    "enable_output", False, "should the solver logs be turned on"
)


def main(argv: Sequence[str]) -> None:
    if len(argv) > 1:
        raise app.UsageError("Too many command-line arguments.")
    model = mathopt.Model()
    # The model is:
    #   max    x + y + 2 z
    #   s.t.   x + y + z <= 1.5
    #          x, y in [0, 1]
    #          z binary
    # With secondary objective
    #   max y
    #
    # The first problem is solved by any convex combination of:
    #   (0.5, 0, 1) and (0, 0.5, 1)
    # But with the secondary objective, the unique solution is (0, 0.5, 1), with
    # a primary objective value of 2.5 and secondary objective value of 0.5.
    x = model.add_variable(lb=0, ub=1)
    y = model.add_variable(lb=0, ub=1)
    z = model.add_binary_variable()
    model.add_linear_constraint(x + y + z <= 1.5)
    model.maximize(x + y + 2 * z)
    aux = model.add_maximization_objective(y, priority=1)

    result = mathopt.solve(
        model,
        mathopt.SolverType.GUROBI,
        params=mathopt.SolveParameters(enable_output=_ENABLE_OUTPUT.value),
    )
    if result.termination.reason != mathopt.TerminationReason.OPTIMAL:
        raise ValueError(
            f"Expected an optimal termination, found: {result.termination}"
        )
    print(f"primary objective: {result.objective_value()}")
    print(f"x: {result.variable_values(x)}")
    print(f"y: {result.variable_values(y)}")
    print(f"z: {result.variable_values(z)}")
    prim_sol = result.solutions[0].primal_solution
    assert prim_sol is not None
    print(f"secondary objective: {prim_sol.auxiliary_objective_values[aux]}")


if __name__ == "__main__":
    app.run(main)
