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
"""Integer programming examples that show how to clone a model."""
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
    # x and y are integer non-negative variables.
    x = model.new_int_var(0.0, math.inf, "x")
    y = model.new_int_var(0.0, math.inf, "y")
    # [END variables]

    # [START constraints]
    # x + 7 * y <= 17.5.
    unused_c1 = model.add(x + 7 * y <= 17.5)

    # x <= 3.5.
    c2 = model.add(x <= 3.5)
    # [END constraints]

    # [START objective]
    # Maximize x + 10 * y.
    model.maximize(x + 10 * y)
    # [END objective]

    # [Start clone]
    # Clone the model.
    print("Cloning the model.")
    model_copy = model.clone()
    x_copy = model_copy.var_from_index(x.index)
    y_copy = model_copy.var_from_index(y.index)
    z_copy = model_copy.new_bool_var("z")
    c2_copy = model_copy.linear_constraint_from_index(c2.index)

    # Add new constraint.
    model_copy.add(x_copy >= 1)
    print(f"Number of constraints in original model ={model.num_constraints}")
    print(f"Number of constraints in cloned model = {model_copy.num_constraints}")

    # Modify a constraint.
    c2_copy.add_term(z_copy, 2.0)
    # [END clone]

    # [START solve]
    # Create the solver with the SCIP backend, and solve the model.
    solver = model_builder.Solver("scip")
    if not solver.solver_is_supported():
        return
    status = solver.solve(model_copy)
    # [END solve]

    # [START print_solution]
    if status == model_builder.SolveStatus.OPTIMAL:
        print("Solution:")
        print(f"Objective value = {solver.objective_value}")
        print(f"x = {solver.value(x_copy)}")
        print(f"y = {solver.value(y_copy)}")
        print(f"z = {solver.value(z_copy)}")
    else:
        print("The problem does not have an optimal solution.")
    # [END print_solution]

    # [START advanced]
    print("\nAdvanced usage:")
    print(f"Problem solved in {solver.wall_time} seconds")
    # [END advanced]


if __name__ == "__main__":
    main()
# [END program]
