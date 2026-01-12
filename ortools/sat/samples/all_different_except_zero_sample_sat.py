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
"""Implements AllDifferentExcept0 using atomic constraints."""

import collections

from ortools.sat.python import cp_model


def all_different_except_0():
    """Encode the AllDifferentExcept0 constraint."""

    # Model.
    model = cp_model.CpModel()

    # Declare our primary variable.
    x = [model.new_int_var(0, 10, f"x{i}") for i in range(5)]

    # Expand the AllDifferentExcept0 constraint.
    variables_per_value = collections.defaultdict(list)
    all_values = set()

    for var in x:
        all_encoding_literals = []
        # Domains of variables are represented by flat intervals.
        for i in range(0, len(var.proto.domain), 2):
            start = var.proto.domain[i]
            end = var.proto.domain[i + 1]
            for value in range(start, end + 1):  # Intervals are inclusive.
                # Create the literal attached to var == value.
                bool_var = model.new_bool_var(f"{var} == {value}")
                model.add(var == value).only_enforce_if(bool_var)

                # Collect all encoding literals for a given variable.
                all_encoding_literals.append(bool_var)

                # Collect all encoding literals for a given value.
                variables_per_value[value].append(bool_var)

                # Collect all different values.
                all_values.add(value)

        # One variable must have exactly one value.
        model.add_exactly_one(all_encoding_literals)

    # Add the all_different constraints.
    for value, literals in variables_per_value.items():
        if value == 0:
            continue
        model.add_at_most_one(literals)

    model.add(x[0] == 0)
    model.add(x[1] == 0)

    model.maximize(sum(x))

    # Create a solver and solve.
    solver = cp_model.CpSolver()
    status = solver.solve(model)

    # Checks and prints the output.
    if status == cp_model.OPTIMAL:
        print(f"Optimal solution: {solver.objective_value}, expected: 27.0")
    elif status == cp_model.FEASIBLE:
        print(f"Feasible solution: {solver.objective_value}, optimal 27.0")
    elif status == cp_model.INFEASIBLE:
        print("The model is infeasible")
    else:
        print("Something went wrong. Please check the status and the log")


all_different_except_0()
# [END program]
