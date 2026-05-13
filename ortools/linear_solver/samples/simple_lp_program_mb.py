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

# [START program]
"""Minimal example to call the GLOP solver using model_builder."""
# [START import]
import math

from ortools.linear_solver.python import model_builder
# [END import]


def main():
    # [START model]
    # Create the model.
    model = model_builder.Model()
    # [END model]

    # [START variables]
    # Create the variables x and y.
    x = model.new_num_var(0.0, math.inf, "x")
    y = model.new_num_var(0.0, math.inf, "y")

    print("Number of variables =", model.num_variables)
    # [END variables]

    # [START constraints]
    # x + 7 * y <= 17.5.
    ct = model.add(x + 7 * y <= 17.5)

    # x <= 3.5.
    model.add(x <= 3.5)

    print("Number of constraints =", model.num_constraints)
    # [END constraints]

    # [START objective]
    # Maximize x + 10 * y.
    model.maximize(x + 10 * y)
    # [END objective]

    # [START solve]
    # Create the solver with the GLOP backend, and solve the model.
    solver = model_builder.Solver("glop")
    if not solver.solver_is_supported():
        return
    status = solver.solve(model)
    # [END solve]

    # [START print_solution]
    if status == model_builder.SolveStatus.OPTIMAL:
        print("Solution:")
        print("Objective value =", solver.objective_value)
        print("x =", solver.value(x))
        print("y =", solver.value(y))

        print("dual_value(ct) =", solver.dual_value(ct))
        print("reduced_cost(x) =", solver.reduced_cost(x))
    else:
        print("The problem does not have an optimal solution.")
    # [END print_solution]

    # [START advanced]
    print("\nAdvanced usage:")
    print("Problem solved in %f seconds" % solver.wall_time)
    # [END advanced]


if __name__ == "__main__":
    main()
# [END program]
