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

"""Non linear example.

Finds a rectangle with maximum available area for given perimeter using
add_multiplication_equality().
"""

from ortools.sat.python import cp_model


def non_linear_sat():
  """Non linear sample."""
  perimeter = 20

  model = cp_model.CpModel()

  x = model.new_int_var(0, perimeter, "x")
  y = model.new_int_var(0, perimeter, "y")
  model.add(2 * (x + y) == perimeter)

  area = model.new_int_var(0, perimeter * perimeter, "s")
  model.add_multiplication_equality(area, x, y)

  model.maximize(area)

  solver = cp_model.CpSolver()

  status = solver.solve(model)

  if status == cp_model.OPTIMAL or status == cp_model.FEASIBLE:
    print(f"x = {solver.value(x)}")
    print(f"y = {solver.value(y)}")
    print(f"s = {solver.value(area)}")
  else:
    print("No solution found.")


non_linear_sat()
