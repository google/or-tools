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

"""Showcases deep copying of a model."""

# [START program]
from ortools.sat.python import cp_model


def clone_model_sample_sat():
    """Showcases cloning a model."""
    # Creates the model.
    # [START model]
    model = cp_model.CpModel()
    # [END model]

    # Creates the variables.
    # [START variables]
    num_vals = 3
    x = model.new_int_var(0, num_vals - 1, "x")
    y = model.new_int_var(0, num_vals - 1, "y")
    z = model.new_int_var(0, num_vals - 1, "z")
    # [END variables]

    # Creates the constraints.
    # [START constraints]
    model.add(x != y)
    # [END constraints]

    # [START objective]
    model.maximize(x + 2 * y + 3 * z)
    # [END objective]

    # Creates a solver and solves.
    # [START solve]
    solver = cp_model.CpSolver()
    status = solver.solve(model)
    # [END solve]

    if status == cp_model.OPTIMAL:
        print("Optimal value of the original model: {}".format(solver.objective_value))

    # Clones the model.
    # [START clone]
    copy = model.clone()

    copy_x = copy.get_int_var_from_proto_index(x.index)
    copy_y = copy.get_int_var_from_proto_index(y.index)

    copy.add(copy_x + copy_y <= 1)
    # [END clone]

    status = solver.solve(copy)

    if status == cp_model.OPTIMAL:
        print("Optimal value of the modified model: {}".format(solver.objective_value))


clone_model_sample_sat()
# [END program]
