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
"""Rabbits and Pheasants quizz."""

from ortools.sat.python import cp_model


def rabbits_and_pheasants_sat():
    """Solves the rabbits + pheasants problem."""
    model = cp_model.CpModel()

    r = model.new_int_var(0, 100, "r")
    p = model.new_int_var(0, 100, "p")

    # 20 heads.
    model.add(r + p == 20)
    # 56 legs.
    model.add(4 * r + 2 * p == 56)

    # Solves and prints out the solution.
    solver = cp_model.CpSolver()
    status = solver.solve(model)

    if status == cp_model.OPTIMAL:
        print(f"{solver.value(r)} rabbits and {solver.value(p)} pheasants")


rabbits_and_pheasants_sat()
# [END program]
