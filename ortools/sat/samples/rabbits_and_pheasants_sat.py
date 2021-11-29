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
"""Rabbits and Pheasants quizz."""

from ortools.sat.python import cp_model


def RabbitsAndPheasantsSat():
    """Solves the rabbits + pheasants problem."""
    model = cp_model.CpModel()

    r = model.NewIntVar(0, 100, 'r')
    p = model.NewIntVar(0, 100, 'p')

    # 20 heads.
    model.Add(r + p == 20)
    # 56 legs.
    model.Add(4 * r + 2 * p == 56)

    # Solves and prints out the solution.
    solver = cp_model.CpSolver()
    status = solver.Solve(model)

    if status == cp_model.OPTIMAL:
        print('%i rabbits and %i pheasants' %
              (solver.Value(r), solver.Value(p)))


RabbitsAndPheasantsSat()
