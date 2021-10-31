#!/usr/bin/env python3
# Copyright 2010-2021 Google LLC
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


def CopyModelSat():
    """Showcases printing intermediate solutions found during search."""
    # Creates the model.
    # [START model]
    model = cp_model.CpModel()
    # [END model]

    # Creates the variables.
    # [START variables]
    num_vals = 3
    x = model.NewIntVar(0, num_vals - 1, 'x')
    y = model.NewIntVar(0, num_vals - 1, 'y')
    z = model.NewIntVar(0, num_vals - 1, 'z')
    # [END variables]

    # Creates the constraints.
    # [START constraints]
    model.Add(x != y)
    # [END constraints]

    # [START objective]
    model.Maximize(x + 2 * y + 3 * z)
    # [END objective]

    # Creates a solver and solves.
    # [START solve]
    solver = cp_model.CpSolver()
    status = solver.Solve(model)
    # [END solve]

    if status == cp_model.OPTIMAL:
        print('Optimal value of the original model: {}'.format(
            solver.ObjectiveValue()))

    # Copy the model.
    copy = cp_model.CpModel()
    copy.CopyFrom(model)

    copy_x = copy.GetIntVarFromProtoIndex(x.Index())
    copy_y = copy.GetIntVarFromProtoIndex(y.Index())

    copy.Add(copy_x + copy_y <= 1)
    status = solver.Solve(copy)

    if status == cp_model.OPTIMAL:
        print('Optimal value of the modified model: {}'.format(
            solver.ObjectiveValue()))


CopyModelSat()
# [END program]
