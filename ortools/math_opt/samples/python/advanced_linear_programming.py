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

"""Advanced linear programming example."""

from typing import Sequence

from absl import app

from ortools.math_opt.python import mathopt


# Model and solve the problem:
#   max  10 * x0 + 6 * x1 + 4 * x2
#   s.t. 10 * x0 + 4 * x1 + 5 * x2 <= 600
#         2 * x0 + 2 * x1 + 6 * x2 <= 300
#                     x0 + x1 + x2 <= 100
#            x0 in [0, infinity)
#            x1 in [0, infinity)
#            x2 in [0, infinity)
#
def main(argv: Sequence[str]) -> None:
    del argv  # Unused.

    model = mathopt.Model(name="Advanced linear programming example")

    # Variables
    x = [model.add_variable(lb=0.0, name=f"x{j}") for j in range(3)]

    # Constraints
    constraints = [
        model.add_linear_constraint(10 * x[0] + 4 * x[1] + 5 * x[2] <= 600, name="c1"),
        model.add_linear_constraint(2 * x[0] + 2 * x[1] + 6 * x[2] <= 300, name="c2"),
        model.add_linear_constraint(sum(x) <= 100, name="c3"),
    ]

    # Objective
    model.maximize(10 * x[0] + 6 * x[1] + 4 * x[2])

    # May raise a RuntimeError on invalid input or internal solver errors.
    result = mathopt.solve(model, mathopt.SolverType.GLOP)

    if result.termination.reason != mathopt.TerminationReason.OPTIMAL:
        raise RuntimeError(f"model failed to solve to optimality: {result.termination}")

    print(f"Problem solved in  {result.solve_time()}")
    print(f"Objective value: {result.objective_value()}")
    variable_values = [result.variable_values()[v] for v in x]
    print(f"Variable values: {variable_values}")

    if not result.has_dual_feasible_solution():
        # MathOpt does not require solvers to return a dual solution on optimal,
        # but most LP solvers always will, see go/mathopt-solver-contracts for
        # details.
        raise RuntimeError("no dual solution was returned on optimal")

    dual_values = [result.dual_values()[c] for c in constraints]
    print(f"Constraint duals: {dual_values}")
    reduced_costs = [result.reduced_costs()[v] for v in x]
    print(f"Reduced costs: {reduced_costs}")

    if not result.has_basis():
        # MathOpt does not require solvers to return a basis on optimal, but most
        # Simplex LP solvers (like Glop) always will, see
        # go/mathopt-solver-contracts for detail
        raise RuntimeError("no basis was returned on optimal")

    constraint_status = [result.constraint_status()[c] for c in constraints]
    print(f"Constraint basis status: {constraint_status}")
    variable_status = [result.variable_status()[v] for v in x]
    print(f"Variable basis status: {variable_status}")


if __name__ == "__main__":
    app.run(main)
