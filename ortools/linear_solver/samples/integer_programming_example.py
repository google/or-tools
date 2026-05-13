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

"""Small example to illustrate solving a MIP problem."""
# [START program]
# [START import]
from ortools.linear_solver import pywraplp
# [END import]


def IntegerProgrammingExample():
    """Integer programming sample."""
    # [START solver]
    # Create the mip solver with the SCIP backend.
    solver = pywraplp.Solver.CreateSolver("SCIP")
    if not solver:
        return
    # [END solver]

    # [START variables]
    # x, y, and z are non-negative integer variables.
    x = solver.IntVar(0.0, solver.infinity(), "x")
    y = solver.IntVar(0.0, solver.infinity(), "y")
    z = solver.IntVar(0.0, solver.infinity(), "z")

    print("Number of variables =", solver.NumVariables())
    # [END variables]

    # [START constraints]
    # 2*x + 7*y + 3*z <= 50
    constraint0 = solver.Constraint(-solver.infinity(), 50)
    constraint0.SetCoefficient(x, 2)
    constraint0.SetCoefficient(y, 7)
    constraint0.SetCoefficient(z, 3)

    # 3*x - 5*y + 7*z <= 45
    constraint1 = solver.Constraint(-solver.infinity(), 45)
    constraint1.SetCoefficient(x, 3)
    constraint1.SetCoefficient(y, -5)
    constraint1.SetCoefficient(z, 7)

    # 5*x + 2*y - 6*z <= 37
    constraint2 = solver.Constraint(-solver.infinity(), 37)
    constraint2.SetCoefficient(x, 5)
    constraint2.SetCoefficient(y, 2)
    constraint2.SetCoefficient(z, -6)

    print("Number of constraints =", solver.NumConstraints())
    # [END constraints]

    # [START objective]
    # Maximize 2*x + 2*y + 3*z
    objective = solver.Objective()
    objective.SetCoefficient(x, 2)
    objective.SetCoefficient(y, 2)
    objective.SetCoefficient(z, 3)
    objective.SetMaximization()
    # [END objective]

    # Solve the problem.
    # [START solve]
    print(f"Solving with {solver.SolverVersion()}")
    status = solver.Solve()
    # [END solve]

    # Print the solution.
    # [START print_solution]
    if status == pywraplp.Solver.OPTIMAL:
        print("Solution:")
        print(f"Objective value = {solver.Objective().Value()}")
        # Print the value of each variable in the solution.
        for variable in [x, y, z]:
            print(f"{variable.name()} = {variable.solution_value()}")
    else:
        print("The problem does not have an optimal solution.")
    # [END print_solution]

    # [START advanced]
    print("\nAdvanced usage:")
    print(f"Problem solved in {solver.wall_time():d} milliseconds")
    print(f"Problem solved in {solver.iterations():d} iterations")
    # [END advanced]


IntegerProgrammingExample()
# [END program]
