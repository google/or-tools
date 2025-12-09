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

"""The sample shows multiple ways to model soft constraints in CP-SAT."""

# [START program]
# [START import]
from ortools.sat.python import cp_model

# [END import]


def infeasible_model() -> None:
    """Base model that is infeasible."""
    # Creates the model.
    # [START infeasible_model]
    model = cp_model.CpModel()
    # [END infeasible_model]

    # Creates the variables.
    # [START infeasible_model_variables]
    x = model.new_int_var(0, 10, "x")
    y = model.new_int_var(0, 10, "y")
    z = model.new_int_var(0, 10, "z")
    # [END infeasible_model_variables]

    # Creates the constraints.
    # [START infeasible_model_constraints]
    model.add(x > y)
    model.add(y > z)
    model.add(z > x)
    # [END infeasible_model_constraints]

    # Creates a solver and solves.
    # [START infeasible_model_solve]
    solver = cp_model.CpSolver()
    status = solver.solve(model)
    # [END infeasible_model_solve]

    # Print solution.
    # [START infeasible_model_print_solution]
    print(f"  Status = {solver.status_name(status)}")
    # [END infeasible_model_print_solution]


def model_with_enforcement_literals() -> None:
    """Adds fixed costs to violated constraints."""
    # Creates the model.
    # [START model_with_enforcement_literals]
    model = cp_model.CpModel()
    # [END model_with_enforcement_literals]

    # Creates the variables.
    # [START model_with_enforcement_literals_variables]
    x = model.new_int_var(0, 10, "x")
    y = model.new_int_var(0, 10, "y")
    z = model.new_int_var(0, 10, "z")
    a = model.new_bool_var("a")
    b = model.new_bool_var("b")
    # [END variables_with_enforcement_literals]

    # Creates the constraints. Adds enforcement literals to the first two
    # constraints, we assume the third constraint is always enforced.
    # [START constraints_with_enforcement_literals]
    model.add(x > y).only_enforce_if(a)
    model.add(y > z).only_enforce_if(b)
    model.add(z > x)
    # [END constraints_with_enforcement_literals]

    # Adds an objective to maximize the number of enforced constraints.
    # [START objective_with_enforcement_literals]
    model.maximize(a + 2 * b)
    # [END objective_with_enforcement_literals]

    # Creates a solver and solves.
    # [START solve_with_enforcement_literals]
    solver = cp_model.CpSolver()
    status = solver.solve(model)
    # [END solve_with_enforcement_literals]

    # Print solution.
    # [START print_solution_with_enforcement_literals]
    print(f"  Status = {solver.status_name(status)}")
    if status == cp_model.OPTIMAL:
        print(f"  Objective value = {solver.objective_value}")
        print(f"  Value of x = {solver.value(x)}")
        print(f"  Value of y = {solver.value(y)}")
        print(f"  Value of z = {solver.value(z)}")
        print(f"  Value of a = {solver.boolean_value(a)}")
        print(f"  Value of b = {solver.boolean_value(b)}")
    # [END print_solution_with_enforcement_literals]


def model_with_linear_violations() -> None:
    """Adds fixed costs to violated constraints."""
    # Creates the model.
    # [START model_with_linear_violations]
    model = cp_model.CpModel()
    # [END model_with_linear_violations]

    # Creates the variables.
    # [START model_with_linear_violations_variables]
    x = model.new_int_var(0, 10, "x")
    y = model.new_int_var(0, 10, "y")
    z = model.new_int_var(0, 10, "z")
    a = model.new_int_var(0, 10, "a")
    b = model.new_int_var(0, 10, "b")
    # [END model_with_linear_violations_variables]

    # Creates the constraints. Adds enforcement literals to the first two
    # constraints, we assume the third constraint is always enforced.
    # [START model_with_linear_violations_constraints]
    model.add(x > y - a)
    model.add(y > z - b)
    model.add(z > x)
    # [END model_with_linear_violations_constraints]

    # Adds an objective to minimize the added slacks.
    # [START model_with_linear_violations_objective]
    model.minimize(a + 2 * b)
    # [END model_with_linear_violations_objective]

    # Creates a solver and solves.
    # [START model_with_linear_violations_solve]
    solver = cp_model.CpSolver()
    status = solver.solve(model)
    # [END model_with_linear_violations_solve]

    # Print solution.
    # [START model_with_linear_violations_print_solution]
    print(f"  Status = {solver.status_name(status)}")
    if status == cp_model.OPTIMAL:
        print(f"  Objective value = {solver.objective_value}")
        print(f"  Value of x = {solver.value(x)}")
        print(f"  Value of y = {solver.value(y)}")
        print(f"  Value of z = {solver.value(z)}")
        print(f"  Value of a = {solver.value(a)}")
        print(f"  Value of b = {solver.value(b)}")
    # [END model_with_linear_violations_print_solution]


def model_with_quadratic_violations() -> None:
    """Adds fixed costs to violated constraints."""
    # Creates the model.
    # [START model_with_quadratic_violations]
    model = cp_model.CpModel()
    # [END model_with_quadratic_violations]

    # Creates the variables.
    # [START model_with_quadratic_violations_variables]
    x = model.new_int_var(0, 10, "x")
    y = model.new_int_var(0, 10, "y")
    z = model.new_int_var(0, 10, "z")
    a = model.new_int_var(0, 10, "a")
    b = model.new_int_var(0, 10, "b")
    square_a = model.new_int_var(0, 100, "square_a")
    square_b = model.new_int_var(0, 100, "square_b")
    # [END model_with_quadratic_violations_variables]

    # Creates the constraints. Adds enforcement literals to the first two
    # constraints, we assume the third constraint is always enforced.
    # [START model_with_quadratic_violations_constraints]
    model.add(x > y - a)
    model.add(y > z - b)
    model.add(z > x)

    model.add_multiplication_equality(square_a, a, a)
    model.add_multiplication_equality(square_b, b, b)
    # [END constraints_with_quadratic_violations]

    # Adds an objective to minimize the added slacks.
    # [START model_with_quadratic_violations_objective]
    model.minimize(square_a + 2 * square_b)
    # [END model_with_quadratic_violations_objective]

    # Creates a solver and solves.
    # [START model_with_quadratic_violations_solve]
    solver = cp_model.CpSolver()
    status = solver.solve(model)
    # [END model_with_quadratic_violations_solve]

    # Print solution.
    # [START print_solution_with_quadratic_violations]
    print(f"  Status = {solver.status_name(status)}")
    if status == cp_model.OPTIMAL:
        print(f"  Objective value = {solver.objective_value}")
        print(f"  Value of x = {solver.value(x)}")
        print(f"  Value of y = {solver.value(y)}")
        print(f"  Value of z = {solver.value(z)}")
        print(f"  Value of a = {solver.value(a)}")
        print(f"  Value of b = {solver.value(b)}")
    # [END print_solution_with_quadratic_violations]


def main() -> None:
    print("Infeasible model:")
    infeasible_model()
    print("Model with enforcement literals:")
    model_with_enforcement_literals()
    print("Model with linear violations:")
    model_with_linear_violations()
    print("Model with quadratic violations:")
    model_with_quadratic_violations()


if __name__ == "__main__":
    main()
# [END program]
